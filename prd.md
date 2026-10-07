# PRODUCT REQUIREMENT DOCUMENT

## ESP32-S3 Universal HUB75 Standalone LED Media Controller

**Versi:** 0.3  
**Target MCU:** ESP32-S3 DevKit N16R8  
**Flash:** 16 MB  
**PSRAM:** 8 MB  
**Display Interface:** HUB75 / HUB75E compatible  
**Panel pertama untuk development:** P2.5 RGB  
**Status:** Prototype / Development

---

# 1. VISI PRODUK

Membangun universal LED media controller berbasis ESP32-S3 yang mampu mengendalikan berbagai jenis RGB LED matrix HUB75 secara standalone.

Controller tidak boleh terikat pada:

- P2.5;
- satu resolusi;
- satu scan rate;
- satu susunan panel;
- satu driver IC;
- satu pixel mapping.

Targetnya adalah:

```text
             UNIVERSAL HUB75 CONTROLLER
                       │
        ┌──────────────┼──────────────┐
        ▼              ▼              ▼
      P2.5             P4             P5
        │              │              │
      HUB75          HUB75          HUB75
        │              │              │
        └──────────────┼──────────────┘
                       ▼
                  ESP32-S3
```

Pitch panel hanya menjadi metadata fisik.

---

# 2. PANEL YANG DITARGETKAN

Target jangka panjang mencakup:

- P2 / P2.5
- P3
- P4
- P5
- P6
- P8
- P10 RGB

dengan syarat panel menggunakan interface yang dapat ditangani HUB75 engine.

Contoh:

| Panel | Resolusi modul | Scan | Target |
|---|---:|---:|---|
| P2.5 | 64×32 | 1/16 | Ya |
| P2.5 | 64×64 | 1/32 | Ya |
| P3 | 64×32 | 1/16 | Ya |
| P4 | 64×32 | 1/16 | Ya |
| P5 | 64×32 | 1/8 | Ya |
| P5 | 32×32 | 1/8 / 1/16 | Ya |
| P6 | 32×32 | 1/8 | Ya |
| P10 RGB | 32×16 / 32×32 | bervariasi | Target |

Daftar tersebut bukan hardcoded compatibility list.

Panel baru dapat ditambahkan melalui **Panel Profile**.

---

# 3. PRINSIP UTAMA

Firmware harus memisahkan:

```text
MEDIA
   ↓
CANVAS
   ↓
LAYOUT
   ↓
PANEL GEOMETRY
   ↓
PIXEL MAPPING
   ↓
SCAN MAPPING
   ↓
DRIVER
   ↓
HUB75
```

Dengan demikian:

> Media Engine tidak perlu mengetahui apakah display menggunakan P2.5, P4 atau P5.

Media Engine hanya mengetahui:

```text
Virtual Canvas
Width × Height
```

---

# 4. ARSITEKTUR SISTEM

```text
              CONTENT SOURCES
                     │
     ┌───────────────┼────────────────┐
     ▼               ▼                ▼
   IMAGE           VIDEO             TEXT
 JPEG/BMP        GIF/MJPEG       Clock/Running
     │               │                │
     └───────────────┼────────────────┘
                     ▼
                MEDIA ENGINE
                     │
                     ▼
                SCENE ENGINE
                     │
                     ▼
               LAYER ENGINE
                     │
                     ▼
              VIRTUAL CANVAS
                     │
                     ▼
               LAYOUT ENGINE
                     │
                     ▼
               PANEL PROFILE
                     │
          ┌──────────┼──────────┐
          ▼          ▼          ▼
       Geometry   Mapping     Driver
          │          │          │
          └──────────┼──────────┘
                     ▼
                SCAN ENGINE
                     │
                     ▼
               HUB75 ENGINE
                     │
                     ▼
               LED DISPLAY
```

---

# 5. TARGET HARDWARE

MCU:

```text
ESP32-S3 N16R8
```

Resources:

```text
Flash : 16 MB
PSRAM : 8 MB
Wi-Fi : 2.4 GHz
USB   : Native USB OTG
```

PSRAM digunakan untuk:

- framebuffer;
- double buffering;
- image decoding;
- MJPEG decoding;
- media buffers;
- scene composition;
- scaling;
- temporary buffers.

---

# 6. UNIVERSAL PANEL MODEL

Sebuah panel tidak didefinisikan hanya berdasarkan pitch.

Panel Profile terdiri dari:

```text
PanelProfile
│
├── Identification
├── Geometry
├── Interface
├── Scan
├── Addressing
├── RGB Mapping
├── Pixel Mapping
├── Driver IC
└── Timing
```

---

# 7. PANEL IDENTIFICATION

Metadata:

```text
name
manufacturer
model
pitch
description
```

Contoh:

```text
Name:
P5 Outdoor RGB 64x32

Pitch:
5 mm
```

Pitch hanya digunakan sebagai informasi.

Pitch **tidak menentukan driver**.

---

# 8. PANEL GEOMETRY

Parameter:

```text
physical_width_mm
physical_height_mm

width
height
```

Contoh:

```text
P2.5

Physical:
160 × 80 mm

Pixel:
64 × 32
```

atau:

```text
P5

Physical:
320 × 160 mm

Pixel:
64 × 32
```

Walaupun ukuran fisik berbeda, keduanya dapat menghasilkan framebuffer:

```text
64 × 32
```

---

# 9. INTERFACE

Tahap pertama:

```text
HUB75
HUB75E
```

Future:

```text
CUSTOM_PARALLEL
```

Panel non-HUB75 tidak otomatis dianggap kompatibel.

---

# 10. HUB75 SIGNAL

Standard signal:

```text
R1
G1
B1

R2
G2
B2

A
B
C
D
E

CLK
LAT
OE
```

Firmware menyediakan seluruh:

```text
A B C D E
```

walaupun panel tertentu hanya menggunakan:

```text
A B C
```

atau:

```text
A B C D
```

---

# 11. MULTI-SCAN ENGINE

Target scan:

```text
1/4
1/8
1/16
1/32

CUSTOM
```

Future:

```text
1/64
```

Scan tidak boleh dihitung hanya berdasarkan tinggi panel.

Contoh:

```text
64×32
```

tidak otomatis berarti:

```text
1/16
```

Profile panel menentukan scan sebenarnya.

---

# 12. ADDRESS LINES

Profile menentukan jumlah address line.

Contoh:

### 1/8

```text
A
B
C
```

### 1/16

```text
A
B
C
D
```

### 1/32

```text
A
B
C
D
E
```

Tetapi custom addressing tetap harus dimungkinkan.

---

# 13. PIXEL MAPPING

Firmware harus mendukung pixel mapping berbeda.

Mode awal:

```text
STANDARD
LINEAR
SERPENTINE
CUSTOM
```

Tujuannya menangani panel yang secara internal tidak menghubungkan pixel secara linear.

---

# 14. ROW MAPPING

Row mapping dipisahkan dari scan rate.

Contoh:

```text
Logical Row
     ↓
Row Mapper
     ↓
Physical Scan Row
```

Profile dapat memiliki mapping seperti:

```text
0 → 0
1 → 2
2 → 4
3 → 6
...
```

jika panel membutuhkan pola tertentu.

Dengan demikian panel aneh tidak membutuhkan perubahan Media Engine.

---

# 15. RGB ORDER

Profile mendukung:

```text
RGB
RBG
GRB
GBR
BRG
BGR
```

Contoh:

```text
rgb_order = GRB
```

dapat memperbaiki panel yang warna fisiknya berbeda tanpa mengubah aplikasi.

---

# 16. DRIVER IC

Driver dipisahkan dari pitch dan scan.

Contoh profile:

```text
GENERIC
ICN2038
ICN2038S
FM6124
FM6126A
ICN2053
CUSTOM
```

Daftar dapat diperluas.

Driver IC tertentu dapat mempunyai:

- initialization sequence;
- special latch timing;
- configuration register;
- brightness behaviour;
- PWM behaviour.

---

# 17. TIMING PROFILE

Parameter:

```text
clock_frequency

latch_pulse

oe_timing

row_settle_time

blanking_time
```

Default profile:

```text
SAFE
```

Advanced:

```text
FAST
CUSTOM
```

---

# 18. PANEL PROFILE FORMAT

Contoh profile P2.5 (schema v1; scan adalah penyebut rasio scan):

```json
{
  "schema_version": 1,
  "id": "p25_64x32_scan16",
  "name": "P2.5 RGB 64x32 1/16",
  "pitch": 2.5,

  "width": 64,
  "height": 32,

  "interface": "HUB75",

  "scan": 16,
  "address_lines": 4,

  "rgb_order": "RGB",

  "pixel_mapping": "STANDARD",
  "row_mapping": "STANDARD",

  "driver": "GENERIC",

  "timing": "SAFE"
}
```

---

# 19. CONTOH PROFILE P5

```json
{
  "schema_version": 1,
  "id": "p5_64x32_scan8_example",
  "name": "P5 RGB 64x32 1/8",
  "pitch": 5,

  "width": 64,
  "height": 32,

  "interface": "HUB75",

  "scan": 8,
  "address_lines": 3,

  "rgb_order": "RGB",

  "pixel_mapping": "STANDARD",
  "row_mapping": "STANDARD",

  "driver": "GENERIC",

  "timing": "SAFE"
}
```

Media Engine tidak berubah. Contoh P5 ini ilustratif, bukan bukti kompatibilitas hardware. STANDARD hanya dipakai jika diagnostic membuktikan mapping benar; panel 1/8 lain dapat memerlukan mapping berbeda.

---

# 20. CUSTOM PANEL PROFILE

Web UI menyediakan:

```text
Add Panel Profile
```

User dapat menentukan:

```text
Width
Height

Scan

Address Lines

RGB Order

Driver

Mapping

Timing
```

Profile dapat disimpan.

Panel baru tidak memerlukan kompilasi ulang jika driver, addressing, dan mapping yang diperlukan sudah didukung firmware. Driver atau protokol baru dapat memerlukan backend dan rilis firmware baru. Profile tidak menjalankan kode arbitrer.

---

# 21. PANEL DIAGNOSTIC MODE

Fitur penting untuk panel yang belum dikenal.

Diagnostic:

```text
Solid Red
Solid Green
Solid Blue

RGB Bars

Row Test

Column Test

Address Test

Scan Test

Checkerboard

Gradient

Pixel Walker
```

---

# 22. ADDRESS TEST

ESP32 menampilkan:

```text
A
B
C
D
E
```

secara bergantian.

Tujuan:

mengetahui address line yang benar-benar digunakan panel.

---

# 23. SCAN DISCOVERY

Advanced diagnostic menyediakan:

```text
Scan 1/4
Scan 1/8
Scan 1/16
Scan 1/32
```

User dapat melihat hasil fisik dan memilih konfigurasi yang benar.

---

# 24. RGB DISCOVERY

Firmware menampilkan:

```text
RED
GREEN
BLUE
```

Jika output menjadi:

```text
RED → GREEN
GREEN → BLUE
BLUE → RED
```

RGB order dapat diperbaiki melalui konfigurasi tanpa rewiring.

---

# 25. PORT MAPPING ESP32-S3

Baseline prototype:

| HUB75 | GPIO |
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
| E | 12 |
| CLK | 13 |
| LAT/STB | 14 |
| OE | 18 |

Mapping ini merupakan **development baseline**.

Mapping final harus diverifikasi terhadap board ESP32-S3 N16R8 yang digunakan.

---

# 26. USB RESERVED

Dipertahankan:

```text
GPIO19 → USB D-
GPIO20 → USB D+
```

Tidak digunakan HUB75.

Tujuan future:

```text
HDMI UVC
USB Storage
USB Maintenance
```

---

# 27. BOARD ABSTRACTION

Nomor GPIO tidak boleh tersebar di source code.

Gunakan:

```text
BoardProfile
```

Contoh:

```text
ESP32S3_DEVKIT_N16R8
CUSTOM_CONTROLLER_V1
CUSTOM_CONTROLLER_V2
```

Board Profile berisi:

```text
HUB75 pins
USB
SD
I2C
Sensors
Buttons
```

---

# 28. LEVEL SHIFTING

ESP32-S3:

```text
3.3 V logic
```

HUB75 panel umumnya:

```text
5 V logic
```

Prototype pendek dapat diuji secara hati-hati, tetapi hardware produk menggunakan buffer high-speed seperti:

```text
74AHCT245
```

atau equivalent yang sesuai.

Arsitektur:

```text
ESP32-S3
   │
 3.3V
   │
   ▼
BUFFER
   │
  5V
   │
   ▼
HUB75
```

---

# 29. POWER

Panel tidak mengambil daya dari ESP32.

```text
             5V PSU
                │
        ┌───────┼────────┐
        ▼       ▼        ▼
      PANEL   BUFFER    ESP32
```

Semua menggunakan:

```text
COMMON GROUND
```

---

# 30. VIRTUAL CANVAS

Media Engine menggambar ke:

```text
VirtualCanvas(width,height)
```

Contoh:

```text
256 × 64
```

Media Engine tidak peduli canvas tersebut dibangun dari:

```text
4 × P2.5
```

atau:

```text
4 × P5
```

selama total pixel sama.

---

# 31. MULTI-PANEL LAYOUT

Support:

```text
Horizontal

Vertical

Matrix

Serpentine

Custom
```

Contoh:

```text
[P1][P2]
[P4][P3]
```

dapat direpresentasikan sebagai:

```text
128 × 64
```

jika setiap panel:

```text
64 × 32
```

---

# 32. MIXED PANEL POLICY

Versi awal **tidak menargetkan pencampuran panel berbeda dalam satu chain HUB75**.

Contoh yang tidak disarankan:

```text
P2.5 + P5
```

dalam satu chain.

Satu output/chain menggunakan satu Panel Profile.

Future multi-output controller dapat menjalankan profile berbeda pada output berbeda.

---

# 33. FRAMEBUFFER

Format utama:

```text
RGB565
```

Memory:

```text
2 bytes / pixel
```

Contoh:

```text
64×32
4 KB

128×64
16 KB

256×64
32 KB

256×128
64 KB
```

PSRAM 8 MB digunakan untuk buffer besar.

---

# 34. DOUBLE BUFFER

Gunakan:

```text
Front Buffer
Back Buffer
```

Display membaca:

```text
Front
```

Media menggambar:

```text
Back
```

kemudian:

```text
swap()
```

Tujuan:

- mengurangi tearing;
- menjaga animasi;
- memisahkan decoder dan display refresh.

---

# 35. MEDIA ENGINE

Provider:

```text
ImageProvider

GIFProvider

MJPEGProvider

TextProvider

ClockProvider

AnimationProvider
```

Future:

```text
HTTPProvider

UVCProvider

StreamProvider
```

---

# 36. FORMAT MEDIA

Standalone V1:

```text
JPEG
BMP
GIF
MJPEG
```

PNG dapat ditambahkan setelah pengujian RAM/decoder.

Video utama:

```text
MJPEG
```

H.264/H.265 tidak menjadi decoder utama ESP32-S3.

---

# 37. MEDIA SCALER

Support:

```text
FIT

FILL

CROP

CENTER

STRETCH
```

Media source dapat memiliki resolusi berbeda dari Virtual Canvas.

---

# 38. SCENE ENGINE

Scene terdiri dari beberapa layer:

```text
Scene
│
├── Background
├── Image
├── Video
├── Logo
├── Text
├── Clock
└── Running Text
```

---

# 39. LAYER

Parameter:

```text
x
y

width
height

visible

opacity

z-index
```

Media compositor menghasilkan satu Virtual Canvas.

---

# 40. RUNNING TEXT

Support:

```text
LEFT
RIGHT
UP
DOWN
STATIC
```

Parameter:

```text
font
size
speed
color
position
loop
```

---

# 41. PLAYLIST

Playlist terdiri dari Scene.

```text
Scene A
   ↓
Scene B
   ↓
Video
   ↓
Promo
   ↓
Loop
```

---

# 42. SCHEDULER

Playlist dapat dipilih berdasarkan:

```text
Time
Day
Date
```

Contoh:

```text
06:00–10:00
MORNING

10:00–18:00
PROMO

18:00–22:00
EVENING

22:00–06:00
NIGHT
```

---

# 43. BRIGHTNESS

Support:

```text
0–100%
```

Mode:

```text
Manual

Schedule
```

Future:

```text
Ambient Light Sensor
```

---

# 44. STORAGE

V1:

```text
Internal Flash
```

V2:

```text
microSD
```

Future:

```text
USB Flash Drive
```

Internal flash digunakan untuk:

```text
Firmware
Configuration
Web UI
Small Media
Panel Profiles
```

---

# 45. WEB CONFIGURATION

First boot:

```text
LED-CONTROLLER-XXXX
```

Browser:

```text
192.168.4.1
```

Menu:

```text
Dashboard

Content

Scenes

Playlist

Schedule

Display

Panel Library

Panel Setup

Diagnostics

Network

System
```

---

# 46. PANEL LIBRARY

Panel Library menjadi fitur penting.

Contoh:

```text
P2.5
 ├── 64×32 1/16
 └── 64×64 1/32

P3
 └── 64×32 1/16

P4
 └── 64×32 1/16

P5
 ├── 64×32 1/8
 └── 32×32 1/8

CUSTOM
```

Profile dapat:

```text
Create
Edit
Duplicate
Delete
Export
Import
```

---

# 47. PANEL SETUP WIZARD

Untuk panel yang tidak dikenal:

```text
New Panel
    ↓
Resolution
    ↓
Scan Test
    ↓
Address Test
    ↓
RGB Test
    ↓
Mapping Test
    ↓
Driver
    ↓
Timing
    ↓
Save Profile
```

Ini merupakan fitur strategis agar controller benar-benar universal.

---

# 48. WIFI

Support:

```text
AP
STA
AP + STA
```

Kegagalan Wi-Fi tidak boleh menghentikan display.

---

# 49. FAIL-SAFE

Jika media rusak:

```text
Skip → Next
```

Jika playlist rusak:

```text
Default Scene
```

Jika Wi-Fi mati:

```text
Standalone playback continues
```

Jika konfigurasi panel rusak:

```text
Last Known Good Configuration
Jika tidak tersedia: blank display + Web UI recovery
```

---

# 50. FREERTOS ARCHITECTURE

Task utama:

```text
DisplayTask

MediaTask

SceneTask

StorageTask

NetworkTask

SystemTask
```

DisplayTask memiliki prioritas tinggi.

Network dan Web UI tidak boleh menyebabkan display flicker.

---

# 51. SOURCE CODE ARCHITECTURE

```text
src/
│
├── main.cpp
│
├── board/
│   ├── board_profile.h
│   └── esp32s3_n16r8.cpp
│
├── display/
│   ├── hub75_engine.cpp
│   ├── framebuffer.cpp
│   ├── panel_profile.cpp
│   ├── panel_layout.cpp
│   ├── pixel_mapper.cpp
│   ├── row_mapper.cpp
│   ├── scan_engine.cpp
│   └── driver_ic.cpp
│
├── panels/
│   ├── generic.cpp
│   ├── icn2038.cpp
│   ├── fm6126.cpp
│   └── custom.cpp
│
├── media/
│   ├── media_engine.cpp
│   ├── jpeg.cpp
│   ├── gif.cpp
│   └── mjpeg.cpp
│
├── scene/
│   ├── compositor.cpp
│   ├── scene.cpp
│   └── layer.cpp
│
├── playlist/
│   ├── playlist.cpp
│   └── scheduler.cpp
│
├── storage/
│   └── storage.cpp
│
├── network/
│   └── wifi.cpp
│
├── web/
│   ├── server.cpp
│   └── api.cpp
│
└── system/
    ├── config.cpp
    ├── ota.cpp
    └── watchdog.cpp
```

---

# 52. PANEL PROFILE DIRECTORY

Profile dapat disimpan terpisah:

```text
/panels/

p25_64x32_scan16.json

p25_64x64_scan32.json

p4_64x32_scan16.json

p5_64x32_scan8.json

custom_001.json
```

Sehingga menambah panel tidak memerlukan modifikasi Media Engine.

---

# 53. FUTURE HDMI INPUT

Arsitektur:

```text
HDMI SOURCE
     ↓
USB UVC CAPTURE
     ↓
ESP32-S3 USB HOST
     ↓
MJPEG
     ↓
Decoder
     ↓
Virtual Canvas
     ↓
Panel Engine
     ↓
HUB75
```

USB native tetap dipertahankan:

```text
GPIO19
GPIO20
```

---

# 54. FUTURE STREAMING

Media Gateway dapat menerima:

```text
YouTube
MP4
RTSP
OBS
Desktop Capture
Camera
```

dan mengirim stream yang telah disesuaikan ke ESP32.

ESP32 tetap menggunakan pipeline yang sama:

```text
Stream
   ↓
Media Provider
   ↓
Virtual Canvas
   ↓
Universal Panel Engine
```

---

# 55. DEVELOPMENT MILESTONE

## M0 — ESP32-S3 Bring-up

Verifikasi:

```text
Flash 16 MB
PSRAM 8 MB
USB
Wi-Fi
```

## M1 — HUB75 Electrical

Test:

```text
R
G
B

CLK
LAT
OE
```

## M2 — Standard Panel

Panel development pertama:

```text
P2.5
```

Test:

```text
RGB
Rows
Columns
```

## M3 — Multi Scan

Implement:

```text
1/8
1/16
1/32
```

## M4 — Panel Profile

Pindahkan seluruh karakteristik panel ke profile.

## M5 — Second Panel Type

Test dengan:

```text
P5 RGB
```

Tujuan:

membuktikan firmware tidak bergantung pada P2.5.

## M6 — Virtual Canvas

Framebuffer + double buffering.

## M7 — Multi Panel

Chain dan serpentine.

## M8 — Media

JPEG/BMP.

## M9 — Text

Static + running text + clock.

## M10 — Video

GIF/MJPEG.

## M11 — Scene Engine

Layer/compositor.

## M12 — Playlist/Scheduler

Standalone signage.

## M13 — Web UI

Panel Library + Media management.

## M14 — microSD

Storage besar.

## M15 — External Sources

HTTP/MJPEG/HDMI UVC.

---

# 56. ACCEPTANCE CRITERIA V1

Controller V1 dianggap berhasil apabila:

1. ESP32-S3 N16R8 dapat menjalankan HUB75.
2. P2.5 development panel bekerja stabil.
3. Panel P5 HUB75 dapat digunakan tanpa mengubah Media Engine.
4. 1/8, 1/16 dan 1/32 scan tersedia.
5. Panel profile dapat dipilih melalui konfigurasi.
6. Custom profile dapat dibuat.
7. RGB order dapat diubah.
8. Row/pixel mapping dapat diubah.
9. Multi-panel dapat dibuat.
10. JPEG dan BMP dapat ditampilkan.
11. Running text berjalan.
12. GIF/MJPEG dapat dimainkan.
13. Scene dapat memiliki beberapa layer.
14. Playlist dapat berjalan standalone.
15. Schedule dapat berjalan tanpa server.
16. Web UI dapat digunakan melalui HP.
17. Konfigurasi tersimpan setelah restart.
18. Wi-Fi terputus tidak menghentikan display.
19. Diagnostic panel tersedia.
20. USB native tetap tersedia untuk ekspansi.

---

# 57. DEFINISI PRODUK

Nama teknis proyek:

**ESP32-S3 Universal HUB75 Standalone LED Media Controller**

Bukan:

```text
P2.5 Controller
```

karena P2.5 hanyalah panel development pertama.

Arsitektur produk:

```text
                 MEDIA ENGINE
                      │
                 SCENE ENGINE
                      │
                 FRAMEBUFFER
                      │
                 VIRTUAL CANVAS
                      │
                 PANEL LAYOUT
                      │
                 PANEL PROFILE
                      │
        ┌─────────────┼─────────────┐
        │             │             │
     Geometry       Scan         Driver
        │             │             │
        └─────────────┼─────────────┘
                      │
                 PIXEL MAPPER
                      │
                  HUB75 ENGINE
                      │
        ┌─────────────┼──────────────┐
        ▼             ▼              ▼
      P2.5            P4             P5
```

Dengan arsitektur ini, penambahan panel baru dilakukan terutama melalui **Panel Profile dan Mapping Layer**, bukan dengan membuat ulang firmware.

---

# 58. PORT MAPPING DEVELOPMENT V0.2

```text
ESP32-S3                 HUB75/HUB75E

GPIO4   ───────────────► R1
GPIO5   ───────────────► G1
GPIO6   ───────────────► B1

GPIO7   ───────────────► R2
GPIO15  ───────────────► G2
GPIO16  ───────────────► B2

GPIO8   ───────────────► A
GPIO9   ───────────────► B
GPIO10  ───────────────► C
GPIO11  ───────────────► D
GPIO12  ───────────────► E

GPIO13  ───────────────► CLK
GPIO14  ───────────────► LAT/STB
GPIO18  ───────────────► OE


RESERVED:

GPIO19  ───────────────► USB D-
GPIO20  ───────────────► USB D+
```

Port mapping ini tetap dianggap **prototype mapping** sampai board ESP32-S3 N16R8 aktual diverifikasi.

---

# 59. PRINSIP PENGEMBANGAN

Setiap kali mendapatkan panel baru:

```text
Panel Baru
    │
    ▼
Identifikasi HUB75
    │
    ▼
Geometry
    │
    ▼
Scan Test
    │
    ▼
RGB Test
    │
    ▼
Row/Pixel Mapping
    │
    ▼
Driver Identification
    │
    ▼
Timing Test
    │
    ▼
Save Panel Profile
```

Jika berhasil:

```text
Panel Profile
     ↓
Panel Library
```

Panel tersebut kemudian dapat digunakan kembali tanpa proses discovery.

---

# 60. TARGET AKHIR

Controller diharapkan berkembang menjadi platform:

**Universal HUB75 + Standalone Media + Network Streaming + HDMI Input**

dengan ESP32-S3 sebagai controller utama dan Panel Profile sebagai abstraction layer yang memungkinkan satu firmware digunakan untuk berbagai jenis RGB LED matrix.

---

# 61. RUANG LINGKUP DAN BATAS V1

Bagian 61–67 menjadi acuan jika daftar target jangka panjang berbeda dari scope V1. Dukungan dinyatakan per kombinasi geometry, scan, mapping, driver, dan timing yang telah diuji; pitch bukan bukti kompatibilitas.

| Parameter | Target awal V1 |
|---|---|
| Output | 1 chain, satu Panel Profile |
| Baseline | Modul 64x32 1/16; model dan driver aktual dicatat saat bring-up |
| Canvas/chain | Maksimal 128x64, maksimal 4 modul 64x32 |
| Scan wajib | 1/8, 1/16, 1/32 pada panel aktual |
| Layer terlihat | Maksimal 4; maksimal 1 decoder animasi aktif |
| Playback | Target 15 FPS MJPEG 128x64 |
| Refresh display | Target minimal 100 Hz pada konfigurasi yang diterima |
| Media sumber | Maksimal 128x64; sumber lebih besar dikonversi sebelum upload |
| Upload | Maksimal 1 MiB per file dan tidak melebihi kuota tersedia |
| Storage | Internal flash; microSD V2 |

Angka ini target engineering sementara, bukan hasil benchmark. M2 mengukur refresh, M7 mengukur kapasitas chain, dan M10 mengukur playback saat compositor/jaringan aktif. Jika tidak tercapai, revisi PRD sebelum acceptance akhir; jangan diam-diam menurunkan kualitas untuk mengklaim lulus. FPS media berbeda dari refresh panel.

1/4, 1/64, custom addressing baru, driver baru, multi-output, HDMI UVC, USB storage, dan streaming eksternal bukan syarat V1. Custom profile V1 menggunakan backend tersedia serta tabel mapping yang tervalidasi. USB19/20 tetap dicadangkan; fungsi host/device masa depan perlu validasi tersendiri.

# 62. KONTRAK PANEL PROFILE DAN LAYOUT

Field wajib schema v1: `schema_version`, `id`, `name`, `width`, `height`, `interface`, `scan`, `address_lines`, `rgb_order`, `pixel_mapping`, `row_mapping`, `driver`, dan `timing`. Metadata opsional: manufacturer, model, description, pitch (mm), physical_width_mm, dan physical_height_mm. Width/height adalah integer pixel positif.

- Schema version tidak dikenal ditolak. ID unik, maksimal 64 karakter ASCII berupa huruf, angka, underscore, atau minus; bukan path bebas. Name maksimal 128 byte UTF-8.
- Scan V1 adalah 8/16/32. Addressing biner standar memakai 3/4/5 address lines masing-masing. Kombinasi lain ditolak tanpa backend yang mendukungnya.
- Enum dan driver harus tersedia dalam daftar kemampuan firmware. Driver tidak dikenal tidak boleh otomatis diganti GENERIC.
- STANDARD berarti mapping bawaan backend; LINEAR/SERPENTINE harus mempunyai transformasi terdokumentasi dan diuji.
- CUSTOM pixel mapping menambahkan `pixel_map`: tepat width*height integer unik dalam rentang 0..width*height-1. Indeks y*width+x menunjuk slot pixel backend; backend menerjemahkan slot ke lane RGB, row address, dan posisi shift. Topologi slot berbeda memerlukan backend baru.
- CUSTOM row mapping menambahkan `row_map`: tepat scan integer unik dalam rentang 0..scan-1, dari row address logis ke fisik. Ini bukan tabel seluruh baris pixel. Urutan baku: layout -> pixel slot -> row address -> RGB order -> driver/scan output. Row mapping tidak diterapkan dua kali.
- SAFE/FAST adalah preset timing backend yang telah diuji. CUSTOM menambahkan `timing_custom` dengan `clock_frequency_hz`, `latch_pulse_ns`, `row_settle_time_ns`, dan `blanking_time_ns`. Backend menetapkan rentang/resolusi, menolak nilai tidak didukung, dan melaporkan nilai efektif. Polaritas OE serta urutan blank/latch/address ditentukan backend.

Layout terpisah dari profile, berisi schema_version, panel_profile_id, canvas_width/height, serta daftar panel dengan chain_index, x, y, dan rotation (0/90/180/270). Chain index unik dan berurutan dari 0. Area panel setelah rotasi tidak boleh tumpang tindih atau keluar canvas; area kosong hitam. Serpentine chain berbeda dari mapping internal panel.

Import divalidasi sebelum disimpan. Export menyertakan schema version. Profile yang masih digunakan tidak boleh dihapus. Status profile dibedakan menjadi belum diuji dan tervalidasi pada hardware, beserta identitas panel dan hasil diagnostic.

# 63. ANGGARAN MEMORI DAN STORAGE

Perhitungan bagian 33 hanya untuk satu canvas RGB565. Pada 128x64, satu canvas 16 KiB dan double buffer 32 KiB. Buffer scan/output, decoder, compositor, queue, stack task, dan jaringan dihitung terpisah. Penempatan buffer mengikuti persyaratan DMA/backend; tidak semua alokasi diasumsikan cocok di PSRAM.

M6/M10 mencatat penggunaan, minimum free heap, dan blok kontigu terbesar pada internal RAM/PSRAM. Target margin awal: minimal 20% masing-masing heap yang tersedia setelah boot tetap bebas pada beban gabungan. Konfigurasi baru dialokasikan sebelum aktivasi; kegagalan mempertahankan konfigurasi lama dan menampilkan error. Queue harus dibatasi.

| Area flash | Anggaran sementara |
|---|---:|
| Bootloader, partition table, NVS, metadata OTA, cadangan sistem | 1 MiB |
| Slot aplikasi A | 3 MiB |
| Slot aplikasi B | 3 MiB |
| Filesystem Web UI, konfigurasi, profile, media | 9 MiB |

Ini anggaran 16 MiB, bukan partition CSV final. Offset/alignment dan ukuran build diverifikasi sebelum M8. Kuota media maksimal 6 MiB, dengan minimal 1 MiB filesystem tetap tersedia. Kuota efektif memperhitungkan overhead dan aset lain; dashboard menampilkan kapasitas aktual.

Upload melalui file sementara, validasi, lalu commit. Koneksi putus/storage penuh tidak mengganti file valid dengan file parsial. File yang dimainkan tidak dihapus sebelum player beralih. Konfigurasi ditulis saat disimpan, bukan setiap frame.

# 64. WAKTU DAN SCHEDULER

V1 memakai waktu sistem, NTP saat jaringan tersedia, dan timezone yang dapat diatur; default Asia/Jakarta. RTC eksternal dengan cadangan daya bukan scope V1. Setelah kehilangan daya, waktu dianggap tidak valid sampai disinkronkan atau diatur pengguna. Selama perangkat tetap menyala, waktu yang sudah valid terus berjalan tanpa Wi-Fi.

Saat waktu tidak valid, pakai default playlist dan brightness manual tersimpan; jadwal tidak dieksekusi. Clock menampilkan --:-- dan Web UI menampilkan status waktu. Setelah sinkronisasi, jadwal dievaluasi ulang tanpa reboot.

Interval awal inklusif, akhir eksklusif. Akhir lebih kecil dari awal berarti lintas tengah malam; filter hari/tanggal mengikuti hari mulai interval. Awal sama dengan akhir ditolak; gunakan opsi sepanjang hari. Konflik memakai priority terbesar, lalu ID secara leksikografis. Tanpa jadwal cocok, pakai default playlist.

Setiap item playlist menunjuk scene dan duration_ms positif. GIF/MJPEG mengulang selama durasi scene, tanpa audio. Perubahan jadwal memulai playlist baru pada batas frame berikutnya. Playlist kosong/semua item gagal memakai default scene bawaan tanpa file media. Evaluasi jadwal minimal setiap detik serta saat waktu atau konfigurasi berubah.

# 65. KONFIGURASI DAN PEMULIHAN

Perubahan profile/layout divalidasi dan dialokasikan sebelum preview dengan brightness awal maksimal 10%. Pengguna memiliki 30 detik untuk Simpan; timeout, gagal aktivasi, atau reboot sebelum konfirmasi mengembalikan konfigurasi tersimpan terakhir. Validasi schema tidak membuktikan mapping fisik benar; pengguna mengonfirmasi hasil diagnostic.

Simpan memakai commit atomik atau dua salinan dengan versi dan checksum. Kerusakan konfigurasi memakai salinan valid terakhir; jika tidak ada, output blank dan Web UI recovery tersedia. Startup menahan output blank sampai backend siap; firmware tidak menebak scan/driver/timing.

First boot memakai AP dengan password unik per perangkat yang tersedia melalui label atau serial maintenance. Konfigurasi, upload, dan update memerlukan autentikasi; password admin ditetapkan saat setup. Kredensial tidak masuk log/export biasa. Mekanisme reset/recovery ditentukan BoardProfile dan diuji; recovery jaringan mempertahankan media. Factory reset menjadi tindakan terpisah dengan konfirmasi.

OTA lokal melalui Web UI terautentikasi memakai image sesuai board dan slot aplikasi. Upload gagal mempertahankan firmware aktif. Image baru ditandai berhasil setelah startup/pemeriksaan konfigurasi; gagal boot rollback. Migrasi konfigurasi mempertahankan salinan yang dapat dibaca firmware sebelumnya. Dukungan rollback framework harus diverifikasi sebelum rilis.

# 66. KONTRAK RENDER DAN MEDIA

Decoder/compositor menulis back canvas; buffer aktif tidak ditulis. Frame lengkap diserahkan dengan ownership yang jelas. Jika backend memiliki buffer scan sendiri, konversi/swap scan dilakukan pada batas refresh yang didukungnya. Refresh aktual ditentukan backend, bukan diasumsikan berupa loop software DisplayTask.

Frame terlambat boleh dibuang agar waktu playback terjaga. Refresh memakai frame lengkap terakhir saat decoder/storage/network terlambat. Diagnostic menampilkan FPS media, refresh display, dropped frames, decoder error, dan minimum free heap.

- FIT: rasio tetap, seluruh sumber terlihat, area sisa memakai background.
- FILL: rasio tetap, memenuhi area, crop di tengah.
- CROP: tanpa scaling, crop berdasarkan posisi yang dikonfigurasi.
- CENTER: tanpa scaling, sumber di tengah; bagian di luar area dipotong.
- STRETCH: scaling independen kedua sumbu.

V1 memakai nearest-neighbor. Opacity layer 0..255; urutan z-index kecil ke besar, lalu urutan deklarasi untuk nilai sama. Alpha seragam per layer; transparansi GIF mengikuti decoder. PNG/per-pixel alpha belum V1. Teks UTF-8 memakai glyph font tersedia, glyph tidak tersedia diganti simbol; speed running text dalam pixel/detik.

JPEG V1 menerima baseline, progressive ditolak. BMP menerima uncompressed 24-bit. GIF diuji untuk transparansi, delay, loop, serta disposal mode yang dinyatakan didukung. MJPEG upload V1 adalah kontrak proyek `.mjpg`: rangkaian baseline JPEG lengkap berurutan tanpa audio, FPS 1..15 dalam metadata media. Parser membatasi ukuran frame dan memvalidasi decoder. AVI dan multipart HTTP bukan format upload V1.

File tidak didukung/rusak/dimensi berlebih ditolak saat import. Kerusakan playback melewati item gagal dan mencatat error; seluruh item gagal memakai default scene tanpa retry tanpa batas.

# 67. ACCEPTANCE TERUKUR

Bagian 56 adalah checklist fitur; tabel ini menetapkan bukti kelulusan. Catat firmware, board, panel/IC, PSU, buffer/kabel, layout, clock, bit depth, brightness, dan media untuk setiap hasil.

| Gate | Kriteria lulus |
|---|---|
| M0–M2 | Flash/PSRAM sesuai board; RGB/row/column benar; refresh baseline minimal 100 Hz |
| M3–M5 | Panel aktual 1/8, 1/16, 1/32 dan P5 menghasilkan pattern benar melalui pergantian profile/layout tanpa mengubah Media Engine |
| M4/M13 | Import/export round-trip mempertahankan field; schema/mapping/driver/timing invalid ditolak tanpa mengubah konfigurasi aktif |
| M6–M7 | 4 modul 64x32 membentuk 128x64, termasuk serpentine; sudut/batas modul benar dan margin memori bagian 63 tercapai |
| M8–M11 | JPEG/BMP/teks/GIF/MJPEG sesuai kontrak, maksimal 4 layer; MJPEG 128x64 rata-rata minimal 15 FPS selama 10 menit, dropped frames maksimal 5% |
| M12 | Waktu invalid, sinkronisasi ulang, jadwal overlap/lintas tengah malam sesuai bagian 64; perubahan efektif maksimal 2 detik dari batas jadwal |
| M13 | AP/STA/AP+STA melalui browser HP; autentikasi dan CRUD berfungsi; upload sambil playback tetap memenuhi target FPS dan refresh |
| Reliability | Playback campuran 24 jam termasuk putus/sambung Wi-Fi tanpa crash, watchdog reset, atau kehabisan memori |
| Recovery | Power loss saat save/upload, konfigurasi/media rusak, dan storage penuh pulih ke data valid/default/recovery blank; file parsial tidak dimainkan |
| OTA | Image salah ditolak; upload putus mempertahankan firmware aktif; gagal boot rollback dengan konfigurasi yang dapat dibaca |
| USB | GPIO19/20 tidak dipakai HUB75; USB maintenance pada board aktual diuji, UVC belum syarat V1 |

Refresh diukur dengan alat ukur sinyal atau counter backend tervalidasi. FPS dihitung dari frame lengkap yang diserahkan ke display. Tearing/ghosting diperiksa dengan pattern bergerak dan hasil visual dicatat. Daftar kompatibilitas hanya memuat kombinasi hardware yang telah diuji.

Sebelum rilis, target sementara bagian 61 harus dikonfirmasi lewat pengukuran atau direvisi eksplisit. HDMI UVC tetap eksplorasi sampai USB host, format capture, bandwidth, dan anggaran memori terbukti pada perangkat aktual.
---

# 68. STATUS IMPLEMENTASI — 7 OKTOBER 2026

Tambahan versi 0.6.0: filesystem LittleFS 9 MiB, mount tanpa format otomatis,
loader BMP/JPEG dari flash dengan batas file 1 MiB dan rollback gambar aktif.
Contoh media disertakan dalam folder data; buildfs/uploadfs tersedia melalui
tools/build.ps1. Pengelolaan/upload media melalui Web UI, persistensi scene,
dan recovery power loss saat menulis masih belum diimplementasikan. Tes host
memeriksa pembacaan parsial/terputus, pergantian format, alokasi gagal, serta
batas path dan ukuran. Mount dan baca flash nyata belum diuji.

Status berikut adalah progres implementasi, bukan pengganti acceptance criteria.

| Area/milestone | Progres software | Validasi hardware |
|---|---|---|
| M0 bring-up | Firmware diagnostik flash/PSRAM/Wi-Fi/USB; build USB dan UART berhasil | Menunggu ESP32 aktual |
| M1–M3 HUB75 dan multi-scan | BoardProfile tersedia; OE ditahan blank setelah startup; backend output belum dibuat | Menunggu board, panel, buffer, dan PSU |
| M4 PanelProfile | Parser/export schema v1, validasi geometry/addressing, enam RGB order; hanya STANDARD/GENERIC/SAFE | Profile development belum terbukti pada panel |
| M6 Virtual Canvas | RGB565 front/back, clipping, swap CPU, dan rollback alokasi; diuji di komputer | PSRAM runtime dan sinkronisasi backend belum diuji |
| Diagnostic | Sembilan pola dirender ke canvas; tersedia lewat serial dan preview PPM pada tes komputer | Scan/address discovery fisik belum tersedia |
| M7 Multi-panel | Grid horizontal/vertical/matrix/serpentine; custom placement/rotasi, validasi/import/export JSON, mapper canvas dan chain row-major; tes komputer lulus | Output chain/refresh fisik belum diuji |
| M8 Gambar | BMP 24-bit dan JPEG baseline grayscale/YCbCr, ImageSource RGB565, lima mode scaler, decode/alokasi transaksional, contoh built-in; tes komputer lulus. Pembacaan LittleFS tersedia; upload melalui browser belum tersedia | Output gambar pada panel dan performa decoder ESP32 belum diuji |
| M9–M15 | Teks, video, scene, scheduler, storage management, Web UI, OTA, dan sumber eksternal belum diimplementasikan | Belum diuji |

Firmware tahap ini versi 0.6.0. Tes komputer menjalankan kode C++ yang sama untuk profile, framebuffer, diagnostic, layout, BMP/JPEG, dan scaler. Validasi input, export round-trip, RGB order, isolasi/rollback buffer, koordinat pola, serpentine/rotasi, orientasi/padding BMP, baseline JPEG termasuk grayscale dan 4:2:0/4:2:2, penolakan progressive/data terpotong, serta lima mode gambar sudah lulus. Layout ke chain row-major merupakan tahap software, bukan buffer scan/DMA. Firmware belum menghasilkan clock/latch atau gambar pada HUB75.

JPEG memakai JPEGDEC dengan commit dipin; workspace dialokasikan di heap, candidate RGB565 dan coverage diperiksa sebelum commit. Gambar valid sebelumnya dipertahankan saat alokasi atau decoding gagal. Firmware menyediakan serial i untuk decode/render contoh JPEG; dapat membaca BMP/JPEG dari LittleFS melalui serial o/v. Peak memory dan performa decode ESP32 perlu diukur pada hardware sebelum M8 dinyatakan selesai penuh.

Preset layout melalui serial: satu panel, horizontal dua panel, vertical dua panel, matrix 2x2, dan serpentine 2x2. Layout hanya aktif dalam RAM; reboot kembali ke satu panel. Buffer/candidate layout divalidasi sebelum aktivasi dan kegagalan alokasi mempertahankan konfigurasi lama. Rotasi custom didefinisikan searah jarum jam. Area kosong tidak mempunyai tujuan pixel fisik. Implementasi mapping internal CUSTOM, driver lain, dan timing lain tetap belum tersedia.

Urutan berikutnya: uji M0 pada board aktual, identifikasi panel/IC, pilih backend HUB75, jalankan RGB/row/column/checkerboard, lalu ukur refresh M1/M2. Pekerjaan software lain boleh disiapkan sebelum hardware tersedia, tetapi milestone hardware tidak dinyatakan selesai berdasarkan compile atau preview.

Panduan build, tes komputer, dan perintah serial tersedia di README.md. Format CUSTOM dan driver tambahan pada bagian 62 tetap merupakan requirement; belum menjadi kemampuan firmware saat ini.
