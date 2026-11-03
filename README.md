# condemned-nx

Buy me a coffee, and support my next project,
Thanks
https://saweria.co/ipungsx

Condemned: Criminal Origins (Xbox 360) on the Nintendo Switch as a homebrew NRO, reffering on
[ds2-nx](https://github.com/VasilkovND/ds2-nx). It's not an emulator: the game's PowerPC code is statically
recompiled to C++ with [ReXGlue](https://github.com/rexglue/rexglue-sdk) v0.10.0 and built for the Switch on
[rexglue-sdk-nx](https://github.com/nx-mod/rexglue-sdk-nx) with a deko3d renderer.

> [!NOTE]
> You need your own copy of the game (the retail Xbox 360 disc). This repository contains no game data and no
> game code: the recompiled C++ is generated on your computer from your `default.xex`.

**Status: 0.2.0, playable.** The game runs from the start on hardware: menus, gameplay, lighting, stencil shadows,
volumetric light, post-processing and characters render correctly, at about 30 to 40 FPS in handheld mode (GPU at
460 MHz). A graphics options menu opens with ZL + ZR + Minus. Known issue: the evidence scanner's screen can stay
black. Installation guide (in Indonesian): [INSTALL.md](INSTALL.md).


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
