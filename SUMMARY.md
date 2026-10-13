# SUMMARY — Mesin PCB Etching Otomatis

Firmware **Etching v0.5.0** untuk **ESP32-S3**: alat etching PCB otomatis dengan dosing
HCl/H2O2/H2O (stepper DRV8825), agitasi, draining, dan rinsing, dikendalikan lewat layar
OLED 128x64 + 6 tombol (PCF8574).

## Fitur Utama

- **Menu utama (3 item):** Pilih Preset, Konfigurasi Preset, Level Cairan
- **Preset (maks 10):** nama, ukuran PCB, mode dosing auto/manual, volume, agitasi, total durasi.
  CRUD lengkap: tambah, edit, duplikat, hapus.
- **Proses otomatis 5 fase** (non-blocking): `DOSING → ETCHING → DRAINING → RINSING → SELESAI`.
- **Sensor level GPIO langsung (5):** HCl/H2O2/H2O/sabun/limbah, rata-rata 4 sampel + histeresis.
- **Dosing stepper non-blocking:** 3 pompa peristaltic (DRV8825), konversi ml -> step.
- **Aktuator DC (PWM LEDC):** motor agitasi, pompa air sabun, pompa limbah, exhaust fan.
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
| `motor.h` | Stepper DRV8825 non-blocking (pompa peristaltik) |
| `dcmotor.h` | DC motor via TB6612FNG (agitasi, pompa sabun, pompa limbah, fan) |
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
| Stepper HCl (SLP/STEP/DIR) | 1/2/42 | DRV8825 |
| Stepper H2O2 | 41/40/39 | |
| Stepper H2O | 38/47/21 | |
| PWM agitasi | 15 | TB6612FNG Driver1 PWA |
| PWM pompa sabun | 16 | TB6612FNG Driver1 PWB |
| PWM pompa limbah | 17 | TB6612FNG Driver2 PWA |
| PWM fan | 18 | TB6612FNG Driver2 PWB |
| Buzzer | 6 | pasif, LEDC |
| LED strip | 7 | |
| E-stop | 8 | INPUT_PULLUP |
| MQ-135 | 9 | analog (placeholder) |
| DS18B20 | 10 | 1-Wire (placeholder) |

## Riwayat Versi

Lihat `LOG.md` untuk detail perubahan per versi.
