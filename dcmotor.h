#pragma once
#include <Arduino.h>
#include "config.h"

// ================= DRIVER TB6612FNG x2 (1 arah) =================
// INA->3.3V, INB->GND, jadi arah tetap; kecepatan via PWM pada PWx.
// Driver 1: PWA=agitasi (15), PWB=pompa sabun (16)
// Driver 2: PWA=pompa limbah (17), PWB=exhaust fan (18)

void dcmotorInit() {
  pinMode(PIN_PWA_D1, OUTPUT);
  pinMode(PIN_PWB_D1, OUTPUT);
  pinMode(PIN_PWA_D2, OUTPUT);
  pinMode(PIN_PWB_D2, OUTPUT);
  analogWrite(PIN_PWA_D1, 0);
  analogWrite(PIN_PWB_D1, 0);
  analogWrite(PIN_PWA_D2, 0);
  analogWrite(PIN_PWB_D2, 0);
}

// speed 1-5 -> PWM; 0 = mati
void agitasiSet(int speed) {
  int pwm = (speed <= 0) ? 0 : map(constrain(speed, 1, 5), 1, 5, 80, 255);
  analogWrite(PIN_PWA_D1, pwm);
}

// Duty langsung 0..255 (dipakai halaman Kalibrasi Agitasi + test manual)
void agitasiSetDuty(int duty) {
  analogWrite(PIN_PWA_D1, constrain(duty, 0, 255));
}

void pompaSabunSet(bool on) { analogWrite(PIN_PWB_D1, on ? 255 : 0); }
void pompaLimbahSet(bool on) { analogWrite(PIN_PWA_D2, on ? 255 : 0); }
void fanSet(bool on) { analogWrite(PIN_PWB_D2, on ? 255 : 0); }

// PWM langsung 0..255 (dipakai halaman Kalibrasi Exhaust Fan)
void fanSetAt(int pwm) { analogWrite(PIN_PWB_D2, constrain(pwm, 0, 255)); }

void dcmotorStopAll() {
  analogWrite(PIN_PWA_D1, 0);
  analogWrite(PIN_PWB_D1, 0);
  analogWrite(PIN_PWA_D2, 0);
  analogWrite(PIN_PWB_D2, 0);
}
