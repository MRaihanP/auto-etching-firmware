# Panduan: Setup Git Repo & Push GitHub (Checkpoint)

Panduan ini untuk membuat repository GitHub dari project firmware ini sebagai checkpoint.

## Prasyarat

- Git terinstall (`git --version`)
- GitHub CLI (`gh`) terinstall dan sudah login, ATAU
- Akun GitHub dengan personal access token

## Langkah 1: Inisialisasi Git Repo

```bash
cd /home/raihanP/DevBox/Projects/firmware/arduino/oled_pcb_baruuu
git init
git add .
git commit -m "v0.8.3 — Fix critical bugs, add TONE_MULAI & TONE_PROSES_SELESAI, non-blocking buzzer"
```

## Langkah 2: Buat .gitignore

File `.gitignore` sudah dibuat. Pastikan isinya:

```
# Arduino / PlatformIO
.build/
.pio/
*.bin
*.elf

# OS files
.DS_Store
Thumbs.db

# IDE
.vscode/
.idea/
```

## Langkah 3: Push ke GitHub

### Opsi A: GitHub CLI (recommended)

```bash
# Buat repo baru (public atau private)
gh repo create oled-pcb-etching-firmware --public --source=. --push

# Atau jika repo sudah ada:
git remote add origin https://github.com/<username>/oled-pcb-etching-firmware.git
git branch -M main
git push -u origin main
```

### Opsi B: Git Manual

```bash
# Buat repo di GitHub web UI terlebih dahulu, lalu:
git remote add origin https://github.com/<username>/oled-pcb-etching-firmware.git
git branch -M main
git push -u origin main
```

## Langkah 4: Verifikasi

```bash
git log --oneline -5
git status
```

## Checklist Sebelum Push

- [ ] Semua bug kritis sudah diperbaiki
- [ ] Tone baru (`TONE_MULAI`, `TONE_PROSES_SELESAI`) sudah ditambahkan
- [ ] Buzzer sudah non-blocking
- [ ] Version bump di `config.h` sudah benar
- [ ] `SUMMARY.md` dan `LOG.md` sudah update
- [ ] Test compile di Arduino IDE / PlatformIO
- [ ] Commit message jelas dan deskriptif

## Struktur Commit yang Baik

```
v0.8.3 — Fix critical bugs, add TONE_MULAI & TONE_PROSES_SELESAI, non-blocking buzzer

- Fix: agitator tidak resume setelah pause (process.h)
- Fix: pause tidak extend fase duration (process.h)
- Fix: blocking delay(2000) di kalibrasi agitasi → non-blocking
- Fix: waste estimate salah (mlH2O dihitung 3x)
- Fix: cek sabun pakai nilai salah (mlH2O → 100ml tetap)
- Fix: E-stop ISR memanggil digitalRead → ISR minimal
- Fix: hitungMl division by zero
- Fix: stepperTest blocking → non-blocking
- Add: TONE_MULAI (mulai proses) & TONE_PROSES_SELESAI (selesai proses)
- Refactor: buzzer non-blocking state machine
- Bump: v0.8.2 → v0.8.3
```
