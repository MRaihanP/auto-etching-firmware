# Etching — Firmware Mesin Etsa PCB

Firmware berbasis ESP32-S3 dengan UI OLED SSD1306 dan 6 tombol untuk mengontrol
mesin etsa PCB: manajemen preset resep pelarut, monitoring level cairan, dan
state machine proses etsa.

**Versi firmware:** v0.4.1

## Struktur File

| File | Fungsi |
|------|--------|
| `Etching.ino` | Titik masuk: `setup()`, `loop()`, navigasi tombol, throttling render |
| `config.h` | Pin map & konstanta hardware (I2C, OLED, MPR121, PCF8574), batas volume, versi |
| `data.h` | Struct `Preset`, 5 preset default, menu utama, enum halaman & state global |
| `input.h` | Baca 6 tombol via PCF8574 I2C, debounce 40 ms |
| `sensor.h` | Baca 5 sensor level cairan capacitive MPR121, rata-rata bergerak, histeresis |
| `presetops.h` | Operasi preset: tambah, duplikat, hapus, edit field, validasi |
| `storage.h` | Simpan/muat preset & ambang sensor ke NVS (Preferences), blob + versi + CRC |
| `process.h` | State machine proses (DOSING → ETCHING → DRAINING → RINSING → SELESAI) |
| `ui.h` | Gambar halaman OLED, toast, animasi list, scroll |

## Fitur Utama

- **Menu utama**: Pilih Preset, Konfigurasi Preset, Level Cairan, Kalibrasi (placeholder)
- **Preset**: maks 10 preset, 5 default. Field: ukuran PCB, %HCl/%H2O2/%H2O,
  volume ml, durasi & kecepatan agitasi, estimasi waktu. %H2O dan semua ml
  dihitung otomatis dari %HCl, %H2O2, dan volume.
- **Proses**: fase DOSING → ETCHING → DRAINING → RINSING → SELESAI dengan
  dukungan jeda, konfirmasi batal, dan timer akumulatif. Logika proses terpisah
  dari render sehingga UI tetap responsif.
- **Level Cairan**: 5 sensor (HCl, H2O2, H2O, Sabun, Limbah) via MPR121 @ 0x5A.
  Status ISI/KOSONG berdasarkan ambang + histeresis; ambang dapat diset dari
  nilai sekarang dan disimpan ke NVS.
- **Penyimpanan**: NVS (Preferences) dengan blob tunggal, versi format, dan
  checksum FNV-1a → data korup/versi berbeda otomatis kembali ke default.

## Hardware

- MCU: ESP32-S3
- Bus I2C bersama (SDA=4, SCL=5, 400 kHz):
  - OLED SSD1306 @ 0x3C (128×64)
  - PCF8574 (6 tombol) @ 0x20
  - MPR121 (sensor level kapasitif) @ 0x5A
- Pin yang sudah dipetakan (belum semua dipakai):
  - DRV8825 ×3: stepper pompa peristaltik (HCl, H2O2, H2O)
  - Buzzer (6), LED strip (7), DS18B20 suhu (10), MQ135 (9), Fan (14), E-stop (8)
  - TB6612FNG ×2: pin belum ditentukan

## Status Proyek

- Proses masih **simulasi** (fase hanya label + log Serial; aktuator belum ada)
- Menu "Kalibrasi" masih placeholder
- Kontrol aktuator (pompa, agitasi, fan) direncanakan di Step 5/6

## Library

Wire, Adafruit_GFX, Adafruit_SSD1306, Preferences (ESP32)
