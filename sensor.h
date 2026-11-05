#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"
#include "dbg.h"
#include "data.h"

// ================= SENSOR LEVEL CAIRAN (nickel strip + touch) =================
// Strip nikel di luar jerican HDPE -> kabel AWG30/32 -> GPIO touch ESP32-S3.
// Moving average filter (window=8) untuk stabilkan pembacaan.
// Kalibrasi: 0% (kosong) dan 100% (penuh) -> konversi ke persentase.
//
// Sejak v1.0.0 logika proses memakai status BASAH/KERING dengan histeresis
// (setara sensor digital), sedangkan persen tetap dipakai untuk tampilan.

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
bool sensorFault[JUMLAH_SENSOR];        // nilai di luar rentang wajar

// Status digital hasil histeresis + debounce.
bool sensorBasah[JUMLAH_SENSOR] = { false, false, false, false, false };
int  sensorHit[JUMLAH_SENSOR]   = { 0, 0, 0, 0, 0 };
unsigned long sensorStatusMulaiMs[JUMLAH_SENSOR] = { 0, 0, 0, 0, 0 };

// Filter state
int  filterBuf[JUMLAH_SENSOR][FILTER_WINDOW];
int  filterIdx[JUMLAH_SENSOR] = { 0, 0, 0, 0, 0 };
long filterSum[JUMLAH_SENSOR] = { 0, 0, 0, 0, 0 };
bool filterInit[JUMLAH_SENSOR] = { false, false, false, false, false };

unsigned long sensorLevelMsTerakhir = 0;
bool sensorSuhuSiapTampilFlag = false;

bool sensorInit() {
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    pinMode(sensorPin[i], INPUT);
    sensorRaw[i]       = 0;
    sensorPersen[i]    = 0;
    // Nilai awal hanya cadangan; kalibrasi NVS menimpanya bila ada.
    sensorKalibKosong[i] = 113000;
    sensorKalibPenuh[i]  = 156000;
    sensorFault[i]     = false;
    sensorBasah[i]     = false;
    sensorHit[i]       = 0;
    filterInit[i]      = false;
  }
  sensorOk = true;
  dbg.println(F("[SENSOR] Nickel strip touch mode, filter MA(8)"));
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    dbgV.print(F("  "));
    dbgV.print(namaSensor[i]);
    dbgV.print(F(": GPIO"));
    dbgV.print(sensorPin[i]);
    dbgV.print(F("  0%="));
    dbgV.print(sensorKalibKosong[i]);
    dbgV.print(F(" 100%="));
    dbgV.println(sensorKalibPenuh[i]);
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

// Perbarui status basah/kering dengan histeresis + debounce N sampel.
void sensorUpdateStatus(int i) {
  int p = sensorPersen[i];
  bool target = sensorBasah[i];
  if (p >= LEVEL_ON_PCT)  target = true;
  if (p <= LEVEL_OFF_PCT) target = false;

  if (target != sensorBasah[i]) {
    sensorHit[i]++;
    if (sensorHit[i] >= TOUCH_DEBOUNCE) {
      sensorBasah[i] = target;
      sensorHit[i] = 0;
      sensorStatusMulaiMs[i] = millis();
      dbg.print(F("[SENSOR] "));
      dbg.print(namaSensor[i]);
      dbg.println(target ? F(" BASAH") : F(" KERING"));
    }
  } else {
    sensorHit[i] = 0;
  }
}

// Adaptasi baseline hanya saat status kering stabil dan bacaan masih rendah.
// Perubahan disimpan di RAM; kalibrasi 0% permanen tetap di NVS.
void sensorAdaptasiBaseline(int i, unsigned long now) {
  if (sensorBasah[i] || sensorFault[i]) return;
  if (now - sensorStatusMulaiMs[i] < (unsigned long)pengaturan.levelStabilMs) return;
  if (sensorPersen[i] > 20) return;
  long selisih = (long)sensorRaw[i] - sensorKalibKosong[i];
  if (labs(selisih) < TOUCH_DELTA_MIN) return;
  sensorKalibKosong[i] += selisih / (long)TOUCH_ADAPT_LAMBAT;
}

void sensorUpdate() {
  if (!sensorOk) return;
  unsigned long now = millis();
  if (now - sensorLevelMsTerakhir < INTERVAL_SENSOR_MS) return;
  sensorLevelMsTerakhir = now;

  bool berubah = false;
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    int raw = bacaFiltered(i);
    sensorRaw[i] = raw;
    sensorFault[i] = (raw < LEVEL_RAW_MIN || raw > LEVEL_RAW_MAX ||
                      sensorKalibPenuh[i] <= sensorKalibKosong[i]);
    int p = kePersen(raw, i);
    if (p != sensorPersen[i]) {
      sensorPersen[i] = p;
      berubah = true;
    }
    sensorUpdateStatus(i);
    sensorAdaptasiBaseline(i, now);
  }
  if (berubah && (halaman == HAL_LEVEL || halaman == HAL_INFO)) perluGambar = true;
}

// Status untuk logika proses.
bool levelBasah(int i) { return (i >= 0 && i < JUMLAH_SENSOR) ? sensorBasah[i] : false; }
bool levelKering(int i) { return (i >= 0 && i < JUMLAH_SENSOR) ? !sensorBasah[i] : false; }
bool levelFault(int i) { return (i >= 0 && i < JUMLAH_SENSOR) ? sensorFault[i] : true; }

void sensorSetKosong(int i) {
  sensorKalibKosong[i] = sensorRaw[i];
}

void sensorSetPenuh(int i) {
  sensorKalibPenuh[i] = sensorRaw[i];
}

void levelMasuk() {
  levelScroll = 0;
}

// ================= SENSOR SUHU (DS18B20 x3, 1-Wire) =================
// Satu bus 1-Wire di PIN_DS18B20, tiga sensor dibedakan lewat DeviceAddress.
// Blok "Suhu Motor / Komponen / Chamber" masing-masing menunjuk ke salah satu
// address lewat kalibSuhuAddr[] (1..3) -> sensorAddress[alamat-1].
// Offset per blok (x10 °C) dihimpun dari kalibrasi KIRI/KANAN dan disimpan
// ke NVS key "kal2".

OneWire ds18Bus(PIN_DS18B20);
DallasTemperature ds18(&ds18Bus);

DeviceAddress sensorAddress[3] = {
  { 0x28, 0xB8, 0xEF, 0x87, 0x3A, 0x25, 0x02, 0x64 },
  { 0x28, 0x80, 0x77, 0x48, 0x3A, 0x25, 0x02, 0x02 },
  { 0x28, 0x8E, 0xFD, 0x23, 0x39, 0x25, 0x02, 0x87 }
};

// address terpilih per blok (1..3, index 0 = sensorAddress[0])
int kalibSuhuAddr[JUMLAH_SUHU] = { 1, 2, 3 };

// offset kalibrasi per blok (x10 °C)
float suhuOffsetX10[JUMLAH_SUHU] = { 0, 0, 0 };

bool ds18Siap = false;
bool sensorSuhuFault[JUMLAH_SUHU] = { false, false, false };
float sensorSuhuCache[JUMLAH_SUHU] = { -127.0f, -127.0f, -127.0f };

void sensorSuhuInit() {
  analogReadResolution(12);                // ADC MQ-135 12-bit (0..4095)
  ds18.begin();
  for (int s = 0; s < 3; s++) ds18.setResolution(sensorAddress[s], 12);
  ds18.setWaitForConversion(false);       // konversi di latar belakang
  int jumlah = ds18.getDeviceCount();
  ds18Siap = (jumlah >= 1);
  dbg.print(F("[SUHU] DS18B20 ditemukan: "));
  dbg.print(jumlah);
  dbg.println(ds18Siap ? F(" sensor") : F(" (tidak ada)"));
  for (int s = 0; s < 3; s++) {
    bool ketemu = false;
    for (int i = 0; i < jumlah; i++) {
      DeviceAddress a;
      if (!ds18.getAddress(a, i)) continue;
      if (memcmp(a, sensorAddress[s], 8) == 0) { ketemu = true; break; }
    }
    if (!ketemu) dbg.print(F("[SUHU] Sensor tidak ditemukan (slot ")), dbg.println(s + 1);
  }
  if (ds18Siap) ds18.requestTemperatures();
}

void sensorSuhuRequest() {
  if (!ds18Siap) return;
  ds18.requestTemperatures();          // tidak memblokir
}

unsigned long sensorSuhuMs = 0;
bool suhuDataBaru = false;

// Panggil tiap loop(): minta konversi baru lalu tandai data baru saat selesai.
// Sejak v1.0.0 juga memperbarui cache + status FAULT tiap blok.
void sensorSuhuUpdate() {
  if (!ds18Siap) {
    for (int i = 0; i < JUMLAH_SUHU; i++) {
      sensorSuhuFault[i] = true;
      sensorSuhuCache[i] = -127.0f;
    }
    return;
  }
  if (!ds18.isConversionComplete()) return;   // konversi lama masih jalan
  unsigned long now = millis();
  if (now - sensorSuhuMs < INTERVAL_SUHU_MS) return;
  sensorSuhuMs = now;

  for (int i = 0; i < JUMLAH_SUHU; i++) {
    int slot = constrain(kalibSuhuAddr[i], 1, 3) - 1;
    float t = ds18.getTempC(sensorAddress[slot]);
    sensorSuhuCache[i] = t;
    sensorSuhuFault[i] = (t <= -100.0f);
  }
  sensorSuhuRequest();
  suhuDataBaru = true;
  sensorSuhuSiapTampilFlag = true;
}

// true tepat setelah konversi selesai; pemicu redraw UI.
bool sensorSuhuSiapTampil() {
  if (!suhuDataBaru) return false;
  suhuDataBaru = false;
  return true;
}

// Baca suhu (°C) untuk blok idx dari cache terakhir.
float sensorSuhuRaw(int idx) {
  if (idx < 0 || idx >= JUMLAH_SUHU) return -127.0f;
  return sensorSuhuCache[idx];
}

// Suhu ternormalisasi = bacaan + offset kalibrasi blok tsb.
float sensorSuhu(int idx) {
  float raw = sensorSuhuRaw(idx);
  if (raw <= -100.0f) return raw;   // error / belum ada sensor
  return raw + suhuOffsetX10[idx] / 10.0f;
}

bool sensorSuhuValid(int idx) { return sensorSuhuRaw(idx) > -100.0f; }
bool sensorSuhuFaultBlok(int idx) { return (idx >= 0 && idx < JUMLAH_SUHU) ? sensorSuhuFault[idx] : true; }

// Suhu tertinggi dari semua blok yang valid; -127 = tidak ada data.
float sensorSuhuTertinggi() {
  float maks = -127.0f;
  for (int i = 0; i < JUMLAH_SUHU; i++) {
    if (!sensorSuhuValid(i)) continue;
    float t = sensorSuhu(i);
    if (t > maks) maks = t;
  }
  return maks;
}

float sensorSuhuChamber() { return sensorSuhu(2); }

// ================= SENSOR UDARA (MQ-135) =================
// ADC -> tegangan sensor (dibagi divider ratio) -> ppm MQ-135.
int sensorUdaraRaw() {
  return analogRead(PIN_MQ135);
}

// Tegangan pada sensor MQ-135 sebelum divider (V).
float mq135Tegangan() {
  int raw = analogRead(PIN_MQ135);
  float vAdc = (raw / ADC_RES) * VREF;          // tegangan di pin ADC
  return vAdc / DIVIDER_RATIO;                  // naikkan kembali sebelum divider
}

// Konversi tegangan -> ppm mengikuti kurva MQ-135 (datasheet).
float mq135PPM() {
  float vc = mq135Tegangan();
  if (vc <= 0.05f) return 0.0f;                  // avoid divide by ~0
  float rs = ((MQ135_VCC / vc) - 1.0f) * MQ135_RLOAD;
  if (rs <= 0.0f) return 0.0f;
  float ratio = rs / MQ135_RLOAD;
  return 25.7f * pow(ratio, -1.81f) * pow(ratio, 0.13f);
}

unsigned long sensorUdaraMs = 0;
bool sensorUdaraFault = false;

// ================= TICK SENSOR TERPADU =================
// Satu pintu masuk: level 50 ms, suhu 1 s, MQ135 500 ms. Tidak bergantung
// pada halaman yang sedang dibuka.
void sensorTick() {
  sensorUpdate();                 // sudah punya interval INTERVAL_SENSOR_MS
  sensorSuhuUpdate();             // sudah punya interval INTERVAL_SUHU_MS

  unsigned long now = millis();
  if (now - sensorUdaraMs >= INTERVAL_UDARA_MS) {
    sensorUdaraMs = now;
    mq135Bacaan = sensorUdaraRaw();
    sensorUdaraFault = (mq135Bacaan <= 0 || mq135Bacaan >= (int)ADC_RES);
    if (pengaturan.mqAktif) mq135PPMTerkini = (int)mq135PPM();
  }
}
