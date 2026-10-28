#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>

// ================= FONT MINI 3x5 (untuk baris status proses) =================
// Font bawaan Adafruit (6x8 / size 2 = 12x16) terlalu besar untuk baris status
// di halaman proses yang hanya punya tinggi 44px. Font ini 3x5 (lebar 4px per
// karakter termasuk spasi), dipakai khusus untuk teks kecil seperti nama fase.
//
// Tabel di bawah sudah diverifikasi dengan render piksel per karakter.

// Tiap glyph = 3 kolom x 5 baris. Tiap kolom satu byte, bit r = baris r,
// LSB (bit 0) = baris paling atas.
const uint8_t FONT_MINI[][3] = {
  {  0,  0,  0 }, // ' '
  { 25,  4, 19 }, // '%'
  {  4,  4,  4 }, // '-'
  {  0, 16,  0 }, // '.'
  { 31, 17, 31 }, // '0'
  { 18, 31, 16 }, // '1'
  { 29, 21, 23 }, // '2'
  { 21, 21, 31 }, // '3'
  {  7,  4, 31 }, // '4'
  { 23, 21, 29 }, // '5'
  { 31, 21, 29 }, // '6'
  {  1, 25,  7 }, // '7'
  { 31, 21, 31 }, // '8'
  { 23, 21, 31 }, // '9'
  {  0, 10,  0 }, // ':'
  { 30,  5, 30 }, // 'A'
  { 31, 21, 10 }, // 'B'
  { 14, 17, 17 }, // 'C'
  { 31, 17, 14 }, // 'D'
  { 31, 21, 17 }, // 'E'
  { 31,  5,  1 }, // 'F'
  { 14, 17, 29 }, // 'G'
  { 31,  4, 31 }, // 'H'
  { 17, 31, 17 }, // 'I'
  {  8, 16, 15 }, // 'J'
  { 31,  4, 27 }, // 'K'
  { 31, 16, 16 }, // 'L'
  { 31,  6, 31 }, // 'M'
  { 31,  1, 30 }, // 'N'
  { 14, 17, 14 }, // 'O'
  { 31,  5,  2 }, // 'P'
  { 14, 25, 22 }, // 'Q'
  { 31,  5, 26 }, // 'R'
  { 18, 21,  9 }, // 'S'
  {  1, 31,  1 }, // 'T'
  { 31, 16, 31 }, // 'U'
  { 15, 16, 15 }, // 'V'
  { 31, 12, 31 }, // 'W'
  { 27,  4, 27 }, // 'X'
  {  3, 28,  3 }, // 'Y'
  { 25, 21, 19 }, // 'Z'
};
const char FONT_MINI_CHARS[] = " %-.0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const int  FONT_MINI_W  = 4;   // 3 kolom + 1 spasi
const int  FONT_MINI_H  = 5;

// Lebar teks dalam piksel (karakter di luar tabel dihitung 4px)
int miniLebar(const char* s) {
  int n = 0;
  for (const char* p = s; *p; p++) n++;
  return n ? n * FONT_MINI_W : 0;
}

// Gambar teks dengan font mini. Karakter di luar tabel (mis. huruf kecil)
// otomatis diklik ke huruf besarnya; yang tidak ada dilewati.
void teksMini(Adafruit_GFX &g, const char* s, int x, int y, uint16_t warna) {
  int cx = x;
  for (const char* p = s; *p; p++) {
    char c = *p;
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');

    const char* ketemu = strchr(FONT_MINI_CHARS, c);
    if (ketemu && c != '\0') {
      int idx = (int)(ketemu - FONT_MINI_CHARS);
      for (int k = 0; k < 3; k++) {
        uint8_t kolom = FONT_MINI[idx][k];
        for (int r = 0; r < 5; r++)
          if (kolom & (1 << r)) g.drawPixel(cx + k, y + r, warna);
      }
    }
    cx += FONT_MINI_W;
  }
}

void teksMiniTengah(Adafruit_GFX &g, const char* s, int y, uint16_t warna) {
  teksMini(g, s, (g.width() - miniLebar(s)) / 2, y, warna);
}

void teksMiniKanan(Adafruit_GFX &g, const char* s, int xKanan, int y, uint16_t warna) {
  teksMini(g, s, xKanan - miniLebar(s), y, warna);
}