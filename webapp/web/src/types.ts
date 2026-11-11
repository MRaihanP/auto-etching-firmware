export type Role = 'viewer' | 'operator' | 'admin';

export interface User {
  id: number;
  username: string;
  role: Role;
}

export interface Safety {
  state?: number;
  kode?: number;
  nama?: string;
  detail?: string;
  aksi?: string;
  pin_high?: boolean;
  clear_remote?: boolean;
}

export interface Preset {
  idx: number;
  nama: string;
  pcbPanjang?: number;
  pcbLebar?: number;
  modeRasio?: number;
  persenHCl?: number;
  persenH2O2?: number;
  persenH2O?: number;
  mlHCl?: number;
  mlH2O2?: number;
  mlH2O?: number;
  volume?: number;
  isiTembagaPct?: number;
  tebalTembagaUm?: number;
  lajuEtchX10?: number;
  modeWaktu?: number;
  agitasiDetik?: number;
  agitasiSpeed?: number;
  siklus?: number;
  bilasSabun?: number;
  bilasAir?: number;
  bilasMl?: number;
  bilasDetik?: number;
  konfirmasiSiklus?: number;
}

export interface LevelItem {
  p?: number;
  basah?: boolean;
  fault?: boolean;
}

export interface TankItem {
  ml?: number;
  kap?: number;
}

export interface Dosing {
  nama?: string;
  target?: number;
  sudah?: number;
}

export interface Status {
  ver?: string;
  build?: string;
  ip?: string;
  rssi?: number;
  uptime?: number;
  epoch?: number;
  reset?: string;
  fase?: number;
  fase_nama?: string;
  langkah?: number;
  langkah_nama?: string;
  siklus?: number;
  siklus_total?: number;
  preset?: number;
  preset_nama?: string;
  volume_ml?: number;
  jalan?: boolean;
  jeda?: boolean;
  tunggu_pcb?: boolean;
  konfirmasi?: boolean;
  pemulihan?: boolean;
  ota_aktif?: boolean;
  ota_hasil?: number;
  safety?: Safety;
  dosis?: Dosing;
  suhu?: number[];
  suhu_valid?: boolean[];
  level?: LevelItem[];
  tank?: TankItem[];
  kalib?: {
    stepper?: boolean[];
    dc?: number[];
    ml_per_rev?: number[];
    rpm?: number[];
    stepper_epoch?: number[];
    dc_duty?: number[];
    dc_epoch?: number[];
    agitasi_epoch?: number;
    agitasi_duty?: number[];
    level?: Array<{ kosong?: number; penuh?: number; ok?: boolean }>;
    suhu?: Array<{ addr?: number; offset_x10?: number }>;
    mq?: { normal?: number; kotor?: number; aktif?: number };
    level_epoch?: number;
    suhu_epoch?: number;
    mq_epoch?: number;
  };
  heap?: number;
  heap_min?: number;
}

export interface EventItem {
  t: number;
  epoch: number;
  kode: number;
  detail: string;
  deviceId: string;
}

export interface Snapshot {
  deviceId: string | null;
  status: Status | null;
  presets: Preset[];
  online: boolean;
  stale: boolean;
  brokerConnected: boolean;
  lastAt: number | null;
  events: EventItem[];
}

export interface Ack {
  id: string;
  ok: boolean;
  cmd: string;
  msg: string;
}

export type WsMessage =
  | { type: 'hello'; data: Snapshot }
  | { type: 'snapshot'; data: Snapshot }
  | { type: 'telemetry'; data: Snapshot }
  | { type: 'event'; data: EventItem }
  | { type: 'ack'; data: Ack }
  | { type: 'connection'; data: { brokerConnected: boolean } };
