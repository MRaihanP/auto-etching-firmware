# Log Perubahan

Format entri: `[Tanggal] vX.Y.Z — ringkasan`
Setiap versi di-backup ke folder `backup/vX.Y.Z_tanggal/` sebelum perubahan dilakukan.

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
