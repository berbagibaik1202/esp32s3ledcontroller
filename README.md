# ESP32-S3 HUB75 LED Media Controller

Tahap sekarang: **M0 + profil, framebuffer, diagnostic, layout, dan gambar BMP/JPEG**,
belum driver output display, playback terjadwal, atau Web UI. Firmware versi 0.6.0.
Spesifikasi produk ada di [prd.md](prd.md).

Verifikasi software pada 7 Oktober 2026: build native USB dan UART berhasil
dengan PlatformIO Core 6.2.0 / Espressif32 7.1.3. Header bootloader USB memakai
flash 16 MiB; partition table tidak tumpang tindih dan berada dalam kapasitas.
RAM statis sekitar 44 KiB; nilai ini belum mencakup alokasi runtime Wi-Fi/PSRAM.
Pengujian hardware M0 belum dilakukan karena board belum tersedia.

## Build tanpa hardware

Memerlukan Python dan PlatformIO Core, atau ekstensi PlatformIO di VS Code.
Di Windows dengan Python launcher:

```powershell
py -m platformio run
py -m platformio run -e esp32s3_n16r8_uart
```

Toolchain/cache PlatformIO proyek disimpan di `.pio-core/` pada drive proyek.
Untuk instalasi awal saat disk C: terbatas, gunakan helper agar direktori
sementara download/unpack juga berada pada drive proyek:

```powershell
powershell -ExecutionPolicy Bypass -File tools/build.ps1
powershell -ExecutionPolicy Bypass -File tools/build.ps1 -Environment esp32s3_n16r8_uart
```

Environment default menggunakan native USB Serial/JTAG CDC. Environment UART
untuk konektor USB-UART bridge, jika board aktual memilikinya. Konfigurasi
sementara mengasumsikan flash QIO 16 MiB dan PSRAM octal 8 MiB N16R8;
identitas module/board harus diperiksa ketika perangkat tersedia.

## Saat board tersedia

1. Hubungkan board saja dengan kabel USB data; panel belum diperlukan untuk M0.
2. Periksa nama module, flash/PSRAM, konektor USB, dan pinout board aktual.
3. Cari port dengan `py -m platformio device list`.
4. Upload dan monitor, ganti COM5 dengan port aktual:

```powershell
py -m platformio run -e esp32s3_n16r8_usb -t upload --upload-port COM5
py -m platformio device monitor -p COM5 -b 115200
```

Untuk USB-UART, gunakan environment `esp32s3_n16r8_uart`. Jika belum terdeteksi,
ikuti prosedur BOOT/RESET board untuk download mode. Serial tidak ditunggu tanpa
batas; tekan `s` jika terminal dibuka setelah log startup lewat, atau reset board.

## Bukti lulus M0

- Log menunjukkan ESP32-S3, flash 16777216 byte, PSRAM 8388608 byte.
- Memory baseline PASS dan write/read sampel PSRAM 64 KiB PASS.
- Wi-Fi initialization PASS, scan selesai; nol jaringan bukan kegagalan otomatis.
- Serial menerima perintah di tabel berikut; status berkala berjalan tanpa reset tak terduga.
- Simpan log startup/status dan identitas board untuk pengujian berikutnya.

Sampel PSRAM bukan pengujian seluruh RAM. Compile berhasil juga tidak membuktikan
USB, RAM, RF, kelistrikan, atau pinout aktual berfungsi.

## Persiapan display tanpa panel

Saat memory baseline PASS, firmware memvalidasi profile development 64x32 1/16
dan mengalokasikan dua framebuffer RGB565 di PSRAM (8 KiB total). Frame pertama
merah dirender dalam memori. Profile belum tervalidasi pada hardware.

| Perintah serial | Fungsi |
|---|---|
| `s` | Status memori, dimensi canvas, dan nomor frame |
| `w` | Ulang scan Wi-Fi |
| `p` | Ringkasan profile development |
| `j` | Export profile sebagai JSON |
| `d` | Render pola berikutnya ke memori dan swap buffer |
| `l` | Posisi modul, rotasi, dan urutan chain aktif |
| `n` | Beralih ke preset layout berikutnya dan render ulang |
| `k` | Export layout sebagai JSON |
| `b` | Render BMP contoh dan beralih ke mode scaler berikutnya |
| `i` | Decode/render JPEG contoh dan beralih ke mode scaler berikutnya |
| `m` | Informasi media contoh dan mode scaler berikutnya |
| `h` | Bantuan |

Urutan pola: merah, hijau, biru, RGB bars, row test, column test, checkerboard,
gradient, pixel walker. Setiap putaran menggeser langkah row/column/walker.
Tidak ada scan/address test fisik sebelum backend HUB75 tersedia.

Parser memakai ArduinoJson 7.4.3, input maksimal 2048 byte, schema v1, ID aman,
dimensi maksimal 128x64, scan 8/16/32, dan addressing biner 3/4/5. Saat ini hanya
STANDARD mapping, GENERIC driver, dan SAFE timing yang diterima. CUSTOM, FAST,
driver lain, field tidak dikenal, serta kombinasi geometry tidak valid ditolak.
Konversi RGB order tersedia sebagai fungsi output; canvas tetap logical RGB565.

Parser/export sudah diuji, tetapi import lewat Web UI, penyimpanan profile/layout,
dan aktivasi output belum diimplementasikan. Batas 128x64
adalah batas software sementara, bukan hasil benchmark HUB75.

## Layout multi-panel

Perintah `n` berputar melalui preset berikut untuk modul development 64x32:

| Preset | Canvas | Susunan chain |
|---|---|---|
| 0: satu panel | 64x32 | 0 |
| 1: horizontal | 128x32 | 0, 1 |
| 2: vertical | 64x64 | 0 di atas, 1 di bawah |
| 3: matrix | 128x64 | baris atas 0,1; bawah 2,3 |
| 4: serpentine | 128x64 | baris atas 0,1; bawah 3,2 |

Rotasi custom 0/90/180/270 derajat searah jarum jam didukung oleh parser/API.
Ukuran modul setelah rotasi digunakan untuk memeriksa batas canvas dan overlap.
Custom layout boleh memiliki area kosong; pixel pada area itu tidak memiliki
tujuan panel. Chain index harus unik dan berurutan mulai 0, tetapi urutan objek
di array JSON bebas. Semua modul menggunakan satu profile.

`LayoutMapper` menyediakan canvas-to-panel, panel-to-canvas, serta copy canvas
ke buffer row-major modul menurut chain index. RGB order diterapkan sekali
pada copy ini. Buffer chain tersebut belum berupa scan/DMA HUB75, dan belum
dipakai oleh backend fisik. Pergantian preset hanya aktif dalam RAM; reboot
kembali ke preset satu panel. Kegagalan alokasi mempertahankan layout/canvas lama.

Contoh JSON tersedia di [serpentine_2x2.json](examples/layouts/serpentine_2x2.json).

## Tes software di komputer

```powershell
powershell -ExecutionPolicy Bypass -File tools/setup-host.ps1
powershell -ExecutionPolicy Bypass -File tools/test-host.ps1
```

Setup mengunduh compiler portabel
[w64devkit 2.10.0](https://github.com/skeeto/w64devkit/releases/tag/v2.10.0)
ke `.build-temp/`, memverifikasi SHA-256, lalu mengekstrak tanpa instalasi sistem.
Alternatif: script tes memakai Visual Studio C++ Build Tools dengan Windows SDK
jika compiler portabel belum tersedia. Jalankan build firmware terlebih dahulu
agar dependency ArduinoJson tersedia.

Tes menjalankan kode C++ yang sama dengan firmware: validasi negatif dan
round-trip profile, enam RGB order, clipping pixel, isolasi front/back,
rollback saat alokasi pertama/kedua gagal, pelepasan memori, dan koordinat pola.
Sembilan preview PPM dihasilkan di `.build-temp/previews/`.
Tes ini sudah lulus; output fisik dan sinkronisasi DMA/task belum diuji.
Tes layout juga memeriksa pemetaan semua pixel, empat rotasi, area kosong,
overlap/batas/chain invalid, round-trip JSON, transfer chain, kapasitas buffer,
penolakan buffer yang tumpang tindih, dan penerapan RGB order.

## Media gambar — M8 software

BMP 24-bit BI_RGB dengan BITMAPINFOHEADER 40 byte sudah didukung. Decoder
menerima orientasi top-down/bottom-up, padding baris 4 byte, dan biSizeImage nol
atau ukuran pixel yang benar. Header/file size/offset/planes/compression/dimensi
divalidasi sebelum sumber diganti. Maksimal sumber 128x64 dan file 1 MiB.
Header DIB lain, palette, compression, serta BMP 16/32-bit ditolak pada tahap ini.

Decoder membaca byte BMP immutable yang dipinjam; pemilik harus menjaga data
tetap hidup selama render. Decoder tidak mengalokasikan framebuffer gambar
kedua. ImageSource menyajikan pixel RGB565 logical kepada renderer yang dapat
dipakai provider format lain nanti.

| Mode | Perilaku |
|---|---|
| FIT | Seluruh sumber terlihat dengan rasio tetap; sisa area memakai background |
| FILL | Rasio tetap memenuhi target, lalu crop di tengah |
| CROP | Tanpa scaling; mulai dari cropX/cropY |
| CENTER | Tanpa scaling; sumber di tengah dan dipotong bila terlalu besar |
| STRETCH | Scaling independen kedua sumbu |

Nearest-neighbor memakai koordinat integer. FIT membulatkan ukuran ke bawah
(minimum 1 pixel), FILL ke atas. Selisih ganjil untuk centering/cropping diletakkan
di kanan/bawah. Target rectangle yang keluar canvas dipotong; pixel di luar
rectangle tetap utuh. API dapat mempertahankan background sebelumnya untuk
area sisa atau menggantinya dengan warna background.

Perintah `b` merender contoh [test_bars.bmp](examples/media/test_bars.bmp)
4x2 ke canvas aktif, bergiliran FIT/FILL/CROP/CENTER/STRETCH. Ini contoh built-in
di firmware, belum pembacaan file flash, upload, atau playlist. Hasil tetap
di memori dan HUB75 blank. CENTER/CROP pada contoh kecil akan tampak kecil saat
nanti output panel tersedia.

Fixture BMP dan header byte firmware dapat dibuat ulang dengan
`py tools/generate-test-bmp.py`. Tes memeriksa kesamaan fixture, seluruh panjang
input terpotong, header invalid, orientasi/padding/warna, rollback sumber, mode
scaler, clipping, preservation background, dan canvas 1x1. Lima preview media
PPM dihasilkan di `.build-temp/previews/` bersama sembilan preview diagnostic.

### JPEG baseline

Adapter memakai [JPEGDEC](https://github.com/bitbank2/JPEGDEC) yang dipin ke
commit `86282979224c8a32fd51e091ed5a35b0c699a52b`. Angka versi pada manifest
upstream berbeda dari library.properties; commit menjadi acuan dependency.
Tidak ada pembaruan otomatis ke HEAD.

Input maksimal 1 MiB dan 128x64, baseline Huffman 8-bit single-scan, grayscale
atau YCbCr. Progressive, arithmetic, CMYK/RGB component IDs, multi-scan,
header/dimensi invalid, EOI hilang, serta data tambahan setelah EOI ditolak.
Exif orientation belum diterapkan; orientasi harus disiapkan pada sumber.

Decoder menghasilkan image RGB565 milik adapter; byte JPEG hanya diperlukan
selama decode. Sumber lama dipertahankan jika parsing, alokasi, atau decoding
gagal. Candidate pixel buffer dan coverage bitmap dialokasikan sebelum decode;
workspace decoder berada di heap, lalu dilepas setelah decode. Firmware memakai
allocator PSRAM untuk ketiganya. Peak memory mencakup gambar lama, candidate,
bitmap, dan workspace; `m` melaporkan ukuran workspace pada build aktual.

Adapter memeriksa seluruh pixel keluar satu kali dan konsumsi bit tidak melewati
entropy yang tersedia. Pemeriksaan entropy bergantung pada layout state JPEGDEC
yang dipin, dengan static assertion saat compile; perubahan commit dependency
harus disertai validasi ulang integrasi. Ini menangani kasus decoder melaporkan
sukses pada stream terpotong meski EOI masih tersedia.

Perintah `i` mendecode [test_bars.jpg](examples/media/test_bars.jpg) 32x16 sekali,
kemudian merendernya bergiliran FIT/FILL/CROP/CENTER/STRETCH. Render tetap ke
memori, belum output HUB75. Kegagalan pertama dapat dicoba ulang dengan `i`.

Tes komputer meliputi toleransi warna JPEG lossy, grayscale, 4:2:0/4:2:2 dengan
dimensi bukan kelipatan MCU, ukuran maksimum dan refill buffer entropy, semua
prefix file terpotong, entropy terpotong dengan EOI tersisa, progressive, sumber
oversize, input yang diubah setelah decode, serta kegagalan kedua alokasi dan ownership.

Fixture dan header built-in dapat dibuat ulang menggunakan
`py tools/generate-test-jpeg.py`. Generasi memerlukan Pillow 12.3.0; dependency
ini hanya untuk generator, bukan build firmware atau tes normal. Contoh instalasi
lokal proyek: `py -m pip install --target .build-temp/python-packages --no-cache-dir Pillow==12.3.0`.

## Struktur dan tahap berikutnya

- `src/board/`: satu sumber pinout dan kapasitas memori baseline.
- `src/main.cpp`: diagnostik M0; tidak menyimpan kredensial Wi-Fi.
- `src/display/panel_profile.*`: parser, validasi, export, dan RGB order.
- `src/display/framebuffer.*`: double buffer dengan alokasi transaksional.
- `src/display/diagnostics.*`: pola logical canvas; belum mengendalikan pin.
- `src/display/panel_layout.*`: validasi/import/export layout dan pemetaan koordinat chain.
- `src/media/bmp.*`: decoder BMP view tanpa alokasi heap.
- `src/media/jpeg.*`: adapter JPEGDEC, decoding transaksional, image RGB565 owned.
- `src/media/image_renderer.*`: scaler dan render ke back buffer.
- `partitions.csv`: dua slot aplikasi 3 MiB, filesystem 9 MiB, cadangan sistem.
  Dua slot baru menyediakan ruang; implementasi OTA/rollback belum tersedia.
- M1/M2 setelah hardware tersedia: verifikasi buffer/PSU/panel/IC, pilih backend
  HUB75, lalu pola RGB/row/column/checkerboard dan pengukuran refresh.

Firmware saat ini hanya menahan OE HIGH sesudah setup GPIO; tidak menghasilkan CLK/LAT.
Saat reset sebelum firmware berjalan, blanking membutuhkan pull-up hardware
sesuai desain buffer. Pinout HUB75 di BoardProfile masih prototype dari PRD.

Referensi konfigurasi:
[manifest board PlatformIO](https://github.com/platformio/platform-espressif32/blob/master/boards/esp32-s3-devkitc-1.json),
[USB CDC Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/cdc_dfu_flash.html).
Kontrak BMP mengikuti [BITMAPINFOHEADER](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader)
dan [BITMAPFILEHEADER](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader).
# Media LittleFS (0.6.0)

Contoh media flash berada di `data/media/test_bars.bmp` dan `test_bars.jpg`.
LittleFS memakai partisi 9 MiB berlabel `spiffs`; isi partisi memakai format
LittleFS. Boot memanggil mount tanpa format otomatis. Jika mount gagal, contoh
built-in dan diagnostik tetap tersedia.

Buat image filesystem tanpa board:

```powershell
powershell -NoProfile -File tools/build.ps1 -Target buildfs
```

Ketika board tersedia, upload filesystem dengan port board yang sesuai:

```powershell
powershell -NoProfile -File tools/build.ps1 -Target uploadfs -UploadPort COM5
```

Upload tersebut mengganti seluruh isi partisi filesystem dengan folder `data`.
Firmware diupload terpisah. Tutup serial monitor sebelum upload.
Gunakan serial `f` untuk kapasitas/daftar media, `o` untuk BMP flash, dan `v`
untuk JPEG flash. Render memakai FIT pada canvas; output HUB75 belum tersedia.

Loader membaca file maksimal 1 MiB ke PSRAM, menerima pembacaan parsial,
dan mempertahankan gambar aktif ketika read/decode/alokasi gagal. Path API
dibatasi `/media/<nama>.bmp`, `.jpg`, atau `.jpeg` dengan ekstensi huruf kecil,
tanpa subdirektori/traversal. BMP mempertahankan byte sumber; JPEG melepas
byte file setelah decode RGB565 berhasil. Tes komputer memeriksa rollback,
pergantian format, kepemilikan memori, serta batas path/ukuran. Mount dan
pembacaan flash pada ESP32 masih menunggu pengujian hardware.

Konfigurasi filesystem mengikuti [dokumentasi PlatformIO ESP32](https://docs.platformio.org/en/latest/platforms/espressif32.html#uploading-files-to-file-system-spiffs).
