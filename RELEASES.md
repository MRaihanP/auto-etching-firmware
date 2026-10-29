# RELEASES — Daftar Versi & Panduan Rollback

Dokumen ini adalah **registri versi** repo `auto-etching-firmware`.
Tujuannya: versi mana pun yang pernah dimiliki bisa ditemukan & dikembalikan
dengan satu perintah.

Rujukan lain: `SUMMARY.md` (fitur) · `LOG.md` (changelog per versi) · `PROGRESS.md` (status)

---

## ⚠️ PENTING: Repo ini punya DUA LINEAGE

| | **Lineage SAH (aktif)** | **Lineage LAMA (batal)** |
|---|---|---|
| Versi | **v0.5.0 → v0.6.0** | v0.5.0 → v0.8.3 |
| Berkas inti | `auto-etching-firmware.ino` + `dcmotor.h`, `buzzer.h`, `motor.h` | `oled_pcb_baruuu.ino` + `aktuator.h` |
| Sumber | branch `main` + **git tag** | folder `backup/` (**tanpa tag**) |
| Status | ✅ jalan, teruji fisik | ❌ **rusak & dibatalkan** (tidak pernah di-commit ke `main`) |

> Nomor versi **v0.5.0–v0.5.1 muncul di kedua lineage**. Yang terdaftar di bawah
> memakai versi lineage **SAH**. Snapshot lineage lama tetap ada di `backup/`
> untuk keperluan arsip, **jangan** di-flash.

---

## 1. Lineage SAH — Versi yang Bisa Di-rollback (pakai git tag)

| Versi | Tanggal | Git tag | Isi utama |
|---|---|---|---|
| **v0.6.3** | 2026-10-29 | `v0.6.3` | **Test Motor disamakan dengan sketch `test/test-motor`** — `pompaTestMulai()` memakai `applySpeedAcceleration()` setelah `runForward()` (urutan sama dengan `setRPM()` sketch); RPM live langsung diterapkan; `pompaRpmSekarang()` membaca **kecepatan aktual** dari queue pulsa; UI menampilkan `target/aktual` (mis. `400/400`) dan redraw 250 ms saat berjalan, sehingga terlihat motor berhenti menaikkan RPM tepat di target; `st` sketch uji mencetak RPM aktual; `FW_VERSI`→v0.6.3 |
| **v0.6.2** | 2026-10-29 | `v0.6.2` | **`PUMP_STEP_PER_REV` 6400 → 3200 (DRV8825 1/16)** — diperbaiki setelah pompa terasa berputar 2x lebih cepat dari RPM target; kini sama dengan sketch `test/test-motor` (`STEPS_PER_REV = 3200`); `PUMP_ACCEL_HZ2` otomatis menjadi **1000 langkah/s²** (= 18,75 rpm/detik, sama dengan `accel = 1000` di sketch); NVS `pompa` **v3** — data v2 dimigrasi otomatis dengan `mlPerRev / 2` (kalibrasi v2 mengira 10 rotasi = 6400×10 langkah, padahal fisik 20 rotasi); `FW_VERSI`→v0.6.2 |
| **v0.6.1** | 2026-10-29 | `v0.6.1` | **Ramp stepper disamakan dengan sketch `test/test-motor`** — akselerasi 1.000 langkah/s² pada 3.200 langkah/putaran = **18,75 rpm/detik**, ditulis sebagai `PUMP_ACCEL_RPM_PER_SEC` agar tidak bergantung microstep; 0→400 rpm ±21,3 s, 0→500 rpm ±26,7 s, deselerasi (`stopMove`) memakai laju sama; `pompaPerkiraanMs()` dihitung ulang dengan profil trapezoid/triangular agar timeout dosing tetap wajar; `FW_VERSI`→v0.6.1 |
| **v0.6.0** | 2026-10-29 | `v0.6.0` | **Ramp semua motor** — stepper akselerasi 50.000 langkah/s² (start & stop), DC/agitasi ramp duty non-blocking `DC_RAMP_PCT_PER_SEC` 100 %/s; **Test Motor** jadi 7 baris scroll (3 stepper + sabun + limbah + agitasi) dengan ubah RPM/duty/SPD live; **kalibrasi stepper baru**: 10 rotasi penuh, hasil ukur 0,1 ml (step 0,1/0,5/1 ml), ramp-aware; **kalibrasi DC baru**: 10 detik pada duty target → ml/detik + margin 10%; **agitasi 3 level** SPD1/SPD2/SPD3 (tanpa ml); NVS `pompa` **v2** (kalibrasi lama di-reset ke default); `FW_VERSI`→v0.6.0 |
| **v0.5.4** | 2026-10-28 | `v0.5.4` | Pompa stepper kini **ramp naik saat start** dan **ramp turun saat stop** (`stopMove()`), `PUMP_ACCEL_HZ2` 1.000.000 → **50.000** (0→500 rpm ±1,07 s); E-stop/timeout/stall tetap `forceStop()`; jeda menghitung sisa langkah setelah motor diam; `FW_VERSI`→v0.5.4 |
| **v0.5.3** | 2026-10-28 | `v0.5.3` | Menu Kalibrasi → Motor + **Test Motor**: halaman baru untuk menyalakan pompa HCl/H₂O₂/Air **kontinu** (START → STOP) satu per satu atau **ketiga sekaligus**; default RPM tetap 500; `FW_VERSI`→v0.5.3 |
| **v0.5.2** | 2026-10-28 | `v0.5.2` | Pompa stepper **DRV8825 1/32** (6400 langkah/putaran) dikendalikan **berbasis RPM 400–700** (default 500, kelipatan 100); pulsa via **FastAccelStepper** (MCPWM/PCNT) menggantikan `loop()`; kalibrasi baru **20 rotasi → ml/rotasi** dengan NVS key `pompa`; dosing selesai berbasis status pompa + timeout `2× estimasi + 10 s`; jeda melanjutkan sisa langkah; sketch uji `test/stepper-rpm/`; `FW_VERSI`→v0.5.2 |
| **v0.5.1** | 2026-10-16 | `v0.5.1` | Exhaust fan dilepas dari TB6612FNG → **langsung 12V, selalu menyala**; GPIO 18 jadi bebas; halaman Kalibrasi Kipas dihapus; `kalibDuty[7]`→`[5]`; `FW_VERSI`→v0.5.1; **sketch uji baru** `test/tb6612fng/`; `PROCESS.md` + `PROGRESS.md` ditulis; `LOG.md` & `SUMMARY.md` diperbarui; lineage lama diarsipkan |
| **v0.5.0** | 2026-10-13/14 | `v0.5.0` | Baseline lineage modular: menu OLED 6 tombol, preset CRUD + NVS (CRC32), 3 pompa stepper non-blocking, 3 motor DC PWM, proses 5 fase, 5 sensor level touch (MA-8), kalibrasi level/suhu/udara/Motor, DS18B20 ×3 (non-blocking), MQ-135 ppm, E-stop, buzzer non-blocking, kompensasi baris atas OLED |

Detail perubahan tiap versi: **`LOG.md`**.

---

## 2. Lineage LAMA — Versi yang Dimiliki (arsip, tidak dipakai)

Seluruh versi berikut **pernah dimiliki** dan tersimpan sebagai snapshot di
folder `backup/`. Semuanya **❌ DIBATALKAN** — arsitektur `aktuator.h` tidak
pernah diuji fisik dan tidak pernah masuk branch `main`.

| Versi | Snapshot | Status |
|---|---|---|
| v0.8.3 | — (hanya di `LOG.md`) | ❌ batal |
| v0.8.2 | `backup/v0.8.2_2026-10-12/` | ❌ batal |
| v0.8.1 | `backup/v0.8.1_2026-10-12/` | ❌ batal |
| v0.8.0 | `backup/v0.8.0_2026-10-12/` | ❌ batal |
| v0.7.1 | `backup/v0.7.1_2026-10-12/` | ❌ batal |
| v0.7.0 | `backup/v0.7.0_2026-10-12/` | ❌ batal |
| v0.6.1 | `backup/v0.6.1_2026-10-12/` | ❌ batal |
| v0.6.0 | `backup/v0.6.0_2026-10-12/` | ❌ batal |
| v0.5.9 | `backup/v0.5.9_2026-10-12/` | ❌ batal (lineage lama) |
| v0.5.8 | `backup/v0.5.8_2026-10-12/` | ❌ batal (lineage lama) |
| v0.5.7 | `backup/v0.5.7_2026-10-12/` | ❌ batal (lineage lama) |
| v0.5.6 | `backup/v0.5.6_2026-10-12/` | ❌ batal (lineage lama) |
| v0.5.5 | `backup/v0.5.5_2026-10-12/` | ❌ batal (lineage lama) |
| v0.5.4 | `backup/v0.5.4_2026-10-12/` | ❌ batal (lineage lama) |
| v0.5.3 | `backup/v0.5.3_2026-10-12/` | ❌ batal (lineage lama) |
| v0.5.2 | `backup/v0.5.2_2026-10-12/` | ❌ batal (lineage lama, **bukan** v0.5.2 di tabel §1) |
| v0.5.1 | `backup/v0.5.1_2026-10-12/` | ❌ batal (lineage lama, **bukan** v0.5.1 di tabel §1) |
| v0.5.0 | `backup/v0.5.0_2026-10-12/` | ❌ batal (lineage lama, **bukan** v0.5.0 di tabel §1) |

> **Perhatian nama sama:** `backup/v0.5.0_2026-10-12/` **bukan** firmware
> yang berjalan hari ini. Yang berjalan adalah hasil `git checkout v0.5.0`.

---

## 3. Cara Rollback

### 3.1 Melihat versi yang tersedia

```bash
git tag -l                    # tag versi lineage SAH
git log --oneline --decorate  # riwayat + posisi tag
```

### 3.2 Melihat isi sebuah versi (tanpa mengubah apa pun)

```bash
git show v0.5.0:config.h | grep FW_VERSI
git diff v0.5.0 v0.5.1 --stat
```

### 3.3 Kembali ke versi lama (untuk di-flash ulang)

```bash
#_opsi 1 — detach HEAD ke versi lama
git checkout v0.5.0
# ... upload ke ESP32 dari Arduino IDE ...
git checkout main               # kembali ke versi terbaru

#_opsi 2 — buat branch agar versi lama bisa di protested
git switch -c legacy/v0.5.0 v0.5.0

#_opsi 3 — batalkan semua perubahan lokal, kembali ke v0.5.0
git reset --hard v0.5.0
```

> ⚠️ `git reset --hard` **menghapus semua perubahan lokal yang belum di-commit**.
> `git checkout v0.5.0` lebih aman karena tidak menghapus apa pun.

### 3.4 Mengembalikan registry versi (kalau `RELEASES.md` hilang / rusak)

```bash
git checkout v0.5.1 -- RELEASES.md
```

### 3.5 Melihat snapshot lineage lama (arsip — jangan di-flash)

```bash
less backup/v0.8.2_2026-10-12/config.h
```

---

## 4. Cara Flash ke ESP32-S3

| Setting | Nilai |
|---|---|
| Board | **ESP32S3 Dev Module** |
| Upload speed | 921600 (turunkan ke 115200 bila upload gagal) |
| Flash size | 8 MB (partition default) |
| Port | `/dev/ttyACM0` atau `/dev/ttyUSB0` (cek `dmesg \| tail`) |

> ⚠️ **Data preset & kalibrasi tersimpan di NVS-flash** dan **tidak hilang**
> saat firmware di-flash ulang. Kalau preset tiba-tiba tidak muncul atau
> berubah, delete flash: `ESP32: Erase Flash` lalu upload ulang.

---

## 5. Aturan Maintainer

1. **Setiap versi yang Navegat = 1 git tag** dengan format `vX.Y.Z`, tanpa prefix lain.
2. **Update 3 berkas berikut setiap versi**: `config.h` (`FW_VERSI`), `LOG.md` (entri baru), `RELEASES.md` (tabel §1).
3. **Backup snapshot** dibuat di `backup/vX.Y.Z_tanggal/` **sebelum** perubahan besar.
4. Nomor versi **tidak pernah dipakai ulang** dalam lineage yang sama.
5. Rilis yang dibatalkan ditandai `❌` di tabel, tidak dihapus — supaya jejaknya jelas.

---

*Terakhir diperbarui: 2026-10-28 — saat v0.5.4 (lineage SAH) dibuat.*