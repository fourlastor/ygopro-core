#include <cassert>
#include <cstring>
#include <new>
#include "duel_arena.h"

int main() {
	duel_arena arena(4096);
	auto* first = static_cast<unsigned char*>(arena.reallocate(nullptr, 0, 128));
	std::memset(first, 0x5a, 128);
	const auto before_shrink = arena.used();
	assert(arena.reallocate(first, 128, 64) == first);
	assert(arena.used() == before_shrink);
	for(int i = 0; i < 64; ++i) assert(first[i] == 0x5a);
	assert(arena.reallocate(first, 64, 64) == first);
	assert(arena.used() == before_shrink);

	auto* tail = static_cast<unsigned char*>(arena.reallocate(nullptr, 0, 128));
	std::memset(tail, 0x3c, 128);
	const auto before_grow = arena.used();
	assert(arena.reallocate(tail, 128, 256) == tail);
	assert(arena.used() == before_grow + 128);
	for(int i = 0; i < 128; ++i) assert(tail[i] == 0x3c);

	// A live intervening block prevents in-place growth of the earlier block.
	const auto before_move = arena.used();
	auto* moved = static_cast<unsigned char*>(arena.reallocate(first, 64, 256));
	assert(moved != first && moved != tail);
	assert(arena.used() == before_move + 256);
	for(int i = 0; i < 64; ++i) assert(moved[i] == 0x5a);
	for(int i = 0; i < 128; ++i) assert(tail[i] == 0x3c);

	// Failed tail growth must leave both the cursor and the live bytes intact.
	const auto before_failure = arena.used();
	bool failed = false;
	try { arena.reallocate(moved, 256, 8192); }
	catch(const std::bad_alloc&) { failed = true; }
	assert(failed && arena.used() == before_failure);
	for(int i = 0; i < 64; ++i) assert(moved[i] == 0x5a);
	assert(arena.reallocate(moved, 256, 0) == nullptr);
	assert(arena.used() == before_failure);
}
