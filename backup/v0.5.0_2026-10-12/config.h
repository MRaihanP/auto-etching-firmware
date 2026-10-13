#pragma once
// ===== Konfigurasi hardware & konstanta global =====
#define SDA_PIN         4
#define SCL_PIN         5
#define I2C_CLOCK       400000

#define OLED_ADDR       0x3C
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

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
#define VOL_MAX     1000   // dikembalikan ke 1000 ml

#define MPR121_ADDR         0x5A
#define JUMLAH_SENSOR       5      // HCl, H2O2, H2O, air sabun, air limbah
#define INTERVAL_SENSOR_MS  50
#define SENSOR_AVG_N        4
#define KALIBRASI_TITIK     5      // 5 titik: 0%, 25%, 50%, 75%, 100%

// ===== PIN MAP (ESP32-S3) =====
#define PIN_SLP1        1
#define PIN_STEP1       2
#define PIN_DIR1        42
#define PIN_SLP2        41
#define PIN_STEP2       40
#define PIN_DIR2        39
#define PIN_SLP3        38
#define PIN_STEP3       47
#define PIN_DIR3        21

#define PIN_PWM_SABUN    13
#define PIN_PWM_LIMBAH   12
#define PIN_PWM_AGITASI  11
#define PIN_PWM_FAN      14
#define PIN_TB_STBY      15

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

#define FW_VERSI  "v0.5.0"