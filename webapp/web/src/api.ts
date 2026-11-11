import type { Snapshot, EventItem, User, WsMessage } from './types';

export interface MeResponse {
  ok: true;
  user: User;
  csrf: string;
}

export class ApiError extends Error {
  status: number;
  constructor(status: number, message: string) {
    super(message);
    this.status = status;
  }
}

let csrfToken = '';

export function setCsrf(token: string): void {
  csrfToken = token;
}

async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
  const headers = new Headers(options.headers);
  if (options.body) headers.set('Content-Type', 'application/json');
  if (csrfToken && options.method && options.method !== 'GET') {
    headers.set('X-CSRF-Token', csrfToken);
  }
  const response = await fetch(path, {
    ...options,
    headers,
    credentials: 'include',
  });
  const text = await response.text();
  let body: unknown = null;
  try {
    body = text ? JSON.parse(text) : null;
  } catch {
    body = { error: text };
  }
  if (!response.ok) {
    const message =
      body && typeof body === 'object' && 'error' in body
        ? String((body as { error: unknown }).error)
        : `HTTP ${response.status}`;
    throw new ApiError(response.status, message);
  }
  return body as T;
}

export async function login(username: string, password: string): Promise<MeResponse> {
  const result = await request<MeResponse>('/api/auth/login', {
    method: 'POST',
    body: JSON.stringify({ username, password }),
  });
  setCsrf(result.csrf);
  return result;
}

export async function me(): Promise<MeResponse> {
  const result = await request<MeResponse>('/api/me');
  setCsrf(result.csrf);
  return result;
}

export async function logout(): Promise<void> {
  await request('/api/auth/logout', { method: 'POST' });
  setCsrf('');
}

export async function fetchStatus(): Promise<Snapshot> {
  const result = await request<{ ok: true } & Snapshot>('/api/status');
  const { ok: _ok, ...snapshot } = result;
  void _ok;
  return snapshot;
}

export async function fetchEvents(): Promise<EventItem[]> {
  const result = await request<{ ok: true; events: EventItem[] }>('/api/events');
  return result.events;
}

export async function sendCommand(cmd: string, preset?: number): Promise<void> {
  await request('/api/commands', {
    method: 'POST',
    body: JSON.stringify(preset === undefined ? { cmd } : { cmd, preset }),
  });
}

export async function sendConfig(body: Record<string, unknown>): Promise<void> {
  await request('/api/config', {
    method: 'POST',
    body: JSON.stringify(body),
  });
}

export async function fetchAudit(): Promise<unknown[]> {
  const result = await request<{ ok: true; audit: unknown[] }>('/api/audit');
  return result.audit;
}

export async function uploadOta(file: File): Promise<{ bytes: number; message: string; ip: string }> {
  const headers = new Headers({
    'Content-Type': 'application/octet-stream',
    'X-OTA-Filename': file.name,
  });
  if (csrfToken) headers.set('X-CSRF-Token', csrfToken);
  const response = await fetch('/api/ota', {
    method: 'POST',
    headers,
    credentials: 'include',
    body: file,
  });
  const text = await response.text();
  let body: unknown = null;
  try {
    body = text ? JSON.parse(text) : null;
  } catch {
    body = { error: text };
  }
  if (!response.ok) {
    const message =
      body && typeof body === 'object' && 'error' in body
        ? String((body as { error: unknown }).error)
        : `HTTP ${response.status}`;
    throw new ApiError(response.status, message);
  }
  return body as { bytes: number; message: string; ip: string };
}

export function connectWs(
  onMessage: (message: WsMessage) => void,
  onState?: (state: 'connecting' | 'open' | 'closed') => void,
): () => void {
  let closed = false;
  let socket: WebSocket | null = null;
  let retry: number | undefined;

  const open = () => {
    if (closed) return;
    onState?.('connecting');
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    socket = new WebSocket(`${protocol}//${window.location.host}/ws`);
    socket.onopen = () => onState?.('open');
    socket.onmessage = (event) => {
      try {
        onMessage(JSON.parse(String(event.data)) as WsMessage);
      } catch {
        // Abaikan frame yang tidak valid.
      }
    };
    socket.onclose = () => {
      onState?.('closed');
      if (!closed) retry = window.setTimeout(open, 2500);
    };
  };

  open();
  return () => {
    closed = true;
    if (retry) window.clearTimeout(retry);
    socket?.close();
  };
}
