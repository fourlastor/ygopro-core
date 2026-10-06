#include "duel_arena.h"
#include <cstdlib>
#include <cstring>
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif

// Aligned memory from outside an arena. Windows has no aligned_alloc: its aligned blocks come
// from _aligned_malloc and go back through _aligned_free, never free.
static void* aligned_allocate(size_t alignment, size_t size) {
#if defined(_WIN32)
	return _aligned_malloc(size, alignment);
#else
	return std::aligned_alloc(alignment, size);
#endif
}
static void aligned_release(void* ptr) noexcept {
#if defined(_WIN32)
	_aligned_free(ptr);
#else
	std::free(ptr);
#endif
}

static thread_local duel_arena* active_arena = nullptr;
duel_arena* current_duel_arena() { return active_arena; }
duel_arena_scope::duel_arena_scope(duel_arena* arena) : previous(active_arena) { active_arena = arena; }
duel_arena_scope::~duel_arena_scope() { active_arena = previous; }
duel_arena::duel_arena(size_t size) : storage(static_cast<uint8_t*>(std::malloc(size))), capacity(size) {
	if(!storage) throw std::bad_alloc();
}
duel_arena::~duel_arena() { std::free(storage); }
void* duel_arena::allocate(size_t size, size_t alignment) {
	const size_t aligned = (used_bytes + alignment - 1) & ~(alignment - 1);
	if(size > capacity - aligned) throw std::bad_alloc();
	void* result = storage + aligned;
	used_bytes = aligned + size;
	return result;
}
void* duel_arena::reallocate(void* ptr, size_t old_size, size_t new_size) {
	if(!ptr) return allocate(new_size, alignof(std::max_align_t));
	if(new_size == 0) return nullptr;
	// A monotonic arena cannot reclaim the old block. Keep it when shrinking
	// instead of allocating and copying another block that enlarges snapshots.
	if(new_size <= old_size) return ptr;
	// The last allocation can grow without abandoning its existing bytes.
	if(static_cast<uint8_t*>(ptr) + old_size == storage + used_bytes) {
		const size_t extra = new_size - old_size;
		if(extra > capacity - used_bytes) throw std::bad_alloc();
		used_bytes += extra;
		return ptr;
	}
	void* result = allocate(new_size, alignof(std::max_align_t));
	std::memcpy(result, ptr, old_size < new_size ? old_size : new_size);
	return result;
}
bool duel_arena::contains(const void* ptr) const { return ptr >= storage && ptr < storage + capacity; }

void* operator new(std::size_t size) {
	if(auto* arena = current_duel_arena()) return arena->allocate(size, alignof(std::max_align_t));
	if(void* p = std::malloc(size)) return p;
	throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void* operator new(std::size_t size, std::align_val_t alignment) {
	if(auto* arena = current_duel_arena()) return arena->allocate(size, static_cast<size_t>(alignment));
	const size_t align = static_cast<size_t>(alignment);
	const size_t rounded = (size + align - 1) & ~(align - 1);
	if(void* p = aligned_allocate(align, rounded)) return p;
	throw std::bad_alloc();
}
void* operator new[](std::size_t size, std::align_val_t alignment) { return ::operator new(size, alignment); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { try { return ::operator new(size); } catch(...) { return nullptr; } }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return ::operator new(size, std::nothrow); }
void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { try { return ::operator new(size, alignment); } catch(...) { return nullptr; } }
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return ::operator new(size, alignment, std::nothrow); }
void operator delete(void* ptr) noexcept { if(!current_duel_arena() || !current_duel_arena()->contains(ptr)) std::free(ptr); }
void operator delete[](void* ptr) noexcept { ::operator delete(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { ::operator delete(ptr); }
static void delete_aligned(void* ptr) noexcept { if(!current_duel_arena() || !current_duel_arena()->contains(ptr)) aligned_release(ptr); }
void operator delete(void* ptr, std::align_val_t) noexcept { delete_aligned(ptr); }
void operator delete[](void* ptr, std::align_val_t) noexcept { delete_aligned(ptr); }
void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept { delete_aligned(ptr); }
void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept { delete_aligned(ptr); }
void operator delete(void* ptr, const std::nothrow_t&) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, const std::nothrow_t&) noexcept { ::operator delete(ptr); }
void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept { delete_aligned(ptr); }
void operator delete[](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept { delete_aligned(ptr); }
