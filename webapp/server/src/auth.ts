import type { FastifyReply, FastifyRequest } from 'fastify';
import { config } from './config.js';
import { getSession, type Role, type SessionUser } from './db.js';

declare module 'fastify' {
  interface FastifyRequest {
    user?: SessionUser;
  }
}

const roleRank: Record<Role, number> = {
  viewer: 1,
  operator: 2,
  admin: 3,
};

export function getSessionFromRequest(req: FastifyRequest): SessionUser | null {
  const token = req.cookies?.sid;
  return token ? getSession(token) : null;
}

export function requireAuth(roles: Role[] = ['viewer', 'operator', 'admin']) {
  return async (req: FastifyRequest, reply: FastifyReply): Promise<void> => {
    const user = getSessionFromRequest(req);
    if (!user) {
      await reply.code(401).send({ ok: false, error: 'login diperlukan' });
      return;
    }
    if (!roles.some((role) => roleRank[user.role] >= roleRank[role])) {
      await reply.code(403).send({ ok: false, error: 'role tidak diizinkan' });
      return;
    }
    req.user = user;
  };
}

export async function requireCsrf(req: FastifyRequest, reply: FastifyReply): Promise<void> {
  const header = req.headers['x-csrf-token'];
  if (!req.user || typeof header !== 'string' || header !== req.user.csrf) {
    await reply.code(403).send({ ok: false, error: 'CSRF token tidak valid' });
    return;
  }
}

export function setSessionCookie(reply: FastifyReply, token: string, expiresAt: number): void {
  reply.setCookie('sid', token, {
    path: '/',
    httpOnly: true,
    secure: config.cookieSecure,
    sameSite: 'strict',
    expires: new Date(expiresAt),
  });
}

export function clearSessionCookie(reply: FastifyReply): void {
  reply.clearCookie('sid', {
    path: '/',
    httpOnly: true,
    secure: config.cookieSecure,
    sameSite: 'strict',
  });
}
