import { test, beforeEach, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { JSDOM } from 'jsdom';
import React from 'react';
import { createRoot } from 'react-dom/client';
import * as jsx from 'react/jsx-runtime';
const { act } = React;
const dom = new JSDOM('<html><body><div id="root"></div></body></html>',{url:'http://localhost'});
globalThis.window=dom.window;globalThis.document=dom.window.document;
globalThis.HTMLElement=dom.window.HTMLElement;globalThis.localStorage=dom.window.localStorage;
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
globalThis.SP_REACT=React;globalThis.SP_JSX=jsx;
let cancels=[];
globalThis.DFL={
 PanelSection:({children,title})=>React.createElement('section',null,React.createElement('h2',null,title),children),
 PanelSectionRow:({children})=>React.createElement('div',null,children),
 ButtonItem:React.forwardRef(({children,onClick,disabled},ref)=>React.createElement('button',{ref,onClick,disabled},children)),
 ToggleField:({label,checked,onChange,disabled})=>React.createElement('label',null,label,React.createElement('input',{type:'checkbox','aria-label':label,checked,disabled,onChange:e=>onChange(e.target.checked)})),
 SliderField:({label,value,onChange,disabled,min,max})=>React.createElement('label',null,label,React.createElement('input',{type:'range','aria-label':label,value,disabled,min,max,onChange:e=>onChange(+e.target.value)})),
 Focusable:({children,onCancel})=>{cancels.push(onCancel);return React.createElement('div',null,children);},
 TextField:({label,value,disabled,onChange})=>React.createElement('label',null,label,React.createElement('input',{value,disabled,onChange})),
 DropdownItem:()=>null,Navigation:{Navigate:()=>{},CloseSideMenus:()=>{}},
 staticClasses:{Title:'title'},findModuleExport:()=>undefined,
};
const calls=[],connections=[];
let health,handlers,plugin,root,serial=0;
const timers=new Map();let nextTimer=1;
const real={setInterval:globalThis.setInterval,clearInterval:globalThis.clearInterval,setTimeout:globalThis.setTimeout,clearTimeout:globalThis.clearTimeout};
const flush=async()=>{for(let i=0;i<15;i++)await Promise.resolve();};
const buttons=()=>[...document.querySelectorAll('button')];
const button=text=>buttons().find(b=>b.textContent===text);
async function click(text){assert.ok(button(text),'button '+text);await act(async()=>{button(text).click();await flush();});}
const status=()=>({success:true,displays:[],sessions:[],enabled:false,strength:25});
beforeEach(async()=>{
 localStorage.clear();calls.length=0;connections.length=0;cancels=[];
 health=Object.fromEntries(['sharp','downsampling','video','nonsteam'].map(id=>[id,{ready:true,error:''}]));
 handlers={
  sharp:{get_capabilities:()=>({success:true,supportsSGSR:true,showFsrOverride:true}),get_status:status,
   set_nis_enabled:status,set_fsr_override:status,set_nis_sharpness:status},
  downsampling:{get_status:()=>({enabled:true,strength:25,sessions:[{display:':1',active:false,enabledApplied:true,strengthApplied:true}]}),
   set_enabled:enabled=>({enabled,strength:25,sessions:[]}),set_strength:strength=>({enabled:true,strength,sessions:[]})},
  video:{get_state:()=>({connectors:[{id:'0:DP-1',label:'AOC Monitor',connected:true,verified:true,enabled:true}],video:{gaming:true}}),switch_connector:()=>({ok:true})},
  nonsteam:{get_context:()=>({ok:true,data:{version:'1.0.0',home:'/home/test',locations:[]}}),
   list_applications:()=>({ok:true,data:{items:[],total:0,page:0}})},
 };
 globalThis.SteamClient={Apps:{},Settings:{GetGlobalCompatTools:async()=>[{strToolName:'proton',strDisplayName:'Proton'}]}};
 window.App={m_CurrentUser:{strSteamID:'76561197960265851'}};
 window.__DECKY_SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_deckyLoaderAPIInit={connect(version,name){
  connections.push(name);return {_version:version,useQuickAccessVisible:()=>true,callable:method=>(...args)=>{
   if(method==='module_status')return Promise.resolve(structuredClone(health));
   assert.equal(method,'module_call');const [id,rpc,params]=args;
   calls.push([id,rpc,...params]);return Promise.resolve().then(()=>handlers[id][rpc](...params));
  }};
 }};
 globalThis.setInterval=(fn,ms)=>{const id=nextTimer++;timers.set(id,{fn,ms,interval:true});return id;};
 globalThis.setTimeout=(fn,ms,...args)=>{if (![2000,4000].includes(ms)) return real.setTimeout(fn,ms,...args);const id=nextTimer++;timers.set(id,{fn,ms});return id;};
 globalThis.clearInterval=globalThis.clearTimeout=id=>{if (!timers.delete(id)) real.clearTimeout(id);};
 const code=readFileSync(new URL('../dist/index.js',import.meta.url),'utf8');
 const mod=await import('data:text/javascript;base64,'+Buffer.from(code+`\n// test ${serial++}`).toString('base64'));
 plugin=mod.default();root=createRoot(document.getElementById('root'));
 await act(async()=>{root.render(plugin.content);await flush();});
});
afterEach(async()=>{
 await act(async()=>{root.unmount();await flush();});plugin.onDismount();
 Object.assign(globalThis,real);assert.equal(timers.size,0,'all original polling timers cleaned up');
});

test('one master entry and four sections; home does not mount or mutate a module',()=>{
 assert.equal(plugin.name,'UGM Options');
 assert.deepEqual(buttons().map(b=>b.textContent),['Sharp Filter','Downsampling','Video Output','Non-Steam']);
 assert.deepEqual(calls,[]);assert.deepEqual(connections,['UGM Options']);
});
test('Sharp exposes the exact existing switches and uses only its own RPC',async()=>{
 await click('Sharp Filter');
 assert.ok(document.querySelector('[aria-label="Use NIS"]'));
 assert.ok(document.querySelector('[aria-label="Use FSR"]'));
 assert.ok(calls.length>0);assert.ok(calls.every(c=>c[0]==='sharp'));
 assert.ok(connections.every(name=>name==='UGM Options'));
});
test('downsampling keeps original controls and forwards the exact toggle value',async()=>{
 await click('Downsampling');
 assert.ok(document.querySelector('[aria-label="Intensidad"]'));
 await act(async()=>{document.querySelector('[aria-label="Activar automáticamente"]').click();await flush();});
 assert.ok(calls.some(c=>c[0]==='downsampling'&&c[1]==='set_enabled'&&c[2]===false));
 assert.ok(calls.every(c=>c[0]==='downsampling'));
});
test('video keeps detected display labels and sends the original connector ID',async()=>{
 await click('Video Output');await click('AOC Monitor');
 assert.deepEqual(calls.at(-1),['video','switch_connector','0:DP-1']);
 assert.ok(calls.every(c=>c[0]==='video'));
});
test('nonsteam keeps original home and catalog navigation without creating shortcuts',async()=>{
 await click('Non-Steam');assert.ok(button('Buscar ejecutable'));
 await click('Aplicaciones instaladas');
 assert.ok(calls.some(c=>c[0]==='nonsteam'&&c[1]==='list_applications'));
 assert.ok(document.body.textContent.includes('No hay resultados'));
 assert.ok(calls.every(c=>c[0]==='nonsteam'));
});
test('back cleans up original timers and restores focus to the selected entry',async()=>{
 await click('Video Output');assert.equal(timers.size,1);
 await click('‹ UGM Options');assert.equal(timers.size,0);
 assert.equal(document.activeElement,button('Video Output'));
 assert.deepEqual(buttons().map(b=>b.textContent),['Sharp Filter','Downsampling','Video Output','Non-Steam']);
});
test('B returns from a module to the master and consumes the event',async()=>{
 await click('Downsampling');let stopped=false;
 await act(async()=>{cancels.at(-1)({stopPropagation(){stopped=true;}});await flush();});
 assert.equal(stopped,true);assert.ok(button('Downsampling'));assert.equal(timers.size,0);
});
test('Non-Steam B first respects its own subpage navigation',async()=>{
 await click('Non-Steam');await click('Aplicaciones instaladas');
 let stopped=false;
 await act(async()=>{cancels.at(-1)({stopPropagation(){stopped=true;}});await flush();});
 assert.equal(stopped,true);assert.ok(button('Aplicaciones instaladas'));assert.ok(button('‹ UGM Options'));
});
test('unavailable backend is isolated; another section still works',async()=>{
 health.video={ready:false,error:'Retira primero el plugin independiente'};
 await click('Video Output');assert.ok(document.body.textContent.includes('Retira primero'));
 assert.equal(calls.length,0);
 await click('‹ UGM Options');await click('Downsampling');
 assert.ok(document.querySelector('[aria-label="Intensidad"]'));
});
test('backend operation failure stays within the original panel',async()=>{
 handlers.video.switch_connector=()=>{throw Error('offline');};
 await click('Video Output');await click('AOC Monitor');
 assert.ok(document.body.textContent.includes('No se pudo comunicar'));
 await click('‹ UGM Options');await click('Non-Steam');assert.ok(button('Buscar ejecutable'));
});

test('a module render crash is caught without losing master navigation',async()=>{
 const original=DFL.PanelSection;
 DFL.PanelSection=props=>{if(props.title==='Salida de vídeo')throw Error('simulated render failure');return original(props);};
 const saved=console.error;console.error=()=>{};
 try {
  await click('Video Output');assert.ok(document.body.textContent.includes('simulated render failure'));
  await click('‹ UGM Options');await click('Downsampling');
  assert.ok(document.querySelector('[aria-label="Intensidad"]'));
 } finally {DFL.PanelSection=original;console.error=saved;}
});

test('returning to Non-Steam preserves its reviewed draft instead of resetting it',async()=>{
 handlers.nonsteam.list_applications=()=>({ok:true,data:{items:[{
  id:'sample',source:'Linux',name:'Existing Game',exe:'/games/sample',startDir:'/games',
  launchOptions:'',compatTool:'',windows:false,executable:true,
 }],total:1,page:0}});
 await click('Non-Steam');await click('Aplicaciones instaladas');await click('Existing Game · Linux');
 assert.ok(button('Añadir a Steam'));
 await click('‹ UGM Options');await click('Video Output');await click('‹ UGM Options');await click('Non-Steam');
 assert.ok(button('Añadir a Steam'));assert.ok(document.body.textContent.includes('Destino: /games/sample'));
 assert.ok(!calls.some(c=>['validate_draft','make_executable'].includes(c[1])));
});
