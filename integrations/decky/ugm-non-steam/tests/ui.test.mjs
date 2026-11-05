import { test, beforeEach, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { writeFileSync, readFileSync, mkdirSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { JSDOM } from 'jsdom';

const dom = new JSDOM('<html><body><div id="root"></div></body></html>', { url: 'http://localhost' });
globalThis.window = dom.window;
globalThis.document = dom.window.document;
globalThis.HTMLElement = dom.window.HTMLElement;
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const React = await import('react');
const { createRoot } = await import('react-dom/client');
const { act } = React;
execFileSync(process.execPath, ['node_modules/typescript/bin/tsc', '--outDir', '.ui-test-build', '--project', 'tsconfig.json']);
const out = resolve('.ui-test-build');
const url = name => pathToFileURL(resolve(out, name)).href;
let code = readFileSync(resolve(out, 'Content.js'), 'utf8');
for (const [source, target] of [['@decky/api', './fake-api.js'], ['@decky/ui', './fake-ui.js'],
  ['./backend', './fake-backend.js'], ['./runtime', './fake-runtime.js'], ['./steam', './steam.js'],
  ['./model', './model.js'], ['./types', './types.js']]) {
  code = code.replaceAll(`from '${source}'`, `from '${target}'`);
}
writeFileSync(resolve(out, 'Content.js'), code);
writeFileSync(resolve(out, 'fake-api.js'), 'export const useQuickAccessVisible = () => false;');
writeFileSync(resolve(out, 'fake-ui.js'), `
import React from 'react';
export const PanelSection = ({children}) => React.createElement('section', null, children);
export const PanelSectionRow = ({children}) => React.createElement('div', null, children);
export const Focusable = ({children}) => React.createElement('div', null, children);
export const ButtonItem = ({children, onClick, disabled}) => React.createElement('button', {onClick, disabled}, children);
export const TextField = ({label, value, disabled, onChange}) => React.createElement('label', null, label, React.createElement('input', {value, disabled, onChange}));
export const DropdownItem = ({label, selectedOption, disabled, rgOptions, onChange}) => React.createElement('label', null, label,
  React.createElement('select', {value:selectedOption, disabled, onChange:e=>onChange({data:e.target.value})}, rgOptions.map(o=>React.createElement('option', {key:o.data, value:o.data}, o.label))));
export const ToggleField = ({label, checked, disabled, onChange}) => React.createElement('label', null, label, React.createElement('input', {type:'checkbox', checked, disabled, onChange:e=>onChange(e.target.checked)}));
export const Navigation = { Navigate:path=>globalThis.UI_TEST.calls.push(['navigate',path]), CloseSideMenus:()=>{} };
`);
writeFileSync(resolve(out, 'fake-backend.js'), `
const rpc = name => async (...args) => {
 const t = globalThis.UI_TEST; t.calls.push([name,...args]);
 if (t.fail === name) return {ok:false,error:'Error simulado'};
 return {ok:true,data: await t.handlers[name](...args)};
};
export const getContext=rpc('context'), listApplications=rpc('apps'), browse=rpc('browse'), inspectPath=rpc('inspect'),
 makeExecutable=rpc('chmod'), validateDraft=rpc('validate'), savedShortcuts=rpc('saved');
`);
writeFileSync(resolve(out, 'fake-runtime.js'), `
export const bridge = {
 account:()=>globalThis.UI_TEST.owner,
 add:async draft=>{const t=globalThis.UI_TEST;t.calls.push(['add',draft]);return t.add(draft);},
 undo:async result=>{globalThis.UI_TEST.calls.push(['undo',result.appId]);}
};
export const compatibilityTools=async()=>[];
`);
const { model } = await import(url('model.js'));
const { Content } = await import(url('Content.js'));
let root;
const draft = () => ({id:'/apps/test.desktop',source:'Linux',name:'Test Game',exe:'/games/game',startDir:'/games',launchOptions:'',compatTool:'',windows:false,executable:true});
const context = {version:'1.0.0',home:'/home/test',locations:[{name:'Carpeta personal',path:'/home/test'}]};
const buttons = () => [...document.querySelectorAll('button')];
const button = text => buttons().find(b => b.textContent === text);
async function click(text) {
  const b=button(text); assert.ok(b,`button ${text}`);
  await act(async()=>{ b.click(); await new Promise(resolve=>setTimeout(resolve,0)); });
}
beforeEach(async () => {
  globalThis.UI_TEST = { owner:'76561197960265851', calls:[], fail:'',
    add:async d=>({appId:123,account:globalThis.UI_TEST.owner,request:{...d},verified:true,warnings:[]}),
    handlers:{context:()=>context,apps:()=>({items:[draft()],total:1,page:0}),
      browse:path=>({items:[{name:'Game.exe',path:'/games/Game.exe',folder:false}],path,parent:'/',total:1,page:0}),
      inspect:()=>({...draft(),exe:'/games/Game.exe',windows:true,executable:true}),
      validate:d=>d,chmod:()=>({...draft(),executable:true}),saved:()=>({items:[],state:'not_saved'})}
  };
  model.reset(globalThis.UI_TEST.owner);
  model.patch({context,path:context.home});
  root=createRoot(document.getElementById('root'));
  await act(async()=>root.render(React.createElement(Content)));
});
afterEach(async()=>{await act(async()=>root.unmount());});

test('home exposes installed apps and file browsing as separate entries', async()=>{
  assert.ok(button('Aplicaciones instaladas')); assert.ok(button('Buscar ejecutable'));
  await click('Aplicaciones instaladas');
  assert.ok(button('Test Game · Linux'));
});
test('installed app selection opens reviewed form, never adds immediately', async()=>{
  await click('Aplicaciones instaladas'); await click('Test Game · Linux');
  assert.equal(model.get().screen,'form');
  assert.ok(document.body.textContent.includes('Destino: /games/game'));
  assert.ok(button('Añadir a Steam'));
  assert.equal(globalThis.UI_TEST.calls.filter(c=>c[0]==='add').length,0);
});
test('manual exe selection requires choosing Proton', async()=>{
  await click('Buscar ejecutable'); await click('Game.exe');
  assert.equal(button('Añadir a Steam').disabled,true);
  await act(async()=>model.patch({tools:[{strToolName:'proton',strDisplayName:'Proton'}]}));
  const select=document.querySelector('select');
  await act(async()=>{select.value='proton';select.dispatchEvent(new window.Event('change',{bubbles:true}));});
  assert.equal(button('Añadir a Steam').disabled,false);
});
test('successful addition is not presented as saved to disk', async()=>{
  await act(async()=>model.patch({screen:'form',draft:draft()}));
  await click('Añadir a Steam');
  assert.equal(model.get().screen,'result');
  assert.ok(document.body.textContent.includes('Guardado en disco aún no comprobado'));
  await click('Comprobar guardado en disco');
  assert.ok(document.body.textContent.includes('Todavía no aparece'));
});
test('undo needs the explicit second step and names only shortcut removal', async()=>{
  await act(async()=>model.patch({screen:'form',draft:draft()})); await click('Añadir a Steam');
  await click('Deshacer este acceso');
  assert.equal(globalThis.UI_TEST.calls.filter(c=>c[0]==='undo').length,0);
  assert.ok(document.body.textContent.includes('No se borran archivos'));
  await click('Confirmar retirada del acceso');
  assert.equal(globalThis.UI_TEST.calls.filter(c=>c[0]==='undo').length,1);
});
test('backend errors remain visible and never create a Steam entry', async()=>{
  await act(async()=>model.patch({screen:'form',draft:draft()}));
  globalThis.UI_TEST.fail='validate'; await click('Añadir a Steam');
  assert.equal(document.querySelector('[role="alert"]').textContent,'Error simulado');
  assert.equal(globalThis.UI_TEST.calls.filter(c=>c[0]==='add').length,0);
});
test('QAM closing during pending operation preserves busy state and final result', async()=>{
  let resolveAdd;
  globalThis.UI_TEST.add=d=>new Promise(resolve=>{resolveAdd=()=>resolve({appId:123,account:globalThis.UI_TEST.owner,request:d,verified:true,warnings:[]});});
  await act(async()=>model.patch({screen:'form',draft:draft()}));
  await act(async()=>{button('Añadir a Steam').click(); await new Promise(resolve=>setTimeout(resolve,0));});
  assert.equal(model.get().busy,true);
  await act(async()=>root.unmount());
  root=createRoot(document.getElementById('root'));
  await act(async()=>root.render(React.createElement(Content)));
  assert.equal(button('Añadir a Steam').disabled,true);
  await act(async()=>{resolveAdd();await new Promise(resolve=>setTimeout(resolve,0));});
  assert.equal(model.get().screen,'result');
  assert.equal(model.get().busy,false);
});
test('opening a created shortcut uses its Steam library route', async()=>{
  await act(async()=>model.patch({screen:'form',draft:draft()})); await click('Añadir a Steam');
  await click('Abrir ficha en Steam');
  assert.deepEqual(globalThis.UI_TEST.calls.at(-1),['navigate','/library/app/123']);
});
