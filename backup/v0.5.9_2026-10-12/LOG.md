# Log Perubahan

Format entri: `[Tanggal] vX.Y.Z — ringkasan`
Setiap versi di-backup ke folder `backup/vX.Y.Z_tanggal/` sebelum perubahan dilakukan.

---

## [2026-10-12] v0.5.9 — Jarak baris lebih lapang (BARIS_H 8→10)

**Perubahan:**
- `ui.h`: `BARIS_H` 8 → **10** — jarak antar baris 1px → 3px
  (teks tidak lagi rapat); 3 baris per layar, sisanya via scroll
- `ui.h`: `uiScrollSet` membulatkan `uiScrollMaks` ke atas ke
  kelipatan `BARIS_H` supaya baris terakhir selalu bisa dimunculkan
- `gambarKalibStepper`: header ringkas 10px + 3 pompa (y=10/20/30)
  + 1 hint (y=37) — muat tanpa scroll
- `gambarError`: `[ENTER] reset` dipindah ke header kanan,
  sisa 3 baris (kode, pesan, Exhaust ON)
- `gambarKalibSensor`: switch senyawa disembunyikan saat ter-scroll
  keluar area
- `gambarCfgEdit`: `EDIT_VISIBLE` 4 → 3
- `gambarPresetDetail`: jarak baris `dy` 8 → `BARIS_H`
- Kalib DS18B20 & Agitasi: ditambahkan scroll (4 baris → 3 terlihat)
- `config.h`: `FW_VERSI` → v0.5.9

**Backup sebelum perubahan:** `backup/v0.5.8_2026-10-12/`

---

## [2026-10-12] v0.5.8 — Offset 20px + sistem scroll halaman (font tak dikecilkan)

**Perubahan:**
- `config.h`: `OLED_TOP_OFF` 10 → **20**
- `ui.h`:
  - `BARIS_H` = 8 (jarak baris standar font size 1, tidak dipadatkan lagi)
  - sistem scroll halaman: `uiScrollSet/uiScrollAtur/uiScrollKe` +
    `gambarScrollbar()`; scroll di-reset otomatis saat pindah halaman
  - **Scroll manual (ATAS/BAWAH):** Cadangan, Kalib MQ-135,
    DS18B20, Agitasi, Tentang
  - **Auto-scroll:** Kalib Sensor — ATAS/BAWAH tetap memilih titik,
    tampilan menggulir agar titik terpilih selalu terlihat
  - Kalib Stepper: nilai test pindah ke header ("Step N"), 3 pompa
    + 1 hint (muat tanpa scroll)
  - Proses: "sisa" pindah ke header; waktu y=33, bar 49–56,
    status (fase + %/ml) y=57
  - Cfg Edit: `EDIT_VISIBLE` 5 → 4 (gulir dengan ATAS/BAWAH)
- `oled_pcb_baruuu.ino`: ATAS/BAWAH = scroll pada halaman tersebut;
  di Cadangan, KIRI/KANAN sekarang memilih baris
- `config.h`: `FW_VERSI` → v0.5.8

**Backup sebelum perubahan:** `backup/v0.5.7_2026-10-12/`

---

## [2026-10-12] v0.5.7 — Geser seluruh UI 10px ke bawah (baris atas OLED rusak)

**Perubahan:**
- `config.h`: tambah `OLED_TOP_OFF 10`
- `ui.h`: seluruh UI digeser 10px ke bawah memakai offset
  terpusat — `TY(y)` untuk bentuk, `teksTengah/teksKanan/teksKiri`
  untuk teks (koordinat logis, y=0 = baris pertama yang terlihat);
  area logo jadi 54px (y=10–63) dan semua halaman dipadatkan agar
  tetap muat:
  - Kalib Sensor: header ringkas 10px + switch kotak putih penuh
    (y=20) + 5 titik jarak 7px (y=29/36/43/50/57)
  - Kalib Stepper: nilai test pindah ke header ("Step N"), 3 pompa
    + 3 hint (semua masuk layar)
  - Kalib MQ-135: 4 baris data + 2 hint; DS18B20 & Agitasi:
    2 baris data + 2 hint (semua hint dipendekkan &lt;=128px)
  - Proses: waktu/sisa/bar/fase dikompakan ke y=24/41/49/57
  - Cadangan: 5 baris jarak 8px; Cfg Edit: `EDIT_ROW_H` 10→8
  - Tentang: tambah baris "Sensor: GPIO 11/12/13/14/3"
- `config.h`: `FW_VERSI` → v0.5.7
- Fix compile: `teksKiri(F("PCB max"))` → `teksKiri("PCB max")` — di
  Arduino-ESP32 3.x `F()` menghasilkan `const __FlashStringHelper*`
  yang tidak otomatis bisa jadi `const char*`

**Backup sebelum perubahan:** `backup/v0.5.6_2026-10-12/`

---

## [2026-10-12] v0.5.6 — Kembali ke layout v0.5.4 (switch y=14)

**Perubahan:**
- `ui.h` (`gambarKalibSensor`): dikembalikan ke
  layout v0.5.4 — switch senyawa (kotak putih penuh
  `[ < HCl > ]`) di y=14 di bawah header, 5 titik
  kalibrasi di bawahnya (y=25/33/41/49/57, jarak 8px).
  Layout v0.5.5 (switch di y=50, titik di atas)
  dicoba dan tidak dipakai; tersimpan di
  `backup/v0.5.5_2026-10-12/`
- `config.h`: `FW_VERSI` → v0.5.6

**Backup sebelum perubahan:** `backup/v0.5.5_2026-10-12/`

---

## [2026-10-12] v0.5.5 — Switch senyawa dipindahkan ke y=50 (bawah)

**Perubahan:**
- `ui.h` (`gambarKalibSensor`): susunan halaman diubah —
  5 titik kalibrasi di atas (y=13/20/27/34/41, jarak 7px),
  selektor senyawa (kotak putih penuh `[ < HCl > ]`)
  dipindahkan ke **y=50 dari atas** sesuai permintaan,
  kapasitas "1000ml" tetap di kanan switch; konstanta
  `Y_SWITCH` dipakai supaya mudah diatur
- `config.h`: `FW_VERSI` → v0.5.5

**Backup sebelum perubahan:** `backup/v0.5.4_2026-10-12/`

---

## [2026-10-12] v0.5.4 — Switch senyawa lebih menonjol (kotak putih penuh)

**Perubahan:**
- `ui.h` (`gambarKalibSensor`): selektor senyawa dari kotak
  berbingkai jadi **kotak putih penuh** (teks hitam) — lebih
  menonjol; digeser 1px ke bawah (y=14, tinggi 10px,
  baris titik tetap y=25/33/41/49/57, semua muat)
- `config.h`: `FW_VERSI` → v0.5.4

**Backup sebelum perubahan:** `backup/v0.5.3_2026-10-12/`

---

## [2026-10-12] v0.5.3 — Layout halaman kalibrasi sensor level (header + selector)

**Perubahan:**
- `ui.h` (`gambarKalibSensor`): susun ulang layout atas yang
  sebelumnya padat — header jadi bar status ("Kalibrasi *" kiri,
  "Raw xxxx" kanan); senyawa ditampilkan dalam kotak berbingkai
  `[ < HCl > ]` (jelas sebagai switch, KIRI/KANAN) dengan kapasitas
  "1000ml" di kanannya; 5 titik kalibrasi mulai y=25 (semua
  terlihat, jarak lebih lega)
- `config.h`: `FW_VERSI` → v0.5.3

**Backup sebelum perubahan:** `backup/v0.5.2_2026-10-12/`

---

## [2026-10-12] v0.5.2 — Perbaikan halaman kalibrasi sensor level

**Perubahan:**
- `ui.h` (`gambarKalibSensor`): layout diperbaiki — titik 75%/100% &
  "Raw now" sebelumnya di bawah layar; kini: header `Kalib: <senyawa>`
  (senyawa jelas kelihatan, `< >` = ganti dgn KIRI/KANAN), baris info
  `Kap 1000ml Raw <nilai>`, 5 titik jarak 8px (semua terlihat),
  titik terpilih di-highlight
- `oled_pcb_baruuu.ino`: navigasi halaman kalibrasi sensor ditukar —
  KIRI/KANAN = ganti senyawa, ATAS/BAWAH = scroll titik kalibrasi
  (sebelumnya terbalik)
- `storage.h`: kapasitas default tangki 500 ml → 1000 ml
- `config.h`: `FW_VERSI` → v0.5.2

**Backup sebelum perubahan:** `backup/v0.5.1_2026-10-12/`

---

## [2026-10-12] v0.5.1 — Migrasi sensor level ke GPIO langsung & pin TB6612FNG

**Perubahan:**
- `config.h`: hapus `MPR121_ADDR`; tambah `PIN_SENSOR_HCL/H2O2/H2O/SABUN/LIMBAH`
  (GPIO 11/12/13/14/3); pin TB6612FNG jadi 15 (agitasi), 16 (sabun), 17 (limbah),
  18 (fan); hapus `PIN_TB_STBY` (STBY di-tie 3.3V di hardware); tambah
  `SENSOR_ANALOG`, `SENSOR_ACTIVE_LOW`, `SENSOR_HYST`; `FW_VERSI` → v0.5.1
- `sensor.h`: ditulis ulang — baca 5 sensor langsung dari GPIO (analog ADC 12-bit
  via `analogRead`, atau float switch digital), rata-rata 4 sampel + histeresis;
  tambah `sensorKosong()` & `sensorPenuh()`
- `aktuator.h`: `pompaDcInit()` tidak lagi mengatur STBY (tie 3.3V)
- `process.h`: watchdog timeout dosing (estimasi 2× durasi langkah + 30 s); dosing
  error `SUMBER_HABIS` kalau sumber habis saat dosing; drain berhenti lebih awal
  saat limbah penuh; tracking ml terpompa; dosing dengan ml=0 langsung dilewati
  (perbaikan hang)
- `ui.h`: layar proses menampilkan ml terpompa/target saat dosing; halaman Tentang
  tidak lagi mencantumkan alamat 0x5A
- `storage.h`: default titik kalibrasi disesuaikan ke skala ADC 12-bit
- `oled_pcb_baruuu.ino`: pesan error sensor level (bukan MPR121)
- Fix compile: `analogSetAttenuation(ADC_ATTEN_DB_11)` dihapus (enum
  tidak tersedia di Arduino-ESP32 3.3.11); pakai default 11 dB

**Asumsi (dari "Pertanyaan Terbuka" dokumen):**
- Sensor level = analog (default `SENSOR_ANALOG=1`); ubah ke 0 untuk float
  switch digital (`SENSOR_ACTIVE_LOW=1` = LOW saat penuh)
- STBY TB6612FNG di-tie 3.3V (GPIO15 dipakai untuk agitasi)
- MS1/MS2/MS3 DRV8825 di-hardwire; `STEPPER_MICROSTEP` tetap 16 — dokumen
  menyebut 1/36, tapi DRV8825 maksimum 1/32; perlu konfirmasi hardware
- Drain "tangki proses kosong" belum terdeteksi (tidak ada sensor tangki proses):
  drain tetap pakai timeout + deteksi limbah penuh

**Backup sebelum perubahan:** `backup/v0.5.0_2026-10-12/`

---

## [2026-10-12] v0.5.0 — Baseline awal (kondisi sebelum migrasi)

Snapshot kondisi firmware saat ini, dicatat sebagai baseline sebelum perubahan lanjutan.

**File:** `oled_pcb_baruuu.ino` + `aktuator.h`, `config.h`, `data.h`, `input.h`, `presetops.h`, `process.h`, `sensor.h`, `storage.h`, `ui.h`

**Fitur yang sudah ada:**
- UI 6 tombol (PCF8574 @0x20) + OLED SSD1306 128×64 (@0x3C), animasi list 200 ms
- Preset (maks 10): tambah/edit/duplikat/hapus; mode dosing auto (47/24/29 %) atau manual; volume 50–1000 ml; mode agitasi auto/manual; speed agitasi 1–5
- Proses otomatis 16 fase: cek level → dosing HCl/H2O2/H2O (stepper non-blocking) → jeda campur → etching (agitasi) → drain → bilas sabun → bilas air → pendinginan → selesai
- Kalibrasi 5 menu: sensor level 5 titik, stepper ml/step, MQ-135 baseline, DS18B20 offset, agitasi duty min
- Penyimpanan NVS (Preferences) + CRC untuk preset & kalibrasi, format versi 2
- E-stop (GPIO8) via interrupt, abort proses, halaman ERROR
- Buzzer nada, LED strip, exhaust fan

**Diketahui belum sesuai `etching-otomatis.md` (rencana lanjutan):**
- Sensor level masih MPR121 (@0x5A), belum GPIO langsung 11/12/13/14/3
- Pin TB6612FNG masih lama: agitasi 11, limbah 12, sabun 13, fan 14, STBY 15
- `STEPPER_MICROSTEP` = 16 (dokumen: 1/36 → 7200 step/putaran)
- Drain hanya timeout tetap (15 s/10 s), belum berdasarkan level tangki proses kosong / limbah penuh
- Belum ada watchdog timeout per fase (dosing bisa hang kalau pompa macet)
- Layar proses belum menampilkan ml terpompa
- DS18B20 masih placeholder (belum pakai OneWire)
- `HAL_PLACEHOLDER` masih ada tapi tak terpakai

**Backup:** `backup/v0.5.0_2026-10-12/`
