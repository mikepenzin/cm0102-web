// The check of the page against the real SDL browser backend: web/server.mjs and the page of web/, with the SDL probe
// (sdlprobe.c, built by build-probe.sh) in place of the game. The probe opens its window and renderer as the runtime
// does and prints what SDL tells it, so this check sees what SDL makes of the page's canvas and events: the layout,
// the mouse, the keys, touch, the on-screen keyboard, full screen, and a start without a connection.
//
//   NODE_PATH=$(npm root -g) node web/test/sdl-check.mjs <probe out folder> [folder for screenshots]
//
// Needs Playwright and the built probe (cm0102_recomp.js and .wasm in the out folder).
import { createRequire } from 'node:module';
import { copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { createApp } from '../server.mjs';

const require = createRequire(`${process.env.NODE_PATH ?? ''}/`);
const { chromium } = require('playwright');
const probe = resolve(process.argv[2] ?? '');
const shots = process.argv[3] ? resolve(process.argv[3]) : null;
const PORT = 8095, URL = `http://127.0.0.1:${PORT}/`;

let ok = true;
const check = (what, cond, detail = '') => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}${detail ? `: ${detail}` : ''}`); ok &&= Boolean(cond); };
const near = (a, b, tol = 2) => Math.abs(a - b) <= tol;

// A small game folder and a build folder with the probe as the engine.
const tmp = mkdtempSync(join(tmpdir(), 'cm0102-sdl-check-'));
mkdirSync(join(tmp, 'game', 'Data'), { recursive: true });
mkdirSync(join(tmp, 'build', 'seasons'), { recursive: true });
writeFileSync(join(tmp, 'game', 'cm0102.exe'), 'MZ');
writeFileSync(join(tmp, 'game', 'Data', 'a.dat'), 'x');
writeFileSync(join(tmp, 'manifest.tsv'), '# sha256\tsize\tpath\nx\t2\tcm0102.exe\ny\t1\tData/a.dat\n');
for (const t of ['rules', 'names', 'clubs', 'club_records']) writeFileSync(join(tmp, 'build', 'seasons', `${t}.tsv`), 'x\n');
for (const ext of ['js', 'wasm']) copyFileSync(join(probe, `cm0102_recomp.${ext}`), join(tmp, 'build', `cm0102_recomp.${ext}`));
writeFileSync(join(tmp, 'build', 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=asyncify\n');
const server = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv') });
await new Promise((r) => server.listen(PORT, '127.0.0.1', r));

// What the probe printed, as objects.
const events = async (p) => (await p.evaluate(() => document.getElementById('log').textContent)).split('\n')
  .filter((l) => l.startsWith('{"probe"')).map((l) => { try { return JSON.parse(l); } catch { return { probe: 'bad', l }; } });
const since = async (p, n) => (await events(p)).slice(n);
const count = async (p) => (await events(p)).length;
const box = (p) => p.evaluate(() => { const r = document.getElementById('canvas').getBoundingClientRect(); return { x: r.left, y: r.top, w: r.width, h: r.height }; });
const stageBox = (p) => p.evaluate(() => { const r = document.getElementById('stage').getBoundingClientRect(); return { x: r.left, y: r.top, w: r.width, h: r.height }; });
// A point of the picture (1280x800) as a point of the page.
const at = async (p, px, py, pw = 1280, ph = 800) => { const b = await box(p); return { x: b.x + px / pw * b.w, y: b.y + py / ph * b.h }; };
const lastSize = async (p) => (await events(p)).filter((e) => e.probe === 'size').at(-1);
const settle = (p, ms = 350) => p.waitForTimeout(ms);
// The picture fills the canvas: SDL's viewport is the whole picture. At a scale that a float does not hold exactly
// SDL reports one less (it cuts 1279.9999 to 1279); that is under a pixel of the canvas, and black in the runtime.
const fills = (sz, w = 1280, h = 800) => sz.viewport[0] === 0 && sz.viewport[1] === 0 && w - sz.viewport[2] <= 1 && sz.viewport[2] <= w && h - sz.viewport[3] <= 1 && sz.viewport[3] <= h;

async function open(context, { expectStart = true } = {}) {
  const p = await context.newPage();
  p.errors = [];
  p.on('pageerror', (e) => p.errors.push(e.message));
  p.on('dialog', (d) => { p.dialogs = (p.dialogs ?? 0) + 1; d.accept().catch(() => {}); });
  await p.goto(URL);
  if (expectStart) { await p.waitForSelector('#go, .box.error'); if (await p.locator('#go').count()) await p.click('#go'); }
  await p.waitForFunction(() => /"probe":"ready"/.test(document.getElementById('log').textContent), null, { timeout: 30000 });
  await settle(p, 600);
  return p;
}

const browser = await chromium.launch();
try {
  // ---- A computer: 1470x748 at twice the pixels, a mouse and a keyboard. ----
  const desk = await browser.newContext({ viewport: { width: 1470, height: 748 }, deviceScaleFactor: 2 });
  let p = await open(desk);
  let b = await box(p), st = await stageBox(p), sz = await lastSize(p);
  check('the canvas has the picture\'s shape and fills the stage', b.w * 5 === b.h * 8 && (st.w - b.w < 8 || st.h - b.h < 5), `${b.w}x${b.h} in ${st.w}x${st.h}`);
  check('SDL\'s window is the canvas box, its pixels twice that', sz.window[0] === b.w && sz.window[1] === b.h && sz.output[0] === 2 * b.w && sz.output[1] === 2 * b.h, JSON.stringify(sz));
  check('the picture fills the canvas: no bar', fills(sz), JSON.stringify(sz.viewport));
  check('no Keyboard button and no chip on a computer', (await p.locator('#kbd-open:not([hidden])').count()) === 0 && (await p.locator('#type-chip:not([hidden])').count()) === 0);
  check('the status is empty while the game runs; the bar has Saved games, Help and Full screen',
    (await p.textContent('#status')) === '' && (await p.locator('#saves-open:not([hidden]), #help-open:not([hidden]), #fullscreen:not([hidden])').count()) === 3);
  if (shots) await p.screenshot({ path: join(shots, 'sdl-desktop.png') });

  let n = await count(p), pt = await at(p, 640, 400);
  await p.mouse.move(pt.x, pt.y);
  await p.mouse.down(); await p.waitForTimeout(60); await p.mouse.up();
  await settle(p);
  let ev = await since(p, n);
  let down = ev.find((e) => e.probe === 'button' && e.down === 1), up = ev.find((e) => e.probe === 'button' && e.down === 0);
  check('a click of the mouse lands at the same place of the picture', down && up && down.button === 1 && near(down.x, 640) && near(down.y, 400), JSON.stringify(down));
  n = await count(p);
  // The test browser sends half a notch for 100 at twice the pixels; SDL adds the halves up and tells whole steps.
  await p.mouse.wheel(0, 100);
  await p.mouse.wheel(0, 100);
  await settle(p, 700);
  ev = (await since(p, n)).filter((e) => e.probe === 'wheel');
  check('the wheel turns: two half notches down are one step down', ev.reduce((a, e) => a + e.y, 0) === -1, JSON.stringify(ev));

  n = await count(p);
  await p.keyboard.press('a');
  await settle(p);
  ev = await since(p, n);
  check('a key goes to the game', ev.some((e) => e.probe === 'keydown' && e.sym === 97) && ev.some((e) => e.probe === 'text' && e.text === 'a'));
  n = await count(p);
  await p.click('#help-open');
  await p.waitForSelector('#help[open]');
  await p.keyboard.press('b');
  await p.keyboard.press('Escape');
  await settle(p);
  ev = await since(p, n);
  check('keys typed while a dialog is open stay with the page', !(await p.locator('#help[open]').count()) && !ev.some((e) => e.probe === 'keydown' || e.probe === 'text'), JSON.stringify(ev.filter((e) => e.probe !== 'move')));
  n = await count(p);
  await p.keyboard.press('c');
  await settle(p);
  check('after the dialog the keys are the game\'s again', (await since(p, n)).some((e) => e.probe === 'keydown' && e.sym === 99));
  n = await count(p);
  await p.click('#saves-open');
  await p.click('#saves-close');
  await p.waitForFunction(() => document.activeElement?.id === 'canvas');   // the dialog's close event gives the keys back
  await p.keyboard.press('d');
  await settle(p);
  check('after a button of the bar the keys are the game\'s again', (await since(p, n)).some((e) => e.probe === 'keydown' && e.sym === 100));

  // The window of the browser changes its size.
  await p.setViewportSize({ width: 1000, height: 700 });
  await settle(p, 700);
  b = await box(p); sz = await lastSize(p);
  check('a smaller window: the canvas and SDL follow, still no bar', b.w * 5 === b.h * 8 && sz.window[0] === b.w && sz.output[0] === 2 * b.w && fills(sz), `${b.w}x${b.h} ${JSON.stringify(sz)}`);
  await p.setViewportSize({ width: 700, height: 900 });
  await settle(p, 700);
  b = await box(p); st = await stageBox(p); sz = await lastSize(p);
  check('a tall window: the picture takes the width and sits in the middle', b.w * 5 === b.h * 8 && st.w - b.w < 8 && near(b.y - st.y, (st.h - b.h) / 2, 1) && sz.window[0] === b.w, `${b.w}x${b.h} at ${b.y}`);
  check('on a narrow bar the buttons show their icons only', (await p.locator('#saves-open span').evaluate((e) => getComputedStyle(e).display)) === 'none');
  await p.setViewportSize({ width: 1470, height: 748 });
  await settle(p, 700);

  // Classic mode: the engine says its picture is 800x600.
  await p.evaluate(() => window.Module._probe_set_picture(800, 600));
  await settle(p, 900);
  b = await box(p); sz = await lastSize(p);
  check('a picture of 800x600: the canvas takes that shape, no bar', b.w * 3 === b.h * 4 && sz.window[0] === b.w && fills(sz, 800, 600), `${b.w}x${b.h} ${JSON.stringify(sz)}`);
  n = await count(p); pt = await at(p, 200, 500, 800, 600);
  await p.mouse.click(pt.x, pt.y, { delay: 60 });
  await settle(p);
  down = (await since(p, n)).find((e) => e.probe === 'button' && e.down === 1);
  check('a click lands right in the 800x600 picture', down && near(down.x, 200) && near(down.y, 500), JSON.stringify(down));
  await p.evaluate(() => window.Module._probe_set_picture(1280, 800));
  await settle(p, 900);

  // Full screen.
  await p.click('#fullscreen');
  await settle(p, 900);
  const fs = await p.evaluate(() => ({ el: document.fullscreenElement?.id ?? null, cls: document.getElementById('app').classList.contains('fs'), bar: getComputedStyle(document.getElementById('bar')).position, label: document.getElementById('fullscreen').getAttribute('aria-label') }));
  b = await box(p); st = await stageBox(p); sz = await lastSize(p);
  check('Full screen: the app fills the screen and the bar floats over the corner', fs.el === 'app' && fs.cls && fs.bar === 'absolute' && fs.label === 'Leave full screen', JSON.stringify(fs));
  check('in full screen the canvas takes the freed height, and SDL follows', near(st.h, 748, 1) && b.w * 5 === b.h * 8 && sz.window[0] === b.w && sz.window[1] === b.h && fills(sz), `${b.w}x${b.h} ${JSON.stringify(sz)}`);
  if (shots) await p.screenshot({ path: join(shots, 'sdl-fullscreen.png') });
  await p.click('#fullscreen');
  await settle(p, 900);
  check('leaving full screen puts the bar back', await p.evaluate(() => !document.fullscreenElement && !document.getElementById('app').classList.contains('fs')));

  // A save: the message, and the question before the page is left.
  await p.evaluate(() => window.Module.FS.writeFile('/user/Probe 2001-07-14.sav', new Uint8Array(0x416ec + 8)));
  await p.waitForSelector('.toast', { timeout: 8000 }).catch(() => {});
  check('a new save shows "Saved in this browser"', (await p.locator('.toast').allTextContents()).includes('Saved in this browser'));
  // The busy sign: the page's thread is held for a while, as the game holds it in a long piece of work. Only the
  // browser's compositor draws then; a screencast takes its frames.
  const cast = await desk.newCDPSession(p);
  const frames = [];
  cast.on('Page.screencastFrame', (f) => { frames.push({ t: Date.now(), data: f.data }); cast.send('Page.screencastFrameAck', { sessionId: f.sessionId }).catch(() => {}); });
  const pill = await p.evaluate(() => { const r = document.getElementById('busy').getBoundingClientRect(); return { x: r.left, y: r.top, w: r.width, h: r.height, vw: innerWidth }; });
  await cast.send('Page.startScreencast', { format: 'png', everyNthFrame: 1 });
  await settle(p, 600);
  const heldAt = Date.now();
  p.evaluate(() => { const end = Date.now() + 3600; while (Date.now() < end); }).catch(() => {});
  await new Promise((r) => setTimeout(r, 3300));
  const free = frames.filter((f) => f.t <= heldAt).at(-1), held2 = frames.filter((f) => f.t > heldAt + 2300).at(-1);
  await settle(p, 900);                                       // the thread is free again
  await cast.send('Page.stopScreencast').catch(() => {});
  // How much the place of the sign differs between a frame with the thread free and one with it held.
  const differs = free && held2 ? await p.evaluate(async ({ a, b, pill }) => {
    const load = async (d) => createImageBitmap(await (await fetch(`data:image/png;base64,${d}`)).blob());
    const [ia, ib] = [await load(a), await load(b)];
    const k = ia.width / pill.vw, x = Math.round(pill.x * k), y = Math.round(pill.y * k), w = Math.round(pill.w * k), h = Math.round(pill.h * k);
    const px = (img) => { const c = new OffscreenCanvas(w, h), g = c.getContext('2d'); g.drawImage(img, x, y, w, h, 0, 0, w, h); return g.getImageData(0, 0, w, h).data; };
    const pa = px(ia), pb = px(ib);
    let sum = 0;
    for (let i = 0; i < pa.length; i += 4) sum += Math.abs(pa[i] - pb[i]) + Math.abs(pa[i + 1] - pb[i + 1]) + Math.abs(pa[i + 2] - pb[i + 2]);
    return sum / (pa.length / 4);
  }, { a: free.data, b: held2.data, pill }) : -1;
  check('while the page\'s thread is held for seconds, the Working sign shows; it is gone when the thread is free',
    differs > 12 && (await p.evaluate(() => getComputedStyle(document.getElementById('busy')).opacity)) === '0', `frames ${frames.length}, the sign's place differs by ${differs.toFixed?.(1) ?? differs} a pixel`);
  check('no page errors on the computer', p.errors.length === 0, p.errors.join('; '));

  // Without a connection: the page, the engine and the game files come from the browser.
  await p.waitForFunction(async () => (await (await caches.open('cm0102-shell')).keys()).length >= 14 && Boolean(navigator.serviceWorker.controller), null, { timeout: 15000 }).catch(() => {});
  const keptN = await p.evaluate(async () => (await (await caches.open('cm0102-shell')).keys()).map((r) => new URL(r.url).pathname));
  check('after a start the page and the engine are kept for a start without a connection',
    ['/', '/loader.js', '/layout.js', '/files.json', '/engine/cm0102_recomp.js', '/engine/cm0102_recomp.wasm'].every((u) => keptN.includes(u)), `${keptN.length} kept`);
  await desk.setOffline(true);
  p.dialogs = 0;
  await p.mouse.click(700, 400);                             // the browser asks before a reload only after a click on the page
  await p.reload().catch(() => {});
  const offline = await p.waitForFunction(() => /"probe":"ready"/.test(document.getElementById('log')?.textContent ?? ''), null, { timeout: 30000 }).then(() => true, () => false);
  check('the browser asked before the page with a running game was left', p.dialogs >= 1, `${p.dialogs} dialogs`);
  check('without a connection the page starts by itself and the engine runs', offline, offline ? '' : (await p.textContent('body').catch(() => '')).slice(0, 200));
  if (offline) {
    await settle(p, 600);
    sz = await lastSize(p);
    check('and the picture fills the canvas as before', fills(sz), JSON.stringify(sz));
  }
  await desk.setOffline(false);
  await desk.close();

  // ---- A touch device: 1180x820 at twice the pixels, no mouse. The on-screen keyboard is played by a stand-in for
  // the visual viewport, which the check makes smaller as the device would. ----
  const pad = await browser.newContext({ viewport: { width: 1180, height: 820 }, deviceScaleFactor: 2, hasTouch: true });
  await pad.addInitScript(() => {
    const vv = new EventTarget();
    Object.assign(vv, { width: innerWidth, height: innerHeight, offsetTop: 0, offsetLeft: 0, scale: 1 });
    Object.defineProperty(window, 'visualViewport', { value: vv, configurable: true });
    window.__keyboard = (px) => { vv.height = innerHeight - px; vv.dispatchEvent(new Event('resize')); };
  });
  p = await open(pad);
  const cdp = await pad.newCDPSession(p);
  const touch = (type, points) => cdp.send('Input.dispatchTouchEvent', { type, touchPoints: points.map((q, i) => ({ x: q.x, y: q.y, id: q.id ?? i })) });
  check('a touch device has the Keyboard button; the start box told of the gestures', (await p.locator('#kbd-open:not([hidden])').count()) === 1);
  sz = await lastSize(p); b = await box(p);
  check('on the touch device the picture fills the canvas too', b.w * 5 === b.h * 8 && sz.window[0] === b.w && fills(sz), `${b.w}x${b.h}`);

  n = await count(p); pt = await at(p, 320, 200);
  await touch('touchStart', [pt]); await p.waitForTimeout(50); await touch('touchEnd', []);
  await settle(p);
  ev = (await since(p, n)).filter((e) => e.probe !== 'move');
  check('a tap is one click of the left button at that place, made by the page (SDL\'s own touch handling never ran)',
    ev.length === 2 && ev[0].probe === 'button' && ev[0].down === 1 && ev[0].button === 1 && ev[0].touch === 0 && near(ev[0].x, 320) && near(ev[0].y, 200) && ev[1].down === 0, JSON.stringify(ev));
  n = await count(p); pt = await at(p, 900, 600);
  await touch('touchStart', [pt]); await p.waitForTimeout(750); await touch('touchEnd', []);
  await settle(p);
  ev = (await since(p, n)).filter((e) => e.probe !== 'move');
  check('a long press is one click of the right button', ev.length === 2 && ev[0].button === 3 && ev[0].down === 1 && near(ev[0].x, 900) && ev[1].button === 3 && ev[1].down === 0, JSON.stringify(ev));
  n = await count(p);
  const from = await at(p, 400, 300), to = await at(p, 400, 500);
  await touch('touchStart', [from]);
  for (let i = 1; i <= 5; i++) { await touch('touchMove', [{ x: from.x, y: from.y + (to.y - from.y) * i / 5 }]); await p.waitForTimeout(20); }
  await touch('touchEnd', []);
  await settle(p);
  ev = await since(p, n);
  const held = ev.filter((e) => e.probe === 'move' && e.held === 1), buttons = ev.filter((e) => e.probe === 'button');
  check('a drag holds the left button from its start to its end', buttons.length === 2 && buttons[0].down === 1 && near(buttons[0].y, 300) && buttons[1].down === 0 && held.length >= 3 && near(held.at(-1).y, 500, 3), JSON.stringify(buttons));
  n = await count(p);
  const f1 = await at(p, 500, 300), f2 = await at(p, 600, 300);
  await touch('touchStart', [f1]); await touch('touchStart', [f1, f2]);
  for (let i = 1; i <= 6; i++) { await touch('touchMove', [{ x: f1.x, y: f1.y + 20 * i }, { x: f2.x, y: f2.y + 20 * i }]); await p.waitForTimeout(20); }
  await touch('touchEnd', [f1].map((q) => ({ ...q, y: q.y + 120 }))); await touch('touchEnd', []);
  await settle(p);
  ev = (await since(p, n)).filter((e) => e.probe !== 'move');
  check('two fingers dragged down turn the wheel up, and click nothing', ev.length === 3 && ev.every((e) => e.probe === 'wheel' && e.y === 1), JSON.stringify(ev));
  n = await count(p);
  await touch('touchStart', [f1]); await touch('touchStart', [f1, f2]); await p.waitForTimeout(60); await touch('touchEnd', [f1]); await touch('touchEnd', []);
  await settle(p);
  ev = (await since(p, n)).filter((e) => e.probe !== 'move');
  check('a tap with two fingers is one click of the right button', ev.length === 2 && ev[0].button === 3 && ev[1].button === 3, JSON.stringify(ev));
  check('SDL saw no finger at all', !(await events(p)).some((e) => e.probe === 'finger'));

  // The on-screen keyboard and a text entry of the game.
  await p.evaluate(() => window.Module._probe_set_typing(640));
  await p.waitForSelector('#type-chip:not([hidden])', { timeout: 3000 }).catch(() => {});
  let chip = await p.locator('#type-chip').boundingBox(); pt = await at(p, 0, 640); b = await box(p);
  check('a text entry takes keys: the chip shows at its row, at the right edge of the picture', chip && near(chip.y + chip.height / 2, pt.y, 2) && near(chip.x + chip.width, b.x + b.w - 12, 2), JSON.stringify(chip));
  check('and a message says how to get the keyboard', (await p.locator('.toast').allTextContents()).some((t) => /Tap to type/.test(t)));
  if (shots) await p.screenshot({ path: join(shots, 'sdl-touch-chip.png') });
  await p.tap('#type-chip');
  await settle(p, 200);
  check('a tap on the chip opens the keyboard: the hidden field has the focus, the chip is gone', (await p.evaluate(() => document.activeElement?.id)) === 'kbd' && (await p.locator('#type-chip:not([hidden])').count()) === 0);
  const before = await box(p);
  await p.evaluate(() => window.__keyboard(400));            // the device's keyboard covers the lower 400 pixels
  await settle(p, 500);
  b = await box(p);
  const rowY = b.y + 640 / 800 * b.h, visibleBottom = 820 - 400;
  check('the keyboard covers the entry\'s row: the picture keeps its size and moves the row into view', b.w === before.w && b.h === before.h && rowY > 60 && rowY < visibleBottom - 30, `row at ${rowY.toFixed(0)}, visible to ${visibleBottom}`);
  check('SDL\'s window did not change for that', (await lastSize(p)).window[0] === before.w);
  if (shots) await p.screenshot({ path: join(shots, 'sdl-touch-keyboard.png') });
  n = await count(p);
  await p.keyboard.type('Ab');
  await p.keyboard.press('Backspace');
  await p.keyboard.insertText('Smith');
  await p.keyboard.press('Enter');
  await settle(p);
  ev = (await since(p, n)).filter((e) => e.probe !== 'move');
  check('what is typed reaches the engine while it sleeps, in order; none of it as SDL keys',
    JSON.stringify(ev) === JSON.stringify([{ probe: 'type', text: 'A' }, { probe: 'type', text: 'b' }, { probe: 'key', vk: 8 }, { probe: 'type', text: 'Smith' }, { probe: 'key', vk: 13 }]), JSON.stringify(ev));
  n = await count(p); pt = await at(p, 100, 560);            // a place of the picture that is in view above the keyboard
  await touch('touchStart', [pt]); await p.waitForTimeout(40); await touch('touchEnd', []);
  await settle(p);
  ev = (await since(p, n)).filter((e) => e.probe === 'button');
  check('a tap on the game with the keyboard open clicks there and keeps the keyboard', ev.some((e) => near(e.x, 100) && near(e.y, 560)) && (await p.evaluate(() => document.activeElement?.id)) === 'kbd',
    `${JSON.stringify(ev)} focus ${await p.evaluate(() => document.activeElement?.id)} at ${pt.x.toFixed(0)},${pt.y.toFixed(0)}`);
  await p.evaluate(() => window.Module._probe_set_typing(-1));
  await settle(p, 600);
  await p.evaluate(() => window.__keyboard(0));
  await settle(p, 400);
  b = await box(p);
  check('the entry is done: the keyboard closes and the picture is back in its place', (await p.evaluate(() => document.activeElement?.id)) !== 'kbd' && b.y === before.y, `top ${b.y}, was ${before.y}`);

  await p.evaluate(() => window.Module._probe_set_typing(200));
  await settle(p, 500);
  pt = await at(p, 300, 205);
  await touch('touchStart', [pt]); await p.waitForTimeout(40); await touch('touchEnd', []);
  await settle(p, 200);
  check('a tap on the text entry itself opens the keyboard', (await p.evaluate(() => document.activeElement?.id)) === 'kbd');
  await p.evaluate(() => window.__keyboard(400));
  await settle(p, 400);
  b = await box(p);
  check('an entry near the top stays where it is under the keyboard', b.y === before.y, `top ${b.y}`);
  await p.evaluate(() => { window.Module._probe_set_typing(-1); window.__keyboard(0); });
  await settle(p, 600);

  // The keyboard opened by its button with no text entry: the picture shrinks into the part that is left.
  await p.tap('#kbd-open');
  await p.evaluate(() => window.__keyboard(400));
  await settle(p, 700);
  b = await box(p); st = await stageBox(p); sz = await lastSize(p);
  check('the Keyboard button with no text entry: the whole picture fits above the keyboard', b.w * 5 === b.h * 8 && b.y + b.h <= 820 - 400 + 1 && sz.window[0] === b.w && fills(sz), `${b.w}x${b.h} at ${b.y} ${JSON.stringify(sz)}`);
  await p.tap('#kbd-open');
  await p.evaluate(() => window.__keyboard(0));
  await settle(p, 700);
  b = await box(p);
  check('Hide keyboard: the picture is at its full size again', b.w === before.w && (await p.textContent('#kbd-open span')) === 'Keyboard', `${b.w}x${b.h}`);
  check('no page errors on the touch device', p.errors.length === 0, p.errors.join('; '));
  await pad.close();
} finally {
  await browser.close();
  server.close();
  rmSync(tmp, { recursive: true, force: true });
}
process.exit(ok ? 0 : 1);
