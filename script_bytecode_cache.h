/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef SCRIPT_BYTECODE_CACHE_H_
#define SCRIPT_BYTECODE_CACHE_H_

#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "duel_arena.h"
#include "lua.h"
#include "lauxlib.h"

// Only immutable compiled chunks are shared. Tables, closures, upvalues and
// execution always belong to the receiving duel's Lua state.
class script_bytecode_cache {
public:
	using bytecode = std::shared_ptr<const std::vector<char>>;
	explicit script_bytecode_cache(size_t budget = 64u * 1024u * 1024u, size_t limit = 4096)
		: budget(budget), limit(limit) {}

	bytecode get(const char* source, size_t length, const char* name) {
		// Cache entries and compiler scratch must survive duel destruction and
		// rollback. Never allocate either inside the currently active arena.
		duel_arena_scope outside(nullptr);
		if(!source || !name || (length && source[0] == LUA_SIGNATURE[0]))
			return {};
		try {
			{
				std::lock_guard<std::mutex> lock(mutex);
				if(auto hit = find(source, length, name))
					return hit;
			}
			// Do not hold the cache lock while compiling. No script is executed
			// here, and this temporary state has no duel libraries or callbacks.
			std::unique_ptr<lua_State, decltype(&lua_close)> compiler(luaL_newstate(), lua_close);
			if(!compiler || luaL_loadbufferx(compiler.get(), source, length, name, "t") != LUA_OK)
				return {};
			auto compiled = std::make_shared<std::vector<char>>();
			auto writer = [](lua_State*, const void* data, size_t size, void* target) -> int {
				auto& bytes = *static_cast<std::vector<char>*>(target);
				const auto* first = static_cast<const char*>(data);
				bytes.insert(bytes.end(), first, first + size);
				return 0;
			};
			// Retain source names and line information for identical diagnostics.
			if(lua_dump(compiler.get(), writer, compiled.get(), 0) != 0)
				return {};
			std::string source_copy(source, length);
			std::string name_copy(name);
			const size_t cost = source_copy.capacity() + compiled->capacity() + 2 * name_copy.capacity();
			if(cost > budget || limit == 0)
				return compiled;

			std::lock_guard<std::mutex> lock(mutex);
			// Another thread may have compiled the same source in the meantime.
			if(auto hit = find(source, length, name))
				return hit;
			if(auto old = entries.find(name); old != entries.end())
				erase(old);
			while(!entries.empty() && (used > budget - cost || entries.size() >= limit))
				erase(entries.find(recent.back()));
			recent.push_front(name_copy);
			try {
				entries.emplace(std::move(name_copy), entry{std::move(source_copy), compiled, recent.begin(), cost});
			} catch(...) {
				recent.pop_front();
				throw;
			}
			used += cost;
			return compiled;
		} catch(const std::bad_alloc&) {
			// The cache is optional. Let the normal loader report any Lua error
			// through the original state and log callback, including syntax errors.
			return {};
		}
	}

private:
	struct entry {
		std::string source;
		bytecode compiled;
		std::list<std::string>::iterator recent;
		size_t cost;
	};
	using entry_map = std::unordered_map<std::string, entry>;
	const size_t budget;
	const size_t limit;
	size_t used{};
	std::mutex mutex;
	std::list<std::string> recent;
	entry_map entries;

	// Called with mutex held. A filename alone is not a valid cache key:
	// different clients can provide different scripts under the same name.
	bytecode find(const char* source, size_t length, const char* name) {
		auto it = entries.find(name);
		if(it == entries.end() || it->second.source.size() != length
		   || std::memcmp(it->second.source.data(), source, length) != 0)
			return {};
		recent.splice(recent.begin(), recent, it->second.recent);
		return it->second.compiled;
	}
	void erase(entry_map::iterator it) {
		used -= it->second.cost;
		recent.erase(it->second.recent);
		entries.erase(it);
	}
};

#endif
