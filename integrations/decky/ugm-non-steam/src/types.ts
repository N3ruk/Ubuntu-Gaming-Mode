export interface Draft {
  name: string; exe: string; startDir: string; launchOptions: string; compatTool: string;
  windows: boolean; executable?: boolean; source?: string; id?: string;
}
export interface Tool { strToolName: string; strDisplayName: string }
export interface SavedShortcut { appId: number; name: string; exe: string; startDir: string; launchOptions: string }
export interface Page<T> { items: T[]; total: number; page: number }
export interface FileEntry { name: string; path: string; folder: boolean }
export interface Files extends Page<FileEntry> { path: string; parent: string }
export interface Context { version: string; home: string; locations: { name: string; path: string }[] }
export type Reply<T> = { ok: true; data: T } | { ok: false; error: string };
export function unwrap<T>(value: Reply<T>): T {
  if (!value.ok) throw new Error(value.error);
  return value.data;
}
