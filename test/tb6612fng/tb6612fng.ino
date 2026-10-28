#define PWMA_PIN 15
#define PWMB_PIN 16

const int PWM_FREQ = 20000;
const int PWM_RES  = 8;
int dutyOn = 200;  // 0-255

void setPWM(int pin, int channel, int val) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, val);
#else
  ledcWrite(channel, val);
#endif
}

void setup() {
  Serial.begin(115200);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PWMA_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(PWMB_PIN, PWM_FREQ, PWM_RES);
#else
  ledcSetup(0, PWM_FREQ, PWM_RES);
  ledcSetup(1, PWM_FREQ, PWM_RES);
  ledcAttachPin(PWMA_PIN, 0);
  ledcAttachPin(PWMB_PIN, 1);
#endif

  setPWM(PWMA_PIN, 0, 0);
  setPWM(PWMB_PIN, 1, 0);

  Serial.println("Perintah:");
  Serial.println("  a on / a off   -> PWMA (GPIO17)");
  Serial.println("  b on / b off   -> PWMB (GPIO18)");
  Serial.println("  duty <0-255>   -> ubah kecepatan (default 200)");
  Serial.println("  off            -> matikan semua");
}

void loop() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "a on") {
    setPWM(PWMA_PIN, 0, dutyOn);
    Serial.println("PWMA (GPIO17) ON");
  } else if (cmd == "a off") {
    setPWM(PWMA_PIN, 0, 0);
    Serial.println("PWMA (GPIO17) OFF");
  } else if (cmd == "b on") {
    setPWM(PWMB_PIN, 1, dutyOn);
    Serial.println("PWMB (GPIO18) ON");
  } else if (cmd == "b off") {
    setPWM(PWMB_PIN, 1, 0);
    Serial.println("PWMB (GPIO18) OFF");
  } else if (cmd == "off") {
    setPWM(PWMA_PIN, 0, 0);
    setPWM(PWMB_PIN, 1, 0);
    Serial.println("Semua OFF");
  } else if (cmd.startsWith("duty ")) {
    dutyOn = constrain(cmd.substring(5).toInt(), 0, 255);
    Serial.print("Duty = ");
    Serial.println(dutyOn);
  } else if (cmd.length() > 0) {
    Serial.println("Perintah tidak dikenal");
  }
}