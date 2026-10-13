# SUMMARY — Mesin PCB Etching Otomatis

Firmware **Etching v0.5.1** untuk **ESP32-S3**: alat etching PCB otomatis dengan dosing
HCl/H2O2/H2O akurat (stepper DRV8825), agitasi, draining, dan rinsing, dikendalikan
lewat layar OLED 128×64 + 6 tombol (PCF8574).

## Fitur Utama

- **Menu utama (5 item):** Pilih Preset, Buat Preset, Cadangan Senyawa, Kalibrasi, Tentang
- **Preset (maks 10):** nama, ukuran PCB (20–300 mm), mode dosing auto (47/24/29 %) atau
  manual, volume 50–1000 ml, mode agitasi auto (30 + vol/10 detik) atau manual (1–180 s),
  speed agitasi 1–5, total durasi (30–3600 s). CRUD lengkap: tambah, edit, duplikat, hapus.
- **Proses otomatis 16 fase** (non-blocking, terpisah dari render UI):
  `CEK_LEVEL → DOSING HCl → DOSING H2O2 → DOSING H2O → JEDA_CAMPUR → ETCHING →
  DRAIN_ETCH → DOSING_SABUN → BILAS_SABUN → DRAIN_SABUN → DOSING_AIR → BILAS_AIR →
  DRAIN_AIR → PENDINGINAN → SELESAI`. Dosing divalidasi (sumber habis → error
  `SUMBER_HABIS`), ada watchdog timeout per fase, dan drain berhenti lebih
  awal bila tangki limbah penuh.
- **Sensor level GPIO langsung (5):** HCl/H2O2/H2O/sabun/limbah
  (GPIO 11/12/13/14/3) — analog ADC 12-bit (mode default) atau float
  switch digital (`SENSOR_ANALOG=0`), rata-rata 4 sampel + histeresis;
  deteksi tangki kosong/penuh (`sensorKosong()`/`sensorPenuh()`)
- **Dosing stepper non-blocking:** 3 pompa peristaltik (DRV8825), konversi ml → step
  lewat `mlPerStep` hasil kalibrasi, 625 step/s (1,6 ms/step), bisa jeda/resume.
- **Aktuator DC (PWM LEDC 25 kHz):** motor agitasi (speed 1–5 → duty 50–255), pompa
  air sabun, pompa limbah, exhaust fan.
- **Validasi bahan sebelum proses:** cek ketersediaan HCl/H2O2/H2O/sabun di sumber dan
  ruang sisa tangki limbah (estimasi limbah = mlHCl + mlH2O2 + 2×mlH2O).
- **Kalibrasi (5 menu):** sensor level dengan baris **Kapasitas** (KIRI/KANAN
  ±50ml) + 5 titik 0/25/50/75/100 % (ATAS/BAWAH pilih baris, ENTER = set
  titik dari raw saat ini; senyawa ditandai `*` bila sudah terkalibrasi,
  otomatis aktif saat titik 100% terisi); stepper ml/step lewat wizard
  ENTER (pompa N step → masukkan volume terukur → `ml/step` dihitung &
  disimpan otomatis); MQ-135 baseline + margin; DS18B20 offset;
  agitasi duty min.
- **Penyimpanan NVS:** preset & kalibrasi disimpan via `Preferences` dengan hash CRC
  (versi format 2); data korup/invalid otomatis reset ke default.
- **Keamanan:** E-stop (GPIO8, interrupt, terkunci hingga dilepas + reset), abort proses,
  semua aktuator mati saat error/batal/selesai, fan menyala saat error, buzzer peringatan.
- **UI:** splash 1,5 s, header, list dengan animasi ease-out, scrollbar, toast 1,2 s,
  dialog (simpan/hapus/batal), halaman proses (waktu lewat, sisa, progress bar,
  nama fase, %, ml terpompa/target saat dosing).

## Cara Kerja

**Offset tampilan:** 14 baris teratas OLED dipakai `OLED_TOP_OFF` (rusak),
jadi seluruh UI digeser ke bawah — area logo efektif y=14–63 (50px).
Semua koordinat di `ui.h` bersifat logis (`TY()` dan primitif teks
menambahkan offset otomatis); jarak baris standar `BARIS_H` = 10px
(font size 1, jeda 3px antar baris) — 4 baris per halaman, sisanya
digulir.

**Scroll:** halaman yang isinya melebihi area tampil digulir dengan
ATAS/BAWAH (Cadangan, Kalib MQ-135 / DS18B20 / Agitasi, Tentang) dengan
scrollbar tipis di kanan. Kalib Sensor memakai auto-scroll mengikuti
titik terpilih (ATAS/BAWAH tetap memilih titik). Di Cadangan,
KIRI/KANAN memilih baris. Scroll di-reset otomatis saat pindah halaman.

**Navigasi:** 6 tombol (ATAS/BAWAH/KIRI/KANAN/ENTER/BACK) dibaca dari PCF8574 dengan
debounce 40 ms; hanya satu tombol yang diproses per siklus (tombol aktif).

**Loop utama (`loop()`):**
1. Baca tombol baru → tangani per halaman (`tanganiTombol`)
2. `stepperUpdateAll()` — update stepper non-blocking
3. `prosesUpdate()` — mesin status proses (fase berpindah berdasarkan selesainya stepper
   atau timeout durasi fase)
4. `sensorUpdate()` — baca 5 sensor GPIO tiap 50 ms (analog 12-bit atau
   digital), rata-rata 4 sampel + histeresis; `sensorKosong()` / `sensorPenuh()`
   dipakai oleh proses (dosing & drain)
5. Update animasi list & toast
6. Render ke OLED tiap 20 ms bila ada perubahan (`perluGambar`)

**Alur proses:** `prosesMulai()` (dari layar detail preset) menghitung
`prosesTotalMsAktual` — durasi tiap fase, dengan fase dosing dihitung dari
ml dan `mlPerStep` hasil kalibrasi (`dosisDurasiMs()`). Estimasi inilah
yang dipakai sebagai penyebut bar progres dan sisa waktu di layar proses,
sehingga bar selalu mencapai 100% saat proses selesai.
Fase dosing menunggu stepper selesai (`stepperAktif()`) dengan watchdog
timeout (estimasi 2× durasi langkah + 30 s) dan berhenti dengan error bila
sumber habis (`sensorKosong()`); fase drain berakhir saat timeout atau
limbah penuh (`sensorPenuh()`). `prosesToggleJeda()` menghentikan agitasi
dan mengSleep stepper; `prosesBatal()` dan E-stop mematikan semua aktuator.

**Konfigurasi sensor level** (`config.h`):
- `SENSOR_ANALOG=1` (default) → sensor analog, `analogRead` skala 0–4095
- `SENSOR_ANALOG=0` → float switch digital, `SENSOR_ACTIVE_LOW=1` berarti
  sensor menghasilkan LOW saat tangki penuh (float switch NO + pull-up)
- `SENSOR_HYST=30` → histeresis deteksi kosong/penuh (count ADC)

**Kalibrasi:** nilai disimpan ke NVS (`PREF_CALIB`) saat keluar halaman kalibrasi lewat
dialog "Simpan perubahan?". `stepperMlPerStepX1000` (x1000 untuk presisi desimal)
dimuat saat boot dan dipakai `stepperInit()`. Halaman kalibrasi sensor level:
KIRI/KANAN mengganti senyawa (HCl/H2O2/H2O/Sabun/Limbah), ATAS/BAWAH menggulir
5 titik, ENTER menyimpan nilai raw saat ini ke titik terpilih.

## Pin Map (kondisi v0.5.1)

| Fungsi | GPIO | Catatan |
|---|---|---|
| I2C SDA / SCL | 4 / 5 | 400 kHz |
| OLED SSD1306 | 0x3C | |
| PCF8574 (tombol) | 0x20 | |
| Sensor level HCl/H2O2/H2O/Sabun/Limbah | 11 / 12 / 13 / 14 / 3 | analog (default) atau digital |
| Stepper HCl (SLP/STEP/DIR) | 1 / 2 / 42 | DRV8825, mikrostep 16 (MS hardwired) |
| Stepper H2O2 | 41 / 40 / 39 | |
| Stepper H2O | 38 / 47 / 21 | |
| PWM agitasi | 15 | TB6612FNG Driver1 PWA |
| PWM pompa sabun | 16 | TB6612FNG Driver1 PWB |
| PWM pompa limbah | 17 | TB6612FNG Driver2 PWA |
| PWM fan | 18 | TB6612FNG Driver2 PWB |
| TB6612FNG STBY | — | di-tie 3.3V di hardware |
| Buzzer | 6 | pasif, LEDC |
| LED strip | 7 | |
| E-stop | 8 | INPUT_PULLUP, interrupt |
| MQ-135 | 9 | analog |
| DS18B20 | 10 | placeholder |

## Struktur File

| File | Isi |
|---|---|
| `oled_pcb_baruuu.ino` | setup/loop, penanganan tombol per halaman |
| `config.h` | Pin, konstanta global, alamat I2C |
| `input.h` | Debounce & pembacaan 6 tombol via PCF8574 |
| `data.h` | Struct Preset/Kalibrasi, menu, enum halaman, state global |
| `presetops.h` | CRUD preset, kalkulasi ml, validasi |
| `sensor.h` | MPR121, interpolasi 5 titik → ml, cek ketersediaan |
| `storage.h` | Preferences (NVS), CRC, simpan/muat preset & kalibrasi |
| `aktuator.h` | Buzzer, fan, agitasi, pompa DC, stepper non-blocking, E-stop, MQ-135, DS18B20 |
| `process.h` | Mesin status 16 fase, jeda/batal/error, estimasi durasi |
| `ui.h` | Rendering OLED: semua halaman, dialog, toast, animasi list |

**Dokumen acuan:** `etching-otomatis.md` (rencana pengembangan), lihat `LOG.md` untuk
riwayat perubahan dan daftar gap terhadap dokumen.
