# Portable in-place snapshot prototype

Provenance: this prototype is based exactly on
`400a541984f03dc6dc4c2d96863cc29d1db79d63` from the fourlastor fork.  It is
an isolated feasibility experiment, not an ABI promise.

## Design

`OCG_CreateDuel` reserves a 64 MiB `duel_arena`, enters a thread-local arena
scope, and placement-allocates the `duel` root there.  The scope is entered by
the mutating/process/message/script public C API calls.  Its global C++
`new`/`delete` hooks therefore contain core objects and STL allocations.
`interpreter` no longer uses `luaL_newstate` when arena-backed: it calls
`lua_newstate` with a Lua allocator that allocates and copies inside the same
arena.  This is necessary because Lua's VM, tables, closures, stacks, and GC
metadata otherwise escape the duel object.

At an `OCG_DUEL_STATUS_AWAITING` response boundary with no pending response,
`OCG_DuelCreateSnapshot` copies the committed arena bytes to external snapshot
storage.  `OCG_DuelRestoreSnapshot` copies those bytes back to the identical
arena base and resets its allocation cursor.  The `OCG_Duel` pointer is stable;
all contained raw pointers retain their values.  This is a real state copy,
not semantic serialization.  The core stores no replay history; differential
tests must construct an independent reference duel externally.

## Measured smoke result

The pinned bundled Lua build and core compiled into `/tmp/libocgcore-snapshot.so`.
The ELF shared build uses `-Bsymbolic-functions`: a CoreCLR/PInvoke host has
already loaded `libstdc++`, whose globally preemptible allocator symbols would
otherwise intercept this DSO's `operator new`/`delete` PLT calls.  The focused
harness therefore also reruns under `LD_PRELOAD=$(c++ -print-file-name=libstdc++.so)`;
the unbound build diverges at the first chain snapshot, while the bound build
remains exact.  A
no-card duel was advanced to an external idle-command boundary, snapshotted,
given end-phase response `7`, restored in place, and given response `7` again.
Both continuation status and binary engine message were identical (`exact=1`).

The committed harness also ran 32 iterations on this Linux host (microseconds
per operation): early idle used/copied 97,601 bytes, snapshot creation 3.4 us,
restore 1.6 us, fresh construction to idle 218.9 us; later effect-selection
used/copied 128,302 bytes, creation 4.5 us, restore 2.0 us, fresh construction
and drive through activation/chain passes to selection 454.4 us.  These are
feasibility numbers, not a production benchmark.

## Current boundaries and blockers

This is intentionally a bounded-growth containment prototype: the arena is
monotonic, growing non-tail Lua `realloc` allocates-and-copies, and frees are
no-ops. Shrinking or unchanged Lua blocks retain their allocation, and the
last allocation can grow in place, reducing arena growth and snapshot traffic.
The allocator regression harness covers these paths and preserved contents.
It needs
a configurable arena size and lifetime/high-water benchmark before production.
Host callbacks suspend TLS arena routing; callback-returned card data is copied
before `cardReaderDone`, and API re-entry is explicitly supported.  All public
duel/query APIs establish an arena scope.  The committed fixture exercises Lua
effect activation, an opponent chain response, and valid two-card selection;
it is still intentionally smaller than a production card corpus.

The source `meson.build` also named absent `group.cpp`; it now names
`libgroup.cpp`, but it still links system Lua rather than the pinned C++ Lua
submodule.  The measured compile used that pinned Lua explicitly.

## Hidden-card swap

`OCG_DuelSwapHiddenCards` (`duel_swap.cpp`) exchanges two cards of one player
between the deck, the hand and the Spell & Trap Zone.  A search restores a
snapshot, deals again what one player cannot see with it, and plays the duel
on from there.  It cancels both cards' field effects, exchanges their slots
and their placement state, applies the field effects again and calls
`adjust_instant`, inside the duel's arena scope.  It refuses (returns 0) when
a slot is empty or both name the same slot, when either card is referred to
by a link of the current chain, when a Spell & Trap Zone card is not
face-down, and when a card that is neither Spell nor Trap would land in that
zone.  Face-down monsters cannot be exchanged.
