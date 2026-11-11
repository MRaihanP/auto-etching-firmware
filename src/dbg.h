#pragma once
#include <Arduino.h>
#include <HWCDC.h>
#include "config.h"

// ================= LOG DUAL-PORT =================
// Port USB ESP32-S3 berbeda tergantung setting IDE (USBMode / USB CDC On Boot):
//   - USB CDC On Boot = ON  -> objek USB adalah `Serial`
//   - USB CDC On Boot = OFF -> objek USB adalah `USBSerial`
// Kadang IDE juga memakai TinyUSB. Supaya log selalu muncul, firmware menulis
// ke UART0 (Serial) DAN ke port USB sekaligus; port yang sama dicek agar tidak
// dobel. Port USB otomatis aktif saat host membuka monitor.
//
// Sejak v1.0.0 penulisan USB hanya dilakukan bila host benar-benar terhubung.
// Ini mencegah loop tersendat saat alat berdiri sendiri tanpa kabel USB
// (beberapa versi core memblokir bila TX buffer penuh).

#if defined(ARDUINO_USB_MODE) && (ARDUINO_USB_MODE == 1)
  #if ARDUINO_USB_CDC_ON_BOOT
    #define DBG_USB (&Serial)      // objek USB = Serial
  #else
    #define DBG_USB (&USBSerial)   // objek USB = USBSerial
  #endif
#else
  #define DBG_USB (&Serial)        // TinyUSB/polos: Serial sudah port USB
#endif

#if LOG_LEVEL >= 1

struct DbgPort {
  static bool usbSiap() {
    // operator bool() pada HWCDC/USBCDC = ada host yang membuka port.
    return (bool)(*DBG_USB);
  }

  void usb(const char* s) {
    if (s == NULL) return;
    if (DBG_USB != (Print*)&Serial && usbSiap()) DBG_USB->print(s);
  }
  void usbFlash(const __FlashStringHelper* s) {
    if (DBG_USB != (Print*)&Serial && usbSiap()) DBG_USB->print(s);
  }
  void kirim(const char* s) {
    if (s == NULL) return;
    Serial.print(s);
    usb(s);
  }
  void kirim(const __FlashStringHelper* s) {
    Serial.print(s);
    usbFlash(s);
  }
  void baris() { kirim("\r\n"); }

  void print(const char* s)                { kirim(s); }
  void print(char c)                       { Serial.print(c); char b[2] = { c, 0 }; usb(b); }
  void print(int v)                        { Serial.print(v); char b[16]; snprintf(b, sizeof(b), "%d", v); usb(b); }
  void print(unsigned int v)               { Serial.print(v); char b[16]; snprintf(b, sizeof(b), "%u", v); usb(b); }
  void print(long v)                       { Serial.print(v); char b[24]; snprintf(b, sizeof(b), "%ld", v); usb(b); }
  void print(unsigned long v)              { Serial.print(v); char b[24]; snprintf(b, sizeof(b), "%lu", v); usb(b); }
  void print(double v)                     { Serial.print(v); char b[32]; snprintf(b, sizeof(b), "%.3f", v); usb(b); }
  void print(const __FlashStringHelper* s) { kirim(s); }

  void println()                           { baris(); }
  void println(const char* s)              { kirim(s); baris(); }
  void println(char c)                     { print(c); baris(); }
  void println(int v)                      { print(v); baris(); }
  void println(unsigned int v)             { print(v); baris(); }
  void println(long v)                     { print(v); baris(); }
  void println(unsigned long v)            { print(v); baris(); }
  void println(double v)                   { print(v); baris(); }
  void println(const __FlashStringHelper* s) { kirim(s); baris(); }
};

// Log detail (LOG_LEVEL 3). Dipakai untuk diagnostik yang tidak perlu selalu
// tampil, misalnya nilai mentah sensor tiap detik.
struct DbgVerbose {
  template <typename T> void print(T) {}
  template <typename T> void println(T) {}
  void println() {}
};

#else  // LOG_LEVEL == 0

struct DbgPort {
  template <typename T> void print(T) {}
  template <typename T> void println(T) {}
  void println() {}
};
struct DbgVerbose {
  template <typename T> void print(T) {}
  template <typename T> void println(T) {}
  void println() {}
};

#endif

DbgPort dbg;
DbgVerbose dbgV;

inline void dbgInit() {
  Serial.begin(115200);        // UART0 (GPIO43/44) - untuk USB-UART eksternal
  DBG_USB->begin();            // USB CDC / USB Serial-JTAG - port yang dipakai IDE
  // Timeout tulis 0 = jangan pernah menunggu host. Aman untuk alat standalone.
  DBG_USB->setTxTimeoutMs(0);
  delay(20);
  dbg.println(F("[BOOT] Log aktif: UART0 + USB"));
}
