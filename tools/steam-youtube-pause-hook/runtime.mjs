import fs from 'node:fs';
import {installPauseHook} from './frontend.mjs';
export const contract = JSON.parse(fs.readFileSync(new URL('./contract.json', import.meta.url)));
export function hookExpression(action, {includeEvents = false, expected = contract} = {}) {
  if (!['apply', 'status', 'remove'].includes(action)) throw Error('Acción de hook inválida');
  return `(async()=>{
    const key='__UGMYoutubePauseHook', action=${JSON.stringify(action)};
    const summary=()=>{
      const value=window[key]?.status() ?? {active:false};
      if (!${JSON.stringify(includeEvents)}) delete value.events;
      return value;
    };
    if (action==='status') return summary();
    if (action==='remove') {
      const result=window[key]?.remove() ?? {active:false};delete window[key];return result;
    }
    const contract=${JSON.stringify(expected)};
    if ((await SteamClient.System.GetSystemInfo()).nSteamVersion !== contract.build) throw Error('Build incompatible');
    let req;const chunks=window.webpackChunksteamui;
    if(!Array.isArray(chunks))throw Error('Runtime webpack incompatible');
    const entry=[['ugm-youtube-pause-'+Date.now()],{},r=>req=r];chunks.push(entry);
    const i=chunks.indexOf(entry);if(i>=0)chunks.splice(i,1);
    if(!req?.m?.[contract.module])throw Error('Módulo ausente');
    const bytes=new TextEncoder().encode(req.m[contract.module].toString());
    const hash=[...new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))].map(x=>x.toString(16).padStart(2,'0')).join('');
    if(hash!==contract.factorySHA256)throw Error('Módulo cambiado: no se aplicará el parche');
    if (window[key]) {
      const value=summary();
      if (!value.active || value.version!=='experimental-1') throw Error('Hook residente incompatible');
      return value;
    }
    const proto=req(contract.module)[contract.export]?.prototype;
    if(!proto)throw Error('Export incompatible');
    window[key]=(${installPauseHook.toString()})(proto,contract.methods);
    return summary();
  })()`;
}
