# RELEASES - Registry Versi dan Rollback

Registry versi repo `auto-etching-firmware`. Rilis yang disiapkan untuk
push terbaru: **v1.3.0**. Tag lama yang sudah ada di Git: `v1.2.0`,
`v1.0.1`, `v1.0.0`, dan tag v0.x.

## 1. Daftar Versi

| Versi | Tanggal | Tag | Ringkasan |
|---|---|---|---|
| **v1.3.0** | 2026-11-11 | `v1.3.0` (rilis ini) | OTA upload `.bin` dari Settings, status `ip`/`build`/`rssi`, versi firmware otomatis setelah OTA, halaman Preset & Kalibrasi lengkap, Settings info/versi/repo, panel kiri hover, nama Auto Etching V1, header firmware dipindah ke `src/` |
| **v1.2.0** | 2026-11-11 | `v1.2.0` | Preset v3, PRECHECK seluruh proses, drain berbasis volume, opsi jeda/pemulihan, error spesifik, IoT MQTT TLS, OTA HTTP, menu Status IoT, diagnosa WiFi, dashboard web self-hosted `webapp/` |
| v1.1.0 | 2026-11-10 | - | Rasio kimia ml/persen, waktu etching otomatis dari tembaga, kapasitas jerigen, tebal Cu dalam oz |
| v1.0.1 | 2026-11-09 | `v1.0.1` | Siklus penuh berulang, kalibrasi DC pulse, evakuasi cairan saat cancel/E-stop |
| v1.0.0 | 2026-11-05 | `v1.0.0` | Mesin standalone lengkap: siklus, error E01-E10, watchdog, pemulihan, kalibrasi model |
| v0.6.6 | 2026-10-30 | `v0.6.6` | Verifikasi hasil kalibrasi LULUS/GAGAL |
| v0.6.5 | 2026-10-30 | `v0.6.5` | Kalibrasi stepper hanya menghitung rotasi di RPM target |
| v0.6.4 | 2026-10-30 | `v0.6.4` | Uji kalibrasi target ml |
| v0.6.3 | 2026-10-29 | `v0.6.3` | Test Motor menampilkan RPM aktual |
| v0.6.2 | 2026-10-29 | `v0.6.2` | Step/rev 3200 (DRV8825 1/16) |
| v0.6.1 | 2026-10-29 | `v0.6.1` | Ramp stepper 18,75 rpm/detik |
| v0.6.0 | 2026-10-29 | `v0.6.0` | Ramp semua motor, Test Motor 7 baris |
| v0.5.4 | 2026-10-28 | `v0.5.4` | Ramp naik/turun stepper |
| v0.5.3 | 2026-10-28 | `v0.5.3` | Halaman Test Motor |
| v0.5.2 | 2026-10-28 | `v0.5.2` | Pompa stepper RPM 400-700 |
| v0.5.1 | 2026-10-16 | `v0.5.1` | Exhaust fan langsung 12 V |
| v0.5.0 | 2026-10-13 | `v0.5.0` | Baseline modular |

Detail perubahan: [`LOG.md`](./LOG.md). Versi lama tidak disimpan sebagai
folder salinan; gunakan Git tag.

## 2. Build Rilis v1.3.0

```bash
~/DevBox/bin/arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs" \
  --libraries /home/raihanP/Arduino/libraries \
  --output-dir build_ota .
```

Hasil build:

```text
Flash : 1.246.234 byte (63% dari 1.966.080)
RAM   : 78.252 byte (23%)
```

Flash USB pertama:

```bash
~/DevBox/bin/arduino-cli upload \
  -p /dev/ttyACM0 \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs" \
  .
```

OTA setelah firmware berjalan:

```bash
curl -X POST \
  --data-binary @build_ota/auto-etching-firmware.ino.bin \
  -H "X-OTA-Password: <OTA_PASSWORD>" \
  http://<IP_ALAT>:3232/update
```

## 3. Rollback

```bash
git tag -l
git log --oneline --decorate
git show v1.0.1:config.h | grep FW_VERSI
git checkout v1.0.1      # lihat/flash versi lama
git checkout main        # kembali ke versi terbaru
```

Untuk membuat branch dari versi lama:

```bash
git switch -c legacy/v1.0.1 v1.0.1
```

Jangan memakai `git reset --hard` pada perubahan lokal yang belum
di-commit. `git checkout` lebih aman.

## 4. Prosedur Tag Rilis

Jalankan setelah build dan pengujian selesai:

```bash
# 1. Pastikan versi di src/config.h dan dokumentasi sudah v1.3.0
grep FW_VERSI src/config.h

# 2. Commit seluruh perubahan
git add -A
git commit -m "v1.3.0: OTA dashboard, kalibrasi lengkap, src headers"

# 3. Buat annotated tag
git tag -a v1.3.0 -m "v1.3.0: OTA dashboard, kalibrasi lengkap, src headers"

# 4. Push branch dan tag
git push origin main
git push origin v1.3.0
```

Cek hasil:

```bash
git describe --tags --always
git show v1.3.0 --stat
```

## 5. Checklist Rilis

- [ ] `FW_VERSI` di `config.h` sesuai versi.
- [ ] `LOG.md` berisi entri versi baru.
- [ ] `RELEASES.md` dan `README.md` menyebut versi baru.
- [ ] Compile bersih dengan FQBN dan library yang didokumentasikan.
- [ ] Tidak ada file build (`build_ota/`, `*.bin`, `*.elf`, `*.map`) ikut commit.
- [ ] `secrets.h` tidak ikut commit; hanya `secrets.example.h` yang dilacak.
- [ ] Tidak ada kredensial produksi di repo publik.
- [ ] Uji minimal: boot, menu, preset, kalibrasi, proses air, E-stop,
      WiFi/NTP/TLS/MQTT, OTA saat IDLE.
- [ ] `git status` bersih setelah commit.
- [ ] Tag dan branch di-push bersama.

## 6. Aturan Maintainer

1. Satu rilis = satu tag `vX.Y.Z`; nomor tidak dipakai ulang.
2. Setiap rilis memperbarui `config.h`, `LOG.md`, `RELEASES.md`, dan
   `README.md` bila fitur berubah.
3. Jangan menyimpan salinan firmware lama di repo; gunakan Git tag.
4. Jangan push build artifact atau file lokal `.commandcode/`.
5. Rilis yang belum diuji hardware harus ditandai jelas di catatan rilis.
