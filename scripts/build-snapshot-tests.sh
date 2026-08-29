#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-"$root/build-snapshot"}
mkdir -p "$out"
mapfile -t lua < <(find "$root/lua/src" -maxdepth 1 -name '*.c' \
  ! -name lbitlib.c ! -name lcorolib.c ! -name ldblib.c ! -name linit.c \
  ! -name loadlib.c ! -name loslib.c ! -name ltests.c ! -name lua.c \
  ! -name luac.c ! -name lutf8lib.c ! -name onelua.c -print)
core=(card.cpp duel.cpp duel_arena.cpp effect.cpp field.cpp libgroup.cpp interpreter.cpp libcard.cpp libdebug.cpp libduel.cpp libeffect.cpp ocgapi.cpp operations.cpp playerop.cpp processor.cpp processor_visit.cpp scriptlib.cpp)
c++ -std=c++17 -fPIC -shared -DOCGCORE_EXPORT_FUNCTIONS -I "$root" -I "$root/lua" -I "$root/lua/src" -include "$root/lua/luaconf-customize.h" "${lua[@]}" "${core[@]/#/$root/}" -o "$out/libocgcore-snapshot.so"
c++ -std=c++17 -I "$root" "$root/tests/snapshot_restore.cpp" -L "$out" -locgcore-snapshot -Wl,-rpath,"$out" -o "$out/snapshot_restore"
"$out/snapshot_restore"
