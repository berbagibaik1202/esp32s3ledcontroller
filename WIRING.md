# Mapping GPIO untuk PCB ESP32-S3 N16R8

Mapping GPIO ini ditetapkan pada 9 Oktober 2026 sebagai acuan wiring dan PCB.
Sumber konfigurasi firmware: `src/board/esp32s3_n16r8.cpp`.
Gunakan nomor GPIO, bukan nomor urut pin pada header development board.

| Sinyal HUB75 | GPIO ESP32-S3 |
|---|---:|
| R1 | 4 |
| G1 | 5 |
| B1 | 6 |
| R2 | 7 |
| G2 | 15 |
| B2 | 16 |
| A | 8 |
| B | 9 |
| C | 10 |
| D | 11 |
| E (disediakan, penggunaan menunggu identifikasi panel) | 12 |
| CLK | 13 |
| LAT / STB | 14 |
| OE | 18 |
| GND | GND bersama ESP32, panel, dan PSU |

GPIO19 (USB D-) dan GPIO20 (USB D+) dicadangkan untuk USB.
Panel menggunakan suplai 5 V langsung dari PSU; jangan mengambil daya panel
dari ESP32. Rancangan menyediakan buffer 74AHCT245 dengan suplai 5 V dan
blanking OE saat reset. Detail rangkaian buffer dan blanking perlu diverifikasi
sebelum fabrikasi PCB.

Panel yang dilaporkan pengguna: P2.5, lebar 128 x tinggi 64 piksel, scan 1/16.
SMD2121/SMD1515 adalah deskripsi kemasan LED; IC driver belum diketahui.
Penetapan GPIO ini tidak menetapkan pemetaan pixel/baris atau driver IC.
Profil hardware aktif 128x64 scan 1/16; output DMA HUB75 sudah diimplementasikan.
Build uji terbaru memakai scan 1/32 dengan E=GPIO12 berdasarkan hasil
uji kolom bergantian. Scan aktual belum dikonfirmasi; spesifikasi pengguna
tetap tercatat 1/16. Brightness awal 1/255. Mapping/IC driver
masih memerlukan pengujian fisik.

Hubungkan sinyal menurut label konektor INPUT pada panel aktual. Nomor pin
konektor dan fungsi E perlu dikonfirmasi dari panel sebelum menyelesaikan PCB.
Status pinout UNVERIFIED pada firmware berarti belum diuji pada hardware;
assignment GPIO di tabel ini sudah menjadi acuan proyek.

---

## Mapping Modul microSD (SPI / FSPI)

Modul microSD terhubung ke bus SPI FSPI ESP32-S3:

| Sinyal microSD | GPIO ESP32-S3 | Keterangan |
|---|---:|---|
| CS | 21 | Chip Select (Output) |
| SCK / CLK | 47 | SPI Clock (Output - dipindah dari GPIO38 agar bebas dari onboard RGB LED) |
| MOSI / DI | 39 | Master Out Slave In (Output) |
| MISO / DO | 40 | Master In Slave Out (Input Pullup) |
| VCC | 5V / 3.3V | Jika modul memiliki chip regulator (AMS1117), hubungkan ke 5V (VIN). Jika modul pasif tanpa regulator, hubungkan ke 3.3V |
| GND | GND | Ground bersama |

*Catatan:*
1. Driver ESP32 FatFs **wajib FAT32** (tidak mendukung exFAT bawaan Windows untuk 64GB/128GB/256GB). Gunakan aplikasi gratis seperti **GUIFormat** (FAT32format) atau **Rufus** (pilih Non-bootable, Large FAT32) untuk memformat kartu 256GB ke FAT32.
2. Jika modul adapter memiliki IC regulator 3.3V dan diberi suplai 3.3V, tegangan akan drop ke ~2.6V sehingga kartu gagal merespons `CMD0`. Pindahkan VCC ke 5V (VIN/VBUS).

---

## Mapping Audio I2S (MAX98357A / I2S DAC Amplifier)

Audio pengingat salat menggunakan peripheral I2S DMA (`I2S_NUM_0`):

| Sinyal I2S Amp | GPIO ESP32-S3 | Keterangan |
|---|---:|---|
| BCLK / SCK | 41 | Bit Clock (Output) |
| LRC / WS | 42 | Word Select / Left-Right Clock (Output) |
| DIN / DOUT | 2 | Serial Data Output (Output) |
| VIN / VCC | 5V / 3.3V | 5V direkomendasikan untuk daya speaker 3W (MAX98357A) |
| GND | GND | Ground bersama |
| GAIN / SD_MODE | Sesuai modul | GAIN ke GND (12dB) / VCC (3dB), SD_MODE pullup mono mix |

---

## Kontrol Fisik & Tombol

GPIO17: tombol fisik konfirmasi preview scan. Sambungkan tombol normally-open
antara GPIO17 dan GND, INPUT_PULLUP, aktif LOW. Tekan singkat saat trial 30 detik
aktif untuk menyimpan scan/mapping yang sedang diuji. Di luar preview tombol
hanya mencetak log, tidak restart, tidak mengganti scan, tidak menulis flash.
Debounce 40 ms, satu event per tekanan, tombol ditahan sejak boot harus dilepas
sebelum dapat mengonfirmasi. Tombol Web Simpan scan tetap tersedia untuk memilih
scan langsung. Brightness tetap 1/255. Uji fisik tombol masih perlu dilakukan.

