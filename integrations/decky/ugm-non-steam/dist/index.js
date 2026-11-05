const manifest = {"name":"UGM Añadir contenido Non-Steam"};
const API_VERSION = 2;
const internalAPIConnection = window.__DECKY_SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_deckyLoaderAPIInit;
if (!internalAPIConnection) {
    throw new Error('[@decky/api]: Failed to connect to the loader as as the loader API was not initialized. This is likely a bug in Decky Loader.');
}
let api;
try {
    api = internalAPIConnection.connect(API_VERSION, manifest.name);
}
catch {
    api = internalAPIConnection.connect(1, manifest.name);
    console.warn(`[@decky/api] Requested API version ${API_VERSION} but the running loader only supports version 1. Some features may not work.`);
}
if (api._version != API_VERSION) {
    console.warn(`[@decky/api] Requested API version ${API_VERSION} but the running loader only supports version ${api._version}. Some features may not work.`);
}
const callable = api.callable;
const useQuickAccessVisible = api.useQuickAccessVisible;
const definePlugin = (fn) => {
    return (...args) => {
        return fn(...args);
    };
};

const getContext = callable('get_context');
const listApplications = callable('list_applications');
const browse = callable('browse');
const inspectPath = callable('inspect_path');
const makeExecutable = callable('make_executable');
const validateDraft = callable('validate_draft');
const savedShortcuts = callable('get_saved_shortcuts');

const quote = (value) => '"' + value.replace(/[\\"$`]/g, '\\$&') + '"';
function unquote(value) {
    if (value.startsWith('"') && value.endsWith('"'))
        return value.slice(1, -1).replace(/\\([\\"$`])/g, '$1');
    return value;
}
const sameTarget = (a, b) => unquote(a.exe) === unquote(b.exe) && a.launchOptions === b.launchOptions;
function diskStatus(result, saved) {
    return saved.some(s => (s.appId >>> 0) === (result.appId >>> 0) && s.name === result.request.name &&
        sameTarget(s, result.request) && unquote(s.startDir) === result.request.startDir) ? 'saved' : 'pending';
}
class SteamBridge {
    constructor(runtime) {
        this.runtime = runtime;
        this.busy = false;
        this.owned = new Map();
    }
    account() {
        const value = this.runtime.account();
        if (!/^7656\d{13}$/.test(value))
            throw new Error('No se ha identificado la cuenta Steam activa.');
        return value;
    }
    assertAccount(expected) {
        if (this.account() !== expected)
            throw new Error('La cuenta Steam ha cambiado. Vuelve a abrir el plugin.');
    }
    requireApi(names) {
        for (const name of names)
            if (typeof this.runtime.apps?.[name] !== 'function')
                throw new Error(`Este cliente Steam no ofrece ${name}. No se ha modificado la biblioteca.`);
    }
    details(id, timeout = 1500) {
        return new Promise(resolve => {
            let done = false;
            let registration;
            let timer;
            const finish = (value) => {
                if (done)
                    return;
                done = true;
                if (timer)
                    clearTimeout(timer);
                try {
                    registration?.unregister();
                }
                catch { /* unloaded */ }
                resolve(value);
            };
            timer = setTimeout(() => finish(null), timeout);
            try {
                registration = this.runtime.apps.RegisterForAppDetails(id, value => {
                    if (value && typeof value.strShortcutExe === 'string' && value.strShortcutExe)
                        finish(value);
                });
                if (done)
                    registration?.unregister(); // synchronous callback
            }
            catch {
                finish(null);
            }
        });
    }
    list() {
        const all = this.runtime.overviews();
        if (!Array.isArray(all))
            throw new Error('No se puede consultar la biblioteca para evitar duplicados.');
        return all.filter(item => item.BIsShortcut?.() || item.app_type === 1073741824);
    }
    coreMatches(details, request, overview) {
        return (details.strDisplayName ?? overview?.display_name) === request.name &&
            unquote(details.strShortcutExe ?? '') === request.exe &&
            unquote(details.strShortcutStartDir ?? '') === request.startDir &&
            (details.strShortcutLaunchOptions ?? details.strLaunchOptions) === request.launchOptions;
    }
    async add(request) {
        if (this.busy)
            throw new Error('Ya hay una operación en curso.');
        this.busy = true;
        try {
            this.requireApi(['AddShortcut', 'SetShortcutName', 'SetShortcutExe', 'SetShortcutStartDir',
                'SetShortcutLaunchOptions', 'RegisterForAppDetails', 'RemoveShortcut']);
            if (request.compatTool)
                this.requireApi(['SpecifyCompatTool']);
            const account = this.account();
            const before = this.list();
            for (let offset = 0; offset < before.length; offset += 8) {
                const details = await Promise.all(before.slice(offset, offset + 8).map(item => this.details(item.appid)));
                this.assertAccount(account);
                if (details.some(item => !item))
                    throw new Error('Steam no ha permitido comprobar todos los accesos existentes. Reintenta cuando termine de cargar.');
                for (const item of details) {
                    const args = item.strShortcutLaunchOptions ?? item.strLaunchOptions;
                    if (args === undefined)
                        throw new Error('Steam no expone los argumentos; no se puede descartar un duplicado.');
                    if (sameTarget(request, { exe: item.strShortcutExe, launchOptions: args }))
                        throw new Error('Este destino con los mismos argumentos ya está en tu biblioteca. No se ha añadido otro acceso.');
                }
            }
            const current = this.list();
            if (current.some(item => !before.some(old => old.appid === item.appid)))
                throw new Error('La biblioteca ha cambiado durante la comprobación. Pulsa Añadir de nuevo.');
            this.assertAccount(account);
            let appId = 0;
            for (const target of [request.exe, quote(request.exe)]) {
                // Never retry an exception: Steam might already have created an entry.
                let raw;
                try {
                    raw = await this.runtime.apps.AddShortcut(request.name, target, '', '');
                }
                catch {
                    throw new Error('Steam no ha confirmado la operación. Comprueba la biblioteca antes de reintentar: podría haber creado el acceso.');
                }
                this.assertAccount(account);
                if (typeof raw !== 'number' || !Number.isInteger(raw) || raw < -2147483648 || raw > 4294967295)
                    throw new Error('Respuesta inesperada de Steam. Comprueba la biblioteca antes de reintentar.');
                if (raw !== 0) {
                    appId = raw >>> 0;
                    break;
                }
            }
            if (!appId)
                throw new Error('Steam no ha creado el acceso. No reinicies la sesión desde el plugin.');
            if (before.some(item => (item.appid >>> 0) === appId))
                throw new Error('Steam ha devuelto un acceso que ya existía. No se ha cambiado ni eliminado.');
            const result = { appId, account, request: { ...request }, verified: false, warnings: [] };
            this.owned.set(appId, result);
            try {
                for (let attempt = 0; attempt < 50 && !this.runtime.overview(appId); attempt++) {
                    await this.runtime.sleep(100);
                    this.assertAccount(account);
                }
                if (!this.runtime.overview(appId))
                    throw new Error('Steam aún no ha registrado todos los datos del acceso.');
                const apps = this.runtime.apps;
                for (const action of [
                    () => apps.SetShortcutName(appId, request.name),
                    () => apps.SetShortcutExe(appId, quote(request.exe)),
                    () => apps.SetShortcutStartDir(appId, request.startDir),
                    () => apps.SetShortcutLaunchOptions(appId, request.launchOptions),
                    () => apps.SetAppLaunchOptions?.(appId, request.launchOptions),
                    () => request.compatTool ? apps.SpecifyCompatTool(appId, request.compatTool) : undefined,
                ]) {
                    this.assertAccount(account);
                    await action();
                }
                for (let attempt = 0; attempt < 5; attempt++) {
                    await this.runtime.sleep(250);
                    this.assertAccount(account);
                    const details = await this.details(appId);
                    this.assertAccount(account);
                    if (details && this.coreMatches(details, request, this.runtime.overview(appId)) &&
                        (!request.compatTool || details.strCompatToolName === request.compatTool)) {
                        result.verified = true;
                        break;
                    }
                }
                if (!result.verified)
                    result.warnings.push('Acceso creado, pero Steam no ha confirmado todos los campos o Proton. Revisa sus propiedades antes de usarlo.');
            }
            catch (error) {
                result.warnings.push(error instanceof Error ? error.message : String(error));
            }
            return result;
        }
        finally {
            this.busy = false;
        }
    }
    async undo(result) {
        if (this.busy)
            throw new Error('Ya hay una operación en curso.');
        if (this.owned.get(result.appId) !== result)
            throw new Error('Este acceso no pertenece a la operación actual.');
        this.busy = true;
        try {
            this.assertAccount(result.account);
            const overview = this.runtime.overview(result.appId);
            if (!overview)
                throw new Error('El acceso ya no está en la biblioteca.');
            const details = await this.details(result.appId);
            this.assertAccount(result.account);
            if (!details || !this.coreMatches(details, result.request, overview))
                throw new Error('El acceso ha cambiado o no se puede comprobar. No se elimina; puedes gestionarlo desde Steam.');
            await this.runtime.apps.RemoveShortcut(result.appId);
            for (let i = 0; i < 30; i++) {
                this.assertAccount(result.account);
                if (!this.runtime.overview(result.appId)) {
                    this.owned.delete(result.appId);
                    return;
                }
                await this.runtime.sleep(100);
            }
            throw new Error('Steam no ha confirmado la retirada del acceso. Comprueba su ficha.');
        }
        finally {
            this.busy = false;
        }
    }
}

// In the response, field 1 is a tool and field 2 an alias such as "Proton
// Stable". Both are valid choices for SpecifyCompatTool.
const FIELD_TOOL = 1;
const FIELD_ALIAS = 2;
const TOOL_NAME = 1;
const TOOL_DISPLAY_NAME = 2;
const TOOL_INCOMPATIBLE = 6;
class CompatToolsResponse {
    constructor() {
        this.tools = [];
    }
    static deserializeBinaryFromReader(response, reader) {
        while (reader.nextField() && !reader.isEndGroup()) {
            const field = reader.getFieldNumber();
            if (field !== FIELD_TOOL && field !== FIELD_ALIAS) {
                reader.skipField();
                continue;
            }
            const tool = { strToolName: "", strDisplayName: "", incompatible: false };
            reader.readMessage(tool, (item, fields) => {
                while (fields.nextField() && !fields.isEndGroup()) {
                    const number = fields.getFieldNumber();
                    if (number === TOOL_NAME)
                        item.strToolName = fields.readString();
                    else if (number === TOOL_DISPLAY_NAME)
                        item.strDisplayName = fields.readString();
                    else if (number === TOOL_INCOMPATIBLE && field === FIELD_TOOL)
                        item.incompatible = fields.readBool();
                    else
                        fields.skipField();
                }
            });
            response.tools.push(tool);
        }
    }
}
async function getCompatManagerTools() {
    const transport = DFL.findModuleExport((value) => value?.SendMsg && value?.GetConnectionDetails);
    const message = DFL.findModuleExport((value) => value?.Init && value?.InitFromMsg && value?.InitFromPacket);
    const protobuf = DFL.findModule((value) => value?.BinaryWriter && value?.BinaryReader);
    if (!transport || !message || !protobuf) {
        throw new Error("Steam's CompatManager service was not found");
    }
    const { BinaryWriter: Writer } = protobuf;
    class EmptyRequest {
        serializeBinary() {
            return new Writer().getResultBuffer();
        }
    }
    const response = await transport.SendMsg("CompatManager.GetCompatTools#1", message.Init(EmptyRequest), CompatToolsResponse, { ePrivilege: 1, eClientExecutionSite: 2 });
    if (!response.BIsValid() || !response.BSuccess()) {
        throw new Error(response.GetErrorMessage());
    }
    return response
        .Body()
        .tools.filter((tool) => tool.strToolName && !tool.incompatible)
        .map(({ strToolName, strDisplayName }) => ({
        strToolName,
        strDisplayName: strDisplayName || strToolName,
    }));
}

const bridge = new SteamBridge({
    get apps() { return SteamClient.Apps; },
    account: () => window.App?.m_CurrentUser?.strSteamID ?? '',
    overviews: () => {
        const store = window.appStore;
        if (!Array.isArray(store?.allApps))
            throw new Error('La biblioteca Steam aún no está disponible. Vuelve a abrir el panel.');
        return store.allApps;
    },
    overview: id => window.appStore?.GetAppOverviewByAppID(id),
    sleep: ms => new Promise(resolve => setTimeout(resolve, ms)),
});
function timeout(promise, ms) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(new Error('Steam no ha respondido a la consulta de Proton.')), ms);
        promise.then(value => { clearTimeout(timer); resolve(value); }, error => { clearTimeout(timer); reject(error); });
    });
}
async function compatibilityTools() {
    const legacy = SteamClient.Settings;
    let tools = [];
    try {
        if (typeof legacy?.GetGlobalCompatTools === 'function')
            tools = await timeout(legacy.GetGlobalCompatTools(), 4000);
    }
    catch { /* current service next */ }
    if (!tools.length)
        tools = await timeout(getCompatManagerTools(), 4000);
    return [...new Map(tools.filter(t => t?.strToolName).map(t => [t.strToolName, t])).values()];
}

const initial = () => ({ screen: 'home', owner: '', context: null, tools: [], toolError: '',
    apps: null, files: null, query: '', path: '', hidden: false, draft: null, result: null,
    disk: 'unknown', error: '', busy: false, confirmUndo: false });
function createModel() {
    let state = initial();
    const listeners = new Set();
    return {
        get: () => state,
        subscribe: (listener) => { listeners.add(listener); return () => { listeners.delete(listener); }; },
        patch: (value) => { state = { ...state, ...value }; listeners.forEach(fn => fn()); },
        reset: (owner) => { state = { ...initial(), owner }; listeners.forEach(fn => fn()); },
    };
}
// In memory only: survives QAM remounts, clears on account change/plugin unload.
const model = createModel();

function unwrap(value) {
    if (!value.ok)
        throw new Error(value.error);
    return value.data;
}

const errorText = (error) => error instanceof Error ? error.message : 'No se ha podido completar la operación.';
function guard(owner) {
    if (model.get().owner !== owner || bridge.account() !== owner)
        throw new Error('La cuenta Steam ha cambiado. Vuelve a abrir el panel.');
}
async function run(action) {
    if (model.get().busy)
        return;
    const owner = model.get().owner;
    model.patch({ busy: true, error: '', confirmUndo: false });
    try {
        guard(owner);
        await action(owner);
    }
    catch (error) {
        if (model.get().owner === owner)
            model.patch({ error: errorText(error) });
    }
    finally {
        if (model.get().owner === owner)
            model.patch({ busy: false });
    }
}
async function refreshTools(owner) {
    try {
        const tools = await compatibilityTools();
        guard(owner);
        model.patch({ tools, toolError: tools.length ? '' : 'Steam no ha devuelto herramientas Proton.' });
    }
    catch (error) {
        if (model.get().owner === owner)
            model.patch({ tools: [], toolError: errorText(error) });
    }
}
async function init() {
    try {
        const owner = bridge.account();
        if (model.get().owner !== owner)
            model.reset(owner);
        const context = unwrap(await getContext());
        guard(owner);
        if (context.version !== '1.0.0')
            throw new Error('Frontend y backend tienen versiones diferentes. Revisa la instalación.');
        model.patch({ context, path: model.get().path || context.home, error: '' });
        void refreshTools(owner);
    }
    catch (error) {
        model.patch({ error: errorText(error) });
    }
}
function back() {
    const s = model.get();
    if (!s.busy)
        model.patch({ screen: s.screen === 'result' && s.draft ? 'form' : 'home', error: '', confirmUndo: false });
}
async function apps(page = 0) {
    await run(async (owner) => {
        const data = unwrap(await listApplications(model.get().query, page));
        guard(owner);
        model.patch({ apps: data, screen: 'apps' });
    });
}
async function files(path, page = 0) {
    await run(async (owner) => {
        const s = model.get();
        const data = unwrap(await browse(path, s.query, page, s.hidden));
        guard(owner);
        model.patch({ files: data, path: data.path, screen: 'files' });
    });
}
async function chooseFile(path) {
    await run(async (owner) => {
        const draft = unwrap(await inspectPath(path));
        guard(owner);
        model.patch({ draft, result: null, disk: 'unknown', screen: 'form' });
    });
}
function chooseApp(draft) {
    model.patch({ draft: { ...draft }, result: null, disk: 'unknown', screen: 'form', error: '' });
}
function patchDraft(patch) {
    const draft = model.get().draft;
    if (draft && !model.get().busy)
        model.patch({ draft: { ...draft, ...patch } });
}
async function add() {
    await run(async (owner) => {
        const state = model.get();
        if (!state.draft)
            return;
        const draft = unwrap(await validateDraft(state.draft));
        guard(owner);
        if (draft.compatTool && !state.tools.some(t => t.strToolName === draft.compatTool))
            throw new Error('La herramienta Proton seleccionada ya no está disponible. Actualiza la lista.');
        const result = await bridge.add(draft);
        guard(owner);
        model.patch({ result, screen: 'result', disk: 'unknown' });
    });
}
async function checkDisk() {
    await run(async (owner) => {
        const result = model.get().result;
        if (!result || result.account !== owner)
            throw new Error('El acceso no pertenece a esta cuenta.');
        const saved = unwrap(await savedShortcuts(owner));
        guard(owner);
        model.patch({ disk: diskStatus(result, saved.items) });
    });
}
async function undo() {
    await run(async (owner) => {
        const result = model.get().result;
        if (!result)
            return;
        await bridge.undo(result);
        guard(owner);
        model.patch({ result: null, screen: 'form', disk: 'unknown' });
    });
}
const Note = ({ children }) => SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx("div", { style: { fontSize: 12, lineHeight: 1.5, opacity: .85, overflowWrap: 'anywhere' }, children: children }) });
const Button = ({ children, onClick, disabled = false }) => SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx(DFL.ButtonItem, { layout: "below", disabled: disabled || model.get().busy, onClick: onClick, children: children }) });
function Content() {
    const s = SP_REACT.useSyncExternalStore(model.subscribe, model.get);
    const visible = useQuickAccessVisible();
    SP_REACT.useEffect(() => {
        if (visible)
            void init();
        const timer = setInterval(() => {
            try {
                const owner = bridge.account();
                if (model.get().owner && model.get().owner !== owner) {
                    model.reset(owner);
                    void init();
                }
            }
            catch { /* During Steam shutdown, never call a mutation. */ }
        }, 1500);
        return () => clearInterval(timer);
    }, [visible]);
    const d = s.draft;
    const page = s.screen === 'apps' ? s.apps : s.files;
    return SP_JSX.jsx(DFL.Focusable, { onCancel: event => { if (s.screen !== 'home') {
            event.stopPropagation();
            back();
        } }, children: SP_JSX.jsxs(DFL.PanelSection, { title: "A\u00F1adir contenido Non-Steam", children: [s.screen !== 'home' && SP_JSX.jsx(Button, { onClick: back, children: "\u2190 Volver" }), s.screen === 'home' && SP_JSX.jsxs(SP_JSX.Fragment, { children: [SP_JSX.jsx(Note, { children: "A\u00F1ade programas o juegos ya instalados a tu biblioteca sin salir de Gaming Mode." }), SP_JSX.jsx(Button, { disabled: !s.context, onClick: () => { model.patch({ query: '' }); void apps(); }, children: "Aplicaciones instaladas" }), SP_JSX.jsx(Button, { disabled: !s.context, onClick: () => { model.patch({ query: '' }); void files(s.path || s.context.home); }, children: "Buscar ejecutable" }), d && SP_JSX.jsxs(Button, { onClick: () => model.patch({ screen: 'form' }), children: ["Continuar con \u00AB", d.name, "\u00BB"] }), s.result && SP_JSX.jsx(Button, { onClick: () => model.patch({ screen: 'result' }), children: "Ver \u00FAltimo acceso a\u00F1adido" }), SP_JSX.jsx(Note, { children: "Linux \u00B7 AppImage \u00B7 Flatpak \u00B7 Snap \u00B7 ejecutables Windows con Proton. No instala juegos ni tiendas." }), SP_JSX.jsx(Button, { onClick: () => { void init(); }, children: "Actualizar detecci\u00F3n" })] }), (s.screen === 'apps' || s.screen === 'files') && SP_JSX.jsxs(SP_JSX.Fragment, { children: [SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx(DFL.TextField, { label: "Filtrar", disabled: s.busy, value: s.query, onChange: event => model.patch({ query: event.target.value }) }) }), SP_JSX.jsx(Button, { onClick: () => { void (s.screen === 'apps' ? apps() : files(s.path)); }, children: "Buscar / actualizar lista" }), s.screen === 'files' && SP_JSX.jsxs(SP_JSX.Fragment, { children: [SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx(DFL.TextField, { label: "Carpeta", disabled: s.busy, value: s.path, onChange: event => model.patch({ path: event.target.value }) }) }), SP_JSX.jsx(Button, { onClick: () => { model.patch({ query: '' }); void files(s.path); }, children: "Abrir carpeta" }), s.context?.locations.map(place => SP_JSX.jsx(Button, { onClick: () => {
                                        model.patch({ query: '' });
                                        void files(place.path);
                                    }, children: place.name }, place.path)), SP_JSX.jsx(Button, { disabled: !s.files || s.files.path === '/', onClick: () => {
                                        model.patch({ query: '' });
                                        void files(s.files.parent);
                                    }, children: "\u2191 Carpeta superior" }), SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx(DFL.ToggleField, { label: "Mostrar archivos ocultos", checked: s.hidden, disabled: s.busy, onChange: hidden => { model.patch({ hidden }); void files(s.files?.path || s.path); } }) }), SP_JSX.jsx(Note, { children: s.files?.path }), s.files?.items.map(item => SP_JSX.jsxs(Button, { onClick: () => {
                                        if (item.folder) {
                                            model.patch({ query: '' });
                                            void files(item.path);
                                        }
                                        else
                                            void chooseFile(item.path);
                                    }, children: [item.folder ? '▸ ' : '', item.name] }, item.path))] }), s.screen === 'apps' && s.apps?.items.map(app => SP_JSX.jsxs(Button, { onClick: () => chooseApp(app), children: [app.name, " \u00B7 ", app.source] }, app.id)), page && SP_JSX.jsxs(SP_JSX.Fragment, { children: [SP_JSX.jsx(Note, { children: page.total ? `${page.total} resultados · página ${page.page + 1} de ${Math.ceil(page.total / 40)}` : 'No hay resultados. Prueba otra carpeta o filtro.' }), SP_JSX.jsx(Button, { disabled: page.page === 0, onClick: () => { void (s.screen === 'apps' ? apps(page.page - 1) : files(s.files.path, page.page - 1)); }, children: "P\u00E1gina anterior" }), SP_JSX.jsx(Button, { disabled: (page.page + 1) * 40 >= page.total, onClick: () => { void (s.screen === 'apps' ? apps(page.page + 1) : files(s.files.path, page.page + 1)); }, children: "P\u00E1gina siguiente" })] })] }), s.screen === 'form' && d && SP_JSX.jsxs(SP_JSX.Fragment, { children: [SP_JSX.jsxs(Note, { children: [d.source, " \u00B7 Revisa los datos del acceso."] }), SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx(DFL.TextField, { label: "Nombre en Steam", value: d.name, disabled: s.busy, onChange: e => patchDraft({ name: e.target.value }) }) }), SP_JSX.jsxs(Note, { children: ["Destino: ", d.exe] }), SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx(DFL.TextField, { label: "Carpeta de inicio", value: d.startDir, disabled: s.busy, onChange: e => patchDraft({ startDir: e.target.value }) }) }), SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx(DFL.TextField, { label: "Argumentos (opcional)", value: d.launchOptions, disabled: s.busy, onChange: e => patchDraft({ launchOptions: e.target.value }) }) }), SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx(DFL.DropdownItem, { label: "Compatibilidad", disabled: s.busy, selectedOption: d.compatTool, rgOptions: [{ data: '', label: d.windows ? 'Selecciona Proton…' : 'Linux nativo (sin Proton)' }, ...s.tools.map(t => ({ data: t.strToolName, label: t.strDisplayName }))], onChange: value => patchDraft({ compatTool: String(value.data) }) }) }), (d.windows || s.toolError) && SP_JSX.jsxs(Note, { children: [d.windows ? 'Windows requiere Proton. Añadir un .exe no instala dependencias ni garantiza que funcione. ' : '', s.toolError] }), SP_JSX.jsx(Button, { onClick: () => { void run(refreshTools); }, children: "Actualizar lista de Proton" }), d.executable === false && SP_JSX.jsxs(SP_JSX.Fragment, { children: [SP_JSX.jsx(Note, { children: "El programa no tiene permiso de ejecuci\u00F3n. Este bot\u00F3n a\u00F1ade \u00FAnicamente el permiso de ejecuci\u00F3n para tu usuario." }), SP_JSX.jsx(Button, { onClick: () => {
                                        void run(async (owner) => {
                                            const info = unwrap(await makeExecutable(d.exe));
                                            guard(owner);
                                            model.patch({ draft: { ...d, executable: info.executable } });
                                        });
                                    }, children: "Permitir ejecuci\u00F3n de este archivo" })] }), SP_JSX.jsx(Button, { disabled: !d.name.trim() || d.executable === false || (d.windows && !d.compatTool) || (!!d.compatTool && !s.tools.some(t => t.strToolName === d.compatTool)), onClick: () => { void add(); }, children: "A\u00F1adir a Steam" }), SP_JSX.jsx(Button, { onClick: () => { model.patch({ screen: 'home' }); }, children: "Elegir otro programa" })] }), s.screen === 'result' && s.result && SP_JSX.jsxs(SP_JSX.Fragment, { children: [SP_JSX.jsxs(Note, { children: [s.result.verified ? 'Acceso comprobado en Steam' : 'Acceso creado; comprobación incompleta', ": ", s.result.request.name] }), s.result.warnings.map((warning, i) => SP_JSX.jsx(Note, { children: warning }, i)), SP_JSX.jsx(Note, { children: s.disk === 'saved' ? 'Los datos del acceso coinciden con la copia guardada en disco. Esto no demuestra que el juego funcione.' :
                                s.disk === 'pending' ? 'Todavía no aparece con estos datos en disco. Steam puede guardarlo al cerrarse normalmente. No fuerces su cierre.' :
                                    'Guardado en disco aún no comprobado. El plugin no reinicia Steam ni sale de UGM.' }), SP_JSX.jsx(Button, { onClick: () => {
                                try {
                                    guard(s.result.account);
                                    DFL.Navigation.Navigate(`/library/app/${s.result.appId}`);
                                    DFL.Navigation.CloseSideMenus();
                                }
                                catch (error) {
                                    model.patch({ error: errorText(error) });
                                }
                            }, children: "Abrir ficha en Steam" }), SP_JSX.jsx(Button, { onClick: () => { void checkDisk(); }, children: "Comprobar guardado en disco" }), SP_JSX.jsx(Button, { onClick: () => model.patch({ confirmUndo: !s.confirmUndo }), children: "Deshacer este acceso" }), s.confirmUndo && SP_JSX.jsxs(SP_JSX.Fragment, { children: [SP_JSX.jsx(Note, { children: "Solo se retira el acceso creado en esta operaci\u00F3n. No se borran archivos, partidas ni prefijos." }), SP_JSX.jsx(Button, { onClick: () => { void undo(); }, children: "Confirmar retirada del acceso" })] }), SP_JSX.jsx(Button, { onClick: () => model.patch({ screen: 'home', draft: null, confirmUndo: false }), children: "A\u00F1adir otro programa" })] }), s.busy && SP_JSX.jsx(Note, { children: "Procesando\u2026" }), s.error && SP_JSX.jsx(DFL.PanelSectionRow, { children: SP_JSX.jsx("div", { role: "alert", style: { color: '#ffb6b6', fontSize: 12, overflowWrap: 'anywhere' }, children: s.error }) })] }) });
}

const AddIcon = () => SP_JSX.jsxs("svg", { width: "1em", height: "1em", viewBox: "0 0 24 24", fill: "none", stroke: "currentColor", strokeWidth: "1.8", "aria-hidden": "true", children: [SP_JSX.jsx("rect", { x: "3", y: "3", width: "18", height: "18", rx: "3" }), SP_JSX.jsx("path", { d: "M12 7v10M7 12h10" })] });
var index = definePlugin(() => ({
    name: 'UGM Añadir contenido Non-Steam',
    titleView: SP_JSX.jsx("div", { style: { fontSize: 14 }, children: "UGM \u00B7 A\u00F1adir contenido Non-Steam" }),
    icon: SP_JSX.jsx(AddIcon, {}), content: SP_JSX.jsx(Content, {}),
}));

export { index as default };
//# sourceMappingURL=index.js.map
