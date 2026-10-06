/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include "ocgapi.h"
#include "ocgapi_constants.h"
#include "script_bytecode_cache.h"

static lua_Integer evaluate(const script_bytecode_cache::bytecode& code) {
	assert(code);
	auto* state = luaL_newstate();
	assert(state);
	assert(luaL_loadbuffer(state, code->data(), code->size(), "cached") == LUA_OK);
	assert(lua_pcall(state, 0, 1, 0) == LUA_OK);
	assert(lua_isinteger(state, -1));
	const auto value = lua_tointeger(state, -1);
	lua_close(state);
	return value;
}

static void cache_contract() {
	script_bytecode_cache cache(4096, 2);
	auto get = [&](const char* source, const char* name) {
		return cache.get(source, std::strlen(source), name);
	};
	auto first = get("return 11", "same.lua");
	assert(get("return 11", "same.lua") == first);
	// Same name and same byte length, different contents: no stale script.
	auto changed = get("return 22", "same.lua");
	assert(changed != first && evaluate(changed) == 22);
	assert(evaluate(first) == 11); // replacement cannot invalidate an active load
	auto second = get("return 33", "second.lua");
	assert(get("return 22", "same.lua") == changed); // refresh LRU order
	get("return 44", "third.lua");
	assert(get("return 22", "same.lua") == changed);
	assert(get("return 33", "second.lua") != second);
	assert(evaluate(second) == 33); // eviction also preserves active readers
	assert(!get("local =", "syntax.lua"));
	assert(evaluate(get("return 55", "syntax.lua")) == 55);
	assert(!cache.get(first->data(), first->size(), "binary.lua"));
	assert(!cache.get("return 1", 8, nullptr));

	// Exercise byte-budget eviction independently of the entry count.
	script_bytecode_cache budget(512, 100);
	const auto padded_one = "return 1 --" + std::string(200, 'x');
	const auto padded_two = "return 2 --" + std::string(200, 'x');
	auto one = budget.get(padded_one.data(), padded_one.size(), "one.lua");
	assert(budget.get(padded_one.data(), padded_one.size(), "one.lua") == one);
	assert(evaluate(budget.get(padded_two.data(), padded_two.size(), "two.lua")) == 2);
	assert(budget.get(padded_one.data(), padded_one.size(), "one.lua") != one);
	assert(evaluate(one) == 1);
	// An oversized chunk can be used, but must not be retained.
	script_bytecode_cache small(1, 100);
	auto uncached = small.get("return 66", 9, "large.lua");
	assert(evaluate(uncached) == 66);
	assert(small.get("return 66", 9, "large.lua") != uncached);

	// Concurrent hits, source replacements and evictions. Each caller must
	// receive its own source's chunk even if another thread replaces the name.
	script_bytecode_cache shared(8192, 4);
	std::vector<std::thread> threads;
	for(int worker = 0; worker < 8; ++worker) {
		threads.emplace_back([&, worker] {
			for(int i = 0; i < 64; ++i) {
				const int value = worker * 100 + i;
				const auto source = "return " + std::to_string(value);
				const auto name = std::to_string(i % 8) + ".lua";
				assert(evaluate(shared.get(source.data(), source.size(), name.c_str())) == value);
			}
		});
	}
	for(auto& thread : threads)
		thread.join();
}

static void card_reader(void*, uint32_t, OCG_CardData* data) { *data = {}; }
static int script_reader(void*, OCG_Duel, const char*) { return 0; }
static void logger(void* payload, const char* message, int) {
	static_cast<std::vector<std::string>*>(payload)->emplace_back(message);
}
static OCG_Duel make_duel(std::vector<std::string>& messages) {
	OCG_DuelOptions options{};
	options.seed[0] = 1;
	options.team1 = {8000, 0, 0};
	options.team2 = {8000, 0, 0};
	options.cardReader = card_reader;
	options.scriptReader = script_reader;
	options.logHandler = logger;
	options.payload3 = &messages;
	OCG_Duel duel{};
	assert(OCG_CreateDuel(&duel, &options) == OCG_DUEL_CREATION_SUCCESS);
	return duel;
}
static bool load(OCG_Duel duel, const char* script, const char* name) {
	return OCG_LoadScript(duel, script, std::strlen(script), name) != 0;
}
static void integration() {
	std::vector<std::string> messages;
	const char* counter = "counter=(counter or 0)+1; Debug.Message(tostring(counter))";
	const char* closure = "local n=0; next_value=function() n=n+1; return n end";
	const char* call = "Debug.Message(tostring(next_value()))";
	for(int iteration = 0; iteration < 3; ++iteration) {
		auto duel = make_duel(messages);
		assert(load(duel, counter, "counter.lua") && messages.back() == "1");
		assert(load(duel, counter, "counter.lua") && messages.back() == "2");
		assert(load(duel, closure, "closure.lua"));
		assert(load(duel, call, "call.lua") && messages.back() == "1");
		assert(load(duel, call, "call.lua") && messages.back() == "2");
		// Loading a cached chunk creates fresh local upvalues in this state.
		assert(load(duel, closure, "closure.lua"));
		assert(load(duel, call, "call.lua") && messages.back() == "1");
		OCG_StartDuel(duel);
		int status = OCG_DUEL_STATUS_CONTINUE;
		for(int i = 0; i < 16 && status == OCG_DUEL_STATUS_CONTINUE; ++i)
			status = OCG_DuelProcess(duel);
		assert(status == OCG_DUEL_STATUS_AWAITING);
		OCG_DuelSnapshot snapshot{};
		assert(OCG_DuelCreateSnapshot(duel, &snapshot) == OCG_DUEL_SNAPSHOT_SUCCESS);
		const auto used = OCG_DuelArenaUsed(duel);
		assert(load(duel, counter, "counter.lua") && messages.back() == "3");
		assert(load(duel, "branch_value=17", "branch.lua")); // first cached after snapshot
		assert(OCG_DuelRestoreSnapshot(duel, snapshot) == OCG_DUEL_SNAPSHOT_SUCCESS);
		assert(OCG_DuelArenaUsed(duel) == used);
		assert(load(duel, "assert(branch_value==nil)", "check.lua"));
		assert(load(duel, "branch_value=17", "branch.lua"));
		assert(load(duel, "assert(branch_value==17)", "check.lua"));
		assert(load(duel, counter, "counter.lua") && messages.back() == "3");
		OCG_DuelDestroySnapshot(snapshot);

		assert(load(duel, "Debug.Message('old')", "revision.lua") && messages.back() == "old");
		assert(load(duel, "Debug.Message('new')", "revision.lua") && messages.back() == "new");
		assert(!load(duel, "local =", "@syntax.lua"));
		assert(messages.back().find("syntax.lua:1:") != std::string::npos);
		for(int i = 0; i < 2; ++i) {
			assert(!load(duel, "\nerror('expected error')", "@runtime.lua"));
			assert(messages.back().find("runtime.lua:2: expected error") != std::string::npos);
		}
		assert(load(duel, "Debug.Message('recovered')", "@syntax.lua") && messages.back() == "recovered");
		OCG_DestroyDuel(duel);
	}
	// Existing clients may supply precompiled chunks directly.
	script_bytecode_cache compiler;
	const char* source = "Debug.Message('binary')";
	auto binary = compiler.get(source, std::strlen(source), "binary.lua");
	auto duel = make_duel(messages);
	assert(OCG_LoadScript(duel, binary->data(), binary->size(), "binary.lua"));
	assert(messages.back() == "binary");
	OCG_DestroyDuel(duel);
}

int main() {
	cache_contract();
	integration();
	// Exercise the production cache concurrently through the public API too.
	std::vector<std::thread> threads;
	for(int i = 0; i < 4; ++i)
		threads.emplace_back(integration);
	for(auto& thread : threads)
		thread.join();
	std::puts("script bytecode cache: source changes, eviction, isolation, rollback and concurrent loads passed");
}
