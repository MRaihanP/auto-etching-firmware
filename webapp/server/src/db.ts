import crypto from 'node:crypto';
import Database from 'better-sqlite3';
import { config, ensureRuntimeDirs } from './config.js';

export type Role = 'viewer' | 'operator' | 'admin';

export interface User {
  id: number;
  username: string;
  role: Role;
}

export interface SessionUser extends User {
  csrf: string;
  expiresAt: number;
}

export interface SessionRow {
  token_hash: string;
  user_id: number;
  csrf: string;
  expires_at: number;
}

ensureRuntimeDirs();

export const db = new Database(config.dbPath);
db.pragma('journal_mode = WAL');
db.pragma('foreign_keys = ON');

db.exec(`
  CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    pass_hash TEXT NOT NULL,
    role TEXT NOT NULL CHECK (role IN ('viewer','operator','admin')),
    created_at INTEGER NOT NULL
  );

  CREATE TABLE IF NOT EXISTS sessions (
    token_hash TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    csrf TEXT NOT NULL,
    expires_at INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    ip TEXT,
    ua TEXT
  );

  CREATE TABLE IF NOT EXISTS audit (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts INTEGER NOT NULL,
    user_id INTEGER,
    username TEXT NOT NULL,
    cmd TEXT NOT NULL,
    payload TEXT NOT NULL,
    result TEXT NOT NULL,
    ip TEXT
  );

  CREATE INDEX IF NOT EXISTS idx_sessions_expires ON sessions(expires_at);
  CREATE INDEX IF NOT EXISTS idx_audit_ts ON audit(ts DESC);
`);

const hashToken = (token: string): string =>
  crypto.createHash('sha256').update(token).digest('hex');

export function hashPassword(password: string): string {
  const salt = crypto.randomBytes(16);
  const hash = crypto.scryptSync(password, salt, 64, { N: 16384, r: 8, p: 1 });
  return `scrypt$${salt.toString('hex')}$${hash.toString('hex')}`;
}

export function verifyPassword(password: string, stored: string): boolean {
  const parts = stored.split('$');
  if (parts.length !== 3 || parts[0] !== 'scrypt') return false;
  try {
    const salt = Buffer.from(parts[1], 'hex');
    const expected = Buffer.from(parts[2], 'hex');
    const actual = crypto.scryptSync(password, salt, expected.length, {
      N: 16384,
      r: 8,
      p: 1,
    });
    return actual.length === expected.length && crypto.timingSafeEqual(actual, expected);
  } catch {
    return false;
  }
}

export function findUser(username: string): (User & { pass_hash: string }) | null {
  const row = db
    .prepare('SELECT id, username, role, pass_hash FROM users WHERE username = ?')
    .get(username) as (User & { pass_hash: string }) | undefined;
  return row ?? null;
}

export function createUser(username: string, password: string, role: Role): User {
  const now = Date.now();
  const info = db
    .prepare(
      'INSERT INTO users (username, pass_hash, role, created_at) VALUES (?, ?, ?, ?)',
    )
    .run(username, hashPassword(password), role, now);
  return { id: Number(info.lastInsertRowid), username, role };
}

export function upsertAdmin(username: string, password: string): User {
  const existing = findUser(username);
  if (existing) {
    db.prepare('UPDATE users SET pass_hash = ?, role = ? WHERE id = ?').run(
      hashPassword(password),
      'admin',
      existing.id,
    );
    return { id: existing.id, username: existing.username, role: 'admin' };
  }
  return createUser(username, password, 'admin');
}

export function createSession(
  userId: number,
  ip: string,
  ua: string,
): { token: string; csrf: string; expiresAt: number } {
  const token = crypto.randomBytes(32).toString('base64url');
  const csrf = crypto.randomBytes(24).toString('base64url');
  const now = Date.now();
  const expiresAt = now + config.sessionTtlMs;
  db.prepare(
    'INSERT INTO sessions (token_hash, user_id, csrf, expires_at, created_at, ip, ua) VALUES (?, ?, ?, ?, ?, ?, ?)',
  ).run(hashToken(token), userId, csrf, expiresAt, now, ip, ua.slice(0, 256));
  return { token, csrf, expiresAt };
}

export function getSession(token: string): SessionUser | null {
  if (!token) return null;
  const now = Date.now();
  db.prepare('DELETE FROM sessions WHERE expires_at <= ?').run(now);
  const row = db
    .prepare(
      `SELECT s.csrf, s.expires_at, u.id, u.username, u.role
       FROM sessions s JOIN users u ON u.id = s.user_id
       WHERE s.token_hash = ? AND s.expires_at > ?`,
    )
    .get(hashToken(token), now) as
    | { csrf: string; expires_at: number; id: number; username: string; role: Role }
    | undefined;
  if (!row) return null;
  return {
    id: row.id,
    username: row.username,
    role: row.role,
    csrf: row.csrf,
    expiresAt: row.expires_at,
  };
}

export function deleteSession(token: string): void {
  if (!token) return;
  db.prepare('DELETE FROM sessions WHERE token_hash = ?').run(hashToken(token));
}

export function audit(
  user: SessionUser | null,
  cmd: string,
  payload: unknown,
  result: string,
  ip: string,
): void {
  db.prepare(
    'INSERT INTO audit (ts, user_id, username, cmd, payload, result, ip) VALUES (?, ?, ?, ?, ?, ?, ?)',
  ).run(
    Date.now(),
    user?.id ?? null,
    user?.username ?? 'anonymous',
    cmd,
    JSON.stringify(payload).slice(0, 4096),
    result.slice(0, 512),
    ip,
  );
}

export function listAudit(limit = 100): unknown[] {
  return db
    .prepare(
      'SELECT id, ts, username, cmd, payload, result, ip FROM audit ORDER BY id DESC LIMIT ?',
    )
    .all(Math.min(Math.max(limit, 1), 500));
}

export function countUsers(): number {
  return (db.prepare('SELECT COUNT(*) AS n FROM users').get() as { n: number }).n;
}
