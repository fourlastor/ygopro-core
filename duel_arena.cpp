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

// Lua's blocks come in size classes, and a block Lua frees goes on its class's
// list: steps of 16 bytes up to 1 KiB, then powers of two up to 16 MiB.  A
// larger block is never reused.
namespace {
constexpr size_t step = 16;
constexpr size_t stepped = 1024;
constexpr size_t stepped_classes = stepped / step;
constexpr size_t largest = 16u * 1024u * 1024u;
constexpr size_t classes = stepped_classes + 14; // 2 KiB to 16 MiB
constexpr size_t no_class = static_cast<size_t>(-1);

// The class of a block of `size` bytes, and in `rounded` the size of its blocks.
size_t class_of(size_t size, size_t& rounded) {
	if(size <= stepped) {
		rounded = size <= step ? step : (size + step - 1) & ~(step - 1);
		return rounded / step - 1;
	}
	if(size > largest) {
		rounded = (size + step - 1) & ~(step - 1);
		return no_class;
	}
	size_t power = 2 * stepped;
	size_t index = stepped_classes;
	while(power < size) {
		power *= 2;
		++index;
	}
	rounded = power;
	return index;
}
}

static thread_local duel_arena* active_arena = nullptr;
duel_arena* current_duel_arena() { return active_arena; }
duel_arena_scope::duel_arena_scope(duel_arena* arena) : previous(active_arena) { active_arena = arena; }
duel_arena_scope::~duel_arena_scope() { active_arena = previous; }
duel_arena::duel_arena(size_t size) : storage(static_cast<uint8_t*>(std::malloc(size))), capacity(size) {
	if(!storage) throw std::bad_alloc();
	freed = static_cast<void**>(allocate(classes * sizeof(void*), alignof(std::max_align_t)));
	for(size_t index = 0; index < classes; ++index)
		freed[index] = nullptr;
}
duel_arena::~duel_arena() { std::free(storage); }
void* duel_arena::allocate(size_t size, size_t alignment) {
	const size_t aligned = (used_bytes + alignment - 1) & ~(alignment - 1);
	if(aligned > capacity || size > capacity - aligned) {
		full = true;
		throw std::bad_alloc();
	}
	void* result = storage + aligned;
	used_bytes = aligned + size;
	return result;
}
void* duel_arena::obtain(size_t size) {
	size_t rounded;
	const size_t index = class_of(size, rounded);
	if(index != no_class && freed[index]) {
		void* block = freed[index];
		std::memcpy(&freed[index], block, sizeof(void*));
		return block;
	}
	return allocate(rounded, alignof(std::max_align_t));
}
// A block goes on the list of the size Lua says it holds: a block kept when Lua
// shrank it is larger than that, which wastes the rest and harms nothing.
void duel_arena::give_back(void* ptr, size_t size) noexcept {
	size_t rounded;
	const size_t index = class_of(size, rounded);
	if(index == no_class)
		return;
	std::memcpy(ptr, &freed[index], sizeof(void*));
	freed[index] = ptr;
}
void* duel_arena::reallocate(void* ptr, size_t old_size, size_t new_size) {
	// Where `ptr` is null, `old_size` says what kind of object Lua makes.
	if(!ptr) return obtain(new_size);
	if(new_size == 0) {
		give_back(ptr, old_size);
		return nullptr;
	}
	if(new_size <= old_size) return ptr;
	size_t old_rounded, new_rounded;
	class_of(old_size, old_rounded);
	class_of(new_size, new_rounded);
	if(new_rounded <= old_rounded) return ptr;
	// The last block grows where it is.
	if(static_cast<uint8_t*>(ptr) + old_rounded == storage + used_bytes) {
		const size_t extra = new_rounded - old_rounded;
		if(extra > capacity - used_bytes) {
			full = true;
			throw std::bad_alloc();
		}
		used_bytes += extra;
		return ptr;
	}
	void* result = obtain(new_size);
	std::memcpy(result, ptr, old_size);
	give_back(ptr, old_size);
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
