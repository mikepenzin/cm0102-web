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
    env: { ...process.env, CM_GAME_DIR: game, CM_WEB_BUILD: build, CM_WEB_BUILD_ASYNCIFY: asyncDir, PORT: String(PORT), CM_WEB_SKIP_VERIFY: '1', CM_ADMIN_TOKEN: 'page-check-admin-token', CM_COFFEE_URL: 'https://buymeacoffee.com/cm0102-test', CM_SELLER: 'A. Maker' },
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
    // An iPhone without JSPI (iOS 26 or before; jjcastle on Reddit, 14 November 2026): the box says to update iOS, as
    // every browser there is WebKit, Chrome too.
    g = await gate('iPhone without JSPI', { suspend: 'jspi', init: () => { delete WebAssembly.Suspending; },
      ctx: { userAgent: 'Mozilla/5.0 (iPhone; CPU iPhone OS 18_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) CriOS/141.0 Mobile/15E148 Safari/604.1', hasTouch: true, isMobile: true } });
    check('an iPhone without JSPI is told to update iOS to 27 (Settings, General, Software Update), before any download',
      /this iPhone does not have yet\. iOS 27 has it, in every browser \(Safari and Chrome alike\): update in Settings, General, Software Update/.test(g.box)
        && /It runs on an iPhone with iOS 27 or later/.test(g.box) && g.start === 0 && g.asked.length === 0, g.box.slice(0, 260));
    g = await gate('no memory', { init: () => { WebAssembly.Memory = function () { throw new RangeError('could not allocate memory'); }; } });
    check('a browser that refuses the memory is blocked before any download',
      /memory that the engine needs/.test(g.box) && g.start === 0 && g.asked.length === 0, g.box.slice(0, 120));
    g = await gate('bad engine', { wasm: Buffer.from('this is not WebAssembly'), click: true });
    const gameAsked = g.asked.filter((u) => u.startsWith('/game/')).length;
    check('an engine the browser cannot compile stops the download of the game files',
      /cannot compile the engine/.test(g.box) && /download of the game files was stopped/.test(g.box) && gameAsked < rows.length / 2,
      `${gameAsked} of ${rows.length} game files asked for`);
    g = await gate('touch only', { ctx: { hasTouch: true, isMobile: true } });
    check('the start box has no Made with line: it is the page\'s footer', !/Made with/.test(g.box), g.box.slice(-80));
    check('a touch device is told the gestures and may start', /tap to click, long press for the right button \(keep the finger down to drag with it\), two fingers to scroll/.test(g.box) && g.start === 1, g.box.slice(0, 200));

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
      // The engine stops itself with no message (Emscripten's abort with nothing: a player's Firefox said only "The game
      // stopped: undefined", 16 November 2026): the box says what happened, and the log has the stack of that moment.
      await pg.evaluate(() => { function f_00412345() { window.Module.onAbort(); } f_00412345(); });
      const aborted = { box: (await pg.textContent('#overlay')).replace(/\s+/g, ' '), log: await pg.textContent('#log') };
      check('an abort with no message: the box says the engine stopped itself, and the log has where',
        /The game stopped/.test(aborted.box) && !/undefined/.test(aborted.box) && /stopped itself \(abort\) without a message/.test(aborted.box)
          && /page: the engine stopped itself \(abort\) without a message/.test(aborted.log) && /page: at .*f_00412345/.test(aborted.log),
        aborted.box.slice(0, 160));
      // The game's own fatal box before the exit (the runtime logs it, waits.c): the error box quotes it.
      await pg.evaluate(() => { window.Module.printErr('recomp: message box from 0x555c66: Error: v3.9.68 discipline..cpp 274'); window.Module.onExit(70); });
      const box = (await pg.textContent('#overlay')).replace(/\s+/g, ' ');
      check('an exit after the game\'s own error box: the box quotes it and where it came from',
        /The game stopped on an error of its own: "v3\.9\.68 discipline\.\.cpp 274" \(at 0x555c66\)/.test(box), box.slice(0, 200));
      // Send to the developer, from the box: the log, a note and a save of the player's choice. This server keeps no
      // files (no bucket), so the log goes and the box says the save did not.
      const hasReport = await pg.locator('#fail-report').count() === 1;
      await pg.click('#fail-report');
      const dlg = await pg.evaluate(() => ({ open: document.getElementById('report').open, error: document.getElementById('report-error').textContent,
        intro: document.querySelector('#report .dlg-body p').textContent.replace(/\s+/g, ' '),
        options: [...document.querySelectorAll('#report-save option')].map((o) => `${o.selected ? '*' : ''}${o.textContent}`) }));
      await pg.fill('#report-note', 'I pressed Exit and it stopped.');
      await pg.selectOption('#report-save', '/user/Harbour City 2001-07-14.sav');
      // A log of long lines (a stack is one line): the page sends each line cut to what the server keeps, so a long log
      // still fits the request (3 MB of log here, more than the 2 MB the server takes).
      await pg.evaluate(() => { for (let i = 0; i < 300; i++) window.Module.printErr(`recomp: long line ${i} ${'x'.repeat(10000)}`); });
      await pg.click('#report-send');
      await pg.waitForFunction(() => /was sent|Sent/.test(document.getElementById('report-state').textContent), null, { timeout: 15000 }).catch(() => {});
      const said = await pg.textContent('#report-state');
      const adm = await (await fetch(`http://127.0.0.1:${PORT}/api/admin/stats`, { headers: { authorization: 'Bearer page-check-admin-token' } })).json();
      const listed = adm.reports?.[0] ?? {};
      check('the error box: Send to the developer opens with the error and the saves; with no save in play, No save is chosen',
        hasReport && dlg.open && /The game ended with code 70/.test(dlg.error) && dlg.options[0] === '*No save' && /^Harbour City 2001-07-14\.sav \(262 KB\)/.test(dlg.options[1] ?? ''),
        JSON.stringify(dlg));
      check('the dialog names everything a report holds (the page\'s size, the device\'s memory, the id or account)',
        /the size of the page and the device's memory/.test(dlg.intro) && /signed in, your account/.test(dlg.intro) && !/Nothing else of your browser/.test(dlg.intro), dlg.intro);
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
      /junk\.sav: it is a text file, not a saved game/.test(refusedNote)
        && /St-Etienne 2002-02-13\.sav: the file is empty \(0 bytes\)/.test(refusedNote) && /notes\.txt: it is too short to be a saved game/.test(refusedNote)
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
    // A save zipped by another site (a player's Matt.zip, 16 November 2026: "only .sav files are saved games"): the
    // saves in it are taken; a zip with none says so.
    const { writeZip } = await import('../zip.js');
    const zTactic = Buffer.alloc(1476); zTactic.writeUInt32LE(0x98ec5e, 0); zTactic.writeUInt32LE(0xf2fffff2, 1472);
    const zipped = Buffer.from(await writeZip([{ name: 'Matt/Matt.sav', data: got }, { name: 'readme.txt', data: Buffer.from('from another site') },
      { name: 'Matt/Pressing.tct', data: zTactic }]));
    await p.setInputFiles('#saves-file', [{ name: 'Matt.zip', mimeType: 'application/zip', buffer: zipped },
      { name: 'Nothing.zip', mimeType: 'application/zip', buffer: Buffer.from(await writeZip([{ name: 'a.txt', data: Buffer.from('x') }])) }]);
    await p.waitForFunction(() => /Matt/.test(document.getElementById('saves-note').textContent), null, { timeout: 15000 }).catch(() => {});
    const zipNote = await p.textContent('#saves-note');
    const zipBack = await p.evaluate(() => { try { return window.Module.FS.readFile('/user/Matt.sav').length; } catch { return -1; } });
    check('a zip from another site: the save in it is uploaded; a zip with no save is refused with its reason',
      zipBack === got.length && /Uploaded Matt\.sav/.test(zipNote) && /Nothing\.zip: it holds no saved game: a\.txt/.test(zipNote)
        && /Matt\.sav/.test(await p.textContent('#saves-list')), `${zipBack} | ${zipNote}`);
    check('and its tactic goes to Tactics, which says so', /Pressing is a tactic: it is under Tactics now/.test(zipNote)
      && /Pressing/.test(await p.textContent('#tactics-list')), zipNote);
    await p.setInputFiles('#saves-file', [{ name: 'Matt.rar', mimeType: 'application/octet-stream', buffer: Buffer.from('Rar!\x1a\x07\x01\x00rest') }]);
    await p.waitForFunction(() => /Matt\.rar/.test(document.getElementById('saves-note').textContent), null, { timeout: 5000 }).catch(() => {});
    check('a RAR archive: the page says it opens .zip only, and how to go on', /Matt\.rar: it is a RAR archive.*unpack it first/.test(await p.textContent('#saves-note')),
      await p.textContent('#saves-note'));
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
    // Saved games in tabs: Saves, Tactics, Databases; the dialog opens on Saves.
    const tabs = await p.evaluate(() => ({ shown: [...document.querySelectorAll('#saves .tabs [data-tab]')].filter((b) => b.offsetParent !== null).map((b) => b.textContent),
      on: document.getElementById('saves').dataset.tab, tactics: document.getElementById('tactics-list').offsetParent !== null }));
    await p.click('#saves .tabs [data-tab="tactics"]');
    const tabbed = await p.evaluate(() => ({ tactics: document.getElementById('tactics-list').offsetParent !== null, saves: document.getElementById('saves-list').offsetParent !== null,
      sets: document.getElementById('sets-list').offsetParent !== null }));
    check('Saved games in tabs: Saves, Tactics, Databases; Saves first, one tab shown at a time',
      tabs.shown.join() === 'Saves,Tactics,Databases' && tabs.on === 'saves' && !tabs.tactics && tabbed.tactics && !tabbed.saves && !tabbed.sets, JSON.stringify([tabs, tabbed]));
    const [tdl] = await Promise.all([p.waitForEvent('download'), p.click('#tactics-list button[data-tactic="1"]')]);
    const tdlPath = await tdl.path();
    check('a tactic downloads as its .tct file', tdl.suggestedFilename() === 'My 4-4-2.tct' && fs.readFileSync(tdlPath).equals(tct(0x98ec5e)), tdl.suggestedFilename());
    await p.click('#tactics-list button[data-tactic-delete="0"]');
    check('a tactic is deleted', !(await p.evaluate(() => window.Module.FS.readdir('/user/tactics').includes('442_diamond_default.tct')))
      && !/442_diamond_default/.test(await p.textContent('#tactics-list')));
    await p.click('#saves-close');
    await p.click('#saves-open');
    check('Saved games opens on Saves again', (await p.evaluate(() => document.getElementById('saves').dataset.tab)) === 'saves' && await p.isVisible('#saves-list'));
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
    check('the heartbeat carries the browser\'s language', statsCalls.some((c) => c.startsWith('/api/stats/beat') && /"lang":"[a-z]{2}(-[A-Za-z]+)?"/.test(c)),
      statsCalls.filter((c) => c.startsWith('/api/stats/beat')).join(' | ').slice(0, 300));
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
    // Play (play.js; the runtime's play.c): a save's Play button asks the engine to load it from its start screen. With
    // a game in play the page asks first, restarts as Restart Game does, and asks the engine after the start.
    const toasts = () => p.evaluate(() => [...document.querySelectorAll('#toasts .toast')].map((t) => t.textContent).join(' | '));
    await p.click('#saves-open');
    await p.click('#saves-list button[data-play="0"]');
    const asked = await p.evaluate(() => ({ played: window.__played, open: document.getElementById('saves').open }));
    const askedToast = await toasts();
    check('Play: the engine is asked for the save, and Saved games closes',
      JSON.stringify(asked.played) === '["Harbour City 2001-07-14.sav"]' && !asked.open && /Opening Harbour City 2001-07-14\.sav/.test(askedToast),
      `${JSON.stringify(asked)} | ${askedToast}`);
    await p.evaluate(() => window.Module.rtPlay(6));
    const failedToast = await toasts();
    check('Play: an engine that could not do it says why, and where the player can open the save',
      /Harbour City 2001-07-14\.sav did not open: the game's File entry did not take its name .*Restore Saved Game on the game's start screen opens it too/.test(failedToast), failedToast);
    // A name the game's File entry refuses: the engine says so at once (play.c's answer), and Saved games stays open.
    await p.evaluate(() => { window.__playAnswer = 1; document.querySelectorAll('#toasts .toast').forEach((t) => t.remove()); });
    await p.click('#saves-open');
    await p.click('#saves-list button[data-play="0"]');
    const refusedPlay = { toast: await toasts(), open: await p.evaluate(() => document.getElementById('saves').open) };
    check('Play: a name the game cannot take is refused at once, with why, and no "Opening"',
      /did not open: its name has characters the game's File entry does not take/.test(refusedPlay.toast) && !/Opening/.test(refusedPlay.toast) && refusedPlay.open,
      JSON.stringify(refusedPlay));
    await p.evaluate(() => { window.__playAnswer = -1; window.__played = []; });
    await p.click('#saves-close');
    await p.evaluate(() => { window.__inGame = true; });
    let question = '';
    p.once('dialog', (d) => { question = d.message(); });            // the page check's own handler accepts it
    const replayed = p.waitForNavigation({ timeout: 30000 }).then(() => true, () => false);
    await p.click('#saves-open');
    await p.click('#saves-list button[data-play="0"]');
    check('Play with a game in play: the page asks first, then loads again', await replayed && /game in play closes/.test(question), question);
    await p.waitForFunction(() => window.__fake, null, { timeout: 120000 });
    await p.waitForFunction(() => window.__played?.length, null, { timeout: 10000 }).catch(() => {});
    check('and after the start the engine is asked for the save, once',
      JSON.stringify(await p.evaluate(() => window.__played)) === '["Harbour City 2001-07-14.sav"]'
        && (await p.evaluate(() => sessionStorage.getItem('cm0102-play'))) === null, JSON.stringify(await p.evaluate(() => window.__played)));
    // A name left waiting by a start that never came (an hour old): not opened at a later start.
    await p.evaluate(() => sessionStorage.setItem('cm0102-play', JSON.stringify({ name: 'Harbour City 2001-07-14.sav', at: Date.now() - 3600000 })));
    await p.reload();
    await p.waitForFunction(() => window.__fake, null, { timeout: 120000 });
    await p.waitForTimeout(500);
    check('an old waiting name is dropped, not opened', JSON.stringify(await p.evaluate(() => window.__played)) === '[]'
      && (await p.evaluate(() => sessionStorage.getItem('cm0102-play'))) === null);
    // The game's long jobs (progress.js, display.c): a stage that runs shows in the bar with its share and time.
    await p.evaluate(() => { window.__progress = 45; window.Module.printErr('recomp: progress: Player histories'); });
    await p.waitForFunction(() => /^Player histories 45% · 0:0\d$/.test(document.getElementById('status').textContent), null, { timeout: 3000 }).catch(() => {});
    const during = await p.textContent('#status');
    await p.evaluate(() => { window.__progress = -1; });
    await p.waitForTimeout(600);
    check('a long job of the game shows in the bar with its share and time, and goes when it ends',
      /^Player histories 45% · 0:0\d$/.test(during) && (await p.textContent('#status')) === '', `${during} | ${await p.textContent('#status')}`);
    // A new version (release.js): the server names another release; the page shows New version when it comes back
    // into view, and the button loads the page again the way Restart Game does.
    const release = await p.evaluate(() => fetch('/version.json').then((r) => r.json()).then((j) => j.release));
    check('the page runs the release the server names', /^[0-9a-f]{12}$/.test(release ?? '') && (await p.isHidden('#update')), release);
    // A deploy with another page file: its time changes, and so the release (the service worker passes the page's
    // request to the server, so the server itself is asked).
    const releaseFile = path.join(__dirname, '..', 'release.js'), was = fs.statSync(releaseFile);
    fs.utimesSync(releaseFile, was.atime, new Date(was.mtimeMs + 60000));
    // The server keeps its answer 10 s: the page comes back into view until it sees the new one.
    for (let i = 0; i < 8 && await p.isHidden('#update'); i++) {
      await p.evaluate(() => document.dispatchEvent(new Event('visibilitychange')));
      await p.waitForSelector('#update:not([hidden])', { timeout: 2000 }).catch(() => {});
    }
    const updateToast = await toasts();
    check('a new release on the server: the page shows New version and says so once', await p.isVisible('#update')
      && /A new version of the game is out/.test(updateToast), updateToast);
    const updated = p.waitForNavigation({ timeout: 30000 }).then(() => true, () => false);
    await p.click('#update');
    check('New version loads the page again (through Restart Game\'s path: the saves stored first)', await updated);
    fs.utimesSync(releaseFile, was.atime, was.mtime);
    await p.waitForFunction(() => window.__fake, null, { timeout: 120000 });
    const coffee = await p.evaluate(() => [...document.querySelectorAll('a.coffee')].map((a) => ({ href: a.href, target: a.target, rel: a.rel,
      shown: a.offsetParent !== null })));
    check('Buy me a coffee: in the bar and in Help, opening the page set on the server in a new tab',
      coffee.length === 2 && coffee.every((c) => c.href === 'https://buymeacoffee.com/cm0102-test' && c.target === '_blank' && /noopener/.test(c.rel))
        && coffee[0].shown, JSON.stringify(coffee));
    // The bar's buttons are one height at every size (a phone showed Buy me a coffee shorter than Account and Help).
    const barSizes = [];
    let sideways = null;
    for (const [name, opts] of [['computer', { viewport: { width: 1280, height: 800 } }],
      ['phone', { viewport: { width: 390, height: 844 }, deviceScaleFactor: 3, isMobile: true, hasTouch: true }],
      ['iPad', { viewport: { width: 1080, height: 810 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true }],
      ['small window', { viewport: { width: 600, height: 700 } }],
      ['phone sideways', { viewport: { width: 844, height: 390 }, deviceScaleFactor: 3, isMobile: true, hasTouch: true }]]) {
      const ctx = await b.newContext(opts);
      const bp = await ctx.newPage();
      await bp.goto(`http://127.0.0.1:${PORT}/`);
      await bp.waitForSelector('#coffee-open:not([hidden])', { timeout: 30000 });
      if (name === 'phone sideways') {
        // The bar is a column at the right edge: the game's stage has the screen's whole height (an iPhone held sideways,
        // 21 November 2026: the bar above took an eighth of it).
        sideways = await bp.evaluate(() => { const bar = document.getElementById('bar').getBoundingClientRect(), st = document.getElementById('stage').getBoundingClientRect();
          const shown = [...document.querySelectorAll('#bar > button, #bar > a')].filter((e) => e.offsetParent !== null).map((e) => e.getBoundingClientRect());
          return { barW: Math.round(bar.width), barRight: Math.round(bar.right), barH: Math.round(bar.height), stageH: Math.round(st.height), stageTop: Math.round(st.top), stageW: Math.round(st.width),
            vw: innerWidth, vh: innerHeight, column: shown.every((r, i) => i === 0 || r.top > shown[i - 1].top), inBar: shown.every((r) => r.bottom <= bar.bottom + 0.5) }; });
      }
      const hs = await bp.evaluate(() => [...document.querySelectorAll('#bar > button, #bar > a')].filter((e) => e.offsetParent !== null)
        .map((e) => `${e.id}=${Math.round(e.getBoundingClientRect().height * 10) / 10}`));
      const coarse = await bp.evaluate(() => matchMedia('(pointer: coarse)').matches);
      barSizes.push(`${name}${coarse ? ' (touch)' : ''}: ${hs.join(' ')}`);
      await ctx.close();
    }
    check('the bar\'s buttons, Buy me a coffee among them, are one height on a computer, a phone, an iPad and a small window',
      barSizes.every((row) => { const hs = row.split(': ')[1].split(' ').map((x) => x.split('=')[1]); return hs.length >= 2 && hs.every((h) => h === hs[0]) && row.includes('coffee-open'); }),
      barSizes.join(' | '));
    check('a phone held sideways: the bar is a column at the right edge, and the game has the height but the 20 pixels at the top edge (iOS keeps that edge)',
      sideways && sideways.column && sideways.inBar && sideways.barRight === sideways.vw && sideways.barW <= 60 && sideways.stageH === sideways.vh - 20 && sideways.stageTop === 20
        && sideways.stageW === sideways.vw - sideways.barW, JSON.stringify(sideways));
    await p.click('#help-open');
    const helpText = (await p.textContent('#help')).replace(/\s+/g, ' ');
    const made = await p.evaluate(() => { const m = document.getElementById('made'), svg = m.querySelector('svg'), r = m.getBoundingClientRect();
      return { text: m.textContent.replace(/\s+/g, ' ').trim(), shown: m.offsetParent !== null, atBottom: Math.abs(r.bottom - innerHeight) < 1, height: r.height,
        fill: svg && getComputedStyle(svg).fill, label: svg?.getAttribute('aria-label'), canvasAbove: document.getElementById('canvas').getBoundingClientRect().bottom <= r.top + 0.5 }; });
    check('the page\'s footer under the game: Made with a red heart by the seller; not in Help', made.text === 'Made with by A. Maker' && made.shown && made.atBottom
      && made.canvasAbove && made.height <= 28 && made.fill === 'rgb(229, 72, 77)' && made.label === 'love' && !/Made with/.test(helpText), JSON.stringify(made));
    check('Help lists the gestures, the right-button drag of a run (and the Mac\'s Control), and holds the engine log',
      /Long press \(a ring fills\), then lift or drag/.test(helpText) && /Right click, then a tap or a drag/.test(helpText) && /A tap with two fingers/.test(helpText)
        && /drag a player with the right button to give him a run/.test(helpText) && /On a Mac, Control and a click is the right button/.test(helpText)
        && /page: main\(--game \/game\)/.test(await p.textContent('#log')));
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
    // A player's run on the tactics board is a drag with the right button (a Reddit report: it could not be made by touch).
    // The long press puts the right button down and the finger drags it; after Right click, a drag is the right button's.
    await t.evaluate(() => {
      window.__seq = [];
      const c = document.getElementById('canvas');
      c.addEventListener('mousedown', (e) => window.__seq.push(`down${e.button}`), true);
      c.addEventListener('mousemove', (e) => { if (e.buttons) window.__seq.push(`move${e.buttons}`); }, true);
      document.addEventListener('mouseup', (e) => window.__seq.push(`up${e.button}@${Math.round(e.clientY)}`), true);
    });
    const dragBy = async (pressMs) => {
      await t.evaluate(() => { window.__seq.length = 0; });
      await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [at] });
      await t.waitForTimeout(pressMs);
      for (let i = 1; i <= 5; i++) {
        await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: [{ ...at, y: at.y - i * 20 }] });
        await t.waitForTimeout(30);
      }
      await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
      await t.waitForTimeout(150);
      return (await t.evaluate(() => window.__seq.join(' '))).replace(/(move2 )+/g, 'move2… ');
    };
    const longDrag = await dragBy(600);
    check('a long press, then a drag: the right button goes down, drags with the finger, and goes up where it lifts',
      longDrag === `down2 move2… up2@${at.y - 100}`, longDrag);
    await t.tap('#right-click');
    const armedDrag = await dragBy(50);
    check('Right click, then a drag: a drag with the right button, and the button is off again',
      armedDrag === `down2 move2… up2@${at.y - 100}` && (await t.getAttribute('#right-click', 'aria-pressed')) === 'false', armedDrag);
    const plainDrag = (await dragBy(50)).replace(/(move1 )+/g, 'move1… ');
    check('a plain drag is still the left button\'s (a player moved on the board)', plainDrag === `down0 move1… up0@${at.y - 100}`, plainDrag);
    // A second finger that touches and lifts while the first one drags (with either button): the first finger's lift
    // still lets the button go, so the game does not keep it held.
    // Touch events made in the page: the DevTools touch API lifts every finger at a touchEnd, not one of two.
    const withSecondFinger = (pressMs) => t.evaluate(async ({ x, y, pressMs: ms }) => {
      window.__seq.length = 0;
      const c = document.getElementById('canvas');
      const T = (id, tx, ty) => new Touch({ identifier: id, target: c, clientX: tx, clientY: ty });
      const fire = (type, touches, changed) => c.dispatchEvent(new TouchEvent(type, { bubbles: true, cancelable: true, touches, targetTouches: touches, changedTouches: changed }));
      const a0 = T(1, x, y), a1 = T(1, x, y - 40), b = T(2, x + 80, y);
      fire('touchstart', [a0], [a0]);
      await new Promise((r) => setTimeout(r, ms));
      fire('touchmove', [a1], [a1]);
      fire('touchstart', [a1, b], [b]);
      fire('touchend', [a1], [b]);                       // the second finger lifts first
      fire('touchend', [], [a1]);
      await new Promise((r) => setTimeout(r, 150));
      return window.__seq.join(' ');
    }, { x: at.x, y: at.y, pressMs });
    const rightTwo = await withSecondFinger(600), leftTwo = await withSecondFinger(50);
    check('a second finger during a drag: the first finger\'s lift still lets the button go (right and left)',
      /^down2 .*up2@\d+$/.test(rightTwo) && /^down0 .*up0@\d+$/.test(leftTwo), `${rightTwo} | ${leftTwo}`);
    // The game reads the place of the last move of a frame, not of the press (its pump 0x68d520, WM_MOUSEMOVE 0x68d922):
    // a drag's press stands alone for 80 ms, so it is read where the finger first touched (an iPhone report: a player
    // dragged onto another for a substitution opened the player he had reached instead). A lift in that time waits too.
    const dragTimes = (dy, small) => t.evaluate(async ({ x, y, dy: d }) => {
      const out = [], c = document.getElementById('canvas');
      const log = (e) => out.push({ type: e.type, b: e.buttons, y: Math.round(e.clientY), t: performance.now() });
      c.addEventListener('mousedown', log, true); c.addEventListener('mousemove', log, true); document.addEventListener('mouseup', log, true);
      const T = (ty) => new Touch({ identifier: 7, target: c, clientX: x, clientY: ty });
      const fire = (type, touches, changed) => c.dispatchEvent(new TouchEvent(type, { bubbles: true, cancelable: true, touches, targetTouches: touches, changedTouches: changed }));
      fire('touchstart', [T(y)], [T(y)]);
      fire('touchmove', [T(y - d)], [T(y - d)]);
      fire('touchmove', [T(y - d - 20)], [T(y - d - 20)]);
      fire('touchend', [], [T(y - d - 20)]);
      await new Promise((r) => setTimeout(r, 250));
      c.removeEventListener('mousedown', log, true); c.removeEventListener('mousemove', log, true); document.removeEventListener('mouseup', log, true);
      return out;
    }, { x: at.x, y: small ? small.y : at.y, dy });
    const quick = await dragTimes(40);
    const down = quick.find((e) => e.type === 'mousedown'), after = quick.filter((e) => e.t > down.t);
    check('a drag\'s press stands alone for 80 ms: read where the finger touched first, then the last place and the lift',
      down.y === at.y && after.length === 2 && after[0].type === 'mousemove' && after[0].b === 1 && after[0].y === at.y - 60
        && after[0].t - down.t >= 75 && after[1].type === 'mouseup' && after[1].y === at.y - 60,
      JSON.stringify(quick.map((e) => [e.type, e.y, Math.round(e.t - quick[0].t)])));
    // A small picture (a phone): rows a few pixels high, so a move of 8 pixels to the next row is a drag, not a tap.
    await t.setViewportSize({ width: 420, height: 320 });
    await t.waitForTimeout(300);
    const sb = await t.locator('#canvas').boundingBox();
    const smallAt = { y: Math.round(sb.y + sb.height * 0.6) };
    const nudge = await dragTimes(8, smallAt);
    check('on a small picture a move of 8 pixels is a drag (a substitution with the next row)',
      nudge.some((e) => e.type === 'mousedown' && e.y === smallAt.y) && nudge.at(-1).type === 'mouseup' && nudge.at(-1).y === smallAt.y - 28,
      JSON.stringify(nudge.map((e) => [e.type, e.y])));
    await t.setViewportSize({ width: 1024, height: 768 });
    check('no page errors on the touch device', terrors.length === 0, terrors.join('; '));
    await tc.close();
    // A Mac: Control and a click is the right button there (and a trackpad's only easy right-button drag); the browser
    // reports the left button with Control, so the page turns it into the right button for the game, press to release.
    {
      const mc = await b.newContext({ viewport: { width: 1024, height: 768 } });
      await mc.addInitScript(() => { Object.defineProperty(Navigator.prototype, 'userAgentData', { get: () => ({ platform: 'macOS' }) }); });
      const m = await mc.newPage();
      await m.goto(`http://127.0.0.1:${PORT}/`);
      await m.waitForSelector('#go');
      await m.click('#go');
      await m.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      await m.evaluate(() => {
        window.__seq = [];
        const c = document.getElementById('canvas');
        c.addEventListener('mousedown', (e) => window.__seq.push(`down${e.button}`));
        document.addEventListener('mouseup', (e) => window.__seq.push(`up${e.button}`));
      });
      const cb2 = await m.locator('#canvas').boundingBox();
      const mx = cb2.x + cb2.width / 2, my = cb2.y + cb2.height / 2;
      await m.mouse.move(mx, my);
      await m.keyboard.down('Control');
      await m.mouse.down();
      await m.mouse.move(mx, my - 60, { steps: 5 });
      await m.keyboard.up('Control');                     // let go before the button: the release is still the right one's
      await m.mouse.up();
      await m.mouse.click(mx, my);
      const macSeq = await m.evaluate(() => window.__seq.join(' '));
      check('a Mac: Control and a drag is the right button for the game, a plain click the left', macSeq === 'down2 up2 down0 up0', macSeq);
      // The window loses the focus while a Control-press is held (Command-Tab): its release never comes, so the page lets
      // the right button go then, and a later click is a plain one.
      await m.evaluate(() => { window.__seq.length = 0; });
      await m.keyboard.down('Control');
      await m.mouse.down();
      await m.keyboard.up('Control');
      await m.evaluate(() => window.dispatchEvent(new Event('blur')));
      const atBlur = await m.evaluate(() => window.__seq.join(' '));
      await m.mouse.up();
      await m.mouse.click(mx, my);
      const afterBlur = await m.evaluate(() => window.__seq.join(' '));
      check('a Mac: a blur during a Control-press lets the right button go, and a later click is the left button\'s',
        atBlur === 'down2 up2' && /down0 up0$/.test(afterBlur) && !/up2.*up2/.test(afterBlur), `${atBlur} | ${afterBlur}`);
      await mc.close();
    }
    // An installed app (display-mode standalone): a computer's app window can still go full screen, so the button
    // shows (it was hidden in every installed app: a player's Mac app had none); an iPhone or iPad home-screen app
    // (navigator.standalone) is full already and gets none.
    for (const [label, ios] of [['a computer\'s app window', false], ['an iPad home-screen app', true]]) {
      const ac = await b.newContext({ viewport: { width: 1200, height: 800 } });
      await ac.addInitScript((onIos) => {
        const real = window.matchMedia.bind(window);
        window.matchMedia = (q) => (/display-mode: standalone/.test(q) ? { matches: true, media: q, addEventListener() {}, removeEventListener() {} } : real(q));
        if (onIos) Object.defineProperty(navigator, 'standalone', { value: true });
      }, ios);
      const ap = await ac.newPage();
      await ap.goto(`http://127.0.0.1:${PORT}/`);
      await ap.click('#consent-no', { timeout: 5000 }).catch(() => {});
      await ap.click('#go', { timeout: 5000 }).catch(() => {});
      await ap.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      await ap.waitForTimeout(300);
      const shown = await ap.isVisible('#fullscreen');
      check(`${label}: Full screen ${ios ? 'is not offered (it is full already)' : 'is offered'}`, shown === !ios, String(shown));
      await ac.close();
    }
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
    await o.click('#saves-open');
    check('and no Play button: an engine without rt_web_play cannot open a save from the page',
      (await o.locator('#saves-list button[data-play]').count()) === 0 && (await o.locator('#saves-list button[data-kind="save"]').count()) > 0);
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
      await pg.waitForTimeout(100);
      // Before full screen, with the game's buttons all there: each one inside the window, all of one height.
      const bar = await pg.evaluate(() => {
        const shown = [...document.querySelectorAll('#bar > button, #bar > a')].filter((e) => e.offsetParent !== null);
        const r = shown.map((e) => e.getBoundingClientRect());
        return { n: shown.length, inside: r.every((x) => x.left >= 0 && x.right <= innerWidth + 0.5), heights: [...new Set(r.map((x) => Math.round(x.height)))],
          cls: document.getElementById('bar').className, title: document.querySelector('#bar .brand').offsetParent !== null,
          made: document.getElementById('made').offsetParent !== null,
          labels: [...document.querySelectorAll('#bar > button span, #bar > a span')].some((e) => e.offsetParent !== null) };
      });
      await pg.tap('#fullscreen');
      await pg.waitForFunction(() => document.getElementById('app').classList.contains('fs'), null, { timeout: 5000 }).catch(() => {});
      await pg.waitForTimeout(300);
      return { ctx, pg, errs, bar };
    }
    const upright = await fullScreenOn({ width: 390, height: 844 });
    const small = await fullScreenOn({ width: 360, height: 740 });
    const phone = await fullScreenOn({ width: 844, height: 390 });
    check('the bar fits a phone held sideways, upright and a small phone: icons only, every button on screen, one height',
      [phone, upright, small].every((x) => x.bar.n >= 6 && x.bar.inside && x.bar.heights.length === 1 && !x.bar.labels)
        && /compact/.test(upright.bar.cls) && /compact/.test(small.bar.cls)       // sideways: a column, icons only by itself
        && upright.bar.title && !small.bar.title,
      [phone, upright, small].map((x) => JSON.stringify(x.bar)).join(' | '));
    await upright.ctx.close();
    await small.ctx.close();
    const phoneBar = await phone.pg.locator('#bar').boundingBox(), phoneCanvas = await phone.pg.locator('#canvas').boundingBox();
    check('the footer: on an upright phone, not on one held sideways (the game takes every pixel), never in full screen',
      upright.bar.made === true && small.bar.made === true && phone.bar.made === false && !(await phone.pg.isVisible('#made')),
      JSON.stringify([upright.bar.made, small.bar.made, phone.bar.made]));
    check('full screen on a phone held sideways: the buttons are a column beside the game, over none of it',
      (await phone.pg.evaluate(() => document.getElementById('app').dataset.bar)) === 'side' && !overlap(phoneBar, phoneCanvas)
        && phoneBar.height > phoneBar.width && (await phone.pg.locator('#bar-fold').isVisible()) === false,
      `${await phone.pg.evaluate(() => document.getElementById('app').dataset.bar)} bar ${JSON.stringify(phoneBar)} canvas ${JSON.stringify(phoneCanvas)}`);
    if (process.argv[2]) await phone.pg.screenshot({ path: process.argv[2].replace(/\.png$/, '-fs-phone.png') });
    check('no page errors in full screen on the phone', phone.errs.length === 0, phone.errs.join('; '));
    await phone.ctx.close();
    const pad = await fullScreenOn({ width: 1180, height: 820 });
    const padOpen = await pad.pg.locator('#bar').boundingBox();
    check('the footer shows on an iPad before full screen, and goes in full screen', pad.bar.made === true && !(await pad.pg.isVisible('#made')),
      JSON.stringify([pad.bar.made, await pad.pg.isVisible('#made')]));
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
      await one.evaluate(() => { window.Module.SDL2 = { audioContext: { state: 'running', suspend() { this.state = 'suspended'; return Promise.resolve(); } } }; });
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
      check('the tab that handed over is silent (an engine that cannot be stopped plays on behind its box)',
        (await one.evaluate(() => window.Module.SDL2.audioContext.state)) === 'suspended');
      check('no page errors with two tabs', errs.length === 0, errs.join('; '));
      await ctx.close();
    }

    // A hand-over whose store takes long (a large folder on a phone): the asking tab starts only once it is done, not
    // after a fixed wait, or it reads the storage while the first tab still writes it.
    {
      const ctx = await b.newContext({ viewport: { width: 1000, height: 700 } });
      const one = await ctx.newPage(), two = await ctx.newPage();
      await one.goto(`http://127.0.0.1:${PORT}/`);
      await one.waitForSelector('#go');
      await one.click('#go');
      await one.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      await one.evaluate(() => { window.__slowStore = 70000; });
      await two.goto(`http://127.0.0.1:${PORT}/`);
      await two.waitForSelector('#turn-take', { timeout: 30000 });
      await two.click('#turn-take');
      await two.waitForFunction(() => window.__fake, null, { timeout: 100000 }).catch(() => {});
      await one.waitForFunction(() => window.__storeDoneAt, null, { timeout: 30000 }).catch(() => {});
      const [doneAt, startedAt] = [await one.evaluate(() => window.__storeDoneAt), await two.evaluate(() => window.__fakeAt)];
      check('a slow store at a hand-over: the asking tab starts only after it is done', Boolean(doneAt && startedAt) && startedAt >= doneAt,
        `store done ${doneAt}, the other tab started ${startedAt} (${startedAt && doneAt ? Math.round((startedAt - doneAt) / 1000) : '?'} s)`);
      await ctx.close();
    }

    // A hand-over while the first tab is still starting (its folder not yet read from IndexedDB): the first tab must
    // not store its empty folder, which would remove every stored file (a review of the two-tab code, 14 November 2026).
    {
      const ctx = await b.newContext({ viewport: { width: 1000, height: 700 } });
      const one = await ctx.newPage(), two = await ctx.newPage();
      await one.addInitScript(() => { window.__slowRead = 8000; });
      await one.goto(`http://127.0.0.1:${PORT}/`);
      await one.waitForSelector('#go');
      await one.click('#go');
      await one.waitForFunction(() => (window.__syncLog ?? []).includes('read'), null, { timeout: 120000 });
      await two.goto(`http://127.0.0.1:${PORT}/`);
      await two.waitForSelector('#turn-take', { timeout: 30000 });
      await two.click('#turn-take');
      await two.waitForFunction(() => window.__fake, null, { timeout: 60000 }).catch(() => {});
      await one.waitForTimeout(9000);                    // past the slow read
      const order = await one.evaluate(() => window.__syncLog.join(', '));
      const firstStore = order.indexOf('store'), readDone = order.indexOf('read done');
      check('a hand-over while the first tab still reads its folder: it stores nothing before the read is done (no wipe)',
        firstStore === -1 || (readDone !== -1 && firstStore > readDone), order);
      await ctx.close();
    }

    // Large saves (a player has several of 690 MB): Saved games says what they take and the room left, marks the bar's
    // button when that is little, shows about six rows of the list, sums the older copies and removes them at one
    // press, and Download all goes in parts when a zip (4 GB) cannot hold everything.
    {
      const ctx = await b.newContext({ viewport: { width: 1100, height: 760 }, acceptDownloads: true });
      await ctx.addInitScript(() => { navigator.storage.estimate = async () => ({ usage: 9.9 * 2 ** 30, quota: 10 * 2 ** 30 }); });
      const pg = await ctx.newPage();
      const errs = [];
      pg.on('pageerror', (e) => errs.push(e.message));
      pg.on('dialog', (d) => d.accept().catch(() => {}));
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      await pg.waitForSelector('#go');
      await pg.click('#go');
      await pg.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      await pg.evaluate(() => {
        const FS = window.Module.FS, s = FS.readFile('/user/Harbour City 2001-07-14.sav');
        FS.mkdirTree('/user/backups');
        for (const n of [1, 2]) FS.writeFile(`/user/backups/Harbour City 2001-07-14.${n}.sav`, s);
        for (const n of [1, 2, 3, 4, 5, 6, 7]) FS.writeFile(`/user/Career ${n}.sav`, s);
      });
      await pg.waitForSelector('#saves-open.low', { timeout: 15000 }).catch(() => {});
      const lowToast = await pg.evaluate(() => [...document.querySelectorAll('#toasts .toast')].map((t) => t.textContent).join(' | '));
      check('little room left in the browser: a mark on the Saved games button, and the player is told',
        await pg.locator('#saves-open.low').count() === 1 && /Little room is left in this browser for saves/.test(lowToast), lowToast.slice(0, 160));
      await pg.click('#saves-open');
      await pg.waitForFunction(() => document.getElementById('saves-space').textContent, null, { timeout: 5000 }).catch(() => {});
      const space = await pg.textContent('#saves-space');
      check('Saved games says what the saves take, the room left, and that it is little',
        /^8 saves, 3 MB · this browser allows about 102 MB more\. That is little: a save may not fit\./.test(space), space);
      const rows = await pg.evaluate(() => {
        const list = document.getElementById('saves-list').getBoundingClientRect();
        return [...document.querySelectorAll('#saves-list > table tr')].filter((tr) => tr.querySelector('td') && tr.getBoundingClientRect().bottom <= list.bottom + 1).length;
      });
      check('the list shows six rows or more at once (it showed two or three)', rows >= 6, `${rows} rows`);
      // A narrower window: no column goes to a letter a line (the Saved column did once the Play button came).
      await pg.setViewportSize({ width: 820, height: 760 });
      const dates = await pg.$$eval('#saves-list > table tr', (trs) => trs.filter((tr) => tr.querySelector('td')).map((tr) => tr.children[2])
        .map((t) => [Math.round(t.getBoundingClientRect().width), t.querySelectorAll('.small').length]));
      check('a narrower window: each save\'s date keeps its width, the day over the time',
        dates.length >= 8 && dates.every(([w, timeLine]) => w >= 80 && timeLine === 1), JSON.stringify(dates.slice(0, 3)));
      await pg.setViewportSize({ width: 1100, height: 760 });
      const summary = await pg.textContent('#saves-list details summary');
      await pg.click('#saves-list details summary');
      await pg.click('#backups-remove');
      await pg.waitForFunction(() => /Removed/.test(document.getElementById('saves-note').textContent), null, { timeout: 5000 }).catch(() => {});
      check('the older copies say what they take, and Remove all older copies removes them (the saves stay)',
        /^Older copies \(2, 1 MB\)$/.test(summary.trim()) && /Removed 2 older copies: 1 MB free again/.test(await pg.textContent('#saves-note'))
          && await pg.evaluate(() => window.Module.FS.readdir('/user/backups').filter((n) => /\.sav$/.test(n)).length === 0 && window.Module.FS.readdir('/user').filter((n) => /^Career/.test(n)).length === 7),
        `${summary.trim()} | ${await pg.textContent('#saves-note')}`);
      // Two files that say they are 3 GB each (the stand-in's file system takes the length it is given): two parts.
      await pg.evaluate(() => { for (const n of [1, 2]) window.Module.FS.writeFile(`/user/Huge ${n}.sav`, { length: 3 * 2 ** 30 }); });
      const [first] = await Promise.all([pg.waitForEvent('download', { timeout: 20000 }), pg.click('#saves-all')]);
      await pg.waitForFunction(() => /part 2/.test(document.getElementById('saves-all').textContent), null, { timeout: 10000 }).catch(() => {});
      const label = await pg.textContent('#saves-all'), partNote = await pg.textContent('#saves-note');
      const [second] = await Promise.all([pg.waitForEvent('download', { timeout: 20000 }), pg.click('#saves-all')]);
      await pg.waitForFunction(() => document.getElementById('saves-all').textContent === 'Download all', null, { timeout: 10000 }).catch(() => {});
      check('Download all of more than a zip holds: a part at each press, the button says which is next',
        / \(1 of 2\)\.zip$/.test(first.suggestedFilename()) && label === 'Download part 2 of 2' && /A zip holds 4 GB: press Download part 2 of 2/.test(partNote)
          && / \(2 of 2\)\.zip$/.test(second.suggestedFilename()) && (await pg.textContent('#saves-all')) === 'Download all',
        `${first.suggestedFilename()} | ${label} | ${second.suggestedFilename()}`);
      check('no page errors with large saves', errs.length === 0, errs.join('; '));
      await ctx.close();
    }

    // A start that stops on an error before the game (a page and a script of two versions): the page loads again once and
    // starts; one that stops every time says so, instead of "Checking the files…" for good, and does not loop.
    for (const times of [1, Infinity]) {
      const ctx = await b.newContext({ viewport: { width: 1000, height: 700 }, serviceWorkers: 'block' });
      let broken = 0, loads = 0;
      await ctx.route(`http://127.0.0.1:${PORT}/files.json`, async (r) => {
        if (broken >= times) return r.continue();
        broken++;
        const j = await (await r.fetch()).json();
        return r.fulfill({ contentType: 'application/json', body: JSON.stringify({ ...j, missing: null }) });   // info.missing.length throws
      });
      const pg = await ctx.newPage();
      pg.on('load', () => { loads++; });
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      if (times === 1) {
        const started = await pg.waitForSelector('#go', { timeout: 30000 }).then(() => true, () => false);
        check('a start that stops on an error loads the page again once, and then starts', started && loads === 2 && broken === 1, `loads ${loads}, broken ${broken}`);
      } else {
        await pg.waitForSelector('.box.error', { timeout: 30000 }).catch(() => {});
        await pg.waitForTimeout(1500);
        const box = (await pg.textContent('#overlay').catch(() => '')).replace(/\s+/g, ' ');
        const logText = await pg.textContent('#log');
        check('a start that stops every time: one more load, then the error box with the message, and the log has it',
          loads === 2 && /The page did not start/.test(box) && /TypeError/.test(logText) && /the start stopped: TypeError: Cannot read properties of null/.test(logText),
          `loads ${loads} | ${box.slice(0, 160)} | ${logText.split('\n').filter((l) => /start stopped/.test(l)).join(' / ').slice(0, 200)}`);
      }
      await ctx.close();
    }

    // A tab the system closes while the game runs (iOS for its memory: "A problem repeatedly occurred", an iPhone, 17
    // November 2026): no pagehide, the page sees nothing. The next visit finds the mark and says so before its start;
    // a visit that closed as usual (a reload) leaves none.
    {
      const ctx = await b.newContext({ viewport: { width: 1000, height: 700 }, serviceWorkers: 'block' });
      let pg = await ctx.newPage();
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      await pg.waitForSelector('#go');
      await pg.click('#consent-no').catch(() => {});
      await pg.click('#go');
      await pg.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      const marked = await pg.evaluate(() => JSON.parse(localStorage.getItem('cm0102-visit') ?? 'null'));
      const cdp = await ctx.newCDPSession(pg);
      const crashed = pg.waitForEvent('crash', { timeout: 15000 }).then(() => true, () => false);
      cdp.send('Page.crash').catch(() => {});            // never answers: the page it would answer from is gone
      const didCrash = await crashed;
      await pg.close().catch(() => {});
      pg = await ctx.newPage();
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      await pg.waitForSelector('#go', { timeout: 30000 }).catch(() => {});
      const box = (await pg.textContent('#overlay').catch(() => '')).replace(/\s+/g, ' ');
      const logText = await pg.textContent('#log');
      check('a tab the system closed while the game ran: the next visit says so before its start, and the log has it',
        didCrash && marked?.step === 'in the game' && /The game stopped suddenly on your last visit \(\d+ min ago, while in the game\): the browser most likely closed the tab because it used too much memory/.test(box)
          && /the last visit stopped without the page closing, while in the game/.test(logText), `crashed ${didCrash} | ${JSON.stringify(marked)} | ${box.slice(0, 220)}`);
      if (await pg.locator('#go').count()) await pg.click('#go');
      await pg.waitForFunction(() => window.__fake, null, { timeout: 60000 }).catch(() => {});
      await pg.reload();
      await pg.waitForFunction(() => window.__fake || document.getElementById('go'), null, { timeout: 60000 }).catch(() => {});
      const again = `${await pg.textContent('#overlay').catch(() => '')} ${await pg.textContent('#log')}`;
      check('a visit that closed as usual (a reload) leaves no mark', !/stopped suddenly|stopped without the page closing/.test(again), again.slice(0, 200));
      await ctx.close();
    }

    // Older copies of saves that are gone (saves.js removeOrphanCopies): the next start removes them; a copy of a save
    // that is here, one in progress, and a copy the player named stay. Delete of a save takes its older copies with it.
    {
      const ctx = await b.newContext({ viewport: { width: 1100, height: 760 } });
      const pg = await ctx.newPage();
      pg.on('dialog', (d) => d.accept().catch(() => {}));
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      await pg.waitForSelector('#go');
      await pg.click('#go');
      await pg.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      await pg.evaluate(() => {
        const FS = window.Module.FS, s = FS.readFile('/user/Harbour City 2001-07-14.sav');
        FS.mkdirTree('/user/backups');
        for (const n of ['Harbour City 2001-07-14.1.sav', 'Harbour City 2001-07-14 (before edit).sav', 'Gone 2001-06-30.1.sav',
          'Gone 2001-06-30.2.sav', 'Gone 2001-06-30 (before edit).sav', 'my own copy.sav']) FS.writeFile(`/user/backups/${n}`, s);
      });
      await pg.waitForTimeout(4000);                    // the folder watcher stores them
      await pg.reload();
      await pg.waitForFunction(() => window.__fake || document.getElementById('go'), null, { timeout: 60000 });
      if (await pg.locator('#go').isVisible().catch(() => false)) await pg.click('#go');
      await pg.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      const left = await pg.evaluate(() => window.Module.FS.readdir('/user/backups').filter((n) => /\.sav$/.test(n)).sort());
      const logText = await pg.textContent('#log');
      check('the next start removes the older copies of a save that is gone, and keeps the others',
        JSON.stringify(left) === JSON.stringify(['Harbour City 2001-07-14 (before edit).sav', 'Harbour City 2001-07-14.1.sav', 'my own copy.sav'])
          && /removed 3 older copies of saves that are gone/.test(logText), JSON.stringify(left));
      await pg.click('#saves-open');
      await pg.waitForSelector('#saves-list button[data-delete="save"]');
      const i = await pg.$$eval('#saves-list button[data-delete="save"]', (bs) => bs.findIndex((x) => x.closest('tr').textContent.includes('Harbour City 2001-07-14.sav')));
      const asked = [];
      pg.removeAllListeners('dialog');
      pg.on('dialog', (d) => { asked.push(d.message()); d.accept().catch(() => {}); });
      await pg.locator('#saves-list button[data-delete="save"]').nth(i).click();
      await pg.waitForFunction(() => /is deleted/.test(document.getElementById('saves-note').textContent), null, { timeout: 5000 }).catch(() => {});
      const after = await pg.evaluate(() => window.Module.FS.readdir('/user/backups').filter((n) => /\.sav$/.test(n)));
      check('Delete of a save says it takes its older copies, and takes them; the player\'s own copy stays',
        /with its older copies \(2\)\?/.test(asked[0] ?? '') && JSON.stringify(after) === JSON.stringify(['my own copy.sav']), `${asked[0]} | ${JSON.stringify(after)}`);
      await ctx.close();
    }

    // A flight: one start with a connection, then none. The service worker gives the page, files.json and the engine,
    // the browser's cache the game files, and the saves are there (checked with the real engine too, 14 November 2026:
    // the start took 4 s offline).
    {
      const ctx = await b.newContext({ viewport: { width: 1000, height: 700 } });
      let pg = await ctx.newPage();
      await pg.goto(`http://127.0.0.1:${PORT}/`);
      await pg.waitForSelector('#go');
      await pg.click('#go');
      await pg.waitForFunction(() => window.__fake, null, { timeout: 120000 });
      await pg.waitForFunction(async () => Boolean((await navigator.serviceWorker.getRegistration())?.active), null, { timeout: 20000 }).catch(() => {});
      await pg.waitForTimeout(4000);                    // primeOffline: the page's files, files.json and the engine
      // The page from the kept copy, the server up again for the rest (an iPhone, 17 November 2026: the page from the
      // copy while the Fly machine woke, then a newer loader.js from the server, which stopped on a missing element).
      // The copy's loader.js is marked; with the page from the copy, the loader must be the copy's too.
      await pg.evaluate(async () => {
        const c = await caches.open('cm0102-shell');
        const key = (await c.keys()).find((k) => new URL(k.url).pathname === '/loader.js');
        const text = await (await c.match(key)).text();
        await c.put(key, new Response(`window.__fromCopy = true;\n${text}`, { headers: { 'content-type': 'text/javascript' } }));
      });
      await pg.close();
      const home = `http://127.0.0.1:${PORT}/`;
      await ctx.route(home, (r) => r.abort('connectionrefused'));
      const mixed = await ctx.newPage();
      await mixed.goto(home).catch(() => {});
      await mixed.waitForSelector('#go, .box.error', { timeout: 30000 }).catch(() => {});
      const copyLoader = await mixed.evaluate(() => window.__fromCopy === true).catch(() => false);
      await mixed.close();
      await ctx.unroute(home);
      const fresh = await ctx.newPage();
      await fresh.goto(home);
      await fresh.waitForSelector('#go', { timeout: 30000 }).catch(() => {});
      const serverLoader = await fresh.evaluate(() => window.__fromCopy !== true);
      await fresh.close();
      check('a page from the service worker\'s copy gets the copy\'s scripts too, one version; a page from the server, the server\'s',
        copyLoader && serverLoader, `copy page ran the copy's loader: ${copyLoader}; server page ran the server's: ${serverLoader}`);
      await ctx.setOffline(true);
      pg = await ctx.newPage();
      const errs = [];
      pg.on('pageerror', (e) => errs.push(e.message));
      await pg.goto(`http://127.0.0.1:${PORT}/`).catch((e) => errs.push(e.message));
      const startedOffline = await pg.waitForFunction(() => window.__fake, null, { timeout: 60000 }).then(() => true, () => false);
      const fromBrowser = startedOffline ? ((await pg.textContent('#log')).match(/page: \d+ files in .*/)?.[0] ?? '') : (await pg.textContent('#overlay').catch(() => '')).replace(/\s+/g, ' ');
      check('without a connection (a flight), after one start with one: the game starts from the browser, every file from it',
        startedOffline && /0\.0 MB downloaded/.test(fromBrowser) && /Harbour City/.test(await pg.evaluate(() => window.Module.FS.readdir('/user').join(','))), `${fromBrowser.slice(0, 140)} ${errs.join('; ')}`);
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
    // A tab that waits for an older page and is then asked to hand over: it stays handed over (it starts nothing and its
    // box keeps saying the game moved) when the older page lets go.
    {
      const ctx = await b.newContext({ viewport: { width: 1000, height: 700 } });
      const old = await ctx.newPage(), one = await ctx.newPage(), two = await ctx.newPage();
      await old.goto(`http://127.0.0.1:${PORT}/privacy`);
      await old.evaluate(() => new Promise((ok) => {
        const r = indexedDB.open('cm0102-vault', 1);
        r.onupgradeneeded = () => r.result.createObjectStore('files');
        r.onsuccess = () => { window.__old = r.result; ok(); };
      }));
      await one.goto(`http://127.0.0.1:${PORT}/`);
      await one.waitForSelector('#go');
      await one.click('#go');
      await one.waitForFunction(() => /An older page of the game/.test(document.getElementById('overlay').textContent), null, { timeout: 60000 }).catch(() => {});
      await two.goto(`http://127.0.0.1:${PORT}/`);
      await two.waitForSelector('#turn-take', { timeout: 30000 }).catch(() => {});
      await two.click('#turn-take');
      await one.waitForFunction(() => /The game moved to another tab/.test(document.getElementById('overlay').textContent), null, { timeout: 30000 }).catch(() => {});
      await old.evaluate(() => window.__old.close());
      await two.waitForFunction(() => window.__fake, null, { timeout: 60000 }).catch(() => {});
      await one.waitForTimeout(3000);
      const oneBox = (await one.textContent('#overlay')).replace(/\s+/g, ' ').trim();
      const oneStarted = await one.evaluate(() => Boolean(window.__fake));
      check('a tab that handed over while it waited for an older page stays handed over when that page lets go',
        /The game moved to another tab/.test(oneBox) && !oneStarted && await two.evaluate(() => Boolean(window.__fake)), oneBox.slice(0, 120));
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
