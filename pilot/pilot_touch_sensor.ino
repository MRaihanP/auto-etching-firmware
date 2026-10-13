/*
  Pilot: Non-Contact Water Level Sensor via Nickel Strip
  Phase 1: Touch Detection + LED RGB Indicator
  
  Pin Mapping:
    GPIO11 = HCl       → LED Merah
    GPIO12 = H2O2      → LED Ungu
    GPIO13 = Air       → LED Biru
    GPIO14 = Air Sabun → LED Hijau
    GPIO3  = Limbah    → LED Putih
  
  LED RGB NeoPixel: GPIO48
  
  Data kalibrasi awal (sentuhan langsung):
    Idle (tidak disentuh): ~110,000 - 116,000
    Touched (disentuh):   ~152,000 - 161,000
    Threshold: 135,000 (midpoint)
  
  Cara pakai:
    1. Upload sketch ini
    2. Buka Serial Plotter (Tools → Serial Plotter)
    3. Sentuh strip nikel → lihat nilai naik + LED menyala
*/

#include <Adafruit_NeoPixel.h>

// ================= KONFIGURASI =================
#define PIN_NEOPIXEL    48
#define NUM_PIXELS      5
#define BRIGHTNESS      50
#define TOUCH_THRESHOLD 135000  // midpoint antara idle (~113k) dan touched (~156k)

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

Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("=== Pilot Touch Sensor ==="));
  Serial.println(F("Sentuh strip nikel untuk melihat perubahan nilai"));
  Serial.println(F("Format: HCl | H2O2 | Air | Sabun | Limbah"));
  Serial.println(F(""));
  
  pixels.begin();
  pixels.setBrightness(BRIGHTNESS);
  pixels.clear();
  pixels.show();
  
  // Test LED: semua menyala putih 500ms
  for (int i = 0; i < NUM_PIXELS; i++) {
    pixels.setPixelColor(i, 0xFFFFFF);
  }
  pixels.show();
  delay(500);
  pixels.clear();
  pixels.show();
}

// ================= LOOP =================
void loop() {
  // Baca semua sensor secara bergantian
  int nilaiTouch[5];
  for (int i = 0; i < 5; i++) {
    nilaiTouch[i] = touchRead(PIN_SENSOR[i]);
  }
  
  // Print ke Serial Plotter (format: nilai1 nilai2 nilai3 nilai4 nilai5)
  for (int i = 0; i < 5; i++) {
    Serial.print(nilaiTouch[i]);
    if (i < 4) Serial.print(F(" "));
  }
  Serial.println();
  
  // Update LED berdasarkan threshold
  for (int i = 0; i < 5; i++) {
    if (nilaiTouch[i] > TOUCH_THRESHOLD) {
      // Strip disentuh → LED menyala dengan warna sesuai sensor
      pixels.setPixelColor(i, WARNA_SENSOR[i]);
    } else {
      // Tidak disentuh → LED mati
      pixels.setPixelColor(i, 0x000000);
    }
  }
  pixels.show();
  
  delay(50);  // ~20Hz sampling rate
}
