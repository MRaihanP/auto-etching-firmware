#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "data.h"
#include "process.h"
#include "presetops.h"
#include "sensor.h"
#include "safety.h"
#include "net.h"
#include "ota.h"
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
        v = (uint8_t)(buf[s * SCREEN_WIDTH + x] << geserBit);
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

// Pecah teks panjang menjadi maksimal 2 baris (masing-masing <= maxChar).
void teksTengahPecah(const char* s, int y, int maxChar) {
  if (s == nullptr) return;
  int len = (int)strlen(s);
  if (len <= maxChar) { teksTengah(s, y, 1); return; }

  int split = maxChar;
  while (split > 0 && s[split] != ' ') split--;
  if (split <= 0) split = maxChar;

  char a[40], b[40];
  int n1 = min(split, (int)sizeof(a) - 1);
  memcpy(a, s, n1); a[n1] = '\0';
  const char* rest = s + split;
  while (*rest == ' ') rest++;
  strncpy(b, rest, sizeof(b) - 1); b[sizeof(b) - 1] = '\0';
  teksTengah(a, y, 1);
  if (b[0]) teksTengah(b, y + 9, 1);
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

void gambarList(const char* const* items, int jumlah, int L,
                int areaAtas, int rowHeight) {
  int areaTinggi = AREA_BAWAH - areaAtas;
  int centerY    = areaAtas + areaTinggi / 2;

  oled.setTextColor(WHITE);
  oled.setTextSize(1);

  for (int j = 0; j < jumlah; j++) {
    float y = centerY + (j - lsPos[L]) * rowHeight;
    if (y < areaAtas - rowHeight || y > AREA_BAWAH + rowHeight) continue;

    int16_t x1, y1; uint16_t w, h;
    oled.getTextBounds(items[j], 0, 0, &x1, &y1, &w, &h);
    int ty = (int)round(y - h / 2.0);
    if (ty + (int)h <= areaAtas || ty > AREA_BAWAH) continue;
    oled.setCursor((SCREEN_WIDTH - w) / 2, ty);
    oled.print(items[j]);
  }

  oled.drawRoundRect(BOX_MARGIN_X, centerY - rowHeight / 2,
                     SCREEN_WIDTH - 2 * BOX_MARGIN_X, rowHeight,
                     BOX_RADIUS, WHITE);
}

// ================= HALAMAN: MAIN MENU =================
void gambarMenuUtama() {
  gambarList(menuUtama, JUMLAH_MENU, L_UTAMA, 0, 16);
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

  teksTengah("Belum ada preset", 34, 1);
  teksTengah("yang dibuat", 42, 1);
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
  if (totalDetik < 0) totalDetik = 0;
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

  char buf[32];
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
  y += dy + 2;
  if (terlihat(y, 1)) oled.drawFastHLine(0, y, SCREEN_WIDTH, WHITE);
  y += 3;

  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("Tembaga"));
    sprintf(buf, "%d%% %ldcm2", p.isiTembagaPct, luasTembagaCm2(p));
    teksKanan(buf, xKanan, y);
  }
  y += dy;
  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("Laju"));
    int oz = tebalOzX10(p);
    sprintf(buf, "%d.%doz %d.%dum/m", oz / 10, oz % 10,
            p.lajuEtchX10 / 10, p.lajuEtchX10 % 10);
    teksKanan(buf, xKanan, y);
  }
  y += dy;
  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("Etching"));
    sprintf(buf, "%ds%s Spd%d", agitasiDetikEfektif(p),
            p.modeWaktu ? " auto" : "", p.agitasiSpeed);
    teksKanan(buf, xKanan, y);
  }
  y += dy;
  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("Siklus"));
    sprintf(buf, "%d x", p.siklus);
    teksKanan(buf, xKanan, y);
  }
  y += dy;
  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("Bilas/siklus"));
    sprintf(buf, "S%d A%d %dml", p.bilasSabun, p.bilasAir, p.bilasMl);
    teksKanan(buf, xKanan, y);
  }
  y += dy;
  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("Konfirmasi"));
    teksKanan(p.konfirmasiSiklus ? "Ya" : "Tidak", xKanan, y);
  }
  y += dy;
  if (terlihat(y, dy)) {
    oled.setCursor(2, y);  oled.print(F("Estimasi"));
    formatMMSS(estimasiTotalDetikPreset(p), buf);
    teksKanan(buf, xKanan, y);
  }

  gambarIndikatorScroll(presetDetailScroll, maxScroll);
}

// ================= HALAMAN: PROSES =================
void gambarDialog(const char* l1, const char* l2, const char* l3) {
  oled.fillRect(8, 10, 112, 34, BLACK);
  oled.drawRect(8, 10, 112, 34, WHITE);
  oled.setTextColor(WHITE);
  oled.setTextSize(1);
  teksTengah(l1, 13, 1);
  teksTengah(l2, 23, 1);
  teksTengah(l3, 33, 1);
}

void gambarWaitPcb() {
  oled.setTextColor(WHITE);
  teksTengah("TARUH PCB", 4, 1);
  teksTengah("di wadah etching,", 16, 1);
  teksTengah("lalu tekan ENTER", 26, 1);
  teksTengah("BACK : batal", 40, 1);
}

void gambarKonfirmasiSiklus() {
  Preset &p = daftarPreset[presetTerpilih];
  char buf[24];
  oled.setTextColor(WHITE);
  sprintf(buf, "Siklus %d/%d selesai", prosesSiklus, p.siklus);
  teksTengah(buf, 0, 1);
  teksTengah("Trace sudah OK?", 9, 1);
  oled.setCursor(2, 19);
  oled.print(konfirmasiSel == 0 ? "> Siklus lagi" : "  Siklus lagi");
  oled.setCursor(2, 29);
  oled.print(konfirmasiSel == 1 ? "> Selesai" : "  Selesai");
  teksTengah("ATAS/BAWAH pilih, ENTER", 39, 1);
}

void gambarJeda() {
  oled.fillRect(0, 0, SCREEN_WIDTH, AREA_BAWAH, BLACK);
  oled.drawRect(0, 0, SCREEN_WIDTH, AREA_BAWAH, WHITE);
  oled.setTextColor(WHITE);
  oled.setTextSize(1);
  teksTengah("PROSES JEDA", 0, 1);

  Preset &p = daftarPreset[presetTerpilih];
  char buf[24];
  sprintf(buf, "S%d/%d %s", prosesSiklus, p.siklus, namaFase[prosesFase]);
  teksTengah(buf, 8, 1);

  const char* opsi[3] = { "Lanjutkan", "Batalkan + drain", "Abaikan (stop)" };
  for (int i = 0; i < 3; i++) {
    sprintf(buf, "%s%s", (jedaSel == i) ? "> " : "  ", opsi[i]);
    oled.setCursor(4, 15 + i * 9);
    oled.print(buf);
  }
  teksTengah("ATAS/BAWAH, ENTER", 41, 1);
}

void gambarProses() {
  if (prosesTungguPcb) { gambarWaitPcb(); return; }
  if (prosesKonfirmasi) { gambarKonfirmasiSiklus(); return; }
  if (prosesJeda) { gambarJeda(); return; }

  Preset &p = daftarPreset[presetTerpilih];
  unsigned long totalMs = prosesTotalMsEst;
  if (totalMs == 0) totalMs = (unsigned long)prosesTotalDetik() * 1000UL;
  float frac = 0.0f;
  if (totalMs > 0) frac = (float)prosesAkumMs / (float)totalMs;
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;

  oled.setTextColor(WHITE);
  char buf[24];
  int persen = (int)(frac * 100);

  char layer[10];
  strncpy(layer, p.nama, sizeof(layer) - 1);
  layer[sizeof(layer) - 1] = '\0';
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(layer);
  sprintf(buf, "%d%%", persen);
  teksKanan(buf, SCREEN_WIDTH, 0);

  int lewatDetik = prosesAkumMs / 1000;
  formatMMSS(lewatDetik, buf);
  teksTengah(buf, 9, 1);

  int barX = 0, barY = 17, barW = SCREEN_WIDTH, barH = 7;
  oled.drawRoundRect(barX, barY, barW, barH, 3, WHITE);
  int isi = (int)((barW - 6) * frac);
  if (isi > 0) oled.fillRect(barX + 3, barY + 1, isi, barH - 2, WHITE);

  int sisaDetik = 0;
  if (totalMs > prosesAkumMs) sisaDetik = (totalMs - prosesAkumMs) / 1000;
  char sisa[12];
  formatMMSS(sisaDetik, sisa);

  // Baris status: siklus/fase + suhu
  sprintf(buf, "S%d/%d", prosesSiklus, p.siklus);
  oled.setCursor(0, 26);
  oled.print(buf);
  oled.setCursor(24, 26);
  oled.print(namaFase[prosesFase]);
  float t = sensorSuhuChamber();
  if (sensorSuhuValid(2)) {
    sprintf(buf, "%.1fC", t);
  } else {
    strcpy(buf, "--C");
  }
  teksKanan(buf, SCREEN_WIDTH, 26);

  sprintf(buf, "sisa %s", sisa);
  oled.setCursor(0, 34);
  oled.print(buf);
  const char* st = prosesPeringatanSuhu ? "TEMP!" :
                   (prosesPeringatanLevel ? "LEVEL!" :
                   (prosesPeringatanLimbah ? "LIMBAH!" :
                   (prosesTangkiMenipis() ? "TANK!" : "OK")));
  teksKanan(st, SCREEN_WIDTH, 34);

  // Dosing/drain berjalan: tampilkan ml yang sedang dipindahkan.
  const char* namaDosis = nullptr;
  int targetDosis = 0, sudahDosis = 0;
  if (prosesLangkah == L_DRAIN || prosesFase == FASE_DRAIN_BATAL) {
    sprintf(buf, "Drain %d ml", volumeWadah);
    oled.setCursor(0, 42);
    oled.print(buf);
  } else if (prosesInfoDosing(&namaDosis, &targetDosis, &sudahDosis)) {
    sprintf(buf, "%s %d/%d ml", namaDosis, sudahDosis, targetDosis);
    oled.setCursor(0, 42);
    oled.print(buf);
  }
}

// ================= HALAMAN: PLACEHOLDER =================
void gambarPlaceholder() {
  gambarHeader(menuUtama[lsSel[L_UTAMA]]);
  teksTengah("Halaman ini", 26, 1);
  teksTengah("belum tersedia", 36, 1);
  oled.setTextSize(1);
  teksTengah("[BACK] kembali", 40, 1);
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
    case FP_PCB_PANJANG: sprintf(buf, "%d mm", p.pcbPanjang); break;
    case FP_PCB_LEBAR:   sprintf(buf, "%d mm", p.pcbLebar);   break;
    case FP_MODE_RASIO:  sprintf(buf, "%s", p.modeRasio ? "ml" : "persen"); break;
    case FP_HCL:
      if (p.modeRasio == 0) sprintf(buf, "%d%%", p.persenHCl);
      else                  sprintf(buf, "%d ml", p.mlHCl);
      break;
    case FP_H2O2:
      if (p.modeRasio == 0) sprintf(buf, "%d%%", p.persenH2O2);
      else                  sprintf(buf, "%d ml", p.mlH2O2);
      break;
    case FP_H2O:         sprintf(buf, "%d%% %dml", p.persenH2O, p.mlH2O); break;
    case FP_VOLUME:      sprintf(buf, "%d ml", volumeTotal(p)); break;
    case FP_ISI_TEMBAGA: sprintf(buf, "%d%%", p.isiTembagaPct); break;
    case FP_TEBAL_TEMBAGA: {
      int oz = tebalOzX10(p);
      sprintf(buf, "%d.%d oz", oz / 10, oz % 10);
      break;
    }
    case FP_LAJU_ETCH:   sprintf(buf, "%d.%d um/m", p.lajuEtchX10 / 10,
                                 p.lajuEtchX10 % 10); break;
    case FP_MODE_WAKTU:  sprintf(buf, "%s", p.modeWaktu ? "Auto" : "Manual"); break;
    case FP_ETCHING:
      if (p.modeWaktu) sprintf(buf, "%ds auto", agitasiDetikEfektif(p));
      else             sprintf(buf, "%d s", p.agitasiDetik);
      break;
    case FP_SPEED:       sprintf(buf, "SPD%d", p.agitasiSpeed); break;
    case FP_SIKLUS:      sprintf(buf, "%d", p.siklus); break;
    case FP_BILAS_SABUN: sprintf(buf, "%d", p.bilasSabun); break;
    case FP_BILAS_AIR:   sprintf(buf, "%d", p.bilasAir); break;
    case FP_BILAS_ML:    sprintf(buf, "%d ml", p.bilasMl); break;
    case FP_BILAS_DETIK: sprintf(buf, "%d s", p.bilasDetik); break;
    case FP_KONFIRMASI:  sprintf(buf, "%s", p.konfirmasiSiklus ? "Ya" : "Tidak"); break;
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

// ================= HALAMAN: LEVEL CAIRAN =================
void gambarLevel() {
  gambarHeader("Level Cairan");

  if (!sensorOk) {
    teksTengah("Sensor tidak", 22, 1);
    teksTengah("aktif", 32, 1);
    teksTengah("[BACK] kembali", 40, 1);
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

    char buf[16];
    const char* st = sensorFault[i] ? "FAULT" : (sensorBasah[i] ? "BASAH" : "KERING");
    sprintf(buf, "%d%% %s", sensorPersen[i], st);
    teksKanan(buf, SCREEN_WIDTH - 1, y);

    int barY = y + 9;
    oled.drawRect(barX, barY, barW, barH, WHITE);
    int isi = (barW - 2) * sensorPersen[i] / 100;
    if (isi > 0) oled.fillRect(barX + 1, barY + 1, isi, barH - 2, WHITE);

    y += ROW_H;
  }

  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 4;
    if (levelScroll > 0)
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    if (levelScroll < maxScroll)
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
  }
}

// ================= HALAMAN: KALIBRASI MAIN =================
const char* const menuKalib[] = { "Motor", "Sensor Level", "Sensor Suhu", "Sensor Udara", "E-Stop" };
const int JUMLAH_MENU_KALIB = 5;

const char* const menuMotor[] = {
  "Pompa HCl", "Pompa H2O2", "Pompa Air",
  "Pompa Sabun", "Pompa Limbah", "Motor Agitasi",
  "Test Motor"
};
const int JUMLAH_MENU_MOTOR = 7;

// Pompa sudah punya hasil kalibrasi tersimpan (NVS dimuat ke RAM saat boot).
bool pompaSudahKalibrasi(int menuIdx) {
  if (menuIdx >= 0 && menuIdx <= 2)
    return kalibStepperA[menuIdx] > 0.0f || kalibStepperMl[menuIdx] > 0.0f;
  if (menuIdx == 3 || menuIdx == 4)
    return kalibDcMlPerSec[menuIdx - 3] > 0.0f;
  return false;
}

// Badge "OK" di kanan header halaman kalibrasi pompa.
void gambarBadgeKalib(bool ada) {
  if (!ada) return;
  oled.setTextSize(1);
  oled.setTextColor(BLACK);
  oled.setCursor(SCREEN_WIDTH - 16, 2);
  oled.print(F("OK"));
  oled.setTextColor(WHITE);
}

void gambarKalibMotorList() {
  gambarHeader("Motor");
  static char label[JUMLAH_MENU_MOTOR][24];
  const char* item[JUMLAH_MENU_MOTOR];
  for (int i = 0; i < JUMLAH_MENU_MOTOR; i++) {
    if (pompaSudahKalibrasi(i)) snprintf(label[i], sizeof(label[i]), "%s [OK]", menuMotor[i]);
    else                        snprintf(label[i], sizeof(label[i]), "%s", menuMotor[i]);
    item[i] = label[i];
  }
  gambarList(item, JUMLAH_MENU_MOTOR, L_AKSI, HEADER_H, 16);
}

// ================= HALAMAN: KALIBRASI STEPPER =================
const int KALIB_STEPPER_ROW_TOP[8] = { 0, 14, 24, 34, 44, 54, 64, 74 };
const int KALIB_STEPPER_ROW_H[8]   = { 12, 10, 10, 10, 10, 10, 10, 10 };
const int KALIB_STEPPER_CONTENT_H  = 84;

void gambarKalibStepper() {
  const char* nama[] = { "HCl", "H2O2", "Air" };
  char judul[20];
  sprintf(judul, "Kal %s", nama[kalibMotorIdx]);
  gambarHeader(judul);
  gambarBadgeKalib(pompaSudahKalibrasi(kalibMotorIdx));

  int idx = kalibMotorIdx;
  int maxScroll = max(0, KALIB_STEPPER_CONTENT_H - (AREA_BAWAH - HEADER_H - 4));
  if (kalibStepperScroll < 0) kalibStepperScroll = 0;
  if (kalibStepperScroll > maxScroll) kalibStepperScroll = maxScroll;

  int baseY = HEADER_H + 4 - kalibStepperScroll;
  char buf[24];
  oled.setTextSize(1);

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
      if (kalibStepperPriming[idx] && pompaAktif(idx)) {
        unsigned long detik = (millis() - kalibStepperPrimeMs[idx]) / 1000UL;
        sprintf(buf, "PRIME %lus", detik);
      } else if (pompaAktif(idx)) {
        sprintf(buf, "STOP %d%%", pompaProgres(idx));
      } else if (kalibStepperCekGelas[idx]) {
        strcpy(buf, "Gelas kosong?");
      } else {
        strcpy(buf, "START 100 ml");
      }
      teksTengah(buf, y + 2, 1);
    }
  }

  const int rowCursor[7] = { 1, 2, 3, 4, 5, 6, 7 };
  for (int r = 0; r < 7; r++) {
    int cur = rowCursor[r];
    int y = baseY + KALIB_STEPPER_ROW_TOP[cur];
    if (y + 10 <= HEADER_H || y >= AREA_BAWAH) continue;
    bool sel = (kalibStepperCursor == cur);
    if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(2, y);

    switch (cur) {
      case 1:
        oled.print(F("Priming"));
        strcpy(buf, (kalibStepperPriming[idx] && pompaAktif(idx)) ? "STOP" : "ENTER");
        break;
      case 2:
        oled.print(F("RPM"));
        sprintf(buf, "%d", rpmPompaValid(kalibStepperRpm[idx]));
        break;
      case 3:
        oled.print(F("Target"));
        sprintf(buf, "%d ml", KALIB_STEPPER_TARGET_ML);
        break;
      case 4:
        if (kalibStepperStep == 1)       oled.print(F("Hasil x0.1"));
        else if (kalibStepperStep == 50) oled.print(F("Hasil x5"));
        else                             oled.print(F("Hasil x1"));
        if (kalibStepperUkurMl10[idx] > 0)
          sprintf(buf, "%.1f ml", kalibStepperUkurMl10[idx] / 10.0f);
        else
          strcpy(buf, "--");
        break;
      case 5:
        oled.print(F("Koreksi"));
        if (kalibStepperOffsetAda[idx])
          sprintf(buf, "%+.1f%%", kalibStepperOffsetX10[idx] / 10.0f);
        else
          strcpy(buf, "--");
        break;
      case 6: {
        oled.print(F("Run"));
        int n = kalibStepperRunKe[idx];
        if (n >= KALIB_STEPPER_RUN_MIN)
          sprintf(buf, "%d/%d %s", KALIB_STEPPER_RUN_MIN, KALIB_STEPPER_RUN_MIN,
                  kalibStepperLulus[idx] ? "LULUS" : "ULANGI");
        else if (n == 0 && pompaSudahKalibrasi(idx))
          strcpy(buf, "TERSIMPAN");
        else
          sprintf(buf, "%d/%d", n, KALIB_STEPPER_RUN_MIN);
        break;
      }
      case 7:
        oled.print(F("Reset"));
        strcpy(buf, "ENTER");
        break;
    }
    teksKanan(buf, SCREEN_WIDTH - 2, y);
  }

  oled.setTextColor(WHITE);
  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 4;
    if (kalibStepperScroll > 0)
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    if (kalibStepperScroll < maxScroll)
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
  }
}

// ================= HALAMAN: TEST MOTOR =================
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
  if (row == 4 || row == 5) return dcAktif(row - 3);
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

  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 3;
    if (testMotorScroll > 0)
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    if (testMotorScroll < maxScroll)
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
  }
}

// ================= HALAMAN: KALIBRASI DC =================
const int KALIB_DC_CONTENT_H  = 84;
const int KALIB_DC_ROW_TOP[8] = { 0, 14, 24, 34, 44, 54, 64, 74 };
const int KALIB_DC_ROW_H[8]   = { 12, 10, 10, 10, 10, 10, 10, 10 };

// Baris: 0 START/STOP, 1 Duty, 2 Hasil, 3 Step, 4 Laju, 5 ms per ml, 6 Run, 7 Reset
void gambarKalibDC() {
  const char* nama[] = { "Sabun", "Limbah" };
  int idx = kalibMotorIdx - 3;
  char judul[16];
  sprintf(judul, "Kal %s", nama[idx]);
  gambarHeader(judul);
  gambarBadgeKalib(kalibDcMlPerSec[idx] > 0.0f);

  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  int maxScroll = max(0, KALIB_DC_CONTENT_H - (AREA_BAWAH - HEADER_H - 4));
  if (kalibDCScroll < 0) kalibDCScroll = 0;
  if (kalibDCScroll > maxScroll) kalibDCScroll = maxScroll;

  int baseY = HEADER_H + 4 - kalibDCScroll;
  char buf[24];

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
        else strcpy(buf, "STOP");
      } else {
        sprintf(buf, "START %lus", kalibDcDurasiRun(idx) / 1000UL);
      }
      teksTengah(buf, y + 2, 1);
    }
  }

  const int rowCursor[7] = { 1, 2, 3, 4, 5, 6, 7 };
  for (int r = 0; r < 7; r++) {
    int cur = rowCursor[r];
    int y = baseY + KALIB_DC_ROW_TOP[cur];
    if (y + 10 <= HEADER_H || y >= AREA_BAWAH) continue;
    bool sel = (kalibDCCursor == cur);
    if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(2, y);

    int n = kalibDcTitikN[idx];
    switch (cur) {
      case 1:
        oled.print(F("Duty"));
        sprintf(buf, "%d%%", kalibDutyDC[idx]);
        break;
      case 2:
        sprintf(buf, "Hasil x%d", kalibDcStep);
        oled.print(buf);
        if (kalibDcUkurMl[idx] > 0) sprintf(buf, "%d ml", kalibDcUkurMl[idx]);
        else strcpy(buf, "--");
        break;
      case 3:
        oled.print(F("Step hasil"));
        sprintf(buf, "%d ml", kalibDcStep);
        break;
      case 4: {
        oled.print(F("Laju run"));
        float lj = kalibDcLajuTerakhir(idx);
        if (lj > 0.0f) sprintf(buf, "%.2f ml/s", lj);
        else strcpy(buf, "--");
        break;
      }
      case 5:
        oled.print(F("1 ml ="));
        if (kalibDcMlPerSec[idx] > 0.0f)
          sprintf(buf, "%.1f ms", 1000.0f / kalibDcMlPerSec[idx]);
        else strcpy(buf, "--");
        break;
      case 6:
        oled.print(F("Run"));
        if (n >= KALIB_DC_RUN_MIN)
          sprintf(buf, "%d/%d TERSIMPAN", KALIB_DC_RUN_MIN, KALIB_DC_RUN_MIN);
        else if (n > 0)
          sprintf(buf, "%d/%d (simpan)", n, KALIB_DC_RUN_MIN);
        else if (kalibDcMlPerSec[idx] > 0.0f)
          strcpy(buf, "TERSIMPAN");
        else
          strcpy(buf, "--");
        break;
      case 7:
        oled.print(F("Reset kalib"));
        strcpy(buf, "ENTER");
        break;
    }
    teksKanan(buf, SCREEN_WIDTH - 2, y);
  }

  oled.setTextColor(WHITE);
  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 4;
    if (kalibDCScroll > 0)
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    if (kalibDCScroll < maxScroll)
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
  }
}

// ================= HALAMAN: AGITASI =================
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

// ================= HALAMAN: KALIBRASI SUHU =================
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
    if (blokSel) oled.drawRoundRect(2, y - 1, SCREEN_WIDTH - 4, SUHU_ROW_H - 2, 3, WHITE);
    oled.setTextColor(WHITE);
    teksTengah(namaSensorSuhu[i], y + 1, 1);

    int rowY = y + 10;
    bool fieldAddr  = blokSel && kalibSuhuField == 0;
    bool fieldSuhu  = blokSel && kalibSuhuField == 1;
    char buf[16];

    sprintf(buf, "addr:%d", kalibSuhuAddr[i]);
    if (fieldAddr) oled.fillRect(4, rowY - 1, 38, 10, WHITE);
    oled.setTextColor(fieldAddr ? BLACK : WHITE);
    oled.setCursor(5, rowY);
    oled.print(buf);

    int vx = SCREEN_WIDTH - 7;
    uint16_t warnaTeks = fieldSuhu ? BLACK : WHITE;
    if (fieldSuhu) oled.fillRect(vx - 30, rowY - 1, 31, 10, WHITE);
    oled.setTextColor(warnaTeks);
    if (sensorSuhuValid(i)) {
      sprintf(buf, "%.1f", sensorSuhu(i));
      int16_t x1, y1; uint16_t w, h;
      oled.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
      oled.setCursor(vx - (int)w - 6, rowY);
      oled.print(buf);
      oled.drawCircle(vx - 5, rowY + 3, 1, warnaTeks);
      oled.setCursor(vx - 1, rowY);
      oled.print(F("C"));
    } else {
      oled.setCursor(vx - 30, rowY);
      oled.print(sensorSuhuStatusTeks());
    }
  }
  oled.setTextColor(WHITE);

  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 4;
    if (kalibSuhuScroll > 0)
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    if (kalibSuhuScroll < maxScroll)
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
  }
}

// ================= HALAMAN: SENSOR UDARA =================
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
  oled.setTextColor(WHITE);
  char buf[24];
  int ppm = pengaturan.mqAktif ? (int)mq135PPM() : 0;
  if (pengaturan.mqAktif) sprintf(buf, "Bacaan: %d ppm", ppm);
  else strcpy(buf, "MQ135 nonaktif");
  teksTengah(buf, y + 4, 1);
}

void gambarKalibMain() {
  gambarHeader("Kalibrasi");
  const char* item[JUMLAH_MENU_KALIB];
  for (int i = 0; i < JUMLAH_MENU_KALIB; i++) item[i] = menuKalib[i];
  gambarList(item, JUMLAH_MENU_KALIB, L_AKSI, HEADER_H, 16);
}

// ================= HALAMAN: DIAGNOSTIK E-STOP =================
void gambarKalibEstop() {
  gambarHeader("E-Stop");

  bool low = (digitalRead(PIN_ESTOP) == LOW);
  oled.setTextColor(WHITE);
  oled.setTextSize(1);

  oled.drawRoundRect(8, 14, 112, 17, 4, WHITE);
  teksTengah(low ? "DITEKAN" : "LEPAS", 18, 1);

  char buf[24];
  sprintf(buf, "GPIO%d: %s (%d)", PIN_ESTOP, low ? "LOW" : "HIGH", low ? 0 : 1);
  teksTengah(buf, 34, 1);

  const char* st = (safetyState == SAF_ESTOP) ? "E-STOP" :
                   (safetyState == SAF_ERROR ? "ERROR" : "AMAN");
  sprintf(buf, "Safety: %s", st);
  teksTengah(buf, 42, 1);
}

// ================= HALAMAN: KALIBRASI LEVEL =================
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
    teksKanan(sensorKalibSimpan[i] ? "[OK]" : "--", SCREEN_WIDTH - 10, y);   // [OK] = tersimpan
  }
  oled.setTextColor(WHITE);
  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 4;
    if (kalibLevelScroll > 0)
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    if (kalibLevelScroll < maxScroll)
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
  }
}

void gambarKalibLevelSet() {
  gambarHeader(namaSensor[kalibSensorIdx]);
  gambarBadgeKalib(sensorKalibSimpan[kalibSensorIdx]);
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  int y = HEADER_H + 4;
  char buf[16];

  bool sel0 = (kalibSetStep == 0);
  if (sel0) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
  oled.setTextColor(sel0 ? BLACK : WHITE);
  oled.setCursor(2, y);
  oled.print(F("0%"));
  sprintf(buf, "= %d", sensorKalibKosong[kalibSensorIdx]);
  teksKanan(buf, SCREEN_WIDTH - 2, y);
  y += 12;

  bool sel100 = (kalibSetStep == 1);
  if (sel100) oled.fillRect(0, y - 1, SCREEN_WIDTH, 10, WHITE);
  oled.setTextColor(sel100 ? BLACK : WHITE);
  oled.setCursor(2, y);
  oled.print(F("100%"));
  sprintf(buf, "= %d", sensorKalibPenuh[kalibSensorIdx]);
  teksKanan(buf, SCREEN_WIDTH - 2, y);
  y += 14;

  oled.setTextColor(WHITE);
  sprintf(buf, "Raw: %d", sensorRaw[kalibSensorIdx]);
  teksTengah(buf, y, 1);
}

// ================= HALAMAN: PENGATURAN =================
const char* const labelSet[] = {
  "Suhu waspada", "Suhu bahaya", "Margin drain",
  "Stabil level", "MQ135", "IoT",
  "Kap HCl", "Kap H2O2", "Kap H2O", "Kap sabun", "Kap limbah"
};
const int JUMLAH_SET = 11;

int settingTop = 0;   // indeks baris pertama yang terlihat

void gambarPengaturan() {
  gambarHeader("Pengaturan");
  oled.setTextSize(1);
  char buf[16];

  const int ROW_H   = 9;
  const int Y0      = HEADER_H + 3;
  int visible = (AREA_BAWAH - Y0) / ROW_H;       // baris yang muat penuh
  if (visible < 1) visible = 1;
  if (visible > JUMLAH_SET) visible = JUMLAH_SET;

  // Auto-scroll: kursor selalu terlihat.
  if (settingCursor < settingTop) settingTop = settingCursor;
  if (settingCursor >= settingTop + visible) settingTop = settingCursor - visible + 1;
  if (settingTop > JUMLAH_SET - visible) settingTop = JUMLAH_SET - visible;
  if (settingTop < 0) settingTop = 0;

  int y = Y0;
  for (int i = settingTop; i < settingTop + visible && i < JUMLAH_SET; i++, y += ROW_H) {
    bool sel = (settingCursor == i);
    if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, ROW_H, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(2, y);
    oled.print(labelSet[i]);
    switch (i) {
      case 0: sprintf(buf, "%.1fC", pengaturan.tempPeringatanX10 / 10.0f); break;
      case 1: sprintf(buf, "%.1fC", pengaturan.tempBahayaX10 / 10.0f); break;
      case 2: sprintf(buf, "%d%%", pengaturan.drainMarginPct); break;
      case 3: sprintf(buf, "%.1fs", pengaturan.levelStabilMs / 1000.0f); break;
      case 4: strcpy(buf, pengaturan.mqAktif ? "ON" : "OFF"); break;
      case 5: strcpy(buf, pengaturan.iotEnabled ? "ON" : "OFF"); break;
      case 6: sprintf(buf, "%d ml", pengaturan.kapHclMl); break;
      case 7: sprintf(buf, "%d ml", pengaturan.kapH2o2Ml); break;
      case 8: sprintf(buf, "%d ml", pengaturan.kapH2oMl); break;
      case 9: sprintf(buf, "%d ml", pengaturan.kapSabunMl); break;
      case 10: sprintf(buf, "%d ml", pengaturan.kapLimbahMl); break;
    }
    teksKanan(buf, SCREEN_WIDTH - 12, y);       // sisakan ruang untuk panah scroll
  }

  // Panah scroll: ada opsi lain di atas / di bawah.
  // Warna panah kontras dengan baris di bawahnya (baris terpilih berlatar putih).
  const int x = SCREEN_WIDTH - 5;
  if (settingTop > 0) {
    uint16_t w = (settingCursor == settingTop) ? BLACK : WHITE;
    oled.fillTriangle(x - 3, Y0 + 4, x + 3, Y0 + 4, x, Y0, w);
  }
  if (settingTop + visible < JUMLAH_SET) {
    uint16_t w = (settingCursor == settingTop + visible - 1) ? BLACK : WHITE;
    int yb = Y0 + (visible - 1) * ROW_H;
    oled.fillTriangle(x - 3, yb + 1, x + 3, yb + 1, x, yb + 6, w);
  }
  oled.setTextColor(WHITE);
}

// ================= HALAMAN: ISI ULANG TANGKI =================
void gambarTank() {
  gambarHeader("Tangki");
  const char* label[5] = { "HCl", "H2O2", "H2O", "Sabun", "Limbah" };
  long nilai[5] = { tankHclMl, tankH2o2Ml, tankH2oMl, tankSabunMl, tankLimbahMl };
  oled.setTextSize(1);
  char buf[16];

  const int ROW_H   = 9;
  const int Y0      = HEADER_H + 1;
  int visible = (AREA_BAWAH - Y0) / ROW_H;
  if (visible < 1) visible = 1;
  if (visible > 5) visible = 5;

  // Auto-scroll seperti menu Pengaturan.
  if (tankEditIdx < tankScroll) tankScroll = tankEditIdx;
  if (tankEditIdx >= tankScroll + visible) tankScroll = tankEditIdx - visible + 1;
  if (tankScroll > 5 - visible) tankScroll = 5 - visible;
  if (tankScroll < 0) tankScroll = 0;

  for (int r = 0; r < visible; r++) {
    int i = tankScroll + r;
    if (i >= 5) break;
    int y = Y0 + r * ROW_H;
    bool sel = (tankEditIdx == i);
    if (sel) oled.fillRect(0, y - 1, SCREEN_WIDTH, ROW_H, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(2, y);
    oled.print(label[i]);
    sprintf(buf, "%ld/%d", nilai[i], kapasitasTank(i));
    teksKanan(buf, SCREEN_WIDTH - 12, y);
  }

  // Panah scroll di kanan.
  const int x = SCREEN_WIDTH - 5;
  if (tankScroll > 0) {
    uint16_t w = (tankEditIdx == tankScroll) ? BLACK : WHITE;
    oled.fillTriangle(x - 3, Y0 + 4, x + 3, Y0 + 4, x, Y0, w);
  }
  if (tankScroll + visible < 5) {
    uint16_t w = (tankEditIdx == tankScroll + visible - 1) ? BLACK : WHITE;
    int yb = Y0 + (visible - 1) * ROW_H;
    oled.fillTriangle(x - 3, yb + 1, x + 3, yb + 1, x, yb + 6, w);
  }
  oled.setTextColor(WHITE);
}

// ================= HALAMAN: STATUS IOT =================
void gambarIotStatus() {
  gambarHeader("Status IoT");

  const int ROW_H = 9;
  const int Y0 = HEADER_H + 1;
  const int TOTAL = 13;
  int visible = (AREA_BAWAH - Y0) / ROW_H;
  if (visible < 1) visible = 1;
  int maxScroll = max(0, TOTAL - visible);
  iotScroll = constrain(iotScroll, 0, maxScroll);

  char ssid[20];
  char ip[20];
  snprintf(ssid, sizeof(ssid), "%.17s", WiFi.SSID().c_str());
  snprintf(ip, sizeof(ip), "%s", WiFi.localIP().toString().c_str());

  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  for (int r = 0; r < visible; r++) {
    int idx = iotScroll + r;
    if (idx >= TOTAL) break;
    int y = Y0 + r * ROW_H;
    char buf[28];
    const char* label = "";
    const char* val = "";

    switch (idx) {
      case 0:
        label = "WiFi";
        switch (WiFi.status()) {
          case WL_CONNECTED:       val = "OK";      break;
          case WL_NO_SSID_AVAIL:   val = "NO_SSID"; break;
          case WL_CONNECT_FAILED:  val = "FAIL";    break;
          case WL_CONNECTION_LOST: val = "LOST";    break;
          case WL_DISCONNECTED:    val = "DISC";    break;
          case WL_IDLE_STATUS:     val = "IDLE";    break;
          default:                 val = "ERR";     break;
        }
        break;
      case 1:
        label = "SSID";
        val = ssid;
        break;
      case 2:
        label = "IP";
        val = ip;
        break;
      case 3:
        label = "RSSI";
        snprintf(buf, sizeof(buf), "%d dBm", WiFi.RSSI());
        val = buf;
        break;
      case 4:
        label = "MQTT";
        val = mqttClient.isConnected() ? "OK" :
              (netMqttStarted ? "START" : "--");
        break;
      case 5:
        label = "Host";
        snprintf(buf, sizeof(buf), "%.17s", netCfg.host);
        val = buf;
        break;
      case 6:
        label = "Port";
        snprintf(buf, sizeof(buf), "%u %s", netCfg.port,
                 (NET_ENABLE_TLS && netCfg.port == 8883) ? "TLS" : "PLAIN");
        val = buf;
        break;
      case 7:
        label = "Device";
        val = netCfg.deviceId;
        break;
      case 8:
        label = "Base";
        val = netCfg.baseTopic;
        break;
      case 9:
        label = "NTP";
        val = (time(nullptr) >= 1767225600L) ? "OK" : "--";
        break;
      case 10:
        label = "OTA";
        val = otaAktif ? "AKTIF" :
              (otaHasilTerakhir == 1 ? "OK" :
              (otaHasilTerakhir == 2 ? "ERROR" :
              (otaStarted ? "SIAP" : "--")));
        break;
      case 11:
        label = "Reason";
        snprintf(buf, sizeof(buf), "%u", (unsigned)netWifiReason);
        val = buf;
        break;
      case 12:
        label = "Heap";
        snprintf(buf, sizeof(buf), "%u", (unsigned)ESP.getFreeHeap());
        val = buf;
        break;
    }

    oled.setCursor(2, y);
    oled.print(label);
    teksKanan(val, SCREEN_WIDTH - 2, y);
  }

  gambarIndikatorScroll(iotScroll, maxScroll);
}

// ================= HALAMAN: INFO / STATUS =================
// 11 baris (versi, 3 suhu, 5 level, MQ135, alasan reset) di area 128x37 px,
// jadi halaman ini bisa di-scroll ATAS/BAWAH.
void gambarInfo() {
  gambarHeader("Info/Status");
  oled.setTextSize(1);

  const int dy = 9;
  const int totalBaris = 1 + JUMLAH_SUHU + JUMLAH_SENSOR + 2;
  int contentH = totalBaris * dy + 2;
  int viewH = AREA_BAWAH - HEADER_H;
  int maxScroll = max(0, contentH - viewH);
  infoScroll = constrain(infoScroll, 0, maxScroll);

  int y = HEADER_H + 2 - infoScroll;
  char buf[24];
  for (int i = 0; i < totalBaris; i++, y += dy) {
    if (y + 7 < HEADER_H || y + 7 > AREA_BAWAH) continue;
    const char* label = "";
    const char* nilai = buf;
    buf[0] = 0;

    if (i == 0) {
      label = "Versi";
      strncpy(buf, FW_VERSI, sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
    } else if (i >= 1 && i <= JUMLAH_SUHU) {
      int s = i - 1;
      label = namaSensorSuhu[s];
      if (sensorSuhuValid(s)) snprintf(buf, sizeof(buf), "%.1fC", sensorSuhu(s));
      else strcpy(buf, "--");
    } else if (i <= JUMLAH_SUHU + JUMLAH_SENSOR) {
      int s = i - JUMLAH_SUHU - 1;
      label = namaSensor[s];
      const char* st = sensorFault[s] ? "FLT" : (sensorBasah[s] ? "BASAH" : "KER");
      snprintf(buf, sizeof(buf), "%d%% %s", sensorPersen[s], st);
    } else if (i == totalBaris - 2) {
      label = "MQ135";
      if (pengaturan.mqAktif) snprintf(buf, sizeof(buf), "%d ppm", mq135PPMTerkini);
      else strcpy(buf, "OFF");
    } else {
      label = "Reset";
      nilai = infoAlasanReset;
    }

    oled.setCursor(2, y);
    oled.print(label);
    teksKanan(nilai, SCREEN_WIDTH - 2, y);
  }

  if (maxScroll > 0) {
    const int x = SCREEN_WIDTH - 4;
    if (infoScroll > 0)
      oled.fillTriangle(x - 2, HEADER_H + 3, x + 2, HEADER_H + 3, x, HEADER_H + 1, WHITE);
    if (infoScroll < maxScroll)
      oled.fillTriangle(x - 2, AREA_BAWAH - 3, x + 2, AREA_BAWAH - 3, x, AREA_BAWAH - 1, WHITE);
  }
}

// ================= TOAST =================
const char* toastTeks = "";
bool toastAktif = false;
unsigned long toastSampai = 0;

void tampilToast(const char* teks) {
  toastTeks = teks;
  toastAktif = true;
  toastSampai = millis() + 1200;
  buzzerNotif();
  perluGambar = true;
}

void gambarToast() {
  oled.fillRect(4, 26, 120, 14, BLACK);
  oled.drawRect(4, 26, 120, 14, WHITE);
  oled.setTextColor(WHITE);
  teksTengah(toastTeks, 30, 1);
}

// ================= OVERLAY KESELAMATAN =================
void gambarSafety() {
  oled.fillRect(0, 0, SCREEN_WIDTH, AREA_BAWAH, BLACK);
  oled.drawRect(0, 0, SCREEN_WIDTH, AREA_BAWAH, WHITE);
  oled.setTextColor(WHITE);
  oled.setTextSize(1);

  char buf[24];
  if (safetyState == SAF_ESTOP) {
    teksTengah("!! E-STOP !!", 0, 1);
    teksTengah(safetyDetail[0] ? safetyDetail : "Semua aktuator padam", 9, 1);
    if (evakuasiAktif) {
      sprintf(buf, "Drain %d ml", evakuasiMl);
      teksTengah(buf, 18, 1);
      sprintf(buf, "Sisa %ds", evakuasiSisaDetik());
      teksTengah(buf, 27, 1);
      teksTengah("Tunggu drain selesai", 36, 1);
    } else {
      teksTengahPecah(safetyAksi[0] ? safetyAksi : "Tekan ENTER reset", 27, 21);
    }
  } else {
    const SafetyFaultInfo& f = safetyInfo(safetyKode);
    sprintf(buf, "!! %s !!", f.nama);
    teksTengah(buf, 0, 1);
    teksTengahPecah(safetyDetail[0] ? safetyDetail : f.pesan, 9, 21);
    if (evakuasiAktif) {
      sprintf(buf, "Drain %d ml / %ds", evakuasiMl, evakuasiSisaDetik());
      teksTengah(buf, 27, 1);
      teksTengah("Tunggu drain selesai", 36, 1);
    } else {
      teksTengahPecah(safetyAksi[0] ? safetyAksi : "ENTER : lanjut", 27, 21);
    }
  }
}

void gambarPemulihan() {
  oled.fillRect(0, 0, SCREEN_WIDTH, AREA_BAWAH, BLACK);
  oled.drawRect(0, 0, SCREEN_WIDTH, AREA_BAWAH, WHITE);
  oled.setTextColor(WHITE);
  oled.setTextSize(1);
  teksTengah("Proses terputus", 0, 1);
  char buf[24];
  sprintf(buf, "S%d %s", pemulihanSiklus,
          (pemulihanFase >= 0 && pemulihanFase <= FASE_SELESAI) ? namaFase[pemulihanFase] : "?");
  teksTengah(buf, 8, 1);

  const char* opsi[3] = { "Drain & bilas", "Drain saja", "Abaikan (stop)" };
  for (int i = 0; i < 3; i++) {
    sprintf(buf, "%s%s", (konfirmasiSel == i) ? "> " : "  ", opsi[i]);
    oled.setCursor(4, 15 + i * 9);
    oled.print(buf);
  }
  teksTengah("ATAS/BAWAH, ENTER", 41, 1);
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
    case HAL_KALIB_ESTOP:      gambarKalibEstop();      break;
    case HAL_TEST_MOTOR:       gambarTestMotor();       break;
    case HAL_PENGATURAN:       gambarPengaturan();      break;
    case HAL_TANK:             gambarTank();            break;
    case HAL_IOT_STATUS:       gambarIotStatus();       break;
    case HAL_INFO:             gambarInfo();            break;
    default: break;
  }

  // Overlay berprioritas: keselamatan > pemulihan > dialog proses > toast.
  // Keselamatan harus selalu menang agar E-STOP tidak pernah tertutup layar lain.
  if (safetyState != SAF_AMAN) {
    gambarSafety();
  } else if (pemulihanTersedia) {
    gambarPemulihan();
  } else {
    bool dialogTampil = false;
    if (halaman == HAL_PROSES && !prosesJeda && dialogBatalMinta) {
      gambarDialog("Batalkan proses?", "ENTER : Ya", "BACK  : Tidak");
      dialogTampil = true;
    } else if (halaman == HAL_PROSES && !prosesJeda && dialogBatal) {
      gambarDialog("Drain wadah?", "ENTER : Ya", "BACK  : Kosong");
      dialogTampil = true;
    }
    if (dialogCfg == DLG_SIMPAN) {
      gambarDialog("Simpan perubahan?", "ENTER : Simpan", "BACK  : Buang");
      dialogTampil = true;
    }
    if (dialogCfg == DLG_HAPUS) {
      gambarDialog("Hapus preset?", "ENTER : Ya", "BACK  : Tidak");
      dialogTampil = true;
    }
    // Layar jeda adalah halaman penuh; jangan timpa dengan toast.
    bool layarPenuh = (halaman == HAL_PROSES && prosesJeda);
    if (toastAktif && !dialogTampil && !layarPenuh) gambarToast();
  }

  geserGambar(UI_Y_OFFSET);
  oled.display();
}
