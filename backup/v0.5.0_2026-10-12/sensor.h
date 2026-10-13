#pragma once
#include <Wire.h>
#include "config.h"
#include "data.h"

const char* namaSensor[JUMLAH_SENSOR] = {
  "HCl", "H2O2", "H2O", "Sabun", "Limbah"
};

bool sensorOk = false;
int  sensorAvg[JUMLAH_SENSOR];
int  sensorMl[JUMLAH_SENSOR];
int  sensorPersen[JUMLAH_SENSOR];
int  levelSel = 0;

int  sensorHist[JUMLAH_SENSOR][SENSOR_AVG_N];
uint8_t sensorHistIdx = 0, sensorHistJumlah = 0;
uint8_t sensorGagal = 0;
unsigned long sensorTerakhirMs = 0, sensorCobaLagiMs = 0;

bool mprTulis(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPR121_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission() == 0);
}

bool mprBaca(uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(MPR121_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPR121_ADDR, (uint8_t)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

bool sensorInit() {
  sensorOk = false;
  if (!mprTulis(0x80, 0x63)) return false;
  delay(2);
  uint8_t c = 0;
  if (!mprBaca(0x5D, &c, 1) || c != 0x24) return false;
  mprTulis(0x5C, 0x10);
  mprTulis(0x5D, 0x20);
  if (!mprTulis(0x5E, JUMLAH_SENSOR)) return false;
  sensorHistIdx = 0; sensorHistJumlah = 0; sensorGagal = 0;
  sensorOk = true;
  Serial.println(F("[SENSOR] MPR121 OK"));
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

  if (!sensorOk) {
    if (now - sensorCobaLagiMs >= 2000) {
      sensorCobaLagiMs = now;
      if (sensorInit()) perluGambar = true;
    }
    return;
  }

  uint8_t buf[2 * JUMLAH_SENSOR];
  if (!mprBaca(0x04, buf, sizeof(buf))) {
    if (++sensorGagal >= 5) {
      sensorOk = false;
      Serial.println(F("!! [SENSOR] MPR121 tidak merespons"));
      perluGambar = true;
    }
    return;
  }
  sensorGagal = 0;

  for (int i = 0; i < JUMLAH_SENSOR; i++)
    sensorHist[i][sensorHistIdx] = buf[2 * i] | ((buf[2 * i + 1] & 0x03) << 8);
  sensorHistIdx = (sensorHistIdx + 1) % SENSOR_AVG_N;
  if (sensorHistJumlah < SENSOR_AVG_N) sensorHistJumlah++;

  bool berubah = false;
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    long sum = 0;
    for (int k = 0; k < sensorHistJumlah; k++) sum += sensorHist[i][k];
    int avg = sum / sensorHistJumlah;
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