import Fastify from 'fastify';
import cookie from '@fastify/cookie';
import helmet from '@fastify/helmet';
import rateLimit from '@fastify/rate-limit';
import fastifyStatic from '@fastify/static';
import websocket from '@fastify/websocket';
import { z } from 'zod';
import { config } from './config.js';
import {
  audit,
  countUsers,
  createSession,
  deleteSession,
  findUser,
  getSession,
  listAudit,
  verifyPassword,
} from './db.js';
import { clearSessionCookie, requireAuth, requireCsrf, setSessionCookie } from './auth.js';
import { MqttBridge } from './mqtt.js';

const app = Fastify({
  logger: {
    level: process.env.LOG_LEVEL ?? 'info',
    redact: {
      paths: ['req.headers.cookie', 'req.headers.authorization', 'req.body.password'],
      censor: '[redacted]',
    },
  },
  trustProxy: config.trustProxy,
  bodyLimit: 64 * 1024,
});

const bridge = new MqttBridge();
const lastCommandAt = new Map<number, number>();

await app.register(cookie);
await app.register(helmet, {
  contentSecurityPolicy: {
    directives: {
      defaultSrc: ["'self'"],
      scriptSrc: ["'self'"],
      styleSrc: ["'self'", "'unsafe-inline'"],
      imgSrc: ["'self'", 'data:'],
      connectSrc: ["'self'", 'ws:', 'wss:'],
      fontSrc: ["'self'"],
      objectSrc: ["'none'"],
      baseUri: ["'self'"],
      frameAncestors: ["'none'"],
    },
  },
});
await app.register(rateLimit, { global: false });
await app.register(websocket);
await app.register(fastifyStatic, {
  root: config.webDist,
  prefix: '/',
  wildcard: false,
});

const loginSchema = z.object({
  username: z.string().min(1).max(64),
  password: z.string().min(1).max(256),
});

const commandSchema = z.object({
  cmd: z.enum(['start', 'pause', 'resume', 'cancel', 'drain', 'status', 'preset', 'pcb', 'clear']),
  preset: z.number().int().min(0).max(9).optional(),
});

const configSchema = z.object({
  target: z.enum(['tank', 'setting', 'preset']),
  idx: z.number().int().min(0).max(9).optional(),
  ml: z.number().int().min(0).max(100_000).optional(),
  name: z.string().min(1).max(64).optional(),
  value: z.union([z.number(), z.string(), z.boolean()]).optional(),
  fields: z.record(z.unknown()).optional(),
});

const settingNames = new Set([
  'tempPeringatanX10',
  'tempBahayaX10',
  'drainMarginPct',
  'levelStabilMs',
  'mqAktif',
  'iotEnabled',
  'kapHclMl',
  'kapH2o2Ml',
  'kapH2oMl',
  'kapSabunMl',
  'kapLimbahMl',
]);

function isIdleSafe(status: Record<string, unknown> | null): boolean {
  if (!status) return false;
  const safety = status.safety as { state?: number } | undefined;
  return status.jalan === false && status.jeda === false &&
    status.tunggu_pcb === false && (safety?.state ?? 0) === 0;
}

function clientIp(req: { ip: string }): string {
  return req.ip || 'unknown';
}

app.get('/api/health', async () => ({
  ok: true,
  mqtt: bridge.brokerConnected,
  uptime: Math.round(process.uptime()),
}));

app.post(
  '/api/auth/login',
  { config: { rateLimit: { max: 5, timeWindow: '1 minute' } } },
  async (req, reply) => {
    const parsed = loginSchema.safeParse(req.body);
    if (!parsed.success) {
      return reply.code(400).send({ ok: false, error: 'username/password tidak valid' });
    }
    const user = findUser(parsed.data.username.trim());
    if (!user || !verifyPassword(parsed.data.password, user.pass_hash)) {
      audit(null, 'login', { username: parsed.data.username }, 'gagal', clientIp(req));
      return reply.code(401).send({ ok: false, error: 'username/password salah' });
    }
    const session = createSession(
      user.id,
      clientIp(req),
      String(req.headers['user-agent'] ?? ''),
    );
    setSessionCookie(reply, session.token, session.expiresAt);
    audit(
      { id: user.id, username: user.username, role: user.role, csrf: session.csrf, expiresAt: session.expiresAt },
      'login',
      {},
      'ok',
      clientIp(req),
    );
    return {
      ok: true,
      user: { id: user.id, username: user.username, role: user.role },
      csrf: session.csrf,
    };
  },
);

app.post(
  '/api/auth/logout',
  { preHandler: [requireAuth(), requireCsrf] },
  async (req, reply) => {
    const token = req.cookies?.sid ?? '';
    deleteSession(token);
    clearSessionCookie(reply);
    audit(req.user ?? null, 'logout', {}, 'ok', clientIp(req));
    return { ok: true };
  },
);

app.get('/api/me', { preHandler: requireAuth() }, async (req) => ({
  ok: true,
  user: {
    id: req.user!.id,
    username: req.user!.username,
    role: req.user!.role,
  },
  csrf: req.user!.csrf,
}));

app.get('/api/status', { preHandler: requireAuth() }, async (req) => {
  const device = (req.query as { device?: string })?.device;
  return { ok: true, ...bridge.snapshot(device) };
});

app.get('/api/events', { preHandler: requireAuth() }, async (req) => {
  const device = (req.query as { device?: string })?.device;
  const snapshot = bridge.snapshot(device);
  return { ok: true, deviceId: snapshot.deviceId, events: snapshot.events };
});

app.get(
  '/api/audit',
  { preHandler: requireAuth(['admin']) },
  async (req) => {
    const limit = Number((req.query as { limit?: string })?.limit ?? 100);
    return { ok: true, audit: listAudit(limit) };
  },
);

app.post(
  '/api/commands',
  { preHandler: [requireAuth(['operator', 'admin']), requireCsrf] },
  async (req, reply) => {
    const parsed = commandSchema.safeParse(req.body);
    if (!parsed.success) {
      return reply.code(400).send({ ok: false, error: 'command tidak valid' });
    }
    const now = Date.now();
    const last = lastCommandAt.get(req.user!.id) ?? 0;
    if (now - last < config.commandMinIntervalMs) {
      return reply.code(429).send({ ok: false, error: 'command terlalu cepat' });
    }
    lastCommandAt.set(req.user!.id, now);

    const snapshot = bridge.snapshot();
    if (snapshot.stale) {
      return reply.code(409).send({ ok: false, error: 'status alat stale/offline' });
    }
    const status = snapshot.status as Record<string, unknown> | null;
    if (
      parsed.data.cmd === 'start' &&
      (status?.jalan === true || status?.jeda === true ||
        status?.tunggu_pcb === true || status?.pemulihan === true)
    ) {
      return reply.code(409).send({ ok: false, error: 'alat tidak IDLE' });
    }
    if (parsed.data.cmd === 'pause' && (status?.jalan !== true || status?.jeda === true)) {
      return reply.code(409).send({ ok: false, error: 'proses tidak sedang berjalan' });
    }
    if (parsed.data.cmd === 'resume' && status?.jeda !== true) {
      return reply.code(409).send({ ok: false, error: 'proses tidak sedang dijeda' });
    }
    if (parsed.data.cmd === 'preset') {
      if (parsed.data.preset === undefined) {
        return reply.code(400).send({ ok: false, error: 'preset index wajib' });
      }
      if (status?.jalan === true || status?.jeda === true) {
        return reply.code(409).send({ ok: false, error: 'preset hanya saat IDLE' });
      }
    }
    if (parsed.data.cmd === 'pcb' && status?.tunggu_pcb !== true) {
      return reply.code(409).send({ ok: false, error: 'alat tidak menunggu PCB' });
    }
    if (parsed.data.cmd === 'clear') {
      const safety = status?.safety as { state?: number; clear_remote?: boolean } | undefined;
      if (safety?.state !== 1 || safety.clear_remote !== true) {
        return reply.code(409).send({
          ok: false,
          error: 'clear hanya setelah E-stop fisik dilepas',
        });
      }
    }

    const extra = parsed.data.preset === undefined ? {} : { preset: parsed.data.preset };
    try {
      const commandPromise = bridge.command(parsed.data.cmd, extra);
      commandPromise.catch(() => undefined);
      const timeout = new Promise<never>((_, reject) =>
        setTimeout(() => reject(new Error('command timeout')), 8000),
      );
      const ack = await Promise.race([commandPromise, timeout]);
      audit(req.user!, `command:${parsed.data.cmd}`, parsed.data, JSON.stringify(ack), clientIp(req));
      return reply.send({ ok: true, ack });
    } catch (error) {
      const message = error instanceof Error ? error.message : 'command gagal';
      audit(req.user!, `command:${parsed.data.cmd}`, parsed.data, message, clientIp(req));
      return reply.code(502).send({ ok: false, error: message });
    }
  },
);

app.post(
  '/api/config',
  { preHandler: [requireAuth(['admin']), requireCsrf] },
  async (req, reply) => {
    const parsed = configSchema.safeParse(req.body);
    if (!parsed.success) {
      return reply.code(400).send({ ok: false, error: 'config tidak valid' });
    }
    const snapshot = bridge.snapshot();
    if (!isIdleSafe(snapshot.status)) {
      return reply.code(409).send({ ok: false, error: 'config hanya saat IDLE dan safety AMAN' });
    }

    const body = parsed.data;
    let payload: Record<string, unknown> | null = null;
    if (body.target === 'tank') {
      if (body.idx === undefined || body.ml === undefined) {
        return reply.code(400).send({ ok: false, error: 'tank butuh idx dan ml' });
      }
      payload = { target: 'tank', idx: body.idx, ml: body.ml };
    } else if (body.target === 'setting') {
      if (!body.name || body.value === undefined || !settingNames.has(body.name)) {
        return reply.code(400).send({ ok: false, error: 'setting tidak diizinkan' });
      }
      payload = { target: 'setting', name: body.name, value: body.value };
    } else {
      if (body.idx === undefined || !body.fields || typeof body.fields !== 'object') {
        return reply.code(400).send({ ok: false, error: 'preset butuh idx dan fields' });
      }
      payload = { target: 'preset', idx: body.idx, fields: body.fields };
    }

    try {
      const ack = await bridge.command('config', { payload });
      audit(req.user!, 'config', body, JSON.stringify(ack), clientIp(req));
      return reply.send({ ok: true, ack });
    } catch (error) {
      const message = error instanceof Error ? error.message : 'config gagal';
      audit(req.user!, 'config', body, message, clientIp(req));
      return reply.code(502).send({ ok: false, error: message });
    }
  },
);

app.get(
  '/ws',
  { websocket: true, preValidation: requireAuth() },
  (socket, req) => {
    const send = (message: unknown) => {
      if (socket.readyState === 1) socket.send(JSON.stringify(message));
    };
    send({ type: 'hello', data: bridge.snapshot() });
    const unsubscribe = bridge.subscribe(send);
    const token = req.cookies?.sid ?? '';
    const expiryTimer = setInterval(() => {
      if (!getSession(token)) socket.close(4001, 'session expired');
    }, 60_000);
    socket.on('close', () => {
      clearInterval(expiryTimer);
      unsubscribe();
    });
    socket.on('error', () => {
      clearInterval(expiryTimer);
      unsubscribe();
    });
    req.log.debug({ user: req.user?.username }, 'websocket connected');
  },
);

app.setNotFoundHandler((req, reply) => {
  if (req.method === 'GET' && !req.url.startsWith('/api') && !req.url.startsWith('/ws')) {
    return reply.sendFile('index.html');
  }
  return reply.code(404).send({ ok: false, error: 'not found' });
});

app.setErrorHandler((error, req, reply) => {
  req.log.error({ err: error }, 'request error');
  const err = error as { statusCode?: number; message?: string };
  const status = err.statusCode && err.statusCode >= 400 ? err.statusCode : 500;
  return reply.code(status).send({
    ok: false,
    error: status === 500 ? 'internal server error' : err.message,
  });
});

bridge.start();
if (countUsers() === 0) {
  app.log.warn('Belum ada user dashboard. Jalankan `pnpm seed` dari folder webapp.');
}

const shutdown = async () => {
  bridge.close();
  await app.close();
  process.exit(0);
};
process.once('SIGINT', shutdown);
process.once('SIGTERM', shutdown);

try {
  await app.listen({ host: config.host, port: config.port });
  app.log.info(`Dashboard: ${config.publicOrigin}`);
} catch (error) {
  app.log.error(error);
  process.exit(1);
}
