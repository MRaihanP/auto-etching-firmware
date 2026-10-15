# PROGRESS — Checklist Implementasi Firmware Etching Otomatis

Firmware: **Etching v0.5.0** · Board: **ESP32-S3 DevKitC**
Rencana teknis lengkap: `PROCESS.md`

Legenda: ✅ **Sudah ada & berfungsi** · ⚠️ **Ada tapi belum fungsional/terpakai** · ⬜ **Belum ada**

---

## 1. GPIO & Hardware

- [x] ✅ **Migrasi stepper DRV8825 → TMC2209 (v0.6.0)** — pin map baru di `config.h`: `PIN_TMC_EN=1`, `PIN_TMC_PDN=2` (1 pasang, **shared 3 driver**), `PIN_STEP1/2/3 = 38/40/42`, `PIN_DIR1/2/3 = 48/39/41`. `PIN_SLP1/2/3` **dihapus**. Polaritas: `TMC_EN_AKTIF_LOW=1` (EN aktif LOW — kebalikan dari SLP DRV8825), reset pulse PDN `TMC_PDN_RESET_US=5`. `motorInit()` urut: pin mode → `stepperSleep(true)` → pulse reset PDN → `stepperSleep(false)` → `hitungLangkahPerMl()`. Mikrostep **1/16 → `STEP_PER_REV 3200`** (rumus umum `200 × 2^n`). Driver bisa dimatikan total: `stepperSleep(bool)` + `pompaMatikanTotal()` (EN = HIGH) dipakai `semuaAktuatorMati()`. GPIO **21 & 47 kini bebas** (dari `PIN_DIR3`/`PIN_STEP3` lama).
- [x] ✅ **Kalibrasi dosing wajib diulang** — `langkahPerMl` berubah dari basis 7200 (DRV8825 1/36) ke 3200 step/rev (TMC2209 1/16). Nilai kalibrasi lama di NVS blob `"kal3"` **tidak valid** — jalankan ulang `Kalibrasi → Motor HCl/H2O2/Air`.
- [x] ✅ I2C SDA=4, SCL=5, 400 kHz (`config.h:5-7`, `.ino:601-602`)
- [x] ✅ OLED SSD1306 0x3C 128×64 terinisialisasi (`.ino:618`)
- [x] ✅ PCF8574 0x20 — 6 tombol, inisialisasi tulis `0xFF` (`.ino:604`)
- [x] ✅ Error I2C dideteksi & dicatat ke Serial (`.ino:604,618`)
- [x] ✅ 5 GPIO sensor level (11/12/13/14/3) `INPUT` (`sensor.h:36-38`)
- [x] ✅ 4 PWM TB6612FNG (15/16/17/18) (`config.h:79-82`, `dcmotor.h`)
- [x] ✅ Buzzer pasif GPIO 6 via LEDC (`buzzer.h:14-18`)
- [x] ✅ **LED status WS2812 GPIO 7** — file baru `led_strip.h`, 8 mode (Boot/Idle/Proses/Jeda/Selesai/Error/E-stop/Warning), animasi non-blocking di `ledStripUpdate()`, integrasi di `process.h` (`aksiFase`, `prosesToggleJeda`, `prosesBatal`, `prosesKeluarSelesai`) + `.ino` (`setup()` init, `loop()` update, handler E-stop)
- [x] ✅ DS18B20 **x3 pada satu bus 1-Wire GPIO 10** (`PIN_DS18B20` = 10, `config.h`), 3 DeviceAddress hardcode (`sensor.h:119-123`), non-blocking `setWaitForConversion(false)`, offset kalibrasi per blok `suhuOffsetX10[3]` + pilih address `kalibSuhuAddr[3]`, halaman kalibrasi `HAL_KALIB_SUHU` (`ui.h:gambarKalibSuhu()`) & `HAL_KALIB_UDARA` (`ui.h:gambarKalibUdara()`), NVS blob `"kal2"` versi 2 — **catatan:** `PIN_DS_CAIRAN=45` & `PIN_DS_MOTOR=36` di `config.h` **tidak dipakai** (sisa desain lama 3-bus)
- [x] ✅ MQ-135 analog GPIO 9 + divider R1=10k/R2=15k (`config.h:109-114`, `sensor.h:210-228`), rekam acuan `mq135Normal`/`mq135Kotor` di halaman kalibrasi
- [x] ✅ E-stop GPIO 8 `INPUT_PULLUP` (`.ino:616`)
- [x] ✅ **RGB WS2812 indikator per sensor (5 sensor)** — aktif di `config.h` (`SENSOR_LED_ENABLE 1`, `PIN_LED_SENSOR 47` — bebas sejak migrasi TMC2209). `led_strip.h`: `ledSensorInit()`/`ledSensorUpdate()`, 5 pixel, warna hijau ≥20% / kuning 5–19% / merah <5%, tiap komponen warna diskalakan level sehingga kecerahan = bar liquid, throttle 250 ms + redraw hanya bila sensor berubah ≥2%. Non-blocking, hook di `.ino` `setup()` + `loop()`.

---

## 2. Modul UI (`ui.h`, `.ino`, `input.h`, `data.h`)

- [x] ✅ Menu utama 4 item: Pilih Preset / Konfigurasi / Level Cairan / Kalibrasi (`data.h:38`)
- [x] ✅ Header + list dengan animasi scroll (smooth) (`ui.h:47-94`)
- [x] ✅ Toast pesan singkat (`ui.h:833-848`)
- [x] ✅ Dialog konfirmasi (simpan/batal/hapus) (`ui.h:279`)
- [x] ✅ Indikator scroll + panah atas/bawah
- [x] ✅ Halaman Daftar Preset + Detail Preset (scroll 5 px)
- [x] ✅ Halaman Konfigurasi: List → Aksi (Edit/Duplikat/Hapus) → Edit 9 field
- [x] ✅ Halaman Level Cairan: 5 sensor, persen + bar progres
- [x] ✅ Halaman Proses: nama preset, timer MM:SS, progress bar, sisa waktu, status fase
- [x] ✅ 6 tombol PCF8574 + debounce 40 ms + blocking first-press (`input.h`)
- [x] ✅ Navigasi ATAS/BAWAH/KIRI/KANAN/ENTER/BACK di semua halaman
- [x] ✅ Render dirty-flag + interval 20 ms (tidak render idle) (`.ino:658`)
- [x] ✅ Tombol dibaca ulang setelah `oled.display()` (`.ino:671`)
- [x] ✅ Kompensasi baris atas OLED rusak (`UI_Y_OFFSET` = 15 px) (`ui.h:13-46`)
- [x] ✅ Konversi suhu DS18B20 non-blocking (`setWaitForConversion(false)`) (`sensor.h:138`)
- [x] ✅ Tampilkan nomor versi firmware di OLED — `FW_VERSI` di pojok kanan atas halaman Menu Utama (`ui.h:gambarMenuUtama()`, `areaAtas` list dinaikkan ke 10 px supaya tidak menimpa baris versi)
- [x] ✅ Level Cairan: status ISI/KOSONG + ambang — `sensor.h:sensorStatusUpdate()` memakai `AMBANG_MARGIN` (10%) naik & `AMBANG_HYST` (3%) turun supaya tidak berkedip di ambang; `sensorAdaCairan(i)` dipakai `ui.h:gambarLevel()` (label `ISI` blok terbalik / `KOSONG`, bar geser ke kanan)
- [x] ✅ Halaman `HAL_PLACEHOLDER` & `gambarPlaceholder()` dihapus —enum `data.h`, `namaHalaman[]`, `case` di `ui.h` & `.ino`, fungsi `gambarPlaceholder()` tidak pernah terjangkau sejak v0.5.2
- [x] ✅ Halaman Kalibrasi Agitasi/Fan tidak lagi pakai `delay()` — `mulaiTestMotor(jenis, durasiMs)` + `upkeepKalibrasiMotor()` dijadwalkan via `millis()`, dicek di `loop()`. `kalibMotorRunning` yang tadinya di-set tapi tidak pernah dibaca kini jadi flag aktif. BACK saat test sedang jalan mematikan motor
- [ ] ⬜ **Halaman ALARM baru** (`HAL_ALARM`) untuk semua kondisi gagal — butuh `prosesGagal()` + `KodeError` dari Bab 6
- [ ] ⬜ Layar proses: progress **per fase** (bukan persen total) — butuh durasi per fase
- [ ] ⬜ Layar proses: panel ml terpompa per bahan (HCl/H2O2/H2O) — butuh `pompaProgres(i)` dari Bab 3
- [ ] ⬜ Layar proses: ikon status aktuator (agitasi/drain/fan/soap) — butuh status aktuator live
- [ ] ⬜ Layar proses: badge peringatan gas MQ-135 / suhu tinggi — butuh ambang proteksi dari Bab 4
- [ ] ⬜ Layar proses: tampilkan siklus agitasi ke berapa — butuh `prosesSiklus` dari Bab 4
- [ ] ⬜ Badge permanen "E-STOP" di header saat latch aktif — butuh latch `estopAktif` dari Bab 6
- [ ] ⬜ Layar selesai: ringkasan total ml terpompa + durasi aktual — butuh `totalMlTerpompa`

---

## 3. Modul Dosing (`process.h` § DOSING, `motor.h`, `presetops.h`)

- [x] ✅ Driver stepper non-blocking (puls STEP tiap `LANGKAH_INTERVAL_US`) (`motor.h:motorUpdate()`)
- [x] ✅ Konversi ml → jumlah langkah (`targetLangkah = ml × langkahPerMl`) (`motor.h:pompaMulai()`)
- [x] ✅ 3 pompa (HCl/H2O2/H2O) dijalankan **paralel** saat fase DOSING (`process.h:aksiFase()`)
- [x] ✅ `pompaMulai()` / `pompaStopAll()` / `pompaSelesai()` tersedia
- [x] ✅ `pompaMatikanTotal()` — stop + `stepperSleep(true)` (EN = HIGH) supaya driver TMC2209 benar-benar tidur
- [x] ✅ Hitung otomatis %H2O dan ml tiap bahan dari %HCl/%H2O2 + volume (`presetops.h:hitungUlang()`)
- [x] ✅ Validasi rentang semua field preset (`presetops.h:validasiPreset()`)
- [x] ✅ Halaman kalibrasi stepper: uji pompa + set ml/rotasi + simpan ke NVS (`HAL_KALIB_STEPPER`)
- [x] ✅ `langkahPerMl = STEP_PER_REV / mlPerRotasi` — `hitungLangkahPerMl()` memakai `STEP_PER_REV` (3200, TMC2209 1/16) bukan asumsi tetap 7200
- [x] ✅ `kalibStepperMl[]` **dipakai** `pompaMulai()` — dibaca `muatKalibStepper()` saat boot, dihitung di `motorInit()`, ditulis `simpanKalibStepper()` saat BACK dari halaman kalibrasi
- [x] ✅ Simpan kalibrasi ml/rotasi ke NVS — blob `"kal3"` (`DataKalib3`: versi + `float stepperMl[3]` + CRC32), terpisah dari `"kal"`/`"kal2"`
- [x] ✅ `STEP_INTERVAL_US` 400 → **1200 µs** (aman 3 pompa paralel, ~830 Hz)
- [ ] ⬜ Fase `FASE_CEK` pre-flight: validasi sensor & volume tangki
- [ ] ⬜ Hitung ml yang dibutuhkan = target − sisa cairan di tangki (dari sensor)
- [ ] ⬜ Cek ketersediaan cairan → `ERR_LEVEL_KURANG` sebelum pompa nyala
- [ ] ⬜ Selesai dosing berbasis kondisi (saat ini masih berbasis fraksi waktu)
- [ ] ⬜ Deteksi pompa macet (langkah tidak bertambah > 3 s)
- [ ] ⬜ Watchdog timeout dosing (90 s)
- [ ] ⬜ Volume tangki (`VOL_TANGKI_*`) sebagai data, bukan `#define` statis

---

## 4. Modul Etching (`process.h` § ETCHING, `dcmotor.h`)

- [x] ✅ Motor agitasi PWM pada speed 1–5 (map 80–255) (`dcmotor.h:22`)
- [x] ✅ Agitasi menyala di fase ETCHING dan RINSING (`process.h:36,44`)
- [x] ✅ Agitasi mati di DRAINING, DOSING, SELESAI, dan saat batal
- [x] ✅ Field `agitasiSpeed` dapat diubah di preset (1–5)
- [x] ✅ Field `agitasiDetik` dapat diubah di preset (1–60 s)
- [ ] ⚠️ `agitasiDetik` **tidak dipakai proses** — hanya tampil di UI (`presetops.h:42`, `ui.h:263`)
- [ ] ⬜ Siklus agitasi periodik: ON N detik → OFF N detik → ulangi
- [ ] ⬜ Exhaust fan otomatis menyala selama ETCHING (asap HCl + panas)
- [ ] ⬜ Exhaust fan menyala selama RINSE_SABUN (busa)
- [ ] ⬜ Ramp-up speed agitasi 1 s (hindari shok motor)
- [ ] ⬜ Proteksi suhu: `sensorSuhu(1)` > 70 °C → turunkan speed; > 85 °C → gagal
- [ ] ⬜ Proteksi gas: `mq135PPM()` > ambang → peringatan di UI
- [ ] ⬜ Auto-stop /pause saat suhu/gas di luar batas
- [ ] ⬜ Simpan &Remember durasi fase etching per preset (budget waktu)

---

## 5. Modul Draining & Rinsing (`process.h` § DRAIN/RINSE)

- [x] ✅ Pompa Limbah (TB6612FNG #2 PWA) PWM on/off (`dcmotor.h:28`)
- [x] ✅ Pompa Limbah menyala di fase DRAINING (`process.h:40`)
- [x] ✅ Pompa Limbah mati saat SELESAI & batal
- [x] ✅ Rinsing: pompa air (stepper #3) 100 ml + agitasi (`process.h:43-45`)
- [x] ✅ `fanSet(false)` saat SELESAI
- [ ] ⚠️ DRAINING/RINSING **selesai berbasis fraksi waktu total preset** (75%/88%/100%), bukan durasi per sub-fase
- [ ] ⚠️ Pompa air **(belum pernah dipakai di proses**, hanya di kalibrasi (`process.h:50,113`)
- [ ] ⬜ Durasi tetap per sub-fase: `T_DRAINING_MS`, `T_RINSE_AIR_MS`, `T_RINSE_SABUN_MS`, `T_DRAIN_AKHIR_MS`
- [ ] ⬜ Guard overfill: `sensorPersen[LIMBAH] > 95%` → stop + alarm
- [ ] ⬜ Pompa air sabun menyala di fase RINSE_SABUN (belum ada sama sekali)
- [ ] ⬜ Deteksi "tangki proses kosong" (opsional, bila sensor ditambahkan)
- [ ] ⬜ Histeresis/debounce level agar pompa tidak thrash
- [ ] ⬜ Volume bilasan air & sab customize dari preset

---

## 6. Modul Emergency Stop & Keselamatan (`process.h` § ESTOP)

- [x] ✅ Baca E-stop GPIO 8 tiap loop (`.ino:634`)
- [x] ✅ E-stop LOW → `prosesBatal()` + toast "E-STOP!" (`.ino:635-638`)
- [x] ✅ E-stop LOW → matikan agitasi, pompaSabun, pompaLimbah, fan, semua pompa (`.ino:639-643`)
- [x] ✅ Semua aktuator mati saat FASE_SELESAI (`process.h:47-53`)
- [x] ✅ Semua aktuator mati saat proses dibatalkan (`.ino:110-116`)
- [x] ✅ Dialog konfirmasi batal saat proses berjalan (`ino:121-141`)
- [x] ✅ Log Serial untuk setiap transisi fase & pembatalan
- [ ] ⬜ Estop dipindah ke **baris pertama `loop()`** (saat ini setelah `tanganiTombol()`, `prosesUpdate()`, `motorUpdate()`, `sensorUpdate()`)
- [ ] ⬜ Latch `estopAktif` — tidak auto-reset saat pin kembali HIGH
- [ ] ⬜ Reset latch hanya lewat BACK di `HAL_ALARM`, dan hanya bila pin HIGH
- [ ] ⬜ Fungsi tunggal `semuaAktuatorMati()` (saat ini 4× duplikasi di 3 tempat)
- [ ] ⬜ Cut power driver stepper (TMC_PDN LOW) saat stop
- [ ] ⬜ `prosesGagal(kode)` sebagai satu-satunya jalan keluar kondisi abnormal
- [ ] ⬜ Timeout/watchdog per fase (DOSING, DRAINING, RINSE)
- [ ] ⬜ Buzzer nada alarm (berulang) berbeda dari nada "selesai"
- [ ] ⬜ Halaman alarm + instruksi pemulihan
- [ ] ⬜ Jeda (`prosesToggleJeda`) ikut mematikan output, bukan hanya timer
- [ ] ⬜ Deteksi kegagalan sensor saat proses (DS18B20/touch) → abort aman
- [ ] ⬜ Fail-safe saat OLED/PCF8574 hilang (hanya E-stop berfungsi, beri peringatan)

---

## 7. Sensor & Kalibrasi (`sensor.h`, `storage.h`)

- [x] ✅ Baca 5 sensor level via `touchRead()` (strip nikel non-contact) (`sensor.h:60`)
- [x] ✅ Moving average window 8 untuk redam noise (`sensor.h:13,59`)
- [x] ✅ Konversi raw → persen dengan clamp 0–100 (`sensor.h:74`)
- [x] ✅ Kalibrasi 0% & 100% per sensor dari UI (`ino:412-460`)
- [x] ✅ Kalibrasi level disimpan ke NVS + CRC + auto-reject bila tidak valid (`storage.h:87`)
- [x] ✅ Baseline default 113000/156000 bila kalibrasi belum ada (`storage.h:120`)
- [x] ✅ Halaman Kalibrasi → 4 submenu (Motor, Sensor Level, Sensor Suhu, Sensor Udara)
- [x] ✅ Kalibrasi MQ-135: rekam acuan udara bersih & kotor (`ino:388-408`)
- [x] ✅ Kalibrasi suhu: pilih sensor per blok + offset ±0,1 °C (`ino:348-373`)
- [x] ✅ Kalibrasi agitasi & fan: uji motor/kipas (`ino:292-344`)
- [x] ✅ Kalibrasi DC: atur duty 0–255 dan catat ml hasil uji (`ino:246-289`)
- [x] ✅ Log diagnostik `[SENSOR]`/`[SUHU]`/`[STORE]` ke Serial
- [x] ✅ **Address DS18B20 tidak lagi hardcode** (v0.6.3) — `sensorSuhuInit()` memindai bus 1-Wire lewat `getAddress()` lalu menyimpan hasilnya ke NVS blob `"kal5"` (`DataKalib5`: `addr[3][8]` + bitmask valid + CRC32). Boot berikutnya memakaitaddress dari NVS supaya urutan blok tidak berubah. Family code 0x28 diverifikasi saat muat — blob rusak ditolak lalu dipindai ulang. Slot kosong ditandai `sensorAddressValid[s] = false` → `sensorSuhuRaw()` mengembalikan -127 → UI tampil `--.-C`. Address dicetak ke Serial saat boot pertama untuk dicocokkan dgn label fisik Motor/Komponen/Chamber
- [x] ✅ `kalibSuhuAddr[]` & `suhuOffsetX10[]` **sudah disimpan ke NVS** (v0.6.4) — `DataKalib2` versi 2 memakai `addrSensor[3]` + `suhuOffsetX10[3]`. **Perbaikan v0.6.4:** sebelumnya status ini ditandai ✅ padahal `simpanKalib2()` tidak pernah dipanggil — struct-nya sudah ada sejak v0.6.0 tapi tidak ada pemanggilnya (bug warisan merge `2c84273`). Sekarang `simpanKalib2()` dipanggil saat BACK dari `HAL_KALIB_SUHU`
- [x] ✅ `mq135Normal`/`mq135Kotor` **sudah disimpan ke NVS** (v0.6.4) — keduanya field di `DataKalib2` versi 2, ditulis saat BACK dari `HAL_KALIB_UDARA` (perbaikan yang sama). *Sisa:* belum dipakai proteksi gas (itu item Bab 4)
- [x] ✅ `kalibMotorRunning` **sudah dibaca** (v0.6.1) — `mulaiTestMotor()` + `upkeepKalibrasiMotor()` menjadwalkan auto-stop test agitasi/fan via `millis()`, plus BACK mematikan motor lebih dulu. Lihat §2 Modul UI
- [x] ✅ `kalibDuty[]` & `kalibMl[]` **sudah disimpan ke NVS** (v0.6.2) — blob `"kal4"` (`DataKalib4`: `duty[7]` + `mlX10[2]`, versi + CRC32), ditulis `simpanKalib4()` saat BACK dari `HAL_KALIB_DC` / `HAL_KALIB_AGITASI` / `HAL_KALIB_FAN`, dibaca `muatKalib4()` di `setup()`. Default 128 (≈50% duty) bila blob belum ada
- [x] ✅ Auto-stop motor kalibrasi setelah N ms (ganti `delay()`) — selesai v0.6.1 (`upkeepKalibrasiMotor()`), lihat catatan `kalibMotorRunning` di atas
- [ ] ⬜ Terapkan `kalibDuty[]` ke motor — saat ini hanya dipakai tampilan (`ui.h`); `dcmotor.h` masih `agitasiSet()` memetakan speed 1–5 → PWM 80–255 tetap, `fanSet()` on/off tetap. Butuh `agitasiSetDuty()`/`fanSetAt()` yang sudah ada di `dcmotor.h` (**file milik Orang A**)
- [ ] ⬜ Simpan semua parameter kalibrasi ke NVS — `kalibDuty`/`kalibMl` selesai v0.6.2, `kalibSuhuAddr`/`suhuOffsetX10`/`mq135*` selesai v0.6.0, `kalibStepperMl` selesai v0.5.7
- [x] ✅ Deteksi sensor touch mati (v0.6.5) — `sensor.h:sensorMatiUpdate()` menandai strip yang `touchRead()`-nya beku di **0** selama `SENSOR_BEKU_MS` (60 detik); `touchRead()` yang sehat selalu non-nol, jadi 0 beku ≈ kabel putus / strip copot. Log `[SENSOR] <nama> MATI` sekali saat berubah, dan `[SENSOR] <nama> pulih` saat hidup lagi (tanpa perlu reset). `ui.h:gambarLevel()` menampilkan `DEAD` menggantikan persen + menyembunyikan isi bar, karena angka 0% yang palsu lebih berbahaya daripada tidak menampilkan apa pun. **Batas deteksi:** strip yang sudah tidak sensitif (buildup karat, HDPE tebal) masih bergerak sedikit dan *tidak* terdeteksi — hanya kasus beku total yang tertangkap. Penting untuk `FASE_CEK` Bab 3 agar tidak memercayai level yang salah

---

## 8. Penyimpanan & Data (`storage.h`, `presetops.h`, `data.h`)

- [x] ✅ Maks 10 preset (`MAKS_PRESET`)
- [x] ✅ 5 preset default saat pertama kali jalan (`data.h:25`)
- [x] ✅ CRUD: tambah, duplikat, hapus dengan pergeseran array (`presetops.h:83-110`)
- [x] ✅ Nama preset unik otomatis "Preset N" (`presetops.h:71`)
- [x] ✅ Simpan preset ke NVS sebagai satu blob + versi + CRC32 (`storage.h:26`)
- [x] ✅ Muat preset: reject bila versi beda / CRC rusak / data tak valid (`storage.h:45`)
- [x] ✅ Auto-reset ke default bila data korup (`storage.h:70`)
- [x] ✅ Simpan kalibrasi pompa (ml/rotasi) ke NVS — blob `"kal3"` (`DataKalib3`: `float stepperMl[3]` + versi + CRC32), ditulis `simpanKalibStepper()` saat BACK dari `HAL_KALIB_STEPPER` (v0.5.7)
- [x] ✅ Simpan offset suhu & acuan MQ-135 ke NVS — blob `"kal2"` (`DataKalib2` versi 2: `addrSensor[3]` + `suhuOffsetX10[3]` + `mq135Normal`/`mq135Kotor`, versi + CRC32), ditulis `simpanKalib2()` saat BACK dari `HAL_KALIB_SUHU` / `HAL_KALIB_UDARA` (v0.6.4)
- [x] ✅ Simpan duty/kecepatan motor per kalibrasi ke NVS — blob `"kal4"` (`DataKalib4`: `duty[7]` + `mlX10[2]`, versi + CRC32), ditulis `simpanKalib4()` saat BACK dari `HAL_KALIB_DC`/`HAL_KALIB_AGITASI`/`HAL_KALIB_FAN` (v0.6.2)
- [ ] ⬜ Backup/restore preset via Serial (export/import)

---

## 9. Infrastruktur Firmware

- [x] ✅ Semua modul non-blocking (`loop()` berbasis `millis()`), tanpa `delay()` di jalur proses
- [x] ✅ Pisah logika proses (`prosesUpdate()`) dari render UI
- [x] ✅ Log debug lengkap via `dbg.h` dengan prefix per modul (`[PROSES]`, `[POMPA]`, `[KALIB]`, …)
- [x] ✅ Struktur file modular (`config/input/data/presetops/sensor/storage/motor/dcmotor/buzzer/process/ui`)
- [x] ✅ Backup versi di folder `backup/` per rilis
- [ ] ⬜ Watchdog timer hardware (`esp_task_wdt`) sebagai jaring pengaman
- [ ] ⬜ Deteksi & pemulihan I2C (PCF8574 hang → reset bus)
- [ ] ⬜ Kontrol via Serial (start/stop/preset/status) untuk debug
- [ ] ⬜ Logging proses ke file / upload via WiFi (opsional)
- [ ] ⬜ Cleaning `#define` mati di `config.h`: `INTERVAL_SENSOR_MS`, `SENSOR_AVG_N`, `SENSOR_AKTIF_LOW`, `SENSOR_TOUCH_READ`, `TOUCH_DEBOUNCE`, `TOUCH_DELTA_*`, `TOUTH_KALIB_SAMPEL`, `SENSOR_LOG_PERIOD_MS`, `AMBANG_MARGIN`, `AMBANG_HYST`

---

## 10. Dokumentasi

- [x] ✅ `README.md`, `GUIDE.md`, `SUMMARY.md`, `LOG.md`
- [x] ✅ `etching-otomatis.md` — rencana pengembangan + pin map awal
- [x] ✅ `water-level-sensor-plan.md` — rencana sensor level non-contact
- [x] ✅ `PROCESS.md` — rencana detail 5 modul proses + cara kerja + pin map TMC2209
- [x] ✅ `PROGRESS.md` — checklist status implementasi (dokumen ini)
- [ ] ⬜ Update `SUMMARY.md` agar sinkron dengan pin map TMC2209 & fitur baru
- [ ] ⬜ Update `LOG.md` / naikkan versi firmware untuk setiap fitur baru

---

## 11. Uji Hardware yang Masih Diperlukan

- [ ] ⬜ Uji 5 sensor touch di jerican kosong vs terisi (repeatability < 5%)
- [ ] ⬜ Verifikasi address DS18B20 fisik cocok dengan `sensor.h:119`
- [ ] ⬜ Timbang air: 3 pompa × 3 ulangan → error ≤ 5% dari target
- [ ] ⬜ Uji driver TMC2209 3 channel (STEP/DIR/EN/PDN) dengan pin map baru
- [ ] ⬜ Uji respons E-stop (target < 5 ms dari_LOW ke semua output mati)
- [ ] ⬜ Uji drain: volume larutan terbuang habis dalam `T_DRAINING_MS`
- [ ] ⬜ Uji overfill guard (isi tangki limbah hampir penuh)
- [ ] ⬜ Uji siklus agitasi (ON/OFF berulang) + fan
- [ ] ⬜ Uji stabilitas level readout saat kabel AWG30 30 cm
- [ ] ⬜ **Seluruh uji wajib pakai air biasa dulu**, baru larutan encer

---

## Ringkasan

| Kategori | ✅ | ⚠️ | ⬜ |
|---|---|---|---|
| GPIO & Hardware | 13 | 0 | 0 |
| Modul UI | 20 | 0 | 8 |
| Modul Dosing | 14 | 0 | 6 |
| Modul Etching | 5 | 1 | 6 |
| Modul Draining & Rinsing | 5 | 3 | 6 |
| Modul Emergency Stop | 8 | 0 | 10 |
| Sensor & Kalibrasi | 20 | 0 | 3 |
| Penyimpanan & Data | 10 | 0 | 2 |
| Infrastruktur | 5 | 0 | 5 |
| Dokumentasi | 6 | 0 | 2 |
| Uji Hardware | 0 | 0 | 10 |

**Total: 106 ✅ · 4 ⚠️ · 57 ⬜**

Status keseluruhan: **prototype berfungsi penuh pada jalur happy-path** (menu,
preset, dosing, agitasi, drain, e-stop dasar). Akurasi dosing **sudah memakai
hasil kalibrasi tersimpan di NVS** (`langkahPerMl` dari `kalibStepperMl[]` ×
`STEP_PER_REV`), tetapi **harus dikalibrasi ulang** setelah migrasi TMC2209.
Yang masih belum ada: **proteksi proses** (watchdog per fase, validasi level
sebelum dosing, latch e-stop, proteksi suhu/gas, deteksi pompa macet).

---

*Dokumen ini harus diperbarui setiap kali ada fitur baru yang selesai.*