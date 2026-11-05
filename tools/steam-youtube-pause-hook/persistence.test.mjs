import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {createHash, webcrypto} from 'node:crypto';
import {createSupervisor} from './supervisor.mjs';
import {hookExpression, contract} from './runtime.mjs';

function fixture() {
  let gaming = true, list = [{id:'A'}], resident = {active:true, version:'experimental-1'}, fail = false, blocked = null;
  const commands = [], reports = [], clients = [];
  const supervisor = createSupervisor({
    inGamingMode: async()=>gaming, targets:async()=>list,
    connect:async(target)=>{
      const client={closed:false, close(){this.closed=true;}, async evaluate(action){
        commands.push([target.id,action]);
        if(blocked) await blocked;
        if(fail) throw Error('connection lost');
        if(action==='apply') resident={active:true,version:'experimental-1'};
        if(action==='remove') resident={active:false};
        return resident;
      }};
      clients.push(client); return client;
    },
    report:async(value)=>reports.push(value), apply:'apply',status:'status',remove:'remove',
  });
  return {supervisor,commands,reports,clients,
    gaming(value){gaming=value;}, targets(value){list=value;}, resident(value){resident=value;},
    fail(value){fail=value;}, block(value){blocked=value;},
  };
}
test('persistencia: aplica una sola vez y supervisa sin duplicar wrappers',async()=>{
  const h=fixture();await h.supervisor.tick();await h.supervisor.tick();
  assert.deepEqual(h.commands,[['A','apply'],['A','status']]);assert.equal(h.clients.length,1);
});
test('persistencia: nuevo target tras reiniciar Steam vuelve a aplicar',async()=>{
  const h=fixture();await h.supervisor.tick();h.targets([{id:'B'}]);await h.supervisor.tick();
  assert.deepEqual(h.commands,[['A','apply'],['A','remove'],['B','apply']]);assert.ok(h.clients[0].closed);
});
test('persistencia: recarga con mismo ID reinserta el hook perdido',async()=>{
  const h=fixture();await h.supervisor.tick();h.resident({active:false});await h.supervisor.tick();
  assert.deepEqual(h.commands,[['A','apply'],['A','status'],['A','apply']]);assert.ok(h.clients[0].closed);
});
test('persistencia: salir de Gaming Mode restaura solo el hook propio',async()=>{
  const h=fixture();await h.supervisor.tick();h.gaming(false);await h.supervisor.tick();
  assert.deepEqual(h.commands,[['A','apply'],['A','remove']]);assert.equal(h.reports.at(-1).state,'waiting-gaming-mode');
});
test('persistencia: contexto ausente o ambiguo no se parchea',async()=>{
  const h=fixture();h.targets([]);await h.supervisor.tick();h.targets([{id:'A'},{id:'B'}]);await h.supervisor.tick();
  assert.deepEqual(h.commands,[]);assert.equal(h.reports.at(-1).state,'ambiguous-shared-context');
});
test('persistencia: desconexión recuperable no mata el servicio',async()=>{
  const h=fixture();await h.supervisor.tick();h.fail(true);await h.supervisor.tick();
  assert.equal(h.reports.at(-1).state,'waiting-or-incompatible');assert.ok(h.clients[0].closed);
  h.fail(false);await h.supervisor.tick();assert.equal(h.reports.at(-1).state,'attached');
});
test('persistencia: stop espera el tick en vuelo, revierte y prohíbe reinyección',async()=>{
  const h=fixture();let release;h.block(new Promise(r=>release=r));
  const tick=h.supervisor.tick();assert.equal(h.supervisor.tick(),tick);
  const stopping=h.supervisor.stop();release();await stopping;
  const length=h.commands.length;await h.supervisor.tick();assert.equal(h.commands.length,length);
  assert.equal(h.commands.at(-1)[1],'remove');assert.ok(h.clients[0].closed);
});
test('persistencia: stop recupera conexión perdida para revertir',async()=>{
  const h=fixture();await h.supervisor.tick();h.fail(true);await h.supervisor.tick();h.fail(false);
  await h.supervisor.stop();assert.equal(h.commands.at(-1)[1],'remove');assert.equal(h.reports.at(-1).state,'stopped');
});

function runtimeFixture() {
  class Component {
    shouldComponentUpdate() {return false;}
    componentDidUpdate() {}
    CreatePlayer() {}
    DestroyPlayer() {}
    OnPlayerReady() {}
    componentWillUnmount() {}
  }
  const names=['shouldComponentUpdate','componentDidUpdate','CreatePlayer','DestroyPlayer','OnPlayerReady','componentWillUnmount'];
  const methods=Object.fromEntries(names.map(k=>[k,Component.prototype[k].toString()]));
  function factory() { /* shipped factory fixture */ }
  const expected={...contract,methods,factorySHA256:createHash('sha256').update(factory.toString()).digest('hex')};
  let required=0;
  const req=()=>{required++;return {N1:Component};};req.m={[expected.module]:factory};
  const chunks=[];chunks.push=function(entry){Array.prototype.push.call(this,entry);entry[2](req);};
  const context={window:{webpackChunksteamui:chunks},SteamClient:{System:{GetSystemInfo:async()=>({nSteamVersion:expected.build})}},crypto:webcrypto,TextEncoder};
  const descriptors=Object.getOwnPropertyDescriptors(Component.prototype);
  return {context,expected,proto:Component.prototype,descriptors,required:()=>required,
    run(action='apply'){return vm.runInNewContext(hookExpression(action,{expected}),context);}};
}
test('compatibilidad: build desconocida se rechaza sin tocar prototipo',async()=>{
  const h=runtimeFixture();h.context.SteamClient.System.GetSystemInfo=async()=>({nSteamVersion:1});
  await assert.rejects(h.run(),/Build incompatible/);assert.equal(h.required(),0);
  assert.deepEqual(Object.getOwnPropertyDescriptors(h.proto),h.descriptors);
});
test('compatibilidad: hash desconocido se rechaza incluso con hook residente',async()=>{
  const h=runtimeFixture();h.expected.factorySHA256='0'.repeat(64);
  h.context.window.__UGMYoutubePauseHook={status:()=>({active:true,version:'experimental-1'})};
  await assert.rejects(h.run(),/Módulo cambiado/);assert.equal(h.required(),0);
  assert.deepEqual(Object.getOwnPropertyDescriptors(h.proto),h.descriptors);
});
test('runtime: primera instalación, status, idempotencia y reversión con hash real',async()=>{
  const h=runtimeFixture();const applied=await h.run();assert.ok(applied.active);assert.equal(h.required(),1);
  const descriptor=Object.getOwnPropertyDescriptor(h.proto,'shouldComponentUpdate');
  await h.run();assert.equal(h.required(),1);assert.deepEqual(Object.getOwnPropertyDescriptor(h.proto,'shouldComponentUpdate'),descriptor);
  assert.equal((await h.run('status')).active,true);assert.ok(!('events' in applied));
  assert.equal((await h.run('remove')).active,false);assert.deepEqual(Object.getOwnPropertyDescriptors(h.proto),h.descriptors);
});
test('runtime: adopta el prototipo validado de la sesión sin reemplazarlo',async()=>{
  const h=runtimeFixture();const resident={status:()=>({active:true,version:'experimental-1',counts:{orders:12},events:[{video:'private'}]})};
  h.context.window.__UGMYoutubePauseHook=resident;
  const result=await h.run();assert.equal(result.counts.orders,12);assert.ok(!('events' in result));
  assert.equal(h.required(),0);assert.equal(h.context.window.__UGMYoutubePauseHook,resident);
});
test('runtime: hook residente no reconocido se rechaza',async()=>{
  const h=runtimeFixture();h.context.window.__UGMYoutubePauseHook={status:()=>({active:true,version:'unknown'})};
  await assert.rejects(h.run(),/Hook residente incompatible/);assert.equal(h.required(),0);
});
