// Which build of the engine this browser runs (web/README.md, "Other browsers"). The server may offer two: the JSPI
// build (Chrome 137 and later: smaller and faster) and the Asyncify build (CM_WEB_BUILD_ASYNCIFY: every browser, so
// Safari and every iPad browser). files.json lists them under engines, each as { base, suspend, version, bytes }.

export const hasJspi = () => typeof WebAssembly === 'object'
  && typeof WebAssembly.Suspending === 'function' && typeof WebAssembly.promising === 'function';

// The JSPI build where the browser has JSPI, else the Asyncify build; with neither that fits, the first (the page's
// requirements then say why it cannot run). A build whose kind is unknown (no CMakeCache.txt) counts as JSPI.
export function pickEngine(info, jspi = hasJspi()) {
  const list = info.engines ?? (info.engine
    ? [{ base: '/engine/', suspend: info.suspend ?? null, version: info.engineVersion, bytes: info.engineBytes ?? 0 }] : []);
  return (jspi && list.find((e) => e.suspend !== 'asyncify')) || list.find((e) => e.suspend === 'asyncify') || list[0] || null;
}

// The address of one of the engine's files (cm0102_recomp.js or .wasm) in the chosen build, under its version: the
// browser and the service worker keep it for good.
export const engineUrl = (engine, name) => `${engine.base}${name}?v=${engine.version}`;
