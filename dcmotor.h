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

// Durasi (ms) untuk memindahkan `ml` pada pompa DC idx (0=sabun, 1=limbah).
// Sudah termasuk DC_MARGIN_PCT. 0 = pompa belum dikalibrasi.
unsigned long dcDosisMs(int idx, int ml) {
  if (idx < 0 || idx > 1 || ml <= 0) return 0;
  float rate = kalibDcMlPerSec[idx];
  if (rate <= 0.0f) return 0;
  float detik = (float)ml / rate * (1.0f + DC_MARGIN_PCT / 100.0f);
  return (unsigned long)(detik * 1000.0f);
}

// ================= KALIBRASI DC 10 DETIK =================
// Alur: RAMPUP (tunggu duty penuh) -> UKUR (KALIB_DC_DETIK detik)
//       -> TURUN (tunggu diam). Volume diukur SETELAH pompa benar-benar
//       berhenti, jadi hasil ml/detik tidak terpengaruh ramp.
void kalibDcMulai(int idx) {
  if (idx < 0 || idx > 1) return;
  kalibDcIdx = idx;
  dcSetPct(idx + 1, kalibDutyDC[idx]);
  kalibDcFase = KDC_RAMPUP;
}

void kalibDcStop() {
  if (kalibDcIdx >= 0) dcStop(kalibDcIdx + 1);
  kalibDcFase = KDC_TURUN;
}

bool kalibDcJalan() { return kalibDcFase != KDC_IDLE; }

int kalibDcSisaDetik() {
  if (kalibDcFase != KDC_UKUR) return 0;
  unsigned long total = (unsigned long)KALIB_DC_DETIK * 1000UL;
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
      if (millis() - kalibDcT0 >= (unsigned long)KALIB_DC_DETIK * 1000UL) {
        dcStop(kalibDcIdx + 1);
        kalibDcFase = KDC_TURUN;
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
