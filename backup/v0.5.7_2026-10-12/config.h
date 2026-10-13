#pragma once
// ===== Konfigurasi hardware & konstanta global =====
#define SDA_PIN         4
#define SCL_PIN         5
#define I2C_CLOCK       400000

#define OLED_ADDR       0x3C
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
// 10 baris teratas OLED rusak → seluruh UI digeser ke bawah (lihat ui.h)
#define OLED_TOP_OFF    10

#define PCF8574_ADDR    0x20
#define JUMLAH_TOMBOL   6
#define DEBOUNCE_MS     40
#define TIDAK_ADA       (-1)
#define P_ATAS   0
#define P_BAWAH  1
#define P_KIRI   3
#define P_KANAN  2
#define P_ENTER  4
#define P_BACK   5

#define INTERVAL_RENDER_MS  20

#define PREF_NS      "etch"
#define PREF_CALIB   "calib"

#define VOL_MIN     50
#define VOL_MAX     1000

// ===== Sensor level GPIO langsung (ganti MPR121) =====
#define JUMLAH_SENSOR       5      // HCl, H2O2, H2O, air sabun, air limbah
#define INTERVAL_SENSOR_MS  50
#define SENSOR_AVG_N        4
#define KALIBRASI_TITIK     5      // 5 titik: 0%, 25%, 50%, 75%, 100%
#define SENSOR_HYST         30     // histeresis deteksi level (count ADC)

// Jenis sensor level:
//  SENSOR_ANALOG = 1 → sensor analog (ADC 12-bit, skala 0-4095), dikalibrasi 5 titik
//  SENSOR_ANALOG = 0 → float switch digital (HIGH/LOW)
#define SENSOR_ANALOG       1
// Mode digital saja: 1 = sensor menghasilkan LOW saat tangki penuh
// (float switch NO + INPUT_PULLUP)
#define SENSOR_ACTIVE_LOW   1

#define PIN_SENSOR_HCL      11
#define PIN_SENSOR_H2O2     12
#define PIN_SENSOR_H2O      13
#define PIN_SENSOR_SABUN    14
#define PIN_SENSOR_LIMBAH   3

// ===== PIN MAP (ESP32-S3) =====
// Stepper DRV8825 ×3 (SLP/STEP/DIR)
// Catatan: MS1/MS2/MS3 DRV8825 di-hardwire di hardware
#define PIN_SLP1        1
#define PIN_STEP1       2
#define PIN_DIR1        42
#define PIN_SLP2        41
#define PIN_STEP2       40
#define PIN_DIR2        39
#define PIN_SLP3        38
#define PIN_STEP3       47
#define PIN_DIR3        21

// ===== TB6612FNG ×2 (1 arah: INA→3.3V, INB→GND) =====
// STBY TB6612FNG di-tie ke 3.3V di hardware (bukan GPIO)
#define PIN_PWM_AGITASI  15   // Driver1 PWA — motor agitasi
#define PIN_PWM_SABUN    16   // Driver1 PWB — pompa air sabun
#define PIN_PWM_LIMBAH   17   // Driver2 PWA — pompa limbah (drain)
#define PIN_PWM_FAN      18   // Driver2 PWB — exhaust fan

#define PIN_BUZZER      6
#define PIN_LED_STRIP   7
#define PIN_DS18B20     10
#define PIN_MQ135       9
#define PIN_ESTOP       8

// ===== Konstanta Aktuator =====
#define STEPPER_MICROSTEP   16
#define STEPPER_ML_PER_REV  5.0
#define STEP_INTERVAL_US    1600   // 1.6ms per step (~625 step/s) - non-blocking

#define FAN_PWM_FREQ        25000
#define FAN_PWM_RES         8
#define BUZZER_PWM_FREQ     2000
#define BUZZER_PWM_RES      8

#define FW_VERSI  "v0.5.7"
