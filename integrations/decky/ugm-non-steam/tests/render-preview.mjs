/** Design preview using actual Content JSX with simulated Decky widgets.
 * Run npm test first. This is not a screenshot of Steam or gamepad validation.
 */
import { JSDOM } from 'jsdom';
import { writeFileSync } from 'node:fs';
const dom=new JSDOM('<div id="root"></div>');
globalThis.window=dom.window;globalThis.document=dom.window.document;
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const React=await import('react');
const {createRoot}=await import('react-dom/client');
const {Content}=await import('../.ui-test-build/Content.js');
const {model}=await import('../.ui-test-build/model.js');
const owner='76561197960265851';
globalThis.UI_TEST={owner,calls:[]};
const context={version:'1.0.0',home:'/home/usuario',locations:[]};
const draft={id:'sample.desktop',name:'Mi juego',exe:'/home/usuario/Juegos/Mi juego.AppImage',startDir:'/home/usuario/Juegos',
 launchOptions:'',windows:false,executable:true,compatTool:'',source:'Archivo Linux'};
const scenes=[
 ['1 · Elegir origen',{screen:'home',context}],
 ['2 · Aplicaciones instaladas',{screen:'apps',context,apps:{items:[{...draft,id:'heroic',name:'Heroic Games Launcher',source:'Flatpak'},
 {...draft,id:'lutris',name:'Lutris',source:'Linux'},{...draft,id:'browser',name:'Navegador',source:'Snap'}],total:3,page:0}}],
 ['3 · Revisar el acceso',{screen:'form',context,draft}],
 ['4 · Resultado y guardado',{screen:'result',context,draft,result:{appId:123,account:owner,request:draft,verified:true,warnings:[]},disk:'unknown'}],
];
let cards='';
for(const [title,state] of scenes){
 model.reset(owner);model.patch(state);
 const root=createRoot(document.getElementById('root'));
 await React.act(async()=>root.render(React.createElement(Content)));
 cards+=`<article><h2>${title}</h2><div class="panel">${document.getElementById('root').innerHTML}</div></article>`;
 await React.act(async()=>root.unmount());
}
const html=`<!doctype html><html lang="es"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>UGM · Añadir contenido Non-Steam — diseño</title><style>
*{box-sizing:border-box}body{background:#10121a;color:#edf2ff;font:14px system-ui,sans-serif;margin:0;padding:32px}
h1{font-size:26px;margin:0 0 10px}header{max-width:1000px;margin:0 auto 30px}header p{color:#b6c3dc;line-height:1.5}
main{display:flex;align-items:flex-start;justify-content:center;gap:18px;flex-wrap:wrap}article{width:320px}h2{font-size:15px;color:#a9c8ff}
.panel{background:#202536;border:1px solid #3b4660;border-top:3px solid #71a5ff;border-radius:8px;padding:14px;box-shadow:0 12px 26px #0005}
button{display:block;border:1px solid #46506a;background:#30384e;color:#f2f5ff;font:inherit;padding:12px 10px;margin:9px 0;width:100%;text-align:left;border-radius:4px}
button:disabled{opacity:.38}label{display:block;color:#c5d2ed;font-size:12px;margin:14px 0}input,select{width:100%;display:block;padding:10px;background:#151a28;color:#f2f5ff;border:1px solid #46506a;border-radius:3px;margin-top:6px;font:14px system-ui}
.panel section>div{margin:8px 0}.panel section>div:first-child{margin-top:0}footer{margin:30px auto;max-width:1000px;color:#96a8c7;font-size:12px}
</style><header><h1>UGM · Añadir contenido Non-Steam</h1><p>Aplicaciones instaladas o ejecutables → Revisar → Añadir a Steam</p>
<p><strong>Vista de diseño.</strong> Usa la interfaz React implementada con controles Decky simulados. No es una captura real de Steam ni demuestra navegación con mando. Datos de ejemplo.</p></header>
<main>${cards}</main><footer>Candidato independiente · Sin instalación · Sin build de UGM · Pendiente de prueba real en Gaming Mode</footer></html>`;
writeFileSync(process.argv[2]||'../../../.build/non-steam-design.html',html);
console.log('Design preview generated (simulated controls, no Steam connection).');
