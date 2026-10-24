#pragma once
#include <Arduino.h>

const char kWebUi[] PROGMEM = R"HTML(<!doctype html>
<html lang="id"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>LED Controller · ESP32-S3</title>
<style>
:root{color-scheme:dark;--bg:#0b0f17;--card:#151c28;--card-hover:#1c2536;--card-gold:rgba(255,209,102,0.06);--line:#27354a;--text:#eef3fa;--muted:#90a3bc;--accent:#5ef0bc;--accent-glow:rgba(94,240,188,0.35);--gold:#ffd166;--gold-glow:rgba(255,209,102,0.3);--cyan:#4cc9f0;--warning:#f5c67c;--danger:#ff7b7b}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
main{max-width:1150px;margin:auto;padding:24px 20px}
header{display:flex;justify-content:space-between;align-items:center;gap:16px;margin-bottom:22px;flex-wrap:wrap}
h1{font-size:26px;margin:4px 0;letter-spacing:-0.5px}h2{font-size:17px;letter-spacing:0.3px;margin:0 0 16px;color:#f0f5fc}p{line-height:1.6;color:var(--muted)}
.eyebrow{color:var(--accent);font-size:12px;letter-spacing:2px;font-weight:600}
.badge{border:1px solid var(--line);border-radius:30px;padding:6px 14px;font-size:13px;background:rgba(255,255,255,0.03);display:inline-flex;align-items:center;gap:6px}
.badge-gold{border-color:rgba(255,209,102,0.3);background:rgba(255,209,102,0.08);color:var(--gold)}
.badge-cyan{border-color:rgba(76,201,240,0.3);background:rgba(76,201,240,0.08);color:var(--cyan)}
.badge-accent{border-color:rgba(94,240,188,0.3);background:rgba(94,240,188,0.08);color:var(--accent)}
.grid{display:grid;grid-template-columns:1.15fr 1fr;gap:18px}
.card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:22px;box-shadow:0 8px 24px rgba(0,0,0,0.22);transition:border-color .2s ease}
.card:hover{border-color:#384b66}
.wide{grid-column:1/-1}
.metrics{display:grid;grid-template-columns:repeat(auto-fit,minmax(125px,1fr));gap:12px}
.metric{background:rgba(0,0,0,0.2);padding:12px 14px;border-radius:12px;border:1px solid rgba(255,255,255,0.04)}
.metric small{display:block;color:var(--muted);margin-bottom:6px;font-size:12px}
.metric strong{font-size:16px;color:#fff;overflow-wrap:anywhere}
.preview{min-height:200px;display:flex;align-items:center;justify-content:center;background:#070a10;border-radius:14px;padding:18px;margin-bottom:16px;border:1px solid var(--line)}
canvas{image-rendering:pixelated;width:100%;max-width:480px;height:auto;border-radius:4px;box-shadow:0 0 24px rgba(0,0,0,0.7)}
.row{display:flex;flex-wrap:wrap;gap:10px;align-items:center}
button,select,input{font:inherit;border-radius:9px;border:1px solid var(--line);padding:10px 12px;background:#101722;color:var(--text);transition:all .15s ease}
button{cursor:pointer;font-weight:550}
button.primary{background:var(--accent);color:#072118;font-weight:650;border-color:var(--accent);box-shadow:0 0 12px var(--accent-glow)}
button.gold{background:var(--gold);color:#2b2000;font-weight:650;border-color:var(--gold);box-shadow:0 0 12px var(--gold-glow)}
button.danger{background:#361717;color:var(--danger);border-color:#5a2323}button.danger:hover{background:#4d1f1f}
button:disabled{opacity:.45;cursor:wait}
button:hover:not(:disabled){filter:brightness(1.12);border-color:#4e678a}
button:focus-visible,input:focus-visible,select:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
label{display:block;color:var(--muted);font-size:13px;margin:12px 0 6px}
input{width:100%}input[type=checkbox]{width:auto;cursor:pointer}input[type=color]{width:64px;height:42px;padding:3px;cursor:pointer}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:26px;background:transparent;cursor:pointer;padding:0;border:none;margin:8px 0}
input[type=range]:focus{outline:none}
input[type=range]::-webkit-slider-runnable-track{width:100%;height:8px;background:#090e15;border-radius:4px;border:1px solid var(--line)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:22px;height:22px;border-radius:50%;background:var(--accent);border:2px solid #081d15;margin-top:-7px;box-shadow:0 0 12px var(--accent-glow);cursor:pointer;transition:transform .12s ease}
input[type=range]::-webkit-slider-thumb:hover{transform:scale(1.18)}
.btn-preset{flex:1;min-width:52px;padding:7px 9px;font-size:12px;border-radius:8px;border:1px solid var(--line);background:#101722;color:var(--text);font-weight:500;text-align:center}
.btn-preset:hover{border-color:var(--accent);color:var(--accent);background:#132230}
.btn-mode{flex:1;min-width:110px;padding:10px 12px;font-size:13px;border-radius:9px;border:1px solid var(--line);background:#101722;color:var(--text);font-weight:550;text-align:center}
.btn-mode:hover{border-color:var(--accent);color:var(--accent);background:#132230}
.btn-mode.active{background:var(--accent);color:#072118;font-weight:650;border-color:var(--accent);box-shadow:0 0 10px var(--accent-glow)}
.brightness-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px}
.brightness-badge{display:flex;align-items:baseline;gap:4px}
.hint{font-size:13px;line-height:1.55}
#notice{position:sticky;top:10px;z-index:10;background:#1b2d40;border:1px solid #3c5b7c;border-radius:10px;padding:12px 16px;margin-bottom:18px;box-shadow:0 6px 18px rgba(0,0,0,0.3)}
#notice[hidden]{display:none}.file{display:flex;justify-content:space-between;align-items:center;gap:14px;padding:12px 0;border-top:1px solid var(--line);flex-wrap:wrap}
.file span{overflow-wrap:anywhere}.error{color:#ff9e9e;border-color:#853737!important;background:#311b1b!important}
#outputNote{border-left:3px solid var(--warning);padding-left:14px;margin:16px 0 0;color:#f0e3ce}
footer{color:var(--muted);font-size:12px;margin-top:28px;text-align:center;padding:10px 0}
.wifi-item{display:flex;justify-content:space-between;align-items:center;padding:7px 10px;margin:5px 0;background:rgba(255,255,255,0.03);border:1px solid var(--line);border-radius:8px;gap:8px}
.wifi-item:hover{background:rgba(255,255,255,0.07)}.wifi-info{display:flex;align-items:center;flex-wrap:wrap;gap:6px;font-size:13px}
.wifi-badge{font-size:11px;padding:2px 6px;border-radius:5px;background:rgba(255,255,255,0.06);color:var(--muted)}
.btn-connect{padding:5px 12px;font-size:12px;border-radius:7px;background:var(--accent);color:#072118;font-weight:600;border-color:var(--accent);cursor:pointer}
/* Mosque, Prayer & Audio Styling */
.mosque-hero{background:linear-gradient(135deg,#121b27 0%,#182638 100%);border:1px solid rgba(255,209,102,0.25);border-radius:18px;padding:22px;margin-bottom:20px;position:relative;overflow:hidden}
.mosque-hero::after{content:"";position:absolute;top:-50%;right:-20%;width:300px;height:300px;background:radial-gradient(circle,rgba(255,209,102,0.08) 0%,transparent 70%);pointer-events:none}
.clock-digits{font-size:42px;font-weight:750;letter-spacing:1.5px;color:var(--gold);font-variant-numeric:tabular-nums;line-height:1}
.clock-date{font-size:15px;color:var(--muted);margin-top:8px}
.clock-hijri{font-size:14px;color:var(--accent);font-weight:600;margin-top:4px}
.countdown-box{background:rgba(0,0,0,0.3);border:1px solid rgba(255,209,102,0.2);border-radius:12px;padding:12px 18px;text-align:right}
.countdown-val{font-size:24px;font-weight:700;color:#fff;font-variant-numeric:tabular-nums}
.prayer-table{width:100%;border-collapse:collapse;margin:12px 0}
.prayer-table th,.prayer-table td{padding:10px 12px;text-align:left;border-bottom:1px solid var(--line)}
.prayer-table th{color:var(--muted);font-size:12px;letter-spacing:0.5px;text-transform:uppercase}
.prayer-table tr:hover td{background:rgba(255,255,255,0.02)}
.prayer-table tr.highlight td{background:rgba(255,209,102,0.08);color:#fff;font-weight:600}
.prayer-badge-lead{background:rgba(255,209,102,0.15);color:var(--gold);border:1px solid rgba(255,209,102,0.3);border-radius:6px;padding:2px 7px;font-size:11px}
.audio-card{background:rgba(0,0,0,0.16);border:1px solid var(--line);border-radius:12px;padding:14px;margin-bottom:10px}
.audio-card.active{border-color:rgba(76,201,240,0.4);background:rgba(76,201,240,0.04)}
.progress-bar-bg{height:8px;background:#0d141e;border-radius:4px;overflow:hidden;border:1px solid var(--line);margin:6px 0}
.progress-bar-fill{height:100%;background:var(--accent);border-radius:4px;transition:width .3s ease}
.progress-bar-fill.sd{background:var(--cyan)}
.tab-filter{display:inline-flex;background:#0d141e;border:1px solid var(--line);border-radius:9px;padding:3px;gap:3px;margin-bottom:12px}
.tab-btn{border:none;background:transparent;color:var(--muted);font-size:12px;padding:6px 12px;border-radius:7px;cursor:pointer}
.tab-btn.active{background:#1e2a3c;color:#fff;font-weight:600}
@media(max-width:760px){.grid{grid-template-columns:1fr}.metrics{grid-template-columns:1fr 1fr}header{align-items:flex-start}.card{padding:18px}h1{font-size:24px}.wide{grid-column:auto}.clock-digits{font-size:32px}.mosque-hero .row{flex-direction:column;align-items:flex-start;gap:14px}.countdown-box{text-align:left;width:100%}}
</style></head><body><main><header><div><div class="eyebrow">ESP32-S3 N16R8 · HUB75 CONTROLLER</div><h1>LED Matrix & Jam Digital Masjid</h1><span style="color:var(--muted)">Pengaturan panel HUB75, video stream, jadwal salat astronomis, audio pengingat, & microSD</span></div><span id="connection" class="badge">Menghubungkan…</span></header>
<div id="notice" role="status" aria-live="polite" hidden></div>

<!-- Mosque Digital Clock Hero Section -->
<section class="mosque-hero">
  <div class="row" style="justify-content:space-between;align-items:center">
    <div>
      <div class="clock-digits" id="heroClock">--:--:-- <span style="font-size:18px;color:var(--muted)">WIB</span></div>
      <div class="clock-date" id="heroDate">Memuat kalender…</div>
      <div class="clock-hijri" id="heroHijri">Kalender Hijriah</div>
    </div>
    <div class="countdown-box">
      <small style="color:var(--muted);display:block;margin-bottom:4px">WAKTU SALAT BERIKUTNYA</small>
      <div class="countdown-val" id="heroNextPrayer">—</div>
      <div class="row" style="margin-top:6px;gap:6px;justify-content:flex-end">
        <span class="badge badge-gold" id="heroCountdownBadge">T - --:--</span>
        <span class="badge badge-cyan" id="heroAudioBadge" style="display:none">🎵 Audio Siap</span>
      </div>
    </div>
  </div>
</section>

<div class="grid"><section class="card wide"><h2>Status perangkat & sistem</h2><div class="metrics">
  <div class="metric"><small>Memori PSRAM</small><strong id="memory">—</strong></div>
  <div class="metric"><small>Media flash</small><strong id="storage">—</strong></div>
  <div class="metric"><small>microSD Card</small><strong id="metricSd">—</strong></div>
  <div class="metric"><small>Waktu aktif</small><strong id="uptime">—</strong></div>
  <div class="metric"><small>Koneksi STA</small><strong id="sta">—</strong></div>
  <div class="metric"><small>Alamat IP</small><strong id="ip">—</strong></div>
  <div class="metric"><small>Kecerahan LED</small><strong id="metricBrightness">—</strong></div>
  <div class="metric"><small>Prioritas Tampilan</small><strong id="metricDisplayMode">—</strong></div>
  <div class="metric"><small>Jam & Salat</small><strong id="metricClock">—</strong></div>
  <div class="metric"><small>Audio I2S</small><strong id="metricAudio">—</strong></div>
  <div class="metric"><small>Video Stream</small><strong id="metricStream">—</strong></div>
  <div class="metric"><small>Canvas aktif</small><strong id="size">—</strong></div>
</div><p id="outputNote">Memeriksa output panel...</p></section>

<!-- Prioritas Mode -->
<section class="card wide"><h2>Prioritas & mode tampilan</h2><p class="hint" style="margin-top:0">Tentukan konten utama yang diprioritaskan untuk tampil pada panel HUB75:</p>
<div class="row" style="gap:8px;margin-bottom:12px">
  <button type="button" class="btn-mode" data-mode="0">Otomatis</button>
  <button type="button" class="btn-mode" data-mode="4">Jam Masjid</button>
  <button type="button" class="btn-mode" data-mode="1">Video Stream</button>
  <button type="button" class="btn-mode" data-mode="2">Animasi Teks</button>
  <button type="button" class="btn-mode" data-mode="3">Gambar Flash</button>
</div><p class="hint" id="modeDescription" style="border-left:3px solid var(--accent);padding-left:10px">Memuat konfigurasi mode...</p></section>

<!-- Manual Clock & Calendar Setting Card -->
<section class="card wide" style="border-color:rgba(76,201,240,0.3)">
  <div class="row" style="justify-content:space-between;margin-bottom:12px">
    <h2 style="margin:0;color:var(--cyan)">⏰ Pengaturan Jam, Tanggal & Kalender Hijriyah Manual</h2>
    <span id="clockSyncStatusBadge" class="badge badge-cyan">RTC / Jam Aktif</span>
  </div>
  <p class="hint">Anda dapat mengatur tanggal dan jam secara manual, atau menyinkronkan langsung dengan waktu browser/HP. Kalender Hijriyah dihitung otomatis (<strong>1448 H</strong>) dan dapat disesuaikan $\pm$ hari sesuai rukyah hilal lokal.</p>

  <form id="manualClockForm">
    <div class="grid" style="grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:14px;align-items:flex-end">
      <div>
        <label for="manualDate">Tanggal Masehi:</label>
        <input type="date" id="manualDate" style="width:100%">
      </div>
      <div>
        <label for="manualTime">Jam, Menit & Detik:</label>
        <input type="time" step="1" id="manualTime" style="width:100%">
      </div>
      <div>
        <label for="hijriAdjInput">Koreksi Kalender Hijriyah:</label>
        <div class="row" style="gap:6px;align-items:center">
          <input type="number" id="hijriAdjInput" min="-3" max="3" value="0" style="width:80px">
          <span class="hint">Hari (-3 s.d. +3)</span>
        </div>
      </div>
      <div class="row" style="gap:8px;margin-top:8px">
        <button class="primary" type="button" id="btnSaveManualClock">💾 Terapkan Jam Manual</button>
        <button type="button" class="gold" id="btnSyncTime2">🕒 Sinkron Jam HP / Browser</button>
      </div>
    </div>
  </form>
</section>

<!-- Mosque Clock & Prayer Times Setting Card -->
<section class="card wide" style="border-color:rgba(255,209,102,0.25)">
  <div class="row" style="justify-content:space-between;margin-bottom:12px">
    <h2 style="margin:0;color:var(--gold)">🕌 Jadwal Waktu Salat & Koreksi Menit</h2>
    <div class="row" style="gap:8px">
      <button type="button" id="btnSyncTime" style="font-size:12px;padding:6px 12px">🕒 Sinkronkan Jam Browser</button>
      <button type="button" id="btnUseGps" style="font-size:12px;padding:6px 12px">📍 Lokasi Perangkat (GPS/Browser)</button>
    </div>
  </div>
  <p class="hint">Penghitungan astronomis mengikuti standar Kemenag RI (Subuh -20°, Isya -18°, Asar panjang bayangan 1x, Ihtiyat pengaman). Anda dapat menambahkan koreksi independen (-30 s/d +30 menit) per waktu salat.</p>

  <form id="prayerSettingsForm">
    <div class="row" style="align-items:flex-end">
      <div><label for="calcSource">Metode Jadwal</label><select id="calcSource"><option value="auto">Astronomis Otomatis (Kemenag RI)</option><option value="manual">Manual / Input Sendiri</option></select></div>
      <div><label for="latitude">Lintang (Latitude)</label><input id="latitude" type="number" step="0.0001" value="-6.2088" style="width:140px"></div>
      <div><label for="longitude">Bujur (Longitude)</label><input id="longitude" type="number" step="0.0001" value="106.8456" style="width:140px"></div>
      <div><label for="timezoneName">Zona Waktu</label><select id="timezoneName" style="width:130px"><option value="WIB">WIB (UTC+7)</option><option value="WITA">WITA (UTC+8)</option><option value="WIT">WIT (UTC+9)</option></select></div>
      <button class="gold" type="button" id="savePrayerConfigBtn">Simpan Lokasi & Metode</button>
    </div>
  </form>

  <div style="overflow-x:auto;margin-top:16px">
    <table class="prayer-table" id="prayerTable">
      <thead>
        <tr>
          <th>Waktu Salat</th>
          <th>Jadwal Dasar</th>
          <th>Koreksi Menit (-30 s/d +30)</th>
          <th>Jadwal Efektif (Panel)</th>
          <th>Status</th>
        </tr>
      </thead>
      <tbody>
        <tr id="row_subuh"><td><strong>Subuh</strong></td><td id="base_subuh">--:--</td><td><div class="row" style="gap:6px"><input type="number" min="-30" max="30" value="0" id="corr_subuh" style="width:75px"><span class="hint">mnt</span></div></td><td id="eff_subuh"><strong>--:--</strong></td><td><span id="badge_subuh" class="badge">Menunggu</span></td></tr>
        <tr id="row_zuhur"><td><strong>Zuhur</strong></td><td id="base_zuhur">--:--</td><td><div class="row" style="gap:6px"><input type="number" min="-30" max="30" value="0" id="corr_zuhur" style="width:75px"><span class="hint">mnt</span></div></td><td id="eff_zuhur"><strong>--:--</strong></td><td><span id="badge_zuhur" class="badge">Menunggu</span></td></tr>
        <tr id="row_asar"><td><strong>Asar</strong></td><td id="base_asar">--:--</td><td><div class="row" style="gap:6px"><input type="number" min="-30" max="30" value="0" id="corr_asar" style="width:75px"><span class="hint">mnt</span></div></td><td id="eff_asar"><strong>--:--</strong></td><td><span id="badge_asar" class="badge">Menunggu</span></td></tr>
        <tr id="row_magrib"><td><strong>Magrib</strong></td><td id="base_magrib">--:--</td><td><div class="row" style="gap:6px"><input type="number" min="-30" max="30" value="0" id="corr_magrib" style="width:75px"><span class="hint">mnt</span></div></td><td id="eff_magrib"><strong>--:--</strong></td><td><span id="badge_magrib" class="badge">Menunggu</span></td></tr>
        <tr id="row_isya"><td><strong>Isya</strong></td><td id="base_isya">--:--</td><td><div class="row" style="gap:6px"><input type="number" min="-30" max="30" value="0" id="corr_isya" style="width:75px"><span class="hint">mnt</span></div></td><td id="eff_isya"><strong>--:--</strong></td><td><span id="badge_isya" class="badge">Menunggu</span></td></tr>
      </tbody>
    </table>
  </div>
  <div class="row" style="justify-content:space-between;margin-top:12px">
    <span class="hint" id="imsakSyuruqNote">Imsak: --:-- · Syuruq (Terbit): --:--</span>
    <button class="primary" type="button" id="saveCorrectionsBtn">💾 Simpan Koreksi Menit Waktu Salat</button>
  </div>

  <!-- Color Customization for Prayer Display -->
  <div style="margin-top:20px;padding:16px;background:rgba(255,255,255,0.03);border:1px solid var(--line);border-radius:10px">
    <div class="row" style="justify-content:space-between;align-items:center;margin-bottom:12px;flex-wrap:wrap;gap:8px">
      <div>
        <h3 style="margin:0;color:var(--gold);font-size:15px">🎨 Kustomisasi Warna Jadwal Salat Panel LED</h3>
        <p class="hint" style="margin:4px 0 0 0">Ubah warna teks nama salat dan jam agar lebih kontras dan menonjol di panel matrix.</p>
      </div>
      <button class="gold" type="button" id="btnSavePrayerColors">Simpan Warna Panel</button>
    </div>
    <div class="row" style="gap:24px;align-items:center;flex-wrap:wrap">
      <div>
        <label for="colorPrayerName" style="font-size:12px">Warna Nama Salat (Biasa)</label>
        <div class="row" style="gap:8px;align-items:center">
          <input type="color" id="colorPrayerName" value="#00ffff" style="width:44px;height:36px;padding:2px;cursor:pointer;border-radius:6px;border:1px solid var(--line)">
          <input type="text" id="hexPrayerName" value="#00FFFF" style="width:90px;font-family:monospace;text-transform:uppercase" maxlength="7">
        </div>
      </div>
      <div>
        <label for="colorPrayerTime" style="font-size:12px">Warna Jam Salat (Biasa)</label>
        <div class="row" style="gap:8px;align-items:center">
          <input type="color" id="colorPrayerTime" value="#ffffff" style="width:44px;height:36px;padding:2px;cursor:pointer;border-radius:6px;border:1px solid var(--line)">
          <input type="text" id="hexPrayerTime" value="#FFFFFF" style="width:90px;font-family:monospace;text-transform:uppercase" maxlength="7">
        </div>
      </div>
      <div>
        <label for="colorPrayerHighlight" style="font-size:12px">Warna Teks Sorotan</label>
        <div class="row" style="gap:8px;align-items:center">
          <input type="color" id="colorPrayerHighlight" value="#ffd100" style="width:44px;height:36px;padding:2px;cursor:pointer;border-radius:6px;border:1px solid var(--line)">
          <input type="text" id="hexPrayerHighlight" value="#FFD100" style="width:90px;font-family:monospace;text-transform:uppercase" maxlength="7">
        </div>
      </div>
      <div>
        <label for="colorPrayerCardBg" style="font-size:12px">Warna Background Kartu (Biasa)</label>
        <div class="row" style="gap:8px;align-items:center">
          <input type="color" id="colorPrayerCardBg" value="#000000" style="width:44px;height:36px;padding:2px;cursor:pointer;border-radius:6px;border:1px solid var(--line)">
          <input type="text" id="hexPrayerCardBg" value="#000000" style="width:90px;font-family:monospace;text-transform:uppercase" maxlength="7">
        </div>
      </div>
      <div>
        <label for="colorPrayerNextBg" style="font-size:12px">Warna Background Kartu (Sorotan)</label>
        <div class="row" style="gap:8px;align-items:center">
          <input type="color" id="colorPrayerNextBg" value="#081824" style="width:44px;height:36px;padding:2px;cursor:pointer;border-radius:6px;border:1px solid var(--line)">
          <input type="text" id="hexPrayerNextBg" value="#081824" style="width:90px;font-family:monospace;text-transform:uppercase" maxlength="7">
        </div>
      </div>
      <!-- Live Preview Mini Cards -->
      <div style="margin-left:auto">
        <label style="font-size:12px;margin-bottom:4px;display:block">Pratinjau Tampilan Panel:</label>
        <div class="row" style="gap:8px">
          <div id="previewCardPrayer" style="background:#000000;border-top:2px solid #1d3950;border-bottom:2px solid #1d3950;padding:6px 12px;border-radius:4px;text-align:center;min-width:75px">
            <div id="prevName" style="color:#00ffff;font-weight:bold;font-size:13px;letter-spacing:1px">IMSAK</div>
            <div id="prevTime" style="color:#ffffff;font-size:14px;font-weight:bold;font-family:monospace">04:12</div>
          </div>
          <div id="previewCardNext" style="background:#081824;border-top:2px solid #ffd100;border-bottom:2px solid #ffd100;padding:6px 12px;border-radius:4px;text-align:center;min-width:75px">
            <div id="prevNextName" style="color:#ffd100;font-weight:bold;font-size:13px;letter-spacing:1px">SHUBUH</div>
            <div id="prevNextTime" style="color:#ffd100;font-size:14px;font-weight:bold;font-family:monospace">04:35</div>
          </div>
        </div>
      </div>
    </div>
  </div>
</section>

<!-- Marquee Running Text Section -->
<section class="card wide" style="border-color:rgba(255,209,102,0.3)">
  <div class="row" style="justify-content:space-between;margin-bottom:12px">
    <h2 style="margin:0;color:var(--gold)">📢 Pesan Teks Berjalan (Running Text / Marquee)</h2>
    <span class="badge badge-gold">Baris Tengah LED</span>
  </div>
  <p class="hint">Tampilan waktu salat berikutnya & hitung mundur pada baris tengah panel LED akan bergantian secara otomatis dengan pesan teks berjalan ini.</p>

  <form id="marqueeForm">
    <div style="margin-bottom:12px">
      <label style="display:flex;align-items:center;gap:8px;cursor:pointer;font-weight:600;color:#fff">
        <input type="checkbox" id="marqEnabled" style="width:18px;height:18px" checked>
        Aktifkan pesan teks berjalan bergantian
      </label>
    </div>

    <label for="marqText">Isi Pesan Teks Berjalan:</label>
    <input type="text" id="marqText" style="width:100%" maxlength="160" placeholder="Contoh: LURUS DAN RAPATKAN SHAF · MOHON NONAKTIFKAN NADA DERING HP" value="LURUS DAN RAPATKAN SHAF · MOHON NONAKTIFKAN NADA DERING HP">

    <div class="grid" style="grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;margin-top:12px">
      <div>
        <label for="marqDirection">Arah Gerakan Marquee:</label>
        <select id="marqDirection" style="width:100%">
          <option value="left" selected>Ke Sebelah Kiri (Kanan ➔ Kiri - Standar)</option>
          <option value="right">Ke Sebelah Kanan (Kiri ➔ Kanan)</option>
        </select>
      </div>

      <div>
        <label for="marqSpeed">Kecepatan Gerakan:</label>
        <select id="marqSpeed" style="width:100%">
          <option value="1">1 - Lambat</option>
          <option value="2" selected>2 - Sedang (Rekomendasi)</option>
          <option value="3">3 - Cepat</option>
        </select>
      </div>

      <div>
        <label for="marqFontScale">Ukuran Font:</label>
        <select id="marqFontScale" style="width:100%">
          <option value="2" selected>Besar (10x14 Pixel - Tinggi Penuh, Sangat Jelas)</option>
          <option value="1">Normal (5x7 Pixel - Muat Banyak Huruf)</option>
        </select>
      </div>

      <div>
        <label for="marqHold">Jeda Tampilan Salat (Detik):</label>
        <input type="number" id="marqHold" min="3" max="60" value="10" style="width:100%">
      </div>

      <div>
        <label for="colorMarq">Warna Teks Marquee:</label>
        <div class="row" style="gap:8px">
          <input type="color" id="colorMarq" value="#ffd100" style="height:38px;width:44px;padding:2px;cursor:pointer">
          <input type="text" id="hexMarq" value="#FFD100" style="width:90px;font-family:monospace;text-transform:uppercase" maxlength="7">
        </div>
      </div>
    </div>

    <div class="row" style="margin-top:16px;gap:8px">
      <button class="primary" type="submit" id="saveMarqueeBtn">Simpan Pesan Marquee</button>
    </div>
  </form>
</section>

<!-- Mosque Flow Section: Adzan, Iqomah & Shalat -->
<section class="card wide" style="border-color:rgba(46,231,144,0.3)">
  <div class="row" style="justify-content:space-between;margin-bottom:12px">
    <h2 style="margin:0;color:var(--gold)">🕌 Alur Layar Jam Masjid (Waktu Adzan, Iqomah & Shalat)</h2>
    <span id="flowStateBadge" class="badge badge-gold">Status: Standby Normal</span>
  </div>
  <p class="hint">Alur otomatis saat waktu salat masuk: Layar menampilkan <strong>Waktu Adzan</strong> (latar siluet muadzin / gambar kustom) &rarr; berganti ke <strong>Hitung Mundur Iqomah</strong> (angka raksasa & bar progres) &rarr; saat 00:00 berganti ke <strong>Saatnya Shalat</strong> (peringatan lurus rapatkan shaf & matikan HP) &rarr; kembali ke jam normal.</p>

  <form id="mosqueFlowForm">
    <div class="grid" style="grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px;margin-bottom:14px">
      <div class="card" style="background:rgba(0,0,0,0.25);border-color:var(--line)">
        <label for="flowAdzanDuration" style="color:var(--cyan);font-weight:bold">⏳ Lama Tampilan Adzan:</label>
        <div class="row" style="gap:8px;align-items:center">
          <input type="number" id="flowAdzanDuration" min="1" max="30" value="3" style="width:90px">
          <span style="font-size:13px;color:var(--muted)">Menit</span>
        </div>
        <p class="hint" style="margin-top:6px">Durasi layar adzan sebelum hitung mundur iqomah dimulai.</p>
      </div>

      <div class="card" style="background:rgba(0,0,0,0.25);border-color:var(--line)">
        <label for="flowShalatDuration" style="color:var(--gold);font-weight:bold">🕌 Lama Tampilan Shalat:</label>
        <div class="row" style="gap:8px;align-items:center">
          <input type="number" id="flowShalatDuration" min="1" max="60" value="10" style="width:90px">
          <span style="font-size:13px;color:var(--muted)">Menit</span>
        </div>
        <p class="hint" style="margin-top:6px">Durasi layar salat sebelum kembali ke jam normal.</p>
      </div>

      <div class="card" style="background:rgba(0,0,0,0.25);border-color:var(--line)">
        <label style="color:#fff;font-weight:bold;margin-bottom:8px;display:block">🌙 Redupkan Layar Saat Shalat:</label>
        <label style="display:flex;align-items:center;gap:8px;cursor:pointer;font-size:13px">
          <input type="checkbox" id="flowDimShalat" style="width:18px;height:18px" checked>
          Matikan layar 30 detik setelah shalat dimulai agar tidak menyilaukan jama'ah
        </label>
      </div>
    </div>

    <!-- Durasi Iqomah per Shalat -->
    <div class="card" style="background:rgba(0,0,0,0.2);margin-bottom:14px;border-color:var(--line)">
      <strong style="color:var(--cyan);display:block;margin-bottom:8px">⏱️ Durasi Hitung Mundur Iqomah per Waktu Salat (Menit):</strong>
      <div class="grid" style="grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:10px">
        <div>
          <label for="flowIqomah_subuh" style="font-size:12px">Subuh</label>
          <input type="number" id="flowIqomah_subuh" min="1" max="60" value="10">
        </div>
        <div>
          <label for="flowIqomah_zuhur" style="font-size:12px">Dzuhur</label>
          <input type="number" id="flowIqomah_zuhur" min="1" max="60" value="8">
        </div>
        <div>
          <label for="flowIqomah_asar" style="font-size:12px">Ashar</label>
          <input type="number" id="flowIqomah_asar" min="1" max="60" value="7">
        </div>
        <div>
          <label for="flowIqomah_magrib" style="font-size:12px">Maghrib</label>
          <input type="number" id="flowIqomah_magrib" min="1" max="60" value="5">
        </div>
        <div>
          <label for="flowIqomah_isya" style="font-size:12px">Isya</label>
          <input type="number" id="flowIqomah_isya" min="1" max="60" value="7">
        </div>
      </div>
    </div>

    <!-- Custom Background Image Paths -->
    <div class="card" style="background:rgba(0,0,0,0.2);margin-bottom:14px;border-color:var(--line)">
      <strong style="color:var(--gold);display:block;margin-bottom:4px">🖼️ Gambar Background Kustom (Opsional):</strong>
      <p class="hint" style="margin-top:0">Jika diisi (misal <code>/media/adzan.jpg</code> atau <code>/sd/adzan.jpg</code>), gambar akan ditampilkan sebagai latar belakang teks. Jika kosong/tidak ditemukan, sistem otomatis menampilkan siluet vektor bawaan yang tajam & artistik.</p>
      <div class="grid" style="grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:12px">
        <div>
          <label for="flowAdzanImg" style="font-size:12px">Background Layar Adzan:</label>
          <input type="text" id="flowAdzanImg" placeholder="/media/adzan.jpg" value="/media/adzan.jpg">
        </div>
        <div>
          <label for="flowIqomahImg" style="font-size:12px">Background Layar Iqomah:</label>
          <input type="text" id="flowIqomahImg" placeholder="/media/iqomah.jpg" value="/media/iqomah.jpg">
        </div>
        <div>
          <label for="flowShalatImg" style="font-size:12px">Background Layar Shalat:</label>
          <input type="text" id="flowShalatImg" placeholder="/media/shalat.jpg" value="/media/shalat.jpg">
        </div>
      </div>
    </div>

    <div class="row" style="margin-top:8px">
      <button class="gold" type="button" id="saveFlowConfigBtn">Simpan Pengaturan Alur</button>
    </div>
  </form>

  <!-- Live Testing / Simulation Box -->
  <div class="card" style="background:rgba(255,209,102,0.04);margin-top:16px;border-color:rgba(255,209,102,0.3)">
    <div class="row" style="justify-content:space-between;align-items:center;margin-bottom:8px">
      <strong style="color:var(--gold)">🧪 Uji Simulasi Layar di Panel LED:</strong>
      <span class="badge badge-gold">Pratinjau Langsung</span>
    </div>
    <p class="hint" style="margin-top:0">Jalankan tampilan langsung pada panel LED selama 60 detik tanpa harus menunggu waktu shalat asli:</p>
    <div class="row" style="gap:10px;align-items:center;flex-wrap:wrap">
      <div>
        <label for="testPrayerSelect" style="font-size:12px;margin:0">Pilihan Salat:</label>
        <select id="testPrayerSelect" style="width:110px">
          <option value="zuhur" selected>Dzuhur</option>
          <option value="subuh">Subuh</option>
          <option value="asar">Ashar</option>
          <option value="magrib">Maghrib</option>
          <option value="isya">Isya</option>
        </select>
      </div>
      <div>
        <label for="testDurationSecs" style="font-size:12px;margin:0">Durasi Tes (detik):</label>
        <input type="number" id="testDurationSecs" value="60" min="5" max="3600" style="width:90px">
      </div>
      <div class="row" style="gap:8px;margin-top:16px">
        <button type="button" class="gold" onclick="triggerFlowTest('adzan')">📢 Uji Layar Adzan</button>
        <button type="button" class="primary" onclick="triggerFlowTest('iqomah')">⏳ Uji Layar Iqomah</button>
        <button type="button" style="background:#2fe790;color:#000;font-weight:600" onclick="triggerFlowTest('shalat')">🕌 Uji Layar Shalat</button>
        <button type="button" class="danger" onclick="triggerFlowTest('normal')">⏹ Stop / Normal</button>
      </div>
    </div>
  </div>
</section>

<!-- Audio Reminder Section -->
<section class="card wide" style="border-color:rgba(76,201,240,0.25)">
  <div class="row" style="justify-content:space-between;margin-bottom:12px">
    <h2 style="margin:0;color:var(--cyan)">🔊 Pengingat Audio Salat (microSD / Flash I2S)</h2>
    <span id="audioStatusBadge" class="badge badge-cyan">Status: Standby</span>
  </div>
  <p class="hint">ESP32-S3 memutar audio WAV PCM tanpa memblokir refresh panel LED HUB75. Audio pengingat berbunyi N-menit sebelum masuk waktu salat secara otomatis.</p>

  <div class="card" style="background:rgba(0,0,0,0.2);margin-bottom:16px;border-color:var(--line)">
    <div class="row" style="justify-content:space-between;align-items:center">
      <div style="flex:1;min-width:260px">
        <label for="globalAudioVol" style="margin-top:0">Volume Audio Master: <strong id="audioVolVal" style="color:var(--cyan)">80%</strong></label>
        <input type="range" id="globalAudioVol" min="0" max="100" value="80">
      </div>
      <div class="row" style="gap:8px;margin-top:12px">
        <select id="quickAudioTestFile" style="max-width:240px"><option value="">-- Pilih File Audio Uji --</option></select>
        <button type="button" class="primary" id="btnTestAudioPlay">▶ Putar Tes</button>
        <button type="button" class="danger" id="btnTestAudioStop">⏹ Hentikan</button>
      </div>
    </div>
  </div>

  <div class="grid" style="grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:14px" id="audioScheduleCards">
    <!-- Subuh Card -->
    <div class="audio-card" id="card_audio_subuh">
      <div class="row" style="justify-content:space-between">
        <strong>Subuh</strong>
        <label><input type="checkbox" id="audio_en_subuh"> Aktif</label>
      </div>
      <label>File Audio</label>
      <select id="audio_file_subuh" style="width:100%"><option value="">(Belum dipilih / Default)</option></select>
      <div class="row" style="margin-top:8px">
        <div style="flex:1"><label style="margin:0">Menit Sebelum</label><input type="number" id="audio_lead_subuh" min="1" max="60" value="10"></div>
        <div style="flex:1"><label style="margin:0">Volume (%)</label><input type="number" id="audio_vol_subuh" min="1" max="100" value="80"></div>
      </div>
      <div class="row" style="margin-top:10px;justify-content:flex-end;gap:6px">
        <button type="button" onclick="testPrayerAudio('subuh')">Tes</button>
        <button type="button" class="primary" onclick="savePrayerAudioItem('subuh')">Simpan</button>
      </div>
    </div>

    <!-- Zuhur Card -->
    <div class="audio-card" id="card_audio_zuhur">
      <div class="row" style="justify-content:space-between">
        <strong>Zuhur</strong>
        <label><input type="checkbox" id="audio_en_zuhur"> Aktif</label>
      </div>
      <label>File Audio</label>
      <select id="audio_file_zuhur" style="width:100%"><option value="">(Belum dipilih / Default)</option></select>
      <div class="row" style="margin-top:8px">
        <div style="flex:1"><label style="margin:0">Menit Sebelum</label><input type="number" id="audio_lead_zuhur" min="1" max="60" value="10"></div>
        <div style="flex:1"><label style="margin:0">Volume (%)</label><input type="number" id="audio_vol_zuhur" min="1" max="100" value="80"></div>
      </div>
      <div class="row" style="margin-top:10px;justify-content:flex-end;gap:6px">
        <button type="button" onclick="testPrayerAudio('zuhur')">Tes</button>
        <button type="button" class="primary" onclick="savePrayerAudioItem('zuhur')">Simpan</button>
      </div>
    </div>

    <!-- Asar Card -->
    <div class="audio-card" id="card_audio_asar">
      <div class="row" style="justify-content:space-between">
        <strong>Asar</strong>
        <label><input type="checkbox" id="audio_en_asar"> Aktif</label>
      </div>
      <label>File Audio</label>
      <select id="audio_file_asar" style="width:100%"><option value="">(Belum dipilih / Default)</option></select>
      <div class="row" style="margin-top:8px">
        <div style="flex:1"><label style="margin:0">Menit Sebelum</label><input type="number" id="audio_lead_asar" min="1" max="60" value="10"></div>
        <div style="flex:1"><label style="margin:0">Volume (%)</label><input type="number" id="audio_vol_asar" min="1" max="100" value="80"></div>
      </div>
      <div class="row" style="margin-top:10px;justify-content:flex-end;gap:6px">
        <button type="button" onclick="testPrayerAudio('asar')">Tes</button>
        <button type="button" class="primary" onclick="savePrayerAudioItem('asar')">Simpan</button>
      </div>
    </div>

    <!-- Magrib Card -->
    <div class="audio-card" id="card_audio_magrib">
      <div class="row" style="justify-content:space-between">
        <strong>Magrib</strong>
        <label><input type="checkbox" id="audio_en_magrib"> Aktif</label>
      </div>
      <label>File Audio</label>
      <select id="audio_file_magrib" style="width:100%"><option value="">(Belum dipilih / Default)</option></select>
      <div class="row" style="margin-top:8px">
        <div style="flex:1"><label style="margin:0">Menit Sebelum</label><input type="number" id="audio_lead_magrib" min="1" max="60" value="10"></div>
        <div style="flex:1"><label style="margin:0">Volume (%)</label><input type="number" id="audio_vol_magrib" min="1" max="100" value="80"></div>
      </div>
      <div class="row" style="margin-top:10px;justify-content:flex-end;gap:6px">
        <button type="button" onclick="testPrayerAudio('magrib')">Tes</button>
        <button type="button" class="primary" onclick="savePrayerAudioItem('magrib')">Simpan</button>
      </div>
    </div>

    <!-- Isya Card -->
    <div class="audio-card" id="card_audio_isya">
      <div class="row" style="justify-content:space-between">
        <strong>Isya</strong>
        <label><input type="checkbox" id="audio_en_isya"> Aktif</label>
      </div>
      <label>File Audio</label>
      <select id="audio_file_isya" style="width:100%"><option value="">(Belum dipilih / Default)</option></select>
      <div class="row" style="margin-top:8px">
        <div style="flex:1"><label style="margin:0">Menit Sebelum</label><input type="number" id="audio_lead_isya" min="1" max="60" value="10"></div>
        <div style="flex:1"><label style="margin:0">Volume (%)</label><input type="number" id="audio_vol_isya" min="1" max="100" value="80"></div>
      </div>
      <div class="row" style="margin-top:10px;justify-content:flex-end;gap:6px">
        <button type="button" onclick="testPrayerAudio('isya')">Tes</button>
        <button type="button" class="primary" onclick="savePrayerAudioItem('isya')">Simpan</button>
      </div>
    </div>
  </div>
</section>

<!-- Video Stream & YouTube -->
<section class="card"><h2>Video stream (HTTP / HTTPS)</h2><form id="streamForm"><label for="streamUrl">URL Video Stream (HTTP / HTTPS MJPEG atau Cloud Stream)</label><input id="streamUrl" placeholder="Contoh: http://stream.online/live.mjpg atau https://relay.cloud/stream"><div id="ytAlert" class="hint" style="border-left:3px solid var(--warning);padding-left:10px;margin-top:8px" hidden><strong style="color:var(--warning)">Petunjuk Video & YouTube:</strong> Mikrokontroler ESP32-S3 menerima format frame <strong>MJPEG</strong> (128x64).<br><br><strong>Pilihan tanpa PC:</strong> Masukkan URL stream MJPEG online/cloud publik.<br><strong>Pilihan lokal:</strong> Jalankan bridge <code>py tools/youtube_stream_bridge.py --url &quot;...&quot;</code></div><div class="row" style="margin-top:10px;align-items:flex-end"><div><label for="streamFps" style="margin-top:0">Target FPS</label><select id="streamFps"><option value="10">10 FPS</option><option value="15" selected>15 FPS (Optimal)</option><option value="20">20 FPS</option><option value="30">30 FPS</option></select></div><button class="primary" type="button" id="startStream">Mulai stream</button><button type="button" id="stopStream">Hentikan</button><button type="button" id="saveStream">Simpan URL</button></div><div class="row" style="margin-top:12px;justify-content:space-between"><span id="streamState" class="badge">Stream: Menunggu</span><span id="streamStats" class="hint">0 frame diterima</span></div></form></section>

<!-- Preview & Control -->
<section class="card"><h2>Preview & kontrol</h2><div class="preview"><canvas id="preview" width="64" height="32" aria-label="Preview gambar canvas ESP32"></canvas></div><div class="row"><button class="primary" data-command="pattern">Pola berikutnya</button><button data-command="bmp">BMP contoh</button><button data-command="jpeg">JPEG contoh</button></div><label for="layout">Layout pengujian</label><div class="row"><select id="layout"><option value="0">Satu panel 128 x 64</option></select><button id="applyLayout">Terapkan</button></div><p class="hint">Panel 128 x 64. Atur mapping pada Pengaturan panel dan sesuaikan cahaya pada Pengaturan kecerahan.</p></section>

<!-- Pengaturan Kecerahan -->
<section class="card"><h2>Pengaturan kecerahan</h2><div><div class="brightness-head"><label for="brightnessRange" style="margin:0;font-weight:600">Tingkat kecerahan panel</label><div class="brightness-badge"><strong id="brightnessVal" style="font-size:22px;color:var(--accent)">1</strong><span style="font-size:12px;color:var(--muted)">/ 255</span><span id="brightnessPct" class="badge" style="padding:2px 7px;font-size:11px;margin-left:4px">1%</span></div></div><input id="brightnessRange" type="range" min="1" max="255" value="1" step="1" aria-label="Slider tingkat kecerahan LED"><label style="margin:12px 0 6px">Pilihan cepat:</label><div class="row" style="gap:6px"><button type="button" class="btn-preset" data-b="1">1 (Min)</button><button type="button" class="btn-preset" data-b="15">15 (6%)</button><button type="button" class="btn-preset" data-b="50">50 (20%)</button><button type="button" class="btn-preset" data-b="128">128 (50%)</button><button type="button" class="btn-preset" data-b="255">255 (Maks)</button></div><div class="row" style="margin-top:16px"><button class="primary" type="button" id="saveBrightness">Simpan kecerahan</button><button type="button" id="toggleBlank">Matikan panel</button></div><p class="hint" style="margin-top:12px">Geser slider untuk melihat efek langsung pada panel LED. Tekan <strong>Simpan kecerahan</strong> agar tetap tersimpan setelah restart controller.</p><p class="hint" style="border-left:3px solid var(--warning);padding-left:10px;margin-top:8px"><strong style="color:var(--warning)">Perhatian daya:</strong> Nilai &gt; 50 menarik arus lebih tinggi. Pastikan adaptor 5V berkapasitas memadai (&ge; 2A–4A) agar ESP32 tidak restart (brownout).</p></div></section>

<!-- Pengaturan Panel -->
<section class="card"><h2>Pengaturan panel</h2><form id="panelForm"><label for="panelScan">Scan panel</label><select id="panelScan"><option value="32">1/32 (ABCDE, E = GPIO12)</option><option value="16">1/16 (ABCD)</option><option value="8">1/8 (ABC, eksperimen)</option><option value="4">1/4 (AB, eksperimen)</option></select><label for="panelMapping">Mapping pixel untuk scan 1/16</label><select id="panelMapping"><option value="0">Blok 16 pixel</option><option value="1">Blok 32 pixel</option><option value="2">Blok 128 pixel</option><option value="3">Bank 16 baris linear</option></select><p class="hint">Panel ini sudah lulus uji pada scan 1/32. Scan 1/16 memerlukan mapping sesuai tipe panel. Simpan scan menyimpan pilihan permanen lalu me-restart controller. Preview me-restart controller selama 30 detik untuk pengujian.</p><div class="row"><button class="primary" type="button" id="savePanel">Simpan scan</button><button type="submit">Uji scan (preview 30 detik)</button></div><p id="panelPreviewState" role="status"></p><div id="panelPreviewActions" hidden><button type="button" id="confirmPanel">Hasil benar, simpan</button><button type="button" id="cancelPanel">Batalkan & pulihkan</button></div></form></section>

<!-- Pengaturan Wi-Fi -->
<section class="card"><h2>Pengaturan Wi-Fi</h2><p class="hint">Hubungkan controller ke Wi-Fi router (mode STA) agar NTP dapat menyinkronkan jam, mengakses video stream, atau mengontrol via jaringan rumah.</p><div style="margin-bottom:14px"><div class="row" style="justify-content:space-between;margin-bottom:8px"><label style="margin:0;font-weight:600">Jaringan Wi-Fi Sekitar</label><button type="button" id="scanWifiBtn" style="padding:6px 12px;font-size:12px">🔍 Pindai Wi-Fi</button></div><div id="wifiList" style="max-height:190px;overflow-y:auto;border:1px solid var(--line);border-radius:10px;padding:6px;background:rgba(0,0,0,0.18)"><p class="hint" style="text-align:center;margin:12px 0" id="wifiScanNotice">Tekan "Pindai Wi-Fi" untuk mencari jaringan 2,4 GHz di sekitar.</p></div></div><form id="wifiForm"><label for="ssid">SSID Wi-Fi tujuan (2,4 GHz)</label><input id="ssid" name="ssid" maxlength="32" autocomplete="off" placeholder="Nama Wi-Fi; kosongkan untuk mode AP"><label for="password">Password Wi-Fi tujuan</label><input id="password" name="password" type="password" maxlength="64" autocomplete="new-password" placeholder="Isi ulang password saat menyimpan"><p class="hint">Password kosong digunakan untuk jaringan tanpa password. Pengaturan disimpan di perangkat.</p><button class="primary" type="submit">Simpan & hubungkan</button></form><p class="hint" id="apInfo">Hotspot: —</p><p class="hint">Hotspot tetap tersedia saat STA terhubung. Anda juga dapat membuka alamat STA langsung dari router Anda.</p></section>

<!-- Teks & Animasi -->
<section class="card wide"><h2>Teks & animasi</h2><form id="animationForm"><label for="textContent">Teks (opsional, maksimal 192 byte UTF-8)</label><input id="textContent" maxlength="192" placeholder="Contoh: SELAMAT DATANG"><p class="hint">Font 5x7: huruf kecil ditampilkan sebagai kapital. Teks panjang disarankan menggunakan animasi scroll.</p><div class="row"><div><label for="textEffect">Animasi teks</label><select id="textEffect"></select></div><div><label for="imageEffect">Animasi gambar</label><select id="imageEffect"></select></div><div><label for="textScale">Ukuran teks</label><select id="textScale"><option value="1">1x</option><option value="2">2x</option><option value="3">3x</option><option value="4">4x</option></select></div><div><label for="textColor">Warna teks</label><input id="textColor" type="color" value="#ffffff"></div><div><label for="backgroundColor">Warna latar</label><input id="backgroundColor" type="color" value="#000000"></div></div><div class="row"><div><label for="durationMs">Durasi fade / slide / blink (ms)</label><input id="durationMs" type="number" min="200" max="60000" step="100" value="3000" required></div><div><label for="scrollSpeed">Kecepatan scroll (piksel/detik)</label><input id="scrollSpeed" type="number" min="1" max="200" value="30" required></div></div><div class="row"><label><input id="animationLoop" type="checkbox" checked> Ulangi animasi</label><label><input id="imageVisible" type="checkbox" checked> Tampilkan gambar di belakang teks</label></div><div class="row"><button class="primary" type="submit">Terapkan & simpan</button><button type="button" data-motion="pause">Pause</button><button type="button" data-motion="resume">Lanjutkan</button><button type="button" data-motion="restart">Mulai ulang</button><span id="animationState" class="hint"></span></div></form><p class="hint">Pilih gambar lewat Preview pada daftar media. Animasi gambar dan teks dapat dikombinasikan. Pengaturan disimpan permanen.</p></section>

<!-- Manajer Penyimpanan & Media (microSD & LittleFS) -->
<section class="card wide">
  <div class="row" style="justify-content:space-between;align-items:center;margin-bottom:8px">
    <h2>Manajer Penyimpanan (microSD & Internal Flash)</h2>
    <div class="row" style="gap:8px;align-items:center">
      <button type="button" id="remountSdBtn" style="padding:4px 10px;font-size:12px">🔄 Deteksi Ulang microSD</button>
      <span id="sdCardBadge" class="badge badge-cyan">microSD: Memeriksa…</span>
      <span id="flashCardBadge" class="badge">Flash: Memeriksa…</span>
    </div>
  </div>

  <div class="grid" style="grid-template-columns:1fr 1fr;gap:14px;margin-bottom:16px">
    <div class="card" style="padding:14px;background:rgba(0,0,0,0.18)">
      <div class="row" style="justify-content:space-between"><small>microSD Card</small><small id="sdUsageText">—</small></div>
      <div class="progress-bar-bg"><div id="sdUsageBar" class="progress-bar-fill sd" style="width:0%"></div></div>
    </div>
    <div class="card" style="padding:14px;background:rgba(0,0,0,0.18)">
      <div class="row" style="justify-content:space-between"><small>Internal Flash (LittleFS)</small><small id="flashUsageText">—</small></div>
      <div class="progress-bar-bg"><div id="flashUsageBar" class="progress-bar-fill" style="width:0%"></div></div>
    </div>
  </div>

  <form id="uploadForm">
    <div class="row" style="align-items:flex-end">
      <div style="flex:1;min-width:240px">
        <label for="uploadFile">Pilih file untuk diunggah (Gambar BMP/JPG, Video MJPEG, Audio WAV/MP3)</label>
        <input id="uploadFile" type="file" accept=".bmp,.jpg,.jpeg,.mjpeg,.mjpg,.wav,.mp3" required>
      </div>
      <div style="min-width:180px">
        <label for="uploadDest">Tujuan Penyimpanan</label>
        <select id="uploadDest"><option value="sd" selected>microSD Card (/sd/media/)</option><option value="flash">Internal Flash (/media/)</option></select>
      </div>
      <button class="primary" type="submit" id="uploadSubmitBtn">Upload Media</button>
      <button type="button" id="refreshMedia">Perbarui Daftar</button>
    </div>
    <div class="row" style="margin-top:8px"><span id="uploadProgress" role="status" class="hint"></span></div>
  </form>

  <div style="margin-top:18px">
    <div class="tab-filter" id="mediaFilterTabs">
      <button type="button" class="tab-btn active" data-filter="">Semua Media</button>
      <button type="button" class="tab-btn" data-filter="audio">🎵 Audio (.wav)</button>
      <button type="button" class="tab-btn" data-filter="video">🎬 Video (.mjpeg)</button>
      <button type="button" class="tab-btn" data-filter="image">🖼️ Gambar (.bmp/.jpg)</button>
    </div>
    <div id="files"></div>
  </div>
</section></div>

<footer>ESP32-S3 N16R8 · HUB75 Matrix Controller & Mosque Clock · Web UI Lokal</footer></main>

<script>
const $=id=>document.getElementById(id);
let loaded=false,lastFrame=-1,busy=false,polling=false,animationLoaded=false,nextPoll=0,liveAnimation=false,brightnessDragging=false,brightnessTimer=0,activeMode=0;
let currentFilter="";

function notice(message,error=false){
  const n=$('notice');
  if(!n)return;
  n.hidden=false;
  n.textContent=message;
  n.classList.toggle('error',error);
}

async function api(path,body,method='POST'){
  const options=body?{method,headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body)}:{cache:'no-store',signal:AbortSignal.timeout(12000)};
  const response=await fetch(path,options);
  const data=await response.json();
  if(!response.ok)throw Error(data.error||'Permintaan gagal');
  return data;
}

async function frame(){
  const r=await fetch('/api/frame',{cache:'no-store',signal:AbortSignal.timeout(12000)});
  if(!r.ok)throw Error('Preview belum tersedia');
  const b=new Uint8Array(await r.arrayBuffer());
  const w=b[0]|b[1]<<8,h=b[2]|b[3]<<8;
  if(!w||!h||b.length!==4+w*h*2)throw Error('Data preview tidak lengkap');
  const c=$('preview');c.width=w;c.height=h;
  const ctx=c.getContext('2d'),img=ctx.createImageData(w,h);
  for(let i=0;i<w*h;i++){
    const v=b[4+i*2]|b[5+i*2]<<8;
    img.data[i*4]=Math.round(((v>>11)&31)*255/31);
    img.data[i*4+1]=Math.round(((v>>5)&63)*255/63);
    img.data[i*4+2]=Math.round((v&31)*255/31);
    img.data[i*4+3]=255;
  }
  ctx.putImageData(img,0,0);
}

const modeLabels=['Otomatis','Video Stream','Animasi Teks','Gambar Flash','Jam Masjid'];
const modeDescs=[
  'Mode Otomatis: Prioritas Video Stream saat aktif & terhubung. Jika offline, otomatis fallback ke Jam Masjid & Jadwal Salat atau Animasi Teks.',
  'Hanya Video Stream: Panel LED khusus menampilkan frame dari URL video stream (MJPEG / YouTube relay).',
  'Hanya Animasi Teks: Panel LED mengabaikan stream dan menampilkan teks berjalan / animasi.',
  'Hanya Gambar Flash: Panel LED menampilkan gambar BMP/JPEG statis dari penyimpanan LittleFS atau microSD.',
  'Mode Jam Masjid & Waktu Salat: Tampilan jam digital presisi, kalender Masehi/Hijriah, hitung mundur dan jadwal 5 waktu salat dengan penanda salat berikutnya.'
];

function updateModeUI(m){
  activeMode=m;
  document.querySelectorAll('.btn-mode').forEach(b=>b.classList.toggle('active',Number(b.dataset.mode)===m));
  if($('metricDisplayMode'))$('metricDisplayMode').textContent=modeLabels[m]||'Otomatis';
  if($('modeDescription'))$('modeDescription').textContent=modeDescs[m]||'';
}

function formatCountdown(sec){
  if(sec<0)sec=0;
  const h=Math.floor(sec/3600),m=Math.floor((sec%3600)/60),s=sec%60;
  if(h>0)return String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');
  return String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');
}

async function refresh(){
  const s=await api('/api/status');
  if($('connection'))$('connection').textContent='Perangkat terhubung';
  const pct=Math.round((s.brightness||1)/255*100);
  if($('outputNote'))$('outputNote').textContent=s.output_ready?(s.output_blanked?'Panel dimatikan sementara (blank).':'Output HUB75 aktif, scan 1/'+s.scan+', kecerahan '+s.brightness+'/255 ('+pct+'%).'):'Output HUB75 gagal/belum aktif; periksa log serial.';
  if($('metricBrightness'))$('metricBrightness').textContent=s.output_blanked?'Mati (0)':s.brightness+' / 255 ('+pct+'%)';
  if($('toggleBlank'))$('toggleBlank').textContent=s.output_blanked?'Nyalakan panel':'Matikan panel';
  if(!brightnessDragging){
    if($('brightnessRange'))$('brightnessRange').value=s.brightness;
    if($('brightnessVal'))$('brightnessVal').textContent=s.brightness;
    if($('brightnessPct'))$('brightnessPct').textContent=pct+'%';
  }
  updateModeUI(s.display_mode!==undefined?s.display_mode:0);
  if($('streamState'))$('streamState').textContent='Stream: '+(s.stream_status||'Siap');
  if($('streamStats'))$('streamStats').textContent=(s.stream_frames||0)+' frame diterima';
  if($('metricStream'))$('metricStream').textContent=s.stream_running?(s.stream_status==='Streaming aktif'?'Aktif':s.stream_status):'Nonaktif';
  if(!loaded){
    if($('ssid'))$('ssid').value=s.target_ssid;
    if($('streamUrl'))$('streamUrl').value=s.stream_url||'';
    if(s.stream_fps&&$('streamFps'))$('streamFps').value=String(s.stream_fps);
    loaded=true;
  }
  if($('streamUrl'))checkYtUrl($('streamUrl').value);
  liveAnimation=(s.display_mode===2||s.display_mode===0)&&s.animation_moving&&!s.animation_paused;
  if($('animationState'))$('animationState').textContent=s.animation_active?(s.animation_paused?'Dijeda':s.animation_moving?'Berjalan':'Statis / selesai'):'Belum diterapkan';
  if($('memory'))$('memory').textContent=s.memory_ok?'Lulus tes':'Periksa memori';
  if($('storage'))$('storage').textContent=s.storage_ok?'Siap':'Belum tersedia';
  if($('uptime'))$('uptime').textContent=Math.floor(s.uptime_ms/60000)+' menit';
  if($('sta'))$('sta').textContent=s.sta_connected?'Terhubung':s.sta_connecting?'Menghubungkan…':'Tidak terhubung';
  if($('ip'))$('ip').textContent=s.sta_connected?s.sta_ip:s.ap_ip;
  if($('size'))$('size').textContent=s.width+' × '+s.height;
  if($('apInfo'))$('apInfo').textContent='Hotspot: '+s.ap_ssid+' · '+(s.ap_ready?s.ap_ip:'Nonaktif');
  if($('layout')&&document.activeElement!==$('layout'))$('layout').value=s.layout;

  // SD Metric
  if($('metricSd'))$('metricSd').textContent=s.sd_mounted?(s.sd_type||'SD Ready'):'Tidak Terpasang';
  // Clock Metric
  if($('metricClock'))$('metricClock').textContent=s.clock_time?(s.clock_time+' '+(s.clock_synced?'(NTP)':'(Manual)')):'—';
  // Audio Metric
  if($('metricAudio'))$('metricAudio').textContent=s.audio_playing?'Memutar Audio':('Siap ('+(s.audio_volume||80)+'%)');

  // Hero Mosque Clock Update
  if(s.clock_time&&$('heroClock'))$('heroClock').innerHTML=s.clock_time+' <span style="font-size:18px;color:var(--muted)">WIB</span>';
  if(s.clock_date&&$('heroDate'))$('heroDate').textContent=s.clock_date;
  if(s.clock_hijri&&$('heroHijri'))$('heroHijri').textContent=s.clock_hijri;
  if(s.next_prayer&&$('heroNextPrayer')){
    if(s.mosque_state_code>0){
      $('heroNextPrayer').innerHTML='<span style="color:var(--gold);font-weight:bold">'+s.mosque_state+' '+s.active_prayer+'</span>';
      if($('heroCountdownBadge')){
        $('heroCountdownBadge').textContent='SISA '+formatCountdown(s.state_remaining_seconds||0);
        $('heroCountdownBadge').className='badge badge-gold';
      }
    }else{
      $('heroNextPrayer').textContent=s.next_prayer+' ('+s.next_prayer_time+')';
      if($('heroCountdownBadge')){
        $('heroCountdownBadge').textContent='T - '+formatCountdown(s.countdown_seconds||0);
        $('heroCountdownBadge').className='badge badge-gold';
      }
    }
  }

  // Audio Hero Badge
  if($('heroAudioBadge')){
    $('heroAudioBadge').style.display=s.audio_playing?'inline-flex':'none';
    if(s.audio_playing)$('heroAudioBadge').textContent='🎵 '+s.audio_file;
  }

  if($('preview')&&(s.generation!==lastFrame||$('preview').width!==s.width||$('preview').height!==s.height)){await frame();lastFrame=s.generation}
}

async function loadPrayerTimes(){
  try{
    const d=await api('/api/v1/prayer-times',null,'GET');
    if($('calcSource'))$('calcSource').value=d.source||'auto';
    if(d.latitude&&$('latitude'))$('latitude').value=d.latitude;
    if(d.longitude&&$('longitude'))$('longitude').value=d.longitude;
    if(d.timezone&&$('timezoneName'))$('timezoneName').value=d.timezone;

    if(d.imsak&&d.syuruq&&$('imsakSyuruqNote')){
      $('imsakSyuruqNote').textContent='Imsak: '+d.imsak+' · Syuruq (Terbit): '+d.syuruq;
    }

    if(d.schedule){
      ['subuh','zuhur','asar','magrib','isya'].forEach(pid=>{
        const p=d.schedule[pid];
        if(!p)return;
        if($('base_'+pid))$('base_'+pid).textContent=p.base;
        if($('corr_'+pid))$('corr_'+pid).value=p.correction;
        if($('eff_'+pid))$('eff_'+pid).innerHTML='<strong>'+p.effective+'</strong>';
        const row=$('row_'+pid);
        const badge=$('badge_'+pid);
        const isCurrent=d.current_prayer&&d.current_prayer.name&&d.current_prayer.name.toLowerCase()===pid;
        const isNext=d.next_prayer&&d.next_prayer.name&&d.next_prayer.name.toLowerCase()===pid;
        if(row)row.classList.toggle('highlight',isCurrent);
        if(badge){
          badge.textContent=isCurrent?'Waktu Salat Saat Ini':(isNext?('Berikutnya ('+formatCountdown(d.next_prayer.countdown_seconds)+')'):'Standby');
          badge.className='badge '+(isCurrent?'badge-gold':(isNext?'badge-cyan':''));
        }
      });
    }

    if(d.colors){
      if(d.colors.name&&$('colorPrayerName')){$('colorPrayerName').value=d.colors.name;if($('hexPrayerName'))$('hexPrayerName').value=d.colors.name.toUpperCase();}
      if(d.colors.time&&$('colorPrayerTime')){$('colorPrayerTime').value=d.colors.time;if($('hexPrayerTime'))$('hexPrayerTime').value=d.colors.time.toUpperCase();}
      if(d.colors.highlight&&$('colorPrayerHighlight')){$('colorPrayerHighlight').value=d.colors.highlight;if($('hexPrayerHighlight'))$('hexPrayerHighlight').value=d.colors.highlight.toUpperCase();}
      if(d.colors.card_bg&&$('colorPrayerCardBg')){$('colorPrayerCardBg').value=d.colors.card_bg;if($('hexPrayerCardBg'))$('hexPrayerCardBg').value=d.colors.card_bg.toUpperCase();}
      if(d.colors.next_bg&&$('colorPrayerNextBg')){$('colorPrayerNextBg').value=d.colors.next_bg;if($('hexPrayerNextBg'))$('hexPrayerNextBg').value=d.colors.next_bg.toUpperCase();}
      updatePrayerColorPreview();
    }

    if(d.marquee){
      if($('marqEnabled')) $('marqEnabled').checked = d.marquee.enabled !== false;
      if($('marqText') && d.marquee.text !== undefined) $('marqText').value = d.marquee.text;
      if($('marqDirection') && d.marquee.direction) $('marqDirection').value = d.marquee.direction;
      if($('marqSpeed') && d.marquee.speed) $('marqSpeed').value = d.marquee.speed;
      if($('marqFontScale') && d.marquee.font_scale) $('marqFontScale').value = d.marquee.font_scale;
      if($('marqHold') && d.marquee.hold_seconds) $('marqHold').value = d.marquee.hold_seconds;
      if(d.marquee.color){
        if($('colorMarq')) $('colorMarq').value = d.marquee.color;
        if($('hexMarq')) $('hexMarq').value = d.marquee.color.toUpperCase();
      }
    }

    if(d.flow){
      if($('flowStateBadge')){
        const stateNames=['Standby Normal','WAKTU ADZAN','HITUNG MUNDUR IQOMAH','SAATNYA SHALAT'];
        const stateBadges=['badge-cyan','badge-gold','badge-gold','badge-gold'];
        const sName=stateNames[d.flow.state_code]||d.flow.state;
        $('flowStateBadge').textContent='Status Layar: '+sName+(d.flow.is_test_mode?' (Mode Uji)':'');
        $('flowStateBadge').className='badge '+(stateBadges[d.flow.state_code]||'badge-cyan');
      }
      if($('flowAdzanDuration')&&d.flow.adzan_duration_mins)$('flowAdzanDuration').value=d.flow.adzan_duration_mins;
      if($('flowShalatDuration')&&d.flow.shalat_duration_mins)$('flowShalatDuration').value=d.flow.shalat_duration_mins;
      if($('flowAdzanImg')&&d.flow.adzan_image)$('flowAdzanImg').value=d.flow.adzan_image;
      if($('flowIqomahImg')&&d.flow.iqomah_image)$('flowIqomahImg').value=d.flow.iqomah_image;
      if($('flowShalatImg')&&d.flow.shalat_image)$('flowShalatImg').value=d.flow.shalat_image;
      if($('flowDimShalat'))$('flowDimShalat').checked=d.flow.dim_screen_during_shalat!==false;

      if(d.flow.iqomah_duration_mins&&Array.isArray(d.flow.iqomah_duration_mins)){
        const pids=['subuh','zuhur','asar','magrib','isya'];
        pids.forEach((pid,idx)=>{
          if($('flowIqomah_'+pid)&&d.flow.iqomah_duration_mins[idx]!==undefined){
            $('flowIqomah_'+pid).value=d.flow.iqomah_duration_mins[idx];
          }
        });
      }
    }
  }catch(e){console.warn('loadPrayerTimes error',e)}
}

async function loadAudioSchedule(){
  try{
    const d=await api('/api/v1/prayer-audio',null,'GET');
    if(d.audio_status){
      if($('audioStatusBadge')){
        $('audioStatusBadge').textContent=d.audio_status.playing?('Memutar: '+d.audio_status.active_file):'Status: Standby';
        $('audioStatusBadge').className='badge '+(d.audio_status.playing?'badge-gold':'badge-cyan');
      }
      if(d.audio_status.volume!==undefined&&!brightnessDragging){
        if($('globalAudioVol'))$('globalAudioVol').value=d.audio_status.volume;
        if($('audioVolVal'))$('audioVolVal').textContent=d.audio_status.volume+'%';
      }
    }
    if(d.audio_items){
      ['subuh','zuhur','asar','magrib','isya'].forEach(pid=>{
        const item=d.audio_items[pid];
        if(!item)return;
        if($('audio_en_'+pid))$('audio_en_'+pid).checked=item.enabled;
        if($('audio_lead_'+pid))$('audio_lead_'+pid).value=item.lead_minutes;
        if($('audio_vol_'+pid))$('audio_vol_'+pid).value=item.volume;
        if($('audio_file_'+pid)&&item.file)$('audio_file_'+pid).value=item.file;
        const card=$('card_audio_'+pid);
        if(card)card.classList.toggle('active',item.enabled);
      });
    }
  }catch(e){console.warn('loadAudioSchedule error',e)}
}

async function loadStorageStatus(){
  try{
    const d=await api('/api/v1/storage/status',null,'GET');
    if($('sdCardBadge')){
      $('sdCardBadge').textContent=d.sd_mounted?('microSD '+d.sd_card_type):'microSD Belum Terpasang';
      $('sdCardBadge').className='badge '+(d.sd_mounted?'badge-cyan':'');
    }
    if($('flashCardBadge')){
      $('flashCardBadge').textContent=d.littlefs_ready?'Flash LittleFS Siap':'Flash Belum Siap';
    }
    if(d.sd_mounted&&d.sd_total_bytes>0){
      const pct=Math.round(d.sd_used_bytes/d.sd_total_bytes*100);
      const usedMb=Math.round(d.sd_used_bytes/1048576);
      const totalMb=Math.round(d.sd_total_bytes/1048576);
      if($('sdUsageText'))$('sdUsageText').textContent=usedMb+' MB / '+totalMb+' MB ('+pct+'%)';
      if($('sdUsageBar'))$('sdUsageBar').style.width=pct+'%';
    }else{
      const errMsg = d.sd_error ? (' ('+d.sd_error+')') : '';
      if($('sdUsageText'))$('sdUsageText').innerHTML='<span style="color:var(--gold);font-size:11px">Belum Terdeteksi — Format wajib FAT32</span>';
      if($('sdUsageBar'))$('sdUsageBar').style.width='0%';
    }
    if(d.littlefs_ready&&d.littlefs_total_bytes>0){
      const pct=Math.round(d.littlefs_used_bytes/d.littlefs_total_bytes*100);
      const usedKb=Math.round(d.littlefs_used_bytes/1024);
      const totalKb=Math.round(d.littlefs_total_bytes/1024);
      if($('flashUsageText'))$('flashUsageText').textContent=usedKb+' KB / '+totalKb+' KB ('+pct+'%)';
      if($('flashUsageBar'))$('flashUsageBar').style.width=pct+'%';
    }
  }catch(e){console.warn('loadStorageStatus error',e)}
}

let availableAudioFiles=[];
function updateAudioDropdowns(){
  const dropdowns=[$('quickAudioTestFile'),$('audio_file_subuh'),$('audio_file_zuhur'),$('audio_file_asar'),$('audio_file_magrib'),$('audio_file_isya')];
  dropdowns.forEach(sel=>{
    if(!sel)return;
    const cur=sel.value;
    sel.replaceChildren();
    const optDefault=document.createElement('option');
    optDefault.value='';
    optDefault.textContent=sel.id==='quickAudioTestFile'?'-- Pilih File Audio Uji --':'(Gunakan Default)';
    sel.append(optDefault);
    availableAudioFiles.forEach(f=>{
      const opt=document.createElement('option');
      opt.value=f.path;
      opt.textContent=(f.storage==='sd'?'[SD] ':'[Flash] ')+f.name;
      sel.append(opt);
    });
    if(cur)sel.value=cur;
  });
}

async function media(){
  try{
    const s=await api('/api/v1/media'+(currentFilter?'?type='+currentFilter:''),null,'GET');
    availableAudioFiles=s.files.filter(f=>f.is_audio||f.name.endsWith('.wav'));
    updateAudioDropdowns();

    $('files').replaceChildren();
    for(const f of s.files){
      const row=document.createElement('div');
      row.className='file';
      const label=document.createElement('span');
      let icon='📁 ';
      if(f.is_audio||f.name.endsWith('.wav'))icon='🎵 ';
      else if(f.is_video||f.name.endsWith('.mjpeg')||f.name.endsWith('.mjpg'))icon='🎬 ';
      else if(f.is_image||f.name.endsWith('.bmp')||f.name.endsWith('.jpg')||f.name.endsWith('.jpeg'))icon='🖼️ ';

      const storageTag=f.storage==='sd'?'<span class="badge badge-cyan" style="font-size:11px;padding:1px 6px">microSD</span>':'<span class="badge" style="font-size:11px;padding:1px 6px">Flash</span>';
      label.innerHTML=icon+'<strong>'+f.name+'</strong> '+storageTag+' · '+Math.round(f.size/1024)+' KB';

      const controls=document.createElement('div');
      controls.className='row';

      if(f.is_video||f.name.endsWith('.mjpeg')||f.name.endsWith('.mjpg')){
        const playBtn=document.createElement('button');
        playBtn.className='primary';
        playBtn.textContent='▶ Putar Video';
        playBtn.onclick=()=>action(()=>api('/api/control',{command:'file',path:f.path}));
        controls.append(playBtn);
      }else if(f.is_audio||f.name.endsWith('.wav')){
        const playAudioBtn=document.createElement('button');
        playAudioBtn.className='primary';
        playAudioBtn.textContent='▶ Putar Audio';
        playAudioBtn.onclick=()=>action(()=>api('/api/v1/prayer-audio/test',{file:f.path,volume:$('globalAudioVol').value}));
        controls.append(playAudioBtn);
      }else{
        const prevBtn=document.createElement('button');
        prevBtn.textContent='Preview';
        prevBtn.onclick=()=>action(()=>api('/api/control',{command:'file',path:f.path}));
        controls.append(prevBtn);
      }

      const del=document.createElement('button');
      del.textContent='Hapus';
      del.onclick=()=>{
        if(confirm('Hapus '+f.name+' dari '+(f.storage==='sd'?'microSD':'Flash')+'?')){
          action(()=>api('/api/media/delete',{path:f.path}));
        }
      };
      controls.append(del);
      row.append(label,controls);
      $('files').append(row);
    }
    if(!s.files.length){
      const emptyMsg=document.createElement('p');
      emptyMsg.className='hint';
      emptyMsg.style.textAlign='center';
      emptyMsg.style.padding='18px 0';
      emptyMsg.textContent='Belum ada file media yang sesuai filter ini.';
      $('files').append(emptyMsg);
    }
  }catch(e){console.warn('media listing error',e)}
}

async function action(fn){
  if(busy)return;
  busy=true;
  document.querySelectorAll('button').forEach(b=>b.disabled=true);
  try{
    while(polling)await new Promise(resolve=>setTimeout(resolve,40));
    const r=await fn();
    notice(r.message||'Perubahan diterapkan');
    await refresh();
    await loadPrayerTimes();
    await loadAudioSchedule();
    await loadStorageStatus();
    await media();
  }catch(e){
    notice(e.message,true);
  }finally{
    busy=false;
    document.querySelectorAll('button').forEach(b=>b.disabled=false);
  }
}

function upload(file,storageTarget){
  return new Promise((resolve,reject)=>{
    if(!file||!file.size){reject(Error('Pilih file yang valid'));return}
    if(file.size>4194304&&storageTarget==='flash'){
      reject(Error('Penyimpanan Flash terbatas maksimal 4 MiB. Gunakan microSD untuk file besar.'));
      return;
    }
    const form=new FormData();
    form.append('file',file);
    const xhr=new XMLHttpRequest();
    xhr.open('POST','/api/media/upload?storage='+encodeURIComponent(storageTarget));
    xhr.timeout=120000;
    xhr.upload.onprogress=e=>{
      if(e.lengthComputable)$('uploadProgress').textContent=Math.round(e.loaded/e.total*100)+'% terkirim ke '+storageTarget.toUpperCase()+'…';
    };
    xhr.upload.onload=()=>{$('uploadProgress').textContent='Menyimpan & memverifikasi file…'};
    xhr.onload=()=>{
      try{
        const r=JSON.parse(xhr.responseText);
        if(xhr.status<200||xhr.status>=300)throw Error(r.error||'Upload gagal');
        $('uploadProgress').textContent='Upload selesai!';
        $('uploadFile').value='';
        resolve(r);
      }catch(e){$('uploadProgress').textContent='Gagal';reject(e)}
    };
    xhr.onerror=()=>{ $('uploadProgress').textContent='Gagal';reject(Error('Koneksi upload terputus'))};
    xhr.ontimeout=()=>{ $('uploadProgress').textContent='Timeout';reject(Error('Upload melewati batas waktu'))};
    xhr.send(form);
  });
}

$('uploadForm').onsubmit=e=>{
  e.preventDefault();
  const file=$('uploadFile').files[0];
  const target=$('uploadDest').value;
  action(()=>upload(file,target));
};
$('refreshMedia').onclick=()=>action(async()=>({message:'Daftar media diperbarui'}));
if($('remountSdBtn'))$('remountSdBtn').onclick=async()=>{
  notice('Mencoba mendeteksi & menghubungkan ulang microSD...');
  try{
    const res=await api('/api/v1/storage/remount',null,'POST');
    if(res.sd_mounted){
      notice('microSD berhasil terpasang: '+(res.sd_card_type||''));
    }else{
      notice('Gagal mount microSD: '+(res.sd_error||'Format wajib FAT32!'), true);
    }
  }catch(e){
    notice('Gagal mount: Periksa format FAT32 dan perkabelan microSD', true);
  }
  await loadStorageStatus();
  await media();
};

// Tab Filter
document.querySelectorAll('#mediaFilterTabs .tab-btn').forEach(btn=>{
  btn.onclick=()=>{
    document.querySelectorAll('#mediaFilterTabs .tab-btn').forEach(b=>b.classList.remove('active'));
    btn.classList.add('active');
    currentFilter=btn.dataset.filter||"";
    media();
  };
});

// Mosque Clock & Prayer Handlers
$('savePrayerConfigBtn').onclick=()=>action(()=>api('/api/v1/prayer-times/settings',{
  source:$('calcSource').value,
  latitude:$('latitude').value,
  longitude:$('longitude').value,
  timezone:$('timezoneName').value
}));

$('saveCorrectionsBtn').onclick=()=>action(()=>api('/api/v1/prayer-times/corrections',{
  subuh:$('corr_subuh').value,
  zuhur:$('corr_zuhur').value,
  asar:$('corr_asar').value,
  magrib:$('corr_magrib').value,
  isya:$('corr_isya').value
}));

function updatePrayerColorPreview(){
  const nCol=$('hexPrayerName')?$('hexPrayerName').value:'#00FFFF';
  const tCol=$('hexPrayerTime')?$('hexPrayerTime').value:'#FFFFFF';
  const hCol=$('hexPrayerHighlight')?$('hexPrayerHighlight').value:'#FFD100';
  const bgCol=$('hexPrayerCardBg')?$('hexPrayerCardBg').value:'#000000';
  const nbgCol=$('hexPrayerNextBg')?$('hexPrayerNextBg').value:'#081824';

  if($('prevName'))$('prevName').style.color=nCol;
  if($('prevTime'))$('prevTime').style.color=tCol;
  if($('previewCardPrayer'))$('previewCardPrayer').style.background=bgCol;

  if($('prevNextName'))$('prevNextName').style.color=hCol;
  if($('prevNextTime'))$('prevNextTime').style.color=hCol;
  if($('previewCardNext')){
    $('previewCardNext').style.background=nbgCol;
    $('previewCardNext').style.borderTopColor=hCol;
    $('previewCardNext').style.borderBottomColor=hCol;
  }
}

function bindColorPair(cId, hId){
  const c=$(cId), h=$(hId);
  if(!c||!h)return;
  c.oninput=()=>{ h.value=c.value.toUpperCase(); updatePrayerColorPreview(); };
  h.oninput=()=>{
    let val=h.value.trim();
    if(!val.startsWith('#')) val='#'+val;
    if(/^#[0-9A-Fa-f]{6}$/.test(val)){
      c.value=val;
      updatePrayerColorPreview();
    }
  };
}
bindColorPair('colorPrayerName','hexPrayerName');
bindColorPair('colorPrayerTime','hexPrayerTime');
bindColorPair('colorPrayerHighlight','hexPrayerHighlight');
bindColorPair('colorPrayerCardBg','hexPrayerCardBg');
bindColorPair('colorPrayerNextBg','hexPrayerNextBg');

if($('btnSavePrayerColors'))$('btnSavePrayerColors').onclick=()=>action(async()=>{
  await api('/api/v1/prayer-times/colors',{
    name_color:$('hexPrayerName').value,
    time_color:$('hexPrayerTime').value,
    highlight_color:$('hexPrayerHighlight').value,
    card_bg_color:$('hexPrayerCardBg').value,
    next_bg_color:$('hexPrayerNextBg').value
  });
  notice('Warna tampilan panel berhasil disimpan!');
});

bindColorPair('colorMarq','hexMarq');

if($('marqueeForm')) $('marqueeForm').onsubmit=(e)=>{
  e.preventDefault();
  action(async()=>{
    await api('/api/v1/prayer-times/marquee',{
      enabled: $('marqEnabled').checked,
      text: $('marqText').value,
      color: $('hexMarq').value,
      direction: $('marqDirection').value,
      speed: parseInt($('marqSpeed').value, 10),
      font_scale: parseInt($('marqFontScale').value, 10),
      hold_seconds: parseInt($('marqHold').value, 10)
    });
    notice('Pengaturan pesan marquee berhasil disimpan!');
    await loadPrayerTimes();
  });
};
if($('saveMarqueeBtn')) $('saveMarqueeBtn').onclick=()=>{ if($('marqueeForm')) $('marqueeForm').requestSubmit(); };

async function saveMosqueFlow(){
  const body={
    adzan_duration_mins: parseInt($('flowAdzanDuration').value, 10)||3,
    shalat_duration_mins: parseInt($('flowShalatDuration').value, 10)||10,
    adzan_image: $('flowAdzanImg').value.trim(),
    iqomah_image: $('flowIqomahImg').value.trim(),
    shalat_image: $('flowShalatImg').value.trim(),
    dim_screen_during_shalat: $('flowDimShalat').checked,
    iqomah_duration_mins: [
      parseInt($('flowIqomah_subuh').value, 10)||10,
      parseInt($('flowIqomah_zuhur').value, 10)||8,
      parseInt($('flowIqomah_asar').value, 10)||7,
      parseInt($('flowIqomah_magrib').value, 10)||5,
      parseInt($('flowIqomah_isya').value, 10)||7
    ]
  };
  action(async()=>{
    const r=await api('/api/v1/prayer-times/flow', body, 'POST');
    notice(r.message||'Pengaturan alur Adzan, Iqomah & Shalat berhasil disimpan!');
    await loadPrayerTimes();
  });
}
if($('saveFlowConfigBtn')) $('saveFlowConfigBtn').onclick=saveMosqueFlow;

async function triggerFlowTest(state){
  const prayer=$('testPrayerSelect')?$('testPrayerSelect').value:'zuhur';
  const duration=parseInt($('testDurationSecs')?$('testDurationSecs').value:60, 10)||60;
  action(async()=>{
    const r=await api('/api/v1/prayer-times/test-flow', {state, prayer, duration}, 'POST');
    notice(r.message||('Uji '+state+' aktif!'));
    await loadPrayerTimes();
  });
}


async function syncBrowserTime(quiet=false){
  const now=new Date();
  const payload={
    epoch:Math.floor(now.getTime()/1000),
    year:now.getFullYear(),
    month:now.getMonth()+1,
    day:now.getDate(),
    hour:now.getHours(),
    minute:now.getMinutes(),
    second:now.getSeconds(),
    timezone_offset:-now.getTimezoneOffset(),
    timezone:$('timezoneName')?$('timezoneName').value:'WIB'
  };
  try{
    const r=await api('/api/v1/clock/sync',payload);
    const timeStr=String(now.getHours()).padStart(2,'0')+':'+String(now.getMinutes()).padStart(2,'0')+':'+String(now.getSeconds()).padStart(2,'0');
    if(!quiet)notice('Jam berhasil disinkronkan ke waktu browser: '+timeStr+' ('+($('timezoneName')?$('timezoneName').value:'WIB')+')');
    await refresh();
    await loadPrayerTimes();
    return r;
  }catch(e){
    if(!quiet)notice('Gagal sinkronisasi jam: '+e.message,true);
    throw e;
  }
}

$('btnSyncTime').onclick=()=>action(()=>syncBrowserTime(false));
if($('btnSyncTime2')) $('btnSyncTime2').onclick=()=>action(()=>syncBrowserTime(false));
if($('timezoneName'))$('timezoneName').onchange=()=>action(()=>syncBrowserTime(false));

async function loadClockStatus(){
  try{
    const d=await api('/api/v1/clock/status',null,'GET');
    if(d.year&&d.month&&d.day&&$('manualDate')){
      const yStr=String(d.year).padStart(4,'0');
      const mStr=String(d.month).padStart(2,'0');
      const dStr=String(d.day).padStart(2,'0');
      $('manualDate').value=yStr+'-'+mStr+'-'+dStr;
    }
    if(d.hour!==undefined&&d.minute!==undefined&&$('manualTime')){
      const hStr=String(d.hour).padStart(2,'0');
      const minStr=String(d.minute).padStart(2,'0');
      const sStr=String(d.second||0).padStart(2,'0');
      $('manualTime').value=hStr+':'+minStr+':'+sStr;
    }
    if(d.hijri_adjustment!==undefined&&$('hijriAdjInput')){
      $('hijriAdjInput').value=d.hijri_adjustment;
    }
    if($('clockSyncStatusBadge')){
      $('clockSyncStatusBadge').textContent=d.synced?'Jam: Tersinkronisasi':'Jam: RTC / Manual';
      $('clockSyncStatusBadge').className='badge '+(d.synced?'badge-cyan':'badge-gold');
    }
  }catch(e){console.warn('loadClockStatus error',e)}
}

async function saveManualClock(){
  const dateVal=$('manualDate')?$('manualDate').value:'';
  const timeVal=$('manualTime')?$('manualTime').value:'';
  if(!dateVal||!timeVal){
    alert('Silakan tentukan tanggal dan jam terlebih dahulu');
    return;
  }
  const dateParts=dateVal.split('-');
  const timeParts=timeVal.split(':');
  if(dateParts.length!==3||timeParts.length<2){
    alert('Format tanggal atau jam salah');
    return;
  }
  const year=parseInt(dateParts[0],10);
  const month=parseInt(dateParts[1],10);
  const day=parseInt(dateParts[2],10);
  const hour=parseInt(timeParts[0],10);
  const minute=parseInt(timeParts[1],10);
  const second=timeParts.length>=3?parseInt(timeParts[2],10):0;
  const hijriAdj=parseInt($('hijriAdjInput')?$('hijriAdjInput').value:0,10)||0;

  action(async()=>{
    const r=await api('/api/v1/clock/manual',{year,month,day,hour,minute,second,hijri_adjustment:hijriAdj},'POST');
    notice(r.message||'Jam & tanggal manual berhasil diterapkan!');
    await loadPrayerTimes();
    await loadClockStatus();
    await refresh();
  });
}
if($('btnSaveManualClock')) $('btnSaveManualClock').onclick=saveManualClock;

$('btnUseGps').onclick=()=>{
  if(!navigator.geolocation){notice('Geolocation tidak didukung browser ini',true);return;}
  notice('Mengambil koordinat lokasi Anda…');
  navigator.geolocation.getCurrentPosition(pos=>{
    $('latitude').value=pos.coords.latitude.toFixed(4);
    $('longitude').value=pos.coords.longitude.toFixed(4);
    notice('Lokasi berhasil didapatkan: '+pos.coords.latitude.toFixed(4)+', '+pos.coords.longitude.toFixed(4)+'. Klik Simpan Lokasi.');
  },err=>{
    notice('Gagal mendapatkan lokasi: '+err.message,true);
  });
};

// Audio Handlers
function savePrayerAudioItem(pid){
  action(()=>api('/api/v1/prayer-audio',{
    prayer:pid,
    enabled:$('audio_en_'+pid).checked?'1':'0',
    file:$('audio_file_'+pid).value,
    lead_minutes:$('audio_lead_'+pid).value,
    volume:$('audio_vol_'+pid).value
  }));
}
window.savePrayerAudioItem=savePrayerAudioItem;

function testPrayerAudio(pid){
  action(()=>api('/api/v1/prayer-audio/test',{
    prayer:pid,
    volume:$('audio_vol_'+pid).value
  }));
}
window.testPrayerAudio=testPrayerAudio;

$('btnTestAudioPlay').onclick=()=>action(()=>{
  const sel=$('quickAudioTestFile').value;
  if(!sel)throw Error('Pilih file audio untuk diuji');
  return api('/api/v1/prayer-audio/test',{file:sel,volume:$('globalAudioVol').value});
});
$('btnTestAudioStop').onclick=()=>action(()=>api('/api/v1/prayer-audio/test',{command:'stop'}));

$('globalAudioVol').oninput=function(){
  $('audioVolVal').textContent=this.value+'%';
};
$('globalAudioVol').onchange=function(){
  action(()=>api('/api/v1/prayer-audio/test',{volume:this.value}));
};

document.querySelectorAll('[data-command]').forEach(b=>b.onclick=()=>action(()=>api('/api/control',{command:b.dataset.command})));
$('applyLayout').onclick=()=>action(()=>api('/api/control',{command:'layout',preset:$('layout').value}));
$('wifiForm').onsubmit=e=>{
  e.preventDefault();
  action(async()=>{
    const r=await api('/api/wifi',{ssid:$('ssid').value,password:$('password').value});
    $('password').value='';
    return r;
  });
};

let scanningWifi=false;
async function startWifiScan(){
  if(scanningWifi)return;
  scanningWifi=true;
  if($('scanWifiBtn')){$('scanWifiBtn').disabled=true;$('scanWifiBtn').textContent='Memindai…'}
  if($('wifiScanNotice'))$('wifiScanNotice').textContent='Sedang mencari jaringan Wi-Fi 2,4 GHz di sekitar…';
  try{
    await api('/api/wifi/scan',{});
    let attempts=0;
    const interval=setInterval(async()=>{
      attempts++;
      try{
        const res=await api('/api/wifi/scan');
        if(!res.scanning||attempts>=8){
          clearInterval(interval);
          renderWifiList(res.networks||[]);
          scanningWifi=false;
          if($('scanWifiBtn')){$('scanWifiBtn').disabled=false;$('scanWifiBtn').textContent='🔍 Pindai Wi-Fi'}
        }
      }catch(err){
        clearInterval(interval);
        scanningWifi=false;
        if($('scanWifiBtn')){$('scanWifiBtn').disabled=false;$('scanWifiBtn').textContent='🔍 Pindai Wi-Fi'}
      }
    },1200);
  }catch(err){
    notice('Gagal memindai Wi-Fi: '+err.message,true);
    scanningWifi=false;
    if($('scanWifiBtn')){$('scanWifiBtn').disabled=false;$('scanWifiBtn').textContent='🔍 Pindai Wi-Fi'}
  }
}
function renderWifiList(networks){
  const container=$('wifiList');
  if(!container)return;
  container.replaceChildren();
  if(!networks.length){
    const p=document.createElement('p');
    p.className='hint';
    p.style.textAlign='center';
    p.style.margin='12px 0';
    p.textContent='Tidak ada Wi-Fi ditemukan atau router di luar jangkauan (pastikan frekuensi 2,4 GHz).';
    container.append(p);
    return;
  }
  networks.forEach(net=>{
    const row=document.createElement('div');
    row.className='wifi-item';
    let sigLabel='Lemah';
    if(net.rssi>=-60)sigLabel='Kuat';
    else if(net.rssi>=-75)sigLabel='Sedang';
    const info=document.createElement('div');
    info.className='wifi-info';
    info.innerHTML=`<strong>${net.ssid}</strong><span class="wifi-badge">${sigLabel} (${net.rssi} dBm)</span><span class="wifi-badge">${net.secure?'🔒 Terkunci':'🔓 Terbuka'}</span>`;
    const btn=document.createElement('button');
    btn.type='button';
    btn.className='btn-connect';
    btn.textContent='Hubungkan';
    btn.onclick=()=>{
      $('ssid').value=net.ssid;
      if(net.secure){
        $('password').value='';
        $('password').focus();
        notice("Wi-Fi '"+net.ssid+"' dipilih. Silakan ketik password lalu klik 'Simpan & hubungkan'.");
      }else{
        $('password').value='';
        notice("Wi-Fi '"+net.ssid+"' (Terbuka tanpa password) dipilih. Klik 'Simpan & hubungkan'.");
      }
    };
    row.append(info,btn);
    container.append(row);
  });
}
if($('scanWifiBtn'))$('scanWifiBtn').onclick=startWifiScan;

/* Prioritas Mode */
document.querySelectorAll('.btn-mode').forEach(b=>{
  b.onclick=()=>action(async()=>{
    const m=Number(b.dataset.mode);
    const r=await api('/api/display',{command:'mode',mode:m});
    updateModeUI(m);
    return r;
  });
});

/* Video Stream & YouTube */
function checkYtUrl(url){
  const isYt=url.includes('youtube.com')||url.includes('youtu.be');
  $('ytAlert').hidden=!isYt;
}
$('streamUrl').oninput=function(){checkYtUrl(this.value)};
$('saveStream').onclick=()=>action(()=>api('/api/display',{command:'stream_config',url:$('streamUrl').value,fps:$('streamFps').value}));
$('startStream').onclick=()=>action(()=>api('/api/display',{command:'stream_config',url:$('streamUrl').value,fps:$('streamFps').value,start:'1'}));
$('stopStream').onclick=()=>action(()=>api('/api/display',{command:'stream_stop'}));

/* Pengaturan kecerahan */
function setBrightnessDisplay(val){$('brightnessVal').textContent=val;$('brightnessPct').textContent=Math.round(val/255*100)+'%'}
$('brightnessRange').oninput=function(){
  brightnessDragging=true;
  const val=Number(this.value);
  setBrightnessDisplay(val);
  clearTimeout(brightnessTimer);
  brightnessTimer=setTimeout(async()=>{
    try{await api('/api/brightness',{value:val,save:'0'})}catch(err){console.warn('Live brightness error',err)}
  },80);
};
$('brightnessRange').onchange=function(){
  brightnessDragging=false;
  const val=Number(this.value);
  action(()=>api('/api/brightness',{value:val,save:'1'}));
};
document.querySelectorAll('.btn-preset').forEach(b=>{
  b.onclick=()=>{
    const val=Number(b.dataset.b);
    $('brightnessRange').value=val;
    setBrightnessDisplay(val);
    action(()=>api('/api/brightness',{value:val,save:'1'}));
  };
});
$('saveBrightness').onclick=()=>action(()=>api('/api/brightness',{value:$('brightnessRange').value,save:'1'}));
$('toggleBlank').onclick=()=>action(()=>api('/api/brightness',{blank:'toggle'}));

let panelLoaded=false;
function panelScanChanged(){$('panelMapping').disabled=$('panelScan').value!=='16'}
$('panelScan').onchange=panelScanChanged;
function showPanelPreview(s){$('panelPreviewActions').hidden=!s.preview_active;$('panelPreviewState').textContent=s.preview_active?'Preview aktif: konfirmasi dalam '+Math.ceil(s.preview_remaining_ms/1000)+' detik.':s.restart_pending?'Controller akan restart...':'Konfigurasi aktif tersimpan.'}
async function loadPanel(){const s=await api('/api/panel');showPanelPreview(s);$('panelScan').value=String(s.scan);$('panelMapping').value=String(s.mapping>=4?0:s.mapping);panelScanChanged();panelLoaded=true}
$('confirmPanel').onclick=()=>action(()=>api('/api/panel',{command:'confirm'}));
$('cancelPanel').onclick=()=>action(()=>api('/api/panel',{command:'cancel'}));
function applyPanel(command){return action(()=>api('/api/panel',{scan:$('panelScan').value,command,mapping:$('panelScan').value==='32'?'4':$('panelScan').value==='16'?$('panelMapping').value:'5'}))}
$('savePanel').onclick=()=>applyPanel('save');
$('panelForm').onsubmit=e=>{e.preventDefault();applyPanel('preview')};
const effectOptions=[['none','Statis'],['fade','Fade'],['slide-left','Slide ke kiri'],['slide-right','Slide ke kanan'],['slide-up','Slide ke atas'],['slide-down','Slide ke bawah'],['blink','Blink'],['scroll-left','Scroll kiri'],['scroll-right','Scroll kanan'],['scroll-up','Scroll atas'],['scroll-down','Scroll bawah']];
for(const id of ['textEffect','imageEffect'])for(const [value,label] of effectOptions){const o=document.createElement('option');o.value=value;o.textContent=label;$(id).append(o)}
function color565(hex){const n=parseInt(hex.slice(1),16);return ((n>>19)&31)<<11|((n>>10)&63)<<5|((n>>3)&31)}
function colorHex(v){const r=Math.round(((v>>11)&31)*255/31),g=Math.round(((v>>5)&63)*255/63),b=Math.round((v&31)*255/31);return '#'+((r<<16)|(g<<8)|b).toString(16).padStart(6,'0')}
async function loadAnimation(){const s=await api('/api/animation');$('textContent').value=s.text;$('textEffect').value=s.text_effect;$('imageEffect').value=s.image_effect;$('textScale').value=s.text_scale;$('textColor').value=colorHex(s.text_color);$('backgroundColor').value=colorHex(s.background);$('durationMs').value=s.duration_ms;$('scrollSpeed').value=s.speed;$('animationLoop').checked=s.loop;$('imageVisible').checked=s.image_visible;animationLoaded=true}
$('animationForm').onsubmit=e=>{e.preventDefault();action(async()=>{const text=$('textContent').value;if(new TextEncoder().encode(text).length>192)throw Error('Teks maksimal 192 byte UTF-8');const settings={text,text_effect:$('textEffect').value,image_effect:$('imageEffect').value,text_scale:Number($('textScale').value),text_color:color565($('textColor').value),background:color565($('backgroundColor').value),duration_ms:Number($('durationMs').value),speed:Number($('scrollSpeed').value),loop:$('animationLoop').checked,image_visible:$('imageVisible').checked};return api('/api/animation',{command:'configure',settings:JSON.stringify(settings)})})};
document.querySelectorAll('[data-motion]').forEach(b=>b.onclick=()=>action(()=>api('/api/animation',{command:b.dataset.motion})));

let pollFailures=0,fastTick=0,timeSyncedOnce=false;
async function poll(){
  if(busy||polling||document.hidden||Date.now()<nextPoll)return;
  polling=true;
  try{
    await refresh();
    if(!timeSyncedOnce){
      timeSyncedOnce=true;
      await loadClockStatus();
    }
    if(pollFailures>=3)notice('Koneksi ke ESP32 pulih');
    pollFailures=0;
    if(!panelLoaded)await loadPanel();else showPanelPreview(await api('/api/panel'));
    if(!animationLoaded)await loadAnimation();
    fastTick++;
    if(fastTick%4===1){
      await loadPrayerTimes();
      await loadClockStatus();
      await loadAudioSchedule();
      await loadStorageStatus();
      await media();
    }
  }catch(e){
    pollFailures++;
    if($('connection'))$('connection').textContent=pollFailures>=3?'Koneksi terputus':'Mencoba kembali...';
    if(pollFailures>=3)notice('Tidak dapat menghubungi ESP32. Pastikan HP/Laptop tetap tersambung ke hotspot atau router yang sama.',true);
  }finally{
    polling=false;
    nextPoll=Date.now()+(liveAnimation?250:1000);
  }
}
poll();
setInterval(poll,500);
</script></body></html>)HTML";
