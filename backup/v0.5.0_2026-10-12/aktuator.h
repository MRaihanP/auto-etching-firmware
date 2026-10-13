#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"

// ================= BUZZER (pasif, LEDC API v3.x) =================
enum ToneType { TONE_NONE, TONE_TOMBOL, TONE_SELESAI, TONE_ERROR, TONE_PERINGATAN, TONE_BOOT };

void buzzerInit() {
  ledcAttach(PIN_BUZZER, BUZZER_PWM_FREQ, BUZZER_PWM_RES);
  ledcWrite(PIN_BUZZER, 0);
}

void buzzerNada(int freq, unsigned long durasiMs, uint8_t volume = 128) {
  if (freq <= 0) { ledcWrite(PIN_BUZZER, 0); delay(durasiMs); return; }
  ledcWriteTone(PIN_BUZZER, freq);
  ledcWrite(PIN_BUZZER, volume);
  delay(durasiMs);
  ledcWrite(PIN_BUZZER, 0);
}

void buzzerMain(ToneType t) {
  switch (t) {
    case TONE_TOMBOL:
      buzzerNada(1200, 25, 80);
      break;
    case TONE_SELESAI:
      buzzerNada(523, 120, 150);  delay(40);
      buzzerNada(659, 120, 150);  delay(40);
      buzzerNada(784, 120, 150);  delay(40);
      buzzerNada(1047, 250, 180);
      break;
    case TONE_ERROR:
      for (int i = 0; i < 3; i++) {
        buzzerNada(392, 150, 200); delay(80);
        buzzerNada(330, 150, 200); delay(80);
        buzzerNada(262, 200, 200); delay(150);
      }
      break;
    case TONE_PERINGATAN:
      buzzerNada(880, 400, 160); delay(100);
      buzzerNada(880, 400, 160);
      break;
    case TONE_BOOT:
      buzzerNada(659, 100, 120); delay(50);
      buzzerNada(784, 100, 120); delay(50);
      buzzerNada(1047, 200, 150);
      break;
    default: break;
  }
}

// ================= FAN =================
void fanInit() { ledcAttach(PIN_PWM_FAN, FAN_PWM_FREQ, FAN_PWM_RES); }
void fanSet(int duty) { ledcWrite(PIN_PWM_FAN, constrain(duty, 0, 255)); }

// ================= MOTOR AGITASI =================
void agitasiInit() { ledcAttach(PIN_PWM_AGITASI, FAN_PWM_FREQ, FAN_PWM_RES); }
void agitasiSet(int speed) {
  speed = constrain(speed, 0, 5);
  int duty = (speed == 0) ? 0 : map(speed, 1, 5, 50, 255);
  ledcWrite(PIN_PWM_AGITASI, duty);
}

// ================= POMPA DC =================
void pompaDcInit() {
  ledcAttach(PIN_PWM_SABUN, FAN_PWM_FREQ, FAN_PWM_RES);
  ledcAttach(PIN_PWM_LIMBAH, FAN_PWM_FREQ, FAN_PWM_RES);
  pinMode(PIN_TB_STBY, OUTPUT);
  digitalWrite(PIN_TB_STBY, HIGH);
}
void pompaSabunSet(int duty) { ledcWrite(PIN_PWM_SABUN, constrain(duty, 0, 255)); }
void pompaLimbahSet(int duty) { ledcWrite(PIN_PWM_LIMBAH, constrain(duty, 0, 255)); }

// ================= STEPPER NON-BLOCKING =================
struct StepperCtx {
  uint8_t pinStep, pinDir, pinSlp;
  float   mlPerStep;
  long    stepTarget;
  long    stepAkumulasi;
  unsigned long lastStepUs;
  bool    aktif;
  bool    pinState;
};

StepperCtx stepper[3];

void stepperInit() {
  const uint8_t pins[][3] = {
    { PIN_STEP1, PIN_DIR1, PIN_SLP1 },
    { PIN_STEP2, PIN_DIR2, PIN_SLP2 },
    { PIN_STEP3, PIN_DIR3, PIN_SLP3 }
  };
  for (int i = 0; i < 3; i++) {
    stepper[i].pinStep = pins[i][0];
    stepper[i].pinDir  = pins[i][1];
    stepper[i].pinSlp  = pins[i][2];
    pinMode(stepper[i].pinStep, OUTPUT);
    pinMode(stepper[i].pinDir, OUTPUT);
    pinMode(stepper[i].pinSlp, OUTPUT);
    digitalWrite(stepper[i].pinSlp, HIGH);
    digitalWrite(stepper[i].pinStep, LOW);
    stepper[i].mlPerStep = kalibrasi.stepperMlPerStepX1000[i] / 1000.0f;
    if (stepper[i].mlPerStep <= 0) {
      stepper[i].mlPerStep = STEPPER_ML_PER_REV / (200.0f * STEPPER_MICROSTEP);
    }
    stepper[i].stepTarget = 0;
    stepper[i].stepAkumulasi = 0;
    stepper[i].lastStepUs = 0;
    stepper[i].aktif = false;
    stepper[i].pinState = false;
  }
}

void stepperSleep(bool sleep) {
  for (int i = 0; i < 3; i++)
    digitalWrite(stepper[i].pinSlp, sleep ? LOW : HIGH);
}

bool stepperMulaiDosis(int idx, int ml) {
  if (ml <= 0 || idx < 0 || idx >= 3) return false;
  if (stepper[idx].aktif) return false;
  
  stepperSleep(false);
  stepper[idx].stepTarget = (long)((float)ml / stepper[idx].mlPerStep);
  stepper[idx].stepAkumulasi = 0;
  stepper[idx].lastStepUs = micros();
  stepper[idx].aktif = true;
  stepper[idx].pinState = false;
  digitalWrite(stepper[idx].pinDir, HIGH);
  
  Serial.print(F("[STEPPER] Mulai idx="));
  Serial.print(idx);
  Serial.print(F(" ml="));
  Serial.print(ml);
  Serial.print(F(" step="));
  Serial.println(stepper[idx].stepTarget);
  
  return true;
}

bool stepperUpdate(int idx) {
  if (idx < 0 || idx >= 3) return false;
  if (!stepper[idx].aktif) return false;
  
  unsigned long now = micros();
  if (now - stepper[idx].lastStepUs >= STEP_INTERVAL_US) {
    stepper[idx].lastStepUs = now;
    stepper[idx].pinState = !stepper[idx].pinState;
    digitalWrite(stepper[idx].pinStep, stepper[idx].pinState);
    
    if (stepper[idx].pinState) {
      stepper[idx].stepAkumulasi++;
    }
    
    if (stepper[idx].stepAkumulasi >= stepper[idx].stepTarget) {
      stepper[idx].aktif = false;
      digitalWrite(stepper[idx].pinStep, LOW);
      Serial.print(F("[STEPPER] Selesai idx="));
      Serial.println(idx);
      return false;
    }
  }
  return true;
}

void stepperUpdateAll() {
  for (int i = 0; i < 3; i++) stepperUpdate(i);
}

bool stepperAktif(int idx) {
  if (idx < 0 || idx >= 3) return false;
  return stepper[idx].aktif;
}

void stepperStopAll() {
  for (int i = 0; i < 3; i++) {
    stepper[i].aktif = false;
    digitalWrite(stepper[i].pinStep, LOW);
  }
}

// Test stepper untuk kalibrasi (blocking, khusus kalibrasi)
void stepperTest(int idx, int stepCount) {
  if (idx < 0 || idx >= 3 || stepCount <= 0) return;
  stepperSleep(false);
  digitalWrite(stepper[idx].pinDir, HIGH);
  for (int s = 0; s < stepCount; s++) {
    digitalWrite(stepper[idx].pinStep, HIGH);
    delayMicroseconds(800);
    digitalWrite(stepper[idx].pinStep, LOW);
    delayMicroseconds(800);
  }
  stepperSleep(true);
}

// ================= LED STRIP =================
void ledStripInit() { pinMode(PIN_LED_STRIP, OUTPUT); }
void ledStripSet(bool on) { digitalWrite(PIN_LED_STRIP, on ? HIGH : LOW); }

// ================= E-STOP =================
volatile bool estopTerkunci = false;
void estopInit() {
  pinMode(PIN_ESTOP, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), []() {
    if (digitalRead(PIN_ESTOP) == LOW) estopTerkunci = true;
  }, FALLING);
}
bool estopAktif() { return estopTerkunci; }
void estopReset() {
  if (digitalRead(PIN_ESTOP) == HIGH) estopTerkunci = false;
}

// ================= MQ-135 =================
void mq135Init() { pinMode(PIN_MQ135, INPUT); }
int mq135Baca() { return analogRead(PIN_MQ135); }
bool mq135Normal() {
  int bacaan = mq135Baca();
  return (bacaan < kalibrasi.mq135Baseline + kalibrasi.mq135Margin);
}

// ================= DS18B20 (placeholder) =================
// Untuk implementasi penuh, install library OneWire + DallasTemperature
float ds18b20Baca() {
  return 25.0 + (kalibrasi.ds18b20OffsetX10 / 10.0);
}

// ================= SAFE STATE =================
void aktuatorMatikanSemua() {
  stepperStopAll();
  stepperSleep(true);
  fanSet(0);
  agitasiSet(0);
  pompaSabunSet(0);
  pompaLimbahSet(0);
  ledStripSet(false);
}

void aktuatorInit() {
  buzzerInit();
  fanInit();
  agitasiInit();
  pompaDcInit();
  stepperInit();
  ledStripInit();
  estopInit();
  mq135Init();
  aktuatorMatikanSemua();
}