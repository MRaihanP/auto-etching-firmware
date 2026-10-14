#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"

// ================= SENSOR LEVEL CAIRAN (uji touch) =================
// Strip nikel + kabel langsung ke pin sentuh ESP32-S3 (GPIO 1-14).
// Mode sentuh: touchReadRaw() -> nilai turun saat disentuh / ada cairan.
// Auto-kalibrasi: baseline diambil dari N sampel awal (wadah kosong/kering),
// lalu dilacak perlahan. Terisi bila |raw - baseline| > ambang relatif.
// Mode digital (fallback): digitalRead(), aktif saat LOW (SENSOR_AKTIF_LOW).
// BELUM dipakai untuk menentukan level cairan - hanya memverifikasi sensor.

const int sensorPin[JUMLAH_SENSOR] = {
  PIN_LEVEL_HCL, PIN_LEVEL_H2O2, PIN_LEVEL_H2O, PIN_LEVEL_SABUN, PIN_LEVEL_LIMBAH
};

const char* namaSensor[JUMLAH_SENSOR] = {
  "HCl", "H2O2", "H2O", "Sabun", "Limbah"
};

bool sensorOk = true;
int  sensorRaw[JUMLAH_SENSOR];          // nilai mentah terakhir (touch: 0-255)
int  sensorBaseline[JUMLAH_SENSOR];     // baseline saat tidak terisi
int  sensorDelta[JUMLAH_SENSOR];        // baseline - raw
bool sensorAktif[JUMLAH_SENSOR];        // state setelah debounce (true = terisi)
unsigned long sensorSentuh[JUMLAH_SENSOR];  // jumlah perubahan 0->1
int  sensorCnt[JUMLAH_SENSOR];          // counter debounce
unsigned long sensorTerakhirMs = 0;
int  levelSel = 0;
bool sensorKalibrasi = true;            // true = masih mencari baseline

// ----- level dalam persen & ml (untuk tampilan bar) -----
// persen = |raw - kering| / deltaPenuh * 100, difilter supaya tidak bergetar.
// PENTING: sensorKering (baseline 0%) TIDAK adaptif. Baseline adaptif
// (sensorBaseline) hanya untuk deteksi biner AKTIF/tidak - kalau dipakai
// untuk persen, baseline akan "mengejar" noise dan persen merayap sendiri.
int  sensorKering[JUMLAH_SENSOR];     // raw saat tangki kosong (fixed)
int  sensorDeltaPenuh[JUMLAH_SENSOR]; // |raw - kering| saat tangki penuh
int  sensorKapasitasMl[JUMLAH_SENSOR];// kapasitas tangki (untuk kolom ml)
bool sensorKalibValid[JUMLAH_SENSOR];
int  sensorPersen[JUMLAH_SENSOR];
int  sensorMl[JUMLAH_SENSOR];
// true setelah muatAmbang() membaca NVS, supaya sensorInit() tidak menimpa
// kalibrasi yang sudah tersimpan dengan nilai awal.
bool sensorKalibDimuat = false;

// Definisi lengkap ada di bawah (setelah sensorUpdate) - deklarasi di sini
// supaya sensorUpdate() bisa memanggilnya.
void sensorKalibTick();

// Variabel level lama ( dipakai minimal agar UI kompatibel )
int  ambang[JUMLAH_SENSOR]        = { 0, 0, 0, 0, 0 };
int  ambangCadangan[JUMLAH_SENSOR];
bool ambangBerubah = false;
bool sensorIsi[JUMLAH_SENSOR];

bool sensorInit() {
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    pinMode(sensorPin[i], INPUT);
    sensorRaw[i]      = 0;
    sensorBaseline[i] = 0;
    sensorDelta[i]    = 0;
    sensorAktif[i]    = false;
    sensorCnt[i]      = 0;
    sensorSentuh[i]   = 0;
    sensorIsi[i]      = false;
    sensorPersen[i]       = 0;
    sensorMl[i]           = 0;
    if (!sensorKalibDimuat) {
      sensorKering[i]      = 0;
      sensorDeltaPenuh[i]  = TOUCH_DELTA_PENUH_AWAL;
      sensorKapasitasMl[i] = 0;
      sensorKalibValid[i]  = false;
    }
  }
  sensorKalibrasi = true;
  sensorOk = true;
  dbg.println(SENSOR_TOUCH_READ ? F("[SENSOR] Mode TOUCH (strip nikel -> pin sentuh)")
                                   : F("[SENSOR] Mode DIGITAL"));
  dbg.print(F("  TOUCH_DELTA="));
  dbg.print(TOUCH_DELTA_PERSEN);
  dbg.print(F("% (min "));
  dbg.print(TOUCH_DELTA_MIN);
  dbg.print(F("), kalibrasi awal="));
  dbg.print(TOUTH_KALIB_SAMPEL);
  dbg.println(F(" sampel"));
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    dbg.print(F("  "));
    dbg.print(namaSensor[i]);
    dbg.print(F(": GPIO"));
    dbg.print(sensorPin[i]);
    dbg.println(SENSOR_TOUCH_READ ? F(" (touchRead raw)") : F(" (digitalRead)"));
  }
  return true;
}

int sensorBacaRaw(int i) {
#if SENSOR_TOUCH_READ
  // Arduino ESP32 core 2.x: touchRead() mengembalikan nilai RAW hasil
  // touch_pad_read_raw_data(), yaitu menurun saat disentuh / ada cairan.
  return touchRead(sensorPin[i]);
#else
  int v = digitalRead(sensorPin[i]);
  return SENSOR_AKTIF_LOW ? (v == LOW ? 1 : 0) : (v == HIGH ? 1 : 0);
#endif
}

// Baseline adaptif: bergerak cepat saat nilai dekat (drift kecil),
// sangat lambat saat nilai jauh agar sentuhan/cairan tidak "diserap".
// Tidak ada asumsi arah (nilai bisa naik ATAU turun).
void sensorAdaptBaseline(int i, int raw) {
  if (sensorKalibrasi) {
    if (sensorBaseline[i] == 0) sensorBaseline[i] = raw;
    else if (raw < sensorBaseline[i]) sensorBaseline[i] = raw;  // cari nilai saat kering
    return;
  }
  long d = (long)raw - sensorBaseline[i];
  int ambangAbs = (int)max((long)TOUCH_DELTA_MIN, (long)sensorBaseline[i] * TOUCH_DELTA_PERSEN / 100);
  int k = (abs(d) > ambangAbs) ? TOUCH_ADAPT_LAMBAT : TOUCH_ADAPT_CEPAT;
  if (k < 2) k = 2;
  sensorBaseline[i] = (int)(sensorBaseline[i] + d / k);
}

unsigned long sensorLogMs = 0;

// ----- persen & ml level (dengan filter supaya bar tidak bergetar) -----
void sensorHitungPersen(int i) {
  if (!sensorKalibValid[i] || sensorDeltaPenuh[i] <= 0 || sensorKering[i] <= 0) {
    sensorPersen[i] = 0;
    sensorMl[i]     = 0;
    return;
  }
  long refs = sensorKering[i] > 0 ? sensorKering[i] : sensorBaseline[i];
  long d = (long)sensorRaw[i] - refs;
  if (d < 0) d = -d;
  long t = d * 100L / sensorDeltaPenuh[i];
  if (t > 100) t = 100;
  int target = (int)t;
  sensorPersen[i] += (target - sensorPersen[i]) / 4;   // rata-rata bergerak
  if (sensorPersen[i] < 0) sensorPersen[i] = 0;
  if (sensorPersen[i] > 100) sensorPersen[i] = 100;

  long ml = (long)sensorPersen[i] * sensorKapasitasMl[i] / 100;
  sensorMl[i] += ((int)ml - sensorMl[i]) / 4;
  if (sensorMl[i] < 0) sensorMl[i] = 0;
}

// Log periodik: pastikan nilai raw selalu terlihat di Serial (untuk tuning)
void sensorLogPeriodik() {
#if SENSOR_LOG_PERIOD_MS
  unsigned long now = millis();
  if (now - sensorLogMs < SENSOR_LOG_PERIOD_MS) return;
  sensorLogMs = now;
  dbg.print(F("[RAW]"));
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    dbg.print(' ');
    dbg.print(namaSensor[i]);
    dbg.print('=');
    dbg.print(sensorRaw[i]);
    dbg.print('/');
    dbg.print(sensorBaseline[i]);
    dbg.print(sensorAktif[i] ? F("(ON)") : F("(off)"));
  }
  dbg.println();
#endif
#if SENSOR_TOUCH_READ
  bool semuaNol = true;
  for (int i = 0; i < JUMLAH_SENSOR; i++) if (sensorRaw[i] > 0) semuaNol = false;
  static bool sudahPeringat = false;
  if (semuaNol && !sudahPeringat) {
    sudahPeringat = true;
    dbg.println(F("!! [SENSOR] Semua channel touch = 0. Cek wiring strip->pin, "
                   "atau set SENSOR_TOUCH_READ=0 bila pakai output digital."));
  }
#endif
}

void sensorUpdate() {
  unsigned long now = millis();
  if (now - sensorTerakhirMs < INTERVAL_SENSOR_MS) return;
  sensorTerakhirMs = now;
  if (!sensorOk) return;

  static int sisaKalib = TOUTH_KALIB_SAMPEL;
  bool berubah = false;

  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    int raw = sensorBacaRaw(i);
    sensorRaw[i] = raw;
    sensorAdaptBaseline(i, raw);

    bool aktif;
#if SENSOR_TOUCH_READ
    int ambangAbs = (int)max((long)TOUCH_DELTA_MIN, (long)sensorBaseline[i] * TOUCH_DELTA_PERSEN / 100);
    sensorDelta[i] = raw - sensorBaseline[i];            // bertanda: bisa +/-
    aktif = !sensorKalibrasi && abs(sensorDelta[i]) >= ambangAbs;
    sensorHitungPersen(i);
#else
    sensorDelta[i] = raw;
    aktif = !sensorKalibrasi && (raw == 1);
    // Mode digital tidak punya analog level: hanya 0% atau 100%
    sensorPersen[i] = aktif ? 100 : 0;
#endif

    // Debounce: hitung sampel berturut-turut saat kondisi BERUBAH dari state
    if (aktif != sensorAktif[i]) {
      if (++sensorCnt[i] >= TOUCH_DEBOUNCE) {
        sensorAktif[i] = aktif;
        sensorIsi[i] = aktif;
        sensorCnt[i] = 0;
        berubah = true;
        if (aktif) sensorSentuh[i]++;   // hitung setiap perubahan 0->1
        dbg.print(F("[TOUCH] "));
        dbg.print(namaSensor[i]);
        dbg.print(aktif ? F(": AKTIF") : F(": NONAKTIF"));
        dbg.print(F(" raw="));
        dbg.print(raw);
        dbg.print(F(" base="));
        dbg.print(sensorBaseline[i]);
        dbg.print(F(" d="));
        dbg.print(sensorDelta[i]);
        dbg.print(F(" ambang="));
        dbg.print(aktif ? ambangAbs : (int)max((long)TOUCH_DELTA_MIN, (long)sensorBaseline[i] * TOUCH_DELTA_PERSEN / 100));
        dbg.print(F(" jumlah="));
        dbg.println(sensorSentuh[i]);
      }
    } else {
      sensorCnt[i] = 0;
    }
  }

  if (sensorKalibrasi && --sisaKalib <= 0) {
    sensorKalibrasi = false;
    dbg.println(F("[SENSOR] Kalibrasi selesai, siap disentuh"));
    dbg.print(F("[SENSOR] Baseline:"));
    for (int i = 0; i < JUMLAH_SENSOR; i++) {
      dbg.print(' ');
      dbg.print(namaSensor[i]);
      dbg.print('=');
      dbg.print(sensorBaseline[i]);
    }
    dbg.println();
    berubah = true;
  }
  sensorLogPeriodik();
  sensorKalibTick();
  // Level Cairan & Kalib Sensor menampilkan bar/angka -> refresh tiap pembacaan
  if (halaman == HAL_LEVEL || halaman == HAL_KALIB_SENSOR ||
      halaman == HAL_KALIB_SENSOR_LIST) perluGambar = true;
  (void)berubah;
}

// ----- helper halaman Level Cairan (read-only) & Kalibrasi Sensor Level -----
void levelMasuk() {}

// BACK pada dialog simpan -> buang perubahan kalibrasi (lihat .ino: muatAmbang)
void levelBuang() {}

// ================= SAMPLER KALIBRASI 0% / 100% =================
// Titik acuan diambil dari rata-rata SAMPEL_KALIB_N (40 x 50ms ~ 2 detik),
// bukan 1 sampel momentan - 1 sampel bisa saja kena noise atau riak.
#define SAMPEL_KALIB_N 40

enum KalibJenis { KALIB_NOL = 0, KALIB_PENUH = 1 };

bool kalibSampling    = false;   // true selama sampel sedang diambil
int  kalibSampelIdx   = 0;       // index sensor yang dikalibrasi
int  kalibSampelJenis = KALIB_NOL;
long kalibSampelSum   = 0;
int  kalibSampelN     = 0;       // jumlah sampel yang sudah terkumpul

int sensorKalibProgres() {   // 0..100 untuk progress bar
  return kalibSampling ? constrain(kalibSampelN * 100 / SAMPEL_KALIB_N, 0, 100) : 100;
}

// Mulai mengumpulkan sampel. Diabaikan kalau sampling masih berjalan.
void sensorKalibMulai(int i, int jenis) {
  if (i < 0 || i >= JUMLAH_SENSOR) return;
  if (kalibSampling) return;
  kalibSampling    = true;
  kalibSampelIdx   = i;
  kalibSampelJenis = jenis;
  kalibSampelSum   = 0;
  kalibSampelN     = 0;
  dbg.print(F("[KALIB] Ambil sampel untuk "));
  dbg.print(namaSensor[i]);
  dbg.println(jenis == KALIB_NOL ? F(" (0%)") : F(" (100%)"));
}

// Dipanggil setiap sensorUpdate() (50ms) selama pengumpulan sampel
void sensorKalibTick() {
  if (!kalibSampling) return;
  int i = kalibSampelIdx;
  if (kalibSampelJenis == KALIB_NOL) kalibSampelSum += sensorRaw[i];
  else {
    long refs = sensorKering[i] > 0 ? sensorKering[i] : sensorBaseline[i];
    long d = (long)sensorRaw[i] - refs;
    if (d < 0) d = -d;
    kalibSampelSum += d;
  }
  if (++kalibSampelN < SAMPEL_KALIB_N) return;

  // ---- selesai: simpan rata-rata ----
  int rata = (int)(kalibSampelSum / kalibSampelN);
  kalibSampling = false;

  if (kalibSampelJenis == KALIB_NOL) {
    sensorKering[i]   = rata;
    sensorBaseline[i] = rata;      // baseline adaptif mulai dari titik kosong ini
    sensorCnt[i]      = 0;
    sensorAktif[i]    = false;
    sensorIsi[i]      = false;
    sensorPersen[i]   = 0;
    sensorMl[i]       = 0;
    dbg.print(F("[KALIB] "));
    dbg.print(namaSensor[i]);
    dbg.print(F(" 0% = "));
    dbg.println(rata);
  } else {
    if (rata < TOUCH_DELTA_MIN * 2) {
      dbg.print(F("[KALIB] GAGAL "));
      dbg.print(namaSensor[i]);
      dbg.println(F(": delta penuh terlalu kecil (tangki mungkin belum penuh)"));
      return;
    }
    sensorDeltaPenuh[i] = rata;
    sensorKalibValid[i] = true;
    sensorPersen[i]     = 100;
    dbg.print(F("[KALIB] "));
    dbg.print(namaSensor[i]);
    dbg.print(F(" 100% = delta "));
    dbg.println(rata);
  }
}

// Ubah kapasitas tangki (ml) - kolom ml di tampilan mengikuti nilai ini
void sensorSetKapasitas(int i, int ml) {
  if (i < 0 || i >= JUMLAH_SENSOR) return;
  sensorKapasitasMl[i] = constrain(ml, 0, 20000);
  sensorHitungPersen(i);
}