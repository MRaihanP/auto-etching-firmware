/*
  UJI MODUL TB6612FNG - 2 KANAL PWM (1 ARAH)
  ==========================================
  Sketch uji mandiri, tidak memakai file firmware utama.

  BOARD : ESP32-S3 DevKitC  (Arduino IDE: ESP32S3 Dev Module)

  ---------------------------------------------------------------
  WIRING
  ---------------------------------------------------------------
    TB6612FNG            ESP32-S3
    ------------------   ------------------
    VCC      --------->  3.3V
    VM       --------->  12V   (adaptor terpisah, bukan pin USB)
    GND      --------->  GND
    STBY     --------->  3.3V   (driver selalu aktif)
    AIN1     --------->  3.3V   } arah tetap maju:
    AIN2     --------->  GND    }
    BIN1     --------->  3.3V   } arah tetap maju:
    BIN2     --------->  GND    }
    PWMA     --------->  GPIO17 (Kanal A)
    PWMB     --------->  GPIO18 (Kanal B)
    AO1/AO2  --------->  Motor A
    BO1/BO2  --------->  Motor B

  AIN1/BIN1 = 3.3V dan AIN2/BIN2 = GND mengunci arah putar jadi MAJU.
  Firmware hanya mengatur kecepatan lewat PWM pada PWMA/PWMB.

  CATATAN: exhaust fan TIDAK lagi memakai modul ini. Arus kipas terlalu
  besar untuk TB6612FNG, jadi kipas disambung langsung ke 12V dan
  menyala terus. Kanal B (GPIO18) di firmware utama dibiarkan kosong.

  ---------------------------------------------------------------
  PERINGATAN
  ---------------------------------------------------------------
  - Motor 12V, arus < 1.2 A (batas TB6612FNG).
  - Adaptor 12V harus GND-nya ikut ke GND ESP32, kalau tidak PWM ter-noise.
  - Pasang 100 uF di dekat VM-GND module dan 0.1 uF keramik di tiap motor.
  - Duty 255 maksimal 60 detik terus-menerus.

  ---------------------------------------------------------------
  CARA PAKAI (Serial Monitor 115200 baud, Newline = ENTER)
  ---------------------------------------------------------------
    a 0-255   duty motor A (GPIO17)
    b 0-255   duty motor B (GPIO18)
    +  / -    duty motor A naik/turun 10
    o         stop semua
    s         uji otomatis semua motor
    h         bantuan
*/

#include <Arduino.h>

const int PIN_PWMA = 17;
const int PIN_PWMB = 18;

int dutyA = 0;
int dutyB = 0;

// ================= PWM =================
void setupPWM() {
  // 20 kHz: di atas frekuensi telinga, motor tidak menjengking.
  ledcAttach(PIN_PWMA, 20000, 8);
  ledcAttach(PIN_PWMB, 20000, 8);
  ledcWrite(PIN_PWMA, 0);
  ledcWrite(PIN_PWMB, 0);
}

void motorA(int duty) {
  dutyA = constrain(duty, 0, 255);
  ledcWrite(PIN_PWMA, dutyA);
}

void motorB(int duty) {
  dutyB = constrain(duty, 0, 255);
  ledcWrite(PIN_PWMB, dutyB);
}

void stopSemua() {
  motorA(0);
  motorB(0);
}

// ================= TAMPILKAN STATUS =================
void tampilStatus(const char* tag) {
  Serial.print(tag);
  Serial.print(F("A(GPIO"));
  Serial.print(PIN_PWMA);
  Serial.print(F(")="));
  Serial.print(dutyA);
  Serial.print(F("  B(GPIO"));
  Serial.print(PIN_PWMB);
  Serial.print(F(")="));
  Serial.print(dutyB);
  Serial.println();
}

// ================= UJI OTOMATIS (non-blocking) =================
struct TahapUji {
  int      dutyA;
  int      dutyB;
  unsigned lamaMs;
  char     nama[14];
};

static const TahapUji DAFTAR_UJI[] = {
  {  60,   0, 1500, "A duty 60"  },
  { 128,   0, 1500, "A duty 128" },
  { 255,   0, 1500, "A duty 255" },
  {   0,  60, 1500, "B duty 60"  },
  {   0, 128, 1500, "B duty 128" },
  {   0, 255, 1500, "B duty 255" },
  {  10,   0, 1200, "A step 10"  },
  {  20,   0, 1200, "A step 20"  },
  {  30,   0, 1200, "A step 30"  },
  {   0,  10, 1200, "B step 10"  },
  {   0,  20, 1200, "B step 20"  },
  {   0,  30, 1200, "B step 30"  },
  { 128, 128, 2000, "A+B 128"    },
  {   0,   0,  500, "STOP"       },
};
static const int JUMLAH_UJI = sizeof(DAFTAR_UJI) / sizeof(DAFTAR_UJI[0]);

bool     ujiJalan = false;
int      ujiIdx   = 0;
unsigned long ujiMulaiMs = 0;

void mulaiUji() {
  ujiJalan   = true;
  ujiIdx     = 0;
  ujiMulaiMs = millis();
  Serial.println(F("[TB6612] === MULAI UJI ==="));
}

void ujiUpdate() {
  if (!ujiJalan) return;
  if (millis() - ujiMulaiMs < DAFTAR_UJI[ujiIdx].lamaMs) return;

  const TahapUji &t = DAFTAR_UJI[ujiIdx];
  motorA(t.dutyA);
  motorB(t.dutyB);
  tampilStatus(t.nama);

  ujiIdx++;
  ujiMulaiMs = millis();
  if (ujiIdx >= JUMLAH_UJI) {
    ujiJalan = false;
    stopSemua();
    Serial.println(F("[TB6612] === UJI SELESAI ==="));
  }
}

// ================= BACA PERINTAH SERIAL =================
void prosesPerintah(char c) {
  // huruf kedua langsung dibaca: "a128" atau "a 128" sama saja
  if (c == 'a' || c == 'A') {
    stopSemua();
    while (Serial.available()) motorA(Serial.parseInt());
    tampilStatus(F("[CMD] "));
  }
  else if (c == 'b' || c == 'B') {
    stopSemua();
    while (Serial.available()) motorB(Serial.parseInt());
    tampilStatus(F("[CMD] "));
  }
  else if (c == '+') { stopSemua(); motorA(dutyA + 10); tampilStatus(F("[CMD] ")); }
  else if (c == '-') { stopSemua(); motorA(dutyA - 10); tampilStatus(F("[CMD] ")); }
  else if (c == 'o' || c == 'O') { stopSemua(); Serial.println(F("[TB6612] STOP")); }
  else if (c == 's' || c == 'S') { mulaiUji(); }
  else if (c == 'h' || c == 'H') {
    Serial.println(F("a 0-255 / b 0-255 / + - / o / s / h"));
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  setupPWM();
  Serial.println();
  Serial.println(F("== UJI TB6612FNG =="));
  Serial.println(F("PWA=GPIO17  PWB=GPIO18"));
  Serial.println(F("115200 baud, Newline=ENTER"));
  Serial.println(F("perintah: h"));
}

void loop() {
  ujiUpdate();
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c != '\n' && c != '\r') prosesPerintah(c);
  }
}
