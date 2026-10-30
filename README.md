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
- **Dosing 3 pompa peristaltik paralel** (DRV8825 1/16 = 3200 langkah/putaran, **400–700 rpm**, ml → langkah, non-blocking)
- **Kecepatan pompa berbasis RPM** — default 500 rpm, dapat diubah 400/500/600/700 per pompa
- **Kalibrasi pompa** — uji **target ml**: masukkan ml → START (dosing pakai kalibrasi sekarang, ramp sama seperti Test Motor) → ukur hasil nyata → isi **Hasil** → ENTER menghitung **offset** dan mengkalibrasi ulang ml/rotasi
- **Kalibrasi pompa DC** — uji **target ml** pada duty terpilih, hasil nyata diisi di baris Hasil → ENTER menghitung offset + ml/detik baru (margin dosing 10%)
- **Agitasi 3 level** — SPD1/SPD2/SPD3 dalam %, tanpa kalibrasi ml
- **3 aktuator DC via TB6612FNG PWM** — motor agitasi, pompa air sabun, pompa limbah
- **5 sensor level non-contact** (strip nikel + `touchRead`, moving average)
- **Sensor suhu DS18B20 ×3** pada satu bus 1-Wire, konversi non-blocking
- **Sensor udara MQ-135** dengan divider analog & konversi ppm
- **Kalibrasi** — stepper (target ml → offset → ml/rotasi), DC (target ml → offset → ml/detik), agitasi (SPD1–3), sensor level 0%/100%, suhu (offset), udara (acuan)
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
| `motor.h` | Driver stepper pompa peristaltik via FastAccelStepper (MCPWM/PCNT) |
| `dcmotor.h` | Driver TB6612FNG (agitasi, pompa sabun, pompa limbah) |
| `buzzer.h` | Buzzer pasif non-blocking (LEDC) + daftar melodi |
| `process.h` | State machine proses etching |
| `ui.h` | Rendering OLED: semua halaman, dialog, toast, animasi |
| `fontmini.h` | Data font mini untuk OLED |
| `test/` | Sketch uji mandiri per modul (`test/test-motor/`, `test/stepper-rpm/`, `test/tb6612fng/`) |
| `backup/` | Snapshot versi lama (**lineage batal** — jangan di-flash) |

## Releases — Daftar Versi

Registri versi lengkap + panduan rollback ada di **[`RELEASES.md`](./RELEASES.md)**.

| Versi | Tanggal | Git tag | Ringkasan |
|---|---|---|---|
| **v0.6.5** | 2026-10-30 | `v0.6.5` | **Kalibrasi stepper menghitung rotasi hanya saat RPM target** — ramp up/down tetap berjalan untuk keamanan pump head tetapi tidak dihitung; posisi awal dicatat saat kecepatan aktual ≥98% target, lalu langkah target dihitung dari titik itu dan dihentikan dengan ramp turun; progres 0% + label `RAMP...` selama ramp; timeout 60 s → `Gagal capai RPM`; `FW_VERSI`→v0.6.5 |
| **v0.6.4** | 2026-10-30 | `v0.6.4` | **Uji kalibrasi target ml per motor** — masukkan target ml, START (ramp sama seperti Test Motor), ukur hasil nyata, isi **Hasil**, ENTER menghitung **offset** + kalibrasi ulang (`cal = calLama × hasil / target`); halaman stepper & DC jadi 6 baris (START/STOP, RPM/Duty, Target, Hasil, Offset, ml/rotasi atau ml/detik); uji DC tanpa margin; `FW_VERSI`→v0.6.4 |
| **v0.6.3** | 2026-10-29 | `v0.6.3` | **Test Motor = sketch `test/test-motor`**: start memakai `applySpeedAcceleration()` seperti sketch, RPM live langsung dipakai ramp; baris HCl/H2O2/Air menampilkan **target/aktual** (mis. `400/400`) sehingga terlihat kecepatan berhenti di target; `st` di sketch uji juga mencetak RPM aktual; `FW_VERSI`→v0.6.3 |
| **v0.6.2** | 2026-10-29 | `v0.6.2` | **Step/rev 6400 → 3200 (DRV8825 1/16)** agar RPM sesuai target dan sama dengan sketch `test/test-motor`; akselerasi menjadi 1000 langkah/s² (= 18,75 rpm/detik); NVS `pompa` **v3** dengan migrasi otomatis `mlPerRev / 2`; `FW_VERSI`→v0.6.2 |
| **v0.6.1** | 2026-10-29 | `v0.6.1` | Ramp stepper disamakan dengan sketch `test/test-motor`: akselerasi **18,75 rpm/detik** (0→500 rpm ±26,7 s, 0→400 rpm ±21,3 s), dipakai juga saat deselerasi; estimasi durasi dosing dibuat akurat (trapezoid/triangular) agar timeout tidak berlebihan; `FW_VERSI`→v0.6.1 |
| **v0.6.0** | 2026-10-29 | `v0.6.0` | **Ramp semua motor** (stepper akselerasi, DC ramp duty non-blocking); Test Motor mencakup 7 baris (3 stepper + 2 DC + agitasi) dengan scroll; kalibrasi stepper **10 rotasi** (ramp-aware, hasil 0,1 ml); kalibrasi DC **10 detik → ml/detik** + margin 10%; agitasi jadi **SPD1–3**; NVS `pompa` v2 |
| **v0.5.4** | 2026-10-28 | `v0.5.4` | Pompa stepper ramp naik saat start & ramp turun saat stop (`stopMove`, akselerasi 50.000); E-stop/timeout/stall tetap potong seketika |
| **v0.5.3** | 2026-10-28 | `v0.5.3` | Halaman **Test Motor** di Kalibrasi → Motor: START/STOP kontinu per pompa atau ketiga pompa sekaligus; default RPM 500 |
| **v0.5.2** | 2026-10-28 | `v0.5.2` | Pompa stepper DRV8825 1/32 berbasis RPM 400–700 (default 500), kalibrasi 20 rotasi + NVS, dosing selesai berbasis pompa, FastAccelStepper |
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

## Library yang Dibutuhkan

| Library | Versi diuji | Catatan |
|---|---|---|
| Adafruit GFX Library | 1.12.x | OLED |
| Adafruit SSD1306 | 2.5.x | OLED |
| OneWire + DallasTemperature | 2.3.x / 4.0.x | DS18B20 |
| **FastAccelStepper** | **2.0.1** | Pulses stepper 400–700 rpm (MCPWM/PCNT ESP32-S3) |

> FastAccelStepper wajib diinstal dari Library Manager Arduino IDE
> (cari “FastAccelStepper” oleh Jochen Kiemes).

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