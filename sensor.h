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

// Nilai cadangan bila sensor belum pernah dikalibrasi.
#define KALIB_DEF_KOSONG  113000
#define KALIB_DEF_PENUH   156000

bool sensorOk = true;
int  sensorRaw[JUMLAH_SENSOR];          // nilai mentah (moving average)
int  sensorPersen[JUMLAH_SENSOR];       // level dalam persen 0-100
int  sensorKalibKosong[JUMLAH_SENSOR];  // nilai saat 0% (kosong)
int  sensorKalibPenuh[JUMLAH_SENSOR];   // nilai saat 100% (penuh)
bool sensorFault[JUMLAH_SENSOR];        // nilai di luar rentang wajar
bool sensorKalibSimpan[JUMLAH_SENSOR] = { false, false, false, false, false };  // kalibrasi tersimpan di NVS
// Waktu (millis) saat persen limbah mulai >= LIMBAH_PENUH_PCT; 0 = belum.
unsigned long sensorLimbahPenuhMs = 0;
// Timer level kritis (>= LIMBAH_KRITIS_PCT) untuk jaring pengaman terakhir.
unsigned long sensorLimbahKritisMs = 0;

// Pasangan 0%/100% layak dipakai (100% harus lebih besar dari 0%).
bool sensorKalibLayak(int i) { return sensorKalibPenuh[i] > sensorKalibKosong[i]; }

// Tandai sensor yang kalibrasinya sudah diisi user (bukan nilai cadangan).
void sensorKalibTandai() {
  for (int i = 0; i < JUMLAH_SENSOR; i++)
    sensorKalibSimpan[i] = sensorKalibLayak(i) &&
        !(sensorKalibKosong[i] == KALIB_DEF_KOSONG && sensorKalibPenuh[i] == KALIB_DEF_PENUH);
}

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
    sensorKalibKosong[i] = KALIB_DEF_KOSONG;
    sensorKalibPenuh[i]  = KALIB_DEF_PENUH;
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

    // Timer "penuh" khusus limbah. Histeresis 85% -> 75% agar bacaan yang
    // berkedip di sekitar ambang tidak mereset hitungan.
    if (i == 4) {
      if (sensorFault[i] || sensorPersen[i] < LIMBAH_PENUH_OFF_PCT)
        sensorLimbahPenuhMs = 0;
      else if (sensorPersen[i] >= LIMBAH_PENUH_PCT && sensorLimbahPenuhMs == 0)
        sensorLimbahPenuhMs = now;

      if (sensorFault[i] || sensorPersen[i] < LIMBAH_KRITIS_OFF_PCT)
        sensorLimbahKritisMs = 0;
      else if (sensorPersen[i] >= LIMBAH_KRITIS_PCT && sensorLimbahKritisMs == 0)
        sensorLimbahKritisMs = now;
    }
  }
  if (berubah) {
    sensorDataBerubah = true;
    if (halaman == HAL_LEVEL || halaman == HAL_INFO) perluGambar = true;
  }
}

// Status untuk logika proses.
bool levelBasah(int i) { return (i >= 0 && i < JUMLAH_SENSOR) ? sensorBasah[i] : false; }
bool levelKering(int i) { return (i >= 0 && i < JUMLAH_SENSOR) ? !sensorBasah[i] : false; }
bool levelFault(int i) { return (i >= 0 && i < JUMLAH_SENSOR) ? sensorFault[i] : true; }

// Penuh khusus tangki limbah: persen tinggi harus bertahan LIMBAH_PENUH_MS.
// Transien saat pompa drain menyala (percikan/foam) tidak dianggap penuh.
bool levelLimbahPenuh() {
  if (sensorFault[4] || sensorLimbahPenuhMs == 0) return false;
  return (millis() - sensorLimbahPenuhMs) >= LIMBAH_PENUH_MS;
}

// Level kritis: jaring pengaman terakhir di tengah proses. Threshold lebih
// tinggi dan lebih lama agar percikan/foam tidak menghentikan proses.
bool levelLimbahKritis() {
  if (sensorFault[4] || sensorLimbahKritisMs == 0) return false;
  return (millis() - sensorLimbahKritisMs) >= LIMBAH_KRITIS_MS;
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

// ================= SENSOR SUHU (DS18B20 x3, 1-Wire) =================
// Satu bus 1-Wire di PIN_DS18B20, tiga sensor dibedakan lewat DeviceAddress.
// Blok "Suhu Motor / Komponen / Chamber" masing-masing menunjuk ke salah satu
// address lewat kalibSuhuAddr[] (1..3) -> sensorAddress[alamat-1].
// Offset per blok (x10 °C) dihimpun dari kalibrasi KIRI/KANAN dan disimpan
// ke NVS key "kal2".

OneWire ds18Bus(PIN_DS18B20);
DallasTemperature ds18(&ds18Bus);

// Urutan slot: 1 = Suhu Motor, 2 = Suhu Komponen, 3 = Suhu Chamber.
DeviceAddress sensorAddress[3] = {
  { 0x28, 0xB8, 0xEF, 0x87, 0x3A, 0x25, 0x02, 0x64 },   // SuhuMotor
  { 0x28, 0x80, 0x77, 0x48, 0x3A, 0x25, 0x02, 0x02 },   // SuhuKomponen
  { 0x28, 0x8E, 0xFD, 0x23, 0x39, 0x25, 0x02, 0x87 }    // SuhuChamber
};

// address terpilih per blok (1..3, index 0 = sensorAddress[0])
int kalibSuhuAddr[JUMLAH_SUHU] = { 1, 2, 3 };

// offset kalibrasi per blok (x10 °C)
float suhuOffsetX10[JUMLAH_SUHU] = { 0, 0, 0 };

// v1.0.2: pembacaan suhu ditulis ulang agar tahan gangguan.
//  - Konversi ditunggu lewat timer (SUHU_KONVERSI_MS), tidak bergantung pada
//    isConversionComplete() yang bisa tidak pernah true bila bus bermasalah.
//  - Bila semua sensor gagal berturut-turut, bus di-scan ulang otomatis
//    (sebelumnya: gagal saat boot = tidak pernah dicoba lagi).
//  - Satu kegagalan baca tidak langsung jadi FAULT (butuh SUHU_GAGAL_MAKS).
//  - Log serial menampilkan alamat yang ditemukan dan level idle bus.
#define SUHU_KONVERSI_MS     800UL    // 12-bit: maks 750 ms
#define SUHU_GAGAL_MAKS      3
#define SUHU_SCAN_ULANG_MS   10000UL

bool ds18Siap = false;
bool sensorSuhuAda[3] = { false, false, false };
bool sensorSuhuFault[JUMLAH_SUHU] = { true, true, true };
int  sensorSuhuGagal[JUMLAH_SUHU] = { SUHU_GAGAL_MAKS, SUHU_GAGAL_MAKS, SUHU_GAGAL_MAKS };
float sensorSuhuCache[JUMLAH_SUHU] = { -127.0f, -127.0f, -127.0f };

unsigned long sensorSuhuMs = 0;       // waktu permintaan konversi terakhir
bool          ds18Menunggu = false;   // konversi sedang berjalan
unsigned long ds18ScanMs   = 0;
int           ds18GagalSiklus = 0;
bool suhuDataBaru = false;
int  ds18Jumlah = 0;          // perangkat yang ditemukan di bus saat scan terakhir
bool ds18IdleHigh = true;     // level idle bus saat scan terakhir

static void suhuCetakAddr(const uint8_t* a) {
  char b[20];
  snprintf(b, sizeof(b), "%02X%02X%02X%02X%02X%02X%02X%02X",
           a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
  dbg.print(b);
}

// Scan bus 1-Wire dan laporkan hasilnya ke serial.
void sensorSuhuScan() {
  ds18.begin();
  ds18.setWaitForConversion(false);      // konversi di latar belakang
  int jumlah = ds18.getDeviceCount();

  pinMode(PIN_DS18B20, INPUT);
  bool idleHigh = (digitalRead(PIN_DS18B20) == HIGH);
  ds18Jumlah = jumlah;
  ds18IdleHigh = idleHigh;

  dbg.print(F("[SUHU] GPIO"));
  dbg.print((int)PIN_DS18B20);
  dbg.print(F(" idle="));
  dbg.print(idleHigh ? F("HIGH") : F("LOW (cek pull-up 4.7k ke 3V3)"));
  dbg.print(F(" perangkat="));
  dbg.println(jumlah);

  for (int i = 0; i < jumlah; i++) {
    DeviceAddress a;
    if (!ds18.getAddress(a, i)) continue;
    dbg.print(F("[SUHU]   bus["));
    dbg.print(i);
    dbg.print(F("] = "));
    suhuCetakAddr(a);
    dbg.println();
  }

  int hadir = 0;
  for (int s = 0; s < 3; s++) {
    sensorSuhuAda[s] = ds18.isConnected(sensorAddress[s]);
    if (sensorSuhuAda[s]) {
      ds18.setResolution(sensorAddress[s], 12);
      hadir++;
    } else {
      dbg.print(F("[SUHU] Slot "));
      dbg.print(s + 1);
      dbg.print(F(" tidak ditemukan: "));
      suhuCetakAddr(sensorAddress[s]);
      dbg.println();
    }
  }
  ds18Siap = (hadir >= 1);
  dbg.print(F("[SUHU] Sensor sesuai address: "));
  dbg.print(hadir);
  dbg.println(F("/3"));
}

void sensorSuhuRequest() {
  ds18.requestTemperatures();            // tidak memblokir (waitForConversion=false)
  sensorSuhuMs = millis();
  ds18Menunggu = true;
}

void sensorSuhuInit() {
  analogReadResolution(12);              // ADC MQ-135 12-bit (0..4095)
  sensorSuhuScan();
  ds18ScanMs = millis();
  if (ds18Siap) sensorSuhuRequest();
}

// Panggil tiap loop(). Siklus: request -> tunggu 800 ms -> baca -> jeda
// sampai INTERVAL_SUHU_MS -> request lagi.
void sensorSuhuUpdate() {
  unsigned long now = millis();

  if (!ds18Siap) {
    for (int i = 0; i < JUMLAH_SUHU; i++) {
      sensorSuhuFault[i] = true;
      sensorSuhuCache[i] = -127.0f;
      sensorSuhuGagal[i] = SUHU_GAGAL_MAKS;
    }
    if (now - ds18ScanMs >= SUHU_SCAN_ULANG_MS) {
      ds18ScanMs = now;
      sensorSuhuScan();
      if (ds18Siap) { ds18GagalSiklus = 0; sensorSuhuRequest(); }
    }
    return;
  }

  if (!ds18Menunggu) {
    if (now - sensorSuhuMs >= INTERVAL_SUHU_MS) sensorSuhuRequest();
    return;
  }
  if (now - sensorSuhuMs < SUHU_KONVERSI_MS) return;
  ds18Menunggu = false;

  int jumlahOk = 0;
  for (int i = 0; i < JUMLAH_SUHU; i++) {
    int slot = constrain(kalibSuhuAddr[i], 1, 3) - 1;
    float t = ds18.getTempC(sensorAddress[slot]);
    bool valid = (t > -100.0f && t < 125.0f && t != 85.0f);   // 85.0 = nilai reset
    if (valid) {
      sensorSuhuCache[i] = t;
      sensorSuhuGagal[i] = 0;
      sensorSuhuFault[i] = false;
      jumlahOk++;
    } else {
      if (sensorSuhuGagal[i] < SUHU_GAGAL_MAKS) sensorSuhuGagal[i]++;
      if (sensorSuhuGagal[i] == SUHU_GAGAL_MAKS && !sensorSuhuFault[i]) {
        dbg.print(F("[SUHU] Blok "));
        dbg.print(i + 1);
        dbg.println(F(" gagal dibaca"));
      }
      if (sensorSuhuGagal[i] >= SUHU_GAGAL_MAKS) {
        sensorSuhuCache[i] = -127.0f;
        sensorSuhuFault[i] = true;
      }
    }
  }

  // Semua gagal beberapa siklus -> anggap bus bermasalah, scan ulang.
  if (jumlahOk == 0) {
    if (++ds18GagalSiklus >= SUHU_GAGAL_MAKS) {
      dbg.println(F("[SUHU] Semua sensor gagal, scan ulang bus"));
      ds18Siap = false;
      ds18ScanMs = now - SUHU_SCAN_ULANG_MS;   // scan segera di panggilan berikut
    }
  } else {
    ds18GagalSiklus = 0;
  }

  suhuDataBaru = true;
  sensorSuhuSiapTampilFlag = true;
}

// Teks status singkat (maks 5 huruf) untuk layar saat suhu tidak terbaca:
//   BUS?  = tidak ada perangkat di bus (cek pull-up 4.7k, DATA, GND, 3V3)
//   ADDR? = ada perangkat tapi alamatnya tidak cocok dengan 3 alamat firmware
//   BACA? = alamat cocok tapi pembacaan gagal (CRC/kabel/noise)
const char* sensorSuhuStatusTeks() {
  if (ds18Jumlah == 0) return "BUS?";
  bool cocok = sensorSuhuAda[0] || sensorSuhuAda[1] || sensorSuhuAda[2];
  if (!cocok) return "ADDR?";
  return "BACA?";
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
