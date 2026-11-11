import crypto from 'node:crypto';
import mqtt, { type MqttClient } from 'mqtt';
import { config } from './config.js';

export interface EventItem {
  t: number;
  epoch: number;
  kode: number;
  detail: string;
  deviceId: string;
}

export interface Ack {
  id: string;
  ok: boolean;
  cmd: string;
  msg: string;
}

export interface DeviceSnapshot {
  deviceId: string | null;
  status: Record<string, unknown> | null;
  presets: Record<string, unknown>[];
  online: boolean;
  stale: boolean;
  brokerConnected: boolean;
  lastAt: number | null;
  events: EventItem[];
}

export type WsMessage =
  | { type: 'hello'; data: DeviceSnapshot }
  | { type: 'snapshot'; data: DeviceSnapshot }
  | { type: 'telemetry'; data: DeviceSnapshot }
  | { type: 'event'; data: EventItem }
  | { type: 'ack'; data: Ack }
  | { type: 'connection'; data: { brokerConnected: boolean } };

interface DeviceState {
  status: Record<string, unknown> | null;
  presets: Record<string, unknown>[];
  statusAt: number;
  lastAt: number;
  online: boolean;
  events: EventItem[];
}

const STATUS_KEYS = [
  'ok',
  'cmd',
  'ver',
  'uptime',
  'epoch',
  'reset',
  'fase',
  'fase_nama',
  'langkah',
  'langkah_nama',
  'siklus',
  'siklus_total',
  'preset',
  'preset_nama',
  'volume_ml',
  'jalan',
  'jeda',
  'tunggu_pcb',
  'konfirmasi',
  'pemulihan',
  'ota_aktif',
  'ota_hasil',
  'safety',
  'dosis',
  'suhu',
  'suhu_valid',
  'level',
  'tank',
  'kalib',
  'heap',
  'heap_min',
] as const;

function parseJson(payload: Buffer): unknown {
  try {
    return JSON.parse(payload.toString('utf8'));
  } catch {
    return null;
  }
}

function sanitizeStatus(value: unknown): Record<string, unknown> | null {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return null;
  const source = value as Record<string, unknown>;
  const out: Record<string, unknown> = {};
  for (const key of STATUS_KEYS) {
    if (key in source) out[key] = source[key];
  }
  try {
    if (JSON.stringify(out).length > 16_384) return null;
  } catch {
    return null;
  }
  return out;
}

function sanitizeEvent(value: unknown, deviceId: string): EventItem | null {
  if (!value || typeof value !== 'object') return null;
  const source = value as Record<string, unknown>;
  return {
    t: Number(source.t) || 0,
    epoch: Number(source.epoch) || 0,
    kode: Number(source.kode) || 0,
    detail: String(source.detail ?? '').slice(0, 300),
    deviceId,
  };
}

export class MqttBridge {
  private client: MqttClient | null = null;
  private devices = new Map<string, DeviceState>();
  private active: string | null = config.deviceId || null;
  private listeners = new Set<(message: WsMessage) => void>();
  private pending = new Map<
    string,
    {
      resolve: (ack: Ack) => void;
      reject: (error: Error) => void;
      timer: NodeJS.Timeout;
    }
  >();

  get brokerConnected(): boolean {
    return Boolean(this.client?.connected);
  }

  start(): void {
    if (this.client) return;
    if (!config.mqttUser || !config.mqttPass) {
      console.warn('[mqtt] MQTT_USER/MQTT_PASS belum diisi; dashboard berjalan offline');
      return;
    }

    const options: mqtt.IClientOptions = {
      username: config.mqttUser,
      password: config.mqttPass,
      clientId: `dashboard-${crypto.randomUUID()}`,
      clean: true,
      protocolVersion: 4,
      reconnectPeriod: 5000,
      connectTimeout: 10_000,
      rejectUnauthorized: true,
      ca: config.mqttCa,
    };

    this.client = mqtt.connect(config.mqttUrl, options);

    this.client.on('connect', () => {
      console.log('[mqtt] terhubung ke broker');
      const topics = this.topics();
      this.client?.subscribe(topics, { qos: 0 }, (error) => {
        if (error) console.error('[mqtt] subscribe gagal:', error.message);
      });
      this.emit({ type: 'connection', data: { brokerConnected: true } });
    });

    this.client.on('close', () => {
      console.warn('[mqtt] koneksi broker tertutup');
      this.emit({ type: 'connection', data: { brokerConnected: false } });
    });

    this.client.on('error', (error) => {
      console.error('[mqtt] error:', error.message);
    });

    this.client.on('message', (topic, payload) => this.onMessage(topic, payload));
  }

  private topics(): string[] {
    const suffix = ['status', 'telemetry', 'event', 'ack', 'online', 'presets'];
    if (config.deviceId) {
      return suffix.map((kind) => `${config.mqttBase}/${config.deviceId}/${kind}`);
    }
    return suffix.map((kind) => `${config.mqttBase}/+/${kind}`);
  }

  private state(deviceId: string): DeviceState {
    let state = this.devices.get(deviceId);
    if (!state) {
      state = {
        status: null,
        presets: [],
        statusAt: 0,
        lastAt: 0,
        online: false,
        events: [],
      };
      this.devices.set(deviceId, state);
      if (!this.active) this.active = deviceId;
    }
    return state;
  }

  private onMessage(topic: string, payload: Buffer): void {
    const prefix = `${config.mqttBase}/`;
    if (!topic.startsWith(prefix)) return;
    const parts = topic.slice(prefix.length).split('/');
    if (parts.length !== 2) return;
    const [deviceId, kind] = parts;
    if (config.deviceId && deviceId !== config.deviceId) return;

    const state = this.state(deviceId);
    const now = Date.now();
    state.lastAt = now;

    if (kind === 'status' || kind === 'telemetry') {
      const status = sanitizeStatus(parseJson(payload));
      if (!status) return;
      state.status = status;
      state.statusAt = now;
      // Online hanya dari topik LWT `online`, bukan dari status retained.
      // Status retained bisa berasal dari sesi lama saat alat sudah mati.
      this.active = deviceId;
      this.emit({
        type: kind === 'status' ? 'snapshot' : 'telemetry',
        data: this.snapshot(deviceId),
      });
      return;
    }

    if (kind === 'online') {
      state.online = payload.toString().trim() === '1';
      this.emit({ type: 'snapshot', data: this.snapshot(deviceId) });
      return;
    }

    if (kind === 'presets') {
      const value = parseJson(payload);
      if (value && typeof value === 'object' && !Array.isArray(value)) {
        const list = (value as { presets?: unknown }).presets;
        if (Array.isArray(list)) {
          state.presets = list
            .filter((item): item is Record<string, unknown> =>
              Boolean(item) && typeof item === 'object' && !Array.isArray(item))
            .slice(0, 10);
          this.emit({ type: 'snapshot', data: this.snapshot(deviceId) });
        }
      }
      return;
    }

    if (kind === 'event') {
      const event = sanitizeEvent(parseJson(payload), deviceId);
      if (!event) return;
      state.events.push(event);
      if (state.events.length > 100) state.events.shift();
      this.emit({ type: 'event', data: event });
      return;
    }

    if (kind === 'ack') {
      const value = parseJson(payload);
      if (!value || typeof value !== 'object') return;
      const source = value as Record<string, unknown>;
      const id = String(source.id ?? '');
      if (!id) return;
      const ack: Ack = {
        id,
        ok: Boolean(source.ok),
        cmd: String(source.cmd ?? ''),
        msg: String(source.msg ?? ''),
      };
      this.emit({ type: 'ack', data: ack });
      const waiter = this.pending.get(id);
      if (waiter) {
        clearTimeout(waiter.timer);
        this.pending.delete(id);
        waiter.resolve(ack);
      }
    }
  }

  snapshot(deviceId?: string): DeviceSnapshot {
    const target = deviceId || this.active || config.deviceId || null;
    const state = target ? this.devices.get(target) : undefined;
    const now = Date.now();
    const stale = !state?.status || !state.online || now - state.statusAt > config.staleMs;
    return {
      deviceId: target,
      status: state?.status ?? null,
      presets: state?.presets ? [...state.presets] : [],
      online: Boolean(state?.online),
      stale,
      brokerConnected: this.brokerConnected,
      lastAt: state?.lastAt ?? null,
      events: state?.events ? [...state.events] : [],
    };
  }

  async command(cmd: string, extra: Record<string, unknown> = {}): Promise<Ack> {
    if (!this.client?.connected) throw new Error('broker MQTT belum terhubung');
    const target = this.active || config.deviceId;
    if (!target) throw new Error('device belum terdeteksi');

    const id = crypto.randomUUID();
    const payload = JSON.stringify({ id, cmd, ...extra });
    const topic = `${config.mqttBase}/${target}/cmd`;

    const ackPromise = new Promise<Ack>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error('ACK timeout'));
      }, config.commandTimeoutMs);
      this.pending.set(id, { resolve, reject, timer });
    });

    try {
      await new Promise<void>((resolve, reject) => {
        const timer = setTimeout(() => reject(new Error('publish timeout')), config.publishTimeoutMs);
        this.client?.publish(topic, payload, { qos: 1 }, (error) => {
          clearTimeout(timer);
          if (error) reject(error);
          else resolve();
        });
      });
    } catch (error) {
      const waiter = this.pending.get(id);
      if (waiter) {
        clearTimeout(waiter.timer);
        this.pending.delete(id);
      }
      throw error;
    }

    return ackPromise;
  }

  subscribe(listener: (message: WsMessage) => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private emit(message: WsMessage): void {
    for (const listener of this.listeners) {
      try {
        listener(message);
      } catch (error) {
        console.error('[ws] listener error:', error);
      }
    }
  }

  close(): void {
    this.client?.end(true);
    this.client = null;
  }
}
