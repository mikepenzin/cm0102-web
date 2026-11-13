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
  const game = path.join(tmp, 'game'), build = path.join(tmp, 'build-web'), asyncDir = path.join(tmp, 'build-web-asyncify');
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
    env: { ...process.env, CM_GAME_DIR: game, CM_WEB_BUILD: build, CM_WEB_BUILD_ASYNCIFY: asyncDir, PORT: String(PORT), CM_WEB_SKIP_VERIFY: '1', CM_ADMIN_TOKEN: 'page-check-admin-token' },
    stdio: ['ignore', 'pipe', 'inherit'],
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
    const gameAsked = g.asked.filter((u) => u.startsWith('/game/')).length;
    check('an engine the browser cannot compile stops the download of the game files',
      /cannot compile the engine/.test(g.box) && /download of the game files was stopped/.test(g.box) && gameAsked < rows.length / 2,
      `${gameAsked} of ${rows.length} game files asked for`);
    g = await gate('touch only', { ctx: { hasTouch: true, isMobile: true } });
    check('a touch device is told the gestures and may start', /tap to click, long press for the right button, two fingers to scroll/.test(g.box) && g.start === 1, g.box.slice(0, 200));

    // The game stops (an exit code, as Restart Game gave before WinExec was replaced): the box names it and offers
    // Start again, which loads the page again.
    {
      const c = await b.newContext({ viewport: { width: 1100, height: 760 } });
      const pg = await c.newPage();
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      await pg.click('#consent-no').catch(() => {});
      await pg.click('#go');
      await pg.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      // An error with a stack (as a trap of the engine): its stack lines go to the log, so an error report names where.
      await pg.evaluate(() => { function f_0068dbc0() { return new Error('RuntimeError: test trap'); }
        window.dispatchEvent(new ErrorEvent('error', { message: 'RuntimeError: test trap', error: f_0068dbc0() })); });
      check('an error\'s stack goes to the log: the report says where it happened',
        /page: at f_0068dbc0/.test(await pg.textContent('#log')), (await pg.textContent('#log')).split('\n').filter((l) => /page: at/.test(l)).slice(0, 2).join(' | '));
      await pg.evaluate(() => window.Module.onExit(70));
      const box = (await pg.textContent('#overlay')).replace(/\s+/g, ' ');
      // Send to the developer, from the box: the log, a note and a save of the player's choice. This server keeps no
      // files (no bucket), so the log goes and the box says the save did not.
      const hasReport = await pg.locator('#fail-report').count() === 1;
      await pg.click('#fail-report');
      const dlg = await pg.evaluate(() => ({ open: document.getElementById('report').open, error: document.getElementById('report-error').textContent,
        options: [...document.querySelectorAll('#report-save option')].map((o) => `${o.selected ? '*' : ''}${o.textContent}`) }));
      await pg.fill('#report-note', 'I pressed Exit and it stopped.');
      await pg.click('#report-send');
      await pg.waitForFunction(() => /was sent|Sent/.test(document.getElementById('report-state').textContent), null, { timeout: 15000 }).catch(() => {});
      const said = await pg.textContent('#report-state');
      const adm = await (await fetch(`http://127.0.0.1:${PORT}/api/admin/stats`, { headers: { authorization: 'Bearer page-check-admin-token' } })).json();
      const listed = adm.reports?.[0] ?? {};
      check('the error box: Send to the developer opens with the error and the saves, the newest chosen',
        hasReport && dlg.open && /The game ended with code 70/.test(dlg.error) && dlg.options[0] === 'No save' && /^\*Harbour City 2001-07-14\.sav \(262 KB\)/.test(dlg.options[1] ?? ''),
        JSON.stringify(dlg));
      check('Send: the report reaches the server with the note; a server without a bucket says the save did not go',
        listed.sent === true && listed.note === 'I pressed Exit and it stopped.' && /The game ended with code 70/.test(listed.message)
          && /The log was sent\. Harbour City 2001-07-14\.sav was not: this server takes no saves/.test(said), `${said} | ${JSON.stringify(listed)}`);
      await pg.click('#report-close');
      const again = pg.waitForNavigation({ timeout: 15000 }).then(() => true, () => false);
      await pg.click('#fail-again');
      check('a stop of the game: the box says so and Start again loads the page again',
        /The game ended with code 70/.test(box) && /Start again/.test(box) && await again, box.slice(0, 120));
      await c.close();
    }

    // A JSPI engine never returns from main: between frames its stack is parked in emscripten_sleep, and the browser
    // kept a parked stack, with the engine and the game files, alive after the page was gone (about 0.5 GB more at
    // each load of the page). The page ends the sleep when it goes. The stand-in's wasm here is a loop of
    // emscripten_sleep(5): (import "env" "emscripten_sleep" (func (param i32))) (export "main").
    {
      const PARKED = Buffer.from([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0,
        1, 8, 2, 0x60, 1, 0x7f, 0, 0x60, 0, 0,
        2, 0x18, 1, 3, 0x65, 0x6e, 0x76, 0x10, ...Buffer.from('emscripten_sleep'), 0, 0,
        3, 2, 1, 1,
        7, 8, 1, 4, 0x6d, 0x61, 0x69, 0x6e, 0, 1,
        0x0a, 0x0d, 1, 0x0b, 0, 3, 0x40, 0x41, 5, 0x10, 0, 0x0c, 0, 0x0b, 0x0b]);
      const good = fs.readFileSync(path.join(build, 'cm0102_recomp.wasm'));
      fs.writeFileSync(path.join(build, 'cm0102_recomp.wasm'), PARKED);
      fs.writeFileSync(path.join(build, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=jspi\n');
      const c = await b.newContext({ viewport: { width: 1100, height: 760 } });
      const pg = await c.newPage();
      const errs = [];
      pg.on('pageerror', (e) => errs.push(e.message));
      pg.on('dialog', (d) => d.accept().catch(() => {}));
      const start = async () => {          // the first visit asks; a later one starts by itself
        await pg.waitForFunction(() => window.__parked || document.getElementById('go')?.offsetParent, null, { timeout: 120000 });
        if (!(await pg.evaluate(() => Boolean(window.__parked)))) {
          await pg.click('#consent-no').catch(() => {});
          await pg.click('#go');
        }
        await pg.waitForFunction(() => window.__parked && window.__sleeps > 3, null, { timeout: 120000 });
        // What main's promise ends with, kept over the reload.
        await pg.evaluate(() => { sessionStorage.removeItem('engine-ended'); window.__main.catch((e) => sessionStorage.setItem('engine-ended', e?.name ?? String(e))); });
      };
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      await start();
      check('a JSPI engine runs through the page\'s sleep (parked between frames)', await pg.evaluate(() => window.__sleeps > 3));
      await pg.reload();
      await pg.waitForFunction(() => document.readyState === 'complete');
      check('a reload of the page ends the parked engine before the page goes',
        (await pg.evaluate(() => sessionStorage.getItem('engine-ended'))) === 'EngineLetGo', String(await pg.evaluate(() => sessionStorage.getItem('engine-ended'))));
      await start();
      const reloaded = pg.waitForNavigation({ timeout: 30000 }).then(() => true, () => false);
      await pg.evaluate(() => { window.Module.rtRestart(); });
      check('Restart Game ends the engine, then the page loads again', await reloaded
        && (await pg.evaluate(() => sessionStorage.getItem('engine-ended'))) === 'EngineLetGo');
      await pg.waitForFunction(() => window.__parked || document.getElementById('go')?.offsetParent, null, { timeout: 120000 });
      check('the page\'s own stop of the engine shows no error box', !/The game stopped/.test(await pg.textContent('body')) && errs.length === 0, errs.join('; '));
      await c.close();
      fs.writeFileSync(path.join(build, 'cm0102_recomp.wasm'), good);
      fs.writeFileSync(path.join(build, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=asyncify\n');
    }

    // Exit Game (code 0) right after a save: the box waits for the browser's storage, and leaving meanwhile asks first.
    {
      const c = await b.newContext({ viewport: { width: 1100, height: 760 } });
      const pg = await c.newPage();
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      await pg.click('#consent-no').catch(() => {});
      await pg.click('#go');
      await pg.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      await pg.evaluate(() => {
        const FS = window.Module.FS, syncfs = FS.syncfs.bind(FS);
        window.__exitStored = false;
        FS.syncfs = (populate, cb) => syncfs(populate, (e) => setTimeout(() => { if (!populate) window.__exitStored = true; cb(e); }, populate ? 0 : 3000));
      });
      await pg.evaluate(() => window.Module.onExit(0));
      const during = (await pg.textContent('#overlay')).replace(/\s+/g, ' ');
      const asks = await pg.evaluate(() => { const e = new Event('beforeunload', { cancelable: true }); window.dispatchEvent(e); return e.defaultPrevented; });
      await pg.waitForFunction(() => /Reload the page to play again/.test(document.getElementById('overlay').textContent), null, { timeout: 10000 }).catch(() => {});
      const after = (await pg.textContent('#overlay')).replace(/\s+/g, ' ');
      const asksAfter = await pg.evaluate(() => { const e = new Event('beforeunload', { cancelable: true }); window.dispatchEvent(e); return e.defaultPrevented; });
      check('Exit Game: the saves are stored before the box says the game ended, and leaving meanwhile asks first',
        /go to this browser's storage/.test(during) && asks && /Reload the page to play again/.test(after)
          && await pg.evaluate(() => window.__exitStored) && !asksAfter, `${during.slice(0, 80)} | ${after.slice(0, 80)} | asks ${asks}/${asksAfter}`);
      await c.close();
    }

    // A server with both builds (CM_WEB_BUILD_ASYNCIFY) and a browser without JSPI (Safari, an iPad): the page runs the
    // Asyncify build, and asks for no file of the JSPI one.
    fs.mkdirSync(asyncDir);
    fs.copyFileSync(path.join(build, 'cm0102_recomp.js'), path.join(asyncDir, 'cm0102_recomp.js'));
    fs.copyFileSync(path.join(build, 'cm0102_recomp.wasm'), path.join(asyncDir, 'cm0102_recomp.wasm'));
    fs.writeFileSync(path.join(asyncDir, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=asyncify\n');
    fs.writeFileSync(path.join(build, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=jspi\n');
    for (const jspi of [false, true]) {
      const c = await b.newContext({ viewport: { width: 1100, height: 760 } });
      if (!jspi) await c.addInitScript(() => { delete WebAssembly.Suspending; });
      const pg = await c.newPage();
      const asked = [];
      pg.on('request', (r) => { const u = new URL(r.url()); if (u.pathname.startsWith('/engine/')) asked.push(u.pathname); });
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      await pg.click('#go');
      await pg.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      const logText = await pg.textContent('#log');
      await c.close();
      const want = jspi ? '/engine/' : '/engine/asyncify/';
      check(jspi ? 'a browser with JSPI runs the JSPI build, though the server has both'
        : 'a browser without JSPI runs the Asyncify build of a server with both',
        asked.length > 0 && asked.every((u) => u === `${want}cm0102_recomp.js` || u === `${want}cm0102_recomp.wasm`)
          && logText.includes(`page: engine ${want} (${jspi ? 'jspi' : 'asyncify'} build`),
        asked.join(', '));
    }
    fs.rmSync(asyncDir, { recursive: true });
    fs.writeFileSync(path.join(build, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=asyncify\n');

    const p = await b.newPage({ viewport: { width: 1100, height: 760 }, deviceScaleFactor: 2 });
    const errors = [];
    p.on('pageerror', (e) => errors.push(e.message));
    p.on('dialog', (d) => d.accept().catch(() => {}));         // the question before a page with a running game is left
    const statsCalls = [];                                     // the page's requests of the statistics, with their bodies
    p.on('request', (r) => { if (r.url().includes('/api/stats/') || r.url().includes('/api/report')) statsCalls.push(`${new URL(r.url()).pathname} ${r.postData() ?? ''}`); });
    await p.goto(`http://127.0.0.1:${PORT}/`);
    await p.waitForSelector('#go');
    // The statistics ask first (the server's opt-in mode, the default): the card, and nothing sent or stored before.
    check('the first visit asks about the statistics, with two answers that look the same',
      await p.isVisible('#consent') && (await p.getAttribute('#consent-yes', 'class')) === (await p.getAttribute('#consent-no', 'class')));
    await p.waitForTimeout(3500);                              // past the first heartbeat of a page that may send
    check('before an answer: no browser id stored, nothing sent', statsCalls.length === 0
      && (await p.evaluate(() => localStorage.getItem('cm0102-anon'))) === null, statsCalls.join(' | '));
    if (process.argv[2]) await p.screenshot({ path: process.argv[2].replace(/\.png$/, '-consent.png') });
    await p.click('#consent-no');
    await p.waitForTimeout(500);
    check('No thanks: the card goes, only the answer is sent (no id), nothing is stored but the answer',
      !(await p.isVisible('#consent')) && statsCalls.length === 1 && /^\/api\/stats\/choice \{"choice":"off"\}$/.test(statsCalls[0])
        && (await p.evaluate(() => [localStorage.getItem('cm0102-anon'), localStorage.getItem('cm0102-stats')])).join() === ',off', statsCalls.join(' | '));
    check('the start box names the size of the download', /461 MB of game files download into this browser once/.test(await p.textContent('#overlay')), (await p.textContent('#overlay p')).trim().slice(0, 90));
    if (process.argv[2]) await p.screenshot({ path: process.argv[2].replace(/\.png$/, '-start.png') });
    // The connection drops once during the first download: that file is asked for again, and the start goes on.
    let dropped = 0;
    await p.route('**/game/Data/staff.dat*', (route) => (dropped++ === 0 ? route.abort('connectionreset') : route.continue()));
    const t0 = Date.now();
    await p.click('#go');
    await p.waitForFunction(() => /MB of 461 MB/.test(document.getElementById('bytes')?.textContent ?? ''), null, { timeout: 30000 }).catch(() => {});
    if (process.argv[2]) await p.screenshot({ path: process.argv[2].replace(/\.png$/, '-loading.png') });
    await p.waitForFunction(() => window.__fake, null, { timeout: 120000 });
    const f = await p.evaluate(() => window.__fake);
    check('every file reached the engine', f.files === rows.length + 4, `${f.files} files`);
    check('every byte reached the engine', f.bytes === total + 4 * 2, `${(f.bytes / 1048576).toFixed(0)} MB in ${((Date.now() - t0) / 1000).toFixed(1)} s`);
    check('a file whose download was cut is asked for again, and the start goes on',
      dropped === 2 && /staff\.dat: .*asked for again in 2 s/.test(await p.textContent('#log')), `${dropped} requests of staff.dat`);
    await p.unroute('**/game/Data/staff.dat*');
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
    // An empty save (0 bytes), as an empty Download made it (two tabs, a player's report): its own message.
    await p.setInputFiles('#saves-file', [{ name: 'junk.sav', mimeType: 'application/octet-stream', buffer: Buffer.from('hello') },
      { name: 'St-Etienne 2002-02-13.sav', mimeType: 'application/octet-stream', buffer: Buffer.alloc(0) },
      { name: 'notes.txt', mimeType: 'text/plain', buffer: Buffer.from('x') }]);
    await p.waitForFunction(() => /notes\.txt/.test(document.getElementById('saves-note').textContent));
    const refusedNote = await p.textContent('#saves-note');
    check('a file that is not a save, an empty save and a file that is not .sav are refused, each with its reason',
      /junk\.sav: it is too short to be a saved game of CM 01\/02: 1 KB, where a saved game has at least 262 KB/.test(refusedNote)
        && /St-Etienne 2002-02-13\.sav: the file is empty \(0 bytes\)/.test(refusedNote) && /only \.sav files/.test(refusedNote)
        && !/junk|St-Etienne/.test(await p.textContent('#saves-list')), refusedNote);
    if (process.argv[2]) await p.screenshot({ path: process.argv[2].replace(/\.png$/, '-saves.png') });
    // A refused upload offers Send to the developer, with the refused files first: the file that did not load can go to
    // the developer as it is (this server keeps no files, so here the log goes and the dialog says the file did not).
    await p.click('#saves-report');
    const offered = await p.evaluate(() => ({ open: document.getElementById('report').open, savesOpen: document.getElementById('saves').open,
      options: [...document.querySelectorAll('#report-save option')].map((o) => `${o.selected ? '*' : ''}${o.textContent}`) }));
    await p.selectOption('#report-save', { label: 'St-Etienne 2002-02-13.sav (0 KB), the file you uploaded' });
    await p.click('#report-send');
    await p.waitForFunction(() => /was sent|Sent/.test(document.getElementById('report-state').textContent), null, { timeout: 15000 }).catch(() => {});
    const offeredSaid = await p.textContent('#report-state');
    const lastReport = (await (await fetch(`http://127.0.0.1:${PORT}/api/admin/stats`, { headers: { authorization: 'Bearer page-check-admin-token' } })).json()).reports?.[0] ?? {};
    check('a refused upload: Send to the developer offers the refused files first, and the report says why they were refused',
      offered.open && !offered.savesOpen && offered.options[1] === '*junk.sav (1 KB), the file you uploaded'
        && offered.options[2] === 'St-Etienne 2002-02-13.sav (0 KB), the file you uploaded'
        && /St-Etienne 2002-02-13\.sav: the file is empty/.test(lastReport.message) && lastReport.sent === true
        && /St-Etienne 2002-02-13\.sav was not: this server takes no saves\. Send the file another way/.test(offeredSaid),
      `${JSON.stringify(offered.options)} | ${offeredSaid}`);
    await p.click('#report-close');
    await p.click('#saves-open');
    // Tactics (tactics.js): a tactic saved by the game, a preset's .pct (its version plus 0x075BCD15, which the game's
    // Load refuses: the page stores the plain one), and two files that are not tactics.
    const tct = (version) => { const t = Buffer.alloc(1476); t.writeUInt32LE(version, 0); t.writeUInt32LE(0xf2fffff2, 1472); return t; };
    await p.setInputFiles('#tactics-file', [
      { name: 'My 4-4-2.tct', mimeType: 'application/octet-stream', buffer: tct(0x98ec5e) },
      { name: '442_diamond_default.pct', mimeType: 'application/octet-stream', buffer: tct(0x07f4b971) },
      { name: 'junk.tct', mimeType: 'application/octet-stream', buffer: Buffer.from('hello') },
      { name: 'notes.txt', mimeType: 'text/plain', buffer: Buffer.from('x') }]);
    await p.waitForFunction(() => /Uploaded/.test(document.getElementById('tactics-note').textContent));
    const tacticsNote = await p.textContent('#tactics-note');
    const stored = await p.evaluate(() => {
      const FS = window.Module.FS, v = (f) => new DataView(FS.readFile(f).buffer).getUint32(0, true);
      return [v('/user/tactics/My 4-4-2.tct'), v('/user/tactics/442_diamond_default.tct')];
    });
    check('tactics upload into the folder tactics; a preset is made loadable; other files are refused with a reason',
      stored[0] === 0x98ec5e && stored[1] === 0x98ec5c && /Uploaded My 4-4-2, 442_diamond_default/.test(tacticsNote)
        && /junk\.tct: it is too short/.test(tacticsNote) && /notes\.txt: only \.tct files/.test(tacticsNote)
        && /My 4-4-2/.test(await p.textContent('#tactics-list')), `${stored.map((x) => x.toString(16))} | ${tacticsNote}`);
    const [tdl] = await Promise.all([p.waitForEvent('download'), p.click('#tactics-list button[data-tactic="1"]')]);
    const tdlPath = await tdl.path();
    check('a tactic downloads as its .tct file', tdl.suggestedFilename() === 'My 4-4-2.tct' && fs.readFileSync(tdlPath).equals(tct(0x98ec5e)), tdl.suggestedFilename());
    await p.click('#tactics-list button[data-tactic-delete="0"]');
    check('a tactic is deleted', !(await p.evaluate(() => window.Module.FS.readdir('/user/tactics').includes('442_diamond_default.tct')))
      && !/442_diamond_default/.test(await p.textContent('#tactics-list')));
    await p.click('#saves-close');
    const storesNow = await p.evaluate(() => window.__syncs.store);
    await p.evaluate(() => window.Module.FS.writeFile('/user/Auto 2001-07-20.sav', new Uint8Array(0x416ec + 8)));
    await p.waitForFunction((n) => window.__syncs.store > n, storesNow, { timeout: 8000 }).catch(() => {});
    check('a new save from the game goes to IndexedDB within seconds', (await p.evaluate(() => window.__syncs.store)) > storesNow);
    // The browser closes the user folder's IndexedDB connection (Android's Chrome does): the page opens it again.
    const openedBefore = await p.evaluate(() => { window.Module.IDBFS.dbs['/user'].closed = true; return window.__syncs.opened; });
    await p.evaluate(() => window.Module.FS.writeFile('/user/Auto 2001-07-27.sav', new Uint8Array(0x416ec + 8)));
    await p.waitForFunction((n) => window.__syncs.opened > n, openedBefore, { timeout: 8000 }).catch(() => {});
    const reopenLog = await p.textContent('#log');
    check('a connection the browser closed is opened again and the save is written',
      (await p.evaluate(() => window.__syncs.opened)) > openedBefore && /opens it again/.test(reopenLog) && !/user folder was not saved/.test(reopenLog),
      reopenLog.split('\n').filter((l) => /storage|not saved/.test(l)).slice(-3).join(' | '));

    const first = (await p.textContent('#log')).match(/page: \d+ files in .*/)?.[0] ?? '';
    check('the first visit downloads every file', /: 0\.0 MB from this browser/.test(first), first);
    // A second visit: every file from the browser's cache storage, none from the server.
    const gameRequests = [];
    p.on('request', (r) => { if (r.url().includes('/game/')) gameRequests.push(r.url()); });
    await p.reload();
    await p.waitForFunction(() => window.__fake, null, { timeout: 120000 });
    check('the second visit starts by itself: no Start box', /from this browser, 0\.0 MB downloaded/.test(await p.textContent('#log')));
    check('and does not ask again: the answer stays', !(await p.isVisible('#consent')) && statsCalls.filter((c) => c.startsWith('/api/stats/beat')).length === 0);
    // Turned on in Help: the browser's id is made then, and the visit counts at once.
    await p.evaluate(() => document.getElementById('help-stats-on').click());
    await p.waitForFunction(() => localStorage.getItem('cm0102-anon') !== null, null, { timeout: 5000 }).catch(() => {});
    await p.waitForTimeout(800);
    check('Help, Statistics on: the id is made, the answer and a heartbeat go',
      (await p.evaluate(() => localStorage.getItem('cm0102-stats'))) === 'on' && /^[0-9a-f-]{36}$/.test(await p.evaluate(() => localStorage.getItem('cm0102-anon')) ?? '')
        && statsCalls.some((c) => c === '/api/stats/choice {"choice":"on"}') && statsCalls.some((c) => c.startsWith('/api/stats/beat')), statsCalls.join(' | ').slice(0, 300));
    await p.evaluate(() => document.getElementById('help-stats-on').click());
    check('and off again: the id is deleted', (await p.evaluate(() => [localStorage.getItem('cm0102-anon'), localStorage.getItem('cm0102-stats')])).join() === ',off');
    // Restart Game (the game's restart_game 0x68dbc0 through WinExec, host_restart in host/web.c): the page saves the
    // user folder to the browser's storage, then loads again. The stand-in's storage keeps nothing, so the save before
    // the reload is marked in sessionStorage, which a reload keeps.
    await p.evaluate(() => {
      sessionStorage.removeItem('saved-before-restart');
      const FS = window.Module.FS, syncfs = FS.syncfs.bind(FS);
      // A slow storage (a large save on a phone): 9 s, past the 8 s after which the page once loaded again anyway.
      FS.syncfs = (populate, cb) => syncfs(populate, (e) => setTimeout(() => {
        if (!populate) sessionStorage.setItem('saved-before-restart', '1');
        cb(e);
      }, populate ? 0 : 9000));
    });
    const reloaded = p.waitForNavigation({ timeout: 30000 }).then(() => true, () => false);
    await p.evaluate(() => window.Module.rtRestart());
    check('Restart Game: the page shows it is storing the saves', /Restarting/.test(await p.textContent('body')));
    check('Restart Game: the page loads again', await reloaded);
    await p.waitForFunction(() => window.__fake, null, { timeout: 120000 });
    check('the user folder went to the browser\'s storage before the reload, and the game starts again by itself',
      (await p.evaluate(() => sessionStorage.getItem('saved-before-restart'))) === '1');
    const again = await p.evaluate(() => window.__fake);
    const second = (await p.textContent('#log')).match(/page: \d+ files in .*/)?.[0] ?? '';
    check('the second visit gives the engine every byte', again.bytes === total + 4 * 2, second);
    check('the second visit asks the server for no game file', gameRequests.length === 0, `${gameRequests.length} requests`);
    await p.click('#help-open');
    check('Help lists the gestures and holds the engine log', /Long press \(a ring fills\), or a tap with two fingers/.test(await p.textContent('#help')) && /Right click, then a tap/.test(await p.textContent('#help')) && /page: main\(--game \/game\)/.test(await p.textContent('#log')));
    if (process.argv[2]) await p.screenshot({ path: process.argv[2].replace(/\.png$/, '-help.png') });
    await p.click('#help-close');
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
      && (await t.textContent('#kbd-open span')) === 'Keyboard' && (await t.locator('#kbd-open svg').count()) === 1);
    check('a touch device\'s file pickers show every file (the page checks them itself)',
      (await t.getAttribute('#saves-file', 'accept')) === null && (await t.getAttribute('#sets-file', 'accept')) === null);
    await t.setInputFiles('#sets-file', { name: 'Update 2026.rar', mimeType: 'application/vnd.rar', buffer: Buffer.from('Rar!\x1a\x07\x00') });
    await t.waitForFunction(() => /rar/.test(document.getElementById('sets-note').textContent), null, { timeout: 5000 }).catch(() => {});
    check('a database as a .rar gets a message, not silence', /Update 2026\.rar: a \.rar or \.7z cannot be read here/.test(await t.textContent('#sets-note')),
      await t.textContent('#sets-note'));
    // The Right click button: the next tap on the game is a click of the right button, then it is off again; a long
    // press (a ring fills under the finger) is one too.
    await t.evaluate(() => { window.__downs = []; document.getElementById('canvas').addEventListener('mousedown', (e) => window.__downs.push(e.button), true); });
    check('a touch device gets the Right click button', await t.isVisible('#right-click'));
    await t.tap('#right-click');
    const armed = await t.getAttribute('#right-click', 'aria-pressed');
    const cb = await t.locator('#canvas').boundingBox();
    await t.touchscreen.tap(cb.x + cb.width / 2, cb.y + cb.height / 2);
    await t.waitForTimeout(200);
    const afterArmed = await t.evaluate(() => [...window.__downs]);
    await t.touchscreen.tap(cb.x + cb.width / 3, cb.y + cb.height / 3);
    await t.waitForTimeout(200);
    check('Right click, then a tap: a right click there, and the button is off again; the next tap is a left click',
      armed === 'true' && afterArmed.at(-1) === 2 && (await t.getAttribute('#right-click', 'aria-pressed')) === 'false'
        && (await t.evaluate(() => window.__downs.at(-1))) === 0, `${armed} ${JSON.stringify(await t.evaluate(() => window.__downs))}`);
    const cdp = await tc.newCDPSession(t);
    const at = { x: Math.round(cb.x + cb.width / 2), y: Math.round(cb.y + cb.height / 2), id: 1 };
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [at] });
    await t.waitForTimeout(200);
    const ring = await t.locator('#fx .ping.hold').count();
    await t.waitForTimeout(400);
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
    await t.waitForTimeout(150);
    check('a long press: a ring fills under the finger, then a right click', ring === 1 && (await t.evaluate(() => window.__downs.at(-1))) === 2
      && (await t.locator('#fx .ping.hold').count()) === 0, `ring ${ring}, ${JSON.stringify(await t.evaluate(() => window.__downs))}`);
    check('no page errors on the touch device', terrors.length === 0, terrors.join('; '));
    await tc.close();
    // An engine built before the keyboard calls: no button, and the log says to build it again.
    const oc = await b.newContext({ viewport: { width: 1024, height: 768 }, hasTouch: true });
    await oc.addInitScript(() => { window.__oldEngine = true; });
    const o = await oc.newPage();
    await o.goto(`http://127.0.0.1:${PORT}/`);
    await o.waitForSelector('#go');
    await o.click('#go');
    await o.waitForFunction(() => window.__fake, null, { timeout: 120000 });
    check('an older engine on a touch device: no Keyboard button, a line in the log',
      (await o.locator('#kbd-open:not([hidden])').count()) === 0 && /build it again for the Keyboard button/.test(await o.textContent('#log')));
    await oc.close();
    // Full screen on a touch device. A phone held sideways has room beside the picture: the page's buttons go there as
    // a column and cover none of the game. An iPad has none: they sit over the corner, and the fold button folds them
    // into a small tab on the right edge, which stays folded at the next visit (a player could not reach a player's
    // Transfer tab under them).
    const overlap = (a, c) => a.x < c.x + c.width && c.x < a.x + a.width && a.y < c.y + c.height && c.y < a.y + a.height;
    async function fullScreenOn(viewport) {
      const ctx = await b.newContext({ viewport, hasTouch: true, isMobile: true });
      const pg = await ctx.newPage();
      const errs = [];
      pg.on('pageerror', (e) => errs.push(e.message));
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      await pg.waitForSelector('#go');
      await pg.click('#consent-no').catch(() => {});
      await pg.click('#go');
      await pg.waitForFunction(() => window.__fake && !document.getElementById('fullscreen').hidden, null, { timeout: 120000 });
      await pg.tap('#fullscreen');
      await pg.waitForFunction(() => document.getElementById('app').classList.contains('fs'), null, { timeout: 5000 }).catch(() => {});
      await pg.waitForTimeout(300);
      return { ctx, pg, errs };
    }
    const phone = await fullScreenOn({ width: 844, height: 390 });
    const phoneBar = await phone.pg.locator('#bar').boundingBox(), phoneCanvas = await phone.pg.locator('#canvas').boundingBox();
    check('full screen on a phone held sideways: the buttons are a column beside the game, over none of it',
      (await phone.pg.evaluate(() => document.getElementById('app').dataset.bar)) === 'side' && !overlap(phoneBar, phoneCanvas)
        && phoneBar.height > phoneBar.width && (await phone.pg.locator('#bar-fold').isVisible()) === false,
      `${await phone.pg.evaluate(() => document.getElementById('app').dataset.bar)} bar ${JSON.stringify(phoneBar)} canvas ${JSON.stringify(phoneCanvas)}`);
    if (process.argv[2]) await phone.pg.screenshot({ path: process.argv[2].replace(/\.png$/, '-fs-phone.png') });
    check('no page errors in full screen on the phone', phone.errs.length === 0, phone.errs.join('; '));
    await phone.ctx.close();
    const pad = await fullScreenOn({ width: 1180, height: 820 });
    const padOpen = await pad.pg.locator('#bar').boundingBox();
    check('full screen on an iPad: the buttons over the corner, with the fold button',
      (await pad.pg.evaluate(() => document.getElementById('app').dataset.bar)) === 'over' && (await pad.pg.locator('#bar-fold').isVisible()),
      await pad.pg.evaluate(() => document.getElementById('app').dataset.bar));
    await pad.pg.tap('#bar-fold');
    const padFolded = await pad.pg.locator('#bar').boundingBox();
    const shown = await pad.pg.evaluate(() => [...document.querySelectorAll('#bar > *')].filter((e) => e.offsetParent !== null).map((e) => e.id));
    check('the fold button folds them into a small tab on the right edge',
      JSON.stringify(shown) === '["bar-fold"]' && padFolded.width < 40 && padFolded.width * padFolded.height < padOpen.width * padOpen.height / 4
        && Math.abs(padFolded.x + padFolded.width - 1180) < 2 && (await pad.pg.getAttribute('#bar-fold', 'aria-expanded')) === 'false',
      `${JSON.stringify(shown)} open ${JSON.stringify(padOpen)} folded ${JSON.stringify(padFolded)}`);
    if (process.argv[2]) await pad.pg.screenshot({ path: process.argv[2].replace(/\.png$/, '-fs-folded.png') });
    await pad.pg.reload();
    await pad.pg.waitForFunction(() => document.getElementById('app').classList.contains('bar-folded'), null, { timeout: 15000 }).catch(() => {});
    check('the bar stays folded at the next visit', await pad.pg.evaluate(() => document.getElementById('app').classList.contains('bar-folded')));
    await pad.pg.evaluate(() => document.getElementById('bar-fold').click());
    check('and the tab opens it again', !(await pad.pg.evaluate(() => document.getElementById('app').classList.contains('bar-folded'))));
    check('no page errors in full screen on the iPad', pad.errs.length === 0, pad.errs.join('; '));
    await pad.ctx.close();
    // The vault after the browser closed its IndexedDB connection (an Android report): its next request opens it
    // again, where it once failed with "The database connection is closing" until the page was loaded again.
    const vc = await b.newContext();
    const v = await vc.newPage();
    await v.goto(`http://127.0.0.1:${PORT}/privacy`);
    const reopen = await v.evaluate(async () => {
      const { createVault } = await import('/vault.js');
      let last = null;
      const idb = { open: (n, ver) => { const r = indexedDB.open(n, ver); r.addEventListener('success', () => { last = r.result; }); return r; } };
      const vault = createVault({ FS: {}, idb });
      await vault.open();
      const first = last;
      first.close();
      try { await vault.clear(); return { ok: last !== first }; } catch (e) { return { ok: false, error: `${e.name}: ${e.message}` }; }
    });
    check('the vault opens its storage again after the browser closed it', reopen.ok, reopen.error ?? '');
    // Two tabs' vaults on one storage, each over its own file system (a small one like MEMFS): tab A's round once
    // removed the save tab B had just stored, and B's Download of it was then an empty file, which an upload refuses
    // as "too short". Now a round removes only the records its own page knew, and a placeholder without one fails to read.
    const twoVaults = await v.evaluate(async () => {
      const { createVault } = await import('/vault.js');
      const makeFS = () => {
        const nodes = new Map(), dirs = new Set(['/', '/user']);
        const enoent = (p) => Object.assign(new Error(`ENOENT ${p}`), { errno: 44 });
        return {
          streams: [], cwd: () => '/',
          mkdirTree: (p) => { let cur = ''; for (const part of p.split('/').filter(Boolean)) { cur += `/${part}`; dirs.add(cur); } },
          modeStringToFlags: (m) => (m.includes('w') ? 577 : 0),
          lookupPath(p) { if (nodes.has(p)) return { node: nodes.get(p) }; if (dirs.has(p)) return { node: { path: p } }; throw enoent(p); },
          getPath: (n) => n.path,
          writeFile(p, data) { const n = nodes.get(p) ?? { path: p }; nodes.set(p, n); n.contents = data; n.usedBytes = data.length; n.mtime = Date.now(); },
          readFile(p) { const n = nodes.get(p); if (!n) throw enoent(p); return n.contents.slice(0, n.usedBytes); },
          unlink(p) { if (!nodes.delete(p)) throw enoent(p); },
          readdir(d) {
            if (!dirs.has(d)) throw enoent(d);
            const names = new Set(['.', '..']);
            for (const p of [...nodes.keys(), ...dirs]) if (p.startsWith(`${d}/`)) names.add(p.slice(d.length + 1).split('/')[0]);
            return [...names];
          },
          stat(p) {
            if (nodes.has(p)) return { mode: 0o100644, size: nodes.get(p).usedBytes, mtime: new Date(nodes.get(p).mtime) };
            if (dirs.has(p)) return { mode: 0o040755, size: 0, mtime: new Date(0) };
            throw enoent(p);
          },
          isFile: (m) => (m & 0o170000) === 0o100000, isDir: (m) => (m & 0o170000) === 0o040000,
        };
      };
      indexedDB.deleteDatabase('cm0102-vault');
      const A = makeFS(), B = makeFS();
      const va = createVault({ FS: A, stable: 0 }), vb = createVault({ FS: B, stable: 0 });
      for (const x of [va, vb]) { await x.open(); x.active = true; await x.mount(); }
      A.writeFile('/user/Old.sav', new Uint8Array(1000).fill(1));
      await va.persist();
      const name = '/user/St-Etienne 2002-02-13.sav';
      B.writeFile(name, new Uint8Array(300000).fill(7));
      await vb.persist();
      const roundA = await va.persist();
      const kept = (await vb.blob(name)).size;
      A.unlink('/user/Old.sav');
      const deleted = await va.persist();                      // a save the page itself deleted still goes
      // A placeholder whose record went some other way (the browser's storage, an older page): its read fails.
      const C = makeFS();
      const vc2 = createVault({ FS: C, stable: 0 });
      await vc2.open(); vc2.active = true; await vc2.mount();
      await vb.persist();
      await new Promise((ok) => { const r = indexedDB.open('cm0102-vault'); r.onsuccess = () => { const t = r.result.transaction(['files'], 'readwrite'); t.objectStore('files').delete(name); t.oncomplete = () => { r.result.close(); ok(); }; }; });
      let lost;
      try { lost = `read ${(await vc2.blob(name)).size} bytes`; } catch (e) { lost = e.message; }
      return { removedByA: roundA.removed, kept, deleted: deleted.removed, lost, listed: C.stat(name).size };
    });
    check('two tabs: one tab\'s round leaves the save the other just stored', twoVaults.removedByA === 0 && twoVaults.kept === 300000
      && twoVaults.deleted === 1, JSON.stringify(twoVaults));
    check('a save whose stored copy is gone fails to read, not an empty file',
      /the browser's storage no longer has St-Etienne 2002-02-13\.sav/.test(twoVaults.lost) && twoVaults.listed === 300000, twoVaults.lost);
    // A write whose commit fails after its request succeeded (a full disk or quota: QuotaExceededError at the commit):
    // the vault once took the request's success for stored and dropped the save from memory, so the save was lost.
    // Now the save stays in memory, reads whole, and the next round stores it.
    const commitFails = await v.evaluate(async () => {
      const { createVault } = await import('/vault.js');
      const nodes = new Map();
      const FS = {
        streams: [], cwd: () => '/', mkdirTree() {}, modeStringToFlags: () => 0,
        lookupPath(p) { if (nodes.has(p)) return { node: nodes.get(p) }; if (p === '/user') return { node: {} }; throw new Error(`ENOENT ${p}`); },
        getPath: (n) => n.path,
        writeFile(p, data) { const n = nodes.get(p) ?? { path: p }; nodes.set(p, n); n.contents = data; n.usedBytes = data.length; n.mtime = Date.now(); },
        readFile(p) { const n = nodes.get(p); return n.contents.slice(0, n.usedBytes); },
        readdir: (d) => (d === '/user' ? ['.', '..', ...[...nodes.keys()].map((k) => k.slice(6))] : (() => { throw new Error('ENOENT'); })()),
        stat(p) { if (nodes.has(p)) return { mode: 0o100644, size: nodes.get(p).usedBytes, mtime: new Date() }; if (p === '/user') return { mode: 0o040755 }; throw new Error('ENOENT'); },
        isFile: (m) => (m & 0o170000) === 0o100000, isDir: (m) => (m & 0o170000) === 0o040000,
      };
      indexedDB.deleteDatabase('cm0102-vault');
      const vault = createVault({ FS, stable: 0 });
      await vault.open(); vault.active = true; await vault.mount();
      const realPut = IDBObjectStore.prototype.put;
      IDBObjectStore.prototype.put = function (...args) {
        const r = realPut.apply(this, args);
        r.addEventListener('success', () => { try { this.transaction.abort(); } catch { /* done */ } });
        return r;
      };
      FS.writeFile('/user/Saint-Etienne 2001-11-02.sav', new Uint8Array(400000).fill(3));
      let failed = '';
      try { await vault.persist(); } catch (e) { failed = e.name || e.message; }
      IDBObjectStore.prototype.put = realPut;
      const inMemory = (await vault.blob('/user/Saint-Etienne 2001-11-02.sav')).size;
      const again = await vault.persist();
      const stored = (await vault.records()).map((r) => `${r.key}:${r.size}`);
      return { failed, inMemory, again: again.stored, stored };
    });
    check('a write whose commit fails keeps the save in memory, and the next round stores it',
      commitFails.failed !== '' && commitFails.inMemory === 400000 && commitFails.again === 1
        && commitFails.stored.join() === '/user/Saint-Etienne 2001-11-02.sav:400000', JSON.stringify(commitFails));
    await vc.close();

    // One tab at a time: a second tab of the game asks first; Play here stops the first one, which stores its saves,
    // then writes nothing more, and the game starts in the second.
    {
      const ctx = await b.newContext({ viewport: { width: 1000, height: 700 } });
      const one = await ctx.newPage(), two = await ctx.newPage();
      const errs = [];
      for (const pg of [one, two]) { pg.on('pageerror', (e) => errs.push(e.message)); pg.on('dialog', (d) => d.accept().catch(() => {})); }
      await one.goto(`http://127.0.0.1:${PORT}/`);
      await one.waitForSelector('#go');
      await one.click('#go');
      await one.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      await two.goto(`http://127.0.0.1:${PORT}/`);
      await two.waitForSelector('#turn-take', { timeout: 30000 }).catch(() => {});
      const asked = (await two.textContent('#overlay')).replace(/\s+/g, ' ').trim();
      check('a second tab does not start: it says the game is open in another tab, and offers Play here',
        /The game is open in another tab/.test(asked) && /two would delete each other's saves/.test(asked)
          && await two.locator('#turn-take').count() === 1 && !(await two.evaluate(() => Boolean(window.__fake))), asked.slice(0, 160));
      const storesBefore = await one.evaluate(() => window.__syncs.store);
      await two.click('#turn-take');
      await two.waitForFunction(() => window.__fake, null, { timeout: 60000 }).catch(() => {});
      await one.waitForSelector('#turn-back', { timeout: 10000 }).catch(() => {});
      const moved = (await one.textContent('#overlay')).replace(/\s+/g, ' ').trim();
      const storesAfter = await one.evaluate(() => window.__syncs.store);
      check('Play here: the first tab stores its saves, says the game moved, and the game starts in the second',
        /The game moved to another tab/.test(moved) && storesAfter > storesBefore && await two.evaluate(() => Boolean(window.__fake)),
        `${moved.slice(0, 80)}; stores ${storesBefore} -> ${storesAfter}`);
      await one.evaluate(() => window.Module.FS.writeFile('/user/Late 2001-08-01.sav', new Uint8Array(0x416ec + 8)));
      await one.waitForTimeout(3000);
      check('the tab that handed over writes nothing more to the browser\'s storage', (await one.evaluate(() => window.__syncs.store)) === storesAfter);
      check('no page errors with two tabs', errs.length === 0, errs.join('; '));
      await ctx.close();
    }

    // A page from before one tab at a time (it takes no turn) in another tab. The saves' storage is now version 2: a
    // page of the last code closes its version 1 connection when asked and cannot open it again, so it writes nothing
    // more there; an older one that keeps its connection holds the new page up, which says so and goes on once it closes.
    {
      const ctx = await b.newContext({ viewport: { width: 1000, height: 700 } });
      const old = await ctx.newPage(), fresh = await ctx.newPage();
      await old.goto(`http://127.0.0.1:${PORT}/privacy`);
      await old.evaluate(() => new Promise((ok) => {
        const r = indexedDB.open('cm0102-vault', 1);
        r.onupgradeneeded = () => r.result.createObjectStore('files');
        r.onsuccess = () => { window.__old = r.result; ok(); };      // no onversionchange: it keeps the connection
      }));
      await fresh.goto(`http://127.0.0.1:${PORT}/`);
      await fresh.waitForSelector('#go');
      await fresh.click('#go');
      await fresh.waitForFunction(() => /An older page of the game/.test(document.getElementById('overlay').textContent), null, { timeout: 60000 }).catch(() => {});
      const held = (await fresh.textContent('#overlay')).replace(/\s+/g, ' ').trim();
      const startedWhileHeld = await fresh.evaluate(() => Boolean(window.__fake));
      await old.evaluate(() => window.__old.close());
      await fresh.waitForFunction(() => window.__fake, null, { timeout: 60000 }).catch(() => {});
      check('an older page that keeps the saves\' storage open: the new page waits and says so, then starts once it is closed',
        /The game is open in another tab/.test(held) && /An older page of the game runs in another tab/.test(held) && !startedWhileHeld
          && await fresh.evaluate(() => Boolean(window.__fake)), held.slice(0, 160));
      const shutOut = await old.evaluate(() => new Promise((ok) => {
        const r = indexedDB.open('cm0102-vault', 1);
        r.onsuccess = () => { r.result.close(); ok('opened'); };
        r.onerror = () => ok(r.error.name);
      }));
      check('and a page of the earlier code cannot open the saves\' storage again (version 1), so it removes nothing', shutOut === 'VersionError', shutOut);
      await ctx.close();
    }
    if (process.argv[2]) await p.screenshot({ path: process.argv[2] });
  } finally {
    await b.close();
    server.kill();
    fs.rmSync(tmp, { recursive: true, force: true });
  }
  process.exit(ok ? 0 : 1);
})().catch((e) => { console.error(e); process.exit(1); });
