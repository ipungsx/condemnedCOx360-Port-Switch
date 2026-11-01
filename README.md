# condemned-nx

Condemned: Criminal Origins (Xbox 360) on the Nintendo Switch as a homebrew NRO, modelled on
[ds2-nx](https://github.com/VasilkovND/ds2-nx). It's not an emulator: the game's PowerPC code is statically
recompiled to C++ with [ReXGlue](https://github.com/rexglue/rexglue-sdk) v0.10.0 and built for the Switch on
[rexglue-sdk-nx](https://github.com/nx-mod/rexglue-sdk-nx) with a deko3d renderer.

> [!NOTE]
> You need your own copy of the game (the retail Xbox 360 disc). This repository contains no game data and no
> game code: the recompiled C++ is generated on your computer from your `default.xex`.

**Status: 0.1.0, bring-up.** Not playable yet. The code generates and builds; it has not been run on a console.
See [STATUS.md](STATUS.md) for what is done, what is known, and what the first runs on hardware must show.

## How it differs from ds2-nx

ds2-nx builds a recompiled tree someone else had already made and fixed (Dead-Space-2-Recomp). No such tree exists
for Condemned, so this port generates its own:

| | ds2-nx | condemned-nx |
|---|---|---|
| Recompiled code | Dead-Space-2-Recomp's `src_tu2/` (checked in, PDB-named) | `tools/codegen.sh` from your `default.xex` (unnamed `sub_` functions) |
| Codegen fixes | done upstream | `recomp/condemned_functions.toml` (3 functions the analysis missed) |
| Discs | 2 discs + Title Update 2 | 1 disc, no title update |
| GPU glue | `ds2_gpu.cpp` (from halo3-nx, game-neutral) | the same file, `cco_gpu.cpp` |
| Shader hooks | `D3DDevice_Create{Vertex,Pixel}Shader` at 0x82220568 / 0x822201C0 | the same XDK functions at 0x826D2780 / 0x826D25F0 (found by shape, 2005 XDK) |
| SDK patches | `deadspace2/sdk-patches/` (19) | the same 19, applied unchanged |

## Layout

| Folder | Contents |
|---|---|
| `condemned/` | The game project: CMake, Switch glue (`src/`), codegen manifest (`recomp/`), scripts (`tools/`), SDK patches |
| `shaders/` | Shader toolchain from ds2-nx: Xenos microcode -> HLSL -> SPIR-V -> shader library and DKSH cache |
| `sdk/` | **Submodule** -> `rexglue-sdk-nx` (`switch` branch, pinned at `f961303`, the commit ds2-nx tested) |
| `toolchain/` | `build_devkita64.sh`: devkitA64 from its GitHub sources, for machines that cannot reach devkitpro.org |
| `tools/switch/cmake/` | `switch-devkitA64.cmake`, the CMake toolchain file |

## Build

See [INSTALL.md](INSTALL.md). In short:

```
python3 condemned/tools/stage_sd.py "Condemned Criminal Origins.iso" -o game   # extract your disc
condemned/tools/codegen.sh game/default.xex                                     # recompile it (~8 min)
condemned/tools/build_linux.sh                                                  # -> condemned/out/sw/condemned.nro
```

## Credits

Everything here stands on ds2-nx (VasilkovND) and the projects it credits: ReXGlue (Tom Clay and contributors,
built on Xenia), rexglue-sdk-nx (nx-mod), NFSMW-NX (StevensND), XenosRecomp (hedge-dev), SwitchWakerHD's uam
(centollOS), devkitPro and switchbrew, Atmosphère, FFmpeg. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Legal

Unofficial fan port with no affiliation to SEGA, Monolith Productions or Warner Bros. "Condemned" is a trademark of
its owner. You provide your own legally obtained copy of the game. No prebuilt files are distributed: the NRO contains
code recompiled from the game, so it is made on your own computer from your copy. Running homebrew requires custom
firmware, which violates Nintendo's terms and can get a console banned.

Source code is under the GPL-3.0 license (see [LICENSE](LICENSE)), as ds2-nx. The SDK changes are under the SDK's
BSD-3-Clause license, and the shader translator changes under MIT.
