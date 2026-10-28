#pragma once
#include <Arduino.h>
#include <HWCDC.h>

// ================= LOG DUAL-PORT =================
// Port USB ESP32-S3 berbeda tergantung setting IDE (USBMode / USB CDC On Boot):
//   - USB CDC On Boot = ON  -> objek USB adalah `Serial`
//   - USB CDC On Boot = OFF -> objek USB adalah `USBSerial`
// Kadang IDE juga memakai TinyUSB. Supaya log selalu muncul, firmware menulis
// ke UART0 (Serial) DAN ke port USB sekaligus; port yang sama dicek agar tidak
// dobel. Port USB otomatis aktif saat host membuka monitor.

#if defined(ARDUINO_USB_MODE) && (ARDUINO_USB_MODE == 1)
  #if ARDUINO_USB_CDC_ON_BOOT
    #define DBG_USB (&Serial)      // objek USB = Serial
  #else
    #define DBG_USB (&USBSerial)   // objek USB = USBSerial
  #endif
#else
  #define DBG_USB (&Serial)        // TinyUSB/polos: Serial sudah port USB
#endif

struct DbgPort {
  void usb(const char* s) {
    if (DBG_USB != (Print*)&Serial) DBG_USB->print(s);
  }
  void usbFlash(const __FlashStringHelper* s) {
    if (DBG_USB != (Print*)&Serial) DBG_USB->print(s);
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
  void print(char c)                       { Serial.print(c); usb(String(c).c_str()); }
  void print(int v)                        { Serial.print(v); usb(String(v).c_str()); }
  void print(unsigned int v)               { Serial.print(v); usb(String(v).c_str()); }
  void print(long v)                       { Serial.print(v); usb(String(v).c_str()); }
  void print(unsigned long v)              { Serial.print(v); usb(String(v).c_str()); }
  void print(double v)                     { Serial.print(v); usb(String(v).c_str()); }
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

DbgPort dbg;

inline void dbgInit() {
  Serial.begin(115200);        // UART0 (GPIO43/44) - untuk USB-UART eksternal
  DBG_USB->begin();            // USB CDC / USB Serial-JTAG - port yang dipakai IDE
  delay(50);
  dbg.println(F("[BOOT] Log aktif: UART0 + USB"));
}