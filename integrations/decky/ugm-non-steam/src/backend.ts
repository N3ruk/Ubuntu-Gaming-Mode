import { callable } from '@decky/api';
import type { Context, Draft, Files, Page, Reply, SavedShortcut } from './types';
export const getContext = callable<[], Reply<Context>>('get_context');
export const listApplications = callable<[string, number], Reply<Page<Draft>>>('list_applications');
export const browse = callable<[string, string, number, boolean], Reply<Files>>('browse');
export const inspectPath = callable<[string], Reply<Draft>>('inspect_path');
export const makeExecutable = callable<[string], Reply<Draft>>('make_executable');
export const validateDraft = callable<[Draft], Reply<Draft>>('validate_draft');
export const savedShortcuts = callable<[string], Reply<{ items: SavedShortcut[]; state: string }>>('get_saved_shortcuts');
