# Panduan pemasangan condemned-nx

Panduan ini menjelaskan cara membuat dan memasang **Condemned: Criminal Origins** (Xbox 360) di Nintendo Switch
sebagai homebrew `.nro`.

Repositori ini hanya berisi kode, skrip dan patch. Tidak ada berkas game maupun berkas jadi yang dibagikan. Program,
kode hasil rekompilasi dan pustaka shader dibuat di komputer Anda sendiri dari **salinan disc Anda sendiri**. Alasannya
sederhana: NRO berisi kode game yang sudah diterjemahkan, dan pustaka shader menyimpan shader asli game. Keduanya
merupakan turunan dari game, sehingga tidak boleh disebarkan.

Urutan pengerjaannya:

1. Ekstrak isi disc.
2. Rekompilasi `default.xex` menjadi kode C++.
3. Siapkan toolchain Switch, lalu bangun NRO.
4. Jalankan NRO sekali di konsol untuk mengambil shader game.
5. Bangun pustaka shader di komputer.
6. Salin semuanya ke kartu SD.

Total waktunya sekitar satu sampai dua jam, sebagian besar untuk kompilasi. Langkah 3 bisa memakan beberapa jam
lebih lama apabila toolchain harus dibangun dari sumbernya.

## Kebutuhan

| Kebutuhan | Rincian |
|---|---|
| Nintendo Switch | Atmosphère dan Homebrew Menu |
| Kartu microSD | Ruang kosong sekitar 3 GB, exFAT atau FAT32 (berkas terbesar 1,4 GB) |
| Image disc | Condemned: Criminal Origins Xbox 360 retail, dalam bentuk `.iso` (XGD2) |
| Komputer | Linux (sudah diuji), ruang kosong sekitar 20 GB, `clang` 19 atau lebih baru, `cmake`, `ninja`, `python3`, `git` |
| Untuk pustaka shader | `dxc` (DirectX Shader Compiler) dan `spirv-val`, misalnya dari Vulkan SDK |

Versi `default.xex` yang dipakai selama pengembangan memiliki SHA-256 berawalan `9d2dbd7a` dan berakhiran `a59fcf`.
Disc dengan versi berbeda kemungkinan tetap bisa dipakai, tetapi berkas `condemned/recomp/*.toml` mungkin perlu
disesuaikan.

Ambil repositori beserta submodulnya:

```
git clone --recursive https://github.com/Fungki/condemnedCOx360-Port-Switch.git condemned-nx
cd condemned-nx
```

## Langkah 1. Ekstrak isi disc

```
python3 condemned/tools/stage_sd.py "Condemned Criminal Origins.iso" -o game
```

Hasilnya adalah folder `game/` yang berisi `default.xex`, `default.archcfg`, `autoexec.cfg`, `Layer0.Arch00`,
`Layer1.Arch00` dan folder `StartupAssets/`. Skrip ini boleh dihentikan dan dijalankan ulang. Berkas yang belum
lengkap akan dilanjutkan dari posisi terakhir.

## Langkah 2. Rekompilasi XEX

```
condemned/tools/codegen.sh game/default.xex
```

Pada pemakaian pertama, skrip ini membangun alat `rexglue` v0.10.0 (sekitar 10 menit). Setelah itu skrip
menghasilkan sekitar 220 MB kode C++ ke `condemned/recomp/generated/default/` (sekitar 8 menit). Folder tersebut
tidak pernah masuk ke repositori.

### Opsional: ikon dari disc

```
REX_DUMP_SECTIONS=dump condemned/out/host-sdk/out/linux-amd64/rexglue -f codegen condemned/recomp/condemned_manifest.toml
python3 condemned/tools/make_icon.py dump/image_82000000.bin
```

Perintah ini membuat ikon Homebrew Menu dari gambar judul resmi di `default.xex` Anda dan menyimpannya di
`condemned/assets/icon.jpg`, yang juga tidak pernah masuk ke repositori. Tanpa langkah ini, NRO memakai ikon netral
`condemned/res/icon.jpg`.

## Langkah 3. Toolchain Switch dan NRO

Pasang devkitPro: `sudo dkp-pacman -S switch-dev deko3d` (lihat https://devkitpro.org/wiki/Getting_Started).
Apabila devkitpro.org tidak dapat diakses dari jaringan Anda, `toolchain/build_devkita64.sh` membangun devkitA64 dari
sumbernya di GitHub (beberapa jam), dan `toolchain/build_libnx.sh` membantu membangun libnx. deko3d dan switch-tools
dibangun dari sumbernya dengan `make install`.

```
condemned/tools/build_linux.sh
```

Skrip ini menerapkan patch SDK di `condemned/sdk-patches/` secara berurutan, lalu membangun program. Hasilnya adalah
`condemned/out/sw/condemned.nro`. Kompilasi pertama memakan waktu cukup lama karena kode hasil rekompilasi berjumlah
ribuan berkas.

## Langkah 4. Mengambil shader dari konsol

Game membuat seluruh shader-nya saat pertama kali dijalankan. Selama pustaka shader belum ada, NRO menyimpan setiap
shader yang dibuat game ke folder `shaders_dump/`.

1. Salin `condemned.nro` ke `sdmc:/switch/condemned-nx/` dan ganti namanya menjadi `condemned-nx.nro`. Salin juga
   folder `game/` dari langkah 1 ke folder yang sama.
2. Jalankan sebuah game sambil **menahan tombol R** untuk membuka Homebrew Menu dalam mode *title takeover*, lalu
   pilih **Condemned: Criminal Origins**. Jangan membukanya dari Album, karena memori yang tersedia di sana tidak
   cukup.
3. Layar hitam pada tahap ini wajar karena belum ada shader. Tunggu sekitar 2 sampai 3 menit, lalu tutup dengan
   tombol HOME.
4. Salin folder `sdmc:/switch/condemned-nx/shaders_dump/` ke komputer. Selama pengembangan, folder ini berisi 347
   shader.

## Langkah 5. Pustaka shader

```
DXC=/path/ke/dxc VAL=/path/ke/spirv-val DKSH=1 shaders/build_library.sh <folder shaders_dump> shaders_out
```

Apabila `dxc` dan `spirv-val` sudah ada di `PATH`, isi `DXC=$(command -v dxc)` dan `VAL=$(command -v spirv-val)`.
Hasilnya adalah `shaders_out/cco_shaders.dslib` dan `shaders_out/shadercache_dksh.bin`. Keluaran yang benar memuat
`347 OK, 0 failed` pada tahap SPIR-V dan `cache: 1373 DKSH` pada tahap DKSH.

Shader yang gagal diterjemahkan dicatat di `shaders_out/err/`. Cache DKSH membuat konsol tidak perlu mengompilasi
shader sendiri, sehingga permainan tidak tersendat saat sebuah efek muncul pertama kali.

## Langkah 6. Kartu SD

```
sdmc:/switch/condemned-nx/
    condemned-nx.nro
    cco_shaders.dslib        dari langkah 5
    shadercache_dksh.bin     dari langkah 5
    game/   default.xex  default.archcfg  autoexec.cfg  Layer0.Arch00  Layer1.Arch00  StartupAssets/
```

Folder `shaders_dump/` boleh dihapus. Folder `cco/` (profil dan data simpanan) dan `cache/` dibuat sendiri oleh
program.

Berkas `cco.toml` di folder yang sama bersifat opsional. Berkas ini memuat pengaturan lanjutan dengan format
`nama = nilai`. Pengaturan yang umum dipakai sudah tersedia di menu grafis, sehingga berkas ini biasanya tidak perlu
dibuat.

## Menu pengaturan grafis

Tekan **ZL + ZR + Minus** saat bermain. Selama menu terbuka, game tidak menerima input.

| Pengaturan | Pilihan | Diterapkan |
|---|---|---|
| GPU (handheld) | 307 / 384 / 460 MHz | saat game dijalankan ulang |
| Texture filtering | Trilinear / 2x / 4x / 8x / 16x | saat game dijalankan ulang |
| Anti-aliasing | Off / FXAA | langsung |
| Film grain | On / Off | langsung |
| Volumetric light | On / Off | langsung |
| Brightness | -5 sampai +5 | langsung |
| Gamma | 0.80 sampai 1.20 | langsung |
| Vibration | Off sampai 100% | langsung |
| FPS counter | Off / On | langsung |

Gunakan arah atas dan bawah untuk memilih baris, kiri dan kanan untuk mengubah nilai, **A** untuk menyimpan, dan
**B** untuk menutup. Pilihan disimpan di `cco_opciones.toml` dan dibaca kembali setiap kali game dimulai. Nilai GPU
dibatasi pada profil resmi Switch, jadi ini bukan overclock. Dalam mode docked, GPU selalu berjalan pada 768 MHz.

## Log dan diagnostik

Log ditulis ke `sdmc:/switch/condemned-nx/logs/`. Apabila menemukan masalah, sertakan folder tersebut beserta
keterangan singkat tentang lokasi dan waktu kejadiannya.

Pengaturan berikut di `cco.toml` membantu pemeriksaan lebih rinci:

```
switch_atajos_depuracion = true
```

Dengan pengaturan itu, **ZL + ZR + kiri** merekam satu frame secara lengkap ke log dan ke folder `volcado_XX`, dan
**ZL + ZR + bawah** menyimpan tangkapan layar ke `capturas/`. Berkas kosong bernama `telemetria.flag` di folder
program mengaktifkan rekaman otomatis berkala. Mode ini memperlambat permainan, jadi sebaiknya dipakai hanya saat
memeriksa masalah.

## Jika ada masalah

| Gejala | Tindakan |
|---|---|
| `Not an Xbox 360 disc image` | Image bukan XGD2 atau XISO, atau berkasnya rusak |
| `default.xex sha256 ... is not the one` | Versi disc berbeda. Jalankan ulang langkah 2 dari XEX Anda |
| `devkitA64 not found` / `deko3d not installed` | Langkah 3 belum selesai |
| `sdk/ has changes that are not this patch series` | Kembalikan submodul dengan `git -C sdk checkout -- .`, lalu ulangi langkah 3 |
| Program langsung tertutup | Periksa kelengkapan folder `game/` dan pastikan dijalankan lewat game sambil menahan R |
| Layar hitam setelah langkah 6 | `cco_shaders.dslib` dan `shadercache_dksh.bin` belum ada di samping NRO |
| Permainan patah-patah berkala | Hapus `telemetria.flag` apabila ada |
