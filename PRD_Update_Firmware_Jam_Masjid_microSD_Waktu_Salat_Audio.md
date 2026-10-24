# PRD Update Firmware --- ESP32-S3 HUB75 Mosque Display Controller

## Penambahan microSD, Jadwal Waktu Salat, Koreksi Waktu, dan Audio Pengingat

**Versi PRD:** 1.0\
**Jenis pekerjaan:** Update inkremental firmware yang sudah ada\
**Target:** ESP32-S3 DevKit N16R8 --- 16 MB Flash, 8 MB PSRAM\
**Display:** LED RGB Matrix HUB75\
**Penyimpanan media tambahan:** microSD SPI\
**Antarmuka kontrol:** Web browser lokal\
**Status:** Spesifikasi untuk agen coding

------------------------------------------------------------------------

## 1. Tujuan Update

Update firmware yang sudah ada untuk menambahkan empat kemampuan utama:

1.  microSD sebagai penyimpanan file media.
2.  Jam dan informasi waktu salat dengan hitung mundur menuju waktu
    salat berikutnya.
3.  Koreksi waktu untuk setiap jadwal salat secara terpisah.
4.  Pemutaran file audio dari microSD sebelum waktu salat, berdasarkan
    jumlah menit yang dapat dikonfigurasi.

Update ini harus bersifat inkremental. Agen wajib memeriksa source code
dan mempertahankan fitur lama yang sudah bekerja, terutama driver HUB75,
board profile, pemetaan pin, konfigurasi panel, dan sistem kontrol yang
sudah tersedia. Jangan mengganti arsitektur atau membuat ulang seluruh
firmware kecuali benar-benar diperlukan dan telah didokumentasikan.

## 2. Aturan Pengembangan dan Kompatibilitas

-   Lakukan audit source code terlebih dahulu: framework, library
    display, board profile, web server, sistem konfigurasi, dan
    mekanisme build.
-   Pertahankan pemetaan HUB75 yang ada. Jangan mengganti pin hanya
    untuk memudahkan implementasi microSD.
-   Pertahankan endpoint/API dan format konfigurasi lama bila
    memungkinkan.
-   Jika skema konfigurasi harus berubah, buat migrasi versi skema yang
    mempertahankan pengaturan lama.
-   Tambahkan fitur dalam modul terpisah dan minimalkan perubahan pada
    jalur refresh display.
-   Jangan menyatakan fitur lolos jika baru selesai ditulis; bedakan
    status implemented, built, dan hardware-verified.
-   Sebelum mengubah file, buat ringkasan struktur proyek dan daftar
    file yang akan diubah.
-   Jangan menghapus atau menimpa media, playlist, maupun konfigurasi
    pengguna ketika firmware diperbarui.

## 3. Board dan GPIO

### 3.1 Profil board

-   Board: ESP32-S3 DevKit N16R8.
-   Flash: 16 MiB.
-   PSRAM: 8 MiB.
-   Nama profil yang telah ada: `ESP32S3_DEVKIT_N16R8_PROTOTYPE`.

### 3.2 Pemetaan HUB75 yang harus dipertahankan

  Sinyal                GPIO
  ------------------- ------
  R1                       4
  G1                       5
  B1                       6
  R2                       7
  G2                      15
  B2                      16
  A                        8
  B                        9
  C                       10
  D                       11
  E                       12
  CLK                     13
  LAT                     14
  OE                      18
  USB D− (reserved)       19
  USB D+ (reserved)       20
  Tombol konfirmasi       17

GPIO 19 dan 20 tetap dicadangkan untuk USB. GPIO 17 tetap menjadi tombol
konfirmasi. Semua pin HUB75 harus tetap sama kecuali pemilik proyek
menyetujui perubahan.

### 3.3 Pemetaan microSD SPI yang diusulkan

  Sinyal     GPIO
  -------- ------
  CS           21
  SCK          38
  MOSI         39
  MISO         40

Pemetaan microSD ini merupakan usulan implementasi dan wajib
diverifikasi terhadap varian board fisik, skematik, serta konflik
periferal sebelum digunakan. Jika salah satu pin tidak tersedia atau
berkonflik, agen harus melaporkan konflik dan meminta persetujuan
sebelum mengubahnya. Jangan memindahkan pin HUB75 untuk mengatasi
konflik tanpa persetujuan.

Pastikan modul microSD sesuai level tegangan 3,3 V pada antarmuka SD.
Jangan mengasumsikan semua adapter memiliki level shifter. Gunakan
ground bersama dan kabel SPI sependek mungkin untuk pengujian awal.

## 4. Fitur A --- microSD Media Storage

### 4.1 Kebutuhan fungsional

-   Inisialisasi microSD saat boot tanpa menggantungkan keseluruhan
    perangkat bila kartu tidak ditemukan.
-   Menampilkan status mounted/unmounted, kapasitas total, ruang kosong,
    dan error terakhir pada web UI.
-   Menyediakan operasi list, read, write, rename, dan delete sesuai
    kebutuhan aplikasi.
-   Menyimpan media besar di microSD, bukan di flash internal.
-   Menyimpan konfigurasi kritis dan web UI di flash internal jika pola
    firmware saat ini mendukungnya.
-   Menggunakan streaming/chunked upload agar file tidak perlu dimuat
    seluruhnya ke RAM.
-   Menolak upload bila ruang penyimpanan tidak mencukupi.
-   Memvalidasi nama file dan mencegah path traversal.
-   Tidak menghapus file yang sedang dipakai oleh playlist tanpa
    konfirmasi dan penanganan playback.
-   Menangani kartu tidak terpasang, gagal mount, kartu penuh, file
    rusak, dan kegagalan operasi file tanpa reboot loop.
-   Mempertahankan file media pada update firmware.

### 4.2 Struktur direktori yang disarankan

``` text
/sd/
  /media/
    /images/
    /audio/
    /video/
  /playlists/
  /config/
  /logs/
```

Jika firmware sudah mempunyai struktur penyimpanan yang mapan, gunakan
struktur tersebut dan dokumentasikan lokasi media aktual; jangan
memindahkan file pengguna secara otomatis.

### 4.3 Format file

-   Gambar: pertahankan format yang telah didukung firmware.
-   Audio: prioritaskan WAV PCM dan/atau MP3 hanya jika decoder dan
    jalur output audio yang dipilih tersedia.
-   Video: fitur video lama tidak boleh rusak. Dukungan format tambahan
    bukan bagian wajib update ini.
-   Nama file harus aman untuk filesystem dan API.

### 4.4 Web UI Media Manager

Tambahkan atau perluas halaman media agar pengguna dapat: - Melihat
daftar file media. - Mengunggah file dari HP/PC. - Memfilter gambar,
audio, dan video. - Melihat ukuran file serta kapasitas microSD. -
Menghapus file dengan dialog konfirmasi. - Memilih file audio untuk
event pengingat salat. - Melihat pesan upload berhasil/gagal dan alasan
kegagalan.

## 5. Fitur B --- Waktu dan Jadwal Salat

### 5.1 Tampilan

Tambahkan komponen yang dapat digunakan pada layout display: - Jam
digital. - Tanggal Masehi. - Tanggal Hijriah bila sumber/algoritma yang
dipilih tersedia. - Nama waktu salat berikutnya. - Jam waktu salat
berikutnya. - Hitung mundur menuju waktu salat berikutnya. - Status
audio pengingat yang akan diputar jika dikonfigurasi.

Countdown harus dihitung dari waktu aktual dan jadwal efektif yang sudah
menerapkan koreksi. Jangan hanya mengurangi angka counter setiap detik
karena hal itu dapat menyebabkan drift.

### 5.2 Waktu sistem

-   Zona waktu default: `Asia/Jakarta` (WIB), dapat dikonfigurasi jika
    firmware mendukung multi-zona.
-   Gunakan NTP ketika jaringan tersedia.
-   Tampilkan status waktu: tersinkron atau belum tersinkron.
-   Jika tidak ada jaringan dan board tidak memiliki RTC eksternal,
    jelaskan bahwa waktu dapat hilang atau tidak akurat setelah daya
    terputus.
-   Jangan menjalankan audio pengingat berdasarkan waktu yang belum
    valid tanpa memberi status yang jelas.
-   Perubahan zona waktu harus memperbarui perhitungan jadwal dan
    countdown secara konsisten.

### 5.3 Sumber jadwal salat

Implementasi harus menyediakan satu mode atau lebih yang eksplisit: 1.
**Jadwal manual:** pengguna memasukkan waktu salat untuk setiap hari
atau jadwal yang berlaku. 2. **Jadwal hasil perhitungan:** bila
library/perhitungan waktu salat yang cocok dengan firmware tersedia,
gunakan parameter lokasi dan metode yang eksplisit.

Jika jadwal otomatis digunakan, pengaturan minimum yang diperlukan: -
Latitude dan longitude. - Metode perhitungan atau otoritas yang
dipilih. - Zona waktu. - Parameter Asar bila relevan. - Koreksi lokal
jika dibutuhkan.

Jangan mengarang jadwal salat dan jangan menampilkan label "otomatis"
jika jadwal sebenarnya dimasukkan secara manual. Bila belum ada
implementasi perhitungan yang teruji, jadwal manual harus tersedia
sebagai baseline.

### 5.4 Waktu salat yang dikelola

Minimal: - Subuh - Zuhur - Asar - Magrib - Isya

Waktu Imsak, Syuruq, atau jadwal tambahan boleh ditambahkan jika
didukung, tetapi tidak boleh mengacaukan definisi waktu salat wajib.
Syuruq bukan waktu salat fardu tersendiri.

### 5.5 Koreksi waktu per jadwal

Web UI harus menyediakan koreksi independen dalam satuan menit untuk
setiap jadwal:

  Jadwal         Koreksi
  -------- -------------
  Subuh      -/+ N menit
  Zuhur      -/+ N menit
  Asar       -/+ N menit
  Magrib     -/+ N menit
  Isya       -/+ N menit

Ketentuan: - Nilai positif menambahkan menit; nilai negatif mengurangi
menit. - Rentang nilai harus dibatasi dan divalidasi; nilai default
`0`. - Tampilkan waktu dasar, koreksi, dan waktu efektif pada UI agar
perubahan dapat diaudit. - Koreksi diterapkan tepat satu kali; jangan
menambahkan koreksi berulang setiap refresh. - Semua fitur yang
menggunakan jadwal---countdown, event audio, dan tampilan waktu---harus
mengacu pada waktu efektif yang sama. - Perubahan koreksi harus disimpan
secara persisten. - Setelah perubahan koreksi, hitung ulang countdown
dan jadwal audio. - Bila waktu efektif berubah dan event audio belum
terjadi, scheduler harus memperbarui event secara konsisten. - Bila
koreksi diubah setelah event hari itu sudah berjalan, jangan memutar
ulang event secara tidak sengaja.

## 6. Fitur C --- Audio Pengingat Sebelum Waktu Salat

### 6.1 Perilaku yang diinginkan

Pengguna dapat memilih file audio dari microSD dan menentukan berapa
menit sebelum masing-masing waktu salat audio mulai diputar.

Contoh konfigurasi: - Audio pengingat Subuh: 10 menit sebelum Subuh. -
Audio pengingat Zuhur: 5 menit sebelum Zuhur. - Audio pengingat Asar: 10
menit sebelum Asar. - Audio pengingat Magrib: 5 menit sebelum Magrib. -
Audio pengingat Isya: 10 menit sebelum Isya.

Contoh tersebut hanya ilustrasi. Semua nilai harus dapat diatur
pengguna, termasuk menonaktifkan audio pada jadwal tertentu.

### 6.2 Pengaturan per jadwal

Setiap waktu salat memiliki: - Enable/disable audio. - File audio dari
microSD. - Lead time: jumlah menit sebelum waktu efektif salat. - Volume
jika perangkat audio mendukung kontrol volume. - Mode playback jika
dibutuhkan: sekali atau pengulangan terbatas. - Status file valid/tidak
ditemukan.

Batas lead time harus dikonfigurasi secara masuk akal dan divalidasi.
Nilai awal yang disarankan adalah 10 menit, tetapi agen boleh memilih
default lain yang konsisten dengan UX dan mencatatnya.

### 6.3 Aturan pemutaran

-   Waktu pemicu = waktu salat efektif − lead time.
-   Waktu salat efektif = waktu dasar + koreksi jadwal.
-   Audio hanya dipicu satu kali per event per tanggal dan jadwal.
-   Scheduler tidak boleh memutar ulang audio akibat refresh halaman,
    reconnect Wi-Fi, atau reboot singkat.
-   Simpan identitas event terakhir yang sudah dipicu, misalnya
    tanggal + nama jadwal + konfigurasi event, sesuai keterbatasan
    persistensi yang tersedia.
-   Setelah boot, jangan memutar seluruh event terlewat secara beruntun.
    Jika perangkat menyala setelah waktu pemicu, lewati event yang sudah
    lewat kecuali pengguna secara eksplisit memilih kebijakan catch-up.
-   Jika waktu sistem belum valid, jangan memicu audio berdasarkan waktu
    yang tidak dapat dipercaya.
-   Jika file tidak ditemukan atau gagal decode, catat error, tampilkan
    peringatan di UI, dan lanjutkan operasi display.
-   Audio tidak boleh memblokir task refresh panel atau web server.
-   Jika audio sedang berjalan dan event lain terpicu, kebijakan default
    adalah tidak menumpuk audio; event baru dilewati atau mengikuti
    kebijakan antrean yang didokumentasikan.
-   Jangan mengasumsikan panel HUB75 menghasilkan suara. ESP32-S3
    memerlukan jalur output audio fisik.

### 6.4 Persyaratan hardware audio

Agen harus mengaudit hardware yang tersedia sebelum implementasi
playback. Kemungkinan jalur output: - I2S DAC/amplifier eksternal yang
sesuai, atau - Modul audio decoder/amplifier yang memang tersedia pada
perangkat.

Jangan menghubungkan speaker pasif langsung ke GPIO ESP32-S3. Jika belum
ada hardware audio, implementasikan konfigurasi, validasi file,
scheduler, dan status audio terlebih dahulu, lalu dokumentasikan bahwa
output suara membutuhkan modul eksternal. Jangan menyatakan audio
benar-benar terdengar sebelum diuji dengan perangkat output nyata.

### 6.5 Format audio

-   WAV PCM dapat dijadikan format baseline jika library playback yang
    dipilih mendukungnya.
-   MP3 dapat didukung jika decoder software yang kompatibel dengan
    framework dan kapasitas RAM tersedia.
-   Dokumentasikan sample rate, bit depth, channel, dan batas ukuran
    yang benar-benar didukung.
-   Jika format tidak didukung, tampilkan alasan yang jelas saat upload
    atau pemilihan file.
-   Jangan memuat seluruh audio besar ke RAM; baca/decode secara
    streaming atau gunakan buffer terbatas.

## 7. Web UI dan API

### 7.1 Halaman yang ditambahkan

1.  **Storage / Media**
    -   Status SD, kapasitas, ruang kosong.
    -   Daftar file, upload, hapus, dan kategori media.
2.  **Prayer Times**
    -   Sumber jadwal: manual atau perhitungan.
    -   Lokasi/metode bila perhitungan digunakan.
    -   Waktu dasar dan waktu efektif untuk setiap jadwal.
    -   Koreksi menit per jadwal.
3.  **Prayer Audio**
    -   Aktif/nonaktif per jadwal.
    -   Pilih file audio dari microSD.
    -   Lead time menit.
    -   Status file dan waktu pemicu berikutnya.
4.  **Display Layout**
    -   Menyediakan komponen jam, jadwal salat, countdown, dan informasi
        pengingat agar dapat ditempatkan sesuai sistem layout firmware
        yang sudah ada.
5.  **System Status**
    -   Status waktu/NTP, SD, audio subsystem, error terakhir.

### 7.2 API

Sesuaikan dengan pola API yang sudah ada. Jika belum ada, endpoint
berikut dapat dijadikan baseline:

  ------------------------------------------------------------------------------------
  Method                  Endpoint                             Fungsi
  ----------------------- ------------------------------------ -----------------------
  GET                     `/api/v1/storage/status`             Status dan kapasitas
                                                               microSD

  GET                     `/api/v1/media?type=audio`           Daftar audio

  POST                    `/api/v1/media/upload`               Upload media secara
                                                               streaming

  DELETE                  `/api/v1/media/{id}`                 Hapus media

  GET                     `/api/v1/prayer-times`               Jadwal dasar, koreksi,
                                                               dan waktu efektif

  PUT                     `/api/v1/prayer-times/settings`      Simpan sumber jadwal
                                                               dan parameter

  PUT                     `/api/v1/prayer-times/corrections`   Simpan koreksi per
                                                               jadwal

  GET                     `/api/v1/prayer-audio`               Konfigurasi audio dan
                                                               event berikutnya

  PUT                     `/api/v1/prayer-audio`               Simpan file, lead time,
                                                               dan enable per jadwal

  POST                    `/api/v1/prayer-audio/test`          Uji playback file yang
                                                               dipilih

  GET                     `/api/v1/clock/status`               Waktu sekarang dan
                                                               status sinkronisasi
  ------------------------------------------------------------------------------------

API harus memakai validasi input, respons error konsisten, dan
penyimpanan konfigurasi atomik. Jangan mengembalikan password atau
kredensial jaringan dalam respons.

## 8. Model Konfigurasi

Gunakan konfigurasi berversi dan sesuaikan dengan struktur konfigurasi
lama. Contoh model konseptual:

``` json
{
  "schemaVersion": 1,
  "timezone": "Asia/Jakarta",
  "prayerTimes": {
    "source": "manual",
    "location": {
      "latitude": -6.2,
      "longitude": 106.8
    },
    "method": "configured-method",
    "schedule": {
      "subuh": { "base": "04:35", "correctionMinutes": 0 },
      "zuhur": { "base": "11:45", "correctionMinutes": 0 },
      "asar": { "base": "15:05", "correctionMinutes": 0 },
      "magrib": { "base": "17:50", "correctionMinutes": 0 },
      "isya": { "base": "19:00", "correctionMinutes": 0 }
    }
  },
  "prayerAudio": {
    "subuh": { "enabled": true, "file": "audio/subuh.mp3", "leadMinutes": 10 },
    "zuhur": { "enabled": false, "file": "", "leadMinutes": 5 },
    "asar": { "enabled": false, "file": "", "leadMinutes": 10 },
    "magrib": { "enabled": false, "file": "", "leadMinutes": 5 },
    "isya": { "enabled": false, "file": "", "leadMinutes": 10 }
  }
}
```

Nilai waktu dalam contoh hanya placeholder demonstrasi dan tidak boleh
dianggap jadwal salat yang benar untuk lokasi tertentu. Implementasi
harus menggunakan format waktu yang konsisten, memvalidasi rentang, dan
memisahkan waktu dasar dari waktu efektif. Bila konfigurasi jadwal lama
sudah ada, lakukan migrasi dengan default aman; jangan mengganti semua
pengaturan dengan contoh di atas.

## 9. Arsitektur Implementasi

Tambahkan modul dengan pola yang sesuai repository: - `storage_manager`:
mount SD, status, dan operasi file. - `media_manager`: daftar media,
metadata, validasi dan lifecycle file. - `prayer_schedule`: sumber
jadwal, waktu dasar, koreksi, waktu efektif. - `clock_service`: waktu
sistem, NTP, timezone dan validitas waktu. - `prayer_countdown`: waktu
salat berikutnya dan countdown. - `prayer_audio_scheduler`: perhitungan
waktu pemicu dan deduplikasi event. - `audio_player`: adapter terhadap
library dan hardware output yang tersedia. - `web_ui` / `api_routes`: UI
dan API baru sesuai arsitektur yang ada. - `config_migration`: migrasi
skema dan penyimpanan atomik.

Nama modul dapat mengikuti konvensi proyek. Jangan memindahkan seluruh
driver display atau mengganti library HUB75 hanya untuk menambahkan
fitur ini.

## 10. Non-Functional Requirements

-   Rendering HUB75 harus tetap berjalan mulus saat SD diakses, file
    diunggah, atau audio diputar.
-   Operasi filesystem dan jaringan tidak boleh memblokir refresh
    display.
-   Gunakan buffer terbatas dan pertimbangkan PSRAM untuk data yang
    sesuai.
-   Konfigurasi bertahan setelah restart.
-   Kesalahan SD/audio tidak boleh menyebabkan reboot loop.
-   Upload file harus divalidasi dan tidak boleh merusak file yang sudah
    ada jika upload gagal.
-   UI harus responsif di ponsel dan desktop.
-   Fitur baru harus dapat dinonaktifkan secara terpisah bila terjadi
    masalah.
-   Log harus mencatat timestamp jika waktu valid, jenis error, dan
    modul terkait tanpa memuat kredensial rahasia.

## 11. Tahapan Implementasi

### Tahap 1 --- Audit dan baseline

-   Audit struktur source, versi framework, library HUB75, web UI/API,
    dan format konfigurasi.
-   Catat build awal dan fitur yang saat ini bekerja.
-   Buat daftar file yang akan disentuh.
-   Simpan baseline untuk regression test.

### Tahap 2 --- microSD

-   Verifikasi pin fisik.
-   Implementasikan mount/status/kapasitas dan operasi file.
-   Tambahkan Media Manager dan upload streaming.
-   Uji kartu hilang, penuh, file rusak, serta restart.

### Tahap 3 --- Waktu dan jadwal salat

-   Integrasikan sumber waktu dan zona waktu.
-   Tambahkan jadwal manual sebagai baseline.
-   Implementasikan koreksi independen per jadwal.
-   Tampilkan waktu dasar dan efektif.
-   Implementasikan countdown yang dihitung ulang dari waktu aktual.

### Tahap 4 --- Audio scheduler dan playback

-   Audit hardware output audio.
-   Implementasikan konfigurasi audio per jadwal dan pemilihan file SD.
-   Implementasikan lead time dan deduplikasi event.
-   Tambahkan tombol test playback.
-   Uji agar audio tidak memblokir display.

### Tahap 5 --- Integrasi UI dan regresi

-   Integrasikan komponen waktu salat dan countdown ke layout yang sudah
    ada.
-   Uji seluruh fitur lama.
-   Uji migrasi konfigurasi dan pemulihan setelah reboot.
-   Dokumentasikan keterbatasan decoder audio dan hardware.

## 12. Kriteria Penerimaan

### microSD

-   [ ] Kartu terdeteksi dan kapasitas tampil di UI.
-   [ ] File audio dapat diunggah dan dipilih dari daftar.
-   [ ] Upload tidak menampung seluruh file dalam RAM.
-   [ ] SD hilang/gagal mount menampilkan error tanpa reboot loop.
-   [ ] Update firmware tidak menghapus file media.

### Jadwal salat dan koreksi

-   [ ] Lima jadwal utama tersedia.
-   [ ] Sumber jadwal manual berfungsi.
-   [ ] Koreksi tiap jadwal dapat positif, nol, atau negatif sesuai
    batas validasi.
-   [ ] Waktu dasar, koreksi, dan waktu efektif terlihat di UI.
-   [ ] Countdown menggunakan waktu efektif dan tidak mengalami drift
    akibat counter mandiri.
-   [ ] Koreksi tersimpan setelah reboot.
-   [ ] Perubahan koreksi menghitung ulang countdown dan event audio.

### Audio pengingat

-   [ ] Pengguna dapat memilih file audio pada microSD untuk tiap
    jadwal.
-   [ ] Lead time dapat diatur per jadwal dalam menit.
-   [ ] Waktu pemicu sesuai waktu efektif salat dikurangi lead time.
-   [ ] Event hanya dipicu satu kali per jadwal per tanggal.
-   [ ] Reconnect web/Wi-Fi tidak memicu audio ulang.
-   [ ] Boot setelah waktu pemicu tidak memainkan event lama secara
    beruntun.
-   [ ] File hilang/format tidak didukung menghasilkan error yang jelas.
-   [ ] Audio tidak memblokir refresh panel dan web UI.
-   [ ] Playback terdengar pada hardware output audio yang nyata; jika
    hardware belum tersedia, fitur hanya ditandai software-implemented,
    bukan hardware-verified.

### Regression dan rilis

-   [ ] Driver HUB75 dan mapping pin tidak berubah.
-   [ ] Fitur lama yang relevan tetap bekerja.
-   [ ] Firmware berhasil dibangun.
-   [ ] Pengujian hardware dicatat terpisah dari hasil build.
-   [ ] Dokumentasi wiring microSD dan output audio tersedia.
-   [ ] Migrasi konfigurasi tidak menghapus pengaturan pengguna.

## 13. Deliverables Agen

1.  Perubahan source code yang terintegrasi ke repository firmware
    aktual.
2.  Daftar file yang ditambah dan diubah beserta alasan.
3.  Konfigurasi microSD dan wiring yang sudah diverifikasi atau diberi
    label belum diverifikasi.
4.  Web UI untuk media, jadwal salat, koreksi, dan audio.
5.  API dan dokumentasinya.
6.  Contoh konfigurasi berversi serta mekanisme migrasi.
7.  Log build dan hasil pengujian.
8.  Panduan setup microSD dan output audio.
9.  Daftar fitur implemented, built, dan hardware-verified.
10. Catatan keterbatasan dan langkah pengujian lanjutan.

## 14. Instruksi Akhir untuk Agen

Lakukan update pada firmware yang sudah ada, bukan rewrite total.
Pertahankan driver HUB75, board profile, pinout, konfigurasi panel, dan
fitur lama yang sudah berfungsi. Tambahkan microSD untuk media, jadwal
dan countdown waktu salat, koreksi menit independen untuk
Subuh/Zuhur/Asar/Magrib/Isya, serta pemutaran audio dari microSD pada N
menit sebelum masing-masing waktu salat.

Gunakan waktu salat efektif yang sudah menerapkan koreksi untuk
tampilan, countdown, dan audio scheduler. Pastikan event audio tidak
dipicu ganda. Audit jalur output audio fisik sebelum mengklaim playback
berhasil. Jangan mengarang jadwal salat, kompatibilitas format audio,
atau keberhasilan hardware. Implementasikan bertahap, jaga jalur
rendering HUB75 non-blocking, lakukan regression test, dan laporkan
secara eksplisit semua asumsi serta perubahan pin yang membutuhkan
persetujuan.
