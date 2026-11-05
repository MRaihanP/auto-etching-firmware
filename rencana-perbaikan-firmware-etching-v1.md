# Rencana Perbaikan Firmware — Alat Etching PCB Otomatis

Dasar: kode **v0.6.6** (Arduino IDE, ESP32-S3) → target **v1.0.0** (mesin standalone lengkap), lalu **v1.1.0** (IoT/MQTT opsional).
Dokumen ini hanya membahas **firmware**. Hardware dianggap final.

**Prinsip utama: tidak menulis ulang dari nol.** Driver pompa, ramp DC, input tombol, buzzer, penyimpanan NVS, menu preset, dan halaman kalibrasi sudah bekerja dan dipertahankan. Yang diganti atau ditambah adalah bagian yang membuat alat belum benar-benar otomatis: mesin proses, keselamatan, pemakaian sensor, dan beberapa kalibrasi.

---

## Bagian A — Kondisi Firmware Saat Ini

### A.1 Modul yang sudah ada

| File | Isi | Status | Rencana |
|---|---|---|---|
| `config.h` | Pin, konstanta, parameter pompa | Baik, banyak define tak terpakai dan komentar usang | Bersihkan, tambah konstanta baru |
| `dbg.h` | Log ke UART0 + USB | Berfungsi | Perbaiki risiko blocking dan boot delay |
| `input.h` | 6 tombol via PCF8574, debounce, auto-repeat kiri/kanan | Baik | Pertahankan |
| `buzzer.h` | Melodi non-blocking | Baik | Tambah pola `warn` dan `error` |
| `motor.h` | 3 stepper via FastAccelStepper, ramp, jeda/lanjut, watchdog macet | Baik | Tambah urutan dosing dan pembulatan profil kalibrasi |
| `dcmotor.h` | PWM TB6612 ×3 dengan ramp duty, agitasi 3 level | Baik | Tambah jeda/lanjut untuk DC |
| `sensor.h` | Level (touch), suhu DS18B20 ×3, MQ135 | Berfungsi tapi belum dipakai proses | Jadikan sensor selalu aktif dan dipakai proses |
| `storage.h` | NVS: preset, kalibrasi level, kalibrasi pompa (ber-versi dan CRC) | Baik | Tambah data baru + migrasi |
| `presetops.h` / `data.h` | Preset (maks 10), field edit, validasi | Baik | Tambah field siklus dan bilas |
| `process.h` | State machine proses | **Masih simulasi** | **Ditulis ulang** (inti perbaikan) |
| `ui.h` + `.ino` | Menu, halaman kalibrasi, Test Motor, toast, dialog, offset OLED | Baik, `tanganiTombol` sangat panjang | Tambah layar baru, refaktor opsional |
| `fontmini.h` | Font 3×5 | Baik | Pertahankan |

### A.2 Temuan

| ID | Tingkat | Lokasi | Temuan | Dampak |
|---|---|---|---|---|
| F1 | Tinggi | `process.h` | Fase ditentukan oleh **persentase waktu** dari `totalDetik` (`faseDariFrac`: 10% dosing, 65% etching, 13% drain, 12% bilas), bukan kondisi nyata | Durasi tiap fase tidak sesuai kerja fisik |
| F2 | Tinggi | `process.h` `aksiFase(DOSING)` | Ketiga pompa stepper **dinyalakan bersamaan**; urutan Air → H2O2 → HCl tidak ada | Menyalahi urutan penuangan yang aman |
| F3 | Tinggi | `process.h`, `data.h` | Tidak ada **siklus berulang**; field `agitasiDetik` tidak dipakai proses | Alur manual 3–4 siklus tidak bisa diotomatiskan |
| F4 | Tinggi | `process.h` | Pompa sabun **tidak pernah dipakai**; bilas air hanya mengisi 100 ml dan **tidak di-drain di akhir** | Cairan tertinggal di wadah |
| F5 | Tinggi | `process.h` `prosesToggleJeda` | Jeda hanya menghentikan stepper; **agitasi dan pompa DC tetap berjalan** | Jeda tidak benar-benar menjeda |
| F6 | Tinggi | `.ino` loop | E-stop hanya dipolling, **tanpa latch dan tanpa state**; pemanggilan stop berulang tiap loop; aktuator bisa dimulai lagi saat tombol ditekan | Tidak ada pengaman yang bisa dipercaya |
| F7 | Tinggi | `sensor.h`, `process.h` | Sensor level, suhu, dan MQ135 **tidak dipakai proses**. Suhu bahkan hanya dibaca saat halaman kalibrasi suhu terbuka (`sensorSuhuUpdate` dipanggil di `HAL_KALIB_SUHU`) | Tidak ada precheck, tangki kosong atau limbah penuh tidak terdeteksi |
| F8 | Sedang | `motor.h` | Kalibrasi stepper menghitung langkah **hanya saat cruise**, padahal dosing menggerakkan total langkah **termasuk ramp**. Dengan 500 rpm dan 500 rpm/s, ramp naik+turun ≈ 26.667 langkah ≈ **8 putaran** per dosing | Galat volume berbeda antara dosis kecil dan besar |
| F9 | Sedang | `process.h`, `motor.h` | Drain berbasis fraksi waktu; durasi `dcDosisMs` hanya dicetak ke log. `pompaSiapSemua()` hanya memeriksa kalibrasi stepper, bukan pompa DC | Drain bisa terlalu pendek atau panjang |
| F10 | Sedang | `sensor.h`, `storage.h` | Offset suhu, pemetaan sensor suhu, dan baseline MQ135 **tidak disimpan** ke NVS; alamat DS18B20 ditanam di kode | Kalibrasi hilang saat restart |
| F11 | Sedang | `process.h` | Timeout hanya ada di dosing; saat timeout proses dibatalkan tanpa kode error atau layar | Kegagalan sulit dipahami |
| F12 | Sedang | `sensor.h`, `config.h` | Level memakai konstanta default tertanam (113000 / 156000) untuk semua sensor, tanpa penanganan drift baseline. Define `SENSOR_AVG_N`, `AMBANG_*`, `TOUCH_*`, `INTERVAL_SENSOR_MS`, `SENSOR_LOG_PERIOD_MS` tidak terpakai | Bacaan bisa bergeser, konfigurasi membingungkan |
| F13 | Sedang | `.ino` setup, `dbg.h` | `delay(2000)` dan `dbgInit()` dipanggil dua kali saat boot. Penulisan ke USB CDC tanpa host **bisa memblokir loop** pada beberapa versi core (perlu diuji) | Boot lambat, kemungkinan loop tersendat saat alat berdiri sendiri |
| F14 | Rendah | beberapa file | LED strip (GPIO7) tidak diinisialisasi. Komentar usang (header `.ino` v0.5.1; komentar RPM dan akselerasi di `config.h`/`motor.h` salah satuan). Typo `TOUTH_`, `MQ135_PIN` ganda dengan `PIN_MQ135`. `sensor.h` memakai `dbg` dan `halaman` tanpa include sendiri | Rawan error saat menambah modul |
| F15 | Rendah | `.ino` | `tanganiTombol` ≈ 600 baris dalam satu fungsi | Sulit dirawat |
| F16 | Rendah | `data.h`, `presetops.h` | Preset default 33:17:50 (HCl:H2O2 ≈ 2:1) tidak mengikuti rasio 1:2:4. Validasi `agitasiSpeed` 1–5 padahal UI 1–3 | Konfirmasi rasio |
| F17 | Info | seluruh kode | Belum ada IoT/MQTT | Fitur yang diminta |

### A.3 Koreksi terhadap rencana sebelumnya

| Topik | Rencana lama | Kenyataan di kode | Keputusan |
|---|---|---|---|
| Level sensor | "Baca digital saja karena WiFi" | Level dibaca dengan `touchRead` pada GPIO11–14 dan **GPIO3** (semua pin touch ESP32-S3), bukan ADC2, jadi tidak berebut dengan WiFi. LVL5 = **GPIO3**, bukan 48 | Pertahankan `touchRead` + persen untuk tampilan. Logika proses memakai status **basah/kering** dengan histeresis (setara digital). Uji dengan WiFi aktif (Tahap 2) |
| Area OLED | Offset 16 px, area 128×48 | `UI_Y_OFFSET = 15`, area pakai **128×49**, geser buffer sudah diimplementasi | Pakai nilai kode |
| Pembangkit pulsa stepper | Pilih saat implementasi | Sudah FastAccelStepper | Tidak perlu diubah |
| Duty agitasi | 50 / 60 / 70% | Default 40 / 70 / 100%, bisa diatur di halaman kalibrasi | Konfirmasi default |
| Pemetaan level | Asumsi | `HCl, H2O2, H2O, Sabun, Limbah`, dan tidak ada sensor di wadah etching | Sesuai asumsi, drain berbasis waktu |

---

## Bagian B — Cara Kerja Target

### B.1 Parameter per preset

Preset sekarang menyimpan nama, ukuran PCB, persen dan ml tiap larutan, agitasi, dan estimasi total. Ditambah (Preset **v2**):

| Field | Nilai | Keterangan |
|---|---|---|
| `siklus` | 1–6 | Jumlah siklus etching |
| `agitasiDetik` | 30–1800 (langkah 10) | **Lama etching/agitasi per siklus.** Semantik dibakukan |
| `agitasiSpeed` | 1–3 | SPD1/2/3 |
| `bilasSabun` | 0–3 | Jumlah bilas air sabun |
| `bilasAir` | 0–5 | Jumlah bilas air |
| `bilasMl` | ml | Volume tiap bilas |
| `bilasDetik` | detik | Agitasi tiap bilas |
| `konfirmasiSiklus` | 0/1 | Jika 1, alat bertanya "trace sudah sempurna?" tiap akhir siklus |
| ~~`totalDetik`~~ | dihapus | Dihitung otomatis dari model kalibrasi |

Pengaturan global baru (key `set` di NVS): ambang suhu peringatan dan bahaya, margin drain, waktu stabil level, status MQ135 aktif, `iot_enabled`.

### B.2 Alur proses

```
IDLE → PRECHECK → WAIT_PCB → [ SIKLUS ETCHING ×N ] → BILAS SABUN ×S → BILAS AIR ×M → SELESAI

Siklus etching:
  DOSE_AIR → DOSE_H2O2 → DOSE_HCL → AGITASI (timer) → DRAIN → DECIDE

Bilas (sabun atau air):
  ISI (pompa sabun atau stepper air) → AGITASI singkat → DRAIN

Overlay: JEDA, ERROR, ESTOP
```

### B.3 Aturan tiap fase

- **PRECHECK:** E-stop tidak aktif; kalibrasi **semua** pompa yang dipakai tersedia (stepper ×3, pompa limbah, pompa sabun bila `bilasSabun > 0`); suhu terbaca; tangki sumber basah dan estimasi volume cukup; tangki limbah **tidak** basah dan masih muat. Gagal → `ERROR` dengan kode dan pesan.
- **WAIT_PCB:** menunggu pengguna menaruh PCB lalu ENTER. PCB tetap di wadah sampai selesai.
- **DOSE:** satu pompa per waktu, urutan Air → H2O2 → HCl. Jumlah langkah dari model kalibrasi pompa. Selesai bila pompa benar-benar berhenti.
- **AGITASI:** duty sesuai SPD preset selama `agitasiDetik`. Suhu chamber dipantau.
- **DRAIN:** pompa limbah selama `(volume − c) / r × (1 + margin) + tambahan`. Sensor limbah menjadi penjaga: basah sebelum atau saat drain → `E06`.
- **DECIDE:** `i < N` → siklus berikutnya. Jika `konfirmasiSiklus`, pengguna memilih ulangi atau selesai.
- **JEDA:** menghentikan **semua** aktuator (dengan ramp), menahan timer, dan melanjutkan dari sisa waktu atau sisa langkah.
- **BATAL:** berhenti, lalu menawarkan **"Drain wadah?"** (volume di wadah dilacak software).
- **ERROR/ESTOP:** semua aktuator padam, kode ditampilkan, keluar hanya lewat tindakan pengguna.

### B.4 Peran sensor

| Sensor | Dipakai untuk |
|---|---|
| Level HCl / H2O2 / H2O / Sabun | Tangki sumber masih ada cairan (precheck dan selama dosing) |
| Level Limbah | Mencegah drain ke tangki penuh |
| DS18B20 chamber / komponen / motor | Peringatan dan batas suhu; di atas batas bahaya → `ERROR` |
| MQ135 | Informasi dan peringatan saja (opsional, tidak pernah menjadi satu-satunya pengaman) |

### B.5 Mode operasi

- **Standalone** (utama): semua fitur lewat tombol dan OLED.
- **Manual/Kalibrasi/Test Motor:** sudah ada, tetap dipertahankan.
- **IoT** (v1.1): pemantauan dan perintah jarak jauh; tidak boleh memengaruhi mode standalone.

---

## Bagian C — Strategi dan Aturan Kerja

1. **Satu tahap = satu build yang bisa diflash dan diuji.** Beri nomor versi di `FW_VERSI` dan git tag.
2. **Jangan ubah dan refaktor di tahap yang sama.** Refaktor besar (Tahap 9) dikerjakan terakhir, saat fitur sudah stabil.
3. **Loop kooperatif tetap dipertahankan.** Stepper sudah dibangkitkan hardware, DC memakai ramp berbasis waktu, jadi tidak perlu pindah ke banyak task. Satu-satunya task terpisah nanti adalah MQTT (core 0).
4. **Uji dengan air dulu.** Larutan asli hanya di Tahap 10.
5. **Mode uji cepat:** `#define DEBUG_PROSES_CEPAT` mengecilkan semua durasi etching dan agitasi (skala 1/10) agar alur lengkap bisa diuji dalam menit.
6. **Regresi wajib setelah tiap tahap** (Lampiran A): boot, navigasi menu, edit/simpan preset, buka tiap halaman kalibrasi, Test Motor, mulai/jeda/batal proses, E-stop.

### Peta versi

| Versi | Tahap | Isi |
|---|---|---|
| v0.6.7 | 0 | Pembersihan baseline |
| v0.7.0 | 1–2 | Keselamatan + sensor aktif |
| v0.8.0 | 3 | Kalibrasi sesuai profil dosing |
| v0.9.0 | 4 | Mesin proses baru |
| v0.9.5 | 5–7 | Pelacak tangki, pemulihan, UI baru |
| v1.0.0 | 9–10 | Refaktor opsional, akseptansi |
| v1.1.0 | 8 | IoT/MQTT |

---

## Bagian D — Tahapan

### Tahap 0 — Pembersihan baseline (v0.6.7)

**Tujuan:** dasar yang bersih dan bisa dipertanggungjawabkan sebelum fitur ditambah.

- [ ] Git tag `v0.6.6` sebelum mulai
- [ ] Setiap header menyertakan dependensinya sendiri (`sensor.h` butuh `dbg.h` dan `data.h`; `data.h` butuh `dbg.h`)
- [ ] Hapus define yang tidak terpakai (F12) atau tandai "cadangan"; perbaiki typo `TOUTH_`; hapus `MQ135_PIN` ganda
- [ ] Perbaiki komentar usang: header `.ino`; satuan "langkah/detik" pada RPM dan perhitungan ramp di `config.h` dan `motor.h`
- [ ] `setup()`: `delay(2000)` + `dbgInit()` kedua hanya bila `DEBUG_BOOT_WAIT` aktif
- [ ] Inisialisasi LED strip (GPIO7), atau catat bahwa strip sepenuhnya hardwired
- [ ] Uji logging: jalankan alat **tanpa USB terhubung** selama 10 menit dengan log aktif; ukur apakah loop tersendat (periode loop tidak boleh melebar). Bila ya, nonaktifkan tulis USB saat tidak ada host atau atur timeout tulis menjadi 0 (sesuai API core yang dipakai)
- [ ] Tambah makro `LOG_LEVEL` agar log detail bisa dimatikan

**Uji:** semua item Lampiran A.
**Lulus:** perilaku identik dengan v0.6.6, boot tanpa delay 2 detik, tidak ada warning kompilasi baru.

### Tahap 1 — Modul keselamatan (v0.7.0)

**Tujuan:** pengaman yang bisa dipercaya sebelum ada logika proses baru (F6).

- [ ] File baru `safety.h`: state global `{OK, ESTOP, ERROR}` dan `faultKode`
- [ ] E-stop dibaca lewat **interrupt** (hanya menandai flag) + debounce 20 ms; **latch**: tetap `ESTOP` sampai tombol dilepas **dan** pengguna menekan ENTER di layar E-stop
- [ ] `safetyDarurat()` dipanggil **sekali saat transisi**: `pompaStopAllDarurat()`, `dcStopAllDarurat()`, batalkan semua state uji kalibrasi, batalkan proses
- [ ] `safetyRaise(kode)` untuk fault non-E-stop; tabel kode di Lampiran C
- [ ] Semua tindakan yang menyalakan aktuator (Test Motor, kalibrasi, proses) **diblok** selama state ≠ `OK`, dengan toast alasan
- [ ] Halaman overlay E-STOP / ERROR (wireframe di Bagian E) dan pola buzzer `error`
- [ ] Hapus blok E-stop lama di `loop()`

**Uji:** tekan E-stop saat (a) proses berjalan, (b) Test Motor, (c) kalibrasi stepper, (d) kalibrasi DC; lepas lalu coba mulai lagi tanpa konfirmasi.
**Lulus:** semua aktuator padam seketika, hanya satu log per kejadian, tidak ada aktuator bisa dinyalakan sebelum konfirmasi.

### Tahap 2 — Sensor aktif dan siap dipakai proses (v0.7.0)

**Tujuan:** sensor menjadi sumber data tepercaya (F7, F10, F12).

- [ ] `sensorTick()` tunggal dipanggil tiap loop dengan interval masing-masing: level 50 ms (`INTERVAL_SENSOR_MS`), suhu 1 s, MQ135 500 ms; **tidak lagi bergantung halaman**
- [ ] **Level:** status `levelBasah(i)` dari persen kalibrasi yang sudah ada: basah bila ≥ ambang ON, kering bila ≤ ambang OFF (histeresis), dan berubah hanya setelah N sampel berturut-turut (pakai `TOUCH_DEBOUNCE`). Tambah penanda kesehatan (nilai di luar rentang wajar → FAULT)
- [ ] **Level:** gunakan hasil kalibrasi per sensor (bukan konstanta tertanam); drift baseline pelan-pelan diadaptasi hanya saat status kering stabil (memakai define adaptasi yang sudah ada)
- [ ] **Suhu:** pemetaan blok ↔ alamat ROM dan offset disimpan ke NVS (key baru `kal2`); alamat di kode hanya menjadi nilai awal. Deteksi sensor lepas → status FAULT
- [ ] **MQ135:** baseline "normal/kotor" disimpan di `kal2`; opsi `mqAktif` di pengaturan; tidak aktif → status diabaikan
- [ ] Ambang suhu `TEMP_PERINGATAN_C` dan `TEMP_BAHAYA_C` di `config.h` (nilai awal ditentukan dari pengukuran nyata)
- [ ] **Uji dengan WiFi aktif** (modul uji sederhana yang menyalakan WiFi): bandingkan fluktuasi bacaan level dengan WiFi mati dan nyala

**Uji:** celup dan angkat tiap strip; cabut satu DS18B20; restart lalu cek offset dan pemetaan tetap; pantau status level dengan WiFi nyala.
**Lulus:** status basah/kering tidak berkedip, nilai kalibrasi bertahan setelah restart, suhu terbaca di halaman mana pun.

### Tahap 3 — Kalibrasi sesuai profil dosing (v0.8.0)

**Tujuan:** volume dosing akurat di semua ukuran dosis (F8, F9).

**Masalah:** uji kalibrasi stepper menghitung langkah hanya saat kecepatan sudah mencapai RPM target, sedangkan dosing menjalankan total langkah termasuk ramp. Satu angka `ml/rotasi` dari satu titik uji hanya cocok untuk volume yang mirip.

**Perubahan stepper (`motor.h`, `ui.h`, `.ino`, `storage.h`):**
- [ ] Jalur uji kalibrasi memakai **`pompaJalanLangkah(idx, N)` yang sama dengan dosing**, sehingga profil identik
- [ ] Setiap uji menghasilkan pasangan (langkah N, volume terukur V)
- [ ] Simpan sampai 6 titik per pompa; model **`langkah = a·ml + b`** (regresi linear, minimal 2 titik dengan volume berbeda, misalnya 20 ml dan 120 ml)
- [ ] Tampilkan galat tiap titik dan koefisien; tombol "Reset titik"
- [ ] Data lama (`mlPerRev`) tetap dipakai bila belum ada model baru (`b = 0`)
- [ ] `pompaMulai()` dan `pompaPerkiraanMs()` memakai model baru
- [ ] Pertahankan uji verifikasi LULUS/GAGAL (`KALIB_TOLERANSI_STEPPER_PCT`), kini di tiga volume

**Perubahan pompa DC (`dcmotor.h`, `ui.h`, `.ino`):**
- [ ] Uji kalibrasi memakai urutan yang sama dengan penggunaan nyata (ramp naik, jalan, ramp turun), dan hasil disimpan sebagai pasangan (waktu, ml)
- [ ] Model `ml = r·t + c`; `dcDosisMs()` dihitung dari model, termasuk margin `DC_MARGIN_PCT`
- [ ] Pompa sabun dan limbah wajib terkalibrasi untuk menjalankan proses

**Format data:** `FORMAT_VERSI_POMPA` 3 → 4, dengan migrasi dari v3 (lihat Bagian F).

**Uji:** dosing 20, 60, 120 ml pada tiap pompa, masing-masing 3 kali; ukur dengan gelas ukur.
**Lulus:** galat ≤ 5% (stepper) dan ≤ 10% (DC) di **ketiga** volume, hasil konsisten dalam 3 pengulangan, data bertahan setelah restart.

### Tahap 4 — Mesin proses baru (v0.9.0)

**Tujuan:** proses etching benar-benar otomatis (F1–F5, F9, F11). Ini inti perbaikan.

**Struktur:** ganti isi `process.h`, pertahankan nama fungsi publik (`prosesMulai`, `prosesUpdate`, `prosesToggleJeda`, `prosesBatal`, `prosesSelesai`, `prosesKeluarSelesai`) agar `ui.h` dan `.ino` tidak berubah besar.

**Tabel state (`Fase` × `Langkah`):**

| Fase | Langkah | Aksi masuk | Selesai bila | Timeout |
|---|---|---|---|---|
| PRECHECK | — | Cek syarat B.3 | Semua lulus | — |
| WAIT_PCB | — | Layar konfirmasi | ENTER | — |
| ETCH | DOSE_AIR | `pompaMulai(H2O)` | Pompa berhenti | 2× estimasi + margin |
| ETCH | DOSE_H2O2 | `pompaMulai(H2O2)` | Pompa berhenti | idem |
| ETCH | DOSE_HCL | `pompaMulai(HCl)` | Pompa berhenti | idem |
| ETCH | AGITASI | `agitasiSet(spd)` | Timer `agitasiDetik` habis | durasi + 10 s |
| ETCH | DRAIN | `pompaLimbahSet(true)` | Waktu drain habis | durasi + margin |
| ETCH | DECIDE | — | `i == N` atau pilihan pengguna | — |
| BILAS_SABUN | ISI/AGITASI/DRAIN | Pompa sabun → agitasi → drain | per langkah | per langkah |
| BILAS_AIR | ISI/AGITASI/DRAIN | Stepper air → agitasi → drain | per langkah | per langkah |
| SELESAI | — | Semua padam, buzzer selesai | ENTER | — |

**Pekerjaan:**
- [ ] Urutan dosing tunggal (satu pompa per waktu); lompat ke langkah berikutnya hanya saat pompa **benar-benar berhenti** (`pompaSelesai`)
- [ ] Durasi dihitung dari model kalibrasi; `prosesTotalDetik()` jadi estimasi hasil hitung, progres berdasarkan langkah yang selesai
- [ ] Drain: `waktu = (volume − c)/r × (1 + margin) + tambahan`; sensor limbah menjadi penjaga (`E06`)
- [ ] Pelacak **volume wadah** (software): bertambah saat dosing/isi, nol setelah drain; dipakai untuk "Drain wadah?" saat batal
- [ ] **Jeda** menghentikan stepper, DC, dan agitasi; **lanjut** memulai ulang langkah dari sisa waktu/langkah (stepper memakai `pompaSisaLangkah` yang sudah ada)
- [ ] **Batal:** hentikan semua; tampilkan dialog "Drain wadah? ENTER: ya, BACK: tidak"
- [ ] Timeout tiap langkah → `safetyRaise(E04)` dengan layar error (bukan batal diam-diam)
- [ ] Hapus `faseDariFrac()` dan seluruh logika persentase waktu
- [ ] Tambah field Preset v2 (Bagian F) dan UI pengeditannya di `presetops.h`/`ui.h`
- [ ] `DEBUG_PROSES_CEPAT` untuk uji alur cepat

**Sub-tahap uji (satu per satu, air saja):**
1. 4a — urutan dosing Air → H2O2 → HCl, volume sesuai model
2. 4b — agitasi dengan timer + jeda/lanjut
3. 4c — drain dengan waktu hitungan, sensor limbah sebagai pengaman
4. 4d — bilas sabun dan bilas air
5. 4e — satu proses penuh, 3 siklus + bilas
6. 4f — matriks jeda/batal/E-stop di **setiap** langkah

**Lulus:** proses selesai tanpa intervensi selain menaruh/mengambil PCB; wadah kosong di akhir; tiap jalur batal/error berakhir di kondisi aman.

### Tahap 5 — Pelacak volume tangki dan precheck (v0.9.5)

**Tujuan:** tahu sisa cairan karena sensor level hanya memberi status di satu titik.

- [ ] Key NVS `tank`: sisa volume HCl, H2O2, H2O, sabun, dan isi tangki limbah (ml)
- [ ] Menu **"Isi Ulang Tangki"**: pengguna memasukkan volume awal tiap tangki
- [ ] Setiap dosing/isi mengurangi sisa, tiap drain menambah isi limbah
- [ ] PRECHECK membandingkan sisa dengan kebutuhan **seluruh** proses (semua siklus + bilas); sensor level tetap sebagai pengaman akhir
- [ ] Peringatan "tangki hampir habis" di layar proses

**Uji:** isi tangki dengan volume diketahui, jalankan proses, bandingkan perkiraan sisa dengan ukur nyata.
**Lulus:** selisih sisa perkiraan dan nyata ≤ 10% setelah 3 proses; proses ditolak bila tidak cukup.

### Tahap 6 — Pemulihan dan ketahanan (v0.9.5)

**Tujuan:** aman setelah gangguan (F11).

- [ ] Key NVS `runstate`: fase, langkah, siklus, volume wadah; ditulis **saat transisi** (bukan tiap detik, agar NVS awet)
- [ ] Boot dengan `runstate` aktif → layar pemulihan: **"Drain & bilas"** atau **"Abaikan"**; tidak ada aksi otomatis setelah boot
- [ ] Task watchdog ESP32 pada loop utama; log alasan reset di boot
- [ ] Batas suhu: peringatan → buzzer dan layar; bahaya → `E02`
- [ ] Audit watchdog macet stepper (3 detik) terhadap ramp yang sangat landai

**Uji:** cabut daya di tiap fase lalu nyalakan; panaskan sensor chamber untuk melewati ambang; tahan loop sengaja untuk memicu watchdog.
**Lulus:** setelah boot tidak ada aktuator menyala sendiri, pemulihan menuntaskan drain dengan benar.

### Tahap 7 — UI baru (v0.9.5)

**Tujuan:** layar yang mendukung alur baru, memakai helper yang sudah ada (`gambarHeader`, `teksTengah`, `gambarDialog`, `tampilToast`, `fontmini`). Area pakai **128×49**, header 12 px.

- [ ] Layar proses baru (siklus, fase, suhu, sisa waktu, peringatan tangki)
- [ ] Layar `WAIT_PCB`, konfirmasi siklus, `ERROR`, `E-STOP`, pemulihan, dialog "Drain wadah?"
- [ ] Menu utama bertambah: **Pengaturan**, **Isi Ulang Tangki**, **Info/Status** (JUMLAH_MENU 4 → 7)
- [ ] Field Preset v2 di editor preset (siklus, bilas, konfirmasi)
- [ ] Info/Status: versi, suhu 3 titik, status 5 level, MQ135, alasan reset terakhir
- [ ] Pola buzzer baru: `warn`, `error`
- [ ] Tahan BACK ≥ 1 detik = kembali ke menu utama (kecuali saat proses)

**Uji:** telusuri semua layar dengan data ekstrem (nama panjang, angka 4 digit, error).
**Lulus:** tidak ada teks terpotong di area 128×49, navigasi tidak menyebabkan reset atau hang.

### Tahap 8 — IoT/MQTT (v1.1.0)

**Tujuan:** pemantauan dan perintah jarak jauh **tanpa** mengganggu standalone. Dikerjakan **setelah** v1.0.0 stabil.

- [ ] Modul `net_mqtt.h` hanya di-include dari `.ino` dalam `#ifdef FITUR_IOT`; modul lain **tidak boleh** meng-include-nya
- [ ] Antarmuka ke inti: `StatusSnapshot` (disalin di loop) keluar, antrean perintah masuk
- [ ] Task FreeRTOS di core 0 untuk WiFi dan MQTT; reconnect dengan backoff, Last Will `offline`, status `retain`
- [ ] Perintah: `pause`, `resume`, `stop`, `start` (tetap butuh ENTER fisik di `WAIT_PCB`), ubah parameter hanya saat `IDLE`
- [ ] Kredensial WiFi/MQTT: awal lewat `secrets.h` yang tidak di-commit; pengisian via serial atau portal captive menyusul. Mengetik password dengan 6 tombol tidak praktis
- [ ] Opsi `iot_enabled` di Pengaturan; WiFi mati total bila nonaktif
- [ ] Topik dan payload: Lampiran D

**Uji wajib:**
1. Proses penuh dengan WiFi dimatikan: hasil sama dengan v1.0.0
2. WiFi/broker diputus di tengah proses: proses tidak berhenti dan tidak melambat
3. `stop` dari MQTT menghentikan proses dengan benar
4. Perintah tidak valid ditolak lewat `ack`
5. Bacaan level touch tetap stabil saat WiFi aktif (ulangi uji Tahap 2)

### Tahap 9 — Refaktor opsional (v1.0.0)

**Tujuan:** perawatan jangka panjang (F15). Dikerjakan hanya bila fitur sudah stabil dan regresi sudah lulus.

- [ ] Pecah `tanganiTombol` menjadi satu fungsi per halaman tanpa mengubah logika (mekanis)
- [ ] Tabel halaman: `{ gambar, tombol, masuk, keluar }` per `Halaman`
- [ ] Setelah refaktor, jalankan seluruh checklist Lampiran A

### Tahap 10 — Akseptansi (v1.0.0)

- [ ] 10 proses berurutan dengan air, tiap proses 3 siklus + bilas
- [ ] Skenario ekstrem: tangki habis di tengah, limbah penuh, suhu tinggi, tombol ditekan berulang, E-stop di tiap langkah
- [ ] Uji dengan larutan asli volume kecil; pastikan exhaust menyala dan cek korosi konektor setelahnya
- [ ] Dokumentasi: cara kalibrasi, daftar error, parameter default

**Lulus:** semua uji lulus, tidak ada reset tak terduga, volume dosing sesuai di semua ukuran.

---

## Bagian E — Rancangan Layar Baru

Karakter 6×8 px → 21 karakter × 6 baris di area 128×49 (di luar header).

**Layar proses**
```
Preset 1           42%
        12:45
[#########-----------]
S2/3 ETCH         31.5C
sisa 05:10        OK
```

**WAIT_PCB**
```
 TARUH PCB
 di wadah etching,
 lalu tekan ENTER
 BACK : batal
```

**Konfirmasi siklus**
```
 Siklus 1/3 selesai
 Trace sudah OK?
 > Ulangi siklus
   Selesai & bilas
```

**ERROR**
```
 !! ERROR E05 !!
 Tangki HCl kosong
 ENTER : lanjut
```

**E-STOP**
```
 !! E-STOP !!
 Lepas tombol lalu
 tekan ENTER
```

**Pemulihan setelah mati listrik**
```
 Proses terputus
 Siklus 2 / ETCH
 > Drain & bilas
   Abaikan
```

**Isi ulang tangki**
```
 Isi Ulang Tangki
 HCl      850 ml
 H2O2     600 ml
 H2O     1200 ml
 Sabun    400 ml
 Limbah   120 ml
```

**Perilaku tombol standar:** ATAS/BAWAH pilih, KIRI/KANAN ubah nilai (tahan = cepat), ENTER konfirmasi, BACK batal/mundur, tahan BACK = ke menu utama.

---

## Bagian F — Perubahan Data dan Migrasi NVS

### Preset v2

```cpp
struct Preset {
  char nama[12];
  int  pcbPanjang, pcbLebar;
  int  persenHCl, persenH2O2, persenH2O;
  int  mlHCl, mlH2O2, mlH2O;
  int  agitasiDetik;       // lama etching per siklus (30..1800 s)
  int  agitasiSpeed;       // 1..3
  int  siklus;             // 1..6      (baru)
  int  bilasSabun;         // 0..3      (baru)
  int  bilasAir;           // 0..5      (baru)
  int  bilasMl;            // ml        (baru)
  int  bilasDetik;         // detik     (baru)
  int  konfirmasiSiklus;   // 0/1       (baru)
};  // totalDetik dihapus
```

`FORMAT_VERSI` 1 → 2. **Migrasi:** baca blob lama, konversi ke struktur baru dengan nilai awal (`siklus=3`, `bilasSabun=1`, `bilasAir=2`, `bilasMl=100`, `bilasDetik=20`, `konfirmasiSiklus=0`). **Jangan reset ke default** agar preset pengguna tidak hilang. `agitasiDetik` lama (4–10 s) ditandai "perlu ditinjau" karena semantiknya berubah.

### Key NVS

| Key | Isi | Versi |
|---|---|---|
| `cfg` | Preset v2 | 2 |
| `kal` | Kalibrasi level (0%/100%) | 1 |
| `pompa` | Model stepper `a, b` + titik uji, model DC `r, c`, RPM, duty, duty agitasi | 4 (migrasi dari 3) |
| `kal2` | Offset suhu, pemetaan alamat DS18B20, baseline MQ135 | 1 |
| `set` | Ambang suhu, margin drain, waktu stabil level, `mqAktif`, `iot_en` | 1 |
| `tank` | Sisa volume tangki + isi limbah | 1 |
| `runstate` | Fase, langkah, siklus, volume wadah | 1 |

Semua key memakai versi + CRC seperti pola yang sudah ada.

---

## Lampiran

### A. Checklist regresi (setiap tahap)

- [ ] Boot normal, versi tercetak, tidak ada reset berulang
- [ ] Navigasi semua menu, animasi list, offset OLED benar
- [ ] Preset: tambah, duplikat, hapus, edit, simpan, restart, data tetap
- [ ] Halaman kalibrasi: stepper, DC, agitasi, level, suhu, udara terbuka dan berfungsi
- [ ] Test Motor: tiap motor dan "Semua" start/stop dengan ramp
- [ ] Proses: mulai, jeda, lanjut, batal, selesai
- [ ] E-stop saat proses dan saat Test Motor
- [ ] Data kalibrasi bertahan setelah restart

### B. Peta perubahan per file

| File | Perubahan |
|---|---|
| `safety.h` (baru) | Tahap 1 |
| `sensor.h` | Tahap 0, 2 |
| `process.h` | Tahap 4 (ditulis ulang) |
| `motor.h` | Tahap 3, 4 |
| `dcmotor.h` | Tahap 3, 4 |
| `storage.h` | Tahap 2, 3, 4, 5, 6 |
| `data.h` / `presetops.h` | Tahap 4, 7 |
| `ui.h` | Tahap 1, 3, 4, 5, 7 |
| `config.h` | Tahap 0, 2, 4 |
| `.ino` | Tahap 0, 1, 3, 7, 9 |
| `buzzer.h` | Tahap 7 |
| `dbg.h` | Tahap 0 |
| `net_mqtt.h` (baru) | Tahap 8 |

### C. Kode error

| Kode | Nama | Penyebab |
|---|---|---|
| E01 | ESTOP | Tombol darurat ditekan |
| E02 | TEMP_HIGH | Suhu melewati batas bahaya |
| E03 | SENSOR_FAIL | Sensor wajib gagal |
| E04 | TIMEOUT | Langkah melebihi batas waktu |
| E05 | TANK_EMPTY | Tangki kosong atau sisa tidak cukup |
| E06 | WASTE_FULL | Tangki limbah penuh |
| E07 | NVS_CORRUPT | Data penyimpanan rusak |
| E08 | RESUME_NEEDED | Proses terputus, butuh keputusan pengguna |
| E09 | CAL_MISSING | Kalibrasi belum lengkap |
| E10 | WDT | Watchdog memicu reset |

### D. Topik MQTT (untuk v1.1)

| Topik | Arah | Isi |
|---|---|---|
| `etch/<id>/status` | alat → broker (retain) | Fase, langkah, siklus, sisa waktu, suhu ×3, status tangki ×5, kode error |
| `etch/<id>/event` | alat → broker | Mulai, selesai, error |
| `etch/<id>/cmd` | broker → alat | `pause`, `resume`, `stop`, `start`, `set` |
| `etch/<id>/ack` | alat → broker | Diterima atau ditolak + alasan |
| `etch/<id>/lwt` | alat → broker (retain) | `online` / `offline` |

### E. Keputusan yang perlu Anda konfirmasi

| # | Pertanyaan | Usulan awal |
|---|---|---|
| 1 | Rasio default: 1:2:4 (HCl:H2O2:Air) atau 33:17:50 seperti preset sekarang? | Ikuti kebutuhan Anda, ubah preset default |
| 2 | Perilaku **Batal**: berhenti saja, atau tawarkan drain? | Tawarkan drain |
| 3 | PCB tetap di wadah selama semua siklus? | Ya |
| 4 | `agitasiDetik` = lama etching per siklus? | Ya (30–1800 s) |
| 5 | Default `konfirmasiSiklus` | Mati |
| 6 | Default duty agitasi: 40/70/100 (kode) atau 50/60/70? | 50/60/70 |
| 7 | LED strip dikendalikan firmware atau hardwired? | Cek, tambahkan init bila perlu |
| 8 | Cara mengisi kredensial WiFi | `secrets.h` dulu |
