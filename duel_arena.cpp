#include "duel_arena.h"
#include <cstdlib>
#include <cstring>
#include <new>

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
	if(void* p = std::aligned_alloc(align, rounded)) return p;
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
void operator delete(void* ptr, std::align_val_t) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, std::align_val_t) noexcept { ::operator delete(ptr); }
void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept { ::operator delete(ptr); }
void operator delete(void* ptr, const std::nothrow_t&) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, const std::nothrow_t&) noexcept { ::operator delete(ptr); }
void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept { ::operator delete(ptr); }
