#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-"$root/build-snapshot"}
mkdir -p "$out"
mapfile -t lua < <(find "$root/lua/src" -maxdepth 1 -name '*.c' \
  ! -name lbitlib.c ! -name lcorolib.c ! -name ldblib.c ! -name linit.c \
  ! -name loadlib.c ! -name loslib.c ! -name ltests.c ! -name lua.c \
  ! -name luac.c ! -name lutf8lib.c ! -name onelua.c -print)
core=(card.cpp duel.cpp duel_arena.cpp duel_swap.cpp effect.cpp field.cpp libgroup.cpp interpreter.cpp libcard.cpp libdebug.cpp libduel.cpp libeffect.cpp ocgapi.cpp operations.cpp playerop.cpp processor.cpp processor_visit.cpp scriptlib.cpp)
c++ -std=c++17 -pthread -fPIC -shared -Wl,-Bsymbolic-functions -DOCGCORE_EXPORT_FUNCTIONS -I "$root" -I "$root/lua" -I "$root/lua/src" -include "$root/lua/luaconf-customize.h" "${lua[@]}" "${core[@]/#/$root/}" -o "$out/libocgcore-snapshot.so"
c++ -std=c++17 -I "$root" "$root/tests/snapshot_restore.cpp" -L "$out" -locgcore-snapshot -Wl,-rpath,"$out" -o "$out/snapshot_restore"
"$out/snapshot_restore"
c++ -std=c++17 -I "$root" "$root/tests/arena_reallocate.cpp" -L "$out" -locgcore-snapshot -Wl,-rpath,"$out" -o "$out/arena_reallocate"
"$out/arena_reallocate"
c++ -std=c++17 -pthread -I "$root" -I "$root/lua/src" "$root/tests/script_bytecode_cache.cpp" -L "$out" -locgcore-snapshot -Wl,-rpath,"$out" -o "$out/script_bytecode_cache"
"$out/script_bytecode_cache"
# Reproduce the managed dlopen symbol scope: libstdc++ is globally loaded
# before the native core.  This must remain exact; the ordinary startup-linked
# binary alone cannot detect allocator-symbol preemption.
if [[ "${SNAPSHOT_PREEMPTION_DIAGNOSTIC:-1}" == 1 ]]; then
	LD_PRELOAD="$(c++ -print-file-name=libstdc++.so)" "$out/snapshot_restore"
	LD_PRELOAD="$(c++ -print-file-name=libstdc++.so)" "$out/script_bytecode_cache"
fi
