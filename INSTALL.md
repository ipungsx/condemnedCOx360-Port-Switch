# Instalasi condemned-nx 0.1.0 (Condemned: Criminal Origins untuk Nintendo Switch)

Tidak ada berkas jadi yang dibagikan. Program, kode hasil rekompilasi, dan shader dibangun di komputer Anda dari
**salinan game Anda sendiri** (disc Xbox 360 retail).

Urutan: ekstrak disc -> rekompilasi XEX -> bangun NRO -> jalankan sekali untuk dump shader -> bangun pustaka shader
-> salin ke kartu SD.

## Yang dibutuhkan

| Apa | Rincian |
|---|---|
| Nintendo Switch | Atmosphère dan Homebrew Menu |
| microSD | Kosong ~5 GB, exFAT atau FAT32 (berkas terbesar 1,4 GB) |
| Image disc | Condemned: Criminal Origins Xbox 360, `.iso` (XGD2). `default.xex` SHA-256 `9d2dbd7a…a59fcf` |
| Komputer | Linux (diuji) atau macOS; ~20 GB kosong; clang >= 19, cmake, ninja, python3, git |

## Langkah 1. Ekstrak disc

```
python3 condemned/tools/stage_sd.py "Condemned Criminal Origins.iso" -o game
```

Hasilnya folder `game/` berisi `default.xex`, `default.archcfg`, `autoexec.cfg`, `Layer0.Arch00`, `Layer1.Arch00`,
dan `StartupAssets/`. Skrip bisa dihentikan dan dijalankan ulang; berkas yang belum lengkap akan dilanjutkan.

## Langkah 2. Rekompilasi XEX

```
condemned/tools/codegen.sh game/default.xex
```

Pertama kali, skrip membangun alat `rexglue` v0.10.0 (±10 menit), lalu menghasilkan ±220 MB C++ ke
`condemned/recomp/generated/default/` (±8 menit). Folder itu tidak pernah di-commit.

## Langkah 2b (opsional). Ikon dari disc Anda

```
REX_DUMP_SECTIONS=dump condemned/out/host-sdk/out/linux-amd64/rexglue -f codegen condemned/recomp/condemned_manifest.toml
python3 condemned/tools/make_icon.py dump/image_82000000.bin
```

Ikon Homebrew Menu dibuat dari gambar title resmi di `default.xex` Anda (64x64, diperbesar ke 256x256 dengan judul
di bawahnya) dan ditulis ke `condemned/assets/icon.jpg`, yang tidak pernah di-commit. Tanpa langkah ini NRO memakai
ikon netral `res/icon.jpg`.

## Langkah 3. Toolchain Switch dan NRO

devkitPro: `sudo dkp-pacman -S switch-dev deko3d` (https://devkitpro.org/wiki/Getting_Started). Jika
devkitpro.org tidak bisa dijangkau, `toolchain/build_devkita64.sh` membangun devkitA64 dari sumber GitHub-nya
(beberapa jam), lalu libnx, deko3d dan switch-tools dibangun dengan `make install` dari sumbernya.

```
condemned/tools/build_linux.sh          # -> condemned/out/sw/condemned.nro
```

## Langkah 4. Dump shader dari konsol

1. Salin ke `sdmc:/switch/condemned-nx/`: `condemned.nro` (ganti nama menjadi `condemned-nx.nro`) dan folder `game/`.
2. Jalankan game apa saja sambil **menahan R** -> Homebrew Menu -> **Condemned: Criminal Origins**. Layar hitam itu
   normal (belum ada shader). Tunggu 2-3 menit, lalu tutup dengan HOME.
3. Salin `sdmc:/switch/condemned-nx/shaders_dump/` ke komputer, pasang Vulkan SDK (`dxc`, `spirv-val`), lalu:

```
DKSH=1 shaders/build_library.sh <shaders_dump> shaders_out
```

Hasilnya `shaders_out/cco_shaders.dslib` dan `shaders_out/shadercache_dksh.bin`.

## Langkah 5. Kartu SD

```
sdmc:/switch/condemned-nx/
    condemned-nx.nro
    cco_shaders.dslib        langkah 4
    shadercache_dksh.bin     langkah 4
    game/   default.xex  default.archcfg  autoexec.cfg  Layer0.Arch00  Layer1.Arch00  StartupAssets/
```

Log ditulis ke `sdmc:/switch/condemned-nx/logs/`. **Selama tahap bring-up, kirimkan seluruh folder `logs/`
(beserta `logs/rex/`) setiap kali menguji**, dengan catatan sampai mana game berjalan.

## Jika ada masalah

| Gejala | Tindakan |
|---|---|
| `Not an Xbox 360 disc image` | Image bukan XGD2/XISO, atau rusak |
| `default.xex sha256 ... is not the one` | Disc berbeda versi: jalankan ulang langkah 2 dari XEX Anda |
| `devkitA64 not found` / `deko3d not installed` | Langkah 3 belum dijalankan |
| Program langsung tertutup | Cek `game/` lengkap dan jalankan lewat game + tahan R (bukan dari Album) |
| Layar hitam setelah langkah 5 | `cco_shaders.dslib` dan cache DKSH belum ada di samping NRO |
