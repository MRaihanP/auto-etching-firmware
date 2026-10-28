# Auto Etching Firmware

Firmware untuk **mesin etching PCB otomatis** berbasis **ESP32-S3 DevKitC**.
Dosing HCl/H₂O₂/H₂O (stepper peristaltik), agitasi, draining, rinsing, dan rinsing
dengan air sabun — dikendalikan lewat **OLED 128×64 + 6 tombol** (PCF8574).

> **Lineage repo:** versi yang berjalan berasal dari lineage modular
> (`auto-etching-firmware.ino` + `dcmotor.h`/`buzzer.h`/`motor.h`).
> Lineage lama (`aktuator.h`, v0.5.0–v0.8.3) **rusak & dibatalkan**, snapshotnya
> ada di `backup/`. Penjelasan lengkap di bagian atas [`LOG.md`](./LOG.md).

## Fitur Utama

- **Menu OLED 6 tombol** — preset, konfigurasi, level cairan, kalibrasi
- **Preset CRUD (maks 10)** — tersimpan di NVS dengan versi + CRC32, auto-reject bila korup
- **Proses etching 5 fase non-blocking** — `DOSING → ETCHING → DRAINING → RINSING → SELESAI`
- **Dosing 3 pompa peristaltik paralel** (stepper, ml → jumlah langkah, non-blocking)
- **3 aktuator DC via TB6612FNG PWM** — motor agitasi, pompa air sabun, pompa limbah
- **5 sensor level non-contact** (strip nikel + `touchRead`, moving average)
- **Sensor suhu DS18B20 ×3** pada satu bus 1-Wire, konversi non-blocking
- **Sensor udara MQ-135** dengan divider analog & konversi ppm
- **Kalibrasi** — stepper (ml/rotasi), DC (duty), agitasi, sensor level 0%/100%, suhu (offset), udara (acuan)
- **E-stop** GPIO 8 — hentikan semua aktuator & batalkan proses
- **Buzzer non-blocking** — nada boot, klik, mulai, batal, selesai
- **Exhaust fan** disambung langsung ke 12V, menyala terus (tidak dikendalikan firmware)

## Struktur Project

| File | Deskripsi |
|------|-----------|
| `auto-etching-firmware.ino` | Setup/loop utama, navigasi tombol, helper konfigurasi |
| `config.h` | Pin map, konstanta global, `FW_VERSI` |
| `dbg.h` | Helper log debug (`[PROSES]`, `[POMPA]`, …) |
| `input.h` | Pembacaan 6 tombol PCF8574 + debounce |
| `data.h` | Struct `Preset`, enum halaman, state global |
| `presetops.h` | CRUD preset, kalkulasi ml, validasi |
| `sensor.h` | Sensor level touch, DS18B20, MQ-135 |
| `storage.h` | NVS (Preferences) + checksum untuk preset & kalibrasi |
| `motor.h` | Driver stepper pompa peristaltik (non-blocking) |
| `dcmotor.h` | Driver TB6612FNG (agitasi, pompa sabun, pompa limbah) |
| `buzzer.h` | Buzzer pasif non-blocking (LEDC) + daftar melodi |
| `process.h` | State machine proses etching |
| `ui.h` | Rendering OLED: semua halaman, dialog, toast, animasi |
| `fontmini.h` | Data font mini untuk OLED |
| `test/` | Sketch uji mandiri per modul (mis. `test/tb6612fng/`) |
| `backup/` | Snapshot versi lama (**lineage batal** — jangan di-flash) |

## Releases — Daftar Versi

Registri versi lengkap + panduan rollback ada di **[`RELEASES.md`](./RELEASES.md)**.

| Versi | Tanggal | Git tag | Ringkasan |
|---|---|---|---|
| **v0.5.1** | 2026-10-16 | `v0.5.1` | Exhaust fan dilepas dari TB6612FNG → langsung 12V; GPIO 18 bebas; sketch uji `test/tb6612fng/`; `PROCESS.md` + `PROGRESS.md` |
| **v0.5.0** | 2026-10-13/14 | `v0.5.0` | Baseline lineage modular: menu, preset, dosing, sensor, kalibrasi, E-stop |

Versi lineage lama (v0.5.0–v0.8.3, arsitektur `aktuator.h`) **dibatalkan** — ada di `backup/`.

Rollback cepat:

```bash
git tag -l                              # lihat semua versi
git checkout v0.5.0                     # kembali ke versi lama (aman, tidak hapus file)
git checkout main                        # kembali ke versi terbaru
```

## Dokumentasi Lain

| Dokumen | Isi |
|---|---|
| [`SUMMARY.md`](./SUMMARY.md) | Ringkasan firmware, fitur, pin map |
| [`PROCESS.md`](./PROCESS.md) | Rencana detail 5 modul proses (UI, Dosing, Etching, Draining & Rinsing, Emergency Stop) + cara kerja tiap fase + tabel GPIO |
| [`PROGRESS.md`](./PROGRESS.md) | Checklist status implementasi per fitur |
| [`RELEASES.md`](./RELEASES.md) | Registri versi & panduan rollback |
| [`LOG.md`](./LOG.md) | Changelog tiap versi |
| [`GUIDE.md`](./GUIDE.md) | Panduan pemilik repo & kontributor |
| `etching-otomatis.md` | Rencana awal (**usang** — sudah dianotasi) |
| `water-level-sensor-plan.md` | Rencana sensor level non-contact |

## Flash ke ESP32-S3

| Setting | Nilai |
|---|---|
| Board | **ESP32S3 Dev Module** |
| Upload speed | 921600 (turunkan ke 115200 bila gagal) |
| Port | `/dev/ttyACM0` atau `/dev/ttyUSB0` |

> Data preset & kalibrasi ada di NVS-flash, **tidak hilang** saat flash ulang.
> Menghapus flash (`ESP32: Erase Flash`) hanya bila preset korup/berubah sendiri.

## License

Property of PCB Express. All rights reserved.