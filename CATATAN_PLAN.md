# CATATAN PLAN PENGEMBANGAN

Plan pengembangan mesin etching PCB otomatis (dosing HCl/H2O2/H2O via
DRV8825 1/36 step, agitasi/drain/fan via TB6612FNG, sensor level GPIO)
tersimpan di:

  /home/raihanP/.opencode/plan/etching-otomatis.md

Ringkas:
- Alur: START (OLED) → DOSING → AGITASI → DRAINING → RINSING → SELESAI
- Sensor level pindah dari MPR121 ke GPIO 11/12/13/14/3
- TB6612FNG: agitasi=GPIO15, pompa sabun=GPIO16, pompa limbah=GPIO17, fan=GPIO18
- Fan GPIO14 lama dihapus; mikrostep DRV8825 1/36
- Implementasi bertahap: config.h → sensor.h → motor.h/dcmotor.h → process.h →
  kalibrasi → keamanan (E-stop, timeout) → uji air

Status: v0.5.0 — Tahap A (config + sensor GPIO), Tahap B (motor.h, dcmotor.h),
Tahap C (process.h aktuator nyata), E-stop, dan header versi sudah diterapkan.
Build PlatformIO (esp32dev) SUKSES. Belum: kalibrasi steps/ml, halaman
Kalibrasi, watchdog timeout per fase, dan uji end-to-end.
