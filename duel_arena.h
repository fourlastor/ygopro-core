/* Portable experimental containment allocator.  It is intentionally a
 * monotonic arena: freeing cannot safely reclaim an object while the engine
 * still holds raw pointers into it. */
#ifndef DUEL_ARENA_H_
#define DUEL_ARENA_H_
#include <cstddef>
#include <cstdint>

class duel_arena {
	uint8_t* storage;
	size_t capacity;
	size_t used_bytes{};
public:
	explicit duel_arena(size_t capacity);
	~duel_arena();
	void* allocate(size_t size, size_t alignment);
	void deallocate(void*) noexcept {}
	void* reallocate(void* ptr, size_t old_size, size_t new_size);
	bool contains(const void* ptr) const;
	uint8_t* data() const { return storage; }
	size_t used() const { return used_bytes; }
	void restore_used(size_t value) { used_bytes = value; }
};

duel_arena* current_duel_arena();
class duel_arena_scope {
	duel_arena* previous;
public:
	explicit duel_arena_scope(duel_arena* arena);
	~duel_arena_scope();
};
#endif
