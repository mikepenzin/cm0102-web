// The web app: serves the browser build of the recompiled game and the player's game files to a page that runs the
// game in the browser (web/README.md). The browser does all the work; this server only hands out files.
//
//   CM_GAME_DIR=<clean files> node web/server.mjs        then open http://127.0.0.1:8080 in Chrome
//
// CM_GAME_DIR  the clean files: the free release with the official patch (tools/clean_files/make_clean_files.py),
//              or the game folder of a Bosman Edition download, which holds the same files
// CM_WEB_BUILD the browser build folder (default build-web): cm0102_recomp.js, .wasm and seasons/*.tsv
// PORT, HOST   default 8080 and 127.0.0.1
//
// The page gets exactly the files of tools/clean_files/manifest.tsv, each checked for its size, and the season
// tables that the build makes (Seasons/*.tsv, which the runtime reads from the game folder). Nothing else is served.
import http from 'node:http';
import { createReadStream, existsSync, readFileSync, statSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, '..');
const SEASON_TABLES = ['rules.tsv', 'names.tsv', 'clubs.tsv', 'club_records.tsv'];
const PAGES = { '/': 'index.html', '/loader.js': 'loader.js' };
const TYPES = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.wasm': 'application/wasm' };

function manifest(path) {
  return readFileSync(path, 'utf8').split('\n').filter((l) => l && !l.startsWith('#')).map((l) => {
    const [sha256, size, file] = l.split('\t');
    return { path: file, size: Number(size), sha256 };
  });
}

// The files the page loads, the ones missing or of the wrong size, and the engine's files.
export function inventory({ gameDir, buildDir, manifestPath = join(ROOT, 'tools/clean_files/manifest.tsv') }) {
  const files = [], missing = [];
  for (const m of manifest(manifestPath)) {
    const disk = join(gameDir, m.path);
    let size = -1;
    try { size = statSync(disk).size; } catch { /* missing */ }
    if (size === m.size) files.push({ path: m.path, size, disk });
    else missing.push({ path: m.path, want: m.size, have: size });
  }
  for (const name of SEASON_TABLES) {
    const disk = join(buildDir, 'seasons', name);
    if (existsSync(disk)) files.push({ path: `Seasons/${name}`, size: statSync(disk).size, disk });
    else missing.push({ path: `Seasons/${name}`, want: null, have: -1, hint: 'build the target seasons_rules' });
  }
  const engine = {};
  for (const ext of ['js', 'wasm']) {
    const disk = join(buildDir, `cm0102_recomp.${ext}`);
    if (existsSync(disk)) engine[ext] = { disk, size: statSync(disk).size, mtime: statSync(disk).mtimeMs };
  }
  let suspend = null;
  try { suspend = /CM0102_WEB_SUSPEND:STRING=(\w+)/.exec(readFileSync(join(buildDir, 'CMakeCache.txt'), 'utf8'))?.[1] ?? null; }
  catch { /* no cache: not a CMake build folder */ }
  return { files, missing, engine, suspend };
}

function sendFile(req, res, disk, type) {
  const st = statSync(disk);
  const etag = `"${st.size.toString(16)}-${Math.floor(st.mtimeMs).toString(16)}"`;
  const headers = { 'Content-Type': type, 'Cache-Control': 'no-cache', ETag: etag };
  if (req.headers['if-none-match'] === etag) { res.writeHead(304, headers); res.end(); return; }
  res.writeHead(200, { ...headers, 'Content-Length': st.size });
  if (req.method === 'HEAD') { res.end(); return; }
  createReadStream(disk).pipe(res);
}

export function createApp({ gameDir, buildDir, manifestPath }) {
  let inv = inventory({ gameDir, buildDir, manifestPath });
  return http.createServer((req, res) => {
    try {
      if (req.method !== 'GET' && req.method !== 'HEAD') { res.writeHead(405); res.end(); return; }
      const path = decodeURIComponent(new URL(req.url, 'http://x').pathname);
      if (PAGES[path]) { sendFile(req, res, join(HERE, PAGES[path]), TYPES[PAGES[path].slice(PAGES[path].lastIndexOf('.'))]); return; }
      if (path === '/files.json') {
        inv = inventory({ gameDir, buildDir, manifestPath });   // a rebuild or new files count without a restart
        const body = JSON.stringify({
          files: inv.files.map(({ path: p, size }) => ({ path: p, size })),
          totalBytes: inv.files.reduce((a, f) => a + f.size, 0),
          missing: inv.missing,
          engine: Boolean(inv.engine.js && inv.engine.wasm),
          engineBytes: inv.engine.wasm?.size ?? 0,
          engineVersion: inv.engine.wasm ? Math.floor(inv.engine.wasm.mtime).toString(36) : null,
          suspend: inv.suspend,
        });
        res.writeHead(200, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
        res.end(body);
        return;
      }
      const engine = /^\/engine\/cm0102_recomp\.(js|wasm)$/.exec(path);
      if (engine && inv.engine[engine[1]]) { sendFile(req, res, inv.engine[engine[1]].disk, TYPES[`.${engine[1]}`]); return; }
      if (path.startsWith('/game/')) {
        const f = inv.files.find((x) => x.path === path.slice(6));    // only listed files: no path leaves the list
        if (f) { sendFile(req, res, f.disk, 'application/octet-stream'); return; }
      }
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('not found');
    } catch (err) {
      res.writeHead(500, { 'Content-Type': 'text/plain' });
      res.end(err.message);
    }
  });
}

// The files of the manifest whose SHA-256 differs from it (a folder that is not the clean files).
export async function verify(inv, manifestPath = join(ROOT, 'tools/clean_files/manifest.tsv')) {
  const { createHash } = await import('node:crypto');
  const want = new Map(manifest(manifestPath).map((m) => [m.path, m.sha256]));
  const bad = [];
  for (const f of inv.files) {
    if (!want.has(f.path)) continue;                    // the season tables come from the build
    const h = createHash('sha256');
    for await (const chunk of createReadStream(f.disk)) h.update(chunk);
    if (h.digest('hex') !== want.get(f.path)) bad.push(f.path);
  }
  return bad;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const gameDir = process.env.CM_GAME_DIR ? resolve(process.env.CM_GAME_DIR) : null;
  if (!gameDir || !existsSync(join(gameDir, 'cm0102.exe'))) {
    console.error('Set CM_GAME_DIR to the folder of the clean files (with cm0102.exe and Data). See web/README.md.');
    process.exit(2);
  }
  const buildDir = resolve(process.env.CM_WEB_BUILD ?? join(ROOT, 'build-web'));
  const inv = inventory({ gameDir, buildDir });
  const mb = (n) => `${(n / 1048576).toFixed(0)} MB`;
  console.log(`game files: ${inv.files.length} (${mb(inv.files.reduce((a, f) => a + f.size, 0))}) from ${gameDir}`);
  if (!process.env.CM_WEB_SKIP_VERIFY) {
    const bad = await verify(inv);
    if (bad.length) {
      console.error(`${bad.length} files differ from the clean files (tools/clean_files/manifest.tsv), first: ${bad.slice(0, 5).join(', ')}`);
      process.exit(3);
    }
    console.log('game files: every SHA-256 matches the clean files');
  }
  if (inv.missing.length) console.log(`missing or of another size: ${inv.missing.length}, first: ${inv.missing.slice(0, 5).map((m) => m.path).join(', ')}`);
  console.log(inv.engine.js && inv.engine.wasm ? `engine: ${buildDir}/cm0102_recomp.wasm (${mb(inv.engine.wasm.size)}, ${inv.suspend ?? 'unknown'} build)`
    : `engine: not built in ${buildDir} (web/README.md, step 3)`);
  const port = Number(process.env.PORT ?? 8080), host = process.env.HOST ?? '127.0.0.1';
  createApp({ gameDir, buildDir }).listen(port, host, () => console.log(`open http://${host}:${port} in Chrome`));
}
