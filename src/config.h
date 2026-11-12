#pragma once
// ===== Konfigurasi hardware & konstanta global =====

// I2C (bus bersama OLED + PCF8574)
#define SDA_PIN         4
#define SCL_PIN         5
#define I2C_CLOCK       400000

// OLED SSD1306
#define OLED_ADDR       0x3C
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// Baris paling atas panel OLED (~10px) rusak.
// Semua UI digambar pada koordinat virtual 0..SCREEN_HEIGHT-1, lalu digeser
// ke bawah UI_Y_OFFSET px (bagian paling bawah terpotong, sudah dihindari
// dengan Area_Bawah di ui.h). Set 0 bila OLED normal.
#define UI_Y_OFFSET     15

// PCF8574 (6 tombol)
#define PCF8574_ADDR    0x20
#define JUMLAH_TOMBOL   6
#define DEBOUNCE_MS     40
#define TIDAK_ADA       (-1)

#define P_ATAS   0
#define P_BAWAH  1
// KIRI/KANAN ditukar sejak v0.4 (P3 = KIRI, P2 = KANAN)
#define P_KIRI   3
#define P_KANAN  2
#define P_ENTER  4
#define P_BACK   5

// Render
#define INTERVAL_RENDER_MS  20

// Aktifkan untuk melihat jumlah render per detik di Serial
// (saat idle harus ~0, saat animasi ~50)
// #define DEBUG_RENDER

// Hanya untuk uji: perlambat render (ms) untuk membuktikan timer proses
// tidak bergantung pada render. Nonaktifkan setelah uji.
// #define DEBUG_LAMBAT_RENDER_MS  300

// Penyimpanan (Preferences/NVS, ESP32)
#define PREF_NS     "etch"

// Batas volume total campuran per proses (ml) - sesuaikan dgn kapasitas wadah/pompa
#define VOL_MIN     50
#define VOL_MAX     1000

// Kapasitas jerigen sumber (ml). Dipakai PRECHECK karena pelacak volume
// tangki tidak aktif: kebutuhan satu proses penuh tidak boleh melebihi isi
// jerigen, sebab pengisian ulang di tengah proses tidak mungkin.
#define KAPASITAS_HCL_ML    1000
#define KAPASITAS_H2O2_ML   1000
#define KAPASITAS_H2O_ML    1000
#define KAPASITAS_SABUN_ML   600

// Sensor level cairan (GPIO langsung, ganti MPR121)
#define JUMLAH_SENSOR       5      // HCl, H2O2, H2O, air sabun, air limbah
#define PIN_LEVEL_HCL       11
#define PIN_LEVEL_H2O2      12
#define PIN_LEVEL_H2O       13
#define PIN_LEVEL_SABUN     14
#define PIN_LEVEL_LIMBAH    3
#define INTERVAL_SENSOR_MS  50     // periode baca sensor level
#define INTERVAL_SUHU_MS    1000   // periode baca DS18B20
#define INTERVAL_UDARA_MS   500    // periode baca MQ-135
#define SENSOR_AVG_N        4      // rata-rata bergerak (sampel)
#define AMBANG_MARGIN       10     // ENTER di Level Cairan: ambang = nilai sekarang - margin
#define AMBANG_HYST         3      // histeresis di sekitar ambang
#define SENSOR_AKTIF_LOW    1      // 1 = sensor aktif saat LOW (untuk mode digitalRead)
#define SENSOR_TOUCH_READ   1      // 1 = touchReadRaw (strip nikel langsung ke pin sentuh)
#define TOUCH_DEBOUNCE      3      // jumlah sampel berturut-turut sebelum state berubah
// Ambang relatif (persen dari baseline) + kecepatan adaptasi baseline.
// Tidak ada asumsi arah: nilai bisa naik ATAU turun saat ada cairan/sentuhan.
#define TOUCH_DELTA_PERSEN  6      // dianggap berubah bila |raw-baseline| > 6% baseline
#define TOUCH_DELTA_MIN     300    // minimal selisih absolut (untuk raw kecil)
#define TOUCH_ADAPT_CEPAT   8      // adaptasi baseline saat nilai dekat
#define TOUCH_ADAPT_LAMBAT  512    // adaptasi baseline saat nilai jauh (jangan serap sentuhan)
#define TOUCH_KALIB_SAMPEL  40     // jumlah sampel awal untuk mencari baseline (kering)
#define SENSOR_LOG_PERIOD_MS 1000  // cetak diagnostik raw semua sensor (0 = mati)
// Ambang status basah/kering untuk logika proses (persen hasil kalibrasi).
// Histeresis mencegah status berkedip saat permukaan cairan bergelombang.
#define LEVEL_ON_PCT        55     // >= ini dianggap BASAH
#define LEVEL_OFF_PCT       45     // <= ini dianggap KERING
// Ambang "penuh" khusus tangki limbah. Lebih tinggi dari LEVEL_ON_PCT dan
// harus bertahan LIMBAH_PENUH_MS supaya percikan/foam saat drain tidak
// memicu E06. Histeresis turun ke LIMBAH_PENUH_OFF_PCT agar bacaan yang
// berkedip di sekitar ambang tidak mereset timer.
#define LIMBAH_PENUH_PCT     85
#define LIMBAH_PENUH_OFF_PCT 75
#define LIMBAH_PENUH_MS      3000UL
// Abaikan sensor limbah selama awal pompa drain (lonjakan/percikan).
#define LIMBAH_DRAIN_GRACE_MS 2000UL
#define LEVEL_STABIL_MS     5000   // status harus stabil selama ini sebelum baseline diadaptasi
#define LEVEL_RAW_MIN       1000L  // di luar rentang ini -> sensor FAULT
#define LEVEL_RAW_MAX       900000L

// Driver TB6612FNG x2 (1 arah: INA->3.3V, INB->GND)
#define PIN_PWA_D1          15    // driver 1 PWA: motor agitasi
#define PIN_PWB_D1          16    // driver 1 PWB: pompa air sabun
#define PIN_PWA_D2          17    // driver 2 PWA: pompa limbah (drain)

// GPIO 18 (driver 2 PWB) kini BEBIAS.
// Exhaust fan disambung langsung ke 12V (arus kipas melebihi kapasitas
// TB6612FNG ~1.2A) dan menyala terus selama mesin dinyalakan, sehingga
// tidak perlu dikendalikan firmware lagi.

// ===== POMPA PERISTALTIK STEPPER (DRV8825 1/16) =====
// Motor 200 langkah/putaran x microstep 1/16 = 3200 langkah/putaran.
// Sama dengan sketch uji motor (STEPS_PER_REV = 3200). Versi lama
// memakai 6400 (1/32) sehingga pompa berputar 2x lebih cepat dari RPM yang
// ditampilkan. Hasil uji langsung: pompa baru mengalirkan cairan pada ~400 rpm,
// batas aman maksimum 700 rpm. RPM hanya diubah kelipatan 100.
#define PUMP_STEP_PER_REV   3200L
#define PUMP_RPM_MIN         400
#define PUMP_RPM_MAX         700
#define PUMP_RPM_DEFAULT     500
#define PUMP_RPM_STEP        100
// Akselerasi profil trapezoid, dinyatakan dalam rpm/detik (lihat PUMP_ACCEL_HZ2
// untuk konversi ke langkah/detik^2). Dengan 500 rpm/s, 0 -> 500 rpm sekitar
// 1 detik. Nilai ini tidak bergantung pada microstep dan dipakai untuk ramp
// naik maupun turun (stopMove). Turunkan bila pump head masih tersentak.
#define PUMP_ACCEL_RPM_PER_SEC  500.0f
#define PUMP_ACCEL_HZ2 \
  ((uint32_t)(PUMP_ACCEL_RPM_PER_SEC * (float)PUMP_STEP_PER_REV / 60.0f + 0.5f))
// Kalibrasi stepper memakai SATU target tetap 100 ml. Nilai nominal hanya
// dipakai saat pompa BELUM pernah dikalibrasi (a = 0 dan ml/rotasi = 0),
// supaya run pertama tetap bisa jalan; hasil ukur user menggantikannya.
//
// 2,0 ml/rotasi ternyata terlalu optimis untuk pompa yang dipakai: hasil uji
// nyata 20-30 ml pada run pertama (target 100 ml), sehingga koreksi pertama
// melonjak 3-5x. Nilai 0,5 ml/rotasi (6400 langkah/ml) membuat run pertama
// lebih panjang dan lebih dekat ke hasil sebenarnya, sehingga kalibrasi
// tidak "kejauhan" sejak awal. Nilai ini tetap hanya titik awal.
#define PUMP_KALIB_NOMINAL_ML_PER_REV 0.5f
#define KALIB_STEPPER_TARGET_ML      100
#define KALIB_STEPPER_RUN_MIN        3
// Koreksi lebih besar dari ini (persen) harus dikonfirmasi ENTER kedua untuk
// mencegah salah ketik yang merusak model.
#define KALIB_STEPPER_KOREKSI_BESAR  50.0f
// Batas langkah per run kalibrasi. 2.000.000 langkah ~ 75 detik pada 500 rpm;
// di atas ini hampir pasti model/prime bermasalah, jadi run ditolak.
#define KALIB_STEPPER_MAX_STEPS      2000000L
// Hasil run dianggap turun drastis bila < 30% hasil terakhir yang diterapkan
// padahal langkah tidak berkurang. Perlu konfirmasi ENTER kedua.
#define KALIB_STEPPER_TURUN_PCT      30.0f
// Batas target/hasil uji kalibrasi (ml).
#define KALIB_ML_MIN         1
#define KALIB_ML_MAX         999
// Toleransi verifikasi hasil kalibrasi (%). Stepper harus ketat; pompa DC
// lebih longgar karena berbasis waktu/duty dan dipengaruhi selang/viskositas.
#define KALIB_TOLERANSI_STEPPER_PCT 5.0f
#define KALIB_TOLERANSI_DC_PCT      10.0f
// Timeout dosing = 2 x estimasi + margin ini.
#define PUMP_TIMEOUT_MARGIN_MS 10000UL

// ===== POMPA DC & AGITASI (TB6612FNG) =====
// Ramp duty non-blocking (% per detik). 100 = 0..100% dalam 1 detik.
#define DC_RAMP_PCT_PER_SEC  100.0f
// Nilai nominal ml/detik untuk uji kalibrasi pertama saat pompa DC belum
// pernah dikalibrasi (ml/detik = 0). Hasil ukur user menggantikannya.
#define DC_KALIB_NOMINAL_ML_PER_SEC 10.0f
// Run PERTAMA setiap sesi kalibrasi DC memakai pulse pendek, bukan target
// 100 ml. Tujuannya: pompa 100% tidak menyemprot jauh melewati target saat
// laju sebenarnya belum diketahui (mis. limbah ~24 ml/detik, bukan 10).
#define DC_KALIB_PULSE_MS_SABUN   5000UL
#define DC_KALIB_PULSE_MS_LIMBAH  3000UL
// Model baru langsung dipakai untuk run target setelah pulse. Minimal 3 run
// (pulse + 2 run target) dan error <= toleransi sebelum status LULUS.
#define KALIB_DC_RUN_MIN          3
// Batas laju wajar (ml/detik) agar salah ukur tidak merusak model.
#define KALIB_DC_R_MIN            0.5f
#define KALIB_DC_R_MAX            100.0f
// Jalankan target kalibrasi maksimal selama ini; lebih dari ini berarti model
// atau target salah dan run dibatalkan.
#define KALIB_DC_MAKS_MS          120000UL
// v1.0.2: kalibrasi pompa DC = run TETAP 100 detik tanpa target ml. Setelah
// selesai, user mengisi hasil ukur (ml). Laju = ml / detik, diulang
// KALIB_DC_RUN_MIN kali lalu dirata-rata.
#define KALIB_DC_DURASI_MS_SABUN  100000UL   // 100 detik
#define KALIB_DC_DURASI_MS_LIMBAH  20000UL   // 20 detik (debit besar)
#define KALIB_DC_ML_MAX           9999
// Fallback drain bila pompa limbah belum dikalibrasi. Sengaja lebih lambat
// dari laju nyata agar wadah tetap tersedot (bukan berhenti terlalu cepat).
#define DC_DRAIN_FALLBACK_ML_PER_SEC 8.0f
// Laju minimum yang dipakai untuk menghitung durasi drain. Model kalibrasi
// yang terlalu rendah (mis. karena salah ukur) membuat drain sangat lama;
// nilai ini mencegah durasi absurd tanpa mengubah model dosing.
#define DC_DRAIN_MIN_ML_PER_SEC 3.0f
// Margin pompa DC: dosing berbasis waktu diberi kelebihan 10% karena laju DC
// kurang presisi (selang, tegangan, viskositas) dan cenderung kurang, bukan
// berlebih. Dipakai oleh dcDosisMs().
#define DC_MARGIN_PCT        10
// Duty default agitasi untuk SPD1/SPD2/SPD3 (persen).
#define AGITASI_SPD1_DEFAULT 40
#define AGITASI_SPD2_DEFAULT 70
#define AGITASI_SPD3_DEFAULT 100

// ===== PIN MAP (ESP32-S3) - dipakai mulai Step 5/6 =====
// Stepper DRV8825 (pompa peristaltik)
#define PIN_SLP1        1     // HCl
#define PIN_STEP1       2
#define PIN_DIR1        42
#define PIN_SLP2        41    // H2O2
#define PIN_STEP2       40
#define PIN_DIR2        39
#define PIN_SLP3        38    // H2O
#define PIN_STEP3       47
#define PIN_DIR3        21
// Lain-lain
#define PIN_BUZZER      6
#define PIN_LED_STRIP   7
#define PIN_DS18B20     10    // 1-Wire (suhu)
#define PIN_MQ135       9     // analog (kualitas udara)

// ===== MQ-135: divider analog sebelum ADC =====
// Catatan: PIN_MQ135 di atas adalah satu-satunya definisi pin MQ-135.
#define VREF        3.3
#define ADC_RES     4095.0
#define R1          10000.0   // R4
#define R2          15000.0   // R5
#define DIVIDER_RATIO (R2 / (R1 + R2))  // = 0.6
#define MQ135_VCC     5.0      // tegangan modul MQ-135
#define MQ135_RLOAD   10.0     // resistor load modul (kOhm)
#define PIN_ESTOP       8     // estop_sense

// ===== KESELAMATAN & PROSES =====
#define TEMP_PERINGATAN_C   45.0f  // di atas ini: peringatan (buzzer + layar)
#define TEMP_BAHAYA_C       60.0f  // di atas ini: ERROR E02
#define DRAIN_MARGIN_PCT    25     // margin waktu drain (%)
// Purge setelah volume utama habis: memastikan sisa di selang/nampan ikut
// tersedot. Durasi utama tetap dihitung dari volume, bukan angka tetap.
#define DRAIN_TAMBAHAN_MS   5000UL
// Estimasi cairan yang tidak tercatat ledger (menempel di nampan/selang).
// Ditambahkan ke target drain dan dihitung sebagai isi limbah.
#define DRAIN_RESIDU_ML     50
// Peringatan/stop limbah: 85% = waspada, 95% selama 5 s = kritis (stop).
#define LIMBAH_KRITIS_PCT     95
#define LIMBAH_KRITIS_OFF_PCT 90
#define LIMBAH_KRITIS_MS      5000UL
#define PROSES_TIMEOUT_MARGIN_MS 10000UL
#define PROSES_POLL_MS      20UL
#define TANGKI_MAKS_ML      5000
#define TANK_PERINGATAN_ML  100    // sisa tangki sumber / ruang limbah minimum
#define WDT_TIMEOUT_S       15

// ===== MODEL WAKTU ETCHING OTOMATIS =====
// Laju etching nominal hasil uji (um/menit). Dipakai sebagai nilai awal;
// setiap mesin sebaiknya dikalibrasi lewat preset.
#define ETCH_LAJU_DEFAULT_X10   30      // 3,0 um/menit
// Faktor keamanan waktu: waktu teori x 150% agar copper tebal / larutan
// yang mulai jenuh tidak berhenti sebelum tembaga habis.
#define ETCH_FAKTOR_AMAN_PCT    150
// Batas muat tembaga larutan etchant (gram Cu per liter). Di atas nilai ini
// larutan tidak lagi mampu melarutkan tembaga secara wajar.
#define KAPASITAS_CU_G_PER_L    120.0f
#define BACK_TAHAN_MS       1000UL // tahan BACK -> kembali ke menu utama

// Skala waktu untuk uji alur cepat (air saja). Aktifkan lewat
// #define DEBUG_PROSES_CEPAT saat menguji urutan lengkap.
#ifdef DEBUG_PROSES_CEPAT
  #define SKALA_UJI_CEPAT(detik)  max(2, (detik) / 10)
#else
  #define SKALA_UJI_CEPAT(detik)  (detik)
#endif

// Boot delay + dbgInit kedua hanya untuk debugging USB. Alat standalone
// tidak perlu menunggu 2 detik.
// #define DEBUG_BOOT_WAIT

// Level log: 0 = mati, 1 = penting, 2 = normal, 3 = detail.
#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif

#define FW_VERSI  "v1.3.1"
