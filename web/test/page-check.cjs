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
  // One file also as brotli, as web/deploy/stage.mjs makes them: the server sends it compressed, the browser unpacks.
  const zlib = require('node:zlib');
  fs.writeFileSync(path.join(game, 'Data/staff.dat.br'), zlib.brotliCompressSync(fs.readFileSync(path.join(game, 'Data/staff.dat'))));

  const server = spawn(process.execPath, [path.join(ROOT, 'web/server.mjs')], {
    env: { ...process.env, CM_GAME_DIR: game, CM_WEB_BUILD: build, PORT: String(PORT), CM_WEB_SKIP_VERIFY: '1' }, stdio: ['ignore', 'pipe', 'inherit'],
  });
  await new Promise((r) => server.stdout.on('data', (d) => { process.stdout.write(`server: ${d}`); if (/open http/.test(d)) r(); }));
  let ok = true;
  const check = (what, cond, detail = '') => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}${detail ? `: ${detail}` : ''}`); ok &&= cond; };
  const b = await chromium.launch();
  try {
    // The requirement gate: each case in a fresh browser, counting what the page asks the server for.
    const gate = async (name, { suspend = 'asyncify', wasm = null, init = null, ctx = {}, click = false }) => {
      fs.writeFileSync(path.join(build, 'CMakeCache.txt'), `CM0102_WEB_SUSPEND:STRING=${suspend}\n`);
      const good = fs.readFileSync(path.join(build, 'cm0102_recomp.wasm'));
      if (wasm) fs.writeFileSync(path.join(build, 'cm0102_recomp.wasm'), wasm);
      const c = await b.newContext({ viewport: { width: 1100, height: 760 }, ...ctx });
      if (init) await c.addInitScript(init);
      const pg = await c.newPage();
      const asked = [];
      pg.on('request', (r) => { const u = new URL(r.url()); if (/^\/(game|engine)\//.test(u.pathname)) asked.push(u.pathname); });
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      await pg.waitForSelector('#go, .box.error');
      if (click && await pg.locator('#go').count()) { await pg.click('#go'); await pg.waitForSelector('.box.error', { timeout: 30000 }); }
      const box = (await pg.textContent('#overlay')).replace(/\s+/g, ' ').trim();
      const result = { box, start: await pg.locator('#go').count(), asked };
      await c.close();
      if (wasm) fs.writeFileSync(path.join(build, 'cm0102_recomp.wasm'), good);
      fs.writeFileSync(path.join(build, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=asyncify\n');
      return result;
    };
    let g = await gate('no JSPI', { suspend: 'jspi', init: () => { delete WebAssembly.Suspending; } });
    check('a JSPI build in a browser without JSPI is blocked before any download',
      /cannot run the game/.test(g.box) && /JSPI/.test(g.box) && g.start === 0 && g.asked.length === 0, g.box.slice(0, 120));
    g = await gate('no memory', { init: () => { WebAssembly.Memory = function () { throw new RangeError('could not allocate memory'); }; } });
    check('a browser that refuses the memory is blocked before any download',
      /memory that the engine needs/.test(g.box) && g.start === 0 && g.asked.length === 0, g.box.slice(0, 120));
    g = await gate('bad engine', { wasm: Buffer.from('this is not WebAssembly'), click: true });
    check('an engine the browser cannot compile stops the start before the game files',
      /cannot compile the engine/.test(g.box) && g.asked.every((u) => u.startsWith('/engine/')) && g.asked.length >= 1,
      `${g.asked.length} requests: ${[...new Set(g.asked)].join(', ')}`);
    g = await gate('touch only', { ctx: { hasTouch: true, isMobile: true } });
    check('a device with no mouse gets a warning and may still start', /no mouse/.test(g.box) && g.start === 1, g.box.slice(0, 160));

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
    // The Saved games panel: the list, a download, uploads, and the watcher's write to IndexedDB.
    await p.waitForSelector('#saves-open:not([hidden])');
    await p.click('#saves-open');
    const listed = await p.textContent('#saves-list');
    check('the panel lists the save, not the save in progress', /Harbour City 2001-07-14\.sav/.test(listed) && !/temp_save_file/.test(listed));
    const [dl] = await Promise.all([p.waitForEvent('download'), p.click('#saves-list button[data-i="0"]')]);
    const dlPath = path.join(tmp, 'downloaded.sav');
    await dl.saveAs(dlPath);
    const got = fs.readFileSync(dlPath);
    const tail = Buffer.from(await p.evaluate(() => window.__fakeSave));
    check('the download is the save, byte for byte', dl.suggestedFilename() === 'Harbour City 2001-07-14.sav'
      && got.length === 0x416ec + 32 && got.subarray(0x416ec - 4).equals(tail), `${dl.suggestedFilename()}, ${got.length} bytes`);
    const storesBefore = await p.evaluate(() => window.__syncs.store);
    await p.setInputFiles('#saves-file', { name: 'Leeds 2002-05-01.sav', mimeType: 'application/octet-stream', buffer: got });
    await p.waitForFunction(() => /Uploaded/.test(document.getElementById('saves-note').textContent));
    const inFs = await p.evaluate(() => window.Module.FS.readFile('/user/Leeds 2002-05-01.sav').length);
    check('an uploaded save lands in the save folder and is listed',
      inFs === got.length && /Leeds 2002-05-01\.sav/.test(await p.textContent('#saves-list')));
    check('an upload is written to IndexedDB at once', (await p.evaluate(() => window.__syncs.store)) > storesBefore);
    await p.setInputFiles('#saves-file', [{ name: 'junk.sav', mimeType: 'application/octet-stream', buffer: Buffer.from('hello') },
      { name: 'notes.txt', mimeType: 'text/plain', buffer: Buffer.from('x') }]);
    await p.waitForFunction(() => /notes\.txt/.test(document.getElementById('saves-note').textContent));
    const refusedNote = await p.textContent('#saves-note');
    check('a file that is not a save, and a file that is not .sav, are refused',
      /junk\.sav: it is too short/.test(refusedNote) && /only \.sav files/.test(refusedNote) && !/junk/.test(await p.textContent('#saves-list')), refusedNote);
    if (process.argv[2]) await p.screenshot({ path: process.argv[2].replace(/\.png$/, '-saves.png') });
    await p.click('#saves-close');
    const storesNow = await p.evaluate(() => window.__syncs.store);
    await p.evaluate(() => window.Module.FS.writeFile('/user/Auto 2001-07-20.sav', new Uint8Array(0x416ec + 8)));
    await p.waitForFunction((n) => window.__syncs.store > n, storesNow, { timeout: 8000 }).catch(() => {});
    check('a new save from the game goes to IndexedDB within seconds', (await p.evaluate(() => window.__syncs.store)) > storesNow);

    const first = (await p.textContent('#log')).match(/page: \d+ files in .*/)?.[0] ?? '';
    check('the first visit downloads every file', /: 0\.0 MB from this browser/.test(first), first);
    // A second visit: every file from the browser's cache storage, none from the server.
    const gameRequests = [];
    p.on('request', (r) => { if (r.url().includes('/game/')) gameRequests.push(r.url()); });
    await p.reload();
    await p.waitForSelector('#go');
    check('the start box says the files are here', /in this browser already/.test(await p.textContent('#overlay')));
    await p.click('#go');
    await p.waitForFunction(() => window.__fake, null, { timeout: 120000 });
    const again = await p.evaluate(() => window.__fake);
    const second = (await p.textContent('#log')).match(/page: \d+ files in .*/)?.[0] ?? '';
    check('the second visit gives the engine every byte', again.bytes === total + 4 * 2, second);
    check('the second visit asks the server for no game file', gameRequests.length === 0, `${gameRequests.length} requests`);
    check('no page errors', errors.length === 0, errors.join('; '));
    check('a computer with a mouse gets no Keyboard button', (await p.locator('#kbd-open:not([hidden])').count()) === 0);
    const sdlBefore = await p.evaluate(() => window.__sdlKeys);
    await p.click('#canvas');
    await p.keyboard.press('KeyA');
    check('keys typed on the game still reach SDL', (await p.evaluate(() => window.__sdlKeys)) > sdlBefore);

    // A touch device (an iPad): the Keyboard button opens the hidden field, and what is typed goes to the runtime.
    const tc = await b.newContext({ viewport: { width: 1024, height: 768 }, hasTouch: true });
    const t = await tc.newPage();
    const terrors = [];
    t.on('pageerror', (e) => terrors.push(e.message));
    await t.goto(`http://127.0.0.1:${PORT}/`);
    await t.waitForSelector('#go');
    await t.click('#go');
    await t.waitForFunction(() => window.__fake, null, { timeout: 120000 });
    check('a touch device gets the Keyboard button', (await t.locator('#kbd-open:not([hidden])').count()) === 1);
    await t.tap('#kbd-open');
    check('the button focuses the hidden field', (await t.evaluate(() => document.activeElement?.id)) === 'kbd');
    await t.keyboard.type('Ab');
    await t.keyboard.press('Backspace');
    await t.keyboard.press('Enter');
    await t.keyboard.insertText('Smith');
    const typed = await t.evaluate(() => window.__typed);
    check('letters, Backspace, Enter and a whole word go to the runtime',
      JSON.stringify(typed) === JSON.stringify([['type', 'A'], ['type', 'b'], ['key', 8], ['key', 13], ['type', 'Smith']]), JSON.stringify(typed));
    check('keys of the field never reach SDL (no double typing)', (await t.evaluate(() => window.__sdlKeys)) === 0);
    await t.tap('#kbd-open');
    check('the button again closes the keyboard', (await t.evaluate(() => document.activeElement?.id)) !== 'kbd'
      && (await t.textContent('#kbd-open')) === 'Keyboard');
    check('no page errors on the touch device', terrors.length === 0, terrors.join('; '));
    await tc.close();
    if (process.argv[2]) await p.screenshot({ path: process.argv[2] });
  } finally {
    await b.close();
    server.kill();
    fs.rmSync(tmp, { recursive: true, force: true });
  }
  process.exit(ok ? 0 : 1);
})().catch((e) => { console.error(e); process.exit(1); });
