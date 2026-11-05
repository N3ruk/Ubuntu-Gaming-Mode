import type { Context, Draft, Files, Page, Tool } from './types';
import type { Addition } from './steam';
export type Screen = 'home' | 'apps' | 'files' | 'form' | 'result';
export interface State {
  screen: Screen; owner: string; context: Context | null; tools: Tool[]; toolError: string;
  apps: Page<Draft> | null; files: Files | null; query: string; path: string; hidden: boolean;
  draft: Draft | null; result: Addition | null; disk: 'unknown' | 'pending' | 'saved';
  error: string; busy: boolean; confirmUndo: boolean;
}
const initial = (): State => ({ screen: 'home', owner: '', context: null, tools: [], toolError: '',
  apps: null, files: null, query: '', path: '', hidden: false, draft: null, result: null,
  disk: 'unknown', error: '', busy: false, confirmUndo: false });
export function createModel() {
  let state = initial();
  const listeners = new Set<() => void>();
  return {
    get: () => state,
    subscribe: (listener: () => void) => { listeners.add(listener); return () => { listeners.delete(listener); }; },
    patch: (value: Partial<State>) => { state = { ...state, ...value }; listeners.forEach(fn => fn()); },
    reset: (owner: string) => { state = { ...initial(), owner }; listeners.forEach(fn => fn()); },
  };
}
// In memory only: survives QAM remounts, clears on account change/plugin unload.
export const model = createModel();
