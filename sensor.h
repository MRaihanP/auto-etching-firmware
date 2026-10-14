#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"

// ================= SENSOR LEVEL CAIRAN (nickel strip + touch) =================
// Strip nikel di luar jerican HDPE -> kabel AWG30/32 -> GPIO touch ESP32-S3.
// Moving average filter (window=8) untuk stabilkan pembacaan.
// Kalibrasi: 0% (kosong) dan 100% (penuh) -> konversi ke persentase.

#define FILTER_WINDOW 8

const int sensorPin[JUMLAH_SENSOR] = {
  PIN_LEVEL_HCL, PIN_LEVEL_H2O2, PIN_LEVEL_H2O, PIN_LEVEL_SABUN, PIN_LEVEL_LIMBAH
};

const char* namaSensor[JUMLAH_SENSOR] = {
  "HCl", "H2O2", "H2O", "Sabun", "Limbah"
};

bool sensorOk = true;
int  sensorRaw[JUMLAH_SENSOR];          // nilai mentah (moving average)
int  sensorPersen[JUMLAH_SENSOR];       // level dalam persen 0-100
int  sensorKalibKosong[JUMLAH_SENSOR];  // nilai saat 0% (kosong)
int  sensorKalibPenuh[JUMLAH_SENSOR];   // nilai saat 100% (penuh)

// Filter state
int  filterBuf[JUMLAH_SENSOR][FILTER_WINDOW];
int  filterIdx[JUMLAH_SENSOR] = { 0, 0, 0, 0, 0 };
long filterSum[JUMLAH_SENSOR] = { 0, 0, 0, 0, 0 };
bool filterInit[JUMLAH_SENSOR] = { false, false, false, false, false };

bool sensorInit() {
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    pinMode(sensorPin[i], INPUT);
    sensorRaw[i]       = 0;
    sensorPersen[i]    = 0;
    sensorKalibKosong[i] = 113000;
    sensorKalibPenuh[i]  = 156000;
    filterInit[i]      = false;
  }
  sensorOk = true;
  dbg.println(F("[SENSOR] Nickel strip touch mode, filter MA(8)"));
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    dbg.print(F("  "));
    dbg.print(namaSensor[i]);
    dbg.print(F(": GPIO"));
    dbg.print(sensorPin[i]);
    dbg.print(F("  0%="));
    dbg.print(sensorKalibKosong[i]);
    dbg.print(F(" 100%="));
    dbg.println(sensorKalibPenuh[i]);
  }
  return true;
}

int bacaFiltered(int idx) {
  int raw = touchRead(sensorPin[idx]);
  if (!filterInit[idx]) {
    for (int i = 0; i < FILTER_WINDOW; i++) filterBuf[idx][i] = raw;
    filterSum[idx] = (long)raw * FILTER_WINDOW;
    filterInit[idx] = true;
    return raw;
  }
  filterSum[idx] -= filterBuf[idx][filterIdx[idx]];
  filterBuf[idx][filterIdx[idx]] = raw;
  filterSum[idx] += raw;
  filterIdx[idx] = (filterIdx[idx] + 1) % FILTER_WINDOW;
  return (int)(filterSum[idx] / FILTER_WINDOW);
}

int kePersen(int nilai, int idx) {
  int range = sensorKalibPenuh[idx] - sensorKalibKosong[idx];
  if (range <= 0) return 0;
  long persen = ((long)(nilai - sensorKalibKosong[idx]) * 100) / range;
  if (persen < 0) persen = 0;
  if (persen > 100) persen = 100;
  return (int)persen;
}

void sensorUpdate() {
  if (!sensorOk) return;
  bool berubah = false;
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    int raw = bacaFiltered(i);
    sensorRaw[i] = raw;
    int p = kePersen(raw, i);
    if (p != sensorPersen[i]) {
      sensorPersen[i] = p;
      berubah = true;
    }
  }
  if (berubah && halaman == HAL_LEVEL) perluGambar = true;
}

void sensorSetKosong(int i) {
  sensorKalibKosong[i] = sensorRaw[i];
}

void sensorSetPenuh(int i) {
  sensorKalibPenuh[i] = sensorRaw[i];
}

void levelMasuk() {
  levelScroll = 0;
}

// ================= SENSOR SUHU (DS18B20) =================
// 3 sensor: Motor, Komponen, Chamber.
// CATATAN: sensor suhu belum dipetakan ke pin terpisah, jadi sementara semua
// blok memakai satu pin (PIN_DS18B20) dan sensorSuhuRaw() mengembalikan
// nilai placeholder. Setelah tiap sensor punya pin sendiri, ganti fungsi ini.
float suhuOffsetX10[JUMLAH_SUHU] = { 0, 0, 0 };   // kalibrasi offset (x10 °C)

float sensorSuhuRaw(int idx) {
  // Placeholder: satu pin DS18B20 dipakai bersama sampai pin per sensor tersedia.
  return 25.0;
}

float sensorSuhu(int idx) {
  return sensorSuhuRaw(idx) + suhuOffsetX10[idx] / 10.0f;
}

// ================= SENSOR UDARA (MQ-135) =================
int sensorUdaraRaw() {
  return analogRead(PIN_MQ135);
}
