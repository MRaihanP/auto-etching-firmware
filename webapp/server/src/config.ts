import 'dotenv/config';
import path from 'node:path';
import fs from 'node:fs';

const cwd = process.cwd();

function env(name: string, fallback = ''): string {
  return process.env[name] ?? fallback;
}

function bool(name: string, fallback: boolean): boolean {
  const value = process.env[name];
  if (value === undefined) return fallback;
  return value === '1' || value.toLowerCase() === 'true';
}

function num(name: string, fallback: number): number {
  const value = Number(process.env[name]);
  return Number.isFinite(value) ? value : fallback;
}

function resolvePath(value: string, fallback: string): string {
  const target = value || fallback;
  return path.isAbsolute(target) ? target : path.resolve(cwd, target);
}

const caPath = env('MQTT_CA_PATH');
let mqttCa: Buffer | undefined;

if (caPath) {
  const resolved = resolvePath(caPath, caPath);
  if (fs.existsSync(resolved)) mqttCa = fs.readFileSync(resolved);
}

export const config = {
  host: env('HOST', '0.0.0.0'),
  port: num('PORT', 8080),
  cookieSecure: bool('COOKIE_SECURE', false),
  trustProxy: bool('TRUST_PROXY', false),
  publicOrigin: env('PUBLIC_ORIGIN', 'http://localhost:8080'),
  webDist: resolvePath(env('WEB_DIST'), '../web/dist'),
  dbPath: resolvePath(env('DB_PATH'), 'data/dashboard.db'),
  sessionTtlMs: num('SESSION_TTL_HOURS', 12) * 60 * 60 * 1000,
  mqttUrl: env('MQTT_URL', 'mqtts://mqtt.loggerindonesia.com:8883'),
  mqttUser: env('MQTT_USER'),
  mqttPass: env('MQTT_PASS'),
  mqttBase: env('MQTT_BASE', 'pelarut').replace(/\/+$/, ''),
  deviceId: env('DEVICE_ID'),
  mqttCa,
  otaPort: num('OTA_PORT', 3232),
  otaPass: env('OTA_PASS'),
  otaMaxBytes: num('OTA_MAX_MB', 3) * 1024 * 1024,
  otaTimeoutMs: num('OTA_TIMEOUT_MS', 30_000),
  adminUser: env('ADMIN_USER', 'admin'),
  adminPassword: env('ADMIN_PASSWORD'),
  staleMs: 15_000,
  commandTimeoutMs: 5_000,
  publishTimeoutMs: 2_000,
  commandMinIntervalMs: 200,
} as const;

export function ensureRuntimeDirs(): void {
  fs.mkdirSync(path.dirname(config.dbPath), { recursive: true });
}
