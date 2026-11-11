# Panduan Repository

Panduan untuk pemilik repo dan contributor. Semua perubahan masuk melalui
Pull Request; `main` harus selalu dapat di-compile.

## 1. Struktur Repo

```text
auto-etching-firmware/
├── auto-etching-firmware.ino
├── config.h
├── data.h
├── dbg.h
├── input.h
├── presetops.h
├── sensor.h
├── storage.h
├── motor.h
├── dcmotor.h
├── buzzer.h
├── safety.h
├── process.h
├── ui.h
├── api.h
├── net.h
├── net_ca.h
├── ota.h
├── fontmini.h
├── partitions.csv
├── secrets.example.h
├── secrets.h              # lokal, di-ignore Git
├── webapp/                # dashboard self-hosted (server + web)
├── README.md
├── DOKUMENTASI.md
├── IOT-API.md
├── PANDUAN-TEST-IOT.md
├── LOG.md
├── RELEASES.md
└── GUIDE.md
```

Tidak ada folder arsip atau sketch pilot di repo. Versi lama diambil dari
Git tag.

## 2. Prasyarat Development

- Arduino CLI / Arduino IDE dengan core ESP32 3.3.x.
- Library: Adafruit GFX, Adafruit SSD1306, OneWire, DallasTemperature,
  FastAccelStepper, ESP32MQTTClient, ArduinoJson.
- Board: ESP32S3 Dev Module, `CDCOnBoot=cdc`, `PartitionScheme=min_spiffs`.
- Kredensial: salin `secrets.example.h` menjadi `secrets.h` di mesin lokal
  dan isi nilainya. `secrets.h` di-ignore Git dan tidak boleh di-commit.

Compile:

```bash
~/DevBox/bin/arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=min_spiffs" \
  --libraries /home/raihanP/Arduino/libraries \
  --output-dir build_ota .
```

## 3. Workflow Contributor

1. Fork repository dan clone.
2. Buat branch: `fix/...`, `feature/...`, `docs/...`, atau `refactor/...`.
3. Edit kode dan dokumentasi terkait.
4. Compile sebelum commit.
5. Commit dengan pesan jelas, lalu push ke fork.
6. Buka Pull Request ke `main`.

Aturan:

- Jangan push langsung ke `main`.
- Jangan commit `build_ota/`, `*.bin`, `*.elf`, `*.map`, atau `.commandcode/`.
- Perubahan yang mengubah perilaku harus memperbarui `LOG.md`,
  `DOKUMENTASI.md`, dan/atau `IOT-API.md`.
- Perubahan hardware/pin harus memperbarui `README.md` dan
  `DOKUMENTASI.md`.
- Jangan menambahkan kredensial produksi ke repo publik.

## 4. Review Pull Request

- [ ] Deskripsi dan tujuan jelas.
- [ ] Compile bersih dengan FQBN yang didokumentasikan.
- [ ] Tidak ada debug sementara atau kode mati.
- [ ] Tidak ada akses aktuator langsung dari callback jaringan.
- [ ] Error/safety tetap ditangani di `safety.h`/`process.h`.
- [ ] Dokumentasi dan versi diperbarui bila perlu.
- [ ] Tidak ada file build/rahasia yang ikut.

## 5. Rilis

Prosedur lengkap ada di [`RELEASES.md`](./RELEASES.md). Ringkasnya:

1. Bump `FW_VERSI` di `config.h`.
2. Update `LOG.md`, `RELEASES.md`, dan `README.md`.
3. Compile dan uji minimal (boot, menu, proses air, E-stop, WiFi/MQTT, OTA).
4. Commit.
5. Buat annotated tag `vX.Y.Z`.
6. Push branch dan tag.

## 6. Dokumentasi

| Dokumen | Tanggung jawab |
|---|---|
| `README.md` | Ringkasan fitur, struktur, build cepat |
| `DOKUMENTASI.md` | Manual pengguna, kalibrasi, error, NVS |
| `IOT-API.md` | Serial/MQTT, topik, payload, remote config |
| `PANDUAN-TEST-IOT.md` | Prosedur uji jaringan dan OTA |
| `LOG.md` | Changelog per rilis |
| `RELEASES.md` | Registry versi, rollback, tag |
| `GUIDE.md` | Alur kerja repo |

## 7. Catatan Keamanan

- E-stop dan clear fault wajib bekerja lokal.
- Jangan menguji dengan HCl/H2O2 tanpa ventilasi dan APD.
- Uji dengan air sebelum larutan asli.
- Kredensial WiFi/MQTT/OTA ada di `secrets.h` lokal yang di-ignore Git.
  Jangan commit file itu atau build yang memuat kredensial asli.
