#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"

const char* namaSensor[JUMLAH_SENSOR] = {
  "HCl", "H2O2", "H2O", "Sabun", "Limbah"
};

// Pin sensor level GPIO langsung (HCl, H2O2, H2O, Sabun, Limbah)
const uint8_t pinSensor[JUMLAH_SENSOR] = {
  PIN_SENSOR_HCL, PIN_SENSOR_H2O2, PIN_SENSOR_H2O, PIN_SENSOR_SABUN, PIN_SENSOR_LIMBAH
};

bool sensorOk = false;
int  sensorAvg[JUMLAH_SENSOR];
int  sensorMl[JUMLAH_SENSOR];
int  sensorPersen[JUMLAH_SENSOR];
int  levelSel = 0;

int  sensorHist[JUMLAH_SENSOR][SENSOR_AVG_N];
uint8_t sensorHistIdx = 0, sensorHistJumlah = 0;
unsigned long sensorTerakhirMs = 0;

bool sensorInit() {
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
#if SENSOR_ANALOG
    pinMode(pinSensor[i], INPUT);
#else
    pinMode(pinSensor[i], INPUT_PULLUP);
#endif
  }
  // Catatan: attenuasi ADC default Arduino-ESP32 = 11 dB
  // (rentang ~0-3.3V, skala 0-4095). Tidak di-set eksplisit supaya
  // kompatibel lintas versi core; kalibrasi 5 titik menangani skala aktual.
  sensorHistIdx = 0; sensorHistJumlah = 0;
  sensorOk = true;
  Serial.println(F("[SENSOR] Sensor level GPIO OK"));
  return true;
}

int hitungMl(int i, int raw) {
  if (!kalibrasiSensorValid[i]) return -1;
  KalibrasiSensor &k = kalibrasiSensor[i];
  if (raw >= k.titik[0]) return 0;
  if (raw <= k.titik[KALIBRASI_TITIK - 1]) return k.kapasitasMl;
  for (int t = 0; t < KALIBRASI_TITIK - 1; t++) {
    int y0 = k.titik[t];
    int y1 = k.titik[t + 1];
    if (raw <= y0 && raw >= y1) {
      int persen0 = t * 25;
      int persen1 = (t + 1) * 25;
      float frac = (float)(y0 - raw) / (float)(y0 - y1);
      int persen = persen0 + (int)((persen1 - persen0) * frac);
      return (int)((long)k.kapasitasMl * persen / 100);
    }
  }
  return 0;
}

void sensorUpdate() {
  unsigned long now = millis();
  if (now - sensorTerakhirMs < INTERVAL_SENSOR_MS) return;
  sensorTerakhirMs = now;

  // Baca semua sensor ke ring buffer histori
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
#if SENSOR_ANALOG
    sensorHist[i][sensorHistIdx] = analogRead(pinSensor[i]);
#else
    // Mode digital: mentah = true kalau tangki penuh
    bool mentah = digitalRead(pinSensor[i]);
    if (SENSOR_ACTIVE_LOW) mentah = !mentah;
    sensorHist[i][sensorHistIdx] = mentah ? 0 : 4095;
#endif
  }
  sensorHistIdx = (sensorHistIdx + 1) % SENSOR_AVG_N;
  if (sensorHistJumlah < SENSOR_AVG_N) sensorHistJumlah++;

  bool berubah = false;
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    int avg;
#if SENSOR_ANALOG
    long sum = 0;
    for (int k = 0; k < sensorHistJumlah; k++) sum += sensorHist[i][k];
    avg = (int)(sum / sensorHistJumlah);
#else
    // Mode digital: mayoritas sampel (debounce)
    int tr = 0;
    for (int k = 0; k < sensorHistJumlah; k++)
      if (sensorHist[i][k] == 0) tr++;
    avg = (tr * 2 > sensorHistJumlah) ? 0 : 4095;
#endif
    if (avg != sensorAvg[i]) { sensorAvg[i] = avg; berubah = true; }
    int ml = hitungMl(i, avg);
    if (ml != sensorMl[i]) { sensorMl[i] = ml; berubah = true; }
    if (kalibrasiSensorValid[i] && kalibrasiSensor[i].kapasitasMl > 0) {
      sensorPersen[i] = (int)((long)ml * 100 / kalibrasiSensor[i].kapasitasMl);
    } else {
      sensorPersen[i] = 0;
    }
  }
  if (berubah && (halaman == HAL_CADANGAN || halaman == HAL_KALIB_SENSOR)) perluGambar = true;
}

// ===== Deteksi level dengan histeresis =====
// Konvensi raw: nilai TINGGI = tangki kering, nilai RENDAH = tangki penuh
// (sesuai logika hitungMl)
bool sensorKosong(int i) {
  if (i < 0 || i >= JUMLAH_SENSOR || !kalibrasiSensorValid[i]) return false;
  return sensorAvg[i] >= kalibrasiSensor[i].titik[0] - SENSOR_HYST;
}
bool sensorPenuh(int i) {
  if (i < 0 || i >= JUMLAH_SENSOR || !kalibrasiSensorValid[i]) return false;
  return sensorAvg[i] <= kalibrasiSensor[i].titik[KALIBRASI_TITIK - 1] + SENSOR_HYST;
}

bool cekKetersediaan(const int kebutuhan[JUMLAH_SENSOR], int& idxKurang) {
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    if (!kalibrasiSensorValid[i]) continue;
    if (i == 4) {
      int ruangSisa = kalibrasiSensor[i].kapasitasMl - sensorMl[i];
      if (ruangSisa < kebutuhan[i]) { idxKurang = i; return false; }
    } else {
      if (sensorMl[i] < kebutuhan[i]) { idxKurang = i; return false; }
    }
  }
  return true;
}
