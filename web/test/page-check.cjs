// The page check: web/server.mjs and web/loader.js at full size, with the Emscripten stand-in (fake-emscripten.js)
// in place of the engine. It makes a game folder of placeholder files with the sizes of tools/clean_files/manifest.tsv
// (sparse, so they take no disk), serves it, opens the page in Chromium and checks what the loader handed the engine.
// Needs Playwright: NODE_PATH=$(npm root -g) node web/test/page-check.cjs [screenshot.png]
const { chromium } = require('playwright');
const { spawn } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const ROOT = path.resolve(__dirname, '..', '..');
const PORT = 8097;

(async () => {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'cm0102-page-check-'));
  const game = path.join(tmp, 'game'), build = path.join(tmp, 'build-web');
  const rows = fs.readFileSync(path.join(ROOT, 'tools/clean_files/manifest.tsv'), 'utf8').split('\n')
    .filter((l) => l && !l.startsWith('#')).map((l) => l.split('\t'));
  for (const [, size, file] of rows) {
    const p = path.join(game, file);
    fs.mkdirSync(path.dirname(p), { recursive: true });
    fs.closeSync(fs.openSync(p, 'w'));
    fs.truncateSync(p, Number(size));
  }
  fs.mkdirSync(path.join(build, 'seasons'), { recursive: true });
  for (const t of ['rules', 'names', 'clubs', 'club_records']) fs.writeFileSync(path.join(build, 'seasons', `${t}.tsv`), 'x\n');
  fs.copyFileSync(path.join(__dirname, 'fake-emscripten.js'), path.join(build, 'cm0102_recomp.js'));
  fs.writeFileSync(path.join(build, 'cm0102_recomp.wasm'), Buffer.from([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0]));
  fs.writeFileSync(path.join(build, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=asyncify\n');
  const total = rows.reduce((a, r) => a + Number(r[1]), 0);

  const server = spawn(process.execPath, [path.join(ROOT, 'web/server.mjs')], {
    env: { ...process.env, CM_GAME_DIR: game, CM_WEB_BUILD: build, PORT: String(PORT), CM_WEB_SKIP_VERIFY: '1' }, stdio: ['ignore', 'pipe', 'inherit'],
  });
  await new Promise((r) => server.stdout.on('data', (d) => { process.stdout.write(`server: ${d}`); if (/open http/.test(d)) r(); }));
  let ok = true;
  const check = (what, cond, detail = '') => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}${detail ? `: ${detail}` : ''}`); ok &&= cond; };
  const b = await chromium.launch();
  try {
    const p = await b.newPage({ viewport: { width: 1100, height: 760 }, deviceScaleFactor: 2 });
    const errors = [];
    p.on('pageerror', (e) => errors.push(e.message));
    await p.goto(`http://127.0.0.1:${PORT}/`);
    await p.waitForSelector('#go');
    check('the start box names the size', /760 game files|game files/.test(await p.textContent('#overlay')), (await p.textContent('#overlay h1')).trim());
    const t0 = Date.now();
    await p.click('#go');
    await p.waitForFunction(() => window.__fake, null, { timeout: 120000 });
    const f = await p.evaluate(() => window.__fake);
    check('every file reached the engine', f.files === rows.length + 4, `${f.files} files`);
    check('every byte reached the engine', f.bytes === total + 4 * 2, `${(f.bytes / 1048576).toFixed(0)} MB in ${((Date.now() - t0) / 1000).toFixed(1)} s`);
    check('the file system owns the arrays (no copy)', f.owned);
    check('the exe and the season tables are in /game', f.exe && f.rules);
    check('main gets the game folder', JSON.stringify(f.args) === JSON.stringify(['--game', '/game']), f.args.join(' '));
    check('the user folder is kept in the browser', JSON.stringify(f.mounts) === '["/user"]');
    check('the game writes to the user folder', f.env.RECOMP_USER_DIR === '/user');
    check('the wasm is served as application/wasm', f.wasmType === 'application/wasm', f.wasmType);
    check('the overlay is gone, the canvas shows the engine', (await p.locator('#overlay .box').count()) === 0);
    check('no page errors', errors.length === 0, errors.join('; '));
    if (process.argv[2]) await p.screenshot({ path: process.argv[2] });
  } finally {
    await b.close();
    server.kill();
    fs.rmSync(tmp, { recursive: true, force: true });
  }
  process.exit(ok ? 0 : 1);
})().catch((e) => { console.error(e); process.exit(1); });
