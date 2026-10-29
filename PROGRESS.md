# PROGRESS — Checklist Implementasi Firmware Etching Otomatis

Firmware: **Etching v0.6.0** · Board: **ESP32-S3 DevKitC**
Rencana teknis lengkap: `PROCESS.md` · Ringkasan: `SUMMARY.md` · Riwayat: `LOG.md`

Legenda: ✅ **Sudah ada & berfungsi** · ⚠️ **Ada tapi belum fungsional/terpakai** · ⬜ **Belum ada**
Referensi `file:line` merujuk ke kode di branch `main`.

---

## 0. Status Lineage Versi (SUDAH DIPUTUSKAN 2026-10-16)

**Keputusan: lineage v0.5.x adalah basis kerja yang SAH. Lineage v0.6–v0.8 DIARSIPKAN dan TIDAK DIPAKAI.**

Konteks: folder `backup/` memuat snapshot `v0.5.0`–`v0.8.2` dan `LOG.md`
mendokumentasikan sampai `v0.8.3` dengan arsitektur berbeda (`aktuator.h`,
`PIN_PWM_FAN`, `fanSet(duty)`). Garis itu **rusak dan tidak pernah di-commit ke
`main`** (terverifikasi: `git log -S'FW_VERSI "v0.8'` kosong; hanya 2 commit yang
pernah menyentuh `config.h`). Working tree `main` = lineage v0.5.x yang **uji
fisik terbukti jalan**. Bukan rollback — v0.6–v0.8 adalah percobaan yang gagal.

- [x] ✅ **Lineage v0.5.x ditetapkan sebagai basis kerja** (keputusan user, 2026-10-16)
- [x] ✅ **v0.6–v0.8 diarsipkan** — snapshot `backup/v0.6.0_…`–`v0.8.2_…` dan entri `LOG.md` v0.6–v0.8 diperlakukan sebagai **riwayat percobaan gagal**, bukan acuan implementasi
- [x] ✅ **`FW_VERSI` = `v0.5.1`** (`config.h:113`) — nomor versi lanjut sesuai keputusan
- [x] ✅ **Entri `LOG.md` v0.5.1 ditulis**, dengan penanda bahwa lineage v0.6–v0.8 dibatalkan
- [x] ✅ **Header `LOG.md`** diberi catatan lineage supaya tidak menyesatkan di masa depan
- [x] ✅ **`etching-otomatis.md` dianotasi "usang"** di bagian pin map (fan di GPIO 18)

---

## 1. GPIO & Hardware

- [x] ✅ Pin map stepper final **DRV8825 1/32**: SLP/STEP/DIR = 1/2/42 (HCl), 41/40/39 (H2O2), 38/47/21 (Air) — rencana TMC2209 dibatalkan
- [x] ✅ `PUMP_STEP_PER_REV 6400` (200 × 1/32) + RPM 400–700 di `config.h`
- [x] ✅ Mikrostep dikonfirmasi **1/32** (jumper MS1/MS2/MS3 di modul DRV8825)
- [x] ✅ I2C SDA=4, SCL=5, 400 kHz (`config.h:5-7`, `.ino:545-546`)
- [x] ✅ OLED SSD1306 0x3C 128×64 terinisialisasi (`config.h:10-12`, `.ino:563`)
- [x] ✅ PCF8574 0x20 — 6 tombol, inisialisasi tulis `0xFF` (`config.h:21`, `.ino:548`)
- [x] ✅ Error I2C dideteksi & dicatat ke Serial (`.ino:548`, `.ino:563`)
- [x] ✅ 5 GPIO sensor level (11/12/13/14/3) `INPUT` (`config.h:54-58`, `sensor.h:37`)
- [x] ✅ 3 PWM TB6612FNG (15/16/17) — agitasi, pompa sabun, pompa limbah (`dcmotor.h:14-22`)
- [x] ✅ **v0.5.1: Exhaust fan dilepas dari TB6612FNG** — arus kipas > 1.2 A, disambung langsung ke 12V & menyala terus; GPIO 18 kini bebas (`config.h:78-84`, `dcmotor.h`)
- [x] ✅ `PIN_PWB_D2` (GPIO 18) dihapus dari konfigurasi + seluruh kode
- [x] ✅ Sketch uji modul: `test/tb6612fng/tb6612fng.ino` (Serial Monitor, non-blocking, 14 tahap uji)
- [x] ✅ Buzzer pasif GPIO 6 via LEDC (`config.h:93`, `buzzer.h:14-18`)
- [x] ✅ DS18B20 di GPIO 10, 3 sensor pada satu bus 1-Wire (`config.h:95`, `sensor.h:116-117`)
- [x] ✅ MQ-135 analog GPIO 9 + divider R1/R2 (`config.h:96,99-107`, `sensor.h:210-228`)
- [x] ✅ E-stop GPIO 8 `INPUT_PULLUP` (`config.h:107`, `.ino:559`)
- [ ] ⬜ **LED status / LED strip (GPIO 7)** — `#define` ada (`config.h:98`), tidak pernah dipakai kode
- [ ] ⬜ RGB WS2812 indikator per sensor (rencana awal GPIO 48 — kini bentrok dengan DIR HCl; GPIO 18 kini alternatif)

---

## 2. Modul UI (`ui.h`, `.ino`, `input.h`, `data.h`)

- [x] ✅ Menu utama 4 item: Pilih Preset / Konfigurasi / Level Cairan / Kalibrasi (`data.h:37-42`)
- [x] ✅ Header + list dengan animasi scroll (smooth) (`ui.h:47-94`, `listGeser`/`listUpdateAnimasi`)
- [x] ✅ Toast pesan singkat (`ui.h:806-819`, dipakai di 6 tempat)
- [x] ✅ Dialog konfirmasi (simpan/batal/hapus) (`ui.h:279-287`)
- [x] ✅ Indikator scroll + panah atas/bawah (`ui.h:191-199`, `ui.h:454-462`)
- [x] ✅ Halaman Daftar Preset + Detail Preset (scroll 5 px) (`ui.h:157-275`)
- [x] ✅ Halaman Konfigurasi: List → Aksi (Edit/Duplikat/Hapus) → Edit 9 field (`ui.h:344-409`)
- [x] ✅ Halaman Level Cairan: 5 sensor, persen + bar progres (`ui.h:411-463`)
- [x] ✅ Halaman Proses: nama preset, timer MM:SS, progress bar, sisa waktu, status fase (`ui.h:289-333`)
- [x] ✅ 6 tombol PCF8574 + debounce 40 ms + blocking first-press (`input.h:45-70`)
- [x] ✅ Navigasi ATAS/BAWAH/KIRI/KANAN/ENTER/BACK di semua halaman (`.ino:75-533`)
- [x] ✅ Render dirty-flag + interval 20 ms, tidak render saat idle (`.ino:635`)
- [x] ✅ Tombol dibaca ulang setelah `oled.display()` (`.ino:647`)
- [x] ✅ Kompensasi baris atas OLED rusak (`UI_Y_OFFSET` = 15 px) (`config.h:18`, `ui.h:13-46`)
- [x] ✅ Konversi suhu DS18B20 non-blocking (`setWaitForConversion(false)`) (`sensor.h:138`)
- [x] ✅ Halaman Proses hanya membaca state, tidak mengubah state proses (pola read-only)
- [ ] ⬜ **Halaman ALARM baru** (`HAL_ALARM`) untuk semua kondisi gagal
- [ ] ⬜ Layar proses: progress **per fase** (bukan persen total)
- [ ] ⬜ Layar proses: panel ml terpompa per bahan (HCl/H2O2/H2O)
- [ ] ⬜ Layar proses: ikon status aktuator (agitasi/drain/sabun — fan tidak lagi dikendalikan)
- [ ] ⬜ Layar proses: badge peringatan gas MQ-135 / suhu tinggi
- [ ] ⬜ Layar proses: tampilkan siklus agitasi ke berapa
- [ ] ⬜ Badge permanen "E-STOP" di header saat latch aktif
- [ ] ⬜ Layar selesai: ringkasan total ml terpompa + durasi aktual
- [ ] ⬜ Tampilkan nomor versi firmware di OLED (sekarang hanya Serial, `.ino:540,576`)
- [ ] ⬜ Level Cairan: status ISI/KOSONG + ambang (define `AMBANG_MARGIN`/`AMBANG_HYST` ada tapi tak terpakai, `config.h:61-62`)
- [ ] ⬜ Halaman `HAL_PLACEHOLDER` & `gambarPlaceholder()` — kode mati, tidak pernah dipanggil (`ui.h:336-342`)
- [ ] ⬜ Halaman Kalibrasi Agitasi masih pakai `delay()` 1000–2000 ms → UI macet & E-stop lambat (`ino:311,317`)

---

## 3. Modul Dosing (`process.h` § DOSING, `motor.h`, `presetops.h`)

- [x] ✅ Driver stepper non-blocking via FastAccelStepper (MCPWM/PCNT) (`motor.h`)
- [x] ✅ Konversi ml → jumlah langkah dari kalibrasi ml/rotasi (`motor.h`)
- [x] ✅ 3 pompa (HCl/H2O2/H2O) dijalankan **paralel** saat fase DOSING (`process.h:28-33`)
- [x] ✅ `pompaMulai()` / `pompaStopAll()` / `pompaSelesai()` / `pompaProgres()` tersedia
- [x] ✅ Hitung otomatis %H2O dan ml tiap bahan dari %HCl/%H2O2 + volume (`presetops.h:17-22`)
- [x] ✅ Validasi rentang semua field preset (`presetops.h:49-62`)
- [x] ✅ Halaman kalibrasi stepper: START 10 rotasi, RPM 400–700, hasil ukur 0,1 ml, ml/rotasi
- [x] ✅ Dosis 3 pompa berjalan **paralel** (sesuai keputusan desain di `PROCESS.md` §0)
- [x] ✅ Kecepatan pompa berbasis RPM (default 500, batas 400–700)
- [x] ✅ `langkahPerMl = PUMP_STEP_PER_REV / mlPerRotasi` (DRV8825 1/32 = 6400)
- [x] ✅ Simpan kalibrasi rpm + ml/rotasi ke NVS (key `pompa`, terpisah dari `kal`)
- [x] ✅ Selesai dosing berbasis kondisi pompa (bukan fraksi waktu total)
- [x] ✅ Watchdog timeout dosing (2× estimasi + 10 detik)
- [x] ✅ Jeda menghentikan pompa dan melanjutkan sisa langkah
- [x] ✅ Deteksi posisi tidak maju > 3 detik (bukan deteksi stall mekanis)
- [ ] ⬜ Dosis memperhitungkan sisa cairan: `butuh_i = target_i − (persenSensor_i × volumeTangki_i / 100)`
- [ ] ⬜ Fase `FASE_CEK` pre-flight: validasi sensor & volume tangki
- [ ] ⬜ Cek ketersediaan cairan → `ERR_LEVEL_KURANG` sebelum pompa nyala
- [ ] ⬜ Volume tangki (`VOL_TANGKI_*`) sebagai data, bukan `#define` statis

---

## 4. Modul Etching (`process.h` § ETCHING, `dcmotor.h`)

- [x] ✅ Motor agitasi PWM pada speed 1–5 (map 80–255) (`dcmotor.h:24-26`)
- [x] ✅ Agitasi menyala di fase ETCHING dan RINSING (`process.h:36,44`)
- [x] ✅ Agitasi mati di DRAINING, DOSING, SELESAI, dan saat batal
- [x] ✅ Field `agitasiSpeed` dapat diubah di preset (1–5) (`presetops.h:43`)
- [x] ✅ Field `agitasiDetik` dapat diubah di preset (1–60 s) (`presetops.h:42`)
- [x] ✅ **Exhaust fan selalu menyala** — disambung langsung ke 12V (v0.5.1), tidak lagi lewat driver
- [ ] ⚠️ `agitasiDetik` **tidak dipakai proses** — hanya tampil di UI (`presetops.h:42`, `ui.h:263`)
- [ ] ⬜ Siklus agitasi periodik: ON N detik → OFF N detik → ulangi
- [ ] ⬜ Ramp-up speed agitasi 1 s (hindari shok motor)
- [ ] ⬜ Proteksi suhu: `sensorSuhu(1)` > 70 °C → turunkan speed; > 85 °C → gagal
- [ ] ⬜ Proteksi gas: `mq135PPM()` > ambang → peringatan di UI
- [ ] ⬜ Auto-stop / pause saat suhu/gas di luar batas
- [ ] ⬜ Hitung & simpan budget durasi fase etching per preset
- [ ] ⬜ Cek drop voltage 12V: fan (selalu nyala) + modul TB6612FNG + motor agitasi berbagi adaptor
- [ ] ⬜ Fuse / proteksi hubung-short pada jalur 12V motor

---

## 5. Modul Draining & Rinsing (`process.h` § DRAIN/RINSE)

- [x] ✅ Pompa Limbah (TB6612FNG #2 PWA) PWM on/off (`dcmotor.h:30`)
- [x] ✅ Pompa Limbah menyala di fase DRAINING (`process.h:40`)
- [x] ✅ Pompa Limbah mati saat SELESAI & batal (`process.h:47-51`, `process.h:102-116`)
- [x] ✅ Rinsing: pompa air (stepper #3) 100 ml + agitasi (`process.h:43-45`)
- [x] ✅ Exhaust fan tidak lagi dikendalikan firmware (langsung 12V, selalu nyala)
- [ ] ⚠️ DRAINING/RINSING **selesai berbasis fraksi waktu total preset** (75%/88%/100%, `process.h:69-75`), bukan durasi per sub-fase
- [ ] ⚠️ Pompa air **(belum pernah dipakai di proses**, hanya di kalibrasi (`process.h:50`, `process.h:112`)
- [ ] ⬜ Durasi tetap per sub-fase: `T_DRAINING_MS`, `T_RINSE_AIR_MS`, `T_RINSE_SABUN_MS`, `T_DRAIN_AKHIR_MS`
- [ ] ⬜ Guard overfill: `sensorPersen[LIMBAH] > 95%` → stop + alarm
- [ ] ⬜ Pompa air sabun menyala di fase RINSE_SABUN (belum ada sama sekali)
- [ ] ⬜ Deteksi "tangki proses kosong" (opsional, bila sensor ditambahkan — GPIO 18 kini bebas)
- [ ] ⬜ Histeresis/debounce level agar pompa tidak thrash
- [ ] ⬜ Volume bilasan air & sab dapat diatur dari preset

---

## 6. Modul Emergency Stop & Keselamatan (`process.h` § ESTOP)

- [x] ✅ Baca E-stop GPIO 8 tiap loop (`.ino:611`)
- [x] ✅ E-stop LOW → `prosesBatal()` + toast "E-STOP!" (`.ino:612-615`)
- [x] ✅ E-stop LOW → matikan agitasi, pompaSabun, pompaLimbah, semua pompa stepper (`.ino:616-619`)
- [x] ✅ Semua aktuator mati saat FASE_SELESAI (`process.h:47-52`)
- [x] ✅ Semua aktuator mati saat proses dibatalkan (`process.h:102-116`)
- [x] ✅ `fanSet()` dihapus dari loop E-stop (v0.5.1) — kipas bukan output firmware lagi
- [x] ✅ Dialog konfirmasi batal saat proses berjalan (`.ino:125-141`)
- [x] ✅ Log Serial untuk setiap transisi fase & pembatalan (`process.h:85-91`, `103-105`)
- [ ] ⬜ Estop dipindah ke **baris pertama `loop()`** (saat ini setelah `tanganiTombol()`, `prosesUpdate()`, `motorUpdate()`, `sensorUpdate()`, `buzzerUpdate()` — `.ino:611`)
- [ ] ⬜ Latch `estopAktif` — tidak auto-reset saat pin kembali HIGH
- [ ] ⬜ Reset latch hanya lewat BACK di `HAL_ALARM`, dan hanya bila pin HIGH
- [ ] ⬜ Fungsi tunggal `semuaAktuatorMati()` (saat ini 4 aktuator stop di 4 tempat: `process.h:29`, `process.h:47-51`, `process.h:110-114`, `.ino:616-619`)
- [ ] ⬜ Cut power driver stepper (TMC_PDN LOW) saat stop
- [ ] ⬜ `prosesGagal(kode)` sebagai satu-satunya jalan keluar kondisi abnormal
- [ ] ⬜ Timeout/watchdog per fase (DOSING, DRAINING, RINSE)
- [ ] ⬜ Buzzer nada alarm (berulang) berbeda dari nada "selesai" (`buzzer.h:30-34`)
- [ ] ⬜ Halaman alarm + instruksi pemulihan
- [ ] ⬜ Jeda (`prosesToggleJeda`) ikut mematikan output, bukan hanya timer (`process.h:95-101`)
- [ ] ⬜ Deteksi kegagalan sensor saat proses (DS18B20/touch) → abort aman
- [ ] ⬜ Fail-safe saat OLED/PCF8574 hilang (hanya E-stop berfungsi, beri peringatan)

---

## 7. Sensor & Kalibrasi (`sensor.h`, `storage.h`)

- [x] ✅ Baca 5 sensor level via `touchRead()` (strip nikel non-contact) (`sensor.h:59-72`)
- [x] ✅ Moving average window 8 untuk redam noise (`config.h` filter, `sensor.h:13`, `sensor.h:59-72`)
- [x] ✅ Konversi raw → persen dengan clamp 0–100 (`sensor.h:74-81`)
- [x] ✅ Kalibrasi 0% & 100% per sensor dari UI (`.ino:389-433`)
- [x] ✅ Kalibrasi level disimpan ke NVS + CRC + auto-reject bila tidak valid (`storage.h:87-104`)
- [x] ✅ Baseline default 113000/156000 bila kalibrasi belum ada (`storage.h:120-121`)
- [x] ✅ Halaman Kalibrasi → 4 submenu (Motor, Sensor Level, Sensor Suhu, Sensor Udara) (`ui.h:466-467`)
- [x] ✅ **v0.5.1: menu Motor jadi 6 item** — Kipas Exhaust dihapus (`ui.h:470-474`, `JUMLAH_MENU_MOTOR`)
- [x] ✅ Kalibrasi MQ-135: rekam acuan udara bersih & kotor (`.ino:365-386`)
- [x] ✅ Kalibrasi suhu: pilih sensor per blok + offset ±0,1 °C (`.ino:325-362`)
- [x] ✅ Kalibrasi agitasi: 3 level SPD1/SPD2/SPD3 (duty %) tanpa ml
- [x] ✅ Kalibrasi DC: uji 10 detik + hasil ml → ml/detik + margin 10%
- [x] ✅ **v0.5.2: kalibrasi pompa stepper** — rotasi penuh pada RPM 400–700, hasil ml/rotasi disimpan NVS (key `pompa`)
- [x] ✅ **v0.5.3: halaman Test Motor** — START/STOP kontinu per pompa atau ketiga pompa sekaligus (Kalibrasi → Motor → Test Motor)
- [x] ✅ **v0.5.4: ramp naik/turun pompa** — akselerasi 50.000 langkah/s², stop memakai `stopMove()`, E-stop/stall tetap `forceStop()`
- [x] ✅ **v0.6.0: ramp semua motor + kalibrasi baru** — stepper 10 rotasi (ramp-aware), DC 10 detik → ml/detik, agitasi SPD1–3, Test Motor 7 baris scroll, NVS `pompa` v2
- [x] ✅ Log diagnostik `[SENSOR]`/`[SUHU]`/`[STORE]`/`[KALIB*]` ke Serial
- [x] ✅ **v0.6.0: `kalibDuty[]` dihapus** — diganti `kalibDutyDC[2]` + `agitasiSpdDuty[3]`
- [ ] ⚠️ **Address DS18B20 hardcode** di `sensor.h:119-123` — harus cocok address fisik
- [ ] ⚠️ `kalibSuhuAddr[]` & `suhuOffsetX10[]` **tidak disimpan ke NVS** (hilang saat reset) (`sensor.h:126-129`)
- [ ] ⚠️ `mq135Normal`/`mq135Kotor` **tidak disimpan ke NVS** dan tidak dipakai proteksi (`data.h:119-122`)
- [x] ✅ **v0.6.0: semua parameter kalibrasi motor disimpan ke NVS** (`pompa` v2: rpm, ml/rotasi, duty DC, ml/detik, duty agitasi)
- [x] ✅ **v0.6.0: uji motor non-blocking** — `kalibDcUpdate()` + `kalibStepperUkur`, tanpa `delay()` saat uji
- [ ] ⬜ Deteksi sensor touch mati (nilai tidak berubah / selalu 0%)

---

## 8. Penyimpanan & Data (`storage.h`, `presetops.h`, `data.h`)

- [x] ✅ Maks 10 preset (`data.h:21`)
- [x] ✅ 5 preset default saat pertama kali jalan (`data.h:25-31`)
- [x] ✅ CRUD: tambah, duplikat, hapus dengan pergeseran array (`presetops.h:83-110`)
- [x] ✅ Nama preset unik otomatis "Preset N" (`presetops.h:71-81`)
- [x] ✅ Simpan preset ke NVS sebagai satu blob + versi + CRC32 (`storage.h:26-43`)
- [x] ✅ Muat preset: reject bila versi beda / CRC rusak / data tak valid (`storage.h:45-61`)
- [x] ✅ Auto-reset ke default bila data korup (`storage.h:70`)
- [x] ✅ Namespace NVS `etch`, key `cfg` (preset), `kal` (kalibrasi level), `pompa` (rpm + ml/rotasi)
- [x] ✅ Simpan kalibrasi pompa (rpm + ml/rotasi) ke NVS + CRC (`storage.h`)
- [ ] ⬜ Simpan offset suhu & acuan MQ-135 ke NVS
- [ ] ⬜ Simpan duty/kecepatan motor per kalibrasi ke NVS
- [ ] ⬜ Backup/restore preset via Serial (export/import)

---

## 9. Infrastruktur Firmware

- [x] ✅ Semua modul non-blocking (`loop()` berbasis `millis()`), tanpa `delay()` di jalur proses
- [x] ✅ Pisah logika proses (`prosesUpdate()`, `motorUpdate()`, `sensorUpdate()`, `buzzerUpdate()`) dari render UI (`.ino:588-590`)
- [x] ✅ Log debug lengkap via `dbg.h` dengan prefix per modul (`[PROSES]`, `[POMPA]`, `[KALIB]`, `[STORE]`, `[SENSOR]`)
- [x] ✅ Struktur file modular (`config/input/data/presetops/sensor/storage/motor/dcmotor/buzzer/process/ui`)
- [x] ✅ Folder `test/` untuk sketch uji mandiri per modul (`test/tb6612fng/`)
- [x] ✅ Backup versi di folder `backup/` per rilis
- [ ] ⬜ Watchdog timer hardware (`esp_task_wdt`) sebagai jaring pengaman
- [ ] ⬜ Deteksi & pemulihan I2C (PCF8574 hang → reset bus)
- [ ] ⬜ Kontrol via Serial (start/stop/preset/status) di firmware utama — **hanya ada di sketch uji**
- [ ] ⬜ Logging proses ke file / upload via WiFi (opsional)
- [ ] ⬜ Cleaning `#define` mati di `config.h`: `INTERVAL_SENSOR_MS`, `SENSOR_AVG_N`, `SENSOR_AKTIF_LOW`, `SENSOR_TOUCH_READ`, `TOUCH_DEBOUNCE`, `TOUCH_DELTA_*`, `TOUTH_KALIB_SAMPEL`, `SENSOR_LOG_PERIOD_MS`, `AMBANG_MARGIN`, `AMBANG_HYST` (`config.h:59-73`)

---

## 10. Dokumentasi

- [x] ✅ `README.md`, `GUIDE.md`, `SUMMARY.md`, `LOG.md`
- [x] ✅ `etching-otomatis.md` — rencana pengembangan + pin map awal
- [x] ✅ `water-level-sensor-plan.md` — rencana sensor level non-contact
- [x] ✅ `PROCESS.md` — rencana detail 5 modul proses + cara kerja + tabel GPIO + pin map DRV8825 1/32
- [x] ✅ `PROGRESS.md` — checklist status implementasi (dokumen ini)
- [x] ✅ **v0.5.1: `SUMMARY.md` diperbarui** — fan dilepas, GPIO 18 jadi bebas, versi naik
- [x] ✅ **v0.5.1: `PROCESS.md` diperbarui** — §1.4 tabel GPIO + catatan perubahan v0.5.1 + `FAN_SELAMA_ETCHING` dihapus
- [x] ✅ **v0.5.1: header changelog `.ino`** didokumentasikan
- [x] ✅ `LOG.md` — **entri v0.5.1 ditulis** (2026-10-16) + header catatan lineage + 17 entri lama ditandai `❌ DIBATALKAN`
- [x] ✅ `README.md` — **diperbaiki** (sebelumnya usang: menyebut Arduino Uno/Nano, `aktuator.h`, EEPROM, RTC) + bagian **Releases** dengan tabel versi
- [x] ✅ **`RELEASES.md` — dibuat**: registri versi 2 lineage + git tag `v0.5.0`/`v0.5.1` + panduan rollback (5 skenario) + cara flash + aturan maintainer
- [x] ✅ `etching-otomatis.md` — **dianotasi "DOKUMEN USANG"** dengan tabel pin map lama vs baru

---

## 11. Uji Hardware yang Masih Diperlukan

- [ ] ⬜ **Upload & uji `test/tb6612fng/tb6612fng.ino`** (belum pernah dijalankan di hardware)
- [ ] ⬜ Uji motor A & B via sketch: duty 60/128/255, cari duty minimum motor
- [ ] ⬜ Verifikasi fan 12V benar-benar menyala terus & tidak drop tegangan
- [ ] ⬜ Uji 5 sensor touch di jerican kosong vs terisi (repeatability < 5%)
- [ ] ⬜ Verifikasi address DS18B20 fisik cocok dengan `sensor.h:119-123`
- [ ] ⬜ Timbang air: 3 pompa × 3 ulangan → error ≤ 5% dari target
- [ ] ⬜ Kalibrasi DC sabun/limbah 10 detik × 3 ulangan → ml/detik konsisten
- [ ] ⬜ Uji ramp agitasi & pompa DC (tidak ada lonjakan duty saat start/stop)
- [ ] ⬜ **Upload & uji `test/stepper-rpm/stepper-rpm.ino`** — verifikasi 400/500/600/700 rpm tanpa step-loss
- [ ] ⬜ Uji driver DRV8825 3 channel (SLP/STEP/DIR) dengan pin map final
- [ ] ⬜ Kalibrasi 10 rotasi tiap pompa pada RPM operasi → catat ml/rotasi, cek konsistensi 3 ulangan
- [ ] ⬜ Uji respons E-stop (target < 5 ms dari LOW ke semua output mati)
- [ ] ⬜ Uji drain: volume larutan terbuang habis dalam `T_DRAINING_MS`
- [ ] ⬜ Uji overfill guard (isi tangki limbah hampir penuh)
- [ ] ⬜ Uji siklus agitasi (ON/OFF berulang) — fan sudah nyala otomatis
- [ ] ⬜ Uji stabilitas level readout saat kabel AWG30 30 cm
- [ ] ⬜ **Seluruh uji wajib pakai air biasa dulu**, baru larutan encer

---

## Ringkasan

| Kategori | ✅ | ⚠️ | ⬜ |
|---|---|---|---|
| 0. Status Lineage Versi | 6 | 0 | 0 |
| 1. GPIO & Hardware | 13 | 0 | 6 |
| 2. Modul UI | 16 | 0 | 12 |
| 3. Modul Dosing | 8 | 2 | 10 |
| 4. Modul Etching | 6 | 1 | 8 |
| 5. Modul Draining & Rinsing | 5 | 2 | 6 |
| 6. Modul Emergency Stop | 8 | 0 | 12 |
| 7. Sensor & Kalibrasi | 14 | 5 | 3 |
| 8. Penyimpanan & Data | 8 | 0 | 4 |
| 9. Infrastruktur | 6 | 0 | 5 |
| 10. Dokumentasi | 12 | 0 | 0 |
| 11. Uji Hardware | 0 | 0 | 13 |

**Total: 102 ✅ · 10 ⚠️ · 79 ⬜**

Status keseluruhan: **prototype berfungsi pada jalur happy-path** (menu, preset, dosing
paralel, agitasi, drain, e-stop dasar, kalibrasi 4 sensor, penyimpanan NVS).
Belum ada **proteksi proses** (watchdog, validasi level, latch e-stop, proteksi
suhu/gas) dan **akurasi dosing belum memakai hasil kalibrasi** — dua hal ini
prioritas utama berikutnya (lihat `PROCESS.md` §11).

---

*Dokumen ini diperbarui setiap kali ada fitur baru yang selesai. Referensi `file:line`
diverifikasi terhadap kode di branch `main` pada 2026-10-16.*