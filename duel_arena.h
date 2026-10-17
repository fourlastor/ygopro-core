/* Portable experimental containment allocator.  What the engine's C++ code
 * allocates is never given back: freeing cannot safely reclaim an object while
 * the engine still holds raw pointers into it.  What Lua frees is reused for
 * what Lua allocates next, by size class: Lua frees a block only once nothing
 * can reach it, and a long duel's searches would otherwise fill the arena with
 * blocks they let go long before.  The lists of those blocks live in the arena
 * itself, so a snapshot holds them with everything else. */
#ifndef DUEL_ARENA_H_
#define DUEL_ARENA_H_
#include <cstddef>
#include <cstdint>

class duel_arena {
	uint8_t* storage;
	size_t capacity;
	size_t used_bytes{};
	// The first freed Lua block of each size class, at the start of the arena.
	void** freed{};
	// Whether an allocation found no room since the duel began or was last
	// restored: the duel cannot go on from there.
	bool full{};
	void* obtain(size_t size);
	void give_back(void* ptr, size_t size) noexcept;
public:
	explicit duel_arena(size_t capacity);
	~duel_arena();
	void* allocate(size_t size, size_t alignment);
	void deallocate(void*) noexcept {}
	// Lua's allocator: a null `ptr` asks for a block, a `new_size` of 0 frees one.
	void* reallocate(void* ptr, size_t old_size, size_t new_size);
	bool contains(const void* ptr) const;
	uint8_t* data() const { return storage; }
	size_t used() const { return used_bytes; }
	bool ran_out() const { return full; }
	void restore_used(size_t value) { used_bytes = value; full = false; }
};

duel_arena* current_duel_arena();
class duel_arena_scope {
	duel_arena* previous;
public:
	explicit duel_arena_scope(duel_arena* arena);
	~duel_arena_scope();
};
#endif
