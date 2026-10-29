#include <FastAccelStepper.h>

// ================= STEPPER (3 motor) =================
const int NUM_MOTORS = 3;

struct MotorPins { uint8_t slp, step, dir; };
const MotorPins PINS[NUM_MOTORS] = {
  {1,  2,  42},   // stepper 1
  {41, 40, 39},   // stepper 2
  {38, 47, 21},   // stepper 3
};

const int STEPS_PER_REV = 3200;      // 200 x 16 microstep, SESUAIKAN dengan MS pin
float accel = 1000;                  // step/s^2

FastAccelStepperEngine engine;
FastAccelStepper *stepper[NUM_MOTORS];
bool  forwardDir[NUM_MOTORS];
float curRPM[NUM_MOTORS];
bool  awake[NUM_MOTORS];             // true = driver aktif, false = sleep

// ================= DC MOTOR via TB6612FNG (IN1/IN2/STBY hardwire) =================
const int NUM_DC = 3;
const uint8_t DC_PIN[NUM_DC]  = {15, 16, 17};
const char*   DC_NAME[NUM_DC] = {"agi", "sabun", "limbah"};

const int PWM_FREQ = 20000;
const int PWM_RES  = 8;
const int PWM_MAX  = (1 << PWM_RES) - 1;   // 255

float dcRamp = 100;                  // %/detik, kecil = lebih halus
float dcTarget[NUM_DC];
float dcCur[NUM_DC];
uint32_t lastDcMs = 0;

// ================= DRIVER SLEEP (stepper) =================
void driverWake(int i) {
  if (awake[i]) return;
  digitalWrite(PINS[i].slp, HIGH);
  awake[i] = true;
  delay(2);                          // driver butuh ~1 ms setelah wake sebelum menerima step
}

void driverSleep(int i) {
  digitalWrite(PINS[i].slp, LOW);
  awake[i] = false;
}

// tidurkan driver yang sudah berhenti total (deselerasi selesai)
void updateSleep() {
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (awake[i] && curRPM[i] == 0 && !stepper[i]->isRunning()) {
      driverSleep(i);
    }
  }
}

// ================= DC FUNCTIONS =================
void pwmWrite(int i, int val) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(DC_PIN[i], val);
#else
  ledcWrite(i, val);                 // channel = index motor
#endif
}

void dcApply(int i) {
  pwmWrite(i, (int)(dcCur[i] * PWM_MAX / 100.0f + 0.5f));
}

void setDc(int i, float pct) {
  pct = constrain(pct, 0.0f, 100.0f);
  dcTarget[i] = pct;
}

void setAllDc(float pct) {
  for (int i = 0; i < NUM_DC; i++) setDc(i, pct);
}

// ramp duty non-blocking, dipanggil terus dari loop()
void updateDc() {
  uint32_t now = millis();
  float dt = (now - lastDcMs) / 1000.0f;
  if (dt <= 0) return;
  lastDcMs = now;
  float stepPct = dcRamp * dt;
  for (int i = 0; i < NUM_DC; i++) {
    if (dcCur[i] == dcTarget[i]) continue;
    if (dcCur[i] < dcTarget[i]) {
      dcCur[i] += stepPct;
      if (dcCur[i] > dcTarget[i]) dcCur[i] = dcTarget[i];
    } else {
      dcCur[i] -= stepPct;
      if (dcCur[i] < dcTarget[i]) dcCur[i] = dcTarget[i];
    }
    dcApply(i);
  }
}

// -1 = semua, -2 = tidak valid, 0..NUM_DC-1 = motor
int dcIndex(const String &t) {
  if (t == "a") return -1;
  for (int i = 0; i < NUM_DC; i++) if (t == DC_NAME[i]) return i;
  int n = t.toInt();
  if (n >= 1 && n <= NUM_DC) return n - 1;
  return -2;
}

// ================= STEPPER FUNCTIONS =================
void setRPM(int i, float rpm) {
  if (rpm <= 0) {
    stepper[i]->stopMove();          // deselerasi dulu, sleep diurus updateSleep()
    curRPM[i] = 0;
    return;
  }
  driverWake(i);                     // bangunkan driver sebelum ada pulsa step
  uint32_t hz = (uint32_t)(rpm * STEPS_PER_REV / 60.0);
  stepper[i]->setSpeedInHz(hz);
  stepper[i]->setAcceleration((uint32_t)accel);
  if (forwardDir[i]) stepper[i]->runForward();
  else               stepper[i]->runBackward();
  stepper[i]->applySpeedAcceleration();
  curRPM[i] = rpm;
}

void setAllRPM(float rpm) {
  for (int i = 0; i < NUM_MOTORS; i++) setRPM(i, rpm);
}

void setDir(int i, bool fwd) {
  forwardDir[i] = fwd;
  if (curRPM[i] > 0) setRPM(i, curRPM[i]);
}

// ================= UI =================
void printHelp() {
  Serial.println(F("=== STEPPER ==="));
  Serial.println(F("a <rpm>          : semua stepper ke RPM        (a 30)"));
  Serial.println(F("<n> <rpm>        : stepper n saja (1..3)       (2 15)"));
  Serial.println(F("d a f|r          : arah semua stepper          (d a r)"));
  Serial.println(F("d <n> f|r        : arah stepper n              (d 1 f)"));
  Serial.println(F("acc <nilai>      : akselerasi step/s^2         (acc 500)"));
  Serial.println(F("=== DC MOTOR (PWM %) ==="));
  Serial.println(F("p a <pct>        : semua DC ke duty %          (p a 50)"));
  Serial.println(F("p <n|nama> <pct> : 1/agi, 2/sabun, 3/limbah    (p sabun 40)"));
  Serial.println(F("pacc <%/detik>   : laju ramp DC                (pacc 50)"));
  Serial.println(F("ps               : stop semua DC"));
  Serial.println(F("=== UMUM ==="));
  Serial.println(F("s                : stop SEMUA (stepper + DC)"));
  Serial.println(F("st               : status"));
  Serial.println(F("h                : bantuan"));
}

float actualRPM(int i) {
  int32_t mhz = stepper[i]->getCurrentSpeedInMilliHz();
  if (mhz < 0) mhz = -mhz;
  return mhz * 0.001f * 60.0f / STEPS_PER_REV;
}

void printStatus() {
  Serial.printf("Stepper accel: %.0f step/s^2\n", accel);
  for (int i = 0; i < NUM_MOTORS; i++) {
    Serial.printf("Stepper %d: %.1f RPM (aktual %.1f), arah %s, driver %s\n",
                  i + 1, curRPM[i], actualRPM(i),
                  forwardDir[i] ? "FWD" : "REV",
                  awake[i] ? "AKTIF" : "SLEEP");
  }
  Serial.printf("DC ramp: %.0f %%/s\n", dcRamp);
  for (int i = 0; i < NUM_DC; i++) {
    Serial.printf("DC %d (%s, GPIO%d): target %.0f%%, sekarang %.0f%%\n",
                  i + 1, DC_NAME[i], DC_PIN[i], dcTarget[i], dcCur[i]);
  }
}

void handleCommand(String line) {
  line.trim();
  line.toLowerCase();
  if (line.length() == 0) return;

  String tok[3];
  int n = 0, start = 0;
  while (n < 3 && start < (int)line.length()) {
    int sp = line.indexOf(' ', start);
    if (sp < 0) sp = line.length();
    tok[n++] = line.substring(start, sp);
    start = sp + 1;
    while (start < (int)line.length() && line[start] == ' ') start++;
  }

  if (tok[0] == "h")  { printHelp(); return; }
  if (tok[0] == "st") { printStatus(); return; }

  // ---- stop semua ----
  if (tok[0] == "s") {
    setAllRPM(0);
    setAllDc(0);
    Serial.println(F("Semua motor stop"));
    return;
  }

  // ---- DC motor ----
  if (tok[0] == "ps") {
    setAllDc(0);
    Serial.println(F("Semua DC stop"));
    return;
  }

  if (tok[0] == "pacc" && n >= 2) {
    float v = tok[1].toFloat();
    if (v > 0) { dcRamp = v; Serial.printf("DC ramp: %.0f %%/s\n", dcRamp); }
    else Serial.println(F("Nilai harus > 0"));
    return;
  }

  if (tok[0] == "p" && n >= 3) {
    float pct = tok[2].toFloat();
    int idx = dcIndex(tok[1]);
    if (idx == -2) { Serial.println(F("Motor DC: 1..3 / agi / sabun / limbah / a")); return; }
    if (idx == -1) {
      setAllDc(pct);
      Serial.printf("Semua DC: %.0f%%\n", pct);
    } else {
      setDc(idx, pct);
      Serial.printf("DC %s: %.0f%%\n", DC_NAME[idx], pct);
    }
    return;
  }

  // ---- stepper ----
  if (tok[0] == "acc" && n >= 2) {
    float v = tok[1].toFloat();
    if (v > 0) {
      accel = v;
      for (int i = 0; i < NUM_MOTORS; i++) {
        stepper[i]->setAcceleration((uint32_t)accel);
        if (curRPM[i] > 0) stepper[i]->applySpeedAcceleration();
      }
      Serial.printf("Accel: %.0f\n", accel);
    } else Serial.println(F("Nilai accel harus > 0"));
    return;
  }

  if (tok[0] == "d" && n >= 3) {
    if (tok[2] != "f" && tok[2] != "r") { Serial.println(F("Arah: f atau r")); return; }
    bool fwd = (tok[2] == "f");
    if (tok[1] == "a") {
      for (int i = 0; i < NUM_MOTORS; i++) setDir(i, fwd);
      Serial.printf("Semua stepper arah %s\n", fwd ? "FWD" : "REV");
    } else {
      int m = tok[1].toInt();
      if (m < 1 || m > NUM_MOTORS) { Serial.println(F("Stepper 1..3")); return; }
      setDir(m - 1, fwd);
      Serial.printf("Stepper %d arah %s\n", m, fwd ? "FWD" : "REV");
    }
    return;
  }

  if (tok[0] == "a" && n >= 2) {
    float rpm = tok[1].toFloat();
    setAllRPM(rpm);
    Serial.printf("Semua stepper: %.1f RPM\n", rpm);
    return;
  }

  int m = tok[0].toInt();
  if (m >= 1 && m <= NUM_MOTORS && n >= 2) {
    float rpm = tok[1].toFloat();
    setRPM(m - 1, rpm);
    Serial.printf("Stepper %d: %.1f RPM\n", m, rpm);
    return;
  }

  Serial.println(F("Command tidak dikenali. Ketik h untuk bantuan."));
}

// ================= SETUP / LOOP =================
void setup() {
  Serial.begin(115200);

  // --- stepper ---
  engine.init();
  for (int i = 0; i < NUM_MOTORS; i++) {
    pinMode(PINS[i].slp, OUTPUT);
    digitalWrite(PINS[i].slp, LOW);  // mulai dalam kondisi sleep
    awake[i] = false;

    stepper[i] = engine.stepperConnectToPin(PINS[i].step);
    if (!stepper[i]) {
      Serial.printf("Gagal init stepper %d\n", i + 1);
      while (true) delay(1000);
    }
    stepper[i]->setDirectionPin(PINS[i].dir);
    stepper[i]->setAcceleration((uint32_t)accel);
    forwardDir[i] = true;
    curRPM[i] = 0;
  }

  // --- PWM TB6612FNG ---
  for (int i = 0; i < NUM_DC; i++) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(DC_PIN[i], PWM_FREQ, PWM_RES);
#else
    ledcSetup(i, PWM_FREQ, PWM_RES);
    ledcAttachPin(DC_PIN[i], i);
#endif
    dcTarget[i] = 0;
    dcCur[i] = 0;
    dcApply(i);
  }
  lastDcMs = millis();

  printHelp();
}

void loop() {
  updateDc();
  updateSleep();
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    handleCommand(line);
  }
}