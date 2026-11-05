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
