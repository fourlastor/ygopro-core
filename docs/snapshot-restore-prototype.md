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
`nm -D` showed exported strong `operator new` and `operator new[]` hooks.  A
no-card duel was advanced to an external idle-command boundary, snapshotted,
given end-phase response `7`, restored in place, and given response `7` again.
Both continuation status and binary engine message were identical (`exact=1`).

## Current boundaries and blockers

This is intentionally a bounded-growth containment prototype: the arena is
monotonic, Lua `realloc` allocates-and-copies, and frees are no-ops.  It needs
a configurable arena size and lifetime/high-water benchmark before production.
Host callbacks suspend TLS arena routing; callback-returned card data is copied
before `cardReaderDone`, and API re-entry is explicitly supported.  All public
duel/query APIs establish an arena scope.  Most importantly, the smoke fixture has not yet exercised card
Lua effects, chains, or selection prompts, so this commit must not be treated
as full snapshot validation.

The source `meson.build` also named absent `group.cpp`; it now names
`libgroup.cpp`, but it still links system Lua rather than the pinned C++ Lua
submodule.  The measured compile used that pinned Lua explicitly.
