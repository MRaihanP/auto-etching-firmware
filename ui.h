#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "data.h"
#include "process.h"
#include "presetops.h"
#include "sensor.h"
#include "fontmini.h"

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= OFFSET PANEL RUSAK =================
// Semua elemen digambar pada koordinat virtual 0..SCREEN_HEIGHT-1.
// Sebelum dikirim ke OLED, framebuffer digeser ke bawah UI_Y_OFFSET px supaya
// baris atas yang rusak tidak dipakai. Konsekuensinya baris paling bawah
// terpotong, jadi semua layout dibatasi pada AREA_BAWAH.
const int AREA_BAWAH = SCREEN_HEIGHT - UI_Y_OFFSET;

// Geser framebuffer ke bawah dy baris piksel (in-place, dari bawah ke atas).
// CATATAN: buffer SSD1306 tersusun per HALAMAN: baris piksel y disimpan di
// byte (y/8)*WIDTH pada bit (y%8). Jadi geseran harus dihitung per halaman,
// bukan per byte - menggeser dy*WIDTH byte akan menggeser 8x terlalu jauh.
void geserGambar(int dy) {
  if (dy <= 0) return;
  if (dy >= SCREEN_HEIGHT) { oled.clearDisplay(); return; }

  uint8_t* buf   = oled.getBuffer();
  int halaman    = SCREEN_HEIGHT / 8;
  int geserHlm   = dy / 8;          // geser halaman utuh
  int geserBit   = dy % 8;          // sisa geser di dalam halaman

  for (int p = halaman - 1; p >= 0; p--) {
    int s = p - geserHlm;           // halaman asal
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      uint8_t v = 0;
      if (s >= 0) {
        v = (uint8_t)(buf[s * SCREEN_WIDTH + x] << geserBit);   // bit tetap di halaman ini
        if (geserBit && s - 1 >= 0)
          v |= (uint8_t)(buf[(s - 1) * SCREEN_WIDTH + x] >> (8 - geserBit));
      }
      buf[p * SCREEN_WIDTH + x] = v;
    }
  }
}

// ================= STATE LIST + ANIMASI =================
#define L_UTAMA   0
#define L_PRESET  1
#define L_CFG     2
#define L_AKSI    3
#define JUMLAH_LIST 4

int           lsSel[JUMLAH_LIST]        = {0};
float         lsPos[JUMLAH_LIST]        = {0};
float         lsAwal[JUMLAH_LIST]       = {0};
float         lsTujuan[JUMLAH_LIST]     = {0};
unsigned long lsWaktuMulai[JUMLAH_LIST] = {0};
bool          lsAnimasi[JUMLAH_LIST]    = {false};

const unsigned int DURASI_ANIMASI_MS = 200;

void listGeser(int L, int delta, int jumlahItem) {
  int baru = lsSel[L] + delta;
  if (baru < 0 || baru > jumlahItem - 1) return;   // mentok di ujung
  lsSel[L]        = baru;
  lsAwal[L]       = lsPos[L];
  lsTujuan[L]     = baru;
  lsWaktuMulai[L] = millis();
  lsAnimasi[L]    = true;
}

void listReset(int L) {
  lsSel[L] = 0; lsPos[L] = 0; lsAwal[L] = 0;
  lsTujuan[L] = 0; lsAnimasi[L] = false;
}

// Lompat ke index tanpa animasi
void listSet(int L, int idx) {
  lsSel[L] = idx; lsPos[L] = idx; lsAwal[L] = idx;
  lsTujuan[L] = idx; lsAnimasi[L] = false;
}

void listUpdateAnimasi(int L) {
  if (!lsAnimasi[L]) return;
  float t = (millis() - lsWaktuMulai[L]) / (float)DURASI_ANIMASI_MS;
  if (t >= 1.0) {
    lsPos[L] = lsTujuan[L];
    lsAnimasi[L] = false;
  } else {
    float te = 1.0 - (1.0 - t) * (1.0 - t);   // ease-out
    lsPos[L] = lsAwal[L] + (lsTujuan[L] - lsAwal[L]) * te;
  }
}

// ================= GEOMETRI =================
const int BOX_MARGIN_X = 4;
const int BOX_RADIUS   = 5;
const int HEADER_H     = 12;

// ================= HELPER GAMBAR =================
void teksTengah(const char* s, int y, int ukuran) {
  int16_t x1, y1; uint16_t w, h;
  oled.setTextSize(ukuran);
  oled.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((SCREEN_WIDTH - w) / 2, y);
  oled.print(s);
}

void teksKanan(const char* s, int xKanan, int y) {
  int16_t x1, y1; uint16_t w, h;
  oled.setTextSize(1);
  oled.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor(xKanan - w, y);
  oled.print(s);
}

void gambarHeader(const char* judul) {
  oled.fillRect(0, 0, SCREEN_WIDTH, HEADER_H, WHITE);
  oled.setTextColor(BLACK);
  teksTengah(judul, 2, 1);
  oled.setTextColor(WHITE);
}

// List generik dengan kotak seleksi statik di tengah area
void gambarList(const char* const* items, int jumlah, int L,
                int areaAtas, int rowHeight) {
  int areaTinggi = SCREEN_HEIGHT - areaAtas;
  int centerY    = areaAtas + areaTinggi / 2;

  oled.setTextColor(WHITE);
  oled.setTextSize(1);

  for (int j = 0; j < jumlah; j++) {
    float y = centerY + (j - lsPos[L]) * rowHeight;
    if (y < areaAtas - rowHeight || y > SCREEN_HEIGHT + rowHeight) continue;

    int16_t x1, y1; uint16_t w, h;
    oled.getTextBounds(items[j], 0, 0, &x1, &y1, &w, &h);
    int ty = (int)round(y - h / 2.0);
    if (ty + (int)h <= areaAtas) continue;   // jangan tumpang tindih header
    oled.setCursor((SCREEN_WIDTH - w) / 2, ty);
    oled.print(items[j]);
  }

  oled.drawRoundRect(BOX_MARGIN_X, centerY - rowHeight / 2,
                     SCREEN_WIDTH - 2 * BOX_MARGIN_X, rowHeight,
                     BOX_RADIUS, WHITE);
}

// ================= HALAMAN: MAIN MENU =================
void gambarMenuUtama() {
  gambarList(menuUtama, JUMLAH_MENU, L_UTAMA, 0, 20);
}

// ================= HALAMAN: PRESET LIST =================
void gambarPresetKosong() {
  gambarHeader("Pilih Preset");

  int cx = SCREEN_WIDTH / 2;
  int topY = 16, botY = 44, halfW = 16;
  oled.drawTriangle(cx, topY, cx - halfW, botY, cx + halfW, botY, WHITE);
  oled.drawTriangle(cx, topY + 2, cx - halfW + 2, botY - 1, cx + halfW - 2, botY - 1, WHITE);
  oled.drawFastVLine(cx, topY + 10, 10, WHITE);
  oled.drawPixel(cx, botY - 4, WHITE);

  teksTengah("Belum ada preset", 48, 1);
  teksTengah("yang dibuat", 56, 1);
}

void gambarPresetList() {
  if (jumlahPreset == 0) { gambarPresetKosong(); return; }

  gambarHeader("Pilih Preset");

  const char* nama[MAKS_PRESET];
  for (int i = 0; i < jumlahPreset; i++) nama[i] = daftarPreset[i].nama;

  gambarList(nama, jumlahPreset, L_PRESET, HEADER_H, 16);
}

// ================= HALAMAN: DETAIL PRESET =================
void formatMMSS(int totalDetik, char* buf) {
  sprintf(buf, "%02d:%02d", totalDetik / 60, totalDetik % 60);
}

bool terlihat(int yPos, int tinggi) {
  return (yPos + tinggi > HEADER_H) && (yPos < AREA_BAWAH);
}

void gambarIndikatorScroll(int scroll, int maxScroll) {
  const int x = SCREEN_WIDTH - 6;
  if (scroll > 0) {
    oled.fillTriangle(x - 3, HEADER_H + 4, x + 3, HEADER_H + 4, x, HEADER_H + 1, WHITE);
  }
  if (scroll < maxScroll) {
    oled.fillTriangle(x - 3, AREA_BAWAH - 4, x + 3, AREA_BAWAH - 4, x, AREA_BAWAH - 1, WHITE);
  }
}

void gambarPresetDetail() {
  Preset &p = daftarPreset[presetTerpilih];
  gambarHeader(p.nama);

  char buf[24];
  oled.setTextColor(WHITE);
  oled.setTextSize(1);

  int maxScroll = max(0, DETAIL_CONTENT_H - (AREA_BAWAH - HEADER_H));
  if (presetDetailScroll > maxScroll) presetDetailScroll = maxScroll;

  int y = HEADER_H + 3 - presetDetailScroll;
  const int dy = 8;
  const int xKanan = SCREEN_WIDTH - 2;

  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("PCB max"));
    sprintf(buf, "%dx%d mm", p.pcbPanjang, p.pcbLebar);
    teksKanan(buf, xKanan, y);
  }
  y += dy;

  if (terlihat(y, 1)) oled.drawFastHLine(0, y, SCREEN_WIDTH, WHITE);
  y += 3;

  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("HCl"));
    sprintf(buf, "%d%%  %d ml", p.persenHCl, p.mlHCl);
    teksKanan(buf, xKanan, y);
  }
  y += dy;

  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("H2O2"));
    sprintf(buf, "%d%%  %d ml", p.persenH2O2, p.mlH2O2);
    teksKanan(buf, xKanan, y);
  }
  y += dy;

  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("H2O"));
    sprintf(buf, "%d%%  %d ml", p.persenH2O, p.mlH2O);
    teksKanan(buf, xKanan, y);
  }
  y += dy;

  int barY = y + 1, barH = 4, barW = SCREEN_WIDTH - 8;
  if (terlihat(barY, barH)) {
    int w1 = barW * p.persenHCl  / 100;
    int w2 = barW * p.persenH2O2 / 100;
    oled.drawRect(4, barY, barW, barH, WHITE);
    oled.fillRect(4, barY, w1, barH, WHITE);                  // HCl: solid
    for (int x = 4 + w1; x < 4 + w1 + w2; x += 2)             // H2O2: arsir
      oled.drawFastVLine(x, barY + 1, barH - 2, WHITE);
  }
  y += barH + 4;                                               // H2O: kosong

  if (terlihat(y, 1)) oled.drawFastHLine(0, y, SCREEN_WIDTH, WHITE);
  y += 3;

  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("Agitasi"));
    sprintf(buf, "%ds  Spd %d", p.agitasiDetik, p.agitasiSpeed);
    teksKanan(buf, xKanan, y);
  }
  y += dy;

  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("Total"));
    formatMMSS(p.totalDetik, buf);
    teksKanan(buf, xKanan, y);
  }

  gambarIndikatorScroll(presetDetailScroll, maxScroll);
}

// ================= HALAMAN: PROSES =================
// Hanya MENGGAMBAR state dari process.h (tidak mengubah state proses)
void gambarDialog(const char* l1, const char* l2, const char* l3) {
  oled.fillRect(10, 14, 108, 38, BLACK);
  oled.drawRect(10, 14, 108, 38, WHITE);
  oled.setTextColor(WHITE);
  oled.setTextSize(1);
  teksTengah(l1, 19, 1);
  teksTengah(l2, 32, 1);
  teksTengah(l3, 41, 1);
}

void gambarProses() {
  Preset &p = daftarPreset[presetTerpilih];

  unsigned long totalMs = (unsigned long)p.totalDetik * 1000UL;
  int lewatDetik = prosesAkumMs / 1000;
  float frac = (float)prosesAkumMs / (float)totalMs;
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;

  // Tanpa header. Urutan dari atas: timer, loading bar, status (paling kecil).
  oled.setTextColor(WHITE);

  char buf[24];
  int persen = (int)(frac * 100);

  // --- Baris 1: nama layer singkat (kiri) + persen (kanan) -------------
  char layer[8];
  strncpy(layer, p.nama, sizeof(layer) - 1);
  layer[sizeof(layer) - 1] = '\0';
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(layer);
  sprintf(buf, "%d%%", persen);
  teksKanan(buf, SCREEN_WIDTH, 0);                       // 0..7

  // --- Timer, diperkecil (size 1, bukan size 2) -------------------------
  formatMMSS(lewatDetik, buf);
  teksTengah(buf, 10, 1);                                // 10..17

  // --- Loading bar ------------------------------------------------------
  int barX = 0, barY = 20, barW = SCREEN_WIDTH, barH = 7;  // 20..26
  oled.drawRoundRect(barX, barY, barW, barH, 3, WHITE);
  int isi = (int)((barW - 6) * frac);
  if (isi > 0) oled.fillRect(barX + 3, barY + 1, isi, barH - 2, WHITE);

  // --- Sisa waktu, tetap di bawah timer, sebelum bar -------------------
  char sisa[12];
  formatMMSS(p.totalDetik - lewatDetik, sisa);
  sprintf(buf, "sisa %s", sisa);
  teksTengah(buf, 28, 1);                                // 28..35

  // --- Status, ukuran paling kecil (font mini 3x5) ---------------------
  const char* st = prosesJeda ? "JEDA" : namaFase[prosesFase];
  teksMiniTengah(oled, st, 38, WHITE);                   // 38..42
}

// ================= HALAMAN: PLACEHOLDER =================
void gambarPlaceholder() {
  gambarHeader(menuUtama[lsSel[L_UTAMA]]);
  teksTengah("Halaman ini", 26, 1);
  teksTengah("belum tersedia", 36, 1);
  oled.setTextSize(1);
  teksTengah("[BACK] kembali", 52, 1);
}

// ================= HALAMAN: KONFIGURASI PRESET =================
const char* const itemAksi[3] = { "Edit", "Duplikat", "Hapus" };
const char* const TEKS_TAMBAH = "+ Tambah Preset";

int jumlahItemCfg() {
  return jumlahPreset + (jumlahPreset < MAKS_PRESET ? 1 : 0);
}

void gambarCfgList() {
  gambarHeader("Konfigurasi");
  const char* item[MAKS_PRESET + 1];
  for (int i = 0; i < jumlahPreset; i++) item[i] = daftarPreset[i].nama;
  int n = jumlahPreset;
  if (jumlahPreset < MAKS_PRESET) item[n++] = TEKS_TAMBAH;
  gambarList(item, n, L_CFG, HEADER_H, 16);
}

void gambarCfgAksi() {
  gambarHeader(daftarPreset[cfgIdx].nama);
  gambarList(itemAksi, 3, L_AKSI, HEADER_H, 16);
}

const int EDIT_ROW_H   = 9;
const int EDIT_VISIBLE = 4;

void formatField(const Preset &p, int f, char* buf) {
  switch (f) {
    case 0: sprintf(buf, "%d mm", p.pcbPanjang); break;
    case 1: sprintf(buf, "%d mm", p.pcbLebar);   break;
    case 2: sprintf(buf, "%d%%", p.persenHCl);   break;
    case 3: sprintf(buf, "%d%%", p.persenH2O2);  break;
    case 4: sprintf(buf, "%d%% auto", p.persenH2O); break;
    case 5: sprintf(buf, "%d ml", volumeTotal(p)); break;
    case 6: sprintf(buf, "%d s", p.agitasiDetik); break;
    case 7: sprintf(buf, "%d", p.agitasiSpeed);   break;
    case 8: formatMMSS(p.totalDetik, buf);        break;
  }
}

void gambarCfgEdit() {
  char judul[20];
  snprintf(judul, sizeof(judul), "%s%s", presetEdit.nama, editBerubah ? " *" : "");
  gambarHeader(judul);

  oled.setTextSize(1);
  for (int r = 0; r < EDIT_VISIBLE; r++) {
    int f = editScroll + r;
    if (f >= JUMLAH_FIELD) break;
    int y = HEADER_H + 1 + r * EDIT_ROW_H;
    bool sel = (f == editField);
    if (sel) oled.fillRect(0, y, SCREEN_WIDTH - 5, EDIT_ROW_H, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(2, y + 1);
    oled.print(namaField[f]);
    char buf[16];
    formatField(presetEdit, f, buf);
    teksKanan(buf, SCREEN_WIDTH - 8, y + 1);
  }
  oled.setTextColor(WHITE);

  // scrollbar tipis di kanan
  int trackY = HEADER_H + 1, trackH = SCREEN_HEIGHT - trackY;
  int thumbH = trackH * EDIT_VISIBLE / JUMLAH_FIELD;
  int thumbY = trackY + (trackH - thumbH) * editScroll / (JUMLAH_FIELD - EDIT_VISIBLE);
  oled.fillRect(SCREEN_WIDTH - 2, thumbY, 2, thumbH, WHITE);
}

// ================= HALAMAN: LEVEL CAIRAN =================
// Layout: nama (kiri) + persen (kanan) + bar progres di bawah.
// Scroll tanpa cursor, seperti preset detail.
void gambarLevel() {
  gambarHeader("Level Cairan");

  if (!sensorOk) {
    teksTengah("Sensor tidak", 22, 1);
    teksTengah("aktif", 32, 1);
    teksTengah("[BACK] kembali", 52, 1);
    return;
  }

  const int ROW_H = 18;
  const int CONTENT_H = JUMLAH_SENSOR * ROW_H + 4;
  int maxScroll = max(0, CONTENT_H - (AREA_BAWAH - HEADER_H));
  if (levelScroll > maxScroll) levelScroll = maxScroll;

  int y = HEADER_H + 2 - levelScroll;
  const int barX = 1;
  const int barW = SCREEN_WIDTH - 2;
  const int barH = 6;

  oled.setTextSize(1);
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    if (y + ROW_H < HEADER_H || y > AREA_BAWAH) { y += ROW_H; continue; }

    oled.setTextColor(WHITE);
    oled.setCursor(1, y);
    oled.print(namaSensor[i]);

    char buf[12];
    sprintf(buf, "%d%%", sensorPersen[i]);
    teksKanan(buf, SCREEN_WIDTH - 1, y);

    int barY = y + 9;
    oled.drawRect(barX, barY, barW, barH, WHITE);
    int isi = (barW - 2) * sensorPersen[i] / 100;
    if (isi > 0) oled.fillRect(barX + 1, barY + 1, isi, barH - 2, WHITE);

    y += ROW_H;
  }

  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 4;
    if (levelScroll > 0) {
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    }
    if (levelScroll < maxScroll) {
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
    }
  }
}

// ================= HALAMAN: KALIBRASI MAIN =================
const char* const menuKalib[] = { "Motor", "Sensor Level", "Sensor Suhu", "Sensor Udara" };
const int JUMLAH_MENU_KALIB = 4;

// ================= HALAMAN: KALIBRASI MOTOR LIST =================
const char* const menuMotor[] = {
  "Pompa HCl", "Pompa H2O2", "Pompa Air",
  "Pompa Sabun", "Pompa Limbah", "Motor Agitasi",
  "Test Motor"
};
const int JUMLAH_MENU_MOTOR = 7;

void gambarKalibMotorList() {
  gambarHeader("Motor");
  const char* item[JUMLAH_MENU_MOTOR];
  for (int i = 0; i < JUMLAH_MENU_MOTOR; i++) item[i] = menuMotor[i];
  gambarList(item, JUMLAH_MENU_MOTOR, L_AKSI, HEADER_H, 16);
}

// ================= HALAMAN: KALIBRASI STEPPER (Pompa HCl/H2O2/Air) =================
// Baris: START/STOP (uji target ml), RPM, Target, Hasil ukur, Offset,
// ml/rotasi, Uji hasil. Start/stop memakai ramp yang sama seperti Test Motor.
// Konten lebih tinggi dari area tampil -> scroll.
const int KALIB_STEPPER_ROW_TOP[7] = { 0, 14, 24, 34, 44, 54, 64 };
const int KALIB_STEPPER_ROW_H[7]   = { 12, 10, 10, 10, 10, 10, 10 };
const int KALIB_STEPPER_CONTENT_H  = 74;

void gambarKalibStepper() {
  const char* nama[] = { "HCl", "H2O2", "Air" };
  char judul[20];
  sprintf(judul, "Kal %s", nama[kalibMotorIdx]);
  gambarHeader(judul);

  int idx = kalibMotorIdx;
  int maxScroll = max(0, KALIB_STEPPER_CONTENT_H - (AREA_BAWAH - HEADER_H - 4));
  if (kalibStepperScroll < 0) kalibStepperScroll = 0;
  if (kalibStepperScroll > maxScroll) kalibStepperScroll = maxScroll;

  int baseY = HEADER_H + 4 - kalibStepperScroll;
  char buf[20];

  oled.setTextSize(1);

  // --- Baris 0: START / STOP (uji target ml) ---
  {
    int y = baseY + KALIB_STEPPER_ROW_TOP[0];
    if (y + 12 > HEADER_H && y < AREA_BAWAH) {
      bool sel = (kalibStepperCursor == 0);
      if (sel) {
        oled.fillRoundRect(20, y - 1, SCREEN_WIDTH - 40, 12, 4, WHITE);
        oled.setTextColor(BLACK);
      } else {
        oled.drawRoundRect(20, y - 1, SCREEN_WIDTH - 40, 12, 4, WHITE);
        oled.setTextColor(WHITE);
      }
      if (pompaKalibRamp(idx)) {
        strcpy(buf, "RAMP...");
      } else if (pompaAktif(idx)) {
        sprintf(buf, "STOP %d%%", pompaProgres(idx));
      } else {
        strcpy(buf, "START");
      }
      teksTengah(buf, y + 2, 1);
    }
  }

  // --- Baris 1: RPM ---
  {
    int y = baseY + KALIB_STEPPER_ROW_TOP[1];
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      bool sel = (kalibStepperCursor == 1);
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      oled.print(F("RPM"));
      sprintf(buf, "%d", rpmPompaValid(kalibStepperRpm[idx]));
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  // --- Baris 2: target ml yang akan didosis ---
  {
    int y = baseY + KALIB_STEPPER_ROW_TOP[2];
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      bool sel = (kalibStepperCursor == 2);
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      sprintf(buf, "Target x%d", kalibStepperTgtStep);
      oled.print(buf);
      sprintf(buf, "%d ml", kalibStepperTargetMl[idx]);
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  // --- Baris 3: hasil ukur nyata (ENTER = hitung offset) ---
  {
    int y = baseY + KALIB_STEPPER_ROW_TOP[3];
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      bool sel = (kalibStepperCursor == 3);
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      oled.print(F("Hasil"));
      if (kalibStepperUkurMl10[idx] > 0)
        sprintf(buf, "%.1f ml", kalibStepperUkurMl10[idx] / 10.0f);
      else
        strcpy(buf, "--");
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  // --- Baris 4: offset terakhir (hasil hitung, % terhadap target) ---
  {
    int y = baseY + KALIB_STEPPER_ROW_TOP[4];
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      bool sel = (kalibStepperCursor == 4);
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      oled.print(F("Offset"));
      if (kalibStepperOffsetAda[idx])
        sprintf(buf, "%+.1f%%", kalibStepperOffsetX10[idx] / 10.0f);
      else
        strcpy(buf, "--");
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  // --- Baris 5: ml per rotasi (hasil kalibrasi) ---
  {
    int y = baseY + KALIB_STEPPER_ROW_TOP[5];
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      bool sel = (kalibStepperCursor == 5);
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      oled.print(F("ml/rotasi"));
      if (kalibStepperMl[idx] > 0.0f) sprintf(buf, "%.3f", kalibStepperMl[idx]);
      else strcpy(buf, "--");
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  // --- Baris 6: Uji hasil (verifikasi kalibrasi tersimpan) ---
  {
    int y = baseY + KALIB_STEPPER_ROW_TOP[6];
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      bool sel = (kalibStepperCursor == 6);
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      oled.print(F("Uji hasil"));
      if (kalibStepperUjiMode[idx]) {
        strcpy(buf, pompaAktif(idx) ? "RUN" : "ISI HASIL");
      } else if (kalibStepperUjiAda[idx]) {
        strcpy(buf, kalibStepperUjiLulus[idx] ? "LULUS" : "GAGAL");
      } else {
        strcpy(buf, "ENTER");
      }
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  oled.setTextColor(WHITE);

  // --- Indikator scroll ---
  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 4;
    if (kalibStepperScroll > 0)
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    if (kalibStepperScroll < maxScroll)
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
  }
}

// ================= HALAMAN: TEST MOTOR (semua motor) =================
// Baris: Semua / HCl / H2O2 / Air / Sabun / Limbah / Agitasi.
// ATAS/BAWAH pilih, KIRI/KANAN ubah nilai (RPM/duty/SPD), ENTER START/STOP.
// Semua motor selalu ramp dari 0 ke target (stepper: akselerasi, DC: duty).
#define TEST_MOTOR_JUMLAH 7
const char* const testMotorLabel[TEST_MOTOR_JUMLAH] = {
  "Semua", "HCl", "H2O2", "Air", "Sabun", "Limbah", "Agitasi"
};
const int TEST_MOTOR_ROW_H = 9;
const int TEST_MOTOR_TOP   = HEADER_H + 1;

bool testMotorAktifBaris(int row) {
  if (row == 0) {
    return pompaAktif(0) || pompaAktif(1) || pompaAktif(2) ||
           dcAktif(DC_SABUN) || dcAktif(DC_LIMBAH) || dcAktif(DC_AGI);
  }
  if (row >= 1 && row <= 3) return pompaAktif(row - 1);
  if (row == 4 || row == 5) return dcAktif(row - 3);   // 4->SABUN, 5->LIMBAH
  if (row == 6)             return dcAktif(DC_AGI);
  return false;
}

void testMotorPastikanTerlihat() {
  int visibleH  = AREA_BAWAH - TEST_MOTOR_TOP;
  int maxScroll = max(0, TEST_MOTOR_JUMLAH * TEST_MOTOR_ROW_H - visibleH);
  int y = testMotorCursor * TEST_MOTOR_ROW_H;
  if (y - testMotorScroll < 0) testMotorScroll = y;
  if (y - testMotorScroll + TEST_MOTOR_ROW_H > visibleH)
    testMotorScroll = y + TEST_MOTOR_ROW_H - visibleH;
  testMotorScroll = constrain(testMotorScroll, 0, maxScroll);
}

void gambarTestMotor() {
  gambarHeader("Test Motor");

  int visibleH  = AREA_BAWAH - TEST_MOTOR_TOP;
  int maxScroll = max(0, TEST_MOTOR_JUMLAH * TEST_MOTOR_ROW_H - visibleH);
  testMotorScroll = constrain(testMotorScroll, 0, maxScroll);

  oled.setTextSize(1);
  char buf[12];

  for (int j = 0; j < TEST_MOTOR_JUMLAH; j++) {
    int y = TEST_MOTOR_TOP + j * TEST_MOTOR_ROW_H - testMotorScroll;
    if (y < TEST_MOTOR_TOP || y + 7 > AREA_BAWAH) continue;

    bool sel = (j == testMotorCursor);
    if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, TEST_MOTOR_ROW_H, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);

    oled.setCursor(2, y);
    oled.print(testMotorLabel[j]);

    buf[0] = 0;
    if (j >= 1 && j <= 3) {
      int idx = j - 1;
      if (pompaAktif(idx)) {
        // Saat berjalan tampilkan target/aktual supaya terlihat RPM berhenti
        // tepat di target (mis. "400/400"), sama seperti sketch test-motor.
        sprintf(buf, "%d/%d", rpmPompaValid(kalibStepperRpm[idx]),
                (int)(pompaRpmSekarang(idx) + 0.5f));
      } else {
        sprintf(buf, "%drpm", rpmPompaValid(kalibStepperRpm[idx]));
      }
    } else if (j == 4 || j == 5) {
      sprintf(buf, "%d%%", kalibDutyDC[j - 4]);
    } else if (j == 6) {
      sprintf(buf, "SPD%d", testAgitSpeed);
    }
    if (buf[0]) teksKanan(buf, 86, y);
    teksKanan(testMotorAktifBaris(j) ? "STOP" : "START", 121, y);
  }
  oled.setTextColor(WHITE);

  // Indikator scroll
  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 3;
    if (testMotorScroll > 0)
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    if (testMotorScroll < maxScroll)
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
  }
}

// ================= HALAMAN: KALIBRASI DC (Pompa Sabun/Limbah) =================
// Uji berbasis target ml: pompa diramp ke duty terpilih, dijalankan selama
// durasi hasil hitung (ml / laju sekarang), lalu diramp turun. User mengukur
// hasil nyata, mengisi baris Hasil, dan ENTER menghitung offset + kalibrasi.
// Baris: START/STOP, Duty, Target, Hasil, Offset, ml/detik, Uji hasil.
// Perlu scroll.
const int KALIB_DC_CONTENT_H  = 74;
const int KALIB_DC_ROW_TOP[7] = { 0, 14, 24, 34, 44, 54, 64 };
const int KALIB_DC_ROW_H[7]   = { 12, 10, 10, 10, 10, 10, 10 };

void gambarKalibDC() {
  const char* nama[] = { "Sabun", "Limbah" };
  int idx = kalibMotorIdx - 3;                 // 0=sabun, 1=limbah
  char judul[16];
  sprintf(judul, "Kal %s", nama[idx]);
  gambarHeader(judul);

  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  int maxScroll = max(0, KALIB_DC_CONTENT_H - (AREA_BAWAH - HEADER_H - 4));
  if (kalibDCScroll < 0) kalibDCScroll = 0;
  if (kalibDCScroll > maxScroll) kalibDCScroll = maxScroll;

  int baseY = HEADER_H + 4 - kalibDCScroll;
  char buf[20];

  // --- Baris 0: START / STOP (dengan sisa detik saat mengukur) ---
  {
    int y = baseY + KALIB_DC_ROW_TOP[0];
    bool sel = (kalibDCCursor == 0);
    if (y + 12 > HEADER_H && y < AREA_BAWAH) {
      if (sel) {
        oled.fillRoundRect(20, y - 1, SCREEN_WIDTH - 40, 12, 4, WHITE);
        oled.setTextColor(BLACK);
      } else {
        oled.drawRoundRect(20, y - 1, SCREEN_WIDTH - 40, 12, 4, WHITE);
        oled.setTextColor(WHITE);
      }
      if (kalibDcJalan()) {
        int sisa = kalibDcSisaDetik();
        if (sisa > 0) sprintf(buf, "STOP %ds", sisa);
        else          strcpy(buf, "STOP");
      } else {
        strcpy(buf, "START");
      }
      teksTengah(buf, y + 2, 1);
    }
  }

  // --- Baris 1: Duty cycle ---
  {
    int y = baseY + KALIB_DC_ROW_TOP[1];
    bool sel = (kalibDCCursor == 1);
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      oled.print(F("Duty"));
      sprintf(buf, "%d%%", kalibDutyDC[idx]);
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  // --- Baris 2: target ml yang akan didosis ---
  {
    int y = baseY + KALIB_DC_ROW_TOP[2];
    bool sel = (kalibDCCursor == 2);
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      sprintf(buf, "Target x%d", kalibDcTgtStep);
      oled.print(buf);
      sprintf(buf, "%d ml", kalibDcTargetMl[idx]);
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  // --- Baris 3: hasil ukur nyata (ENTER = hitung offset) ---
  {
    int y = baseY + KALIB_DC_ROW_TOP[3];
    bool sel = (kalibDCCursor == 3);
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      sprintf(buf, "Hasil x%d", kalibDcStep);
      oled.print(buf);
      if (kalibDcUkurMl[idx] > 0) sprintf(buf, "%d ml", kalibDcUkurMl[idx]);
      else                        strcpy(buf, "--");
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  // --- Baris 4: offset terakhir (% terhadap target) ---
  {
    int y = baseY + KALIB_DC_ROW_TOP[4];
    bool sel = (kalibDCCursor == 4);
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      oled.print(F("Offset"));
      if (kalibDcOffsetAda[idx])
        sprintf(buf, "%+.1f%%", kalibDcOffsetX10[idx] / 10.0f);
      else
        strcpy(buf, "--");
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  // --- Baris 5: ml/detik (hasil kalibrasi) ---
  {
    int y = baseY + KALIB_DC_ROW_TOP[5];
    bool sel = (kalibDCCursor == 5);
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      oled.print(F("ml/detik"));
      if (kalibDcMlPerSec[idx] > 0.0f) sprintf(buf, "%.2f", kalibDcMlPerSec[idx]);
      else strcpy(buf, "--");
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  // --- Baris 6: Uji hasil (verifikasi kalibrasi tersimpan) ---
  {
    int y = baseY + KALIB_DC_ROW_TOP[6];
    bool sel = (kalibDCCursor == 6);
    if (y + 10 > HEADER_H && y < AREA_BAWAH) {
      if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
      oled.setTextColor(sel ? BLACK : WHITE);
      oled.setCursor(2, y);
      oled.print(F("Uji hasil"));
      if (kalibDcUjiMode[idx]) {
        strcpy(buf, kalibDcJalan() ? "RUN" : "ISI HASIL");
      } else if (kalibDcUjiAda[idx]) {
        strcpy(buf, kalibDcUjiLulus[idx] ? "LULUS" : "GAGAL");
      } else {
        strcpy(buf, "ENTER");
      }
      teksKanan(buf, SCREEN_WIDTH - 2, y);
    }
  }

  oled.setTextColor(WHITE);

  // --- Indikator scroll ---
  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 4;
    if (kalibDCScroll > 0)
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    if (kalibDCScroll < maxScroll)
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
  }
}

// ================= HALAMAN: KALIBRASI / SETTING AGITASI =================
// Tidak perlu kalibrasi ml. Cukup 3 level kecepatan (SPD1..SPD3) dalam %.
// KIRI/KANAN ubah duty baris terpilih; ENTER = jalankan/hentikan agitasi
// pada SPD tersebut (selalu ramp dari 0%).
void gambarKalibAgitasi() {
  gambarHeader("Motor Agitasi");

  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  const char* spd[] = { "SPD1", "SPD2", "SPD3" };
  char buf[12];

  int y = HEADER_H + 5;
  for (int i = 0; i < 3; i++, y += 11) {
    bool sel = (kalibDCCursor == i);
    if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);

    oled.setCursor(2, y);
    oled.print(spd[i]);

    sprintf(buf, "%d%%", constrain(agitasiSpdDuty[i], 0, 100));
    teksKanan(buf, 95, y);

    bool jalan = agitasiAktif() && (agitasiSpdUji == i + 1);
    teksKanan(jalan ? "ON" : "OFF", SCREEN_WIDTH - 2, y);
  }

  oled.setTextColor(WHITE);
}

// ================= HALAMAN: KALIBRASI SENSOR SUHU =================
// 3 blok. Tiap blok: nama di atas tengah; baris bawah berisi "addr: x" (kiri)
// dan "xx.x°C" (kanan). ENTER pindah field aktif, KIRI/KANAN mengubahnya.
void gambarKalibSuhu() {
  gambarHeader("Sensor Suhu");

  int maxScroll = max(0, SUHU_CONTENT_H - (AREA_BAWAH - HEADER_H - 3));
  if (kalibSuhuScroll > maxScroll) kalibSuhuScroll = maxScroll;
  if (kalibSuhuScroll < 0) kalibSuhuScroll = 0;

  int baseY = HEADER_H + 3 - kalibSuhuScroll;

  oled.setTextSize(1);
  for (int i = 0; i < JUMLAH_SUHU; i++) {
    int y = baseY + SUHU_ROW_TOP[i];
    if (y + SUHU_ROW_H < HEADER_H || y > AREA_BAWAH) continue;

    bool blokSel = (i == kalibSuhuIdx);
    // Kotak selector: inset 2px dari kiri/kanan, 1px dari bawah blok.
    if (blokSel) oled.drawRoundRect(2, y - 1, SCREEN_WIDTH - 4, SUHU_ROW_H - 2, 3, WHITE);

    // Nama sensor di atas tengah (2px dari atas kotak)
    oled.setTextColor(WHITE);
    teksTengah(namaSensorSuhu[i], y + 1, 1);

    int rowY = y + 10;              // baris addr + suhu, 2px dari nama
    bool fieldAddr  = blokSel && kalibSuhuField == 0;
    bool fieldSuhu  = blokSel && kalibSuhuField == 1;

    char buf[16];

    // --- addr: x (kiri) ---
    sprintf(buf, "addr:%d", kalibSuhuAddr[i]);
    if (fieldAddr) oled.fillRect(4, rowY - 1, 38, 10, WHITE);
    oled.setTextColor(fieldAddr ? BLACK : WHITE);
    oled.setCursor(5, rowY);
    oled.print(buf);

    // --- xx.x °C (kanan) ---
    int vx = SCREEN_WIDTH - 7;
    uint16_t warnaTeks = fieldSuhu ? BLACK : WHITE;
    if (fieldSuhu) oled.fillRect(vx - 30, rowY - 1, 31, 10, WHITE);
    oled.setTextColor(warnaTeks);
    float t = sensorSuhu(i);
    if (sensorSuhuValid(i)) {
      sprintf(buf, "%.1f", t);
      int16_t x1, y1; uint16_t w, h;
      oled.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
      oled.setCursor(vx - (int)w - 6, rowY);
      oled.print(buf);
      // Simbol derajat = lingkaran kecil (font tidak punya glyph "°")
      oled.drawCircle(vx - 5, rowY + 3, 1, warnaTeks);
      oled.setCursor(vx - 1, rowY);
      oled.print(F("C"));
    } else {
      oled.setCursor(vx - 30, rowY);
      oled.print(F("--.-C"));
    }
  }
  oled.setTextColor(WHITE);

  // Indikator scroll
  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 4;
    if (kalibSuhuScroll > 0)
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    if (kalibSuhuScroll < maxScroll)
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
  }
}

// ================= HALAMAN: KALIBRASI SENSOR UDARA (MQ-135) =================
// Dua baris acuan (Normal / Kotor) + bacaan real-time di bawah.
void gambarKalibUdara() {
  gambarHeader("Sensor Udara");

  oled.setTextSize(1);

  static const char* label[2] = { "Udara Normal", "Udara Kotor" };
  int nilai[2] = { mq135Normal, mq135Kotor };
  int y = HEADER_H + 4;
  for (int i = 0; i < 2; i++, y += 11) {
    bool sel = (kalibUdaraSel == i);
    if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(4, y);
    oled.print(label[i]);
    char buf[12];
    sprintf(buf, "%d", nilai[i]);
    teksKanan(buf, SCREEN_WIDTH - 4, y);
  }

  // Bacaan real-time di bawah tengah, dengan gap dari baris di atasnya
  oled.setTextColor(WHITE);
  char buf[24];
  int ppm = (int)mq135PPM();
  sprintf(buf, "Bacaan: %d ppm", ppm);
  teksTengah(buf, y + 4, 1);
}

void gambarKalibMain() {
  gambarHeader("Kalibrasi");
  const char* item[JUMLAH_MENU_KALIB];
  for (int i = 0; i < JUMLAH_MENU_KALIB; i++) item[i] = menuKalib[i];
  gambarList(item, JUMLAH_MENU_KALIB, L_AKSI, HEADER_H, 16);
}

// ================= HALAMAN: KALIBRASI SENSOR LEVEL =================
void gambarKalibLevel() {
  gambarHeader("Sensor Level");

  const int ROW_H = 10;
  const int VISIBLE = (AREA_BAWAH - HEADER_H) / ROW_H;
  int maxScroll = max(0, JUMLAH_SENSOR - VISIBLE);
  if (kalibLevelScroll > maxScroll) kalibLevelScroll = maxScroll;

  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    int y = HEADER_H + 2 + (i - kalibLevelScroll) * ROW_H;
    if (y < HEADER_H - 1 || y > AREA_BAWAH) continue;
    bool sel = (i == kalibSensorIdx);
    if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 9, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(2, y);
    oled.print(namaSensor[i]);
  }
  oled.setTextColor(WHITE);

  // Indikator scroll
  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 4;
    if (kalibLevelScroll > 0) {
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    }
    if (kalibLevelScroll < maxScroll) {
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
    }
  }
}

// ================= HALAMAN: SET 0% / 100% =================
void gambarKalibLevelSet() {
  gambarHeader(namaSensor[kalibSensorIdx]);

  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  int y = HEADER_H + 4;
  // 0%
  bool sel0 = (kalibSetStep == 0);
  if (sel0) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
  oled.setTextColor(sel0 ? BLACK : WHITE);
  oled.setCursor(2, y);
  oled.print(F("0%"));
  char buf[16];
  sprintf(buf, "= %d", sensorKalibKosong[kalibSensorIdx]);
  teksKanan(buf, SCREEN_WIDTH - 2, y);
  y += 12;

  // 100%
  bool sel100 = (kalibSetStep == 1);
  if (sel100) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
  oled.setTextColor(sel100 ? BLACK : WHITE);
  oled.setCursor(2, y);
  oled.print(F("100%"));
  sprintf(buf, "= %d", sensorKalibPenuh[kalibSensorIdx]);
  teksKanan(buf, SCREEN_WIDTH - 2, y);
  y += 14;

  // Raw value live
  oled.setTextColor(WHITE);
  sprintf(buf, "Raw: %d", sensorRaw[kalibSensorIdx]);
  teksTengah(buf, y, 1);
}

// ================= TOAST (pesan singkat) =================
const char* toastTeks = "";
bool toastAktif = false;
unsigned long toastSampai = 0;

void tampilToast(const char* teks) {
  toastTeks = teks;
  toastAktif = true;
  toastSampai = millis() + 1200;
  perluGambar = true;
}

void gambarToast() {
  oled.fillRect(14, 26, 100, 14, BLACK);
  oled.drawRect(14, 26, 100, 14, WHITE);
  oled.setTextColor(WHITE);
  teksTengah(toastTeks, 30, 1);
}

// ================= RENDER =================
void gambarLayar() {
  oled.clearDisplay();
  switch (halaman) {
    case HAL_MENU_UTAMA:    gambarMenuUtama();    break;
    case HAL_PRESET_LIST:   gambarPresetList();   break;
    case HAL_PRESET_DETAIL: gambarPresetDetail(); break;
    case HAL_PROSES:        gambarProses();       break;
    case HAL_PLACEHOLDER:   gambarPlaceholder();  break;
    case HAL_CFG_LIST:      gambarCfgList();      break;
    case HAL_CFG_AKSI:      gambarCfgAksi();      break;
    case HAL_CFG_EDIT:      gambarCfgEdit();      break;
    case HAL_LEVEL:         gambarLevel();        break;
    case HAL_KALIB_MAIN:    gambarKalibMain();    break;
    case HAL_KALIB_LEVEL:   gambarKalibLevel();   break;
    case HAL_KALIB_LEVEL_SET: gambarKalibLevelSet(); break;
    case HAL_KALIB_MOTOR_LIST: gambarKalibMotorList(); break;
    case HAL_KALIB_STEPPER:    gambarKalibStepper();    break;
    case HAL_KALIB_DC:         gambarKalibDC();         break;
    case HAL_KALIB_AGITASI:    gambarKalibAgitasi();    break;
    case HAL_KALIB_SUHU:       gambarKalibSuhu();       break;
    case HAL_KALIB_UDARA:      gambarKalibUdara();      break;
    case HAL_TEST_MOTOR:       gambarTestMotor();       break;
  }
  // Overlay
  if (halaman == HAL_PROSES && dialogBatal)
    gambarDialog("Batalkan proses?", "ENTER : Ya", "BACK  : Tidak");
  if (dialogCfg == DLG_SIMPAN)
    gambarDialog("Simpan perubahan?", "ENTER : Simpan", "BACK  : Buang");
  if (dialogCfg == DLG_HAPUS)
    gambarDialog("Hapus preset?", "ENTER : Ya", "BACK  : Tidak");
  if (toastAktif) gambarToast();
  geserGambar(UI_Y_OFFSET);   // hindarkan baris atas yang rusak
  oled.display();
}
