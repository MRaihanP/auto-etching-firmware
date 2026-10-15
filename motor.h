#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"  // untuk kalibStepperMl[3]

// ================= DRIVER STEPPER TMC2209 x3 (pompa peristaltik) =================
// Non-blocking: update di loop().
//
// Perubahan dari DRV8825 (v0.6.0):
//   - DRV8825 pakai pin SLP (tidur saat LOW). TMC2209 pakai EN (aktif saat LOW)
//     + PDN (power-down/reset, aktif saat LOW). Perhatikan-polarity-nya dibalik:
//     `digitalWrite(PIN_TMC_EN, LOW)` = driver AKTIF.
//   - EN & PDN di-OR-kan ke 3 driver sekaligus (1 pasang pin).
//   - Mikrostep 1/16 -> 3200 step/rev (`STEP_PER_REV`), bukan 1/36 -> 7200.
//     Angka ini wajib sesuai jumper MS1/MS2 pada driver.
//
// langkahPerMl: jumlah langkah per ml, dihitung dari kalibStepperMl (ml per
// rotasi) sebagai  STEP_PER_REV / ml_per_rev.

const int PIN_STEP[3] = { PIN_STEP1, PIN_STEP2, PIN_STEP3 };
const int PIN_DIR [3] = { PIN_DIR1,  PIN_DIR2,  PIN_DIR3  };

long langkahPerMl[3] = { 1200, 1200, 1200 };  // default, akan diupdate di motorInit()
long  targetLangkah[3] = { 0, 0, 0 };
long  jalanLangkah[3]  = { 0, 0, 0 };
bool  pompaJalan[3]    = { false, false, false };
unsigned long langkahTerakhirUs[3] = { 0, 0, 0 };
// Jeda antar pulsa. 1200 us (~830 Hz) dipilih supaya 3 pompa paralel tidak
// step-loss; DRV8825 lama pakai 400 us dan berisiko kehilangan langkah.
const unsigned int LANGKAH_INTERVAL_US = 1200;

// Aktifkan / matikan driver.
//   sleep = true  -> EN = HIGH (TMC2209 mati, motor bebas, driver's CurrentScale idle)
//   sleep = false -> EN = LOW  (driver aktif, bisa menerima pulsa STEP)
void stepperSleep(bool sleep) {
#if TMC_EN_AKTIF_LOW
  digitalWrite(PIN_TMC_EN, sleep ? HIGH : LOW);
#else
  digitalWrite(PIN_TMC_EN, sleep ? LOW : HIGH);
#endif
}

// Konversi STEP_PER_REV -> step/ml dari kalibrasi (ml per rotasi)
void hitungLangkahPerMl() {
  for (int i = 0; i < 3; i++) {
    float mlPerRev = kalibStepperMl[i];
    if (mlPerRev < 0.1) mlPerRev = 0.1;      // guard div-by-zero
    if (mlPerRev > 10.0) mlPerRev = 10.0;    // batas wajar
    langkahPerMl[i] = (long)((float)STEP_PER_REV / mlPerRev);
    dbg.print(F("[MOTOR] Pompa "));
    dbg.print(i);
    dbg.print(F(": "));
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
    pinMode(PIN_STEP[i], OUTPUT);
    pinMode(PIN_DIR[i], OUTPUT);
    digitalWrite(PIN_DIR[i], HIGH);   // arah maju (sedot)
    digitalWrite(PIN_STEP[i], LOW);
  }
  // EN & PDN -> OUTPUT. Mulai dengan driver nonaktif.
  pinMode(PIN_TMC_EN,  OUTPUT);
  pinMode(PIN_TMC_PDN, OUTPUT);
  stepperSleep(true);                 // EN = HIGH -> mati dulu

  // Pulse reset via PDN: LOW (reset) -> delay -> HIGH (lepas reset).
  digitalWrite(PIN_TMC_PDN, LOW);
  delayMicroseconds(TMC_PDN_RESET_US);
  digitalWrite(PIN_TMC_PDN, HIGH);
  delayMicroseconds(TMC_PDN_RESET_US);

  stepperSleep(false);                // EN = LOW -> aktif, siap menerima STEP
  hitungLangkahPerMl();               // hitung langkahPerMl dari kalibrasi NVS
  dbg.print(F("[MOTOR] TMC2209 x3 aktif, STEP_PER_REV="));
  dbg.print(STEP_PER_REV);
  dbg.println(F(" (EN/PDN shared)"));
}

// Mulai memompa idx: 0=HCl, 1=H2O2, 2=H2O. ml dalam ml.
void pompaMulai(int idx, int ml) {
  if (idx < 0 || idx > 2 || ml <= 0) return;
  stepperSleep(false);                // pastikan driver aktif
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

// Semua pompa berhenti + driver dimatikan (EN = HIGH).
// Dipakai saat E-stop / batal / selesai supaya motor tidak panas dan CurrentScale
// tidak menguras catu daya.
void pompaMatikanTotal() {
  pompaStopAll();
  stepperSleep(true);
}