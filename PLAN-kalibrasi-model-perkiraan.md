# Rencana: Kalibrasi Stepper dengan Model Perkiraan Awal (Pre-Calibration Estimate)

## Tujuan
- Motor punya model perkiraan volume cairan keluar meski belum dikalibrasi.
- Target kalibrasi dicoba keluarkan dengan perkiraan awal (tanpa data kalibrasi).
- User mengukur hasil nyata → sistem hitung koreksi (`ml/rotasi` aktual) → simpan untuk dosing selanjutnya.

---

## 1. Model Perkiraan Awal (Tanpa Kalibrasi)

### Parameter yang dibutuhkan
- `PUMP_STEP_PER_REV` = langkah stepper per rotasi (dari hardware/config)
- `PUMP_VOLUME_PER_REV_THEORETICAL` = volume teoretis per rotasi (ml/rev) — diturunkan dari geometri pump head (contoh: 0.5 ml/rev untuk syringe pump standar). Jika tidak diketahui, pakai default aman (misal 0.5 ml/rev) untuk semua pompa.
- `RPM_TARGET` = target kecepatan rotasi (dikonversi ke Hz via `rpmKeHz()`)

### Rumus Langkah Target
```
langkah_target = lround( target_ml * PUMP_STEP_PER_REV / PUMP_VOLUME_PER_REV_THEORETICAL )
```
- Sama bentuknya dengan `pompaMulai()`, tapi pakai `PUMP_VOLUME_PER_REV_THEORETICAL` sebagai `calMlPerRev`.
- Fungsi baru: `pompaMulaiEstimasi(idx, target_ml)` → panggil `pompaJalanLangkah()` dengan langkah_target.

### Batasan Model Perkiraan
- Tanpa kalibrasi, `ml/rev` teoretis mungkin tidak akurat ±10–20% karena variasi ketebalan selang, tekanan, viskositas cairan.
- Tujuan: memberikan titik awal yang cukup dekat untuk uji kalibrasi pertama.

---

## 2. Alur Kalibrasi Baru (Single-Page UI)

### State Machine (5 state, tetap di 1 halaman)
```
KALIB_MAN_IDLE
KALIB_MAN_RUN
KALIB_MAN_MEASURE
KALIB_MAN_SAVE
KALIB_MAN_FAILED
```

### Baris Form (scrollable, navigasi serupa page lain)
1. **Pompa** (HCl / H2O2 / Air) — pilih dengan LEFT/RIGHT
2. **Target (ml)** — pilih 50 / 100 / 200 ml dengan LEFT/RIGHT
3. **Mode** — PRIMING / LANGSUNG (LEFT/RIGHT toggle)
4. **Hasil (ml)** — hasil ukur user (editable di state MEASURE)
5. **[ CALIBRATE ]** — tombol aksi (ENTER = mulai/stop)
6. **Error (%)** — tampil setelah SAVE (read-only)

### Transisi State
| State | Deskripsi | Tombol |
|---|---|---|
| `IDLE` | Pilih pompa, target, mode. Cek apakah ada kalibrasi lama. | UP/DOWN pilih baris. LEFT/RIGHT ubah nilai. ENTER di CALIBRATE → RUN |
| `RUN` | Motor berputar sesuai target_ml. Jika ada kalibrasi lama → pakai `kalibStepperMl[idx]`. Jika belum → pakai `PUMP_VOLUME_PER_REV_THEORETICAL`. | ENTER = stop. Setelah selesai/stop → otomatis ke MEASURE |
| `MEASURE` | User masukkan hasil ukur nyata ke baris **Hasil**. | LEFT/RIGHT ubah angka Hasil. ENTER → SAVE |
| `SAVE` | Hitung `ml/rotasi` aktual: `actual_ml_per_rev = hasil_ukur_ml / rotasi_actual`. Simpan ke `kalibStepperMl[idx]`. Tampilkan Error %. | ENTER → kembali ke IDLE (cursor ke CALIBRATE). BACK → IDLE tanpa save |
| `FAILED` | Kalibrasi digagalkan karena user tidak memasukkan hasil ukur (tekan BACK di MEASURE). | ENTER atau BACK → kembali ke IDLE (cursor ke CALIBRATE). Data lama tetap dipertahankan |

---

## 3. Perhitungan Koreksi Tanpa Bias

### Rumus Kalibrasi
```
rotasi_actual = langkah_target / PUMP_STEP_PER_REV
ml_per_rev_aktual = hasil_ukur_ml / rotasi_actual
```
- `langkah_target` = langkah yang sudah dikirim ke motor (diketahui dari `pompaDosisKalibrasi()` atau `pompaMulaiEstimasi()`).
- `rotasi_actual` = `langkah_target / PUMP_STEP_PER_REV` (bisa koma, disimpan sebagai float/double).
- `ml_per_rev_aktual` = volume aktual per rotasi, dipakai untuk dosing selanjutnya.

### Penyimpanan
- Simpan `ml_per_rev_aktual` ke `kalibStepperMl[idx]` (ml/rev).
- `pompaMulai(idx, ml)` akan pakai nilai ini untuk hitung langkah masa depan.

### Tampilan Error
```
error_pct = (ml_per_rev_aktual - PUMP_VOLUME_PER_REV_THEORETICAL) / PUMP_VOLUME_PER_REV_THEORETICAL * 100
```
Tampilkan di baris **Error (%)** setelah save.

---

## 4. Perubahan File yang Dibutuhkan

### `config.h`
- Tambah konstanta:
  - `PUMP_VOLUME_PER_REV_THEORETICAL` = 0.5f (default teoretis, bisa diubah sesuai hardware)
  - `KALIB_TOLERANSI_PCT` = 15.0f (batas error maksimal untuk warning, opsional)

### `data.h`
- Tambah enum state: `KALIB_MAN_MEASURE`, `KALIB_MAN_SAVE`, `KALIB_MAN_FAILED` (jika belum ada).
- Tambah variabel:
  - `float kalibStepperRevolutions[3]` — simpan rotasi terakhir yang dijalankan (untuk perhitungan kalibrasi).
  - `float kalibStepperMeasuredMl[3]` — simpan hasil ukur nyata yang dimasukkan user.
  - `bool kalibHasRun` — flag bahwa motor sudah berjalan untuk kalibrasi saat ini.

### `motor.h`
- Tambah fungsi baru: `bool pompaMulaiEstimasi(int idx, int ml)` — mirip `pompaMulai()` tapi pakai `PUMP_VOLUME_PER_REV_THEORETICAL` sebagai fallback saat belum ada kalibrasi.
- Tambah fungsi/mekanisme untuk menyimpan `langkah_target` terakhir kalibrasi agar bisa dihitung `rotasi_actual`-nya.

### `ui.h`
- Update `gambarKalibStepper()` agar menampilkan 6 baris dengan scroll, navigasi cursor-based serupa page lain.
- Tampilkan state CALIBRATE / MEASURE / SAVE / FAILED dengan teks berbeda.
- Tambah baris **Error (%)** untuk menampilkan perbedaan antara teoretis dan aktual.
- Pada state FAILED, tampilkan pesan "BATAL" di baris CALIBRATE atau Error.

### `auto-etching-firmware.ino`
- Handler `HAL_KALIB_STEPPER` di-update untuk state machine baru:
  - IDLE → RUN: cek `kalibStepperMl[idx]`. Jika > 0 → panggil `pompaMulai(idx, target_ml)`. Jika 0 → panggil `pompaMulaiEstimasi(idx, target_ml)`. Simpan `langkah_target` dan `rotasi_actual`.
  - RUN: tekan ENTER = `pompaStopAll()`. Setelah motor berhenti → MEASURE.
  - MEASURE: LEFT/RIGHT edit `kalibStepperMeasuredMl[idx]`. ENTER → SAVE. BACK → FAILED.
  - SAVE: hitung `ml_per_rev`, simpan ke `kalibStepperMl[idx]`, tampilkan Error %. ENTER → IDLE.
  - FAILED: tampilkan status gagal. ENTER atau BACK → IDLE (cursor ke CALIBRATE). Data lama tetap dipertahankan.

---

## 5. Verifikasi
- Setelah implementasi, kompilasi dengan `arduino-cli compile --fqbn esp32:esp32:esp32s3 .` harus EXIT=0.
- Test alur:
  1. Pilih pompa 0 (HCl), target 50 ml, mode Priming.
  2. Tekan CALIBRATE → motor berputar sesuai kalibrasi lama (jika ada) atau perkiraan awal (jika belum ada).
  3. Setelah selesai, masukkan hasil ukur misal 48 ml, tekan ENTER.
  4. Sistem hitung `ml_per_rev_aktual` dan simpan.
  5. Cek bahwa dosing selanjutnya pakai nilai baru.
  6. Ulang kalibrasi: pastikan sistem pakai `kalibStepperMl[idx]` yang baru, bukan teoretis.
  7. Tekan BACK saat di MEASURE → pastikan state FAILED, data lama tetap ada.

---

## 6. Catatan
- `PUMP_VOLUME_PER_REV_THEORETICAL` bisa diubah nanti sesuai karakteristik nyata pompa.
- Jika `kalibStepperMl[idx] == 0.0f` (belum dikalibrasi), fallback ke model teoretis untuk dosing produksi juga (`pompaMulaiEstimasi`).
- Jika ada kalibrasi lama, kalibrasi berikutnya hanya memperbaiki offset — tidak kembali ke teoretis.
- Tidak ada bias kuantisasi karena menghitung langkah langsung dengan `lround()` seperti sebelumnya.
- Kalibrasi dianggap **gagal** jika user menekan BACK tanpa memasukkan hasil ukur. Data lama tetap dipertahankan.
