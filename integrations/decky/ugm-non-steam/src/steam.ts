/** Shortcut sequencing adapted from Add Non-Steam Games (BSD-3-Clause).
 * See LICENSE / NOTICE.md. No VDF writes and no Steam restart.
 */
import type { Draft, SavedShortcut } from './types';
export interface Details {
  strDisplayName?: string; strShortcutExe?: string; strShortcutStartDir?: string;
  strShortcutLaunchOptions?: string; strLaunchOptions?: string; strCompatToolName?: string;
}
export interface Overview { appid: number; display_name?: string; app_type?: number; BIsShortcut?: () => boolean }
export interface Apps {
  AddShortcut(name: string, exe: string, start: string, args: string): Promise<number>;
  SetShortcutName(id: number, value: string): unknown;
  SetShortcutExe(id: number, value: string): unknown;
  SetShortcutStartDir(id: number, value: string): unknown;
  SetShortcutLaunchOptions(id: number, value: string): unknown;
  SetAppLaunchOptions?(id: number, value: string): unknown;
  SpecifyCompatTool?(id: number, value: string): unknown;
  RemoveShortcut(id: number): unknown;
  RegisterForAppDetails(id: number, callback: (details: Details) => void): { unregister(): void };
}
export interface Runtime {
  apps: Apps; account(): string; overviews(): Overview[];
  overview(id: number): Overview | null; sleep(ms: number): Promise<void>;
}
export interface Addition {
  appId: number; account: string; request: Draft; verified: boolean; warnings: string[];
}
export const quote = (value: string) => '"' + value.replace(/[\\"$`]/g, '\\$&') + '"';
export function unquote(value: string): string {
  if (value.startsWith('"') && value.endsWith('"')) return value.slice(1, -1).replace(/\\([\\"$`])/g, '$1');
  return value;
}
export const sameTarget = (a: Pick<Draft, 'exe' | 'launchOptions'>, b: Pick<Draft, 'exe' | 'launchOptions'>) =>
  unquote(a.exe) === unquote(b.exe) && a.launchOptions === b.launchOptions;
export function diskStatus(result: Addition, saved: SavedShortcut[]): 'saved' | 'pending' {
  return saved.some(s => (s.appId >>> 0) === (result.appId >>> 0) && s.name === result.request.name &&
    sameTarget(s, result.request) && unquote(s.startDir) === result.request.startDir) ? 'saved' : 'pending';
}
export class SteamBridge {
  private busy = false;
  private owned = new Map<number, Addition>();
  constructor(private runtime: Runtime) {}
  account(): string {
    const value = this.runtime.account();
    if (!/^7656\d{13}$/.test(value)) throw new Error('No se ha identificado la cuenta Steam activa.');
    return value;
  }
  private assertAccount(expected: string) {
    if (this.account() !== expected) throw new Error('La cuenta Steam ha cambiado. Vuelve a abrir el plugin.');
  }
  private requireApi(names: (keyof Apps)[]) {
    for (const name of names) if (typeof this.runtime.apps?.[name] !== 'function')
      throw new Error(`Este cliente Steam no ofrece ${name}. No se ha modificado la biblioteca.`);
  }
  private details(id: number, timeout = 1500): Promise<Details | null> {
    return new Promise(resolve => {
      let done = false;
      let registration: { unregister(): void } | undefined;
      let timer: ReturnType<typeof setTimeout> | undefined;
      const finish = (value: Details | null) => {
        if (done) return;
        done = true;
        if (timer) clearTimeout(timer);
        try { registration?.unregister(); } catch { /* unloaded */ }
        resolve(value);
      };
      timer = setTimeout(() => finish(null), timeout);
      try {
        registration = this.runtime.apps.RegisterForAppDetails(id, value => {
          if (value && typeof value.strShortcutExe === 'string' && value.strShortcutExe) finish(value);
        });
        if (done) registration?.unregister(); // synchronous callback
      } catch { finish(null); }
    });
  }
  private list(): Overview[] {
    const all = this.runtime.overviews();
    if (!Array.isArray(all)) throw new Error('No se puede consultar la biblioteca para evitar duplicados.');
    return all.filter(item => item.BIsShortcut?.() || item.app_type === 1073741824);
  }
  private coreMatches(details: Details, request: Draft, overview: Overview | null): boolean {
    return (details.strDisplayName ?? overview?.display_name) === request.name &&
      unquote(details.strShortcutExe ?? '') === request.exe &&
      unquote(details.strShortcutStartDir ?? '') === request.startDir &&
      (details.strShortcutLaunchOptions ?? details.strLaunchOptions) === request.launchOptions;
  }
  async add(request: Draft): Promise<Addition> {
    if (this.busy) throw new Error('Ya hay una operación en curso.');
    this.busy = true;
    try {
      this.requireApi(['AddShortcut', 'SetShortcutName', 'SetShortcutExe', 'SetShortcutStartDir',
        'SetShortcutLaunchOptions', 'RegisterForAppDetails', 'RemoveShortcut']);
      if (request.compatTool) this.requireApi(['SpecifyCompatTool']);
      const account = this.account();
      const before = this.list();
      for (let offset = 0; offset < before.length; offset += 8) {
        const details = await Promise.all(before.slice(offset, offset + 8).map(item => this.details(item.appid)));
        this.assertAccount(account);
        if (details.some(item => !item)) throw new Error('Steam no ha permitido comprobar todos los accesos existentes. Reintenta cuando termine de cargar.');
        for (const item of details) {
          const args = item!.strShortcutLaunchOptions ?? item!.strLaunchOptions;
          if (args === undefined) throw new Error('Steam no expone los argumentos; no se puede descartar un duplicado.');
          if (sameTarget(request, { exe: item!.strShortcutExe!, launchOptions: args }))
            throw new Error('Este destino con los mismos argumentos ya está en tu biblioteca. No se ha añadido otro acceso.');
        }
      }
      const current = this.list();
      if (current.some(item => !before.some(old => old.appid === item.appid)))
        throw new Error('La biblioteca ha cambiado durante la comprobación. Pulsa Añadir de nuevo.');
      this.assertAccount(account);
      let appId = 0;
      for (const target of [request.exe, quote(request.exe)]) {
        // Never retry an exception: Steam might already have created an entry.
        let raw: number;
        try { raw = await this.runtime.apps.AddShortcut(request.name, target, '', ''); }
        catch { throw new Error('Steam no ha confirmado la operación. Comprueba la biblioteca antes de reintentar: podría haber creado el acceso.'); }
        this.assertAccount(account);
        if (typeof raw !== 'number' || !Number.isInteger(raw) || raw < -2147483648 || raw > 4294967295)
          throw new Error('Respuesta inesperada de Steam. Comprueba la biblioteca antes de reintentar.');
        if (raw !== 0) { appId = raw >>> 0; break; }
      }
      if (!appId) throw new Error('Steam no ha creado el acceso. No reinicies la sesión desde el plugin.');
      if (before.some(item => (item.appid >>> 0) === appId))
        throw new Error('Steam ha devuelto un acceso que ya existía. No se ha cambiado ni eliminado.');
      const result: Addition = { appId, account, request: { ...request }, verified: false, warnings: [] };
      this.owned.set(appId, result);
      try {
        for (let attempt = 0; attempt < 50 && !this.runtime.overview(appId); attempt++) {
          await this.runtime.sleep(100);
          this.assertAccount(account);
        }
        if (!this.runtime.overview(appId)) throw new Error('Steam aún no ha registrado todos los datos del acceso.');
        const apps = this.runtime.apps;
        for (const action of [
          () => apps.SetShortcutName(appId, request.name),
          () => apps.SetShortcutExe(appId, quote(request.exe)),
          () => apps.SetShortcutStartDir(appId, request.startDir),
          () => apps.SetShortcutLaunchOptions(appId, request.launchOptions),
          () => apps.SetAppLaunchOptions?.(appId, request.launchOptions),
          () => request.compatTool ? apps.SpecifyCompatTool!(appId, request.compatTool) : undefined,
        ]) { this.assertAccount(account); await action(); }
        for (let attempt = 0; attempt < 5; attempt++) {
          await this.runtime.sleep(250);
          this.assertAccount(account);
          const details = await this.details(appId);
          this.assertAccount(account);
          if (details && this.coreMatches(details, request, this.runtime.overview(appId)) &&
            (!request.compatTool || details.strCompatToolName === request.compatTool)) {
            result.verified = true;
            break;
          }
        }
        if (!result.verified) result.warnings.push('Acceso creado, pero Steam no ha confirmado todos los campos o Proton. Revisa sus propiedades antes de usarlo.');
      } catch (error) { result.warnings.push(error instanceof Error ? error.message : String(error)); }
      return result;
    } finally { this.busy = false; }
  }
  async undo(result: Addition): Promise<void> {
    if (this.busy) throw new Error('Ya hay una operación en curso.');
    if (this.owned.get(result.appId) !== result) throw new Error('Este acceso no pertenece a la operación actual.');
    this.busy = true;
    try {
      this.assertAccount(result.account);
      const overview = this.runtime.overview(result.appId);
      if (!overview) throw new Error('El acceso ya no está en la biblioteca.');
      const details = await this.details(result.appId);
      this.assertAccount(result.account);
      if (!details || !this.coreMatches(details, result.request, overview))
        throw new Error('El acceso ha cambiado o no se puede comprobar. No se elimina; puedes gestionarlo desde Steam.');
      await this.runtime.apps.RemoveShortcut(result.appId);
      for (let i = 0; i < 30; i++) {
        this.assertAccount(result.account);
        if (!this.runtime.overview(result.appId)) { this.owned.delete(result.appId); return; }
        await this.runtime.sleep(100);
      }
      throw new Error('Steam no ha confirmado la retirada del acceso. Comprueba su ficha.');
    } finally { this.busy = false; }
  }
}
