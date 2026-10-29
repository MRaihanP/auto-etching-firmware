# SUMMARY — Mesin PCB Etching Otomatis

Firmware **Etching v0.6.0** untuk **ESP32-S3**: alat etching PCB otomatis dengan dosing
HCl/H2O2/H2O (stepper DRV8825 1/32, 400–700 rpm), agitasi, draining, dan rinsing, dikendalikan lewat layar
OLED 128x64 + 6 tombol (PCF8574).

> **Catatan lineage:** repo ini punya dua lineage firmware. Lineage lama
> (`aktuator.h` + `oled_pcb_baruuu.ino`, versi v0.5.0–v0.8.3) **rusak dan dibatalkan**;
> snapshot-nya tersimpan di `backup/`. Lineage aktif (dokumen ini) =
> `auto-etching-firmware.ino` + `dcmotor.h`/`buzzer.h`/`motor.h` di branch `main`.
> Detailnya di bagian atas `LOG.md`.

## Fitur Utama

- **Menu utama (3 item):** Pilih Preset, Konfigurasi Preset, Level Cairan
- **Preset (maks 10):** nama, ukuran PCB, mode dosing auto/manual, volume, agitasi, total durasi.
  CRUD lengkap: tambah, edit, duplikat, hapus.
- **Proses otomatis 5 fase** (non-blocking): `DOSING → ETCHING → DRAINING → RINSING → SELESAI`.
- **Sensor level GPIO langsung (5):** HCl/H2O2/H2O/sabun/limbah, rata-rata 4 sampel + histeresis.
- **Dosing stepper non-blocking:** 3 pompa peristaltik (DRV8825 1/32), kecepatan
  **400–700 rpm** (default 500), konversi ml → langkah dari kalibrasi ml/rotasi.
  Pulsa dibangkitkan FastAccelStepper (MCPWM/PCNT), bukan `loop()`.
  Start/stop memakai **ramp** (akselerasi 50.000 langkah/s²): naik ke RPM target
  ±1 detik dan turun bertahap saat dihentikan. E-stop/timeout tetap potong seketika.
- **Kalibrasi pompa:** uji **10 rotasi** pada RPM terpilih; hasil ml/rotasi disimpan
  di NVS (key `pompa`). Ramp naik/turun dihitung stepper, jadi jumlah langkah
  tetap tepat 10 rotasi.
- **Kalibrasi pompa DC:** uji **10 detik** pada duty penuh, hasil **ml/detik**
  disimpan di NVS; dosing berbasis waktu diberi margin 10%.
- **Agitasi 3 level:** SPD1/SPD2/SPD3 dalam %, tanpa kalibrasi ml; tiap level
  selalu ramp dari 0%.
- **Test Motor:** halaman di Kalibrasi → Motor dengan **7 baris** (Semua / HCl /
  H2O2 / Air / Sabun / Limbah / Agitasi) dan scroll; KIRI/KANAN mengubah
  RPM/duty/SPD secara live.
- **Aktuator DC (PWM LEDC):** motor agitasi (3 level), pompa air sabun, pompa
  limbah. Semua start/stop memakai ramp duty non-blocking
  (`DC_RAMP_PCT_PER_SEC`), tidak pernah lompat 0 → 100%.
- **Exhaust fan:** disambung **langsung ke 12V** dan menyala terus (arus kipas
  melebihi kapasitas TB6612FNG ~1.2 A). GPIO 18 kini bebas.
- **Kalibrasi level cairan:** ambang tersimpan di NVS (Preferences).
- **Penyimpanan NVS:** preset via `Preferences` dengan checksum.
- **Keamanan:** E-stop (GPIO), abort proses, semua aktuator mati saat batal/selesai.
- **UI:** header, list dengan animasi, toast, dialog, halaman proses dengan progress.

## Struktur File

| File | Isi |
|---|---|
| `auto-etching-firmware.ino` | setup/loop utama, navigasi menu, helper konfigurasi |
| `config.h` | Pin map, konstanta global, konfigurasi hardware |
| `input.h` | Debounce & pembacaan 6 tombol via PCF8574 |
| `data.h` | Struct Preset, enum halaman, state global, list |
| `presetops.h` | CRUD preset, kalkulasi ml, validasi |
| `sensor.h` | Pembacaan 5 sensor GPIO, rata-rata, histeresis |
| `storage.h` | Preferences (NVS), checksum, simpan/muat preset & ambang |
| `motor.h` | Stepper DRV8825 1/32 via FastAccelStepper (pompa peristaltik, RPM) |
| `dcmotor.h` | DC motor via TB6612FNG (agitasi, pompa sabun, pompa limbah) |
| `test/tb6612fng/tb6612fng.ino` | Sketch uji mandiri modul TB6612FNG (Serial Monitor) |
| `buzzer.h` | Buzzer non-blocking (state machine berbasis millis) |
| `process.h` | State machine proses etching, jeda/batal/selesai |
| `ui.h` | Rendering OLED: semua halaman, dialog, toast, animasi |
| `fontmini.h` | Font data untuk OLED |
| `dbg.h` | Debug helper (serial print) |

## Pin Map (v0.5.0)

| Fungsi | GPIO | Catatan |
|---|---|---|
| I2C SDA / SCL | 4 / 5 | 400 kHz, bus bersama OLED + PCF8574 |
| OLED SSD1306 | 0x3C | |
| PCF8574 (tombol) | 0x20 | 6 tombol |
| Sensor level | 11/12/13/14/3 | HCl/H2O2/H2O/sabun/limbah |
| Stepper HCl (SLP/STEP/DIR) | 1/2/42 | DRV8825, 1/32 = 6400 step/rev |
| Stepper H2O2 | 41/40/39 | DRV8825, 1/32 = 6400 step/rev |
| Stepper H2O | 38/47/21 | DRV8825, 1/32 = 6400 step/rev |
| PWM agitasi | 15 | TB6612FNG Driver1 PWA |
| PWM pompa sabun | 16 | TB6612FNG Driver1 PWB |
| PWM pompa limbah | 17 | TB6612FNG Driver2 PWA |
| (bebas) | 18 | Driver2 PWB tidak dipakai — fan langsung 12V |
| Buzzer | 6 | pasif, LEDC |
| LED strip | 7 | |
| E-stop | 8 | INPUT_PULLUP |
| MQ-135 | 9 | analog (placeholder) |
| DS18B20 | 10 | 1-Wire (placeholder) |

## Riwayat Versi

Lihat `LOG.md` untuk detail perubahan per versi.
