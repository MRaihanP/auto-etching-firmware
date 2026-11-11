#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"

// ================= DRIVER TB6612FNG x2 (1 arah) =================
// INA->3.3V, INB->GND, jadi arah tetap; kecepatan via PWM pada PWx.
// Driver 1: PWA=agitasi (15), PWB=pompa sabun (16)
// Driver 2: PWA=pompa limbah (17)
//
// CATATAN: exhaust TIDAK memakai driver ini. Arus kipas melebihi
// kapasitas TB6612FNG (~1.2 A), jadi kipas disambung langsung ke 12V
// dan menyala terus. GPIO 18 (PWB driver 2) tidak lagi dipakai.
//
// Semua perubahan duty memakai RAMP non-blocking (DC_RAMP_PCT_PER_SEC):
// motor tidak pernah langsung 0 -> 100%, baik saat start maupun stop.

enum DcChannel { DC_AGI = 0, DC_SABUN = 1, DC_LIMBAH = 2 };
const int PIN_DC[3] = { PIN_PWA_D1, PIN_PWB_D1, PIN_PWA_D2 };

float         dcTargetPct[3] = { 0.0f, 0.0f, 0.0f };
float         dcCurPct[3]    = { 0.0f, 0.0f, 0.0f };
unsigned long dcTerakhirMs   = 0;

int pctKePwm(float pct) {
  int v = (int)(constrain(pct, 0.0f, 100.0f) * 255.0f / 100.0f + 0.5f);
  return constrain(v, 0, 255);
}

void dcTerapkan(int ch) {
  analogWrite(PIN_DC[ch], pctKePwm(dcCurPct[ch]));
}

void dcmotorInit() {
  for (int ch = 0; ch < 3; ch++) {
    pinMode(PIN_DC[ch], OUTPUT);
    analogWriteFrequency(PIN_DC[ch], 20000);   // 20 kHz, di atas ambang dengar
    analogWrite(PIN_DC[ch], 0);
    dcTargetPct[ch] = 0.0f;
    dcCurPct[ch]    = 0.0f;
  }
  dcTerakhirMs = millis();
}

// Set target duty (%). Ramp berjalan di dcmotorUpdate().
void dcSetPct(int ch, float pct) {
  if (ch < 0 || ch > 2) return;
  dcTargetPct[ch] = constrain(pct, 0.0f, 100.0f);
}

void dcStop(int ch) { dcSetPct(ch, 0.0f); }

// Stop normal: target 0, turun bertahap.
void dcStopAll() {
  for (int ch = 0; ch < 3; ch++) dcStop(ch);
}

// Stop darurat: duty dipotong seketika (E-stop).
void dcStopAllDarurat() {
  for (int ch = 0; ch < 3; ch++) {
    dcTargetPct[ch] = 0.0f;
    dcCurPct[ch]    = 0.0f;
    analogWrite(PIN_DC[ch], 0);
  }
}

bool dcAktif(int ch) {
  if (ch < 0 || ch > 2) return false;
  return dcTargetPct[ch] > 0.1f || dcCurPct[ch] > 0.1f;
}

bool dcDiTarget(int ch) {
  if (ch < 0 || ch > 2) return false;
  return fabsf(dcCurPct[ch] - dcTargetPct[ch]) <= 1.0f;
}

bool dcDiam(int ch) {
  if (ch < 0 || ch > 2) return false;
  return dcCurPct[ch] <= 0.1f;
}

float dcPersen(int ch) {
  if (ch < 0 || ch > 2) return 0.0f;
  return dcCurPct[ch];
}

// Panggil tiap loop(): naik/turunkan duty menuju target.
void dcmotorUpdate() {
  unsigned long now = millis();
  float dt = (now - dcTerakhirMs) / 1000.0f;
  if (dt <= 0.0f) return;
  dcTerakhirMs = now;

  float langkah = DC_RAMP_PCT_PER_SEC * dt;
  for (int ch = 0; ch < 3; ch++) {
    if (dcCurPct[ch] == dcTargetPct[ch]) continue;

    if (dcCurPct[ch] < dcTargetPct[ch]) {
      dcCurPct[ch] += langkah;
      if (dcCurPct[ch] > dcTargetPct[ch]) dcCurPct[ch] = dcTargetPct[ch];
    } else {
      dcCurPct[ch] -= langkah;
      if (dcCurPct[ch] < dcTargetPct[ch]) dcCurPct[ch] = dcTargetPct[ch];
    }
    dcTerapkan(ch);
  }
}

// ================= AGITASI: 3 level kecepatan =================
// Tidak perlu kalibrasi ml; cukup duty (%) untuk SPD1/SPD2/SPD3.
int agitasiDuty(int speed) {
  int s = constrain(speed, 1, 3);
  return constrain(agitasiSpdDuty[s - 1], 0, 100);
}

void agitasiSet(int speed) {
  if (speed <= 0) dcStop(DC_AGI);
  else            dcSetPct(DC_AGI, agitasiDuty(speed));
}

bool agitasiAktif() { return dcAktif(DC_AGI); }

// ================= POMPA DC (sabun/limbah) =================
void pompaSabunSet(bool on)  { dcSetPct(DC_SABUN,  on ? kalibDutyDC[0] : 0.0f); }
void pompaLimbahSet(bool on) { dcSetPct(DC_LIMBAH, on ? kalibDutyDC[1] : 0.0f); }

// ================= MODEL KALIBRASI DC =================
// v1.0.0: ml = r * detik + c. Dibangun dari titik (durasi, volume terukur).
// Data lama (ml/detik saja) tetap dipakai dengan c = 0.
float dcRate(int idx) {
  return (idx >= 0 && idx < 2) ? kalibDcMlPerSec[idx] : 0.0f;
}

void dcHitungModel(int idx) {
  if (idx < 0 || idx >= 2) return;
  int n = kalibDcTitikN[idx];
  if (n <= 0) return;
  if (n == 1) {
    if (kalibDcTitikDetik[idx][0] > 0.0f)
      kalibDcMlPerSec[idx] = kalibDcTitikMl[idx][0] / kalibDcTitikDetik[idx][0];
    kalibDcC[idx] = 0.0f;
  } else {
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < n; i++) {
      double x = kalibDcTitikDetik[idx][i];
      double y = kalibDcTitikMl[idx][i];
      sx += x; sy += y; sxx += x * x; sxy += x * y;
    }
    double den = n * sxx - sx * sx;
    if (fabs(den) < 1e-9) return;
    double r = (n * sxy - sx * sy) / den;
    double c = (sy - r * sx) / n;
    if (r <= 0.0) return;
    kalibDcMlPerSec[idx] = (float)r;
    kalibDcC[idx] = (float)c;
  }
}

void dcTambahTitik(int idx, float detik, float mlTerukur) {
  if (idx < 0 || idx >= 2 || detik <= 0.0f || mlTerukur <= 0.0f) return;
  int n = kalibDcTitikN[idx];
  if (n >= 6) {
    for (int i = 1; i < 6; i++) {
      kalibDcTitikDetik[idx][i - 1] = kalibDcTitikDetik[idx][i];
      kalibDcTitikMl[idx][i - 1]    = kalibDcTitikMl[idx][i];
    }
    n = 5;
  }
  kalibDcTitikDetik[idx][n] = detik;
  kalibDcTitikMl[idx][n]    = mlTerukur;
  kalibDcTitikN[idx] = n + 1;
  dcHitungModel(idx);
}

void dcResetTitik(int idx) {
  if (idx < 0 || idx >= 2) return;
  kalibDcTitikN[idx] = 0;
  kalibDcMlPerSec[idx] = 0.0f;
  kalibDcC[idx] = 0.0f;
}

// Durasi (ms) untuk memindahkan `ml` memakai model r*t + c.
// Sudah termasuk DC_MARGIN_PCT. 0 = laju belum diketahui.
unsigned long dcDurasiDariMl(int idx, int ml, int marginPct) {
  float r = dcRate(idx);
  if (idx < 0 || idx > 1 || ml <= 0 || r <= 0.0f) return 0;
  float detik = ((float)ml - kalibDcC[idx]) / r;
  if (detik <= 0.0f) return 0;
  detik *= (1.0f + marginPct / 100.0f);
  return (unsigned long)(detik * 1000.0f);
}

// Durasi (ms) untuk memindahkan `ml` pada laju `rate` (ml/detik).
// Sudah termasuk DC_MARGIN_PCT. 0 = laju belum diketahui.
unsigned long dcDosisMsRate(int ml, float rate) {
  if (ml <= 0 || rate <= 0.0f) return 0;
  float detik = (float)ml / rate * (1.0f + DC_MARGIN_PCT / 100.0f);
  return (unsigned long)(detik * 1000.0f);
}

// Durasi uji kalibrasi TANPA margin. Kalau margin ikut diterapkan saat uji,
// hasil ukur akan selalu ~10% di atas target dan kalibrasi jadi melenceng.
unsigned long dcKalibrasiMs(int ml, float rate) {
  if (ml <= 0 || rate <= 0.0f) return 0;
  return (unsigned long)(((float)ml / rate) * 1000.0f);
}

// Durasi uji memakai model (tanpa margin).
unsigned long dcKalibrasiMsModel(int idx, int ml) {
  if (idx < 0 || idx > 1 || ml <= 0) return 0;
  float r = dcRate(idx);
  if (r <= 0.0f) return 0;
  float detik = ((float)ml - kalibDcC[idx]) / r;
  if (detik <= 0.0f) return 0;
  return (unsigned long)(detik * 1000.0f);
}

// Durasi (ms) untuk memindahkan `ml` pada pompa DC idx (0=sabun, 1=limbah).
// 0 = indeks/ml tidak valid. Bila pompa belum dikalibrasi, dipakai laju
// nominal (fallback) supaya proses tidak berhenti total.
unsigned long dcDosisMs(int idx, int ml) {
  if (idx < 0 || idx > 1 || ml <= 0) return 0;
  unsigned long ms = dcDurasiDariMl(idx, ml, DC_MARGIN_PCT);
  if (ms == 0) ms = dcDosisMsRate(ml, DC_KALIB_NOMINAL_ML_PER_SEC);
  return ms;
}

// Durasi drain: model limbah + margin global + tambahan tetap.
// Bila model limbah belum ada, pakai laju fallback yang lebih lambat dari
// laju nyata (aman: lebih baik sedikit lama daripada wadah belum kosong).
unsigned long dcDrainMs(int ml) {
  if (ml <= 0) return 0;
  float r = dcRate(1);
  if (r <= 0.0f) r = DC_DRAIN_FALLBACK_ML_PER_SEC;
  // Jangan biarkan model yang terlalu rendah membuat drain berjam-jam.
  if (r < DC_DRAIN_MIN_ML_PER_SEC) r = DC_DRAIN_MIN_ML_PER_SEC;

  float detik = (float)ml / r *
                (1.0f + pengaturan.drainMarginPct / 100.0f);
  unsigned long ms = (unsigned long)(detik * 1000.0f);
  return ms + DRAIN_TAMBAHAN_MS;
}

// ================= JEDA / LANJUT POMPA DC =================
float dcTargetSebelumJeda[3] = { 0.0f, 0.0f, 0.0f };
bool  dcJedaAktif = false;

// Hentikan semua kanal dengan ramp, tetapi ingat target aslinya.
void dcJedaSemua() {
  if (dcJedaAktif) return;
  for (int ch = 0; ch < 3; ch++) dcTargetSebelumJeda[ch] = dcTargetPct[ch];
  dcStopAll();
  dcJedaAktif = true;
}

// Pulihkan target sebelum jeda (ramp naik lagi).
void dcLanjutSemua() {
  if (!dcJedaAktif) return;
  for (int ch = 0; ch < 3; ch++) dcTargetPct[ch] = dcTargetSebelumJeda[ch];
  dcJedaAktif = false;
}

// ================= UJI KALIBRASI DC (target ml) =================
// Run PERTAMA sebuah sesi memakai pulse pendek (durasi tetap, tanpa model).
// Run berikutnya memakai target ml dari model. Timer dihitung sejak perintah
// ON, jadi hasil ukur mencakup ramp-up/down - sama seperti dosing proses.
// Model diperbarui dari hasil ukur terakhir: r = ml / detik perintah, c = 0.
// Dengan begitu satu koreksi langsung mendekati laju nyata dan bisa
// menyesuaikan diri bila pompa sudah lama dipakai.

void kalibDcResetModel(int idx) {
  if (idx < 0 || idx > 1) return;
  kalibDcTitikN[idx]     = 0;
  kalibDcMlPerSec[idx]   = 0.0f;
  kalibDcC[idx]          = 0.0f;
  kalibDcModelDuty[idx]  = -1;
  kalibDcOffsetX10[idx]  = 0;
  kalibDcOffsetAda[idx]  = false;
  kalibDcLulus[idx]      = false;
  kalibDcUjiAda[idx]     = false;
}

// Reset penanda sesi (run ke-1 = pulse lagi) tanpa menghapus model.
void kalibDcResetSesi(int idx) {
  if (idx < 0 || idx > 1) return;
  kalibDcRunKe[idx]      = 0;
  kalibDcLulus[idx]      = false;
  kalibDcUjiAda[idx]     = false;
  kalibDcPulseRun[idx]   = false;
  kalibDcUkurMl[idx]     = 0;
  kalibDcOffsetAda[idx]  = false;
  kalibDcDetikAktual     = 0.0f;
  kalibDcDurasiMs        = 0;
}

// Tambah satu run kalibrasi (durasi perintah, volume terukur).
// Laju tiap run = ml / detik; model = RATA-RATA laju semua run dalam set
// (maks KALIB_DC_RUN_MIN). Run berikutnya setelah set penuh memulai set baru.
// c selalu 0, sehingga 1 ml butuh 1000 / r milidetik.
bool kalibDcTambahRun(int idx, float detik, float ml) {
  if (idx < 0 || idx > 1 || detik <= 0.0f || ml <= 0.0f) return false;
  float r = ml / detik;
  if (r < KALIB_DC_R_MIN || r > KALIB_DC_R_MAX) return false;
  int n = kalibDcTitikN[idx];
  if (n < 0 || n >= KALIB_DC_RUN_MIN) n = 0;
  kalibDcTitikDetik[idx][n] = detik;
  kalibDcTitikMl[idx][n]    = ml;
  n++;
  kalibDcTitikN[idx] = n;
  double sum = 0.0;
  for (int i = 0; i < n; i++)
    sum += (double)kalibDcTitikMl[idx][i] / (double)kalibDcTitikDetik[idx][i];
  kalibDcMlPerSec[idx]  = (float)(sum / n);
  kalibDcC[idx]         = 0.0f;
  kalibDcModelDuty[idx] = kalibDutyDC[idx];
  return true;
}

// Laju run terakhir (ml/detik); 0 bila belum ada.
float kalibDcLajuTerakhir(int idx) {
  if (idx < 0 || idx > 1) return 0.0f;
  int n = kalibDcTitikN[idx];
  if (n <= 0 || n > 6 || kalibDcTitikDetik[idx][n - 1] <= 0.0f) return 0.0f;
  return kalibDcTitikMl[idx][n - 1] / kalibDcTitikDetik[idx][n - 1];
}

// Durasi run kalibrasi tetap per pompa (0 = sabun, 1 = limbah).
unsigned long kalibDcDurasiRun(int idx) {
  return (idx == 1) ? KALIB_DC_DURASI_MS_LIMBAH : KALIB_DC_DURASI_MS_SABUN;
}

unsigned long kalibDcPulseMs(int idx) {
  return (idx == 0) ? DC_KALIB_PULSE_MS_SABUN : DC_KALIB_PULSE_MS_LIMBAH;
}

void kalibDcMulai(int idx) {
  if (idx < 0 || idx > 1) return;
  kalibDcIdx      = idx;
  kalibDcT0       = millis();
  kalibDcDetikAktual = 0.0f;
  dcSetPct(idx + 1, kalibDutyDC[idx]);
  kalibDcFase = KDC_UKUR;   // timer sudah berjalan, termasuk ramp-up
}

void kalibDcStop() {
  if (kalibDcIdx < 0) { kalibDcFase = KDC_IDLE; return; }
  // Durasi perintah = dari ON sampai perintah OFF. Ini yang dipakai
  // sebagai x pada model, bukan durasi setelah duty target tercapai.
  if (kalibDcFase == KDC_UKUR) {
    kalibDcDetikAktual = (millis() - kalibDcT0) / 1000.0f;
  }
  dcStop(kalibDcIdx + 1);
  kalibDcFase = KDC_TURUN;
}

bool kalibDcJalan() { return kalibDcFase != KDC_IDLE; }

int kalibDcSisaDetik() {
  if (kalibDcFase != KDC_UKUR) return 0;
  unsigned long total = kalibDcDurasiMs;
  if (total == 0) return 0;
  unsigned long lewat = millis() - kalibDcT0;
  if (lewat >= total) return 0;
  return (int)((total - lewat + 999UL) / 1000UL);
}

void kalibDcUpdate() {
  switch (kalibDcFase) {
    case KDC_RAMPUP:
      if (dcDiTarget(kalibDcIdx + 1)) {
        kalibDcFase = KDC_UKUR;
        kalibDcT0   = millis();
      }
      break;
    case KDC_UKUR:
      if (millis() - kalibDcT0 >= kalibDcDurasiMs) {
        kalibDcStop();   // mencatat durasi perintah & memulai ramp turun
      }
      break;
    case KDC_TURUN:
      if (dcDiam(kalibDcIdx + 1)) {
        kalibDcFase = KDC_IDLE;
        kalibDcIdx  = -1;
      }
      break;
    default: break;
  }
}
