# SUMMARY — Mesin PCB Etching Otomatis

Firmware **Etching v0.8.3** untuk **ESP32-S3**: alat etching PCB otomatis dengan dosing
HCl/H2O2/H2O akurat (stepper DRV8825), agitasi, draining, dan rinsing, dikendalikan
lewat layar OLED 128×64 + 6 tombol (PCF8574).

## Fitur Utama

- **Menu utama (5 item):** Pilih Preset, Buat Preset, Cadangan Senyawa, Kalibrasi, Tentang
- **Preset (maks 10):** nama, ukuran PCB (20–300 mm), mode dosing auto (47/24/29 %) atau
  manual, volume 50–1000 ml, mode agitasi auto/manual, speed agitasi 1–5, total durasi.
  CRUD lengkap: tambah, edit, duplikat, hapus.
- **Proses otomatis 16 fase** (non-blocking): `CEK_LEVEL → DOSING HCl → DOSING H2O2 →
  DOSING H2O → JEDA_CAMPUR → ETCHING → DRAIN_ETCH → DOSING_SABUN → BILAS_SABUN →
  DRAIN_SABUN → DOSING_AIR → BILAS_AIR → DRAIN_AIR → PENDINGINAN → SELESAI`.
- **Sensor level GPIO langsung (5):** HCl/H2O2/H2O/sabun/limbah — analog ADC 12-bit
  atau float switch digital, rata-rata 4 sampel + histeresis.
- **Dosing stepper non-blocking:** 3 pompa peristaltik (DRV8825), konversi ml → step
  lewat `mlPerStep` hasil kalibrasi, 625 step/s, bisa jeda/resume.
- **Aktuator DC (PWM LEDC 25 kHz):** motor agitasi, pompa air sabun, pompa limbah, exhaust fan.
- **Kalibrasi (5 menu):** sensor level 5 titik, stepper ml/step, MQ-135 baseline,
  DS18B20 offset, agitasi duty min.
- **Penyimpanan NVS:** preset & kalibrasi via `Preferences` dengan hash CRC.
- **Keamanan:** E-stop (interrupt), abort proses, semua aktuator mati saat error/batal/selesai.
- **UI:** splash 1.5 s, header, list dengan animasi ease-out, scrollbar, toast 1.2 s,
  dialog, halaman proses dengan progress bar.

## Tone Buzzer

| Tone | Keterangan | Pola |
|------|-----------|------|
| `TONE_TOMBOL` | Tombol ditekan | 1200 Hz, 25 ms |
| `TONE_SELESAi` | Proses selesai (existing) | 523→659→784→1047 Hz |
| `TONE_ERROR` | Error/alarm | 392→330→262 Hz ×3 |
| `TONE_PERINGATAN` | Peringatan | 880 Hz ×2 |
| `TONE_BOOT` | Boot/startup | 659→784→1047 Hz |
| **`TONE_MULAI`** | **Mulai proses (BARU)** | 440→660→880 Hz |
| **`TONE_PROSES_SELESAI`** | **Selesai proses (BARU)** | 523→784→1047 Hz |

Semua tone bersifat **non-blocking** (state machine berbasis `millis()`).

## Pin Map (v0.8.3)

| Fungsi | GPIO | Catatan |
|---|---|---|
| I2C SDA / SCL | 4 / 5 | 400 kHz |
| OLED SSD1306 | 0x3C | |
| PCF8574 (tombol) | 0x20 | |
| Sensor level | 11/12/13/14/3 | analog atau digital |
| Stepper HCl (SLP/STEP/DIR) | 1/2/42 | DRV8825, microstep 16 |
| Stepper H2O2 | 41/40/39 | |
| Stepper H2O | 38/47/21 | |
| PWM agitasi | 15 | TB6612FNG Driver1 PWA |
| PWM pompa sabun | 16 | TB6612FNG Driver1 PWB |
| PWM pompa limbah | 17 | TB6612FNG Driver2 PWA |
| PWM fan | 18 | TB6612FNG Driver2 PWB |
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
| `sensor.h` | Pembacaan 5 sensor GPIO, interpolasi 5 titik → ml |
| `storage.h` | Preferences (NVS), CRC, simpan/muat preset & kalibrasi |
| `aktuator.h` | Buzzer non-blocking, fan, agitasi, pompa DC, stepper, E-stop |
| `process.h` | Mesin status 16 fase, jeda/batal/error, estimasi durasi |
| `ui.h` | Rendering OLED: semua halaman, dialog, toast, animasi list |

## Riwayat Versi

Lihat `LOG.md` untuk detail perubahan per versi.
