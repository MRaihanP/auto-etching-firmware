#include <Adafruit_NeoPixel.h>

#define PIN_NEOPIXEL 48
#define NUM_PIXELS   1      // DevKitC hanya punya 1 NeoPixel

const int PIN_SENSOR[5] = { 11, 12, 13, 14, 3 };
const char* NAMA_SENSOR[5] = { "HCl", "H2O2", "Air", "Sabun", "Limbah" };

//                        HCl     H2O2    Air     Sabun   Limbah
const uint32_t THRESH[5] = {134500, 136000, 136500, 136500, 131000};
const uint32_t HYST = 3000;

const uint32_t WARNA_SENSOR[5] = {
  0xFF0000, 0x8000FF, 0x0000FF, 0x00FF00, 0xFFFFFF
};

Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
bool aktif[5] = {false, false, false, false, false};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("=== Pilot Touch Sensor ==="));

  pixels.begin();
  pixels.setBrightness(50);
  pixels.clear();
  pixels.show();

  // Tes LED: putih 500 ms
  pixels.setPixelColor(0, 0xFFFFFF);
  pixels.show();
  delay(500);
  pixels.clear();
  pixels.show();
}

void loop() {
  uint32_t nilai[5];
  int sensorAktifPertama = -1;

  for (int i = 0; i < 5; i++) {
    nilai[i] = touchRead(PIN_SENSOR[i]);

    if (!aktif[i] && nilai[i] > THRESH[i] + HYST) aktif[i] = true;
    else if (aktif[i] && nilai[i] < THRESH[i] - HYST) aktif[i] = false;

    if (aktif[i] && sensorAktifPertama < 0) sensorAktifPertama = i;

    Serial.print(nilai[i]);
    if (i < 4) Serial.print(' ');
  }
  Serial.println();

  // 1 LED: tampilkan warna sensor aktif pertama, mati jika tidak ada
  pixels.setPixelColor(0, sensorAktifPertama >= 0 ? WARNA_SENSOR[sensorAktifPertama] : 0);
  pixels.show();

  delay(50);
}