## Firmware 0.7.0: preview konfigurasi panel

Gunakan Uji scan (preview 30 detik), lalu sambungkan kembali dan tekan
Hasil benar, simpan sebelum waktu habis. Batalkan/timeout/reset sebelum
konfirmasi mengembalikan konfigurasi committed melalui restart. Brightness
selalu 1/255. API panel memerlukan command=preview/confirm/cancel; instruksi
save langsung pada catatan lama diganti. Lihat PRD bagian 68 dan 70.

## Pemeriksaan koneksi hotspot (13 Oktober 2026)

Pengguna melaporkan HP ke hotspot ESP32 putus-putus. Log awal selama 22 detik
menunjukkan STA tidak dikonfigurasi, AP aktif, dan tidak ada reset berulang;
HP belum tersambung selama capture, jadi penyebab RF/HP belum terbukti.
Membuka port USB-UART untuk log dapat mereset board melalui DTR/RTS; boot
pertama pada capture bukan bukti restart spontan.

Firmware sekarang memakai AP-only jika SSID router kosong, Wi-Fi sleep
nonaktif, dan mempertahankan hotspot saat STA berhasil. Pemulihan STA mencoba
lagi setiap 30 detik tanpa full-channel scan; konfigurasi kosong tidak memicu
retry. Callback event hanya menulis counter atomic/log, tanpa mengubah Wi-Fi.
Status API/serial mencatat mode/channel/sleep, jumlah join/leave AP, disconnect
STA dan reason, serta RSSI STA. Tidak ada perubahan daya RF, scan panel, atau
brightness LED. UI mengulang request gagal dan baru menandai putus setelah
3 kegagalan polling; status/banner dipulihkan ketika request berhasil.

Penyebab koneksi HP masih memerlukan capture saat HP tersambung. Jika HP
menyebut jaringan tanpa internet, pilih tetap tersambung; jaringan controller
memang lokal. Restart setelah menyimpan scan tetap disengaja.

## Pilihan scan melalui UI controller

Pengaturan panel menyediakan scan 1/32 (ABCDE, E=GPIO12), 1/16 (ABCD), 1/8 (ABC), dan 1/4 (AB),
dengan empat mapping khusus 1/16. Default 1/32 adalah konfigurasi yang telah
lulus pengujian pengguna. Brightness tetap 1/255 pada semua pilihan.
Simpan scan & restart menulis satu record konfigurasi NVS `led-panel/config`,
kemudian restart setelah respons HTTP terkirim. Konfigurasi dimuat sebelum
inisialisasi canvas/output; record lama atau invalid memakai default 1/32.
Pengaturan media, animasi, dan Wi-Fi tetap terpisah.
`GET /api/panel` membaca pilihan tersimpan dan konfigurasi aktif;
`POST /api/panel` menerima scan=16/mapping=0..3, scan=32/mapping=4,
atau scan=4/8 dengan mapping=5 (bank baris linear).
Input lain menghasilkan HTTP 400 tanpa menulis konfigurasi atau restart.
Setelah restart, buka kembali halaman jika koneksi Wi-Fi perlu tersambung ulang.
Scan 1/4 dan 1/8 bersifat eksperimen dengan mapping bank baris linear.
DMA width=128*(32/scan), height=2*scan. Pin alamat tidak terpakai dilepas
dari bus DMA. Semua scan memakai brightness 1/255 dan default tetap 1/32.
Tes memeriksa cakupan semua 8192 pixel tanpa overlap pada bank linear
1/4, 1/8, 1/16, 1/32; ketepatan mapping fisik 1/4 dan 1/8 belum diuji.
Driver IC khusus belum didukung.

## Hasil uji panel terbaru (13 Oktober 2026)

Pengguna mengonfirmasi seluruh LED menyala setelah upload scan 1/32 ABCDE,
E=GPIO12, DMA 128x64 dengan mapping langsung. Konfigurasi ini dipertahankan
sebagai baseline hardware; brightness tetap 1/255. Pola merah penuh menguji
cakupan LED, tetapi belum membuktikan posisi pixel, urutan warna, atau seluruh
mapping gambar. Langkah berikutnya: RGB bars, row/column, dan checkerboard.

## Konfigurasi uji terbaru: scan 1/32 dengan E

Setelah uji pertama, pengguna melihat 16 kolom merah bergantian dengan 16 kolom
gelap, dan mengonfirmasi E tersambung GPIO12. Firmware uji berikutnya memakai
buffer DMA 128x64, scan 1/32 ABCDE, mapping langsung (status mapping=4).
Ini hipotesis pengujian, bukan konfirmasi bahwa spesifikasi scan 1/16 salah.
Brightness tetap 1/255. `q` tidak mengubah mapping dalam build uji ini.
Catatan scan 1/16 di bawah menjelaskan konfigurasi uji sebelumnya.

## Output HUB75 128x64 scan 1/16 (13 Oktober 2026)

Firmware sekarang menggunakan ESP32-HUB75-MatrixPanel-DMA 3.0.14, dipin ke
commit f17fb7fe9d487e9643f919eb5aeedea8d9d1f8d7. Bagian lama di bawah yang
menyebut output belum tersedia adalah catatan tahap pengembangan sebelumnya.

Panel aktif satu modul 128x64, ABCD scan 1/16, E tidak dipakai. Buffer listrik
DMA 256x32 dengan double buffering dan 6-bit per channel di RAM internal.
Canvas RGB565 tetap di PSRAM; transfer hanya dari Arduino loop ketika frame
berubah, sementara DMA terus melakukan refresh saat HTTP/decode berjalan.
Pin tetap mengikuti WIRING.md. Driver awal SHIFTREG, clock 8 MHz, phase false.

Brightness selalu 1/255 (perintah paling rendah yang tidak nol), ditetapkan
sebelum frame berwarna pertama dikirim. Inisialisasi DMA dimulai dengan buffer
hitam. Tidak ada kontrol yang menaikkan brightness. Serial `x` bergantian
blank 0/255 dan lanjut 1/255. `d` mengganti pola; `q` bergantian mapping
0=FOUR_SCAN_64PX_HIGH blok16, 1=blok32, 2=blok128, 3=bank16 linear.
Mapping awal 0; pilihan mapping sementara di RAM dan reset kembali 0.
Kesesuaian mapping/IC driver dengan panel aktual belum dibuktikan.

Web menampilkan status output sebenarnya. Layout aktif dibatasi satu modul;
konfigurasi media/animasi tersimpan tetap dipulihkan dengan layout satu modul.
Profil development 64x32 tetap tersedia untuk fixture tes software saja.
Tes mapping memeriksa semua 8192 pixel pada keempat mode tanpa overlap/lubang.

Upload lewat USB-UART (COM14 pada komputer ini):

```powershell
py -m platformio run -e esp32s3_n16r8_uart -t upload --upload-port COM14
```

Verifikasi 13 Oktober 2026: build USB/UART, tes host, dan tes Web UI lulus.
Upload UART ke COM14 berhasil dengan hash terverifikasi. Log sesudah reset
menunjukkan Flash 16 MiB, PSRAM 8 MiB, memory PASS, HUB75 READY, brightness
1/255, mapping 0, dan web server aktif. Pengamatan sampai 16 detik tanpa
reset berulang. Log lokal: `.build-temp/hub75-boot.log`. Tampilan fisik
masih menunggu pengamatan pengguna.

Upload aplikasi tidak menimpa LittleFS/NVS. Sesudah upload, periksa serial
115200 untuk `[PASS] HUB75 DMA` dan `brightness=1/255`, lalu periksa panel
secara visual. Bila gelap, periksa log dahulu; IC yang memerlukan inisialisasi
khusus belum dapat dipilih tanpa identifikasi IC.

Referensi mapping: https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/blob/3.0.14/src/ESP32-HUB75-VirtualMatrixPanel_T.hpp

# ESP32-S3 HUB75 LED Media Controller

Tahap sekarang: **M0 + profil, framebuffer, diagnostic, layout, dan gambar BMP/JPEG**,
belum driver output display atau playback terjadwal; Web UI lokal sudah tersedia. Firmware versi 0.6.0.
Spesifikasi produk ada di [prd.md](prd.md).

Mapping GPIO yang ditetapkan untuk wiring/PCB tersedia di [WIRING.md](WIRING.md).

## Wi-Fi STA dengan fallback AP

Salin `src/wifi_config.local.h.example` menjadi `src/wifi_config.local.h`,
lalu isi `LED_STA_SSID` dan `LED_STA_PASSWORD`. File lokal tersebut diabaikan Git.
Tanpa SSID tujuan, firmware langsung membuka AP:

- Nama: `ESP32S3-LED-Controller`
- Password: `ledpanel123`
- IP: lihat log serial (default softAP `192.168.4.1`).

Jika SSID dikonfigurasi, firmware melakukan scan asynchronous dan menghubungkan
STA jika SSID ditemukan. Jika ada jaringan hidden, firmware juga mencoba SSID
tujuan secara langsung. SSID tidak ditemukan, kegagalan scan, atau timeout
koneksi STA 15 detik akan membuka AP fallback. Kehilangan koneksi STA memicu
scan ulang. Serial `w` melakukan scan dan mencoba STA kembali; AP tetap tersedia
sampai STA berhasil mendapatkan koneksi, kemudian AP dimatikan.
Serial `s` menampilkan status STA/AP dan IP, tanpa mencetak password.
Web UI tersedia pada port HTTP 80 di alamat AP atau STA. Pengaturan dari Web UI
disimpan di NVS dan menjadi acuan saat boot berikutnya.
API mengikuti [dokumentasi Wi-Fi Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html).

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

## Web UI lokal

Hubungkan HP/komputer ke `ESP32S3-LED-Controller` dengan password `ledpanel123`,
lalu buka `http://192.168.4.1` menggunakan HTTP. Dalam mode STA, gunakan IP
perangkat yang tercetak di serial atau tercantum pada daftar klien router.

Fitur: status memori/Wi-Fi/LittleFS, preview canvas RGB565, pola berikutnya,
contoh BMP/JPEG, lima preset layout, daftar dan render media LittleFS, serta
pengaturan SSID/password STA yang tersimpan di NVS. Password tidak dikembalikan
melalui API. Setiap penyimpanan Wi-Fi memerlukan pengisian ulang password;
kosong berarti jaringan terbuka. AP tetap tersedia selama percobaan STA dan
berhenti saat STA berhasil terhubung. Upload/hapus BMP/JPEG melalui browser tersedia; output HUB75 belum tersedia.
UI disimpan dalam firmware sehingga tetap dapat dibuka ketika LittleFS gagal mount.

Pengujian HTTP hardware setelah komputer bergabung ke AP:
`py tools/test-web-hardware.py`. Tes menguji endpoint, preview, kontrol, media,
validasi request, dan fallback untuk SSID yang tidak tersedia; konfigurasi STA
kosong dikembalikan setelah tes fallback. Tes tidak mengganti SSID yang sudah diisi.
Backend menggunakan [WebServer Espressif](https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer)
dan [Preferences/NVS](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html).

## Pengelolaan media dan konfigurasi display

Web UI menyediakan upload satu BMP/JPEG per permintaan, progress upload, Preview,
dan Hapus dengan konfirmasi. Batas sumber 128x64 pixel, file 1 MiB, dan 64 file
media. Nama file hanya ASCII huruf/angka/underscore/dash/titik, dengan ekstensi
`.bmp`, `.jpg`, atau `.jpeg` huruf kecil. Nama yang sudah ada ditolak (HTTP 409),
bukan ditimpa. JPEG harus baseline dan BMP harus 24-bit BI_RGB.

Upload ditulis ke `/.media-upload.tmp`, diperiksa menggunakan decoder yang sama
dengan renderer, lalu dipindah ke `/media/<nama>` setelah seluruh request selesai.
File rusak, multipart dengan lebih dari satu file, dan transfer terputus tidak
masuk ke daftar media. File sementara dibersihkan saat abort/boot. Penulisan
menyisakan cadangan storage 32 KiB; storage/batas jumlah penuh menghasilkan HTTP
507. Pengecekan kapasitas tidak menggantikan pemeriksaan hasil write/rename.

Kontrol Web UI menyimpan preset layout dan pilihan media dalam satu record NVS.
Saat restart, preset dan gambar pilihan dipulihkan. Pola/gambar built-in tidak
menyimpan posisi animasi; saat dipilih, media flash yang tersimpan dikosongkan.
Pengaturan serial tetap untuk diagnostik dan tidak disimpan otomatis. Menghapus
file yang sedang dipilih menghapus pilihan persisten; preview saat ini tetap
tersimpan dalam RAM. File tidak ditemukan/rusak saat boot memakai pola diagnostik.

Tes hardware: `py tools/test-media-hardware.py` setelah bergabung ke AP. Tes reset
menggunakan COM14 dan profil Windows `Codex-LED-Web-Test`; sesuaikan helper untuk
komputer lain. Tes membuat file dengan prefix unik, lalu menghapus hanya file
buatannya dan mengembalikan layout/pilihan media semula. Recovery terhadap power
loss pada saat commit masih perlu diuji terpisah; restart normal bukan bukti
kelulusan seluruh acceptance recovery PRD.

Tes UI tanpa board: `node tools/test-web-ui.cjs`.

## Teks dan animasi

Web UI memiliki form Teks & animasi. Pilih gambar lewat Preview daftar media,
isi teks opsional, pilih efek teks/gambar, warna, scale 1-4, durasi/kecepatan, dan
loop, lalu Terapkan & simpan. Matikan Tampilkan gambar untuk teks saja.
Pause/Lanjutkan/Mulai ulang mengendalikan timeline. Pengaturan tersimpan di NVS;
restart memulai animasi dari nol. Gambar baru mengulang animasi yang aktif.

Efek: statis, fade, slide empat arah, blink, dan scroll empat arah. Font 5x7
memakai kapital untuk huruf kecil; glyph yang belum tersedia menjadi '?'.
Teks maksimal 192 byte UTF-8. Durasi 200-60000 ms dan scroll 1-200 pixel/detik.
Dua layer awal: gambar dan teks; clock/scene umum/playlist belum tersedia.
Rendering memakai waktu elapsed, back buffer, dan snapshot gambar PSRAM;
file tidak dibaca/di-decode tiap frame. Tick target 20 FPS dan preview browser
sekitar 4 FPS bukan benchmark hardware.

Tes: tools/test-host.ps1 dan node tools/test-web-ui.cjs. Setelah tes host,
jalankan py tools/generate-animation-preview.py untuk membuat preview HTML
mandiri di .build-temp/previews/animation-preview.html. Detail kontrak dan status
acceptance ada di PRD bagian 69. Firmware belum diupload sesuai arahan pengguna.


Update tombol Simpan scan: sesuai permintaan pengguna, UI tetap menyediakan
save langsung selain trial 30 detik. POST /api/panel command=save memvalidasi
scan/mapping, menghapus candidate lama bila ada, menulis record config, lalu
restart. Save tidak dibatasi timer dan tetap berlaku sesudah restart.
Trial/confirm/cancel tetap tersedia. Ini pengecualian eksplisit atas alur
preview wajib bagian 65, atas instruksi pengguna. Brightness tetap 1/255.


GPIO17: tombol fisik konfirmasi preview scan. Sambungkan tombol normally-open
antara GPIO17 dan GND, INPUT_PULLUP, aktif LOW. Tekan singkat saat trial 30 detik
aktif untuk menyimpan scan/mapping yang sedang diuji. Di luar preview tombol
hanya mencetak log, tidak restart, tidak mengganti scan, tidak menulis flash.
Debounce 40 ms, satu event per tekanan, tombol ditahan sejak boot harus dilepas
sebelum dapat mengonfirmasi. Tombol Web Simpan scan tetap tersedia untuk memilih
scan langsung. Brightness tetap 1/255. Uji fisik tombol masih perlu dilakukan.


Hotspot hilang saat idle: capture 45 detik tidak menunjukkan reboot; uptime
485-530 detik dan satu client tetap terhubung. Penyebab intermittent belum
terbukti. Firmware kini mencatat AP START/STOP, running sebenarnya, counter
start/stop dan reset_reason. Pemeriksaan tiap 5 detik memulihkan AP hanya jika
AP_STOP terdeteksi atau mode low-level tidak memiliki AP; AP sehat tidak
restart periodik. Restart disengaja untuk panel dicatat [SYSTEM].
Pengujian SSID/RF/interferensi/PSU masih perlu dilakukan jika log menunjukkan
AP tetap running ketika HP tidak melihat SSID. Ini bukan klaim masalah selesai.


Uji isolasi RF: pengguna mengonfirmasi SSID muncul kembali ketika DMA HUB75
dihentikan selama 30 detik. AP tetap running, tidak ada restart atau AP STOP
baru. Ini mendukung interferensi output terhadap Wi-Fi, bukan bukti gangguan
password atau watchdog. Firmware mencoba clock LCD 4 MHz dengan build flag
S3_LCD_DIV_NUM=40 (160 MHz / 40); metadata i2sspeed juga 4 MHz supaya estimasi
refresh konsisten. Sebelumnya selector HZ_8M pada library S3 dipetakan divider
16 = 10 MHz; label konfigurasi 8 MHz lama bukan pengukuran clock fisik.
Target min_refresh_rate diset 100, tetap perlu pengukuran refresh aktual.
Brightness tetap 1/255, scan/mapping tersimpan tidak berubah.
Perintah serial z menghentikan DMA 30 detik dan mengembalikan output otomatis.
SSID dan pola panel perlu diuji bersamaan pada clock baru. Jika gangguan
berlanjut, posisi antena/ribbon/GND/buffer PCB perlu diperiksa; jangan menutup
acceptance Wi-Fi berdasarkan pengamatan singkat.


Scrolling: batas render sebelumnya 50 ms (20 FPS) + loop delay 10 ms diganti
interval 16 ms (~60 FPS target) dan loop yield 1 ms. Timeline elapsed tetap,
sehingga speed pixel/detik tidak berubah. Clock output tetap 4 MHz untuk
mempertahankan hasil uji RF, brightness 1/255. Status output_fps menghitung
handoff frame ke DMA per jendela 2 detik, transfer_us waktu transfer terakhir.
Ini FPS media/frame submission, bukan pengukuran refresh panel. FPS saat
frame statis nol adalah normal. Target tidak dianggap tercapai tanpa log
saat animasi aktif; kelancaran visual/Wi-Fi perlu dinilai pengguna.


Hasil setelah upload perbaikan scrolling: animasi tersimpan aktif dan handoff
DMA terukur 49-50 FPS, transfer satu frame sekitar 13,3 ms pada capture serial.
AP running, brightness 1/255, tidak ada client Wi-Fi selama pengukuran ini.
Ini bukan benchmark ketika browser/upload aktif atau acceptance 24 jam.
Target ~60 belum dicapai penuh; capaian nyata 49-50 FPS dicatat tanpa klaim 60.
