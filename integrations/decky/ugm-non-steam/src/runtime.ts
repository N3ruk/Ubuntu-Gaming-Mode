import { SteamBridge, type Apps, type Overview } from './steam';
import { getCompatManagerTools } from './compatTools';
import type { Tool } from './types';
export const bridge = new SteamBridge({
  get apps() { return SteamClient.Apps as unknown as Apps; },
  account: () => window.App?.m_CurrentUser?.strSteamID ?? '',
  overviews: () => {
    const store = window.appStore as unknown as { allApps?: Overview[] };
    if (!Array.isArray(store?.allApps)) throw new Error('La biblioteca Steam aún no está disponible. Vuelve a abrir el panel.');
    return store.allApps;
  },
  overview: id => window.appStore?.GetAppOverviewByAppID(id) as unknown as Overview | null,
  sleep: ms => new Promise(resolve => setTimeout(resolve, ms)),
});
function timeout<T>(promise: Promise<T>, ms: number): Promise<T> {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('Steam no ha respondido a la consulta de Proton.')), ms);
    promise.then(value => { clearTimeout(timer); resolve(value); }, error => { clearTimeout(timer); reject(error); });
  });
}
export async function compatibilityTools(): Promise<Tool[]> {
  const legacy = SteamClient.Settings as unknown as { GetGlobalCompatTools?: () => Promise<Tool[]> };
  let tools: Tool[] = [];
  try { if (typeof legacy?.GetGlobalCompatTools === 'function') tools = await timeout(legacy.GetGlobalCompatTools(), 4000); } catch { /* current service next */ }
  if (!tools.length) tools = await timeout(getCompatManagerTools(), 4000);
  return [...new Map(tools.filter(t => t?.strToolName).map(t => [t.strToolName, t])).values()];
}
