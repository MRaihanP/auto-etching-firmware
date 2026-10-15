#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"  // untuk kalibStepperMl[3]

// ================= DRIVER STEPPER DRV8825 x3 (pompa peristaltik) =================
// Non-blocking: update di loop(). Mikrostep 1/36 (MS1/MS2/MS3 diasumsikan
// ter-hardwire; bila perlu, tambah GPIO MS).
// stepsPerMl: jumlah langkah per ml, dihitung dari kalibStepperMl (ml per rotasi).
// 7200 step/rev (200 * 36 mikrostep) / ml_per_rev = step per ml.

const int PIN_SLP [3] = { PIN_SLP1,  PIN_SLP2,  PIN_SLP3  };
const int PIN_STEP[3] = { PIN_STEP1, PIN_STEP2, PIN_STEP3 };
const int PIN_DIR [3] = { PIN_DIR1,  PIN_DIR2,  PIN_DIR3  };

long langkahPerMl[3] = { 1200, 1200, 1200 };  // default, akan diupdate di motorInit()
long  targetLangkah[3] = { 0, 0, 0 };
long  jalanLangkah[3]  = { 0, 0, 0 };
bool  pompaJalan[3]    = { false, false, false };
unsigned long langkahTerakhirUs[3] = { 0, 0, 0 };
const unsigned int LANGKAH_INTERVAL_US = 400;  // jeda antar pulsa (kecepatan)

// Konversi 7200 step/rev -> step/ml dari kalibrasi (ml per rotasi)
void hitungLangkahPerMl() {
  for (int i = 0; i < 3; i++) {
    float mlPerRev = kalibStepperMl[i];
    if (mlPerRev < 0.1) mlPerRev = 0.1;      // guard div-by-zero
    if (mlPerRev > 10.0) mlPerRev = 10.0;    // batas wajar
    langkahPerMl[i] = (long)(7200.0 / mlPerRev);
    dbg.print(F("[MOTOR] Pompa "));
    dbg.print(i);
    dbg.print(F(": "));
    // print float 2 desimal via snprintf (dbg tidak support print(float, decimals))
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", mlPerRev);
    dbg.print(buf);
    dbg.print(F(" ml/rev -> "));
    dbg.print(langkahPerMl[i]);
    dbg.println(F(" step/ml"));
  }
}

void motorInit() {
  for (int i = 0; i < 3; i++) {
    pinMode(PIN_SLP[i], OUTPUT);
    pinMode(PIN_STEP[i], OUTPUT);
    pinMode(PIN_DIR[i], OUTPUT);
    digitalWrite(PIN_SLP[i], HIGH);   // aktif
    digitalWrite(PIN_DIR[i], HIGH);   // arah maju (sedot)
  }
  hitungLangkahPerMl();  // hitung langkahPerMl dari kalibrasi NVS
}

// Mulai memompa idx: 0=HCl, 1=H2O2, 2=H2O. ml dalam ml.
void pompaMulai(int idx, int ml) {
  if (idx < 0 || idx > 2 || ml <= 0) return;
  targetLangkah[idx] = (long)ml * langkahPerMl[idx];
  jalanLangkah[idx]  = 0;
  pompaJalan[idx]    = true;
  dbg.print(F("[POMPA] Mulai idx "));
  dbg.print(idx);
  dbg.print(F(", "));
  dbg.print(ml);
  dbg.println(F(" ml"));
}

bool pompaSelesai(int idx) { return !pompaJalan[idx]; }

void pompaStopAll() {
  for (int i = 0; i < 3; i++) pompaJalan[i] = false;
}

void motorUpdate() {
  unsigned long now = micros();
  for (int i = 0; i < 3; i++) {
    if (!pompaJalan[i]) continue;
    if (jalanLangkah[i] >= targetLangkah[i]) {
      pompaJalan[i] = false;
      dbg.print(F("[POMPA] Selesai idx "));
      dbg.println(i);
      continue;
    }
    if (now - langkahTerakhirUs[i] >= LANGKAH_INTERVAL_US) {
      langkahTerakhirUs[i] = now;
      digitalWrite(PIN_STEP[i], HIGH);
      delayMicroseconds(10);
      digitalWrite(PIN_STEP[i], LOW);
      jalanLangkah[i]++;
    }
  }
}
