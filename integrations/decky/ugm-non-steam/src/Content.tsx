import { useEffect, useSyncExternalStore } from 'react';
import { useQuickAccessVisible } from '@decky/api';
import { ButtonItem, DropdownItem, Focusable, Navigation, PanelSection, PanelSectionRow,
  TextField, ToggleField } from '@decky/ui';
import * as backend from './backend';
import { bridge, compatibilityTools } from './runtime';
import { diskStatus } from './steam';
import { model } from './model';
import { unwrap, type Draft } from './types';

const errorText = (error: unknown) => error instanceof Error ? error.message : 'No se ha podido completar la operación.';
function guard(owner: string) {
  if (model.get().owner !== owner || bridge.account() !== owner) throw new Error('La cuenta Steam ha cambiado. Vuelve a abrir el panel.');
}
async function run(action: (owner: string) => Promise<void>) {
  if (model.get().busy) return;
  const owner = model.get().owner;
  model.patch({ busy: true, error: '', confirmUndo: false });
  try { guard(owner); await action(owner); }
  catch (error) { if (model.get().owner === owner) model.patch({ error: errorText(error) }); }
  finally { if (model.get().owner === owner) model.patch({ busy: false }); }
}
async function refreshTools(owner: string) {
  try {
    const tools = await compatibilityTools(); guard(owner);
    model.patch({ tools, toolError: tools.length ? '' : 'Steam no ha devuelto herramientas Proton.' });
  } catch (error) { if (model.get().owner === owner) model.patch({ tools: [], toolError: errorText(error) }); }
}
async function init() {
  try {
    const owner = bridge.account();
    if (model.get().owner !== owner) model.reset(owner);
    const context = unwrap(await backend.getContext()); guard(owner);
    if (context.version !== '1.0.0') throw new Error('Frontend y backend tienen versiones diferentes. Revisa la instalación.');
    model.patch({ context, path: model.get().path || context.home, error: '' });
    void refreshTools(owner);
  } catch (error) { model.patch({ error: errorText(error) }); }
}
function back() {
  const s = model.get();
  if (!s.busy) model.patch({ screen: s.screen === 'result' && s.draft ? 'form' : 'home', error: '', confirmUndo: false });
}
async function apps(page = 0) {
  await run(async owner => {
    const data = unwrap(await backend.listApplications(model.get().query, page)); guard(owner);
    model.patch({ apps: data, screen: 'apps' });
  });
}
async function files(path: string, page = 0) {
  await run(async owner => {
    const s = model.get();
    const data = unwrap(await backend.browse(path, s.query, page, s.hidden)); guard(owner);
    model.patch({ files: data, path: data.path, screen: 'files' });
  });
}
async function chooseFile(path: string) {
  await run(async owner => {
    const draft = unwrap(await backend.inspectPath(path)); guard(owner);
    model.patch({ draft, result: null, disk: 'unknown', screen: 'form' });
  });
}
function chooseApp(draft: Draft) {
  model.patch({ draft: { ...draft }, result: null, disk: 'unknown', screen: 'form', error: '' });
}
function patchDraft(patch: Partial<Draft>) {
  const draft = model.get().draft;
  if (draft && !model.get().busy) model.patch({ draft: { ...draft, ...patch } });
}
async function add() {
  await run(async owner => {
    const state = model.get();
    if (!state.draft) return;
    const draft = unwrap(await backend.validateDraft(state.draft)); guard(owner);
    if (draft.compatTool && !state.tools.some(t => t.strToolName === draft.compatTool))
      throw new Error('La herramienta Proton seleccionada ya no está disponible. Actualiza la lista.');
    const result = await bridge.add(draft); guard(owner);
    model.patch({ result, screen: 'result', disk: 'unknown' });
  });
}
async function checkDisk() {
  await run(async owner => {
    const result = model.get().result;
    if (!result || result.account !== owner) throw new Error('El acceso no pertenece a esta cuenta.');
    const saved = unwrap(await backend.savedShortcuts(owner)); guard(owner);
    model.patch({ disk: diskStatus(result, saved.items) });
  });
}
async function undo() {
  await run(async owner => {
    const result = model.get().result;
    if (!result) return;
    await bridge.undo(result); guard(owner);
    model.patch({ result: null, screen: 'form', disk: 'unknown' });
  });
}
const Note = ({ children }: { children: React.ReactNode }) => <PanelSectionRow>
  <div style={{ fontSize: 12, lineHeight: 1.5, opacity: .85, overflowWrap: 'anywhere' }}>{children}</div>
</PanelSectionRow>;
const Button = ({ children, onClick, disabled = false }: { children: React.ReactNode; onClick: () => void; disabled?: boolean }) =>
  <PanelSectionRow><ButtonItem layout="below" disabled={disabled || model.get().busy} onClick={onClick}>{children}</ButtonItem></PanelSectionRow>;

export function Content() {
  const s = useSyncExternalStore(model.subscribe, model.get);
  const visible = useQuickAccessVisible();
  useEffect(() => {
    if (visible) void init();
    const timer = setInterval(() => {
      try {
        const owner = bridge.account();
        if (model.get().owner && model.get().owner !== owner) { model.reset(owner); void init(); }
      } catch { /* During Steam shutdown, never call a mutation. */ }
    }, 1500);
    return () => clearInterval(timer);
  }, [visible]);
  const d = s.draft;
  const page = s.screen === 'apps' ? s.apps : s.files;
  return <Focusable onCancel={event => { if (s.screen !== 'home') { event.stopPropagation(); back(); } }}>
    <PanelSection title="Añadir contenido Non-Steam">
      {s.screen !== 'home' && <Button onClick={back}>← Volver</Button>}
      {s.screen === 'home' && <>
        <Note>Añade programas o juegos ya instalados a tu biblioteca sin salir de Gaming Mode.</Note>
        <Button disabled={!s.context} onClick={() => { model.patch({ query: '' }); void apps(); }}>Aplicaciones instaladas</Button>
        <Button disabled={!s.context} onClick={() => { model.patch({ query: '' }); void files(s.path || s.context!.home); }}>Buscar ejecutable</Button>
        {d && <Button onClick={() => model.patch({ screen: 'form' })}>Continuar con «{d.name}»</Button>}
        {s.result && <Button onClick={() => model.patch({ screen: 'result' })}>Ver último acceso añadido</Button>}
        <Note>Linux · AppImage · Flatpak · Snap · ejecutables Windows con Proton. No instala juegos ni tiendas.</Note>
        <Button onClick={() => { void init(); }}>Actualizar detección</Button>
      </>}
      {(s.screen === 'apps' || s.screen === 'files') && <>
        <PanelSectionRow><TextField label="Filtrar" disabled={s.busy} value={s.query}
          onChange={event => model.patch({ query: event.target.value })} /></PanelSectionRow>
        <Button onClick={() => { void (s.screen === 'apps' ? apps() : files(s.path)); }}>Buscar / actualizar lista</Button>
        {s.screen === 'files' && <>
          <PanelSectionRow><TextField label="Carpeta" disabled={s.busy} value={s.path}
            onChange={event => model.patch({ path: event.target.value })} /></PanelSectionRow>
          <Button onClick={() => { model.patch({ query: '' }); void files(s.path); }}>Abrir carpeta</Button>
          {s.context?.locations.map(place => <Button key={place.path} onClick={() => {
            model.patch({ query: '' }); void files(place.path);
          }}>{place.name}</Button>)}
          <Button disabled={!s.files || s.files.path === '/'} onClick={() => {
            model.patch({ query: '' }); void files(s.files!.parent);
          }}>↑ Carpeta superior</Button>
          <PanelSectionRow><ToggleField label="Mostrar archivos ocultos" checked={s.hidden} disabled={s.busy}
            onChange={hidden => { model.patch({ hidden }); void files(s.files?.path || s.path); }} /></PanelSectionRow>
          <Note>{s.files?.path}</Note>
          {s.files?.items.map(item => <Button key={item.path} onClick={() => {
            if (item.folder) { model.patch({ query: '' }); void files(item.path); }
            else void chooseFile(item.path);
          }}>{item.folder ? '▸ ' : ''}{item.name}</Button>)}
        </>}
        {s.screen === 'apps' && s.apps?.items.map(app => <Button key={app.id} onClick={() => chooseApp(app)}>{app.name} · {app.source}</Button>)}
        {page && <>
          <Note>{page.total ? `${page.total} resultados · página ${page.page + 1} de ${Math.ceil(page.total / 40)}` : 'No hay resultados. Prueba otra carpeta o filtro.'}</Note>
          <Button disabled={page.page === 0} onClick={() => { void (s.screen === 'apps' ? apps(page.page - 1) : files(s.files!.path, page.page - 1)); }}>Página anterior</Button>
          <Button disabled={(page.page + 1) * 40 >= page.total} onClick={() => { void (s.screen === 'apps' ? apps(page.page + 1) : files(s.files!.path, page.page + 1)); }}>Página siguiente</Button>
        </>}
      </>}
      {s.screen === 'form' && d && <>
        <Note>{d.source} · Revisa los datos del acceso.</Note>
        <PanelSectionRow><TextField label="Nombre en Steam" value={d.name} disabled={s.busy} onChange={e => patchDraft({ name: e.target.value })} /></PanelSectionRow>
        <Note>Destino: {d.exe}</Note>
        <PanelSectionRow><TextField label="Carpeta de inicio" value={d.startDir} disabled={s.busy} onChange={e => patchDraft({ startDir: e.target.value })} /></PanelSectionRow>
        <PanelSectionRow><TextField label="Argumentos (opcional)" value={d.launchOptions} disabled={s.busy} onChange={e => patchDraft({ launchOptions: e.target.value })} /></PanelSectionRow>
        <PanelSectionRow><DropdownItem label="Compatibilidad" disabled={s.busy} selectedOption={d.compatTool}
          rgOptions={[{ data: '', label: d.windows ? 'Selecciona Proton…' : 'Linux nativo (sin Proton)' }, ...s.tools.map(t => ({ data: t.strToolName, label: t.strDisplayName }))]}
          onChange={value => patchDraft({ compatTool: String(value.data) })} /></PanelSectionRow>
        {(d.windows || s.toolError) && <Note>{d.windows ? 'Windows requiere Proton. Añadir un .exe no instala dependencias ni garantiza que funcione. ' : ''}{s.toolError}</Note>}
        <Button onClick={() => { void run(refreshTools); }}>Actualizar lista de Proton</Button>
        {d.executable === false && <>
          <Note>El programa no tiene permiso de ejecución. Este botón añade únicamente el permiso de ejecución para tu usuario.</Note>
          <Button onClick={() => { void run(async owner => {
            const info = unwrap(await backend.makeExecutable(d.exe)); guard(owner);
            model.patch({ draft: { ...d, executable: info.executable } });
          }); }}>Permitir ejecución de este archivo</Button>
        </>}
        <Button disabled={!d.name.trim() || d.executable === false || (d.windows && !d.compatTool) || (!!d.compatTool && !s.tools.some(t => t.strToolName === d.compatTool))}
          onClick={() => { void add(); }}>Añadir a Steam</Button>
        <Button onClick={() => { model.patch({ screen: 'home' }); }}>Elegir otro programa</Button>
      </>}
      {s.screen === 'result' && s.result && <>
        <Note>{s.result.verified ? 'Acceso comprobado en Steam' : 'Acceso creado; comprobación incompleta'}: {s.result.request.name}</Note>
        {s.result.warnings.map((warning, i) => <Note key={i}>{warning}</Note>)}
        <Note>{s.disk === 'saved' ? 'Los datos del acceso coinciden con la copia guardada en disco. Esto no demuestra que el juego funcione.' :
          s.disk === 'pending' ? 'Todavía no aparece con estos datos en disco. Steam puede guardarlo al cerrarse normalmente. No fuerces su cierre.' :
          'Guardado en disco aún no comprobado. El plugin no reinicia Steam ni sale de UGM.'}</Note>
        <Button onClick={() => {
          try { guard(s.result!.account); Navigation.Navigate(`/library/app/${s.result!.appId}`); Navigation.CloseSideMenus(); }
          catch (error) { model.patch({ error: errorText(error) }); }
        }}>Abrir ficha en Steam</Button>
        <Button onClick={() => { void checkDisk(); }}>Comprobar guardado en disco</Button>
        <Button onClick={() => model.patch({ confirmUndo: !s.confirmUndo })}>Deshacer este acceso</Button>
        {s.confirmUndo && <>
          <Note>Solo se retira el acceso creado en esta operación. No se borran archivos, partidas ni prefijos.</Note>
          <Button onClick={() => { void undo(); }}>Confirmar retirada del acceso</Button>
        </>}
        <Button onClick={() => model.patch({ screen: 'home', draft: null, confirmUndo: false })}>Añadir otro programa</Button>
      </>}
      {s.busy && <Note>Procesando…</Note>}
      {s.error && <PanelSectionRow><div role="alert" style={{ color: '#ffb6b6', fontSize: 12, overflowWrap: 'anywhere' }}>{s.error}</div></PanelSectionRow>}
    </PanelSection>
  </Focusable>;
}
