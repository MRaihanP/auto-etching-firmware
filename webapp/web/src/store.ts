import { create } from 'zustand';
import * as api from './api';
import { demoEvents, demoSnapshot, demoUser, isDemoMode } from './demo';
import type { EventItem, Snapshot, User } from './types';

type ConnectionState = 'connecting' | 'open' | 'closed';

interface AppState {
  user: User | null;
  csrf: string;
  snapshot: Snapshot | null;
  events: EventItem[];
  connection: ConnectionState;
  booting: boolean;
  error: string;
  toast: string;
  login: (username: string, password: string) => Promise<void>;
  logout: () => Promise<void>;
  boot: () => Promise<void>;
  refresh: () => Promise<void>;
  setSnapshot: (snapshot: Snapshot) => void;
  setConnection: (connection: ConnectionState) => void;
  addEvent: (event: EventItem) => void;
  setToast: (toast: string) => void;
  clearToast: () => void;
}

const demo = isDemoMode();

export const useApp = create<AppState>((set, get) => ({
  user: demo ? demoUser : null,
  csrf: '',
  snapshot: demo ? demoSnapshot() : null,
  events: demo ? demoEvents : [],
  connection: demo ? 'open' : 'connecting',
  booting: !demo,
  error: '',
  toast: '',

  login: async (username, password) => {
    if (isDemoMode()) {
      set({ user: demoUser, snapshot: demoSnapshot(), events: demoEvents });
      return;
    }
    set({ error: '' });
    try {
      const result = await api.login(username, password);
      set({ user: result.user, csrf: result.csrf });
      await get().refresh();
    } catch (error) {
      set({ error: error instanceof Error ? error.message : 'Login gagal' });
      throw error;
    }
  },

  logout: async () => {
    if (isDemoMode()) {
      set({ toast: 'Mode demo: logout dinonaktifkan.' });
      return;
    }
    try {
      await api.logout();
    } finally {
      set({
        user: null,
        csrf: '',
        snapshot: null,
        events: [],
        connection: 'closed',
        toast: '',
      });
    }
  },

  boot: async () => {
    if (isDemoMode()) {
      set({
        user: demoUser,
        snapshot: demoSnapshot(),
        events: demoEvents,
        connection: 'open',
        booting: false,
      });
      return;
    }
    try {
      const result = await api.me();
      set({ user: result.user, csrf: result.csrf });
      await get().refresh();
    } catch (error) {
      if (error instanceof api.ApiError && error.status !== 401) {
        set({ error: error.message });
      }
    } finally {
      set({ booting: false });
    }
  },

  refresh: async () => {
    if (isDemoMode()) {
      set({ snapshot: demoSnapshot(), events: demoEvents, error: '' });
      return;
    }
    try {
      const snapshot = await api.fetchStatus();
      const events = await api.fetchEvents();
      set({ snapshot, events, error: '' });
    } catch (error) {
      set({ error: error instanceof Error ? error.message : 'Gagal memuat status' });
    }
  },

  setSnapshot: (snapshot) => {
    set({ snapshot, events: snapshot.events.length ? snapshot.events : get().events });
  },

  setConnection: (connection) => set({ connection }),

  addEvent: (event) => {
    set({ events: [event, ...get().events].slice(0, 100) });
  },

  setToast: (toast) => set({ toast }),
  clearToast: () => set({ toast: '' }),
}));
