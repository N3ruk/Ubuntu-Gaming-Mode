import { createReadStream } from 'node:fs';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { STEAM_ROOT, SUPPORTED_BUILD, SUPPORTED_SP_SHA256 } from './backend.mjs';

// Enrolled on 2026-11-01 while the live client reported SUPPORTED_BUILD.
// .installed files are inventories, NOT version metadata. A cached server
// manifest cannot establish the installed version either.
export const PINNED_FILES = Object.freeze({
  'steamui/sp.js': SUPPORTED_SP_SHA256,
  'linux64/steamclient.so': '3e32119f473cb9c2615e1b282da8498b9986378e8b71537a4aed9f77e7b797e0',
  'ubuntu12_32/steam': 'f3bdb267eeea0b588d46bc395621b6518d63f2440f70727a02b21451478fa15c',
});

export async function verifyPinnedBuild(root = STEAM_ROOT, pins = PINNED_FILES) {
  for (const [relativePath, expected] of Object.entries(pins)) {
    const hash = createHash('sha256');
    for await (const part of createReadStream(path.join(root, relativePath))) hash.update(part);
    if (hash.digest('hex') !== expected) throw Error(`Steam ha cambiado: huella no compatible (${relativePath})`);
  }
  return SUPPORTED_BUILD;
}

// 2026-11-22: steamrt64 moved the active native client and added a separate
// recheck-types argument to ApplyUpdate. Keep the previously verified client.
export const BUILD_PROFILES = Object.freeze([
  { build: SUPPORTED_BUILD, applyContract: 'legacy', pins: PINNED_FILES },
  { build: 1794536281, applyContract: 'recheck', pins: Object.freeze({
    'steamui/sp.js': '4b818cd371388e06c66fe8c0bb592b7dfd4a6fb55eb56844d1b360b64aec3046',
    'steamrt64/steamclient.so': 'dab8df0daa27ffa6594ef5d6bc8cf707f37990220628f1f249fb30f4d88ade91',
    'steamrt64/steam': 'bfe832f9f5883d3114425a53b7a170f11784508641a0ae407f237848fa33c387',
  }) },
]);

export async function verifyKnownBuild(root = STEAM_ROOT, profiles = BUILD_PROFILES) {
  for (const profile of profiles) {
    try {
      await verifyPinnedBuild(root, profile.pins);
      return { build: profile.build, applyContract: profile.applyContract };
    } catch (error) {
      if (error.code !== 'ENOENT' && !error.message.startsWith('Steam ha cambiado:')) throw error;
    }
  }
  throw Error('Steam ha cambiado: ningún perfil UI/cliente nativo verificado coincide');
}
