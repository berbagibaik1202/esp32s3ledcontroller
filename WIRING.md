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


GPIO17: tombol fisik konfirmasi preview scan. Sambungkan tombol normally-open
antara GPIO17 dan GND, INPUT_PULLUP, aktif LOW. Tekan singkat saat trial 30 detik
aktif untuk menyimpan scan/mapping yang sedang diuji. Di luar preview tombol
hanya mencetak log, tidak restart, tidak mengganti scan, tidak menulis flash.
Debounce 40 ms, satu event per tekanan, tombol ditahan sejak boot harus dilepas
sebelum dapat mengonfirmasi. Tombol Web Simpan scan tetap tersedia untuk memilih
scan langsung. Brightness tetap 1/255. Uji fisik tombol masih perlu dilakukan.
