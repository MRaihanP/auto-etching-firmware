# Auto Etching Firmware

Firmware untuk alat etching PCB otomatis berbasis Arduino. Menggunakan Arduino Uno/Nano dengan tambahan modul RTC, stepper motor, dan OLED display.

## Fitur Utama

- **Proses etching otomatis** — timer, agitasi, dan suhu terkontrol
- **OLED UI** — menampilkan status proses, timer, dan suhu
- **Non-blocking buzzer** — feedback audio tanpa memblokir proses
- **E-Stop** — emergency stop dengan ISR
- **Preset operasi** — beberapa profil etching yang bisa dipilih
- **EEPROM storage** — menyimpan konfigurasi dan terakhir digunakan

## Struktur Project

| File | Deskripsi |
|------|-----------|
| `auto-etching-firmware.ino` | File utama firmware (setup & loop) |
| `config.h` | Konfigurasi pin, konstanta, dan version |
| `process.h` | State machine proses etching |
| `sensor.h` | Pembacaan sensor (suhu, arus) |
| `input.h` | Penanganan input (tombol, encoder) |
| `ui.h` | Tampilan OLED dan UI |
| `storage.h` | Penyimpanan EEPROM |
| `aktuator.h` | Kontrol aktuator (motor, pump, buzzer) |
| `presetops.h` | Preset operasi etching |

## Panduan

Lihat [`GUIDE.md`](./GUIDE.md) untuk panduan lengkap:
- **Bagian A** — Panduan untuk Pemilik Repository
- **Bagian B** — Panduan untuk Contributor

## Changelog

Lihat [`LOG.md`](./LOG.md) untuk riwayat perubahan versi.

## License

Property of PCB Express. All rights reserved.
