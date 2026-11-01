#!/bin/bash
# Build condemned.nro with devkitPro's toolchain (Linux; macOS works the same way with Homebrew's tools).
#
# Once:
#   1. devkitPro (https://devkitpro.org/wiki/Getting_Started):  sudo dkp-pacman -S switch-dev deko3d
#      (or tools/../toolchain/build_devkita64.sh, which builds the same toolchain from its GitHub sources)
#   2. cmake, ninja, python3, git, clang >= 19 (for the host codegen tool)
#   3. condemned/tools/codegen.sh path/to/game/default.xex     (the recompiled code, from your disc)
#
# Then, from the repository root:   condemned/tools/build_linux.sh
# The NRO ends up in condemned/out/sw/condemned.nro (copy it as sdmc:/switch/condemned-nx/condemned-nx.nro).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GEN="${CCO_GEN_DIR:-$ROOT/condemned/recomp/generated/default}"
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export PATH="$DEVKITPRO/tools/bin:$PATH"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN)}"

[ -x "$DEVKITPRO/devkitA64/bin/aarch64-none-elf-g++" ] || { echo "devkitA64 not found in $DEVKITPRO (step 1)"; exit 1; }
[ -f "$DEVKITPRO/libnx/lib/libdeko3d.a" ] || { echo "deko3d not installed in $DEVKITPRO/libnx (step 1)"; exit 1; }
[ -f "$GEN/sources.cmake" ] || { echo "no recompiled code in $GEN: run condemned/tools/codegen.sh (step 3)"; exit 1; }

cd "$ROOT"
if [ ! -f sdk/CMakeLists.txt ]; then
  echo "== sdk submodule"
  git submodule update --init sdk
fi
if [ ! -f sdk/thirdparty/uam/uam_api.h ]; then
  echo "== Switch third-party sources"
  (cd sdk && python3 tools/switch_setup.py)
fi

# ds2-nx's SDK patches (condemned/sdk-patches/), applied once, in order (see ds2-nx's PORTING.md for each).
PATCH_STAMP=condemned/out/sdk-patches.stamp
PATCH_HASH="$(cat "$ROOT"/condemned/sdk-patches/*.patch | sha256sum | cut -d' ' -f1)"
if [ -f "$PATCH_STAMP" ] && [ "$(cat "$PATCH_STAMP")" = "$PATCH_HASH" ]; then
  echo "== sdk patches already applied"
elif git -C sdk diff --quiet --ignore-submodules -- . ':!thirdparty'; then
  for p in "$ROOT"/condemned/sdk-patches/*.patch; do
    echo "== sdk patch: $(basename "$p")"
    git -C sdk apply "$p"
  done
  mkdir -p condemned/out && echo "$PATCH_HASH" > "$PATCH_STAMP"
else
  echo "sdk/ has changes that are not this patch series: reset it (git -C sdk checkout -- .) and run again"
  exit 1
fi

BUILD=condemned/out/sw
if [ ! -f "$BUILD/build.ninja" ] || [ "${RECONFIGURE:-0}" = 1 ]; then
  echo "== configure"
  cmake -S condemned -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/tools/switch/cmake/switch-devkitA64.cmake" \
    -DREXSDK_DIR="$ROOT/sdk" -DCCO_GEN_DIR="$GEN" -DREXGLUE_SWITCH_DEKO3D=ON
fi

echo "== build ($JOBS jobs)"
cmake --build "$BUILD" -j "$JOBS"
ls -la "$BUILD/condemned.nro"
