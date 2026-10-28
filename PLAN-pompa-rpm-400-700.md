# Plan — Pompa Peristaltik Stepper 400–700 RPM (DRV8825 1/32)

Tanggal: 2026-10-28
Target versi: **v0.5.2**

## 1. Data uji & keputusan

- Driver terpasang: **DRV8825**, microstep **1/32**.
- Motor 200 langkah/putaran → **6400 langkah/putaran**.
- Putaran motor langsung (tanpa gearbox).
- Pompa baru memindahkan cairan pada **±400 rpm**.
- Batas aman maksimum: **700 rpm**.
- RPM default: **500**, dapat diubah **400 / 500 / 600 / 700** (perubahan kelipatan 100).

Konsekuensi kecepatan langkah:

| RPM | Langkah/detik | Periode |
|---:|---:|---:|
| 400 | 42.667 | 23,4 µs |
| 500 | 53.333 | 18,75 µs |
| 600 | 64.000 | 15,6 µs |
| 700 | 74.667 | 13,4 µs |

Tiga pompa paralel dapat mencapai **224.000 langkah/detik**. Ini tidak mungkin
dihasilkan dengan `digitalWrite()` di `loop()`. Solusi: **FastAccelStepper**
(MCPWM/PCNT ESP32-S3, maksimum 200 kHz per kanal, 4 kanal MCPWM/PCNT).

## 2. Arsitektur

### 2.1 `motor.h`
- Ganti generator pulsa `loop()` + `delayMicroseconds()` dengan FastAccelStepper.
- 3 stepper: STEP/DIR/SLP sesuai pin map `config.h`.
- `SLP` DRV8825 aktif HIGH → `setEnablePin(pin, false)`.
- `setAutoEnable(true)` agar driver tidur setelah gerakan selesai.
- API baru:
  - `pompaSiap(idx)` — stepper ada + `ml/rotasi` sudah dikalibrasi.
  - `pompaSiapSemua()`.
  - `pompaMulai(idx, ml)` — dosing, memakai `ml/rotasi`.
  - `pompaPutarRevolusi(idx, rev)` — untuk uji kalibrasi (tidak butuh ml/rotasi).
  - `pompaStop(idx)`, `pompaStopAll()`.
  - `pompaAktif(idx)`, `pompaSelesai(idx)`, `pompaProgres(idx)`.
  - `pompaPerkiraanMs(idx, ml)` — estimasi durasi dosing.
- Kecepatan: `hz = rpm × 6400 / 60`.
- Akselerasi: `PUMP_ACCEL_HZ2` (default 1.000.000 langkah/s²), dapat disetel.
- Tidak ada `delay()` di jalur pompa.

### 2.2 Kalibrasi (`data.h`, `ui.h`, `.ino`)
Halaman Kalibrasi → Motor → Pompa HCl/H2O2/Air:
- Baris 0: `START` / `STOP` — uji **20 rotasi** pada RPM terpilih.
- Baris 1: `RPM` — 400/500/600/700.
- Baris 2: `Hasil 20 rev` — total ml terukur (KIRI/KANAN ±1 ml).
- Baris 3: `ml/rot` — otomatis = hasil ÷ 20.
- Scroll otomatis seperti halaman Pompa DC.
- BACK: hentikan uji (bila jalan) + simpan ke NVS + toast.
- Jika `ml/rot = 0`, dosing ditolak dengan pesan “Kalibrasi pompa”.

### 2.3 Penyimpanan (`storage.h`)
- Key NVS baru: `pompa`.
- Struct `DataPompa { versi, rpm[3], mlPerRev[3], crc }`.
- Terpisah dari key `kal` agar kalibrasi level lama tidak invalid.
- Default: RPM 500, ml/rot 0 (belum dikalibrasi).

### 2.4 Proses (`process.h`, `.ino`)
- Sebelum mulai proses: `pompaSiapSemua()`; jika belum → toast dan batal.
- Dosing memakai `pompaMulai()` berbasis kalibrasi.
- Fase DOSING ditahan sampai semua pompa selesai, lalu lanjut ETCHING.
- Timeout dosing: 2× estimasi + 10 detik → stop + batal.
- E-stop: `forceStop()` + `disableOutputs()` semua driver.
- Rinsing tetap `pompaMulai(2, 100)`.

## 3. Perubahan file

| File | Perubahan |
|---|---|
| `config.h` | `PUMP_STEP_PER_REV 6400`, `PUMP_RPM_MIN/MAX/DEFAULT/STEP`, `PUMP_ACCEL_HZ2`, `PUMP_KALIB_REV`, versi v0.5.2 |
| `data.h` | state RPM, hasil uji, ml/rotasi, cursor + scroll |
| `motor.h` | rewrite ke FastAccelStepper |
| `storage.h` | `DataPompa`, `simpanPompa()`, `muatPompa()` |
| `ui.h` | halaman kalibrasi stepper baru + scroll |
| `auto-etching-firmware.ino` | handler kalibrasi, pre-check proses, `muatPompa()` |
| `process.h` | dosing berbasis pompa selesai + timeout |
| `test/stepper-rpm/stepper-rpm.ino` | sketch uji mandiri RPM 400–700 |
| `README.md`, `SUMMARY.md`, `LOG.md`, `PROGRESS.md`, `PROCESS.md`, `RELEASES.md` | dokumentasi + dependency FastAccelStepper |

## 4. Urutan implementasi

1. Tulis plan ini.
2. `config.h` + `data.h`.
3. Rewrite `motor.h` (FastAccelStepper).
4. `storage.h` (NVS pompa).
5. `ui.h` + `.ino` (kalibrasi RPM).
6. `process.h` (dosing selesai + timeout + pre-check).
7. `test/stepper-rpm` sketch.
8. Compile `esp32:esp32:esp32s3` dengan arduino-cli.
9. Update dokumentasi, backup, commit.
10. Push ke `origin/main` + tag `v0.5.2`.

## 5. Kriteria uji

- Compile tanpa error/warning kritis.
- Uji kalibrasi 20 rotasi di 400/500/700 rpm; hasil masuk NVS dan bertahan setelah reboot.
- Dosing 3 pompa paralel pada 500 rpm; volume error ≤5% untuk 10/50/100 ml.
- Tidak ada missed step / stall pada 400–700 rpm.
- E-stop menghentikan ketiga pompa seketika.
- UI tetap responsif saat 3 pompa berjalan.

## 6. Risiko

- 1/32 microstep pada 700 rpm membutuhkan torsi tinggi; jika motor stall, turunkan
  akselerasi atau RPM operasi (default 500).
- FastAccelStepper adalah dependency baru; harus diinstal di Arduino IDE.
- Tanpa encoder, firmware hanya menghitung pulsa; stall mekanis tidak terdeteksi.
- Volume kecil pada 400 rpm bisa terlalu cepat; jika error >5%, pertimbangkan
  burst dosing atau pompa/tube khusus.
