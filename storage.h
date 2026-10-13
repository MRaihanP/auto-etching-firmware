#pragma once
#include <Preferences.h>
#include "config.h"
#include "data.h"
#include "presetops.h"

// Semua preset disimpan sebagai SATU blob (satu key) + versi + checksum.
// Satu tulisan = lebih aman kalau daya putus saat menyimpan.
#define FORMAT_VERSI 1

struct DataSimpan {
  uint32_t versi;
  uint32_t jumlah;
  Preset   p[MAKS_PRESET];
  uint32_t crc;          // harus field terakhir
};

Preferences prefs;

uint32_t hitungCrc(const uint8_t* d, size_t n) {   // FNV-1a 32-bit
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < n; i++) { h ^= d[i]; h *= 16777619UL; }
  return h;
}

bool simpanPreset() {
  DataSimpan d;
  memset(&d, 0, sizeof(d));
  d.versi  = FORMAT_VERSI;
  d.jumlah = jumlahPreset;
  for (int i = 0; i < jumlahPreset; i++) d.p[i] = daftarPreset[i];
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("cfg", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.print(F("[STORE] Simpan "));
  dbg.print(jumlahPreset);
  dbg.println(ok ? F(" preset: OK") : F(" preset: GAGAL"));
  return ok;
}

void muatPreset() {
  DataSimpan d;
  bool ada = false, valid = false;

  if (prefs.begin(PREF_NS, false)) {
    size_t len = prefs.getBytesLength("cfg");
    ada = (len > 0);
    if (len == sizeof(d)) {
      prefs.getBytes("cfg", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc))) &&
              (d.jumlah <= MAKS_PRESET);
      for (uint32_t i = 0; valid && i < d.jumlah; i++)
        if (!validasiPreset(d.p[i])) valid = false;
    }
    prefs.end();
  }

  if (valid) {
    jumlahPreset = d.jumlah;
    for (int i = 0; i < jumlahPreset; i++) daftarPreset[i] = d.p[i];
    dbg.print(F("[STORE] Dimuat "));
    dbg.print(jumlahPreset);
    dbg.println(F(" preset"));
  } else {
    resetKeDefault();
    dbg.println(ada ? F("[STORE] Data korup/versi beda -> default")
                       : F("[STORE] Data belum ada -> default"));
    simpanPreset();
  }
}

// ================= AMBANG SENSOR LEVEL =================
#define FORMAT_VERSI_AMBANG 1

struct DataAmbang {
  uint32_t versi;
  int32_t  ambang[JUMLAH_SENSOR];
  uint32_t crc;          // harus field terakhir
};

bool simpanAmbang() {
  DataAmbang d;
  memset(&d, 0, sizeof(d));
  d.versi = FORMAT_VERSI_AMBANG;
  for (int i = 0; i < JUMLAH_SENSOR; i++) d.ambang[i] = ambang[i];
  d.crc = hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc));

  bool ok = false;
  if (prefs.begin(PREF_NS, false)) {
    ok = (prefs.putBytes("amb", &d, sizeof(d)) == sizeof(d));
    prefs.end();
  }
  dbg.println(ok ? F("[STORE] Simpan ambang: OK") : F("[STORE] Simpan ambang: GAGAL"));
  return ok;
}

void muatAmbang() {
  DataAmbang d;
  bool valid = false;
  if (prefs.begin(PREF_NS, false)) {
    if (prefs.getBytesLength("amb") == sizeof(d)) {
      prefs.getBytes("amb", &d, sizeof(d));
      valid = (d.versi == FORMAT_VERSI_AMBANG) &&
              (d.crc == hitungCrc((const uint8_t*)&d, sizeof(d) - sizeof(d.crc)));
      for (int i = 0; valid && i < JUMLAH_SENSOR; i++)
        if (d.ambang[i] < 0 || d.ambang[i] > 1023) valid = false;
    }
    prefs.end();
  }
  for (int i = 0; i < JUMLAH_SENSOR; i++) ambang[i] = valid ? d.ambang[i] : 0;
  dbg.println(valid ? F("[STORE] Ambang dimuat") : F("[STORE] Ambang belum ada (0 = belum diatur)"));
}
