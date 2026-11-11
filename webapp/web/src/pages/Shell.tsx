import { useState, type ReactNode } from 'react';
import {
  ListChecks,
  LogOut,
  PanelLeftClose,
  PanelLeftOpen,
  Radio,
  RefreshCw,
  Settings,
  ShieldAlert,
  SlidersHorizontal,
  Wifi,
  WifiOff,
} from 'lucide-react';
import * as api from '../api';
import { isDemoMode } from '../demo';
import { useApp } from '../store';
import type { User } from '../types';
import { DEVICE_NAME } from '../version';
import Dashboard from './Dashboard';
import Events from './Events';
import ConfigPage from './ConfigPage';
import SettingsPage from './Settings';
import { Badge, Button, cn } from '../components/ui';

type Tab = 'dashboard' | 'events' | 'config' | 'settings';

export default function Shell({ user }: { user: User }) {
  const [tab, setTab] = useState<Tab>('dashboard');
  const [collapsed, setCollapsed] = useState(false);
  const snapshot = useApp((state) => state.snapshot);
  const connection = useApp((state) => state.connection);
  const refresh = useApp((state) => state.refresh);
  const logout = useApp((state) => state.logout);
  const setToast = useApp((state) => state.setToast);
  const [clearing, setClearing] = useState(false);
  const safety = snapshot?.status?.safety;
  const safetyState = safety?.state ?? 0;
  const online = Boolean(snapshot?.online && !snapshot.stale && snapshot.brokerConnected);

  const clearEstop = async () => {
    if (!window.confirm('Clear latch E-stop? Pastikan tombol fisik sudah dilepas.')) return;
    if (isDemoMode()) {
      setToast('Mode demo: clear tidak dikirim.');
      return;
    }
    setClearing(true);
    try {
      await api.sendCommand('clear');
      setToast('Clear E-stop diterima.');
      await refresh();
    } catch (error) {
      setToast(error instanceof Error ? error.message : 'Clear gagal');
    } finally {
      setClearing(false);
    }
  };

  return (
    <div className="min-h-dvh lg:flex">
      <aside
        onMouseEnter={() => setCollapsed(false)}
        onMouseLeave={() => setCollapsed(true)}
        className={cn(
          'hidden shrink-0 border-r border-line bg-panel/60 p-3 transition-all duration-200 lg:flex lg:flex-col',
          collapsed ? 'w-16' : 'w-64',
        )}
      >
        {!collapsed && (
          <div className="flex items-center justify-between gap-2">
            <Brand />
            <button
              aria-label="Minimalkan panel kiri"
              title="Minimalkan panel"
              onClick={() => setCollapsed(true)}
              className="rounded-lg p-2 text-slate-400 hover:bg-white/5 hover:text-slate-200"
            >
              <PanelLeftClose size={17} />
            </button>
          </div>
        )}
        <nav className={cn('space-y-1', collapsed ? 'mt-2' : 'mt-8')}>
          <NavItem collapsed={collapsed} active={tab === 'dashboard'} icon={<Radio size={17} />} onClick={() => setTab('dashboard')}>
            Dashboard
          </NavItem>
          <NavItem collapsed={collapsed} active={tab === 'events'} icon={<ListChecks size={17} />} onClick={() => setTab('events')}>
            Events
          </NavItem>
          <NavItem collapsed={collapsed} active={tab === 'config'} icon={<SlidersHorizontal size={17} />} onClick={() => setTab('config')}>
            Preset &amp; Kalibrasi
          </NavItem>
          <NavItem collapsed={collapsed} active={tab === 'settings'} icon={<Settings size={17} />} onClick={() => setTab('settings')}>
            Settings
          </NavItem>
        </nav>
        <div className="mt-auto space-y-3">
          {collapsed && (
            <button
              aria-label="Perluas panel kiri"
              title="Perluas panel"
              onClick={() => setCollapsed(false)}
              className="mx-auto grid h-10 w-10 place-items-center rounded-xl text-slate-400 hover:bg-white/5 hover:text-slate-200"
            >
              <PanelLeftOpen size={17} />
            </button>
          )}
          {!collapsed && (
            <div className="rounded-xl border border-line bg-ink/50 p-3 text-xs">
              <div className="text-slate-500">Masuk sebagai</div>
              <div className="mt-1 font-medium text-slate-200">{user.username}</div>
              <div className="mt-1"><Badge tone="info">{user.role}</Badge></div>
            </div>
          )}
          <Button
            tone="ghost"
            className={cn('w-full', collapsed ? 'justify-center px-0' : 'justify-start')}
            title="Keluar"
            aria-label="Keluar"
            onClick={() => void logout()}
          >
            <LogOut size={16} /> {!collapsed && 'Keluar'}
          </Button>
        </div>
      </aside>

      <div className="min-w-0 flex-1 pb-24 lg:pb-0">
        <header className="sticky top-0 z-30 border-b border-line bg-ink/90 px-4 py-3 backdrop-blur lg:px-8">
          <div className="mx-auto flex max-w-7xl items-center justify-between gap-3">
            <div className="flex min-w-0 items-center gap-3">
              <div className="lg:hidden"><Brand compact /></div>
              <div className="min-w-0">
                <div className="truncate text-sm font-semibold text-slate-100">
                  {DEVICE_NAME}
                </div>
                <div className="hidden truncate text-[11px] text-slate-500 sm:block">
                  {snapshot?.deviceId ?? 'Menunggu device'} · {online ? 'data real-time' : 'stale / offline'}
                </div>
              </div>
            </div>
            <div className="flex items-center gap-2">
              <Badge tone={online ? 'safe' : 'danger'}>
                {online ? <Wifi size={12} /> : <WifiOff size={12} />}
                {online ? 'ONLINE' : 'OFFLINE'}
              </Badge>
              <Badge tone={connection === 'open' ? 'info' : 'muted'}>
                WS {connection}
              </Badge>
              <Button tone="ghost" aria-label="Refresh" onClick={() => void refresh()}>
                <RefreshCw size={16} />
              </Button>
            </div>
          </div>
        </header>

        {safetyState !== 0 && (
          <div role="alert" className="border-b border-danger/30 bg-danger/15 px-4 py-2 text-xs text-danger lg:px-8">
            <div className="mx-auto flex max-w-7xl flex-wrap items-center gap-2">
              <ShieldAlert size={15} />
              <span className="font-semibold">{safety?.nama ?? 'FAULT'}</span>
              <span className="min-w-0 flex-1 truncate text-slate-300">{safety?.detail}</span>
              {user.role !== 'viewer' && safetyState === 1 && safety?.clear_remote && (
                <Button tone="danger" onClick={() => void clearEstop()} disabled={clearing}>
                  {clearing ? 'Clearing...' : 'Clear E-stop'}
                </Button>
              )}
            </div>
          </div>
        )}

        <main className="mx-auto max-w-7xl px-4 py-5 lg:px-8">
          {tab === 'dashboard' && <Dashboard onOpenEvents={() => setTab('events')} />}
          {tab === 'events' && <Events />}
          {tab === 'config' && <ConfigPage />}
          {tab === 'settings' && <SettingsPage />}
        </main>
      </div>

      <nav className="fixed inset-x-0 bottom-0 z-40 grid grid-cols-4 border-t border-line bg-panel/95 px-2 py-2 backdrop-blur lg:hidden">
        <MobileNav active={tab === 'dashboard'} icon={<Radio size={18} />} onClick={() => setTab('dashboard')}>
          Dashboard
        </MobileNav>
        <MobileNav active={tab === 'events'} icon={<ListChecks size={18} />} onClick={() => setTab('events')}>
          Events
        </MobileNav>
        <MobileNav active={tab === 'config'} icon={<SlidersHorizontal size={18} />} onClick={() => setTab('config')}>
          Preset
        </MobileNav>
        <MobileNav active={tab === 'settings'} icon={<Settings size={18} />} onClick={() => setTab('settings')}>
          Settings
        </MobileNav>
      </nav>
    </div>
  );
}

function Brand({ compact }: { compact?: boolean }) {
  return (
    <div className="flex items-center gap-2">
      <div className="grid h-9 w-9 place-items-center rounded-xl border border-safe/30 bg-safe/10 text-safe">
        <Radio size={18} />
      </div>
      {!compact && (
        <div>
          <div className="text-sm font-bold tracking-tight text-slate-100">Auto Etching</div>
          <div className="text-[10px] text-slate-500">control panel</div>
        </div>
      )}
    </div>
  );
}

function NavItem({
  active,
  icon,
  children,
  onClick,
  collapsed,
}: {
  active: boolean;
  icon: ReactNode;
  children: ReactNode;
  onClick: () => void;
  collapsed?: boolean;
}) {
  return (
    <button
      onClick={onClick}
      title={collapsed ? String(children) : undefined}
      aria-label={collapsed ? String(children) : undefined}
      className={cn(
        'flex w-full items-center gap-3 rounded-xl px-3 py-2.5 text-left text-sm font-medium transition',
        collapsed && 'justify-center px-0',
        active ? 'bg-safe/10 text-safe' : 'text-slate-400 hover:bg-white/5 hover:text-slate-200',
      )}
    >
      {icon}
      {!collapsed && children}
    </button>
  );
}

function MobileNav({
  active,
  icon,
  children,
  onClick,
}: {
  active: boolean;
  icon: ReactNode;
  children: ReactNode;
  onClick: () => void;
}) {
  return (
    <button
      onClick={onClick}
      className={cn(
        'flex flex-col items-center gap-1 rounded-lg py-1.5 text-[10px] font-medium',
        active ? 'text-safe' : 'text-slate-500',
      )}
    >
      {icon}
      {children}
    </button>
  );
}
