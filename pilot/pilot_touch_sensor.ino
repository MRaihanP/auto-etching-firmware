/*
  Pilot: Non-Contact Water Level Sensor via Nickel Strip
  Phase 2: Variable Measurement 0-100% + LED Brightness + Serial Command
  
  Pin Mapping:
    GPIO11 = HCl       → LED Merah
    GPIO12 = H2O2      → LED Ungu
    GPIO13 = Air       → LED Biru
    GPIO14 = Air Sabun → LED Hijau
    GPIO3  = Limbah    → LED Putih
  
  LED RGB NeoPixel: GPIO48 (1 LED onboard ESP32-S3 DevKitC-1)
  
  Serial Commands:
    0 = tampilkan semua sensor
    1 = HCl only
    2 = H2O2 only
    3 = Air only
    4 = Sabun only
    5 = Limbah only
    c = mode kalibrasi (set 0% dan 100%)
  
  Cara pakai:
    1. Upload sketch ini
    2. Buka Serial Monitor (115200 baud)
    3. Ketik 0 untuk lihat semua sensor
    4. Ketik 1-5 untuk lihat sensor tertentu
    5. LED brightness = persentase level cairan
*/

#include <Adafruit_NeoPixel.h>

// ================= KONFIGURASI =================
#define PIN_NEOPIXEL    48
#define NUM_PIXELS      1
#define BRIGHTNESS_MAX  255

// Pin sensor
const int PIN_SENSOR[5] = { 11, 12, 13, 14, 3 };
const char* NAMA_SENSOR[5] = { "HCl", "H2O2", "Air", "Sabun", "Limbah" };

// Warna LED per sensor (R, G, B)
const uint32_t WARNA_SENSOR[5] = {
  0xFF0000,  // Merah
  0x8000FF,  // Ungu
  0x0000FF,  // Biru
  0x00FF00,  // Hijau
  0xFFFFFF   // Putih
};

// Kalibrasi: nilai touch saat 0% (kosong) dan 100% (penuh)
// Default: idle ~113k, touched ~156k (dari data pengujian)
int kalibKosong[5] = { 113000, 113000, 113000, 116000, 110000 };
int kalibPenuh[5]  = { 156000, 161000, 161000, 157000, 152000 };

Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

int sensorPilih = 0;  // 0 = semua, 1-5 = sensor tertentu
bool modeKalibrasi = false;
int kalibStep = 0;    // 0 = set 0%, 1 = set 100%

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pixels.begin();
  pixels.setBrightness(50);
  pixels.clear();
  pixels.show();
  
  Serial.println(F("=== Pilot Touch Sensor Phase 2 ==="));
  Serial.println(F("Commands: 0=semua, 1-5=sensor, c=kalibrasi"));
  Serial.println(F(""));
  printMenu();
}

void printMenu() {
  Serial.println(F("--- Menu ---"));
  Serial.println(F("0 = tampilkan semua sensor"));
  Serial.println(F("1 = HCl only"));
  Serial.println(F("2 = H2O2 only"));
  Serial.println(F("3 = Air only"));
  Serial.println(F("4 = Sabun only"));
  Serial.println(F("5 = Limbah only"));
  Serial.println(F("c = mode kalibrasi"));
  Serial.println(F("-------------"));
}

// ================= KONVERSI =================
int kePersen(int nilai, int idx) {
  if (kalibPenuh[idx] <= kalibKosong[idx]) return 0;
  long persen = ((long)(nilai - kalibKosong[idx]) * 100) / (kalibPenuh[idx] - kalibKosong[idx]);
  if (persen < 0) persen = 0;
  if (persen > 100) persen = 100;
  return (int)persen;
}

// ================= LOOP =================
void loop() {
  // Baca semua sensor
  int nilaiTouch[5];
  for (int i = 0; i < 5; i++) {
    nilaiTouch[i] = touchRead(PIN_SENSOR[i]);
  }
  
  // Handle serial command
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd >= '0' && cmd <= '5') {
      sensorPilih = cmd - '0';
      modeKalibrasi = false;
      Serial.print(F("→ Sensor: "));
      if (sensorPilih == 0) Serial.println(F("Semua"));
      else Serial.println(NAMA_SENSOR[sensorPilih - 1]);
    }
    else if (cmd == 'c' || cmd == 'C') {
      modeKalibrasi = true;
      kalibStep = 0;
      Serial.println(F("=== Mode Kalibrasi ==="));
      Serial.println(F("1. Kosongkan jerican, lalu tekan 'k' untuk set 0%"));
    }
    else if (cmd == 'k' || cmd == 'K') {
      if (modeKalibrasi) {
        if (kalibStep == 0) {
          for (int i = 0; i < 5; i++) kalibKosong[i] = nilaiTouch[i];
          Serial.println(F("✓ 0% tersimpan. Isi jerican penuh, lalu tekan 'k' untuk set 100%"));
          kalibStep = 1;
        } else {
          for (int i = 0; i < 5; i++) kalibPenuh[i] = nilaiTouch[i];
          Serial.println(F("✓ 100% tersimpan. Kalibrasi selesai!"));
          modeKalibrasi = false;
          Serial.println(F("Nilai kalibrasi:"));
          for (int i = 0; i < 5; i++) {
            Serial.print(NAMA_SENSOR[i]);
            Serial.print(F(": 0%="));
            Serial.print(kalibKosong[i]);
            Serial.print(F("  100%="));
            Serial.println(kalibPenuh[i]);
          }
        }
      }
    }
  }
  
  // Mode kalibrasi: LED putih berkedip
  if (modeKalibrasi) {
    uint32_t warna = (millis() / 500) % 2 == 0 ? 0xFFFFFF : 0x000000;
    pixels.setPixelColor(0, warna);
    pixels.show();
    delay(50);
    return;
  }
  
  // Tampilkan & update LED
  if (sensorPilih == 0) {
    // Semua sensor
    for (int i = 0; i < 5; i++) {
      int persen = kePersen(nilaiTouch[i], i);
      Serial.print(NAMA_SENSOR[i]);
      Serial.print(F(": "));
      Serial.print(nilaiTouch[i]);
      Serial.print(F(" ("));
      Serial.print(persen);
      Serial.println(F("%)"));
    }
    Serial.println(F("---"));
  } else {
    // Sensor tertentu
    int idx = sensorPilih - 1;
    int persen = kePersen(nilaiTouch[idx], idx);
    Serial.print(NAMA_SENSOR[idx]);
    Serial.print(F(": "));
    Serial.print(nilaiTouch[idx]);
    Serial.print(F(" ("));
    Serial.print(persen);
    Serial.println(F("%)"));
    
    // LED: warna sensor, brightness = persen
    uint32_t warna = WARNA_SENSOR[idx];
    // Scale brightness by percentage
    uint8_t r = ((warna >> 16) & 0xFF) * persen / 100;
    uint8_t g = ((warna >> 8) & 0xFF) * persen / 100;
    uint8_t b = (warna & 0xFF) * persen / 100;
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
  }
  
  delay(100);
}
