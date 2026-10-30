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

// Sensor level cairan (GPIO langsung, ganti MPR121)
#define JUMLAH_SENSOR       5      // HCl, H2O2, H2O, air sabun, air limbah
#define PIN_LEVEL_HCL       11
#define PIN_LEVEL_H2O2      12
#define PIN_LEVEL_H2O       13
#define PIN_LEVEL_SABUN     14
#define PIN_LEVEL_LIMBAH    3
#define INTERVAL_SENSOR_MS  50     // periode baca sensor
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
#define TOUTH_KALIB_SAMPEL  40     // jumlah sampel awal untuk mencari baseline (kering)
#define SENSOR_LOG_PERIOD_MS 1000  // cetak diagnostik raw semua sensor (0 = mati)

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
// Disamakan dengan sketch test/test-motor (STEPS_PER_REV = 3200). Versi lama
// memakai 6400 (1/32) sehingga pompa berputar 2x lebih cepat dari RPM yang
// ditampilkan. Hasil uji langsung: pompa baru mengalirkan cairan pada ~400 rpm,
// batas aman maksimum 700 rpm. RPM hanya diubah kelipatan 100.
#define PUMP_STEP_PER_REV   3200L
#define PUMP_RPM_MIN         400
#define PUMP_RPM_MAX         700
#define PUMP_RPM_DEFAULT     500
#define PUMP_RPM_STEP        100
// Akselerasi profil trapezoid, dinyatakan dalam rpm/detik supaya perilakunya
// sama persis dengan sketch test/test-motor (accel 1000 langkah/detik^2 pada
// 3200 langkah/putaran = 18,75 rpm/detik). Nilai ini tidak bergantung pada
// microstep, jadi tetap sama walau PUMP_STEP_PER_REV diubah.
// Efek: 0 -> 400 rpm +-21,3 detik, 0 -> 500 rpm +-26,7 detik, dan deselerasi
// (stopMove) memakai laju yang sama. Ramp sehalus ini mencegah pump head
// rusak akibat lonjakan torsi saat start/stop. Naikkan bila dosing terlalu
// lambat, turunkan bila masih ada hentakan.
#define PUMP_ACCEL_RPM_PER_SEC  18.75f
#define PUMP_ACCEL_HZ2 \
  ((uint32_t)(PUMP_ACCEL_RPM_PER_SEC * (float)PUMP_STEP_PER_REV / 60.0f + 0.5f))
// Uji kalibrasi v0.6.4 memakai target ml; jumlah rotasi dihitung dari target
// dan kalibrasi yang sedang dipakai (lihat pompaDosisKalibrasi()). Nilai
// nominal hanya dipakai saat pompa BELUM pernah dikalibrasi (ml/rotasi = 0)
// supaya uji pertama tetap bisa jalan; hasil ukur user menggantikannya.
#define PUMP_KALIB_NOMINAL_ML_PER_REV 2.0f
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
#define MQ135_PIN   9
#define VREF        3.3
#define ADC_RES     4095.0
#define R1          10000.0   // R4
#define R2          15000.0   // R5
#define DIVIDER_RATIO (R2 / (R1 + R2))  // = 0.6
#define MQ135_VCC     5.0      // tegangan modul MQ-135
#define MQ135_RLOAD   10.0     // resistor load modul (kOhm)
#define PIN_ESTOP       8     // estop_sense

#define FW_VERSI  "v0.6.6"
