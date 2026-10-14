#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "data.h"
#include "process.h"
#include "presetops.h"
#include "sensor.h"
#include "sensor_extra.h"
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
#define L_KALIB   4
#define L_KALIB_SENSOR 5
#define L_KALIB_POMPA   6
#define L_KALIB_DC      7
#define JUMLAH_LIST 8

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
  gambarHeader(placeholderJudul[0] ? placeholderJudul : menuUtama[lsSel[L_UTAMA]]);
  teksTengah("Halaman ini", 26, 1);
  teksTengah("belum tersedia", 36, 1);
  oled.setTextSize(1);
  teksTengah("[BACK] kembali", 52, 1);
}

// ================= HALAMAN: MENU KALIBRASI =================
void gambarKalibrasiList() {
  gambarHeader("Kalibrasi");
  gambarList(menuKalibrasi, JUMLAH_MENU_KALIB, L_KALIB, HEADER_H, 16);
}

// Pilih tangki yang mau dikalibrasi (gaya list menu lain: terpilih di tengah)
void gambarKalibSensorList() {
  gambarHeader("Sensor Level");
  char teks[JUMLAH_SENSOR][20];
  const char* item[JUMLAH_SENSOR];
  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    if (!sensorKalibValid[i])
      snprintf(teks[i], sizeof(teks[i]), "%-6s uncal", namaSensor[i]);
    else if (sensorKapasitasMl[i] > 0)
      snprintf(teks[i], sizeof(teks[i]), "%-6s %d%% %dml",
               namaSensor[i], constrain(sensorPersen[i], 0, 100), sensorMl[i]);
    else
      snprintf(teks[i], sizeof(teks[i]), "%-6s %d%%",
               namaSensor[i], constrain(sensorPersen[i], 0, 100));
    item[i] = teks[i];
  }
  gambarList(item, JUMLAH_SENSOR, L_KALIB_SENSOR, HEADER_H, 16);
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
// Satu baris per tangki: nama (kiri) + persentase (kanan) sejajar, lalu bar
// progress di bawahnya. Baris terpilih dibalik (latar putih, teks hitam).
// Tinggi baris 12px = teks 8 + bar 3 + jeda 1. Jeda 1px itu wajib: tanpa jeda,
// pita putih baris terpilih menempel di bar milik baris tetangga dan saat
// halaman di-scroll tampak seperti "bar tertutup putih".
// Bar 3px (border 1 + isi 1) masih terbaca dan sama dengan bar di halaman lain.
const int LEVEL_TEKS_H = 8;
const int LEVEL_BAR_H  = 3;
const int LEVEL_JEDA   = 1;                              // jeda setelah bar
const int BARIS_LVL    = LEVEL_TEKS_H + LEVEL_BAR_H + LEVEL_JEDA;   // 12px
const int LEVEL_BAR_X  = 2;
const int LEVEL_BAR_W  = SCREEN_WIDTH - 8;               // x=2..121 (scrollbar di 126)
// Pita baris terpilih harus menutup TEKS + BAR (otherwise bar Selected berwarna
// hitam di atas background hitam -> tidak kelihatan saat scroll) dan lebarnya
// harus sampai x=122 supaya seluruh bar berada di atas pita.
const int LEVEL_SEL_W  = SCREEN_WIDTH - 5;               // x=0..122

int levelScrollPx = 0;

int levelBarisTampil() {
  return max(1, (AREA_BAWAH - HEADER_H) / BARIS_LVL);
}

int levelMaksScroll() {
  return max(0, (JUMLAH_SENSOR - levelBarisTampil()) * BARIS_LVL);
}

// Barsihkan scroll supaya baris terpilih selalu utuh di layar
void levelScrollKe() {
  int tampil  = levelBarisTampil();
  int maks    = levelMaksScroll();
  int barisAtas = levelScrollPx / BARIS_LVL;
  if (levelSel < barisAtas)              levelScrollPx = levelSel * BARIS_LVL;
  if (levelSel >= barisAtas + tampil)    levelScrollPx = (levelSel - tampil + 1) * BARIS_LVL;
  if (levelScrollPx > maks) levelScrollPx = maks;
  if (levelScrollPx < 0)   levelScrollPx = 0;
}

// Level Cairan = READ-ONLY: monitoring % + ml per tangki.
// Pengaturan ada di Kalibrasi -> Sensor Level (ATAS/BAWAH pilih, KIRI/KANAN
// ubah, ENTER jalankan aksi, BACK + dialog simpan).
void gambarLevel() {
  gambarHeader("Level Cairan");

  if (!sensorOk) {
    teksTengah("Sensor tidak", 22, 1);
    teksTengah("aktif", 32, 1);
    teksTengah("[BACK] kembali", 52, 1);
    return;
  }

  levelScrollKe();

  for (int i = 0; i < JUMLAH_SENSOR; i++) {
    int y = HEADER_H + i * BARIS_LVL - levelScrollPx;

    // Culling: hanya baris yang utuh (baris potong di tepi layar dilewati)
    if (y + LEVEL_TEKS_H <= HEADER_H) continue;
    if (y + BARIS_LVL > AREA_BAWAH) continue;

    bool valid  = sensorKalibValid[i];
    bool adaMl  = valid && sensorKapasitasMl[i] > 0;
    int  persen = constrain(sensorPersen[i], 0, 100);
    bool sel    = (i == levelSel);

    // --- Baris teks: nama kiri, persen + ml kanan (sejajar) ---
    // Pita putih menutup teks + bar (tinggi penuh 1 baris)
    if (sel) oled.fillRect(0, y, LEVEL_SEL_W, BARIS_LVL, WHITE);
    oled.setTextSize(1);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(LEVEL_BAR_X, y);
    oled.print(namaSensor[i]);

    char buf[18];
    if (!valid) {
      strcpy(buf, "uncal");
    } else if (adaMl) {
      snprintf(buf, sizeof(buf), "%d%% %d ml", persen, sensorMl[i]);
    } else {
      snprintf(buf, sizeof(buf), "%d%%", persen);
    }
    teksKanan(buf, SCREEN_WIDTH - 6, y);
    oled.setTextColor(WHITE);

    // --- Bar progress di bawah teks, sisa 1px baris jadi jeda ke baris berikutnya ---
    int by = y + LEVEL_TEKS_H;
    uint16_t warna = sel ? BLACK : WHITE;
    oled.drawRect(LEVEL_BAR_X, by, LEVEL_BAR_W, LEVEL_BAR_H, warna);
    if (valid && persen > 0) {
      int isi = (LEVEL_BAR_W - 2) * persen / 100;
      if (isi > 0) oled.fillRect(LEVEL_BAR_X + 1, by + 1, isi, LEVEL_BAR_H - 2, warna);
    }
  }

  // Scrollbar tipis di kanan (hanya kalau masih ada yang tersembunyi)
  int maks = levelMaksScroll();
  if (maks > 0) {
    int trackY = HEADER_H, trackH = AREA_BAWAH - trackY;
    int thumbH = max(4, trackH * levelBarisTampil() / JUMLAH_SENSOR);
    int thumbY = trackY + (trackH - thumbH) * levelScrollPx / maks;
    oled.fillRect(SCREEN_WIDTH - 2, thumbY, 2, thumbH, WHITE);
  }
}

// Baris nilai generik untuk halaman kalibrasi: label kiri, nilai rata kanan.
// Pita putih baris terpilih setinggi 8px (tanpa bar progress di bawahnya).
void barisKalib(int y, const char* label, const char* nilai, bool sel) {
  if (sel) oled.fillRect(0, y, LEVEL_SEL_W, 8, WHITE);
  oled.setTextSize(1);
  oled.setTextColor(sel ? BLACK : WHITE);
  oled.setCursor(2, y);
  oled.print(label);
  teksKanan(nilai, SCREEN_WIDTH - 6, y);
  oled.setTextColor(WHITE);
}

// Footer petunjuk tombol di bawah daftar baris
void footerKalib(int jumlahBaris, const char* teks) {
  teksTengah(teks, HEADER_H + jumlahBaris * 8 + 1, 1);
}

// ================= HALAMAN: KALIBRASI POMPA STEPPER =================
// Dipakai item Motor HCl / H2O2 / Air (satu halaman untuk 3 pompa)
void gambarPompaList() {
  gambarHeader("Pompa");
  char teks[3][20];
  const char* item[3];
  for (int i = 0; i < 3; i++) {
    snprintf(teks[i], sizeof(teks[i]), "%-5s %ld l/ml",
             namaPompaStepper[i], langkahPerMlX1000[i] / 1000);
    item[i] = teks[i];
  }
  gambarList(item, 3, L_KALIB_POMPA, HEADER_H, 16);
}

void gambarKalibPompa() {
  gambarHeader(namaPompaStepper[kalibPompaIdx]);

  char buf[20];
  snprintf(buf, sizeof(buf), "%ld", langkahPerMlX1000[kalibPompaIdx] / 1000);
  barisKalib(HEADER_H, "Langkah/ml", buf, kalibPompaBaris == 0);
  barisKalib(HEADER_H + 8, "Status", "belum dites", false);

  footerKalib(2, "KIRI/KANAN ubah");
}

// ================= HALAMAN: KALIBRASI POMPA DC =================
// Dipakai item Motor Air Sabun / Motor Limbah
void gambarPompaDcList() {
  gambarHeader("Pompa DC");
  char teks[2][20];
  const char* item[2];
  for (int i = 0; i < 2; i++) {
    if (pompaDcMlPerDtkX10[i] > 0)
      snprintf(teks[i], sizeof(teks[i]), "%-9s %d.%d ml/s",
               namaPompaDC[i], pompaDcMlPerDtkX10[i] / 10, pompaDcMlPerDtkX10[i] % 10);
    else
      snprintf(teks[i], sizeof(teks[i]), "%-9s uncal", namaPompaDC[i]);
    item[i] = teks[i];
  }
  gambarList(item, 2, L_KALIB_DC, HEADER_H, 16);
}

void gambarKalibPompaDc() {
  gambarHeader(namaPompaDC[kalibDcIdx]);

  char buf[20];
  if (pompaDcMlPerDtkX10[kalibDcIdx] > 0)
    snprintf(buf, sizeof(buf), "%d.%d", pompaDcMlPerDtkX10[kalibDcIdx] / 10,
             pompaDcMlPerDtkX10[kalibDcIdx] % 10);
  else
    strcpy(buf, "-");
  barisKalib(HEADER_H, "ml per detik", buf, kalibDcBaris == 0);

  footerKalib(1, "KIRI/KANAN ubah");
}

// ================= HALAMAN: KALIBRASI MOTOR AGITASI =================
void gambarKalibAgitasi() {
  gambarHeader("Agitasi");

  char buf[16];
  snprintf(buf, sizeof(buf), "%d / 255", agitasiDutyMin);
  barisKalib(HEADER_H, "Duty minimum", buf, kalibAgitasiBaris == 0);
  barisKalib(HEADER_H + 8, "Status", kalibAgitasiNyala ? "NYALA" : "mati", false);

  footerKalib(2, "ENTER: nyala/mati");
}

// ================= HALAMAN: KALIBRASI EXHAUST FAN =================
void gambarKalibFan() {
  gambarHeader("Exhaust Fan");

  char buf[16];
  snprintf(buf, sizeof(buf), "%d / 255", fanPwmMin);
  barisKalib(HEADER_H, "PWM minimum", buf, kalibFanBaris == 0);
  barisKalib(HEADER_H + 8, "Status", kalibFanNyala ? "NYALA" : "mati", false);

  footerKalib(2, "ENTER: nyala/mati");
}

// ================= HALAMAN: KALIBRASI SENSOR UDARA (MQ-135) =================
void gambarKalibUdara() {
  gambarHeader("Sensor Udara");

  char buf[20];
  snprintf(buf, sizeof(buf), "%d", mq135Baseline);
  barisKalib(HEADER_H, "Baseline", buf, kalibUdaraBaris == 0);
  snprintf(buf, sizeof(buf), "%d", mq135Margin);
  barisKalib(HEADER_H + 8, "Margin", buf, kalibUdaraBaris == 1);
  snprintf(buf, sizeof(buf), "%d", udaraBaca());
  barisKalib(HEADER_H + 16, "Bacaan", buf, false);
  barisKalib(HEADER_H + 24, "Status",
             udaraBaca() < mq135Baseline + mq135Margin ? "NORMAL" : "TIDAK NORMAL", false);

  footerKalib(4, "ENTER: set baseline");
}

// ================= HALAMAN: KALIBRASI SENSOR SUHU (DS18B20) =================
void gambarKalibSuhu() {
  gambarHeader("Sensor Suhu");

  char buf[20];
  if (!dsTerdeteksi) {
    teksTengah("DS18B20 tidak", 20, 1);
    teksTengah("ditemukan", 30, 1);
    teksTengah("cek GPIO10", 40, 1);
    return;
  }

  float c = suhuBaca(2500);                  // blocking, hanya di halaman ini
  int   o = ds18b20OffsetX10;
  snprintf(buf, sizeof(buf), "%d.%d C", o / 10, abs(o) % 10);
  barisKalib(HEADER_H, "Offset", buf, kalibSuhuBaris == 0);
  snprintf(buf, sizeof(buf), "%.1f C", c);
  barisKalib(HEADER_H + 8, "Bacaan", buf, false);
  snprintf(buf, sizeof(buf), "%.1f C", c + o / 10.0);
  barisKalib(HEADER_H + 16, "Diterapkan", buf, false);

  footerKalib(3, "KIRI/KANAN ubah offset");
}

// ================= HALAMAN: KALIBRASI SENSOR LEVEL =================
// 3 baris nilai + footer petunjuk tombol (isi kontent 37px).
// Baris: 0 = Kapasitas (ml), 1 = Baseline 0%, 2 = Delta 100%
const int KAL_BARIS = 3;
const char* KAL_NAMA[KAL_BARIS] = { "Kapasitas", "Baseline 0%", "Delta 100%" };

// Petunjuk tombol sesuai baris yang dipilih (max 21 karakter)
const char* kalibPetunjuk(int baris) {
  switch (baris) {
    case 0:  return "KIRI/KANAN ubah ml";
    case 1:  return "ENTER: tandai 0%";
    default: return "ENTER: tandai 100%";
  }
}

void gambarKalibSensor() {
  int i = kalibSensorIdx;

  // Header: judul kiri, level sekarang (ml) kanan
  oled.fillRect(0, 0, SCREEN_WIDTH, HEADER_H, WHITE);
  oled.setTextColor(BLACK);
  oled.setTextSize(1);
  char judul[24];
  if (sensorKalibValid[i]) snprintf(judul, sizeof(judul), "Kalib %s", namaSensor[i]);
  else                      snprintf(judul, sizeof(judul), "Kalib %s *", namaSensor[i]);
  oled.setCursor(2, 2);
  oled.print(judul);
  if (kalibSampling) {
    char rb[14];
    snprintf(rb, sizeof(rb), "sampel %d%%", sensorKalibProgres());
    teksKanan(rb, SCREEN_WIDTH - 4, 2);
  } else if (sensorKalibValid[i]) {
    char rb[12];
    if (sensorKapasitasMl[i] > 0) snprintf(rb, sizeof(rb), "%dml", sensorMl[i]);
    else                          snprintf(rb, sizeof(rb), "%d%%", sensorPersen[i]);
    teksKanan(rb, SCREEN_WIDTH - 4, 2);
  }
  oled.setTextColor(WHITE);

  if (!sensorOk) {
    teksTengah("Sensor tidak aktif", 24, 1);
    teksTengah("[BACK] kembali", 40, 1);
    return;
  }

  char buf[20];
  const int step = 8;
  for (int b = 0; b < KAL_BARIS; b++) {
    int y = HEADER_H + b * step;
    bool sel = (b == kalibBaris) && !kalibSampling;

    if (sel) oled.fillRect(0, y, LEVEL_SEL_W, step, WHITE);
    oled.setTextColor(sel ? BLACK : WHITE);
    oled.setCursor(2, y);
    oled.print(KAL_NAMA[b]);

    switch (b) {
      case 0:
        if (sensorKapasitasMl[i] > 0) snprintf(buf, sizeof(buf), "%d ml", sensorKapasitasMl[i]);
        else                          strcpy(buf, "-");
        break;
      case 1:
        if (sensorKering[i] > 0) snprintf(buf, sizeof(buf), "%d", sensorKering[i]);
        else                     strcpy(buf, "-");
        break;
      default:
        if (sensorKalibValid[i]) snprintf(buf, sizeof(buf), "%d", sensorDeltaPenuh[i]);
        else                     strcpy(buf, "-");
        break;
    }
    teksKanan(buf, SCREEN_WIDTH - 6, y);
    oled.setTextColor(WHITE);
  }

  // Baris kaki: petunjuk tombol, atau progress bar saat sedang mengambil sampel
  int fy = HEADER_H + KAL_BARIS * step + 1;    // y=37, muat sampai 44
  if (kalibSampling) {
    int isi = (LEVEL_BAR_W - 2) * sensorKalibProgres() / 100;
    oled.drawRect(LEVEL_BAR_X, fy, LEVEL_BAR_W, LEVEL_BAR_H, WHITE);
    if (isi > 0) oled.fillRect(LEVEL_BAR_X + 1, fy + 1, isi, LEVEL_BAR_H - 2, WHITE);
  } else {
    teksTengah(kalibPetunjuk(kalibBaris), fy, 1);
  }
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
    case HAL_KALIB_LIST:    gambarKalibrasiList(); break;
    case HAL_KALIB_SENSOR_LIST: gambarKalibSensorList(); break;
    case HAL_KALIB_SENSOR:  gambarKalibSensor();  break;
    case HAL_KALIB_POMPA_LIST: gambarPompaList(); break;
    case HAL_KALIB_POMPA:  gambarKalibPompa();  break;
    case HAL_KALIB_DC_LIST: gambarPompaDcList(); break;
    case HAL_KALIB_DC:      gambarKalibPompaDc(); break;
    case HAL_KALIB_AGITASI: gambarKalibAgitasi(); break;
    case HAL_KALIB_FAN:     gambarKalibFan();    break;
    case HAL_KALIB_UDARA:   gambarKalibUdara();  break;
    case HAL_KALIB_SUHU:    gambarKalibSuhu();   break;
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
