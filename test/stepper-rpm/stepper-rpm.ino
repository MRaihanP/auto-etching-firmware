/*
  Uji mandiri pompa peristaltik stepper DRV8825 1/32.

  Tujuan:
    - Verifikasi kecepatan 400-700 rpm (motor 200 step/rev, 1/32 = 6400 step/rev).
    - Cari RPM minimum agar cairan benar-benar terpompa.
    - Ukur ml/rotasi pada RPM tertentu.

  Perintah Serial (115200 baud):
    h            tampilkan bantuan
    p <0-2>      pilih pompa (0=HCl, 1=H2O2, 2=Air)
    r <rpm>      set RPM (400-700, kelipatan 100)
    v <rev>      putar sejumlah rotasi penuh
    m <steps>    putar sejumlah langkah
    s            stop semua
    t            tes 10 rotasi semua pompa pada RPM sekarang

  Pin map sama dengan config.h proyek utama.
*/

#include <Arduino.h>
#include <FastAccelStepper.h>

#define STEP_PER_REV 6400L
#define RPM_MIN      400
#define RPM_MAX      700
#define RPM_DEFAULT  500
#define ACCEL_HZ2    50000L    // ramp 0->500 rpm ±1,07 detik

const int PIN_STEP[3] = { 2, 40, 47 };
const int PIN_DIR [3] = { 42, 39, 21 };
const int PIN_SLP [3] = { 1, 41, 38 };

FastAccelStepperEngine engine;
FastAccelStepper* stepper[3] = { nullptr, nullptr, nullptr };
int  rpm[3] = { RPM_DEFAULT, RPM_DEFAULT, RPM_DEFAULT };
int  pilih = 0;

int rpmValid(int r) {
  r = constrain(r, RPM_MIN, RPM_MAX);
  return RPM_MIN + ((r - RPM_MIN) / 100) * 100;
}

uint32_t rpmKeHz(int r) {
  return (uint32_t)((float)rpmValid(r) * STEP_PER_REV / 60.0f);
}

void bantuan() {
  Serial.println();
  Serial.println(F("Perintah: h | p <0-2> | r <400-700> | v <rev> | m <steps> | s | x | t"));
  Serial.print(F("Pompa terpilih: "));
  Serial.println(pilih);
  Serial.print(F("RPM: "));
  Serial.println(rpm[pilih]);
}

void mulai(int idx, long langkah) {
  if (idx < 0 || idx > 2 || stepper[idx] == nullptr || langkah <= 0) return;
  stepper[idx]->setSpeedInHz(rpmKeHz(rpm[idx]));
  stepper[idx]->setAcceleration(ACCEL_HZ2);
  stepper[idx]->move((int32_t)langkah);
  Serial.print(F("[MULAI] pompa="));
  Serial.print(idx);
  Serial.print(F(" langkah="));
  Serial.print(langkah);
  Serial.print(F(" rpm="));
  Serial.println(rpm[idx]);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  engine.init();
  for (int i = 0; i < 3; i++) {
    pinMode(PIN_SLP[i], OUTPUT);
    digitalWrite(PIN_SLP[i], LOW);
    stepper[i] = engine.stepperConnectToPin(PIN_STEP[i]);
    if (stepper[i] == nullptr) {
      Serial.print(F("!! Gagal connect pompa "));
      Serial.println(i);
      continue;
    }
    stepper[i]->setDirectionPin(PIN_DIR[i], true);
    stepper[i]->setEnablePin(PIN_SLP[i], false);
    stepper[i]->setAutoEnable(true);
    stepper[i]->setSpeedInHz(rpmKeHz(rpm[i]));
    stepper[i]->setAcceleration(ACCEL_HZ2);
  }
  bantuan();
}

void loop() {
  static String baris;

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      baris.trim();
      if (baris.length() > 0) {
        char cmd = baris.charAt(0);
        long arg = baris.substring(1).toInt();
        switch (cmd) {
          case 'h': bantuan(); break;
          case 'p':
            pilih = constrain((int)arg, 0, 2);
            Serial.print(F("Pompa: ")); Serial.println(pilih);
            break;
          case 'r':
            rpm[pilih] = rpmValid((int)arg);
            Serial.print(F("RPM: ")); Serial.println(rpm[pilih]);
            break;
          case 'v': mulai(pilih, arg * STEP_PER_REV); break;
          case 'm': mulai(pilih, arg); break;
          case 's':
            for (int i = 0; i < 3; i++) {
              if (stepper[i]) stepper[i]->stopMove();   // ramp turun
            }
            Serial.println(F("[STOP] semua (ramp)"));
            break;
          case 'x':
            for (int i = 0; i < 3; i++) {
              if (stepper[i]) { stepper[i]->forceStop(); stepper[i]->disableOutputs(); }
            }
            Serial.println(F("[STOP] semua (darurat)"));
            break;
          case 't':
            for (int i = 0; i < 3; i++) mulai(i, 10 * STEP_PER_REV);
            break;
          default: bantuan(); break;
        }
      }
      baris = "";
    } else {
      if (baris.length() < 40) baris += c;
    }
  }

  static unsigned long t = 0;
  if (millis() - t >= 1000) {
    t = millis();
    for (int i = 0; i < 3; i++) {
      if (stepper[i] && stepper[i]->isRunning()) {
        Serial.print(F("[JALAN] pompa="));
        Serial.print(i);
        Serial.print(F(" pos="));
        Serial.println(stepper[i]->getCurrentPosition());
      }
    }
  }
}
