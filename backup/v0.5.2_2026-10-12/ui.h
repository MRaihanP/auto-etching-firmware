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
void gambarList(const char* const* items, int jumlah, int L, int areaAtas, int rowHeight) {
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
    if (ty + (int)h <= areaAtas) continue;
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
  teksTengah("PCB ETCH", 16, 2);
  oled.setTextSize(1);
  teksTengah(FW_VERSI, 40, 1);
  teksTengah("Booting...", 54, 1);
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
  return (yPos + tinggi > HEADER_H) && (yPos < SCREEN_HEIGHT);
}
void gambarIndikatorScroll(int scroll, int maxScroll) {
  const int x = SCREEN_WIDTH - 6;
  if (scroll > 0)
    oled.fillTriangle(x - 3, HEADER_H + 4, x + 3, HEADER_H + 4, x, HEADER_H + 1, WHITE);
  if (scroll < maxScroll)
    oled.fillTriangle(x - 3, SCREEN_HEIGHT - 4, x + 3, SCREEN_HEIGHT - 4, x, SCREEN_HEIGHT - 1, WHITE);
}

void gambarPresetDetail() {
  Preset &p = daftarPreset[presetTerpilih];
  gambarHeader(p.nama);
  char buf[24];
  oled.setTextColor(WHITE);
  oled.setTextSize(1);
  int maxScroll = max(0, DETAIL_CONTENT_H - (SCREEN_HEIGHT - HEADER_H));
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
  if (terlihat(y, dy)) { oled.setCursor(2, y); oled.print(F("HCl")); sprintf(buf, "%d%%  %d ml", p.persenHCl, p.mlHCl); teksKanan(buf, xKanan, y); }
  y += dy;
  if (terlihat(y, dy)) { oled.setCursor(2, y); oled.print(F("H2O2")); sprintf(buf, "%d%%  %d ml", p.persenH2O2, p.mlH2O2); teksKanan(buf, xKanan, y); }
  y += dy;
  if (terlihat(y, dy)) { oled.setCursor(2, y); oled.print(F("H2O")); sprintf(buf, "%d%%  %d ml", p.persenH2O, p.mlH2O); teksKanan(buf, xKanan, y); }
  y += dy;
  int barY = y + 1, barH = 4, barW = SCREEN_WIDTH - 8;
  if (terlihat(barY, barH)) {
    int w1 = barW * p.persenHCl  / 100;
    int w2 = barW * p.persenH2O2 / 100;
    oled.drawRect(4, barY, barW, barH, WHITE);
    oled.fillRect(4, barY, w1, barH, WHITE);
    for (int x = 4 + w1; x < 4 + w1 + w2; x += 2)
      oled.drawFastVLine(x, barY + 1, barH - 2, WHITE);
  }
  y += barH + 4;
  if (terlihat(y, 1)) oled.drawFastHLine(0, y, SCREEN_WIDTH, WHITE);
  y += 3;
  if (terlihat(y, dy)) { oled.setCursor(2, y); oled.print(F("Agitasi")); sprintf(buf, "%ds  Spd %d", p.agitasiDetik, p.agitasiSpeed); teksKanan(buf, xKanan, y); }
  y += dy;
  if (terlihat(y, dy)) { oled.setCursor(2, y); oled.print(F("Total")); formatMMSS(p.totalDetik, buf); teksKanan(buf, xKanan, y); }
  gambarIndikatorScroll(presetDetailScroll, maxScroll);
}

// ================= HALAMAN: PROSES =================
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
  float frac = totalMs > 0 ? (float)prosesAkumMs / (float)totalMs : 0;
  gambarHeader(p.nama);
  char buf[24];
  oled.setTextColor(WHITE);
  formatMMSS(lewatDetik, buf);
  teksTengah(buf, 17, 2);
  oled.setTextSize(1);
  char buf2[24], sisa[12];
  int sisaDetik = p.totalDetik - lewatDetik;
  if (sisaDetik < 0) sisaDetik = 0;
  formatMMSS(sisaDetik, sisa);
  bool dosingStepper = (prosesFase == FASE_DOSING_HCL ||
                        prosesFase == FASE_DOSING_H2O2 ||
                        prosesFase == FASE_DOSING_H2O);
  if (dosingStepper) {
    int target = (prosesFase == FASE_DOSING_HCL)  ? p.mlHCl  :
                 (prosesFase == FASE_DOSING_H2O2) ? p.mlH2O2 : p.mlH2O;
    sprintf(buf2, "%s %d/%dml", sisa, prosesMlTerpompa, target);
  } else {
    sprintf(buf2, "sisa %s", sisa);
  }
  teksTengah(buf2, 35, 1);
  int barX = 6, barY = 45, barW = SCREEN_WIDTH - 12, barH = 8;
  oled.drawRoundRect(barX, barY, barW, barH, 2, WHITE);
  int isi = (int)((barW - 4) * frac);
  if (isi > 0) oled.fillRect(barX + 2, barY + 2, isi, barH - 4, WHITE);
  const char* st = prosesJeda ? "JEDA" : namaFase[prosesFase];
  oled.setCursor(2, 56);
  oled.print(st);
  sprintf(buf, "%d%%", (int)(frac * 100));
  teksKanan(buf, SCREEN_WIDTH - 2, 56);
}

// ================= HALAMAN: CADANGAN SENYAWA =================
void gambarCadangan() {
  gambarHeader("Cadangan Senyawa");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    int y = HEADER_H + 3 + i * 10;
    bool sel = (i == levelSel);
    if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(1, y);
    oled.print(namaSensor[i]);
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
    teksKanan(buf, SCREEN_WIDTH - 2, y);
  }

}

// ================= HALAMAN: MENU KALIBRASI =================
void gambarKalibrasi() {
  gambarHeader("Kalibrasi");
  gambarList(menuKalibrasi, JUMLAH_MENU_KALIB, L_KALIB, HEADER_H, 16);
}

// ================= HALAMAN: KALIBRASI SENSOR LEVEL =================
void gambarKalibSensor() {
  // Header tampilkan senyawa aktif; tanda < > = ganti dgn KIRI/KANAN
  char judul[24];
  snprintf(judul, sizeof(judul), "Kalib: <%s>%s",
           namaSensor[kalibSensorSel], kalibSensorBerubah ? " *" : "");
  gambarHeader(judul);
  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  char buf[32];
  // Baris info: kapasitas & nilai raw saat ini
  int y = HEADER_H + 1;
  snprintf(buf, sizeof(buf), "Kap %dml Raw %d",
           kalibrasiSensor[kalibSensorSel].kapasitasMl,
           sensorAvg[kalibSensorSel]);
  oled.setCursor(2, y); oled.print(buf);
  y += 9;

  // 5 titik kalibrasi (ATAS/BAWAH: scroll, ENTER: set titik)
  const int persen[KALIBRASI_TITIK] = {0, 25, 50, 75, 100};
  for (int t = 0; t < KALIBRASI_TITIK; t++) {
    bool sel = (t == kalibSensorTitik);
    if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 8, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    snprintf(buf, sizeof(buf), "%3d%%: raw=%4d",
             persen[t], kalibrasiSensor[kalibSensorSel].titik[t]);
    oled.setCursor(2, y); oled.print(buf);
    y += 8;
  }
  oled.setTextColor(WHITE);
}

// ================= HALAMAN: KALIBRASI STEPPER =================
void gambarKalibStepper() {
  gambarHeader("Kalib Stepper");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  
  int y = HEADER_H + 4;
  const char* namaPompa[3] = {"HCl", "H2O2", "H2O"};
  
  for (int i = 0; i < 3; i++) {
    bool sel = (i == kalibStepperIdx);
    if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    
    oled.setCursor(2, y);
    oled.print(namaPompa[i]);
    
    char buf[24];
    float mlPerStep = kalibrasi.stepperMlPerStepX1000[i] / 1000.0;
    snprintf(buf, sizeof(buf), "%.3f ml/step", mlPerStep);
    teksKanan(buf, SCREEN_WIDTH - 2, y);
    y += 12;
  }
  
  y += 4;
  char buf[32];
  snprintf(buf, sizeof(buf), "Test: %d step", kalibStepperStep);
  oled.setCursor(2, y);
  oled.print(buf);
  
  y += 14;
  teksTengah("ATAS/BAWAH: pilih pompa", y, 1);
  y += 10;
  teksTengah("KIRI/KANAN: ubah step", y + 2, 1);
  y += 10;
  teksTengah("ENTER: jalankan test", y + 4, 1);
}

// ================= HALAMAN: KALIBRASI MQ-135 =================
void gambarKalibMQ135() {
  gambarHeader("Kalib MQ-135");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  
  int y = HEADER_H + 4;
  char buf[32];
  
  snprintf(buf, sizeof(buf), "Baseline: %d", kalibrasi.mq135Baseline);
  oled.setCursor(2, y); oled.print(buf);
  y += 12;
  
  snprintf(buf, sizeof(buf), "Margin:   %d", kalibrasi.mq135Margin);
  oled.setCursor(2, y); oled.print(buf);
  y += 12;
  
  snprintf(buf, sizeof(buf), "Bacaan:   %d", mq135Baca());
  oled.setCursor(2, y); oled.print(buf);
  y += 12;
  
  oled.print(mq135Normal() ? "Status: NORMAL" : "Status: TIDAK NORMAL");
  y += 16;
  
  teksTengah("ENTER: set baseline", y, 1);
  y += 10;
  teksTengah("KIRI/KANAN: ubah margin", y + 2, 1);
}

// ================= HALAMAN: KALIBRASI DS18B20 =================
void gambarKalibDS18B20() {
  gambarHeader("Kalib DS18B20");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  
  int y = HEADER_H + 4;
  char buf[32];
  
  float offset = kalibrasi.ds18b20OffsetX10 / 10.0;
  snprintf(buf, sizeof(buf), "Offset: %.1f C", offset);
  oled.setCursor(2, y); oled.print(buf);
  y += 12;
  
  float suhu = ds18b20Baca();
  snprintf(buf, sizeof(buf), "Suhu:   %.1f C", suhu);
  oled.setCursor(2, y); oled.print(buf);
  y += 16;
  
  teksTengah("KIRI/KANAN: ubah offset", y, 1);
  y += 10;
  teksTengah("(bandingkan dg termometer)", y + 2, 1);
}

// ================= HALAMAN: KALIBRASI AGITASI =================
void gambarKalibAgitasi() {
  gambarHeader("Kalib Agitasi");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  
  int y = HEADER_H + 4;
  char buf[32];
  
  snprintf(buf, sizeof(buf), "Duty min: %d", kalibrasi.agitasiDutyMin);
  oled.setCursor(2, y); oled.print(buf);
  y += 16;
  
  teksTengah("KIRI/KANAN: ubah duty", y, 1);
  y += 10;
  teksTengah("ENTER: test motor", y + 2, 1);
  y += 10;
  teksTengah("(cari duty min yang bisa", y + 4, 1);
  y += 10;
  teksTengah("gerakkan wadah)", y + 6, 1);
}

// ================= HALAMAN: TENTANG =================
void gambarTentang() {
  gambarHeader("Tentang");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  int y = HEADER_H + 4;
  oled.setCursor(2, y); oled.print(F("Firmware: ")); oled.println(FW_VERSI);
  y += 10;
  oled.setCursor(2, y); oled.print(F("MCU: ESP32-S3"));
  y += 10;
  oled.setCursor(2, y); oled.print(F("Display: OLED 128x64"));
  y += 10;
  oled.setCursor(2, y); oled.print(F("I2C: 0x3C/0x20"));
  y += 14;
  teksTengah("[BACK] kembali", 56, 1);
}

// ================= HALAMAN: ERROR =================
void gambarError() {
  gambarHeader("ERROR");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(2, 18); oled.print(prosesErrorKode);
  oled.setCursor(2, 30); oled.print(prosesErrorPesan);
  teksTengah("Exhaust ON", 44, 1);
  teksTengah("[ENTER] reset", 56, 1);
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

const int EDIT_ROW_H   = 10;
const int EDIT_VISIBLE = 5;

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
  int trackY = HEADER_H + 1, trackH = SCREEN_HEIGHT - trackY;
  int thumbH = trackH * EDIT_VISIBLE / JUMLAH_FIELD;
  int thumbY = trackY + (trackH - thumbH) * editScroll / max(1, JUMLAH_FIELD - EDIT_VISIBLE);
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
  oled.fillRect(14, 26, 100, 14, BLACK);
  oled.drawRect(14, 26, 100, 14, WHITE);
  oled.setTextColor(WHITE);
  teksTengah(toastTeks, 30, 1);
}

// ================= RENDER UTAMA =================
void gambarLayar() {
  oled.clearDisplay();
  switch (halaman) {
    case HAL_SPLASH:        gambarSplash();       break;
    case HAL_MENU_UTAMA:    gambarMenuUtama();    break;
    case HAL_PRESET_LIST:   gambarPresetList();   break;
    case HAL_PRESET_DETAIL: gambarPresetDetail(); break;
    case HAL_PROSES:        gambarProses();       break;
    case HAL_PLACEHOLDER:   teksTengah("Placeholder", 30, 1); break;
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