# Dokumentasi Firmware v1.0.1

Dokumen ini merangkum cara pakai, prosedur kalibrasi, daftar error, dan
parameter default firmware **v1.0.1** untuk mesin etching PCB otomatis
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
4. Satu **siklus penuh** terdiri dari: dosing air → H2O2 → HCl (satu pompa
   pada satu waktu) → agitasi `agitasiDetik` → drain → bilas sabun
   (`bilasSabun` kali) → bilas air (`bilasAir` kali) → drain.
   Tiap bilas memakai `bilasMl` dan agitasi `bilasDetik`.
5. Siklus penuh diulang sebanyak `siklus` di preset. Jumlah bilas pada preset
   adalah **per siklus**, bukan total di akhir.
6. Bila `konfirmasiSiklus` aktif, layar konfirmasi muncul di akhir **setiap
   siklus penuh** (termasuk siklus terakhir):
   - **Siklus lagi** — menjalankan siklus penuh berikutnya.
   - **Selesai** — mengakhiri proses.
7. **SELESAI** — tekan **ENTER/BACK** untuk keluar; wadah sudah kosong.

Saat proses berjalan:

- **ENTER** — jeda / lanjut. Jeda menghentikan stepper, DC, dan agitasi;
  lanjut meneruskan sisa langkah/waktu.
- **BACK** — dialog batal. Setelah dikonfirmasi, semua motor lain dimatikan
  dan **pompa limbah langsung menyala** sampai cairan wadah tersedot. Tidak
  perlu memilih drain lagi.
- **E-stop** — semua motor mati seketika, lalu **pompa limbah menyala**
  sampai cairan tersedot (durasi memakai model limbah atau fallback waktu).
  Reset E-stop menunggu evakuasi selesai.

Proses baru **ditolak** bila `volumeWadah > 0`; drain dulu lewat dialog yang
muncul saat mencoba mulai.

## 4. Kalibrasi

Kalibrasi disimpan di NVS dan dipakai langsung oleh dosing serta drain.
Lakukan kalibrasi **sebelum** menjalankan proses dengan larutan asli.

### 4.1 Pompa stepper (HCl, H2O2, air)

Alur ini memakai **target tetap 100 ml**. Target tidak bisa diedit.

1. **Kalibrasi → Motor → Pompa HCl/H2O2/Air**.
2. Isi jeriken dengan air/cairan, masukkan selang input ke jeriken.
   Siapkan **wadah buang** untuk priming dan **gelas takar kosong/kering**
   untuk pengukuran.
3. Pilih baris **Priming**, tekan **ENTER**. Setelah cairan mulai keluar,
   **biarkan pompa jalan 10–15 detik lagi** ke wadah buang. Durasi priming
   tampil di tombol START (misalnya `PRIME 12s`). Tekan **ENTER** untuk
   berhenti.
4. Pindahkan selang output ke gelas takar. Tunggu sampai tetesan terakhir
   berhenti. Gelas harus benar-benar kosong dan kering.
5. Pilih **START 100 ml**, tekan **ENTER**. Pompa memakai model saat ini:
   - sudah dikalibrasi → memakai `a`/`b` tersimpan;
   - belum dikalibrasi → memakai base **6400 langkah/ml** (0,5 ml/rotasi).
6. Setelah motor berhenti, tunggu tetesan terakhir, lalu baca volume di
   gelas takar. Baris **Hasil** mulai dari `--`:
   - tekan **ENTER** untuk mengisi cepat **100 ml**, lalu sesuaikan dengan
     **KIRI/KANAN**; atau
   - langsung **KIRI/KANAN** dari 0 untuk hasil di bawah 100 ml.
   Resolusi default **1 ml**; **ENTER** pada baris **Koreksi** mengganti
   resolusi **0,1 / 1 / 5 ml** (tampil sebagai `Hasil x1`, `x0.1`, `x5`).
7. Tekan **ENTER lagi** pada baris **Hasil**. Firmware menghitung koreksi,
   menyimpan, dan langsung menerapkannya ke pompa tersebut. Tombol START
   berubah menjadi **“Gelas kosong?”** — kosongkan dan keringkan gelas,
   lalu tekan **ENTER** dua kali (konfirmasi, lalu mulai run berikutnya).
8. **Kosongkan dan keringkan gelas takar** sebelum run berikutnya, lalu
   ulangi langkah 5–7 minimal **3 kali**. Baris **Run** menampilkan
   `0/3` … `3/3 LULUS` atau `3/3 ULANGI`. Jika aliran tidak keluar atau
   ada gelembung udara, ulangi **Priming** ke wadah buang sebelum START
   berikutnya.

Rumus koreksi:

```text
S = jumlah langkah yang diperintahkan pada run ini
V = volume terukur yang dimasukkan (ml)
a_baru = S / V
b_baru = 0
```

Contoh: target 100 ml, hasil 130 ml → `a_baru = S / 130`, sehingga run
berikutnya memakai langkah 100/130 dari sebelumnya.

Catatan penting:

- **Gelas harus kosong dan kering setiap run.** Sisa air dari priming atau
  run sebelumnya membuat hasil ukur terlalu besar dan kalibrasi menjadi
  berulang-ulang (gejala seperti H2O2: 25 → 50 → 90 → 98 → 100).
- Jika **tidak ada cairan keluar**, jangan isi hasil. Tekan **START** lagi
  setelah priming; hasil run yang belum diterapkan otomatis dibuang dan
  model tidak berubah.
- Jika muncul **“Cek priming!”**, hasil turun drastis dibanding run
  sebelumnya. Periksa selang input (harus terendam), udara di selang,
  sambungan longgar, atau selang pompa yang aus. Tekan **ENTER** lagi hanya
  jika hasil ukur memang benar.
- Jika muncul **“Cek pompa/prime”** saat START, model meminta langkah
  terlalu panjang (> 2.000.000 langkah). Reset lalu kalibrasi ulang; periksa
  juga priming dan selang.
- Jika muncul **“Hasil tak wajar”**, hasil ukur akan menghasilkan model di
  luar batas langkah aman. Periksa kembali angka hasil dan priming; model
  tidak diubah.
- **LULUS** bila selisih hasil ukur terhadap 100 ml ≤ 5%; **ULANGI** bila
  lebih.
- Selisih lebih dari 50% (misalnya hasil < 50 ml atau > 150 ml) harus
  dikonfirmasi dengan **ENTER** kedua untuk mencegah salah ketik.
- Setelah run pertama, model langsung dipakai oleh proses. Proses **tidak
  diblokir** menunggu 3 run, tetapi UI menandai status 3 run.
- **STOP** di tengah run tidak mengubah model. START berikutnya juga
  meminta konfirmasi **“Gelas kosong?”** karena gelas berisi sisa run
  parsial.
- **Reset** menghapus model dan mengembalikan pompa ke base nominal; proses
  akan menolak start dengan E09 sampai pompa dikalibrasi ulang.
- Pengaturan **RPM** tidak bisa diubah saat pompa berjalan. Jika aliran
  tidak stabil atau motor terasa kehilangan langkah, coba **RPM 400** dan
  kalibrasi ulang pada RPM tersebut.
- Model lama yang sudah tersimpan tetap dipakai. Tekan **Reset** bila ingin
  memulai ulang dengan base 6400 langkah/ml.

### 4.2 Pompa DC (sabun dan limbah)

Alur baru memakai **run pulse pendek** pada run pertama setiap sesi. Ini
mencegah pompa 100% menyemprot jauh melewati target saat laju sebenarnya
belum diketahui. Setelah itu target tetap **100 ml** (bisa diubah).

1. **Kalibrasi → Motor → Pompa Sabun / Pompa Limbah**.
2. Atur **Duty** (perubahan duty otomatis menghapus model lama karena laju
   berubah). Atur **Target** bila perlu; default 100 ml.
3. Siapkan **gelas takar kosong/kering**.
4. **ENTER** pada **START pulse** (run pertama). Pompa berjalan ±3 detik
   untuk limbah atau ±5 detik untuk sabun. Setelah berhenti, ukur volume
   nyata dan isi di baris **Hasil**, lalu **ENTER**.
5. **ENTER** pada **START** untuk run target 100 ml. Ukur lagi, isi Hasil,
   **ENTER**. Model langsung dikoreksi dengan `r = hasil / detik perintah`,
   `c = 0`; durasi perintah sudah mencakup ramp-up/down, sama seperti dosing
   proses.
6. Ulangi sampai baris **Run** menampilkan **LULUS** (minimal 3 run dan
   error ≤ 10%). Baris **Reset kalib** menghapus model dan memulai sesi baru.

Catatan:
- Pompa **sabun wajib terkalibrasi** bila preset memakai bilas sabun.
- Pompa **limbah tidak wajib** untuk memulai proses: drain memakai waktu
  fallback (laju 10 ml/detik + margin) sampai model tersedia.
- Bila hasil ukur tidak wajar (mis. > 4× target atau < ¼ target), hasil
  ditolak agar model tidak rusak. Jalankan START lagi.

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

- [ ] Boot normal, versi v1.0.1 tercetak, tidak ada reset berulang.
- [ ] Semua menu terbuka; preset tambah/edit/hapus bertahan setelah restart.
- [ ] Kalibrasi stepper dan DC menghasilkan model dan lulus verifikasi
      (DC: run pertama pulse pendek, lalu target 100 ml, minimal 3 run).
- [ ] Test Motor: tiap motor dan “Semua” start/stop dengan ramp.
- [ ] Proses dengan air: mulai, jeda, lanjut, beberapa siklus penuh, selesai.
- [ ] Cancel proses → semua motor lain mati dan pompa limbah menyala
      sampai wadah tersedot.
- [ ] E-stop saat proses dan saat Test Motor → pompa limbah mengevakuasi
      cairan; reset baru bisa setelah evakuasi selesai.
- [ ] Cabut listrik di tengah proses → layar pemulihan muncul di boot.
- [ ] Tangki habis / limbah penuh / suhu tinggi memunculkan error yang benar.

Dengan larutan asli (volume kecil): pastikan exhaust menyala, gunakan APD,
dan periksa korosi/kebocoran konektor setelah pengujian.
