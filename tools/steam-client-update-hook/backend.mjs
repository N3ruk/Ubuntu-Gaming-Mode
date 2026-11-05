import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
export const SUPPORTED_BUILD = 1792618285;
export const SUPPORTED_SP_SHA256 = '692083495994d2b2cf686effd89c11fa987aefa45787cbfa212627c37eda9c7a';
export const STEAM_ROOT = path.join(process.env.HOME, '.local/share/Steam');
export function manifestVersion(text) {
  // Parse Valve KeyValues, not a loose match against package version fields.
  const tokens = [];
  const pattern = /\s+|\/\/[^\n]*|"((?:[^"\\]|\\.)*)"|([{}])/gy;
  let offset = 0;
  while (offset < text.length) {
    pattern.lastIndex = offset;
    const match = pattern.exec(text);
    if (!match) throw Error('Manifiesto Valve no interpretable');
    offset = pattern.lastIndex;
    if (match[1] !== undefined) tokens.push({ value: match[1].replace(/\\(["\\])/g, '$1') });
    else if (match[2]) tokens.push(match[2]);
  }
  let index = 0;
  function object(nested = false, depth = 0) {
    if (depth > 10) throw Error('Manifiesto demasiado anidado');
    const result = Object.create(null);
    while (index < tokens.length && tokens[index] !== '}') {
      const key = tokens[index++];
      if (!key || typeof key !== 'object' || Object.hasOwn(result, key.value)) throw Error('Claves de manifiesto inválidas/duplicadas');
      const value = tokens[index++];
      if (value === '{') result[key.value] = object(true, depth + 1);
      else if (value && typeof value === 'object') result[key.value] = value.value;
      else throw Error('Valor de manifiesto inválido');
    }
    if (nested && tokens[index++] !== '}') throw Error('Manifiesto incompleto');
    return result;
  }
  const data = object();
  if (index !== tokens.length || Object.keys(data).some(k => !['steamrt64', 'kvsign2', 'kvsignatures'].includes(k)) || !data.steamrt64) throw Error('Plataforma de manifiesto inesperada');
  const version = data.steamrt64.version;
  if (typeof version !== 'string' || !/^\d{9,12}$/.test(version) || !Number.isSafeInteger(Number(version))) throw Error('Versión de manifiesto inválida');
  return Number(version);
}
export async function selectedManifest(root = STEAM_ROOT) {
  let branch = '';
  try { branch = (await fs.readFile(path.join(root, 'package/beta'), 'utf8')).trim(); }
  catch (error) { if (error.code !== 'ENOENT') throw error; }
  if (branch && !/^[a-zA-Z0-9_]{1,64}$/.test(branch)) throw Error('Rama Steam no compatible');
  const name = `steam_client_${branch ? branch + '_' : ''}steamrt64`;
  await fs.access(path.join(root, 'package', name + '.installed'));
  return { branch: branch || 'public', name };
}
export async function checkClient(installed, { root = STEAM_ROOT, fetcher = fetch } = {}) {
  if (!Number.isSafeInteger(installed) || installed <= 0) throw Error('Steam no ha informado una versión instalada válida');
  const selection = await selectedManifest(root);
  const url = `https://client-update.fastly.steamstatic.com/${selection.name}`;
  const response = await fetcher(url, { signal: AbortSignal.timeout(10000), redirect: 'error', headers: { 'Cache-Control': 'no-cache' } });
  if (!response.ok) throw Error(`Consulta del cliente Steam: HTTP ${response.status}`);
  if (Number(response.headers.get('content-length')) > 262144) throw Error('Manifiesto demasiado grande');
  let size = 0;
  const parts = [];
  for await (const part of response.body) {
    size += part.length;
    if (size > 262144) { throw Error('Manifiesto demasiado grande'); }
    parts.push(Buffer.from(part));
  }
  const content = Buffer.concat(parts);
  const latest = manifestVersion(content.toString('utf8'));
  if (JSON.stringify(selection) !== JSON.stringify(await selectedManifest(root))) throw Error('La rama ha cambiado durante la consulta; repite la búsqueda');
  if (latest < installed) throw Error(`El servidor ofrece una versión anterior (${latest}); no se ofrecerá una bajada de versión`);
  return { ...selection, installed, latest, available: latest > installed, checked: Math.floor(Date.now() / 1000), bytes: size, sha256: createHash('sha256').update(content).digest('hex') };
}
