# Dokumentasi Firmware v1.0.0

Dokumen ini merangkum cara pakai, prosedur kalibrasi, daftar error, dan
parameter default firmware **v1.0.0** untuk mesin etching PCB otomatis
berbasis ESP32-S3. Untuk riwayat perubahan lihat [`LOG.md`](./LOG.md) dan
[`RELEASES.md`](./RELEASES.md).

## 1. Build dan flash

```bash
~/DevBox/bin/arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" .
```

> `CDCOnBoot=cdc` **wajib**. Tanpa opsi itu, `dbg.h` gagal compile karena
> `USBSerial` tidak tersedia pada varian board default.

Upload memakai Arduino IDE / `arduino-cli upload` dengan board
**ESP32S3 Dev Module**, upload speed 921600 (turunkan ke 115200 bila gagal).
Data preset, kalibrasi, pengaturan, dan pelacak tangki disimpan di NVS dan
**tidak hilang** saat flash ulang.

## 2. Menu utama

| Menu | Fungsi |
|---|---|
| **Pilih Preset** | Daftar preset, mulai proses, jeda/lanjut, batal |
| **Konfigurasi Preset** | Tambah, duplikat, hapus, edit 14 field preset |
| **Level Cairan** | Lihat status 5 sensor dan kalibrasi 0%/100% |
| **Kalibrasi** | Motor (stepper/DC/agitasi), level, suhu, udara |
| **Pengaturan** | Ambang suhu, margin drain, kestabilan level, MQ-135, IoT |
| **Isi Ulang Tangki** | Isi volume awal HCl, H2O2, H2O, sabun, limbah |
| **Info/Status** | Versi, suhu 3 titik, status level, MQ-135, alasan reset |

Tombol:

- **ATAS / BAWAH** — navigasi daftar atau baris.
- **KIRI / KANAN** — ubah nilai numerik.
- **ENTER** — pilih / konfirmasi / ubah nilai ON-OFF.
- **BACK** — kembali satu halaman. **Tahan BACK ≥ 1 detik** kembali ke menu
  utama (tidak berlaku saat proses berjalan, layar error/E-stop, atau dialog
  pemulihan).

## 3. Alur proses

1. Pilih preset → **ENTER** untuk mulai.
2. **PRECHECK** memeriksa kalibrasi pompa, sensor wajib, level sumber, level
   limbah, sisa tangki, dan kapasitas tangki limbah.
3. **WAIT_PCB** — pasang PCB, lalu **ENTER**.
4. Tiap siklus: dosing air → H2O2 → HCl (satu pompa pada satu waktu) →
   agitasi sesuai `agitasiDetik` → drain.
5. Setelah siklus terakhir: bilas sabun (`bilasSabun` kali), lalu bilas air
   (`bilasAir` kali). Tiap bilas memakai `bilasMl` dan agitasi `bilasDetik`.
6. Bila `konfirmasiSiklus` aktif, layar konfirmasi muncul di akhir **setiap**
   siklus, termasuk siklus terakhir:
   - **Selesai & bilas** — langsung ke tahap bilas.
   - **Ulangi** — mengulang siklus terakhir.
7. **SELESAI** — tekan **ENTER/BACK** untuk keluar; wadah sudah kosong.

Saat proses berjalan:

- **ENTER** — jeda / lanjut. Jeda menghentikan stepper, DC, dan agitasi;
  lanjut meneruskan sisa langkah/waktu.
- **BACK** — dialog batal. Setelah batal, bila wadah masih berisi, muncul
  dialog **“Drain wadah?”** (ENTER = drain, BACK = lewati).
- **E-stop** — semua aktuator langsung mati dan proses dibatalkan.

Proses baru **ditolak** bila `volumeWadah > 0`; drain dulu lewat dialog yang
muncul saat mencoba mulai.

## 4. Kalibrasi

Kalibrasi disimpan di NVS dan dipakai langsung oleh dosing serta drain.
Lakukan kalibrasi **sebelum** menjalankan proses dengan larutan asli.

### 4.1 Pompa stepper (HCl, H2O2, air)

1. **Kalibrasi → Motor → Pompa HCl/H2O2/Air**.
2. Isi **Target** (ml), misalnya 20 ml untuk titik pertama.
3. **ENTER** pada START untuk menjalankan dosing memakai profil yang sama
   dengan proses nyata (ramp naik → jalan → ramp turun).
4. Ukur volume nyata dengan gelas ukur, lalu isi **Hasil**.
5. **ENTER** pada baris Hasil menambah titik uji `(langkah, ml)`.
6. Ulangi dengan volume berbeda (misalnya 120 ml). Model
   **`langkah = a·ml + b`** dihitung dari minimal dua titik dengan volume
   berbeda.
7. Baris **Uji hasil** menjalankan verifikasi memakai model tersimpan;
   **LULUS** bila galat ≤ 5%, **GAGAL** bila lebih. Bila GAGAL, **ENTER**
   pada Hasil menambahkan titik koreksi lalu uji ulang.
8. **Reset titik** menghapus semua titik dan mengembalikan model ke nol.

Bila pompa belum pernah dikalibrasi, firmware memakai nilai nominal
2,0 ml/rotasi agar uji pertama tetap bisa dijalankan.

### 4.2 Pompa DC (sabun dan limbah)

1. **Kalibrasi → Motor → Pompa Sabun / Pompa Limbah**.
2. Isi **Target** (ml) dan pilih duty.
3. **ENTER** pada START; pompa berjalan selama durasi yang dihitung.
4. Ukur hasil nyata, isi **Hasil**, lalu **ENTER** untuk menambah titik
   `(detik, ml)`.
5. Model **`ml = r·detik + c`** dipakai dosing dan perhitungan drain
   (termasuk margin 10% untuk dosing dan 25% untuk drain).
6. Ulangi pada durasi/volume berbeda untuk memperkuat model.

Pompa sabun dan pompa limbah **wajib terkalibrasi** agar proses dapat
berjalan. Bila kalibrasi limbah belum ada, drain ditolak dengan **E09**,
bukan dianggap selesai.

### 4.3 Agitasi

**Kalibrasi → Motor → Motor Agitasi** menyimpan duty SPD1/SPD2/SPD3.
Default: 40% / 70% / 100%. Preset memilih salah satu level ini.

### 4.4 Sensor level

**Kalibrasi → Sensor Level** (atau menu **Level Cairan**): untuk tiap sensor,
simpan nilai **0%** saat kering dan **100%** saat terendam. Status proses
memakai histeresis:

- basah bila ≥ **55%**
- kering bila ≤ **45%**
- nilai di luar rentang wajar → **FAULT** (E03)

### 4.5 Sensor suhu

**Kalibrasi → Sensor Suhu**: pemetaan blok ↔ alamat ROM dan offset
disimpan di NVS (`kal2`). Sensor yang lepas/rusak berstatus FAULT. Sensor
chamber wajib valid sebelum proses dimulai.

### 4.6 Sensor udara MQ-135

**Kalibrasi → Sensor Udara** menyimpan baseline “normal/kotor”. Sensor hanya
dipakai bila `mqAktif` = ON di **Pengaturan**.

## 5. Parameter default

| Parameter | Nilai default | Lokasi |
|---|---|---|
| Rasio kimia | HCl 33% : H2O2 17% : H2O 50% | preset |
| Volume preset baru | 360 ml | preset |
| Lama etching | 300 s per siklus | preset |
| Speed agitasi | SPD3 (100%) | preset |
| Jumlah siklus | 3 | preset |
| Bilas sabun / air | 1 / 2 kali | preset |
| Volume tiap bilas | 100 ml | preset |
| Agitasi tiap bilas | 20 s | preset |
| Konfirmasi siklus | OFF | preset |
| Batas volume preset | 50–1000 ml | `VOL_MIN`/`VOL_MAX` |
| RPM pompa stepper | 500 (rentang 400–700) | `config.h` |
| Duty agitasi SPD1/2/3 | 40 / 70 / 100% | `config.h` |
| Suhu peringatan / bahaya | 45 °C / 60 °C | `Pengaturan` |
| Margin drain | 25% + 3 s | `DRAIN_MARGIN_PCT` |
| Kapasitas tangki limbah | 5000 ml | `TANGKI_MAKS_ML` |
| Peringatan tangki | 100 ml | `TANK_PERINGATAN_ML` |
| Watchdog loop | 15 s | `WDT_TIMEOUT_S` |
| Toleransi verifikasi | stepper 5%, DC 10% | `config.h` |

Nilai di **Pengaturan** dapat diubah dari layar. Nilai rasio kimia dan field
preset lain diubah lewat **Konfigurasi Preset**.

## 6. Pelacak tangki

Menu **Isi Ulang Tangki** menyimpan volume awal:

- HCl, H2O2, H2O, air sabun, dan isi tangki limbah (ml).

Setiap dosing/isi mengurangi sisa tangki; setiap drain menambah volume
limbah. **PRECHECK** membandingkan sisa tangki dengan kebutuhan **seluruh**
proses (semua siklus + bilas). Bila sisa kurang → **E05**; bila limbah tidak
cukup menampung → **E06**. Saat sisa mendekati batas, layar proses
menampilkan peringatan **TANK!**.

## 7. Pemulihan setelah listrik padam

Status proses ditulis ke NVS `runstate` saat transisi langkah (bukan tiap
detik). Bila alat mati di tengah proses, boot berikutnya menampilkan pilihan:

- **Drain & bilas** — drain isi wadah, satu bilas air + agitasi singkat,
  lalu drain lagi.
- **Abaikan** — tidak menjalankan aktuator, tetapi volume sisa tetap
  dilacak. Proses baru akan meminta drain lebih dulu.

Tidak ada aktuator yang menyala otomatis setelah boot. Bila proses terputus
dan pilihan belum diambil, alat menunggu keputusan pengguna.

## 8. Daftar error

| Kode | Nama | Arti | Tindakan |
|---|---|---|---|
| **E01** | ESTOP | Tombol darurat ditekan | Lepas tombol, tekan ENTER untuk clear |
| **E02** | TEMP_HIGH | Suhu melewati batas bahaya | Dinginkan, cek sensor/kipas, clear |
| **E03** | SENSOR_FAIL | Sensor wajib gagal/FAULT | Cek wiring dan kalibrasi sensor |
| **E04** | TIMEOUT | Langkah melebihi batas waktu | Cek pompa/selang, clear, ulangi |
| **E05** | TANK_EMPTY | Tangki kosong atau sisa kurang | Isi ulang tangki, clear |
| **E06** | WASTE_FULL | Tangki limbah penuh | Kosongkan limbah, clear |
| **E07** | NVS_CORRUPT | Data penyimpanan rusak | Data korup ditolak; reset/isi ulang data |
| **E08** | RESUME_NEEDED | Proses terputus, butuh keputusan | Pilih Drain & bilas / Abaikan |
| **E09** | CAL_MISSING | Kalibrasi pompa belum lengkap | Kalibrasi pompa yang ditandai |
| **E10** | WDT | Watchdog memicu reset | Cek log, beban loop, lalu clear |

Saat error aktif, semua tombol lain diblok dan aktuator mati. **ENTER**
membersihkan error (E-stop hanya bisa dibersihkan setelah tombol fisik
dilepas). Buzzer berbunyi pola error saat fault terjadi.

## 9. Catatan NVS

| Key | Isi |
|---|---|
| `cfg` | Daftar preset (v2, dengan checksum) |
| `kal` | Kalibrasi pompa (v4: model stepper + DC) |
| `pompa` | Data pompa lama (migrasi otomatis) |
| `kal2` | Sensor suhu, level, dan MQ-135 |
| `set` | Pengaturan global |
| `tank` | Sisa volume tangki |
| `runstate` | Status proses untuk pemulihan |

Migrasi dari v0.6.6 berjalan otomatis saat boot pertama v1.0.0. Preset v1
yang memiliki `agitasiDetik < 30` dinaikkan ke 300 s dan dicatat di log.

## 10. Pengujian sebelum dipakai

Checklist minimum di bench (tanpa larutan asli):

- [ ] Boot normal, versi v1.0.0 tercetak, tidak ada reset berulang.
- [ ] Semua menu terbuka; preset tambah/edit/hapus bertahan setelah restart.
- [ ] Kalibrasi stepper dan DC menghasilkan model dan lulus verifikasi.
- [ ] Test Motor: tiap motor dan “Semua” start/stop dengan ramp.
- [ ] Proses dengan air: mulai, jeda, lanjut, batal, drain, selesai.
- [ ] E-stop saat proses dan saat Test Motor.
- [ ] Cabut listrik di tengah proses → layar pemulihan muncul di boot.
- [ ] Tangki habis / limbah penuh / suhu tinggi memunculkan error yang benar.

Dengan larutan asli (volume kecil): pastikan exhaust menyala, gunakan APD,
dan periksa korosi/kebocoran konektor setelah pengujian.
