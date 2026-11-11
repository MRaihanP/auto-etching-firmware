import type { EventItem, Preset, Snapshot, User } from './types';

const demoPresets: Preset[] = [
  {
    idx: 0,
    nama: 'Preset 1',
    pcbPanjang: 100,
    pcbLebar: 80,
    modeRasio: 0,
    persenHCl: 33,
    persenH2O2: 17,
    persenH2O: 50,
    mlHCl: 120,
    mlH2O2: 60,
    mlH2O: 180,
    volume: 360,
    isiTembagaPct: 40,
    tebalTembagaUm: 35,
    lajuEtchX10: 30,
    modeWaktu: 1,
    agitasiDetik: 300,
    agitasiSpeed: 3,
    siklus: 2,
    bilasSabun: 1,
    bilasAir: 2,
    bilasMl: 100,
    bilasDetik: 20,
    konfirmasiSiklus: 0,
  },
  {
    idx: 1,
    nama: 'Preset 2',
    pcbPanjang: 80,
    pcbLebar: 60,
    modeRasio: 0,
    persenHCl: 40,
    persenH2O2: 20,
    persenH2O: 40,
    mlHCl: 100,
    mlH2O2: 50,
    mlH2O: 100,
    volume: 250,
    isiTembagaPct: 40,
    tebalTembagaUm: 35,
    lajuEtchX10: 30,
    modeWaktu: 1,
    agitasiDetik: 240,
    agitasiSpeed: 3,
    siklus: 3,
    bilasSabun: 1,
    bilasAir: 2,
    bilasMl: 100,
    bilasDetik: 20,
    konfirmasiSiklus: 0,
  },
];

export function isDemoMode(): boolean {
  if (typeof window === 'undefined') return false;
  return (
    new URLSearchParams(window.location.search).has('demo') ||
    import.meta.env.VITE_DEMO === '1'
  );
}

export const demoUser: User = {
  id: 0,
  username: 'demo',
  role: 'admin',
};

export const demoEvents: EventItem[] = [
  {
    t: 420000,
    epoch: 1794387317,
    kode: 100,
    detail: 'Proses dimulai: Preset 1',
    deviceId: 'esp32-demo',
  },
  {
    t: 450000,
    epoch: 1794387347,
    kode: 104,
    detail: 'Drain dimulai: 360 ml + residu 50 ml',
    deviceId: 'esp32-demo',
  },
  {
    t: 480000,
    epoch: 1794387377,
    kode: 106,
    detail: 'Sinkron tangki dari sensor: HCl 92%',
    deviceId: 'esp32-demo',
  },
];

export function demoSnapshot(): Snapshot {
  const now = Date.now();
  return {
    deviceId: 'esp32-demo',
    presets: demoPresets,
    online: true,
    stale: false,
    brokerConnected: true,
    lastAt: now,
    events: demoEvents,
    status: {
      ver: 'v1.2.0',
      uptime: 482_000,
      epoch: 1794387317,
      reset: 'PWRON',
      fase: 3,
      fase_nama: 'ETCH',
      langkah: 5,
      langkah_nama: 'AGITASI',
      siklus: 1,
      siklus_total: 2,
      preset: 0,
      preset_nama: 'Preset 1',
      volume_ml: 360,
      jalan: true,
      jeda: false,
      tunggu_pcb: false,
      konfirmasi: false,
      pemulihan: false,
      ota_aktif: false,
      ota_hasil: 0,
      safety: { state: 0, kode: 0, nama: 'OK', detail: '', aksi: '', pin_high: true, clear_remote: false },
      dosis: { nama: 'HCl', target: 120, sudah: 45 },
      suhu: [31.2, 30.8, 32.1],
      suhu_valid: [true, true, true],
      level: [
        { p: 92, basah: true, fault: false },
        { p: 87, basah: true, fault: false },
        { p: 84, basah: true, fault: false },
        { p: 44, basah: false, fault: false },
        { p: 0, basah: false, fault: false },
      ],
      tank: [
        { ml: 1000, kap: 1000 },
        { ml: 1000, kap: 1000 },
        { ml: 950, kap: 1000 },
        { ml: 600, kap: 600 },
        { ml: 479, kap: 5000 },
      ],
      kalib: {
        stepper: [true, true, true],
        dc: [12.5, 24.0],
        ml_per_rev: [0.52, 0.49, 0.55],
        rpm: [500, 500, 500],
        stepper_epoch: [1794387000, 1794386000, 1794385000],
        dc_duty: [80, 100],
        dc_epoch: [1794384000, 1794383000],
        agitasi_epoch: 1794382000,
      },
      heap: 250_000,
      heap_min: 240_000,
    },
  };
}
