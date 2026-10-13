# Plan: Non-Contact Water Level Sensor via Nickel Strip Capacitive Sensing

## Tujuan
Membaca level cairan (persen/ml) dari 5 tabung HDPE secara non-contact menggunakan strip nikel sebagai electrode capacitive sensor.

## Spesifikasi Hardware
- **Jerican:** HDPE, tinggi 18.4 cm
- **Strip nikel:** 16 cm panjang × 0.8 cm lebar, 1 strip per jerican, ditempel di luar (agak naik dari dasar)
- **Kabel:** AWG30/32 (sangat tipis)
- **MCU:** ESP32-S3 DevKitC
- **LED RGB:** GPIO48 (WS2812/NeoPixel)

## Pin Mapping
| Tabung | GPIO | Warna LED |
|--------|------|-----------|
| HCl | 11 | Merah |
| H2O2 | 12 | Ungu |
| Air | 13 | Biru |
| Air Sabun | 14 | Hijau |
| Air Limbah | 3 | Putih |

## Prinsip Kerja
Strip nikel ditempel di luar jerican HDPE (~1mm). Cairan di dalam jerican mengubah kapasitansi strip terhadap ground. ESP32-S3 memiliki peripheral touch sensor internal yang bisa mendeteksi perubahan kapasitansi ini.

## Tahapan Pengujian

### Phase 1: Pilot Sketch — Touch Detection + LED Indicator
**Tujuan:** Memastikan strip nikel bisa mendeteksi sentuhan/cairan, dengan indikator LED RGB.

**Langkah:**
1. Buat sketch `pilot_touch_sensor.ino`:
   - Inisialisasi LED RGB di GPIO48 (WS2812)
   - Baca `touchRead()` untuk semua 5 GPIO secara bergantian
   - Print raw value ke Serial Plotter
   - Set threshold sementara (akan dikalibrasi nanti)
   - Jika touch < threshold → LED menyala dengan warna sesuai sensor
   - Jika touch > threshold → LED mati
2. Uji dengan sentuhan langsung ke strip
3. Uji dengan strip di jerican kosong vs terisi air

**Kriteria lolos:**
- Semua 5 strip bisa mendeteksi sentuhan/cairan
- LED RGB menyala dengan warna yang benar untuk setiap sensor
- Tidak ada crosstalk antar sensor

### Phase 2: Pilot Sketch — Kalibrasi 0% dan 100%
**Tujuan:** Mapping nilai touch → persentase level cairan.

**Langkah:**
1. Untuk setiap sensor:
   - Baca nilai touch saat jerican kosong (0%)
   - Baca nilai touch saat jerican penuh (100%)
   - Simpan kedua nilai ini sebagai baseline
2. Hitung persen: `level% = (raw - rawKosong) / (rawPenuh - rawKosong) * 100`
3. Tampilkan di Serial Monitor: `Sensor: 75%`
4. Uji repeatability: isi → kosong → isi lagi

**Kriteria lolos:**
- Setiap level bisa dibedakan dengan jelas
- Repeatability error < 5%

### Phase 3: Pilot Sketch — Semua Sensor + Kabel Panjang
**Tujuan:** Verifikasi 5 strip bisa dibaca bersamaan dengan kabel AWG30/32 ~30cm.

**Langkah:**
1. Hubungkan semua 5 strip dengan kabel tipis
2. Baca semua sensor secara bergantian
3. Cek tidak ada crosstalk antar sensor
4. Tambah moving average filter untuk reduksi noise

**Kriteria lolos:**
- Semua sensor bisa dibaca stabil
- Noise < 5% dari range

### Phase 4: Integrasi ke Firmware Utama
**Tujuan:** Integrasi sensor level ke firmware utama.

**Langkah:**
1. Update `sensor.h`:
   - Ganti implementasi sensor level saat ini dengan touch sensor
   - Fungsi `sensorInit()` → inisialisasi touch pin
   - Fungsi `sensorUpdate()` → baca semua 5 sensor, update `sensorRaw[]`
   - Fungsi `sensorAktif[]` → tentukan threshold untuk "cukup" / "kurang"
2. Update `config.h`:
   - Pastikan pin mapping sudah benar
   - Tambah threshold default untuk setiap sensor
3. Update `ui.h`:
   - Halaman Level Cairan sudah ada, pastikan tampilannya sesuai
   - Tambah menu kalibrasi 0% dan 100%
4. Update `storage.h`:
   - Simpan threshold kalibrasi ke NVS

**Kriteria lolos:**
- Semua menu yang melibatkan sensor level berfungsi
- Kalibrasi level bisa disimpan dan dimuat dari NVS

### Phase 5: Final — Output dalam Persen/ml
**Tujuan:** Menampilkan level dalam persen atau ml.

**Langkah:**
1. Tambah fungsi konversi touch value → persen (dari kalibrasi)
2. Tampilkan di UI: `Level: 75%` atau `Level: 375 ml`
3. Tambah alarm/peringatan jika level cairan kurang

## Risiko & Mitigasi
| Risiko | Mitigasi |
|--------|----------|
| Noise dari kabel tipis AWG30/32 | Moving average filter di software |
| HDPE terlalu tebal (>1mm) | Uji dulu, jika tidak sensitif bisa pakai strip lebih lebar |
| Cairan korosif merusak strip | Strip hanya di luar jerican, tidak kontak langsung |
| Interferensi antar sensor | Baca sensor secara bergantian, jangan paralel |
| Baseline drift | Auto-calibration saat boot, atau tombol kalibrasi manual |

## File yang Dibuat/Dimodifikasi
- `pilot_touch_sensor.ino` — sketch pilot (Phase 1-3)
- `sensor.h` — update implementasi sensor level
- `config.h` — pastikan pin mapping
- `ui.h` — tampilan level cairan + menu kalibrasi
- `storage.h` — simpan kalibrasi
