# Log Perubahan

Format entri: `[Tanggal] vX.Y.Z — ringkasan`
Setiap versi di-backup ke folder `backup/vX.Y.Z_tanggal/` sebelum perubahan dilakukan.

> ## ⚠️ CATATAN LINEAGE (2026-10-16) — BACA DULU
>
> Repo ini punya **DUA lineage firmware**. Semua entri bertanggal
> **2026-10-12 s.d. 2026-10-13** di bawah ini sudah ditandai
> `❌ DIBATALKAN (lineage lama)` dan **tidak boleh dipakai sebagai acuan**.
>
> | | **Lineage LAMA (batal)** | **Lineage SAH (aktif)** |
> |---|---|---|
> | Rentang versi | v0.5.0 → v0.8.3 | v0.5.0 → v0.5.2 (dimulai ulang) |
> | Berkas inti | `aktuator.h` + `oled_pcb_baruuu.ino` | `dcmotor.h` + `buzzer.h` + `motor.h` + `auto-etching-firmware.ino` |
> | Driver motor | `PIN_PWM_FAN`, `fanSet(duty)` | `PIN_PWA_D1/D2`, `PIN_PWB_D1` |
> | Status | ❌ **rusak, tidak pernah di-commit ke `main`, tidak pernah diuji fisik** | ✅ **berjalan di branch `main`, uji fisik terbukti** |
> | Arsip | folder `backup/v0.5.0_2026-10-12/`–`v0.8.2_2026-10-12/` | branch `main` |
>
> Semua snapshot di `backup/` memakai `aktuator.h`, **termasuk** `v0.5.0` &
> `v0.5.1` — sehingga entri 2026-10-12 untuk dua versi itu ditandai
> `❌ LINEAGE LAMA` dan **bukan** versi yang sama dengan lineage aktif.
> Nomor versi **direset** ketika rewrite dimulai; entri v0.5.1 tanggal
> 2026-10-16 memakai arsitektur lineage aktif.
>
> Rujukan acuan yang benar: `PROCESS.md` (rencana) & `PROGRESS.md` (status).

---

## [2026-10-30] v0.6.5 — Kalibrasi stepper: rotasi dihitung hanya saat RPM target

**Latar belakang:**
- Uji kalibrasi v0.6.4 menghitung seluruh langkah perintah, termasuk ramp
  up/down. Saat ramp up pompa belum menyedot cairan, sehingga ml/rotasi yang
  dihitung menjadi terlalu kecil.
- Rotasi yang dipakai untuk kalibrasi harus rotasi **setelah kecepatan
  aktual mencapai RPM target**, bukan saat ramp.

**Perubahan Kode:**
- `motor.h`
  - `pompaDosisKalibrasi()` sekarang menjalankan pompa kontinu dengan ramp
    (`runForward()`), lalu menunggu kecepatan aktual ≥ 98% RPM target.
  - Begitu speed target tercapai, posisi awal dicatat dan langkah target
    (`target ml / kalibrasi`) dihitung dari titik itu.
  - Setelah langkah target tercapai, pompa dihentikan dengan `pompaStop()`
    (deselerasi/ramp turun), bukan `forceStop()`.
  - Timeout 60 detik bila RPM target tidak tercapai → berhenti dengan ramp
    dan toast `Gagal capai RPM`.
  - Progres kalibrasi 0% selama ramp, lalu naik hanya untuk langkah yang
    dihitung; status baris START menampilkan `RAMP...` saat ramp up.
  - Status kalibrasi dibersihkan di `pompaStop()`, `pompaStopDarurat()`,
    `pompaJalanLangkah()`, dan `pompaTestMulai()` agar tidak bentrok dengan
    dosing/Test Motor.
- `auto-etching-firmware.ino` — toast awal `Ramp ke RPM...`; saat selesai
  tetap `Selesai, ukur ml`, dan membedakan kegagalan capai RPM.
- `ui.h` — baris START/STOP menampilkan `RAMP...` selama fase ramp up.

**Catatan:**
- Target ml sekarang berarti volume yang keluar **saat pompa sudah di RPM
  target**. Ramp up/down tetap berputar demi keamanan pump head, tetapi tidak
  dihitung sebagai volume.
- Karena ramp sangat landai (18,75 rpm/detik), uji kalibrasi akan terasa lama
  (mis. 0→500 rpm ±26,7 detik, lalu ±26,7 detik turun). Ini konsekuensi dari
  syarat start/stop yang sama dengan Test Motor.

---

## [2026-10-30] v0.6.4 — Uji kalibrasi per motor (target ml → offset → kalibrasi ulang)

**Latar belakang:**
- Kalibrasi lama memakai 10 rotasi (stepper) / 10 detik (DC), lalu user
  menghitung sendiri. Tidak ada verifikasi bahwa hasil dosing benar-benar
  sesuai target ml.
- Start/stop kalibrasi juga harus memakai jalur yang sama dengan Test Motor
  (ramp naik/turun) agar pump head tidak rusak.

**Perubahan Kode:**
- `motor.h`
  - `pompaJalanLangkah()` kini memanggil `applySpeedAcceleration()` setelah
    `move()` (paritas dengan sketch `test/test-motor`).
  - Fungsi baru `pompaDosisKalibrasi(idx, ml, calMlPerRev)` — dosing sejauh
    target ml memakai kalibrasi yang dipilih, lewat jalur ramp yang sama.
- `dcmotor.h`
  - `dcKalibrasiMs(ml, rate)` — durasi uji **tanpa** margin 10%; kalau margin
    ikut dihitung saat uji, hasil ukur selalu ~10% di atas target.
- `data.h` — state uji baru: target ml, hasil ukur, offset, kalibrasi efektif
  saat uji, untuk 3 pompa stepper dan 2 pompa DC.
- `ui.h` — halaman kalibrasi stepper & DC menjadi **6 baris** (START/STOP,
  RPM/Duty, Target, Hasil, Offset, ml/rotasi atau ml/detik), scrollable.
- `auto-etching-firmware.ino`
  - START menjalankan uji target ml dengan kalibrasi sekarang; STOP memakai
    `pompaStop()`/`dcStop()` (ramp turun).
  - ENTER di baris **Hasil** menghitung `offset = (hasil - target) / target`
    dan kalibrasi baru `cal = calLama * hasil / target`, lalu simpan NVS.
  - Target/Duty/Hasil dikunci saat uji berjalan supaya hasil tidak ambigu.
- `storage.h` — field lama (hasil 10 rotasi/10 detik) tidak lagi diturunkan;
  `mlPerRev` dan `dcMlPerSec` tetap format NVS `pompa` v3.

**Catatan:**
- Uji stepper bergerak tepat sejumlah langkah = target ml / kalibrasi, jadi
  hasil ml/rotasi tetap akurat meskipun ramp belum mencapai RPM target.
- Uji DC memakai duty terpilih dan durasi tanpa margin; margin 10% tetap
  dipakai saat dosing normal (`dcDosisMs`).

---

## [2026-10-29] v0.6.3 — Test Motor: RPM aktual ditampilkan & start = sketch

**Latar belakang:**
- Setelah pemetaan 3200 langkah/putaran (v0.6.2), Test Motor perlu dipastikan
  berperilaku **sama persis** dengan sketch `test/test-motor`: setelah ramp
  selesai, kecepatan harus **berhenti di target** (400 tetap 400), tidak terus
  naik.
- Sebelumnya tidak ada cara melihat kecepatan aktual, jadi sulit membedakan
  "masih ramp" dengan "sudah berhenti di target".

**Perubahan Kode:**
- `motor.h`
  - `pompaTestMulai()` kini memanggil `applySpeedAcceleration()` setelah
    `runForward()`, urutan yang sama dengan `setRPM()` di sketch.
  - `pompaAturRpmLive()` memeriksa hasil `setSpeedInHz()` lalu
    `applySpeedAcceleration()`; perubahan RPM saat berjalan langsung dipakai
    ramp generator (tidak menunggu restart).
  - Fungsi baru `pompaRpmSekarang(idx)` membaca **kecepatan aktual** dari
    queue pulsa (`getCurrentSpeedInMilliHz`), bukan nilai target.
- `ui.h` — baris HCl/H2O2/Air di Test Motor menampilkan `target/aktual`
  (mis. `400/400`) saat berjalan, `400rpm` saat berhenti.
- `auto-etching-firmware.ino` — redraw Test Motor tiap 250 ms selama stepper
  berjalan agar angka aktual terlihat naik lalu berhenti di target.
- `test/test-motor/test-motor.ino` — perintah `st` mencetak RPM aktual juga,
  jadi bisa dibandingkan langsung dengan firmware.

**Catatan:**
- Ramp tetap **18,75 rpm/detik** (sama dengan sketch, `accel = 1000` pada 3200
  langkah/putaran). Artinya 0 → 400 rpm memerlukan **±21 detik**; selama itu
  angka aktual memang naik. Setelah ±21 detik harus berhenti di 400.
- Kalau ingin lebih cepat mencapai target tanpa mengubah sketch, naikkan
  `PUMP_ACCEL_RPM_PER_SEC` (mis. `37.5` = 0→400 rpm ±10,7 s); itu bukan bug,
  hanya setelan ramp.

---

## [2026-10-29] v0.6.2 — Step/rev 6400 → 3200: RPM akhirnya sesuai target

**Latar belakang:**
- Setelah v0.6.1, pompa stepper terasa **berputar lebih cepat dari RPM yang
  ditampilkan** (target 500 rpm, putaran nyata ±2x lipat).
- Penyebabnya bukan ramp, melainkan **microstep**. Firmware memakai
  `PUMP_STEP_PER_REV = 6400` (asumsi DRV8825 1/32), sedangkan modul yang
  terpasang berjalan di **1/16 = 3200 langkah/putaran** — persis sama dengan
  sketch `test/test-motor` (`STEPS_PER_REV = 3200`).
- Rumus `step/detik = rpm × langkah/putaran / 60` mengirim pulsa 2x lebih
  banyak per detik, jadi motor memang berputar 2x lebih cepat. Ramp juga ikut
  2x lebih tajam dalam rpm/detik, yang berisiko merusak pump head.

**Perubahan Kode:**
- `config.h`
  - `PUMP_STEP_PER_REV` = **3200L** (200 langkah × 1/16), sama dengan sketch.
  - `PUMP_ACCEL_HZ2` kini otomatis **1000** langkah/s² — angka `accel = 1000`
    di sketch. Dalam rpm/detik tetap 18,75 rpm/detik (tidak berubah).
  - `FW_VERSI` → `v0.6.2`.
- `motor.h` — komentar kecepatan diperbarui: 400–700 rpm = 21,333–37,333
  langkah/detik.
- `storage.h`
  - `FORMAT_VERSI_POMPA` 2 → **3**.
  - `muatPompa()` menerima data v2 dan **membagi `mlPerRev` dengan 2**.
    Alasannya: kalibrasi v2 dijalankan saat firmware memakai 6400 langkah/
    putaran, sehingga perintah "10 rotasi" sebenarnya = **20 rotasi fisik** dan
    ml/rotasi tersimpan 2x terlalu besar. Data hasil migrasi langsung
    disimpan ulang sebagai v3.

**Catatan:**
- Kalibrasi v2 **tidak perlu diulang**; migrasi menjaga volume dosing tetap
  sama. Tetap disarankan cek sekali dengan menimbang/mengukur hasil dosing.
- Kalau ternyata modul DRV8825 Anda benar-benar 1/32 (jumper MS1/MS2/MS3
  semua HIGH), kembalikan `PUMP_STEP_PER_REV` ke `6400L` **dan** naikkan
  `FORMAT_VERSI_POMPA` ke 4 dengan migrasi `mlPerRev *= 2`, lalu kalibrasi
  ulang. Jangan hanya mengubah angkanya tanpa migrasi.
- Gejala "lebih cepat dari target" seharusnya hilang: pada 500 rpm poros pompa
  berputar 500 rpm (8,33 putaran/detik), sama dengan sketch.

---

## [2026-10-29] v0.6.1 — Ramp stepper disamakan dengan sketch `test/test-motor`

**Latar belakang:**
- Ramp v0.6.0 (akselerasi 50.000 langkah/s²) mencapai 500 rpm hanya dalam
  ±1,07 detik. Lonjakan torsi sebesar itu membuat **pump head peristaltik
  rusak**.
- Sketch `test/test-motor` memakai `accel = 1000` langkah/s² pada
  `STEPS_PER_REV = 3200` = **18,75 rpm/detik**. Ramp inilah yang dianggap aman.

**Perubahan Kode:**
- `config.h`
  - Baru: `PUMP_ACCEL_RPM_PER_SEC = 18.75f` (setara sketch di atas).
  - `PUMP_ACCEL_HZ2` sekarang **dihitung** dari nilai rpm/detik tersebut:
    `18,75 x 6400 / 60 = 2000` langkah/s². Jadi ramp tidak berubah walau
    microstep (`PUMP_STEP_PER_REV`) diubah.
  - Efek: 0 → 400 rpm ±21,3 s, 0 → 500 rpm ±26,7 s, 0 → 700 rpm ±37,3 s.
    Deselerasi (`stopMove`) memakai laju yang sama, jadi berhenti pun halus.
- `motor.h` — `pompaPerkiraanMs()` memakai rumus **trapezoid/triangular**
  yang akurat. Rumus lama (`langkah/hz + hz/akselerasi`) terlalu
  overestimate ketika ramp sangat landai, sehingga timeout dosing membengkak
  tanpa perlu.

**Catatan:**
- Jumlah langkah (dan volume) tetap tepat: ramp hanya mengubah *kapan*
  kecepatan dicapai, bukan jumlah pulsa. Dosing jadi lebih lama, bukan
  berkurang volumenya.
- Kalibrasi 10 rotasi bisa berjalan tanpa pernah mencapai RPM target
  (profil triangular), tetapi ml/rotasi tetap valid karena pompa peristaltik
  memindahkan volume tetap per putaran.
- Bila dosing terasa terlalu lambat, naikkan `PUMP_ACCEL_RPM_PER_SEC`
  (mis. `37.5` = 0→500 rpm ±13,3 s). Bila masih terasa hentakan, turunkan.

---

## [2026-10-29] v0.6.0 — Ramp semua motor, Test Motor 7 baris, kalibrasi baru

**Latar belakang:**
- Motor tidak boleh langsung 0 → target (stepper maupun DC) karena beban
  mekanis (peristaltik/agitator) bisa slip atau stall.
- Kalibrasi lama hanya mencatat duty mentah (0–255) untuk DC dan agitasi,
  sehingga tidak ada hubungannya dengan volume (ml).
- Hasil ukur pompa stepper perlu resolusi lebih halus dari 1 ml.

**Perubahan Kode:**
- `config.h`
  - `PUMP_KALIB_REV` 20 → **10** rotasi.
  - Baru: `DC_RAMP_PCT_PER_SEC` 100 %/s, `KALIB_DC_DETIK` 10, `DC_MARGIN_PCT` 10,
    `AGITASI_SPD1/2/3_DEFAULT` = 40/70/100 %.
- `dcmotor.h` — **ditulis ulang**:
  - 3 kanal (`DC_AGI`, `DC_SABUN`, `DC_LIMBAH`) dengan target & duty aktual
    dalam persen, ramp **non-blocking** di `dcmotorUpdate()` (tidak ada
    lonjakan 0 → 100 %).
  - `dcSetPct/dcStop/dcStopAll/dcStopAllDarurat/dcAktif/dcDiTarget/dcDiam`.
  - `agitasiSet(1..3)` memakai `agitasiSpdDuty[]`; `pompaSabunSet/pompaLimbahSet`
    memakai `kalibDutyDC[]`.
  - State machine kalibrasi DC: `RAMPUP → UKUR (10 s) → TURUN`; volume diukur
    setelah pompa benar-benar diam sehingga hasil ml/detik tidak terpengaruh
    ramp (volume ramp naik & turun saling meniadakan).
  - `dcDosisMs(idx, ml)` = durasi dosing berbasis ml/detik + margin 10 %.
- `motor.h` — `pompaAturRpmLive(idx, rpm)` untuk mengubah RPM saat pompa
  berjalan (`applySpeedAcceleration()`, tetap ikut ramp).
- `data.h`
  - Halaman `HAL_TEST_POMPA` → **`HAL_TEST_MOTOR`**.
  - Baru: `kalibStepperHasilMl10[]`, `kalibStepperStep` (0,1/0,5/1 ml),
    `kalibDutyDC[2]`, `kalibDcHasilMl[2]`, `kalibDcMlPerSec[2]`, `kalibDcStep`,
    state `KalibDcFase`, `agitasiSpdDuty[3]`, `agitasiSpdUji`,
    `testMotorCursor/testMotorScroll/testAgitSpeed`, `kalibDcUkur`,
    `kalibStepperUkur`.
  - Dihapus: `kalibDuty[5]`, `kalibMl[2]`, `testPompaCursor`,
    `kalibMotorRunning/StartMs/RunMs`.
  - Preset default: speed agitasi 4/5 → 3.
- `storage.h` — **`DataPompa` versi 2**: `rpm[3]`, `mlPerRev[3]`, `dcDuty[2]`,
  `dcMlPerSec[2]`, `agitDuty[3]` + CRC. Kalibrasi versi lama otomatis tidak
  valid → default. Speed agitasi preset lama (4/5) di-clamp ke 3 saat dimuat.
- `presetops.h` — field “Speed agit” dibatasi 1–3.
- `ui.h`
  - Kalibrasi stepper: header `HCl 10rev`, baris START/STOP, RPM, **Hasil**
    (0,1 ml, step bisa diganti), ml/rotasi 3 desimal, scroll.
  - **Test Motor**: 7 baris (Semua / HCl / H2O2 / Air / Sabun / Limbah /
    Agitasi), auto-scroll, nilai RPM/duty/SPD + status START/STOP.
  - Kalibrasi DC: header `Sabun 10s`, START/STOP dengan hitung mundur
    `STOP Ns`, Duty, Hasil (step 1/5/10 ml), ml/detik.
  - Agitasi: 3 baris SPD1/SPD2/SPD3 (duty %) + status ON/OFF; ENTER
    menjalankan level terpilih (ramp dari 0 %), tanpa `delay()`.
- `auto-etching-firmware.ino`
  - `testMotorMulaiSemua()` / `testMotorHentikanSemua()`.
  - Handler stepper: START = putar `PUMP_KALIB_REV` rotasi, RPM & hasil live,
    toast “Selesai, ukur ml” saat uji selesai.
  - Handler DC: START/STOP uji 10 detik, duty & hasil live, step hasil.
  - Handler agitasi: KIRI/KANAN duty, ENTER ON/OFF level terpilih.
  - `dcmotorUpdate()` + `kalibDcUpdate()` dipanggil tiap `loop()`.
  - E-stop memakai `dcStopAllDarurat()` (tanpa ramp) + reset state uji.
  - `process.h`: `dcmotorStopAll()` → `dcStopAll()`; DRAINING mencatat
    perkiraan durasi buang dari ml/detik kalibrasi + margin.

**Catatan:**
- Kalibrasi stepper tetap tepat 10 rotasi meskipun ada ramp, karena
  FastAccelStepper menghitung langkah, bukan waktu.
- Kalibrasi DC: duty memakai persen (0–100), bukan 0–255 lagi.
- NVS `pompa` naik versi → **kalibrasi motor harus diulang** setelah update.
- DC kurang presisi (selang/tegangan/viskositas); margin 10 % dipakai pada
  dosing berbasis waktu, dan pembacaan ml/detik tetap ditampilkan apa adanya.

---

## [2026-10-28] v0.5.4 — Ramp naik/turun pompa stepper (akselerasi & deselerasi)

**Perubahan Kode:**
- `config.h` — `PUMP_ACCEL_HZ2` turun dari `1000000` ke **`50000`** langkah/detik².
  Ramp 0 → 500 rpm (53.333 Hz) kini ~**1,07 detik**, sehingga start terlihat
  naik bertahap, bukan langsung 100% ke RPM target.
- `motor.h` — `pompaStop()` memakai **`stopMove()`** (deselerasi) dan tidak lagi
  `forceStop()` + `disableOutputs()` seketika.
  - `pompaTargetAktif` tetap `true` selama ramp turun, jadi UI masih
    menampilkan `STOP` sampai motor benar-benar diam.
  - `motorUpdate()` menyelesaikan status, mematikan driver, dan mencatat
    `[POMPA] Berhenti idx ...` setelah deselerasi selesai.
  - Fungsi baru **`pompaStopDarurat()`** / **`pompaStopAllDarurat()`** memakai
    `forceStop()` + `disableOutputs()` untuk E-stop dan kondisi fault.
  - Watchdog stall 3 detik sekarang memakai stop darurat.
  - Jeda (`pompaJedaSemua`) juga deselerasi; sisa langkah dihitung setelah
    motor diam agar tidak over-dosing. Lanjut saat masih deselerasi ditunda
    lewat `pompaLanjutPending` dan dijalankan otomatis setelah berhenti.
- `auto-etching-firmware.ino` — handler E-stop memanggil
  `pompaStopAllDarurat()` (tanpa ramp) sebelum `prosesBatal()`.
- `process.h` — timeout dosing memakai `pompaStopAllDarurat()`.
- Estimasi durasi `pompaPerkiraanMs()` memakai koreksi trapezoid
  `langkah/hz + hz/akselerasi`.

**Catatan:**
- Stop normal (tombol STOP di Test Motor, ganti fase, batal proses) = ramp turun.
- E-stop, timeout, dan stall = potong seketika (tanpa ramp) demi keamanan.
- Perilaku ramp perlu diverifikasi di hardware; bila motor stall saat
  akselerasi, turunkan `PUMP_ACCEL_HZ2` (mis. `30000`).

---

## [2026-10-28] v0.5.3 — Halaman Test Motor (3 pompa sekaligus) + default 500 rpm

**Perubahan Kode:**
- `ui.h` — menu Kalibrasi → Motor bertambah item **“Test Motor”** (7 item).
  Halaman baru `gambarTestPompa()`: baris **Semua / HCl / H2O2 / Air**;
  tiap baris menampilkan `START` atau `STOP`.
- `motor.h` — `pompaTestMulai(idx)`: menjalankan pompa **kontinu**
  (`runForward()`) sampai dihentikan; tidak butuh kalibrasi ml/rotasi.
- `auto-etching-firmware.ino` — handler `HAL_TEST_POMPA`: ENTER menyalakan
  (label berubah jadi STOP), ENTER di STOP menghentikan; baris “Semua”
  menyalakan/menghentikan ketiga pompa sekaligus; BACK menghentikan semua.
- `data.h` — halaman `HAL_TEST_POMPA` + `testPompaCursor`.
- Default RPM tetap **500** (`PUMP_RPM_DEFAULT`), batas 400–700 kelipatan 100.

**Catatan:**
- Pompa uji tidak berhenti otomatis setelah N rotasi — hanya STOP/BACK yang
  menghentikan. Watchdog 3 detik tetap aktif bila posisi tidak maju (stall).
- Pin map dikonfirmasi sama dengan tabel pengguna:
  `SLP1=1, STEP1=2, DIR1=42, SLP2=41, STEP2=40, DIR2=39, SLP3=38, STEP3=47, DIR3=21`.

---

## [2026-10-28] v0.5.2 — Pompa stepper berbasis RPM 400–700 (DRV8825 1/32)

**Latar belakang:**
- Uji langsung: pompa peristaltik HCl/H₂O₂/air baru memindahkan cairan pada
  **±400 rpm**; batas aman maksimum **700 rpm**.
- Firmware lama memakai `LANGKAH_INTERVAL_US = 400` (tetap 2.500 langkah/detik).
  Pada DRV8825 1/32 (6.400 langkah/putaran) itu hanya **≈23,4 rpm** — jauh di
  bawah 400 rpm, sehingga pompa tidak mengalirkan cairan.
- Pada 400–700 rpm, 3 pompa paralel membutuhkan hingga **224.000 langkah/detik**;
  `digitalWrite()` di `loop()` tidak mampu. Pulsa kini dibangkitkan
  **FastAccelStepper** (MCPWM/PCNT ESP32-S3, maks. 200 kHz/kanal).

**Perubahan Kode:**
- `config.h` — `PUMP_STEP_PER_REV 6400`, `PUMP_RPM_MIN/MAX/DEFAULT/STEP`,
  `PUMP_ACCEL_HZ2`, `PUMP_KALIB_REV 20`, `PUMP_TIMEOUT_MARGIN_MS`; versi v0.5.2.
- `motor.h` — rewrite total: FastAccelStepper, API RPM, `pompaMulai()` memakai
  kalibrasi ml/rotasi, `pompaPutarRevolusi()`, `pompaProgres()`,
  `pompaPerkiraanMs()`, jeda/lanjut dengan sisa langkah, watchdog 3 detik.
- `data.h` — state `kalibStepperRpm[3]`, `kalibStepperHasilMl[3]`,
  `kalibStepperMl[3]` (ml/rotasi), cursor + scroll.
- `storage.h` — key NVS baru `pompa` (`DataPompa`: rpm + ml/rotasi + CRC),
  terpisah dari `kal`; `muatPompa()` dipanggil saat boot.
- `ui.h` — halaman kalibrasi stepper: START/STOP, RPM, Hasil 20 rev, ml/rotasi,
  scroll otomatis.
- `auto-etching-firmware.ino` — handler kalibrasi baru (RPM kelipatan 100,
  hasil ±1 ml, simpan saat BACK), pre-check `pompaSiapSemua()` sebelum proses.
- `process.h` — dosing ditahan sampai semua pompa selesai, timeout 2× estimasi
  + 10 detik, jeda menghentikan pompa dan melanjutkan sisa langkah.
- `test/stepper-rpm/stepper-rpm.ino` — sketch uji mandiri 400–700 rpm.

**Dokumentasi:**
- `README.md` — dependency FastAccelStepper, fitur RPM, versi v0.5.2.
- `SUMMARY.md`, `PROGRESS.md`, `PROCESS.md`, `RELEASES.md` disinkronkan.
- Plan: `PLAN-pompa-rpm-400-700.md` (salinan juga di `/home/raihanP/.opencode/plan/pump-rpm-400-700.md`).

**Catatan teknis:**
- RPM hanya 400/500/600/700 (kelipatan 100).
- Kalibrasi: jalankan 20 rotasi pada RPM terpilih, ukur total ml, masukkan
  “Hasil 20 rev”; firmware menghitung `ml/rotasi = hasil / 20`.
- `ml/rotasi = 0` berarti belum dikalibrasi; proses dosing ditolak.
- Tanpa encoder, watchdog hanya mendeteksi posisi yang tidak maju, bukan
  stall mekanis.

---

## [2026-10-16] v0.5.1 — Exhaust fan lepas dari TB6612FNG, GPIO 18 jadi bebas

**Perubahan Hardware:**
- **Exhaust fan tidak lagi dikendalikan firmware.** Arus kipas melebihi kapasitas
  TB6612FNG (~1.2 A) sehingga kipas gagal terangkat dan PWM tidak berefek.
  Kipas kini disambung **langsung ke adaptor 12V dan menyala terus** selama mesin
  dinyalakan. Konsekuensi: E-stop **tidak lagi mematikan kipas**.

**Perubahan Kode:**
- `config.h` — `PIN_PWB_D2` (GPIO 18) dihapus, diganti komentar penjelasan.
  GPIO 18 kini bebas (kandidat sensor tangki proses / LED status).
- `dcmotor.h` — `fanSet()` dihapus; `dcmotorInit()` & `dcmotorStopAll()` tidak
  lagi menyentuh GPIO 18.
- `process.h` — `fanSet(false)` dihapus dari `aksiFase(FASE_SELESAI)` dan
  `prosesBatal()`.
- `auto-etching-firmware.ino` — `case HAL_KALIB_FAN` (±25 baris) dihapus;
  `fanSet(false)` di loop E-stop dihapus; navigasi Kalibrasi Motor
  disederhanakan (`kalibMotorIdx == 5` langsung ke Agitasi).
- `data.h` — enum `HAL_KALIB_FAN` & entri `namaHalaman[]` dihapus (18 item,
  tetap sinkron); `kalibDuty[7]` → `kalibDuty[5]`; komentar indeks motor diperbarui.
- `ui.h` — `gambarKalibFan()` (±32 baris) dihapus; `menuMotor[]` 7 → 6 item;
  case di `gambarLayar()` dihapus.
- `FW_VERSI` → `v0.5.1`.

**Fitur Baru:**
- **`test/tb6612fng/tb6612fng.ino`** — sketch uji mandiri modul TB6612FNG.
  Perintah Serial: `a 0-255`, `b 0-255`, `+`, `-`, `o`, `s` (urutan uji 14
  tahap), `h`. Non-blocking, PWM 20 kHz 8-bit, `ledcAttach/ledcWrite`.
  Tahap "step kecil" (duty 10/20/30) untuk mencari duty minimum motor.

**Dokumentasi:**
- `PROCESS.md` — tabel GPIO §1.4 diperbarui (GPIO 18 jadi bebas) + catatan v0.5.1;
  task "Exhaust fan otomatis" dibatalkan; `FAN_SELAMA_ETCHING` dihapus.
- `SUMMARY.md` — versi, pin map, dan daftar file disinkronkan.
- `PROGRESS.md` — checklist diperbarui + status lineage versi (§0).

**Catatan Teknis:**
- Motor DC yang dikendalikan firmware kini hanya 3: agitasi (GPIO 15),
  pompa air sabun (GPIO 16), pompa limbah (GPIO 17).
- Semua `fanSet()` / `PIN_PWB_D2` / `HAL_KALIB_FAN` sudah bersih dari kode.

---

## [2026-10-13] v0.8.3 — ❌ DIBATALKAN (lineage lama)

**Perbaikan Kritis:**
- **Agitator tidak resume setelah pause** (`process.h`): saat unpause, agitasi tidak
  di-restart → etching tidak rata. Fix: restore speed agitasi sesuai fase aktif.
- **Pause tidak extend fase duration** (`process.h`): `faseMulaiMs` tidak disesuaikan
  saat unpause → fase terpotong durasi jeda. Fix: `faseMulaiMs += jedaDurasi`.
- **Blocking delay(2000) di kalibrasi agitasi** (`.ino`): E-stop tidak responsif
  selama 2 detik. Fix: non-blocking pakai `agitasiTestMulaiMs` + cek di `loop()`.

**Prioritas Tinggi:**
- **Waste estimate salah** (`process.h`): `mlH2O` dihitung 3x → estimasi limbah
  terlalu besar, proses bisa gagal start. Fix: `mlHCl + mlH2O2 + mlH2O + 100 + 100`.
- **Cek sabun salah** (`process.h`): `kebutuhan[3] = p.mlH2O` — sabun dicek seolah-olah
  air. Fix: minimum tetap 100ml.
- **E-stop ISR memanggil digitalRead** (`aktuator.h`): ISR terlalu lama, berpotensi
  crash. Fix: ISR hanya set flag.
- **Buzzer blocking** (`aktuator.h`): `buzzerNada()` pakai `delay()` → seluruh sistem
  beku saat tone bermain. Fix: non-blocking state machine + `buzzerUpdate()` di loop.
- **stepperTest blocking** (`aktuator.h`): `delayMicroseconds()` selama 1.6 detik untuk
  1000 step. Fix: non-blocking `stepperTestMulai()`/`stepperTestUpdate()`.
- **hitungMl division by zero** (`sensor.h`): dua titik kalibrasi sama → NaN. Fix:
  guard `if (y0 == y1)`.

**Fitur Baru:**
- **`TONE_MULAI`** — nada mulai proses (440→660→880 Hz), dipanggil di `prosesMulai()`.
- **`TONE_PROSES_SELESAI`** — nada selesai proses (523→784→1047 Hz), dipanggil
  menggantikan `TONE_SELESAI` di `FASE_PENDINGINAN`.

**Lainnya:**
- `prosesMulai()`: guard `if (prosesJalan) return`
- `prosesBatal()`: reset `prosesMlTerpompa` & `watchdogSampaiMs`
- `dosisDurasiMs()`: hapus `+1` step (over-dosing)
- `teksTengah()`: fix unused variables
- `config.h`: `FW_VERSI` → v0.8.3

**Backup sebelum perubahan:** `backup/v0.8.2_2026-10-12/`

---

## [2026-10-12] v0.8.2 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `presetops.h`: `namaField` kembali 11 field — "Durasi total"
  (index 9) dan "Nama" (index 10)
- `presetops.h` (`ubahField`): field "Durasi total" KIRI/KANAN mengubah
  **±10 detik** — durasi total tidak bisa diubah langsung karena
  dijumlah dari seluruh fase, jadi levers-nya adalah fase etching
  (`agitasiDetik`, dibatasi 1–180 s)
- `ui.h` (`formatField`): kolom "Durasi total" menampilkan **estimasi
  mesin** `hitungTotalDurasiAktual()` dalam `mm:ss`, jadi selalu sesuai
  dengan apa yang akan dijalankan (bukan lagi angka yang tidak terpakai)
- `config.h`: `FW_VERSI` → v0.8.2

**Catatan:** perubahan "Durasi total" dan "Durasi agitasi" sama-sama
mengubah `agitasiDetik` — yang pertama langkah 10 detik, yang kedua
1 detik, supaya durasi etching bisa diatur kasar dan halus.
Column "Durasi total" hanya-baca (KIRI/KANAN tidak berlaku padanya? —
tidak, berlaku lewat agitasi 10 detik).

**Backup sebelum perubahan:** `backup/v0.8.1_2026-10-12/`

---

## [2026-10-12] v0.8.1 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- **Bug:** field "Durasi total" masih bisa diedit di halaman Edit dan
  menyimpan toast "Tersimpan", tetapi sejak v0.7.1 nilainya tidak lagi
  dibaca firmware apa pun (durasi proses dihitung dari Volume +
  Durasi agitasi) → hasil edit tidak berefek apa pun
- `data.h`: `namaField` 11 → 10 field; "Durasi total" dihapus,
  "Nama" menjadi index 9 (`JUMLAH_FIELD` = 10)
- `presetops.h`: `ubahField()` — `case 9` (totalDetik) dihapus;
  `case 7` (Durasi agitasi) kini dapat diedit di mode **auto** maupun
  manual (sebelumnya hanya mode manual), sehingga ada levers waktu
  etching yang benar-benar bekerja
- `ui.h` (`formatField`): `case 9` = Nama (bukan durasi total)
- Field `totalDetik` tetap ada di struct `Preset` (kompatibilitas data
  NVS) tapi tidak lagi tampil/diedit
- `config.h`: `FW_VERSI` → v0.8.1

**Cara mengatur durasi sekarang:** ubah **Volume** (menambah durasi
dosing) dan/atau **Durasi agitasi** (menambah durasi etching); hasil
terlihat di halaman Detail pada baris `Etching` dan `Durasi proses`.

**Backup sebelum perubahan:** `backup/v0.8.0_2026-10-12/`

---

## [2026-10-12] v0.8.0 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `ui.h` (`gambarPresetDetail`) — tampilan baru:
  - header: nama preset (kiri) + ukuran PCB `100x80mm` (kanan)
  - komposisi: baris HCl/H2O2/H2O + bar komposisi
  - **Volume total** = jumlah keseluruhan senyawa (`volumeTotal`)
  - **Etching** = waktu etching `agitasiDetik` format `mm:ss` + `Spd`
  - **Durasi proses** = estimasi mesin (`hitungTotalDurasiAktual`),
    menggantikan `totalDetik` (tebakan preset)
- **Fix scroll tersembunyi:** `terlihat()` kini memakai tinggi glyph
  (8px) sehingga baris terakhir tidak lagi terpotong;
  `maxScroll` di `.ino` memakai `(AREA_BAWAH - HEADER_H)` (sebelumnya
  `SCREEN_HEIGHT - HEADER_H` yang mengabaikan offset 14px → jangkauan
  scroll kurang, baris "Total" tak pernah terlihat);
  `DETAIL_CONTENT_H` 66 → 68, `DETAIL_SCROLL_STEP` 8 → 10
- `config.h`: `FW_VERSI` → v0.8.0

**Backup sebelum perubahan:** `backup/v0.7.1_2026-10-12/`

---

## [2026-10-12] v0.7.1 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- **Bug**: bar progres membagi `prosesAkumMs` dengan `p.totalDetik`
  (durasi tebakan preset) sementara proses selesai berdasarkan
  `prosesTotalMsAktual` → bar berhenti di ~50% dan tidak pernah 100%
- `ui.h` (`gambarProses`): penyebut bar & baris sisa memakai
  `prosesTotalMsAktual` (estimasi mesin, sama dengan nilai yang dipaksa
  di `FASE_SELESAI`) → bar mencapai 100%; `frac` di-clamp 0…1; sisa
  tidak negatif; header kanan kini `sisa/total` (mis. `02:22/02:22`)
- `process.h`: `hitungTotalDurasiAktual()` menghitung durasi fase
  dosing dari kalibrasi stepper — `dosisDurasiMs()` =
  `ml ÷ mlPerStep × 1.6 ms + 0.3 s` (sebelumnya 30 detik tetap per pompa)
- `config.h`: `FW_VERSI` → v0.7.1

**Catatan:** field `totalDetik` di preset tetap ada untuk tampilan di
halaman Detail, tetapi tidak lagi menentukan bar progres.

**Backup sebelum perubahan:** `backup/v0.7.0_2026-10-12/`

---

## [2026-10-12] v0.7.0 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- **P0-1 flag terkalibrasi** (`oled_pcb_baruuu.ino`): saat titik 100%
  (index terakhir) di-ENTER, `kalibrasiSensorValid[senyawa] = true` +
  toast "HCl terkalibrasi". Sebelumnya flag ini tidak pernah di-set
  sehingga `hitungMl()` selalu -1, `cekKetersediaan()` melewati
  semua sensor, dan halaman Cadangan tetap "uncal"
- **P0-2 wizard volume stepper** (`.ino`, `data.h`, `ui.h`): ENTER →
  pompa jalan N step → mode input "Volume keluar" (KIRI/KANAN ±5ml)
  → ENTER menghitung `ml/step = volume / step`, menyimpannya ke
  `kalibrasi.stepperMlPerStepX1000[]`, memperbarui `stepper[].mlPerStep`
  agar langsung dipakai dosing; BACK = batal; toast hasil
- **P0-3 edit kapasitas** (`.ino`, `ui.h`): baris "Kapasitas: <n>ml"
  jadi baris pertama halaman sensor (`kalibSensorTitik = -1`);
  KIRI/KANAN mengubah ±50ml (100–5000ml), per senyawa
- Navigasi halaman sensor: ATAS/BAWAH pilih baris; KIRI/KANAN
  ganti senyawa (atau ubah kapasitas bila baris Kapasitas dipilih)
- `ui.h`: switch senyawa menampilkan `*` bila senyawa sudah terkalibrasi
- `config.h`: `FW_VERSI` → v0.7.0

**Backup sebelum perubahan:** `backup/v0.6.1_2026-10-12/`

---

## [2026-10-12] v0.6.1 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `ui.h` (`gambarKalibSensor`): switch senyawa `[ < HCl > ]` dan
  kapasitas `1000ml` dipindahkan ke area tetap di y=24–33, di atas
  area scroll — sebelumnya ikut ter-scroll dan hilang saat titik
  75%/100% dipilih; 5 titik kini area scroll-nya mulai y=34
  (3 titik terlihat, auto-scroll mengikuti titik terpilih)
- `config.h`: `FW_VERSI` → v0.6.1

**Backup sebelum perubahan:** `backup/v0.6.0_2026-10-12/`

---

## [2026-10-12] v0.6.0 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `config.h`: `OLED_TOP_OFF` 20 → **14** (area logo y=14–63, 50px)
- `ui.h`: dengan 6px ekstra, halaman sekarang muat **4 baris**
  teks per layar (`BARIS_H` tetap 10, jeda 3px dipertahankan):
  - Proses: waktu y=28, bar y=45–52, status y=53 (disebar ulang)
  - Splash: judul 26, versi 46, booting 54 (disebar ulang)
  - Cfg Edit: `EDIT_VISIBLE` 3 → 4
  - Halaman berbasis scroll (Cadangan, Kalib Sensor/MQ-135/
    DS18B20/Agitasi, Tentang) menyesuaikan otomatis
- `config.h`: `FW_VERSI` → v0.6.0

**Backup sebelum perubahan:** `backup/v0.5.9_2026-10-12/`

---

## [2026-10-12] v0.5.9 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `ui.h`: `BARIS_H` 8 → **10** — jarak antar baris 1px → 3px
  (teks tidak lagi rapat); 3 baris per layar, sisanya via scroll
- `ui.h`: `uiScrollSet` membulatkan `uiScrollMaks` ke atas ke
  kelipatan `BARIS_H` supaya baris terakhir selalu bisa dimunculkan
- `gambarKalibStepper`: header ringkas 10px + 3 pompa (y=10/20/30)
  + 1 hint (y=37) — muat tanpa scroll
- `gambarError`: `[ENTER] reset` dipindah ke header kanan,
  sisa 3 baris (kode, pesan, Exhaust ON)
- `gambarKalibSensor`: switch senyawa disembunyikan saat ter-scroll
  keluar area
- `gambarCfgEdit`: `EDIT_VISIBLE` 4 → 3
- `gambarPresetDetail`: jarak baris `dy` 8 → `BARIS_H`
- Kalib DS18B20 & Agitasi: ditambahkan scroll (4 baris → 3 terlihat)
- `config.h`: `FW_VERSI` → v0.5.9

**Backup sebelum perubahan:** `backup/v0.5.8_2026-10-12/`

---

## [2026-10-12] v0.5.8 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `config.h`: `OLED_TOP_OFF` 10 → **20**
- `ui.h`:
  - `BARIS_H` = 8 (jarak baris standar font size 1, tidak dipadatkan lagi)
  - sistem scroll halaman: `uiScrollSet/uiScrollAtur/uiScrollKe` +
    `gambarScrollbar()`; scroll di-reset otomatis saat pindah halaman
  - **Scroll manual (ATAS/BAWAH):** Cadangan, Kalib MQ-135,
    DS18B20, Agitasi, Tentang
  - **Auto-scroll:** Kalib Sensor — ATAS/BAWAH tetap memilih titik,
    tampilan menggulir agar titik terpilih selalu terlihat
  - Kalib Stepper: nilai test pindah ke header ("Step N"), 3 pompa
    + 1 hint (muat tanpa scroll)
  - Proses: "sisa" pindah ke header; waktu y=33, bar 49–56,
    status (fase + %/ml) y=57
  - Cfg Edit: `EDIT_VISIBLE` 5 → 4 (gulir dengan ATAS/BAWAH)
- `oled_pcb_baruuu.ino`: ATAS/BAWAH = scroll pada halaman tersebut;
  di Cadangan, KIRI/KANAN sekarang memilih baris
- `config.h`: `FW_VERSI` → v0.5.8

**Backup sebelum perubahan:** `backup/v0.5.7_2026-10-12/`

---

## [2026-10-12] v0.5.7 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `config.h`: tambah `OLED_TOP_OFF 10`
- `ui.h`: seluruh UI digeser 10px ke bawah memakai offset
  terpusat — `TY(y)` untuk bentuk, `teksTengah/teksKanan/teksKiri`
  untuk teks (koordinat logis, y=0 = baris pertama yang terlihat);
  area logo jadi 54px (y=10–63) dan semua halaman dipadatkan agar
  tetap muat:
  - Kalib Sensor: header ringkas 10px + switch kotak putih penuh
    (y=20) + 5 titik jarak 7px (y=29/36/43/50/57)
  - Kalib Stepper: nilai test pindah ke header ("Step N"), 3 pompa
    + 3 hint (semua masuk layar)
  - Kalib MQ-135: 4 baris data + 2 hint; DS18B20 & Agitasi:
    2 baris data + 2 hint (semua hint dipendekkan &lt;=128px)
  - Proses: waktu/sisa/bar/fase dikompakan ke y=24/41/49/57
  - Cadangan: 5 baris jarak 8px; Cfg Edit: `EDIT_ROW_H` 10→8
  - Tentang: tambah baris "Sensor: GPIO 11/12/13/14/3"
- `config.h`: `FW_VERSI` → v0.5.7
- Fix compile: `teksKiri(F("PCB max"))` → `teksKiri("PCB max")` — di
  Arduino-ESP32 3.x `F()` menghasilkan `const __FlashStringHelper*`
  yang tidak otomatis bisa jadi `const char*`

**Backup sebelum perubahan:** `backup/v0.5.6_2026-10-12/`

---

## [2026-10-12] v0.5.6 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `ui.h` (`gambarKalibSensor`): dikembalikan ke
  layout v0.5.4 — switch senyawa (kotak putih penuh
  `[ < HCl > ]`) di y=14 di bawah header, 5 titik
  kalibrasi di bawahnya (y=25/33/41/49/57, jarak 8px).
  Layout v0.5.5 (switch di y=50, titik di atas)
  dicoba dan tidak dipakai; tersimpan di
  `backup/v0.5.5_2026-10-12/`
- `config.h`: `FW_VERSI` → v0.5.6

**Backup sebelum perubahan:** `backup/v0.5.5_2026-10-12/`

---

## [2026-10-12] v0.5.5 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `ui.h` (`gambarKalibSensor`): susunan halaman diubah —
  5 titik kalibrasi di atas (y=13/20/27/34/41, jarak 7px),
  selektor senyawa (kotak putih penuh `[ < HCl > ]`)
  dipindahkan ke **y=50 dari atas** sesuai permintaan,
  kapasitas "1000ml" tetap di kanan switch; konstanta
  `Y_SWITCH` dipakai supaya mudah diatur
- `config.h`: `FW_VERSI` → v0.5.5

**Backup sebelum perubahan:** `backup/v0.5.4_2026-10-12/`

---

## [2026-10-12] v0.5.4 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `ui.h` (`gambarKalibSensor`): selektor senyawa dari kotak
  berbingkai jadi **kotak putih penuh** (teks hitam) — lebih
  menonjol; digeser 1px ke bawah (y=14, tinggi 10px,
  baris titik tetap y=25/33/41/49/57, semua muat)
- `config.h`: `FW_VERSI` → v0.5.4

**Backup sebelum perubahan:** `backup/v0.5.3_2026-10-12/`

---

## [2026-10-12] v0.5.3 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `ui.h` (`gambarKalibSensor`): susun ulang layout atas yang
  sebelumnya padat — header jadi bar status ("Kalibrasi *" kiri,
  "Raw xxxx" kanan); senyawa ditampilkan dalam kotak berbingkai
  `[ < HCl > ]` (jelas sebagai switch, KIRI/KANAN) dengan kapasitas
  "1000ml" di kanannya; 5 titik kalibrasi mulai y=25 (semua
  terlihat, jarak lebih lega)
- `config.h`: `FW_VERSI` → v0.5.3

**Backup sebelum perubahan:** `backup/v0.5.2_2026-10-12/`

---

## [2026-10-12] v0.5.2 — ❌ DIBATALKAN (lineage lama)
**Perubahan:**
- `ui.h` (`gambarKalibSensor`): layout diperbaiki — titik 75%/100% &
  "Raw now" sebelumnya di bawah layar; kini: header `Kalib: <senyawa>`
  (senyawa jelas kelihatan, `< >` = ganti dgn KIRI/KANAN), baris info
  `Kap 1000ml Raw <nilai>`, 5 titik jarak 8px (semua terlihat),
  titik terpilih di-highlight
- `oled_pcb_baruuu.ino`: navigasi halaman kalibrasi sensor ditukar —
  KIRI/KANAN = ganti senyawa, ATAS/BAWAH = scroll titik kalibrasi
  (sebelumnya terbalik)
- `storage.h`: kapasitas default tangki 500 ml → 1000 ml
- `config.h`: `FW_VERSI` → v0.5.2

**Backup sebelum perubahan:** `backup/v0.5.1_2026-10-12/`

---

## [2026-10-12] v0.5.1 — ❌ LINEAGE LAMA — (lihat catatan lineage di atas)
**Perubahan:**
- `config.h`: hapus `MPR121_ADDR`; tambah `PIN_SENSOR_HCL/H2O2/H2O/SABUN/LIMBAH`
  (GPIO 11/12/13/14/3); pin TB6612FNG jadi 15 (agitasi), 16 (sabun), 17 (limbah),
  18 (fan); hapus `PIN_TB_STBY` (STBY di-tie 3.3V di hardware); tambah
  `SENSOR_ANALOG`, `SENSOR_ACTIVE_LOW`, `SENSOR_HYST`; `FW_VERSI` → v0.5.1
- `sensor.h`: ditulis ulang — baca 5 sensor langsung dari GPIO (analog ADC 12-bit
  via `analogRead`, atau float switch digital), rata-rata 4 sampel + histeresis;
  tambah `sensorKosong()` & `sensorPenuh()`
- `aktuator.h`: `pompaDcInit()` tidak lagi mengatur STBY (tie 3.3V)
- `process.h`: watchdog timeout dosing (estimasi 2× durasi langkah + 30 s); dosing
  error `SUMBER_HABIS` kalau sumber habis saat dosing; drain berhenti lebih awal
  saat limbah penuh; tracking ml terpompa; dosing dengan ml=0 langsung dilewati
  (perbaikan hang)
- `ui.h`: layar proses menampilkan ml terpompa/target saat dosing; halaman Tentang
  tidak lagi mencantumkan alamat 0x5A
- `storage.h`: default titik kalibrasi disesuaikan ke skala ADC 12-bit
- `oled_pcb_baruuu.ino`: pesan error sensor level (bukan MPR121)
- Fix compile: `analogSetAttenuation(ADC_ATTEN_DB_11)` dihapus (enum
  tidak tersedia di Arduino-ESP32 3.3.11); pakai default 11 dB

**Asumsi (dari "Pertanyaan Terbuka" dokumen):**
- Sensor level = analog (default `SENSOR_ANALOG=1`); ubah ke 0 untuk float
  switch digital (`SENSOR_ACTIVE_LOW=1` = LOW saat penuh)
- STBY TB6612FNG di-tie 3.3V (GPIO15 dipakai untuk agitasi)
- MS1/MS2/MS3 DRV8825 di-hardwire; `STEPPER_MICROSTEP` tetap 16 — dokumen
  menyebut 1/36, tapi DRV8825 maksimum 1/32; perlu konfirmasi hardware
- Drain "tangki proses kosong" belum terdeteksi (tidak ada sensor tangki proses):
  drain tetap pakai timeout + deteksi limbah penuh

**Backup sebelum perubahan:** `backup/v0.5.0_2026-10-12/`

---

## [2026-10-12] v0.5.0 — ❌ LINEAGE LAMA — (lihat catatan lineage di atas)
Snapshot kondisi firmware saat ini, dicatat sebagai baseline sebelum perubahan lanjutan.

**File:** `oled_pcb_baruuu.ino` + `aktuator.h`, `config.h`, `data.h`, `input.h`, `presetops.h`, `process.h`, `sensor.h`, `storage.h`, `ui.h`

**Fitur yang sudah ada:**
- UI 6 tombol (PCF8574 @0x20) + OLED SSD1306 128×64 (@0x3C), animasi list 200 ms
- Preset (maks 10): tambah/edit/duplikat/hapus; mode dosing auto (47/24/29 %) atau manual; volume 50–1000 ml; mode agitasi auto/manual; speed agitasi 1–5
- Proses otomatis 16 fase: cek level → dosing HCl/H2O2/H2O (stepper non-blocking) → jeda campur → etching (agitasi) → drain → bilas sabun → bilas air → pendinginan → selesai
- Kalibrasi 5 menu: sensor level 5 titik, stepper ml/step, MQ-135 baseline, DS18B20 offset, agitasi duty min
- Penyimpanan NVS (Preferences) + CRC untuk preset & kalibrasi, format versi 2
- E-stop (GPIO8) via interrupt, abort proses, halaman ERROR
- Buzzer nada, LED strip, exhaust fan

**Diketahui belum sesuai `etching-otomatis.md` (rencana lanjutan):**
- Sensor level masih MPR121 (@0x5A), belum GPIO langsung 11/12/13/14/3
- Pin TB6612FNG masih lama: agitasi 11, limbah 12, sabun 13, fan 14, STBY 15
- `STEPPER_MICROSTEP` = 16 (dokumen: 1/36 → 7200 step/putaran)
- Drain hanya timeout tetap (15 s/10 s), belum berdasarkan level tangki proses kosong / limbah penuh
- Belum ada watchdog timeout per fase (dosing bisa hang kalau pompa macet)
- Layar proses belum menampilkan ml terpompa
- DS18B20 masih placeholder (belum pakai OneWire)
- `HAL_PLACEHOLDER` masih ada tapi tak terpakai

**Backup:** `backup/v0.5.0_2026-10-12/`
