#pragma once
#include <Arduino.h>
#include "config.h"
#include "data.h"

// ================= BUZZER (non-blocking, LEDC API v3.x) =================
enum ToneType {
    TONE_NONE, TONE_TOMBOL, TONE_SELESAI, TONE_ERROR, TONE_PERINGATAN,
    TONE_BOOT, TONE_MULAI, TONE_PROSES_SELESAI
};

// Tone sequence entry: freq=0 means silence
struct NadaSeq {
    int freq;
    unsigned long durasiMs;
    uint8_t volume;
    unsigned long jedaMs;
};

// --- Existing tones (dipertahankan) ---
const NadaSeq seqTombol[] = {
    {1200, 25, 80, 0}
};
const NadaSeq seqSelesai[] = {
    {523, 120, 150, 40},
    {659, 120, 150, 40},
    {784, 120, 150, 40},
    {1047, 250, 180, 0}
};
const NadaSeq seqError[] = {
    {392, 150, 200, 80},
    {330, 150, 200, 80},
    {262, 200, 200, 150},
    {392, 150, 200, 80},
    {330, 150, 200, 80},
    {262, 200, 200, 0}
};
const NadaSeq seqPeringatan[] = {
    {880, 400, 160, 100},
    {880, 400, 160, 0}
};
const NadaSeq seqBoot[] = {
    {659, 100, 120, 50},
    {784, 100, 120, 50},
    {1047, 200, 150, 0}
};

// --- Dua tone baru ---
const NadaSeq seqMulai[] = {
    {440, 100, 140, 60},
    {660, 100, 140, 60},
    {880, 200, 160, 0}
};
const NadaSeq seqProsesSelesai[] = {
    {523, 80, 150, 30},
    {784, 80, 150, 30},
    {1047, 250, 180, 0}
};

// Non-blocking buzzer state
struct BuzzerState {
    const NadaSeq* seq;
    int totalNada;
    int idx;
    unsigned long mulaiMs;
    bool aktif;
};

BuzzerState buzzerState = {nullptr, 0, 0, 0, false};

void buzzerInit() {
    ledcAttach(PIN_BUZZER, BUZZER_PWM_FREQ, BUZZER_PWM_RES);
    ledcWrite(PIN_BUZZER, 0);
}

void buzzerSet(int freq, uint8_t volume) {
    if (freq <= 0) {
        ledcWrite(PIN_BUZZER, 0);
    } else {
        ledcWriteTone(PIN_BUZZER, freq);
        ledcWrite(PIN_BUZZER, volume);
    }
}

void buzzerMulai(ToneType t) {
    switch (t) {
        case TONE_TOMBOL:       buzzerState.seq = seqTombol;       buzzerState.totalNada = sizeof(seqTombol)/sizeof(seqTombol[0]); break;
        case TONE_SELESAI:      buzzerState.seq = seqSelesai;      buzzerState.totalNada = sizeof(seqSelesai)/sizeof(seqSelesai[0]); break;
        case TONE_ERROR:        buzzerState.seq = seqError;        buzzerState.totalNada = sizeof(seqError)/sizeof(seqError[0]); break;
        case TONE_PERINGATAN:   buzzerState.seq = seqPeringatan;   buzzerState.totalNada = sizeof(seqPeringatan)/sizeof(seqPeringatan[0]); break;
        case TONE_BOOT:         buzzerState.seq = seqBoot;         buzzerState.totalNada = sizeof(seqBoot)/sizeof(seqBoot[0]); break;
        case TONE_MULAI:        buzzerState.seq = seqMulai;        buzzerState.totalNada = sizeof(seqMulai)/sizeof(seqMulai[0]); break;
        case TONE_PROSES_SELESAI: buzzerState.seq = seqProsesSelesai; buzzerState.totalNada = sizeof(seqProsesSelesai)/sizeof(seqProsesSelesai[0]); break;
        default: buzzerState.aktif = false; return;
    }
    buzzerState.idx = 0;
    buzzerState.mulaiMs = millis();
    buzzerState.aktif = true;
    const NadaSeq& n = buzzerState.seq[0];
    buzzerSet(n.freq, n.volume);
}

void buzzerUpdate() {
    if (!buzzerState.aktif) return;
    unsigned long now = millis();
    const NadaSeq& nada = buzzerState.seq[buzzerState.idx];
    if (now - buzzerState.mulaiMs >= nada.durasiMs + nada.jedaMs) {
        buzzerState.idx++;
        if (buzzerState.idx >= buzzerState.totalNada) {
            ledcWrite(PIN_BUZZER, 0);
            buzzerState.aktif = false;
        } else {
            buzzerState.mulaiMs = now;
            const NadaSeq& next = buzzerState.seq[buzzerState.idx];
            buzzerSet(next.freq, next.volume);
        }
    }
}

void buzzerMain(ToneType t) {
    buzzerMulai(t);
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
    // STBY TB6612FNG di-tie ke 3.3V di hardware (bukan GPIO)
    ledcAttach(PIN_PWM_SABUN, FAN_PWM_FREQ, FAN_PWM_RES);
    ledcAttach(PIN_PWM_LIMBAH, FAN_PWM_FREQ, FAN_PWM_RES);
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

// --- Non-blocking stepper test (untuk kalibrasi) ---
int stepperTestIdx = -1;
int stepperTestSteps = 0;
int stepperTestDone = 0;
unsigned long stepperTestLastUs = 0;
bool stepperTestAktif = false;

void stepperTestMulai(int idx, int stepCount) {
    if (idx < 0 || idx >= 3 || stepCount <= 0) return;
    stepperTestIdx = idx;
    stepperTestSteps = stepCount;
    stepperTestDone = 0;
    stepperTestLastUs = micros();
    stepperTestAktif = true;
    stepperSleep(false);
    digitalWrite(stepper[idx].pinDir, HIGH);
}

void stepperTestUpdate() {
    if (!stepperTestAktif) return;
    unsigned long now = micros();
    if (now - stepperTestLastUs >= 800) {
        stepperTestLastUs = now;
        bool high = (stepperTestDone % 2 == 0);
        digitalWrite(stepper[stepperTestIdx].pinStep, high ? HIGH : LOW);
        if (!high) stepperTestDone++;
        if (stepperTestDone >= stepperTestSteps) {
            stepperTestAktif = false;
            stepperSleep(true);
        }
    }
}

void stepperTestStop() {
    stepperTestAktif = false;
    if (stepperTestIdx >= 0 && stepperTestIdx < 3)
        digitalWrite(stepper[stepperTestIdx].pinStep, LOW);
    stepperSleep(true);
}

// ================= LED STRIP =================
void ledStripInit() { pinMode(PIN_LED_STRIP, OUTPUT); }
void ledStripSet(bool on) { digitalWrite(PIN_LED_STRIP, on ? HIGH : LOW); }

// ================= E-STOP =================
volatile bool estopTerkunci = false;
void estopInit() {
    pinMode(PIN_ESTOP, INPUT_PULLUP);
    // ISR minimal: hanya set flag, tanpa digitalRead
    attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), []() {
        estopTerkunci = true;
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
float ds18b20Baca() {
    return 25.0 + (kalibrasi.ds18b20OffsetX10 / 10.0);
}

// ================= SAFE STATE =================
void aktuatorMatikanSemua() {
    stepperStopAll();
    stepperTestStop();
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
