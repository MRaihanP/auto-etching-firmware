#pragma once
#include <Arduino.h>
#include <FastAccelStepper.h>
#include "config.h"
#include "data.h"

// ================= POMPA PERISTALTIK STEPPER (DRV8825 1/32) =================
// 3 pompa: HCl, H2O2, H2O.
//
// Kecepatan dihitung dari RPM, bukan interval tetap:
//   step/detik = rpm * PUMP_STEP_PER_REV / 60
//
// Pada 1/32 microstep (6400 langkah/putaran), 400-700 rpm = 42.667-74.667
// langkah/detik per pompa. Tiga pompa paralel bisa >200.000 langkah/detik,
// jadi pulsa dibangkitkan oleh FastAccelStepper (MCPWM/PCNT ESP32-S3),
// bukan oleh loop()/digitalWrite().
//
// DRV8825 SLP aktif HIGH -> setEnablePin(pin, false).

FastAccelStepperEngine pompaEngine;
FastAccelStepper* pompaStepper[3] = { nullptr, nullptr, nullptr };

const int PIN_STEP[3] = { PIN_STEP1, PIN_STEP2, PIN_STEP3 };
const int PIN_DIR [3] = { PIN_DIR1,  PIN_DIR2,  PIN_DIR3  };
const int PIN_SLP [3] = { PIN_SLP1,  PIN_SLP2,  PIN_SLP3  };

bool          pompaTargetAktif[3]   = { false, false, false };
long          pompaAwalLangkah[3]   = { 0, 0, 0 };
long          pompaTargetLangkah[3] = { 0, 0, 0 };
long          pompaSisaLangkah[3]   = { 0, 0, 0 };
long          pompaPosisiTerakhir[3]= { 0, 0, 0 };
unsigned long pompaTerakhirGerakMs[3] = { 0, 0, 0 };

// RPM dibatasi 400-700 dan hanya kelipatan 100 (400/500/600/700).
int rpmPompaValid(int rpm) {
  int r = constrain(rpm, PUMP_RPM_MIN, PUMP_RPM_MAX);
  return PUMP_RPM_MIN + ((r - PUMP_RPM_MIN) / PUMP_RPM_STEP) * PUMP_RPM_STEP;
}

uint32_t rpmKeHz(int rpm) {
  return (uint32_t)lround((double)rpmPompaValid(rpm) * (double)PUMP_STEP_PER_REV / 60.0);
}

// Stepper terpasang dan siap menerima perintah gerak.
bool pompaBisaGerak(int idx) {
  return idx >= 0 && idx < 3 && pompaStepper[idx] != nullptr;
}

// Siap untuk dosing: stepper ada + ml/rotasi sudah dikalibrasi.
bool pompaSiap(int idx) {
  return pompaBisaGerak(idx) && kalibStepperMl[idx] > 0.0f;
}

bool pompaSiapSemua() {
  for (int i = 0; i < 3; i++) if (!pompaSiap(i)) return false;
  return true;
}

float pompaLangkahPerMl(int idx) {
  if (!pompaSiap(idx)) return 0.0f;
  return (float)PUMP_STEP_PER_REV / kalibStepperMl[idx];
}

void motorInit() {
  pompaEngine.init();

  for (int i = 0; i < 3; i++) {
    pinMode(PIN_SLP[i], OUTPUT);
    digitalWrite(PIN_SLP[i], LOW);   // DRV8825 sleep = driver nonaktif

    pompaStepper[i] = pompaEngine.stepperConnectToPin(PIN_STEP[i]);
    if (pompaStepper[i] == nullptr) {
      dbg.print(F("[POMPA] Gagal connect STEP idx "));
      dbg.println(i);
      continue;
    }

    pompaStepper[i]->setDirectionPin(PIN_DIR[i], true);   // HIGH = maju
    pompaStepper[i]->setEnablePin(PIN_SLP[i], false);     // SLP aktif HIGH
    pompaStepper[i]->setAutoEnable(true);                 // tidur setelah gerak
    pompaStepper[i]->setSpeedInHz(1000);
    pompaStepper[i]->setAcceleration(PUMP_ACCEL_HZ2);

    dbg.print(F("[POMPA] Siap idx "));
    dbg.print(i);
    dbg.print(F(" STEP="));
    dbg.print(PIN_STEP[i]);
    dbg.print(F(" DIR="));
    dbg.print(PIN_DIR[i]);
    dbg.print(F(" SLP="));
    dbg.println(PIN_SLP[i]);
  }
}

bool pompaJalanLangkah(int idx, long langkah) {
  if (!pompaBisaGerak(idx) || langkah <= 0) return false;

  FastAccelStepper* s = pompaStepper[idx];
  if (s->isRunning()) s->forceStop();

  uint32_t hz = rpmKeHz(kalibStepperRpm[idx]);
  if (s->setSpeedInHz(hz) != 0) {
    dbg.print(F("[POMPA] Speed invalid idx "));
    dbg.println(idx);
    return false;
  }
  s->setAcceleration(PUMP_ACCEL_HZ2);

  pompaAwalLangkah[idx]    = (long)s->getCurrentPosition();
  pompaTargetLangkah[idx]  = langkah;
  pompaPosisiTerakhir[idx] = pompaAwalLangkah[idx];
  pompaTerakhirGerakMs[idx]= millis();
  pompaTargetAktif[idx]    = true;

  if (s->move((int32_t)langkah) != MOVE_OK) {
    pompaTargetAktif[idx] = false;
    dbg.print(F("[POMPA] move() gagal idx "));
    dbg.println(idx);
    return false;
  }

  dbg.print(F("[POMPA] Mulai idx "));
  dbg.print(idx);
  dbg.print(F(" langkah="));
  dbg.print(langkah);
  dbg.print(F(" rpm="));
  dbg.print(rpmPompaValid(kalibStepperRpm[idx]));
  dbg.print(F(" hz="));
  dbg.println(hz);
  return true;
}

// Dosing: ml -> langkah memakai hasil kalibrasi ml/rotasi.
bool pompaMulai(int idx, int ml) {
  if (idx < 0 || idx > 2 || ml <= 0) return false;
  if (!pompaSiap(idx)) {
    dbg.print(F("[POMPA] Belum dikalibrasi idx "));
    dbg.println(idx);
    return false;
  }
  long langkah = (long)lround((double)ml * (double)PUMP_STEP_PER_REV / (double)kalibStepperMl[idx]);
  return pompaJalanLangkah(idx, langkah);
}

// Uji kalibrasi: putar sejumlah rotasi penuh (tidak butuh ml/rotasi).
bool pompaPutarRevolusi(int idx, int rev) {
  if (rev <= 0) return false;
  return pompaJalanLangkah(idx, (long)rev * PUMP_STEP_PER_REV);
}

void pompaStop(int idx) {
  if (idx < 0 || idx > 2) return;
  bool aktif = pompaTargetAktif[idx];
  if (pompaStepper[idx] != nullptr) {
    pompaStepper[idx]->forceStop();
    pompaStepper[idx]->disableOutputs();
  }
  pompaTargetAktif[idx] = false;
  pompaSisaLangkah[idx] = 0;
  if (aktif) {
    dbg.print(F("[POMPA] Stop idx "));
    dbg.println(idx);
  }
}

void pompaStopAll() {
  for (int i = 0; i < 3; i++) {
    pompaSisaLangkah[i] = 0;
    pompaStop(i);
  }
}

// Jeda: simpan sisa langkah tiap pompa lalu hentikan.
void pompaJedaSemua() {
  for (int i = 0; i < 3; i++) {
    if (!pompaTargetAktif[i] || pompaStepper[i] == nullptr) continue;
    long done = (long)pompaStepper[i]->getCurrentPosition() - pompaAwalLangkah[i];
    long sisa = pompaTargetLangkah[i] - done;
    pompaSisaLangkah[i] = (sisa > 0) ? sisa : 0;
    pompaStepper[i]->forceStop();
    pompaStepper[i]->disableOutputs();
    pompaTargetAktif[i] = false;
    dbg.print(F("[POMPA] Jeda idx "));
    dbg.print(i);
    dbg.print(F(" sisa="));
    dbg.println(pompaSisaLangkah[i]);
  }
}

// Lanjut setelah jeda: jalankan sisa langkah masing-masing pompa.
bool pompaLanjutSemua() {
  bool ok = true;
  for (int i = 0; i < 3; i++) {
    if (pompaSisaLangkah[i] <= 0) continue;
    long sisa = pompaSisaLangkah[i];
    pompaSisaLangkah[i] = 0;
    if (!pompaJalanLangkah(i, sisa)) ok = false;
  }
  return ok;
}

bool pompaAktif(int idx) {
  return idx >= 0 && idx < 3 && pompaTargetAktif[idx];
}

bool pompaSelesai(int idx) {
  return idx >= 0 && idx < 3 && !pompaTargetAktif[idx];
}

bool semuaPompaSelesai() {
  for (int i = 0; i < 3; i++) if (pompaTargetAktif[i]) return false;
  return true;
}

int pompaProgres(int idx) {
  if (idx < 0 || idx > 2 || !pompaTargetAktif[idx] || pompaTargetLangkah[idx] <= 0) return 100;
  long done = (long)pompaStepper[idx]->getCurrentPosition() - pompaAwalLangkah[idx];
  long persen = done * 100L / pompaTargetLangkah[idx];
  return (int)constrain(persen, 0L, 100L);
}

// Estimasi durasi dosing (ms), termasuk waktu akselerasi/decelerasi.
unsigned long pompaPerkiraanMs(int idx, int ml) {
  if (!pompaSiap(idx) || ml <= 0) return 0;
  double hz   = (double)rpmKeHz(kalibStepperRpm[idx]);
  double rev  = (double)ml / (double)kalibStepperMl[idx];
  double langkah = rev * (double)PUMP_STEP_PER_REV;
  double ramp = 2.0 * hz / (double)PUMP_ACCEL_HZ2;   // naik + turun
  double detik = langkah / hz + ramp;
  return (unsigned long)(detik * 1000.0);
}

// Panggil tiap loop(). Tidak membangkitkan pulsa; hanya memantau status.
void motorUpdate() {
  unsigned long now = millis();
  for (int i = 0; i < 3; i++) {
    if (!pompaTargetAktif[i] || pompaStepper[i] == nullptr) continue;

    long pos = (long)pompaStepper[i]->getCurrentPosition();
    if (pos != pompaPosisiTerakhir[i]) {
      pompaPosisiTerakhir[i] = pos;
      pompaTerakhirGerakMs[i] = now;
    }

    if (!pompaStepper[i]->isRunning()) {
      pompaTargetAktif[i] = false;
      dbg.print(F("[POMPA] Selesai idx "));
      dbg.print(i);
      dbg.print(F(" pos="));
      dbg.println(pos);
      continue;
    }

    // Watchdog: posisi tidak bergerak > 3 detik -> hentikan.
    if (now - pompaTerakhirGerakMs[i] > 3000UL) {
      dbg.print(F("[POMPA] Macet idx "));
      dbg.println(i);
      pompaStop(i);
    }
  }
}
