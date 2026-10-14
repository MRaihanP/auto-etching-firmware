#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"
#include "data.h"
#include "motor.h"   // langkahPerMl[] dipakai pompaTerapkanKalibrasi()

// ================= SENSOR TAMBAHAN =================
// Sensor Suhu (DS18B20, GPIO10) dan Sensor Udara (MQ-135, GPIO9), plus nilai
// kalibrasi aktuator. Halaman kalibrasi untuk keduanya sudah ada; yang belum
// adalah logika test/auto-kalibrasi (lihat LOG.md).

// ---- Sensor Suhu (DS18B20) ----
OneWire owSuhu(PIN_DS18B20);
DallasTemperature dsSuhu(&owSuhu);

bool dsTerdeteksi = false;
float dsTerakhirC = 0.0;
unsigned long dsTerakhirMs = 0;

void suhuInit() {
  dsSuhu.begin();
  dsSuhu.setResolution(12);            // 0.1 degC
  DeviceAddress addr;
  dsTerdeteksi = dsSuhu.getAddress(addr, 0);   // DallasTemperature 4.x: idx sebagai parameter
  dbg.print(F("[SUHU] DS18B20 "));
  dbg.println(dsTerdeteksi ? F("ditemukan") : F("TIDAK ditemukan (cek GPIO10)"));
}

// CATATAN: requestTemperatures() blocking ~750 ms. Karena itu HANYA dipanggil
// dari halaman Kalib Sensor Suhu (1x per ~2 detik), tidak pernah di loop().
float suhuBaca(uint32_t maksUmurMs) {
  if (!dsTerdeteksi) return 0.0;
  unsigned long now = millis();
  if (now - dsTerakhirMs < maksUmurMs) return dsTerakhirC;
  dsTerakhirMs = now;
  dsSuhu.requestTemperatures();
  dsTerakhirC = dsSuhu.getTempCByIndex(0);
  return dsTerakhirC;
}

// ---- Sensor Udara (MQ-135) ----
bool mqTerdeteksi = false;

void udaraInit() {
  pinMode(PIN_MQ135, INPUT);
  mqTerdeteksi = true;                 // tidak ada cara deteksiPresence di analog
}

int udaraBaca() {
  return analogRead(PIN_MQ135);
}

// ================= NILAI KALIBRASI AKTUATOR =================
// Semua nilai bisa diedit dari menu Kalibrasi dan disimpan ke NVS.
// Default 0/0 berarti "belum dikalibrasi" (tanda '-' di tampilan).

const char* namaPompaStepper[3] = { "HCl", "H2O2", "Air" };
const char* namaPompaDC[2]      = { "Air Sabun", "Limbah" };

int32_t langkahPerMlX1000[3]  = { 1200000, 1200000, 1200000 };  // 1200 langkah/ml (perkiraan)
int16_t pompaDcMlPerDtkX10[2] = { 0, 0 };                        // 0 = belum dikalibrasi
int16_t agitasiDutyMin        = 80;    // 0..255, batas bawah motor agitasi
int16_t fanPwmMin             = 200;   // 0..255, PWM minimum exhaust fan
int16_t mq135Baseline         = 180;
int16_t mq135Margin           = 30;
int16_t ds18b20OffsetX10      = 0;     // -50.0 .. +50.0 degC (x10)

// Status nyala untuk halaman kalibrasi Agitasi / Fan (dimatikan lagi saat
// meninggalkan halaman supaya motor tidak forgotten menyala)
bool kalibAgitasiNyala = false;
bool kalibFanNyala      = false;

// Salin langkahPerMlX1000 ke motor.h (dipakai pompaMulai) sebagai bulat.
void pompaTerapkanKalibrasi() {
  for (int i = 0; i < 3; i++) {
    long v = langkahPerMlX1000[i] / 1000;
    if (v < 1) v = 1;
    langkahPerMl[i] = v;
  }
}