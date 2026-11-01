#!/bin/bash
# Generate the recompiled C++ of Condemned from YOUR default.xex with ReXGlue v0.10.0 (host build).
#
#   condemned/tools/codegen.sh path/to/game/default.xex
#
# Builds the rexglue CLI once (upstream rexglue-sdk v0.10.0, the version the Switch SDK matches), copies
# recomp/*.toml next to the XEX's generated tree and runs codegen into condemned/recomp/generated/default/
# (~220 MB of C++, ~8 minutes). Nothing it writes is committed (.gitignore).
#
# Needs: clang/clang++ 19 or newer (std::expected), cmake, ninja, git, python3.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XEX="${1:?usage: codegen.sh path/to/default.xex}"
XEX="$(cd "$(dirname "$XEX")" && pwd)/$(basename "$XEX")"
HOST_SDK="${HOST_SDK:-$ROOT/condemned/out/host-sdk}"
CC_HOST="${CC_HOST:-$(command -v clang-20 || command -v clang-19 || command -v clang)}"
CXX_HOST="${CXX_HOST:-$(command -v clang++-20 || command -v clang++-19 || command -v clang++)}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN)}"

REXGLUE="$HOST_SDK/out/linux-amd64/rexglue"
if [ ! -x "$REXGLUE" ]; then
  echo "== rexglue v0.10.0 (host)"
  if [ ! -d "$HOST_SDK/.git" ]; then
    git clone -q https://github.com/rexglue/rexglue-sdk "$HOST_SDK"
    git -C "$HOST_SDK" checkout -q c94f5ebdcb3c9d1a460ca48e04f9758448f8d518   # v0.10.0
    git -C "$HOST_SDK" submodule update --init --depth 1 --jobs 8
    # REX_DUMP_SECTIONS=<dir> writes the XEX's sections (decrypted) for tools/find_gap_functions.py.
    git -C "$HOST_SDK" apply "$ROOT/condemned/tools/rexglue-dump-sections.patch"
  fi
  cmake -S "$HOST_SDK" -B "$HOST_SDK/out/host" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$CC_HOST" -DCMAKE_CXX_COMPILER="$CXX_HOST" -DCMAKE_CXX_STANDARD=23 \
    -DREXGLUE_ENABLE_TRACY=OFF -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2
  ninja -C "$HOST_SDK/out/host" rexglue -j "$JOBS"
fi

GAME_DIR="$(dirname "$XEX")"
RECOMP="$ROOT/condemned/recomp"
# The manifest points at the XEX relative to itself.
REL_XEX="$(python3 -c "import os,sys;print(os.path.relpath(sys.argv[1],sys.argv[2]))" "$XEX" "$RECOMP")"
REL_GAME="$(python3 -c "import os,sys;print(os.path.relpath(sys.argv[1],sys.argv[2]))" "$GAME_DIR" "$RECOMP")"
sed -e "s|@XEX@|$REL_XEX|; s|@GAME@|$REL_GAME|" "$RECOMP/condemned_manifest.toml.in" > "$RECOMP/condemned_manifest.toml"

echo "== codegen (default.xex -> condemned/recomp/generated/default)"
(cd "$RECOMP" && "$REXGLUE" -f codegen condemned_manifest.toml)
ls "$RECOMP/generated/default/sources.cmake" >/dev/null
echo "Done: $(ls "$RECOMP/generated/default"/*.cpp | wc -l) sources"
