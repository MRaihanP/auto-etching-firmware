#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "data.h"
#include "process.h"
#include "presetops.h"
#include "sensor.h"
#include "aktuator.h"

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===== 20 baris teratas OLED rusak → seluruh UI digeser ke bawah =====
// Semua koordinat Y bersifat logis: y=0 = baris pertama yang terlihat.
// Bentuk memakai TY(), teks memakai teksTengah/teksKanan/teksKiri.
// Sisa tinggi untuk teks: y=0..37 (ink 7px terakhir y=37 → 57..63)
#define TOP_OFF    OLED_TOP_OFF
#define TY(y)      ((y) + TOP_OFF)
#define AREA_BAWAH (SCREEN_HEIGHT - TOP_OFF)   // 44
#define BARIS_H    8                           // jarak baris standar (font size 1)

#define L_UTAMA   0
#define L_PRESET  1
#define L_CFG     2
#define L_AKSI    3
#define L_KALIB   4
#define JUMLAH_LIST 5

int           lsSel[JUMLAH_LIST]        = {0};
float         lsPos[JUMLAH_LIST]        = {0};
float         lsAwal[JUMLAH_LIST]       = {0};
float         lsTujuan[JUMLAH_LIST]     = {0};
unsigned long lsWaktuMulai[JUMLAH_LIST] = {0};
bool          lsAnimasi[JUMLAH_LIST]    = {false};
const unsigned int DURASI_ANIMASI_MS = 200;

void listGeser(int L, int delta, int jumlahItem) {
  int baru = lsSel[L] + delta;
  if (baru < 0 || baru > jumlahItem - 1) return;
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
    float te = 1.0 - (1.0 - t) * (1.0 - t);
    lsPos[L] = lsAwal[L] + (lsTujuan[L] - lsAwal[L]) * te;
  }
}

const int BOX_MARGIN_X = 4;
const int BOX_RADIUS   = 5;
const int HEADER_H     = 12;

// ================= SCROLL HALAMAN =================
// Dipakai halaman yang isinya lebih tinggi dari area tampil (ATAS/BAWAH)
int uiScrollPx = 0, uiScrollMaks = 0, uiScrollArea = 0;

void uiScrollSet(int topBaris, int tinggiIsi) {
  uiScrollArea = AREA_BAWAH - topBaris;
  uiScrollMaks = max(0, tinggiIsi - uiScrollArea);
  uiScrollPx = constrain((uiScrollPx / BARIS_H) * BARIS_H, 0, uiScrollMaks);
}
void uiScrollAtur(int delta) {   // +1/-1 baris (dari .ino)
  uiScrollPx = constrain(((uiScrollPx + delta * BARIS_H) / BARIS_H) * BARIS_H,
                         0, uiScrollMaks);
  perluGambar = true;
}
void uiScrollKe(int yRel, int tinggi) {   // auto-scroll: pastikan item terlihat
  if (yRel < uiScrollPx) uiScrollPx = yRel;
  if (yRel + tinggi > uiScrollPx + uiScrollArea) uiScrollPx = yRel + tinggi - uiScrollArea;
  uiScrollPx = constrain((uiScrollPx / BARIS_H) * BARIS_H, 0, uiScrollMaks);
}
void gambarScrollbar() {
  if (uiScrollMaks <= 0) return;
  int x = SCREEN_WIDTH - 2;
  int top = TY(HEADER_H), tinggi = TY(AREA_BAWAH) - top;
  int thumb = max(6, tinggi * uiScrollArea / (uiScrollArea + uiScrollMaks));
  int pos = top + (tinggi - thumb) * uiScrollPx / max(1, uiScrollMaks);
  oled.fillRect(x, pos, 2, thumb, WHITE);
}

// ===== primitif teks (koordinat logis, + TOP_OFF otomatis) =====
void teksTengah(const char* s, int y, int ukuran) {
  int16_t x1, y1; uint16_t w, h;
  oled.setTextSize(ukuran);
  oled.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((SCREEN_WIDTH - w) / 2, TY(y));
  oled.print(s);
}
void teksKanan(const char* s, int xKanan, int y) {
  int16_t x1, y1; uint16_t w, h;
  oled.setTextSize(1);
  oled.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor(xKanan - w, TY(y));
  oled.print(s);
}
void teksKiri(const char* s, int x, int y) {
  oled.setTextSize(1);
  oled.setCursor(x, TY(y));
  oled.print(s);
}
void gambarHeader(const char* judul) {
  oled.fillRect(0, TOP_OFF, SCREEN_WIDTH, HEADER_H, WHITE);
  oled.setTextColor(BLACK);
  teksTengah(judul, 2, 1);
  oled.setTextColor(WHITE);
}
void gambarList(const char* const* items, int jumlah, int L, int areaAtas, int rowHeight) {
  int areaTinggi = AREA_BAWAH - areaAtas;
  int centerY = TY(areaAtas) + areaTinggi / 2;
  oled.setTextColor(WHITE);
  oled.setTextSize(1);
  for (int j = 0; j < jumlah; j++) {
    float y = centerY + (j - lsPos[L]) * rowHeight;
    if (y < TY(areaAtas) - rowHeight || y > SCREEN_HEIGHT + rowHeight) continue;
    int16_t x1, y1; uint16_t w, h;
    oled.getTextBounds(items[j], 0, 0, &x1, &y1, &w, &h);
    int ty = (int)round(y - h / 2.0);
    if (ty + (int)h <= TY(areaAtas)) continue;
    oled.setCursor((SCREEN_WIDTH - w) / 2, ty);
    oled.print(items[j]);
  }
  oled.drawRoundRect(BOX_MARGIN_X, centerY - rowHeight / 2,
                     SCREEN_WIDTH - 2 * BOX_MARGIN_X, rowHeight,
                     BOX_RADIUS, WHITE);
}

// ================= HALAMAN: SPLASH =================
void gambarSplash() {
  oled.setTextSize(2);
  teksTengah("PCB ETCH", 10, 2);
  oled.setTextSize(1);
  teksTengah(FW_VERSI, 28, 1);
  teksTengah("Booting...", 37, 1);
}

// ================= HALAMAN: MAIN MENU =================
void gambarMenuUtama() {
  gambarList(menuUtama, JUMLAH_MENU, L_UTAMA, 0, 20);
}

// ================= HALAMAN: PRESET LIST =================
void gambarPresetKosong() {
  gambarHeader("Pilih Preset");
  int cx = SCREEN_WIDTH / 2;
  int topY = 4, botY = 24, halfW = 16;
  oled.drawTriangle(cx, TY(topY), cx - halfW, TY(botY), cx + halfW, TY(botY), WHITE);
  oled.drawTriangle(cx, TY(topY) + 2, cx - halfW + 2, TY(botY) - 1, cx + halfW - 2, TY(botY) - 1, WHITE);
  oled.drawFastVLine(cx, TY(topY) + 8, 8, WHITE);
  oled.drawPixel(cx, TY(botY) - 4, WHITE);
  teksTengah("Belum ada preset", 28, 1);
  teksTengah("yang dibuat", 36, 1);
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
  if (scroll > 0)
    oled.fillTriangle(x - 3, TY(HEADER_H + 2), x + 3, TY(HEADER_H + 2), x, TY(HEADER_H - 1), WHITE);
  if (scroll < maxScroll)
    oled.fillTriangle(x - 3, TY(AREA_BAWAH - 4), x + 3, TY(AREA_BAWAH - 4), x, TY(AREA_BAWAH - 1), WHITE);
}

void gambarPresetDetail() {
  Preset &p = daftarPreset[presetTerpilih];
  gambarHeader(p.nama);
  char buf[24];
  oled.setTextColor(WHITE);
  int maxScroll = max(0, DETAIL_CONTENT_H - (AREA_BAWAH - HEADER_H));
  if (presetDetailScroll > maxScroll) presetDetailScroll = maxScroll;
  int y = HEADER_H + 3 - presetDetailScroll;
  const int dy = 8;
  const int xKanan = SCREEN_WIDTH - 2;

  if (terlihat(y, dy)) {
    teksKiri("PCB max", 2, y);
    sprintf(buf, "%dx%d mm", p.pcbPanjang, p.pcbLebar);
    teksKanan(buf, xKanan, y);
  }
  y += dy;
  if (terlihat(y, 1)) oled.drawFastHLine(0, TY(y), SCREEN_WIDTH, WHITE);
  y += 3;
  if (terlihat(y, dy)) { teksKiri("HCl", 2, y); sprintf(buf, "%d%%  %d ml", p.persenHCl, p.mlHCl); teksKanan(buf, xKanan, y); }
  y += dy;
  if (terlihat(y, dy)) { teksKiri("H2O2", 2, y); sprintf(buf, "%d%%  %d ml", p.persenH2O2, p.mlH2O2); teksKanan(buf, xKanan, y); }
  y += dy;
  if (terlihat(y, dy)) { teksKiri("H2O", 2, y); sprintf(buf, "%d%%  %d ml", p.persenH2O, p.mlH2O); teksKanan(buf, xKanan, y); }
  y += dy;
  int barY = y + 1, barH = 4, barW = SCREEN_WIDTH - 8;
  if (terlihat(barY, barH)) {
    int w1 = barW * p.persenHCl  / 100;
    int w2 = barW * p.persenH2O2 / 100;
    oled.drawRect(4, TY(barY), barW, barH, WHITE);
    oled.fillRect(4, TY(barY), w1, barH, WHITE);
    for (int x = 4 + w1; x < 4 + w1 + w2; x += 2)
      oled.drawFastVLine(x, TY(barY) + 1, barH - 2, WHITE);
  }
  y += barH + 4;
  if (terlihat(y, 1)) oled.drawFastHLine(0, TY(y), SCREEN_WIDTH, WHITE);
  y += 3;
  if (terlihat(y, dy)) { teksKiri("Agitasi", 2, y); sprintf(buf, "%ds  Spd %d", p.agitasiDetik, p.agitasiSpeed); teksKanan(buf, xKanan, y); }
  y += dy;
  if (terlihat(y, dy)) { teksKiri("Total", 2, y); formatMMSS(p.totalDetik, buf); teksKanan(buf, xKanan, y); }
  gambarIndikatorScroll(presetDetailScroll, maxScroll);
}

// ================= HALAMAN: PROSES =================
void gambarDialog(const char* l1, const char* l2, const char* l3) {
  oled.fillRect(10, TY(4), 108, 38, BLACK);
  oled.drawRect(10, TY(4), 108, 38, WHITE);
  oled.setTextColor(WHITE);
  oled.setTextSize(1);
  teksTengah(l1, 8, 1);
  teksTengah(l2, 19, 1);
  teksTengah(l3, 28, 1);
}

void gambarProses() {
  Preset &p = daftarPreset[presetTerpilih];
  unsigned long totalMs = (unsigned long)p.totalDetik * 1000UL;
  int lewatDetik = prosesAkumMs / 1000;
  float frac = totalMs > 0 ? (float)prosesAkumMs / (float)totalMs : 0;

  // Header: nama preset kiri, sisa waktu kanan
  char buf[24];
  oled.fillRect(0, TOP_OFF, SCREEN_WIDTH, HEADER_H, WHITE);
  oled.setTextColor(BLACK);
  oled.setTextSize(1);
  oled.setCursor(2, TY(2)); oled.print(p.nama);
  int sisaDetik = p.totalDetik - lewatDetik;
  if (sisaDetik < 0) sisaDetik = 0;
  char sisa[12];
  formatMMSS(sisaDetik, sisa);
  char sisaBuf[16];
  snprintf(sisaBuf, sizeof(sisaBuf), "sisa %s", sisa);
  teksKanan(sisaBuf, SCREEN_WIDTH - 2, 2);
  oled.setTextColor(WHITE);

  // Waktu elapsed (besar)
  formatMMSS(lewatDetik, buf);
  teksTengah(buf, 13, 2);

  // Progress bar
  int barX = 6, barY = 29, barW = SCREEN_WIDTH - 12, barH = 8;
  oled.drawRoundRect(barX, TY(barY), barW, barH, 2, WHITE);
  int isi = (int)((barW - 4) * frac);
  if (isi > 0) oled.fillRect(barX + 2, TY(barY) + 2, isi, barH - 4, WHITE);

  // Status baris: fase kiri, persen/ml kanan
  const char* st = prosesJeda ? "JEDA" : namaFase[prosesFase];
  bool dosingStepper = (prosesFase == FASE_DOSING_HCL ||
                        prosesFase == FASE_DOSING_H2O2 ||
                        prosesFase == FASE_DOSING_H2O);
  if (dosingStepper) {
    int target = (prosesFase == FASE_DOSING_HCL)  ? p.mlHCl  :
                 (prosesFase == FASE_DOSING_H2O2) ? p.mlH2O2 : p.mlH2O;
    snprintf(buf, sizeof(buf), "%d/%dml", prosesMlTerpompa, target);
  } else {
    snprintf(buf, sizeof(buf), "%d%%", (int)(frac * 100));
  }
  teksKiri(st, 2, 37);
  teksKanan(buf, SCREEN_WIDTH - 2, 37);
}

// ================= HALAMAN: CADANGAN SENYAWA =================
void gambarCadangan() {
  gambarHeader("Cadangan Senyawa");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  uiScrollSet(HEADER_H, JUMLAH_SENSOR * BARIS_H);
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    int y = HEADER_H + i * BARIS_H - uiScrollPx;
    if (y < HEADER_H || y > AREA_BAWAH - BARIS_H) continue;
    bool sel = (i == levelSel);
    if (sel) oled.fillRect(0, TY(y) - 1, SCREEN_WIDTH - 4, 9, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    teksKiri(namaSensor[i], 1, y);
    char buf[20];
    if (kalibrasiSensorValid[i]) {
      if (i == 4) {
        int ruangSisa = kalibrasiSensor[i].kapasitasMl - sensorMl[i];
        snprintf(buf, sizeof(buf), "sisa %3dml", ruangSisa);
      } else {
        snprintf(buf, sizeof(buf), "%3dml %2d%%", sensorMl[i], sensorPersen[i]);
      }
    } else {
      snprintf(buf, sizeof(buf), "uncal");
    }
    teksKanan(buf, SCREEN_WIDTH - 5, y);
  }
  oled.setTextColor(WHITE);
  gambarScrollbar();
}

// ================= HALAMAN: MENU KALIBRASI =================
void gambarKalibrasi() {
  gambarHeader("Kalibrasi");
  gambarList(menuKalibrasi, JUMLAH_MENU_KALIB, L_KALIB, HEADER_H, 16);
}

// ================= HALAMAN: KALIBRASI SENSOR LEVEL =================
// Konten: switch (10px) + 5 titik (5×8px); ATAS/BAWAH = pilih titik
// dengan auto-scroll agar titik terpilih selalu terlihat
void gambarKalibSensor() {
  char buf[32];
  // Header ringkas 10px: judul kiri, raw kanan
  oled.fillRect(0, TOP_OFF, SCREEN_WIDTH, 10, WHITE);
  oled.setTextColor(BLACK);
  oled.setTextSize(1);
  snprintf(buf, sizeof(buf), "Kalibrasi%s", kalibSensorBerubah ? " *" : "");
  oled.setCursor(2, TOP_OFF); oled.print(buf);
  snprintf(buf, sizeof(buf), "Raw %d", sensorAvg[kalibSensorSel]);
  teksKanan(buf, SCREEN_WIDTH - 2, 0);
  oled.setTextColor(WHITE);

  const int TOP_ISI = 10, ISI_H = 10 + KALIBRASI_TITIK * BARIS_H;
  uiScrollSet(TOP_ISI, ISI_H);
  uiScrollKe(10 + kalibSensorTitik * BARIS_H, BARIS_H);

  int y = TOP_ISI - uiScrollPx;
  // Selektor senyawa (KIRI/KANAN): kotak putih penuh + kapasitas
  snprintf(buf, sizeof(buf), "< %s >", namaSensor[kalibSensorSel]);
  int16_t x1, y1; uint16_t w, h;
  oled.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  oled.fillRoundRect(2, TY(y), w + 8, 10, 3, WHITE);
  oled.setTextColor(BLACK);
  oled.setCursor(6, TY(y) + 1); oled.print(buf);
  oled.setTextColor(WHITE);
  snprintf(buf, sizeof(buf), "%dml", kalibrasiSensor[kalibSensorSel].kapasitasMl);
  teksKanan(buf, SCREEN_WIDTH - 5, y + 1);
  y += 10;

  // 5 titik kalibrasi (ATAS/BAWAH: scroll+pilih, ENTER: set)
  const int persen[KALIBRASI_TITIK] = {0, 25, 50, 75, 100};
  for (int t = 0; t < KALIBRASI_TITIK; t++) {
    int ry = y + t * BARIS_H;
    if (ry < TOP_ISI || ry > AREA_BAWAH - BARIS_H) continue;
    bool sel = (t == kalibSensorTitik);
    if (sel) oled.fillRect(0, TY(ry) - 1, SCREEN_WIDTH - 4, 9, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    snprintf(buf, sizeof(buf), "%3d%%: raw=%4d",
             persen[t], kalibrasiSensor[kalibSensorSel].titik[t]);
    oled.setCursor(2, TY(ry)); oled.print(buf);
  }
  oled.setTextColor(WHITE);
  gambarScrollbar();
}

// ================= HALAMAN: KALIBRASI STEPPER =================
void gambarKalibStepper() {
  char buf[32];
  oled.fillRect(0, TOP_OFF, SCREEN_WIDTH, HEADER_H, WHITE);
  oled.setTextColor(BLACK);
  oled.setTextSize(1);
  oled.setCursor(2, TY(2)); oled.print("Kalib Stepper");
  snprintf(buf, sizeof(buf), "Step %d", kalibStepperStep);
  teksKanan(buf, SCREEN_WIDTH - 2, 2);
  oled.setTextColor(WHITE);

  int y = HEADER_H;
  const char* namaPompa[3] = {"HCl", "H2O2", "H2O"};
  for (int i = 0; i < 3; i++) {
    bool sel = (i == kalibStepperIdx);
    if (sel) oled.fillRect(0, TY(y) - 1, SCREEN_WIDTH - 4, 9, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(2, TY(y)); oled.print(namaPompa[i]);
    float mlPerStep = kalibrasi.stepperMlPerStepX1000[i] / 1000.0;
    snprintf(buf, sizeof(buf), "%.3f ml/step", mlPerStep);
    teksKanan(buf, SCREEN_WIDTH - 5, y);
    y += BARIS_H;
  }
  oled.setTextColor(WHITE);
  teksTengah("ENTER: jalankan test", y, 1);
}

// ================= HALAMAN: KALIBRASI MQ-135 =================
void gambarKalibMQ135() {
  gambarHeader("Kalib MQ-135");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  char buf[32];
  uiScrollSet(HEADER_H, 6 * BARIS_H);
  int y = HEADER_H - uiScrollPx;

  snprintf(buf, sizeof(buf), "Baseline: %d", kalibrasi.mq135Baseline);
  oled.setCursor(2, TY(y)); oled.print(buf); y += BARIS_H;
  snprintf(buf, sizeof(buf), "Margin:   %d", kalibrasi.mq135Margin);
  oled.setCursor(2, TY(y)); oled.print(buf); y += BARIS_H;
  snprintf(buf, sizeof(buf), "Bacaan:   %d", mq135Baca());
  oled.setCursor(2, TY(y)); oled.print(buf); y += BARIS_H;
  oled.setCursor(2, TY(y)); oled.print(mq135Normal() ? "Status: NORMAL" : "Status: TIDAK NORMAL");
  y += BARIS_H;
  teksTengah("ENTER: set baseline", y, 1); y += BARIS_H;
  teksTengah("KIRI/KANAN: margin", y, 1);
  gambarScrollbar();
}

// ================= HALAMAN: KALIBRASI DS18B20 =================
void gambarKalibDS18B20() {
  gambarHeader("Kalib DS18B20");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  char buf[32];
  int y = HEADER_H;

  float offset = kalibrasi.ds18b20OffsetX10 / 10.0;
  snprintf(buf, sizeof(buf), "Offset: %.1f C", offset);
  oled.setCursor(2, TY(y)); oled.print(buf); y += BARIS_H;
  float suhu = ds18b20Baca();
  snprintf(buf, sizeof(buf), "Suhu:   %.1f C", suhu);
  oled.setCursor(2, TY(y)); oled.print(buf); y += BARIS_H;

  teksTengah("KIRI/KANAN: offset", y, 1); y += BARIS_H;
  teksTengah("(vs termometer)", y, 1);
}

// ================= HALAMAN: KALIBRASI AGITASI =================
void gambarKalibAgitasi() {
  gambarHeader("Kalib Agitasi");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  char buf[32];
  int y = HEADER_H;

  snprintf(buf, sizeof(buf), "Duty min: %d", kalibrasi.agitasiDutyMin);
  oled.setCursor(2, TY(y)); oled.print(buf); y += BARIS_H;
  oled.setCursor(2, TY(y)); oled.print("(cari duty awal gerak)"); y += BARIS_H;

  teksTengah("KIRI/KANAN: ubah duty", y, 1); y += BARIS_H;
  teksTengah("ENTER: test motor", y, 1);
}

// ================= HALAMAN: TENTANG =================
void gambarTentang() {
  gambarHeader("Tentang");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  uiScrollSet(HEADER_H, 6 * BARIS_H);
  int y = HEADER_H - uiScrollPx;

  oled.setCursor(2, TY(y)); oled.print(F("Firmware: ")); oled.print(FW_VERSI); y += BARIS_H;
  oled.setCursor(2, TY(y)); oled.print(F("MCU: ESP32-S3")); y += BARIS_H;
  oled.setCursor(2, TY(y)); oled.print(F("Display: OLED 128x64")); y += BARIS_H;
  oled.setCursor(2, TY(y)); oled.print(F("I2C: 0x3C/0x20")); y += BARIS_H;
  oled.setCursor(2, TY(y)); oled.print(F("Sensor: GPIO 11/12/13/14/3")); y += BARIS_H;
  teksTengah("[BACK] kembali", y, 1);
  gambarScrollbar();
}

// ================= HALAMAN: ERROR =================
void gambarError() {
  gambarHeader("ERROR");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(2, TY(HEADER_H)); oled.print(prosesErrorKode);
  oled.setCursor(2, TY(HEADER_H + BARIS_H)); oled.print(prosesErrorPesan);
  teksTengah("Exhaust ON", HEADER_H + 2 * BARIS_H, 1);
  teksTengah("[ENTER] reset", HEADER_H + 3 * BARIS_H, 1);
}

// ================= HALAMAN: KONFIGURASI PRESET =================
const char* const itemAksi[3] = { "Edit", "Duplikat", "Hapus" };
const char* const TEKS_TAMBAH = "+ Tambah Preset";

int jumlahItemCfg() {
  return jumlahPreset + (jumlahPreset < MAKS_PRESET ? 1 : 0);
}

void gambarCfgList() {
  gambarHeader("Buat Preset");
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

const int EDIT_ROW_H   = BARIS_H;
const int EDIT_VISIBLE = 4;

void formatField(const Preset &p, int f, char* buf) {
  switch (f) {
    case 0: sprintf(buf,  "%d mm ", p.pcbPanjang); break;
    case 1: sprintf(buf,  "%d mm ", p.pcbLebar);   break;
    case 2: sprintf(buf,  "%s ", p.modeDosing == 0 ? "auto" : "manual"); break;
    case 3: sprintf(buf,  "%d%% ", p.persenHCl);   break;
    case 4: sprintf(buf,  "%d%% ", p.persenH2O2);  break;
    case 5: sprintf(buf,  "%d ml ", volumeTotal(p)); break;
    case 6: sprintf(buf,  "%s ", p.modeAgitasi == 0 ? "auto" : "manual"); break;
    case 7: sprintf(buf,  "%d s ", p.agitasiDetik); break;
    case 8: sprintf(buf,  "%d ", p.agitasiSpeed);   break;
    case 9: formatMMSS(p.totalDetik, buf);        break;
    case 10: snprintf(buf, 16, "%s", p.nama); break;
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
    int y = HEADER_H + r * EDIT_ROW_H;
    bool sel = (f == editField);
    if (sel) oled.fillRect(0, TY(y), SCREEN_WIDTH - 5, EDIT_ROW_H, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(2, TY(y) + 1);
    oled.print(namaField[f]);
    char buf[16];
    formatField(presetEdit, f, buf);
    teksKanan(buf, SCREEN_WIDTH - 8, y + 1);
  }
  oled.setTextColor(WHITE);
  int trackY = HEADER_H, trackH = AREA_BAWAH - trackY;
  int thumbH = trackH * EDIT_VISIBLE / JUMLAH_FIELD;
  int thumbY = TY(trackY) + (trackH - thumbH) * editScroll / max(1, JUMLAH_FIELD - EDIT_VISIBLE);
  oled.fillRect(SCREEN_WIDTH - 2, thumbY, 2, thumbH, WHITE);
}

// ================= TOAST =================
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
  oled.fillRect(14, TY(12), 100, 14, BLACK);
  oled.drawRect(14, TY(12), 100, 14, WHITE);
  oled.setTextColor(WHITE);
  teksTengah(toastTeks, 16, 1);
}

// ================= RENDER UTAMA =================
void gambarLayar() {
  // Reset scroll saat pindah halaman
  static Halaman lastHalaman = HAL_SPLASH;
  if (halaman != lastHalaman) {
    uiScrollPx = 0;
    lastHalaman = halaman;
  }

  oled.clearDisplay();
  switch (halaman) {
    case HAL_SPLASH:        gambarSplash();       break;
    case HAL_MENU_UTAMA:    gambarMenuUtama();    break;
    case HAL_PRESET_LIST:   gambarPresetList();   break;
    case HAL_PRESET_DETAIL: gambarPresetDetail(); break;
    case HAL_PROSES:        gambarProses();       break;
    case HAL_PLACEHOLDER:   teksTengah("Placeholder", 20, 1); break;
    case HAL_CFG_LIST:      gambarCfgList();      break;
    case HAL_CFG_AKSI:      gambarCfgAksi();      break;
    case HAL_CFG_EDIT:      gambarCfgEdit();      break;
    case HAL_CADANGAN:      gambarCadangan();     break;
    case HAL_KALIBRASI:     gambarKalibrasi();    break;
    case HAL_KALIB_SENSOR:  gambarKalibSensor();  break;
    case HAL_KALIB_STEPPER: gambarKalibStepper(); break;
    case HAL_KALIB_MQ135:   gambarKalibMQ135();   break;
    case HAL_KALIB_DS18B20: gambarKalibDS18B20(); break;
    case HAL_KALIB_AGITASI: gambarKalibAgitasi(); break;
    case HAL_TENTANG:       gambarTentang();      break;
    case HAL_ERROR:         gambarError();        break;
  }

  if (halaman == HAL_PROSES && dialogBatal)
    gambarDialog("Batalkan proses?", "ENTER : Ya", "BACK  : Tidak");
  if (dialogCfg == DLG_SIMPAN)
    gambarDialog("Simpan perubahan?", "ENTER : Simpan", "BACK  : Buang");
  if (dialogCfg == DLG_HAPUS)
    gambarDialog("Hapus preset?", "ENTER : Ya", "BACK  : Tidak");
  if (toastAktif) gambarToast();
  oled.display();
}
