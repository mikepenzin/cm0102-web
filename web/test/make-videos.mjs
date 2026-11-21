// Two how-to videos of the page (web/README.md, "How-to videos"): how to edit a saved game (and a data set), and how
// to use the app around the game. A player asked where the editor is (18 November 2026). The page is recorded in
// Chromium on a real site (the real engine and the game's files), with a pointer drawn on it, so the game's own screens
// are in the films. The waits (the game files' download, the game starting, a save loading) are cut out. Each step's
// words and the box it points at go into a log, and videos-compose.mjs makes the film of it with HyperFrames: a title
// card, the capture in a frame with a caption under it for each step and a zoom on what the step points at, an end card.
// CM_VIDEO_SAVE=<a .sav of a Juventus game> NODE_PATH=$(npm root -g) node web/test/make-videos.mjs <folder> [editor|app]
//   → <folder>/editor.mp4 and <folder>/app.mp4 (and <folder>/<name>-project/, the HyperFrames project, to edit or
//   render again: node .claude/skills/hyperframes/scripts/plugin-cli.mjs render, in it)
// CM_VIDEO_SITE: the site to record (default https://cm0102-web.fly.dev; or a local server with the game's files,
// http://127.0.0.1:8080). Each film starts in a new browser profile, so it downloads the game files once (cut out).
// The page's scripts (web/*.js) come from this checkout, so a change to the page shows before it is deployed.
// CM_VIDEO_SAVE: a saved game of the real game, Juventus in 2001/02 (the editor film edits Del Piero in it); a new game
// with Italy and Juventus, saved on its first day, is enough. The music is the film's take of "Matchday Manager"
// (test/video-music/); CM_VIDEO_MUSIC (or CM_VIDEO_MUSIC_APP, CM_VIDEO_MUSIC_EDITOR) names another file, or none.
// Other music on a film already made: node web/test/videos-music.mjs <folder> [editor|app].
import { createRequire } from 'node:module';
import { execFileSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, readFileSync, readdirSync, renameSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { basename, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { composeVideo, musicOf, render } from './videos-compose.mjs';

const require = createRequire(import.meta.url);
const { chromium } = require('playwright');
const OUT = resolve(process.argv[2] ?? '.');
const BASE = (process.env.CM_VIDEO_SITE || 'https://cm0102-web.fly.dev').replace(/\/+$/, '');
const SITE = new URL(BASE).host;
if (!process.env.CM_VIDEO_SAVE) throw new Error('CM_VIDEO_SAVE: a .sav of the real game (Juventus, 2001/02)');
const SAVE = { name: basename(process.env.CM_VIDEO_SAVE), buffer: readFileSync(process.env.CM_VIDEO_SAVE), mimeType: 'application/octet-stream' };
mkdirSync(OUT, { recursive: true });
const tmp = mkdtempSync(join(tmpdir(), 'cm0102-videos-'));
const WEB = resolve(fileURLToPath(import.meta.url), '../..');
const PAGE_JS = new Set(readdirSync(WEB).filter((f) => f.endsWith('.js') && f !== 'sw.js'));

// Places on the game's own screen, in its pixels (the picture is 1280x800, scaled into the canvas's box).
const GAME = { w: 1280, h: 800 };
const AT = {
  ok: [1060, 698],            // Game Settings (the first start): Ok; on the main menu, an empty place
  options: [71, 459],         // in a game: Game Options (Save Game is its first line)
  find: [72, 400],            // in a game: Find
  findPlayer: [176, 444],     // Find's list: Player
  found: [267, 274],          // the first player the search found
  physical: [1060, 336],      // a player's Profile: Pace, in the Physical column
};

// On the page: the pointer (with a ring at each click). It is a popover, raised above each dialog the page opens
// (dialogs sit in the top layer).
const OVERLAY = String.raw`(() => {
  const css = '#vid-ptr{position:fixed;inset:auto;margin:0;padding:0;border:0;background:transparent;overflow:visible;pointer-events:none;color:inherit}'
    + '#vid-ptr{left:0;top:0;width:30px;height:30px}'
    + '#vid-ptr .ring{position:absolute;left:-16px;top:-16px;width:34px;height:34px;border-radius:50%;border:3px solid #35b673;opacity:0}'
    + '#vid-ptr.down .ring{animation:vring .55s ease-out}@keyframes vring{from{transform:scale(.35);opacity:1}to{transform:scale(1.5);opacity:0}}';
  const raise = () => { const el = document.getElementById('vid-ptr'); if (!el) return; try { if (el.matches(':popover-open')) el.hidePopover(); el.showPopover(); } catch {} };
  addEventListener('DOMContentLoaded', () => {
    const style = document.createElement('style'); style.textContent = css; document.head.append(style);
    const ptr = document.createElement('div'); ptr.id = 'vid-ptr'; ptr.popover = 'manual';
    ptr.innerHTML = '<div class="ring"></div><svg width="30" height="30" viewBox="0 0 24 24"><path d="M4 2l15 10.5-6.6 1.3 3.9 7.2-2.7 1.4-3.9-7.3L4 20z" fill="#fff" stroke="#111" stroke-width="1.4" stroke-linejoin="round"/></svg>';
    ptr.style.left = '640px'; ptr.style.top = '400px'; document.body.append(ptr);
    raise();
    new MutationObserver(raise).observe(document.body, { subtree: true, attributes: true, attributeFilter: ['open'] });
  });
  addEventListener('mousemove', (e) => { const p = document.getElementById('vid-ptr'); if (p) { p.style.left = e.clientX + 'px'; p.style.top = e.clientY + 'px'; } }, true);
  addEventListener('mousedown', () => { const p = document.getElementById('vid-ptr'); if (!p) return; p.classList.remove('down'); void p.offsetWidth; p.classList.add('down'); }, true);
})();`;

const browser = await chromium.launch();
let current = null;                                      // the film being recorded: a failure leaves its screen

// The parts of the capture that are cut out of the film ([start, end] on its clock), and a time on the film's clock.
function cutTime(cuts, t) {
  let gone = 0;
  for (const c of cuts) {
    if (t >= c.end) gone += c.end - c.start;
    else if (t > c.start) gone += t - c.start;
  }
  return Math.round((t - gone) * 1000) / 1000;
}

async function recorder(name) {
  const dir = join(tmp, `video-${name}`);
  // A new profile: no game files, no saves, the game's first start (its Game Settings).
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 }, acceptDownloads: true,
    serviceWorkers: 'block', recordVideo: { dir, size: { width: 1280, height: 800 } } });
  await ctx.addInitScript(OVERLAY);
  // The first-visit tour (tour.js) is not in the films: they show the buttons themselves.
  await ctx.addInitScript(() => { try { localStorage.setItem('cm0102-tour-seen', '1'); } catch { /* none */ } });
  // The page's own scripts from this checkout (the films show the page about to be released); the engine, the game
  // files and the server's answers from the site. The service worker is off: it would answer for the page.
  await ctx.route((url) => url.origin === new URL(BASE).origin && PAGE_JS.has(url.pathname.slice(1)), (route) =>
    route.fulfill({ contentType: 'text/javascript', body: readFileSync(join(WEB, new URL(route.request().url()).pathname.slice(1))) }));
  const p = await ctx.newPage();
  current = { name, p };
  const t0 = Date.now();                                 // the capture starts with the page
  const now = () => (Date.now() - t0) / 1000;
  p.on('dialog', (d) => d.accept());
  p.on('download', (d) => d.cancel().catch(() => {}));
  const pause = (ms) => p.waitForTimeout(ms);
  // A step: its words, shown for ms, in the log; the box pointed at just before it (point, not click) is its zoom.
  const captions = [];
  const cuts = [];
  let pointed = null;
  const say = async (text, sub = '', ms = 3000) => {
    const start = now();
    await pause(ms);
    captions.push({ text, sub, start, end: now(), focus: pointed });
    pointed = null;
  };
  const point = async (sel, { steps = 24, wait = 350 } = {}) => {
    const el = p.locator(sel).first();
    await el.scrollIntoViewIfNeeded();
    const b = await el.boundingBox();
    await p.mouse.move(b.x + b.width / 2, b.y + b.height / 2, { steps });
    await pause(wait);
    pointed = { x: b.x, y: b.y, w: b.width, h: b.height };
    return b;
  };
  const click = async (sel, opts) => { const b = await point(sel, opts); pointed = null; await p.mouse.click(b.x + b.width / 2, b.y + b.height / 2); await pause(500); };
  const type = async (sel, text) => {
    const b = await point(sel);
    await p.mouse.click(b.x + b.width / 2, b.y + b.height / 2, { clickCount: 3 });
    await p.keyboard.press('ControlOrMeta+A');
    await p.keyboard.type(text, { delay: 85 });
    await pause(400);
  };
  // The game's own screen: a place in its pixels → the page; a press held as a hand would (a quick one can be missed).
  const onGame = async (x, y) => {
    const b = await p.locator('#canvas').boundingBox();
    return { x: b.x + x * b.width / GAME.w, y: b.y + y * b.height / GAME.h, s: b.width / GAME.w };
  };
  const pointGame = async ([x, y], [w, h] = [0, 0], { steps = 24, wait = 350 } = {}) => {
    const c = await onGame(x, y);
    await p.mouse.move(c.x, c.y, { steps });
    await pause(wait);
    pointed = w ? { x: c.x - (w * c.s) / 2, y: c.y - (h * c.s) / 2, w: w * c.s, h: h * c.s } : null;
    return c;
  };
  const clickGame = async (at, opts) => {
    const c = await pointGame(at, undefined, opts);
    pointed = null;
    await p.mouse.down(); await pause(150); await p.mouse.up(); await pause(600);
    return c;
  };
  const typeGame = async (text) => {
    await p.evaluate(() => document.activeElement?.blur());          // a key goes to the game, not to a page's button
    for (const ch of text) { await p.keyboard.down(ch); await pause(70); await p.keyboard.up(ch); await pause(70); }
  };
  const keyGame = async (key) => { await p.evaluate(() => document.activeElement?.blur()); await p.keyboard.down(key); await pause(80); await p.keyboard.up(key); await pause(300); };
  // The game busy (its progress in the bar's status) until it has been quiet for a few seconds.
  const settle = async (min = 2000, quiet = 4) => {
    await pause(min);
    for (let n = 0, t = Date.now(); n < quiet && Date.now() - t < 600000; await pause(1000)) {
      const s = await p.evaluate(() => document.getElementById('status')?.textContent.trim() ?? '');
      n = !s || s === 'Ready' ? n + 1 : 0;
    }
  };
  // Work that is not in the film: what it shows is cut from the capture.
  const offscreen = async (fn) => {
    const start = now();
    await fn();
    cuts.push({ start, end: now() });
    pointed = null;
  };
  // After Start: the game files come down, the game starts; on its first start, its Game Settings: Ok. The press goes
  // on until the main menu is surely there (on the menu that place is empty).
  const boot = async () => {
    await p.waitForFunction(() => !/Loading the game files/.test(document.getElementById('overlay')?.innerText ?? ''), null, { timeout: 600000 });
    await settle(20000);
    for (let i = 0; i < 12; i++) { await clickGame(AT.ok, { steps: 2, wait: 100 }); await pause(4000); }
    await settle(1000);
  };
  const uploadSave = async () => {
    await click('#saves-open');
    const [chooser] = await Promise.all([p.waitForEvent('filechooser'), click('#saves-upload')]);
    await chooser.setFiles(SAVE);
    await p.waitForSelector(`#saves-list tr:has-text("${SAVE.name}")`, { timeout: 60000 });
  };
  const finish = async (file) => {
    const video = p.video();
    await ctx.close();
    const webm = await video.path();
    const capture = join(tmp, `${name}-capture.mp4`), mp4 = join(OUT, file), project = join(OUT, `${name}-project`);
    // The capture at 30 fps, without the cut parts; the steps' times on the cut clock.
    const keep = cuts.length ? `,select='not(${cuts.map((c) => `between(t,${c.start.toFixed(3)},${c.end.toFixed(3)})`).join('+')})',setpts=N/30/TB` : '';
    execFileSync('ffmpeg', ['-y', '-v', 'error', '-i', webm, '-vf', `fps=30${keep}`, '-c:v', 'libx264', '-preset', 'slow', '-crf', '14', '-r', '30', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', capture]);
    const steps = captions.map((c) => ({ ...c, start: cutTime(cuts, c.start), end: cutTime(cuts, c.end) }));
    rmSync(project, { recursive: true, force: true });
    const music = musicOf(name);
    const seconds = composeVideo({ dir: project, capture, log: { captions: steps }, music });
    writeFileSync(join(project, 'steps.json'), JSON.stringify({ steps, cuts }, null, 1));
    render(project, `${mp4}.part.mp4`);
    renameSync(`${mp4}.part.mp4`, mp4);
    console.log(`made ${mp4} (${Math.round(seconds)} s)`);
  };
  // The page before the film's first step: the statistics question answered (No thanks).
  const open = async () => {
    await p.goto(BASE);
    await p.waitForSelector('#go');
    await p.click('#consent-no', { timeout: 8000 }).catch(() => {});
  };
  return { p, pause, say, point, click, type, pointGame, clickGame, typeGame, keyGame, settle, offscreen, boot, uploadSave, open, finish };
}

// In a game: Find, Player, the name, the first player found; his Profile, pointed at his Pace.
async function findPlayer(v, name) {
  await v.clickGame(AT.find);
  await v.clickGame(AT.findPlayer);
  await v.typeGame(name);
  await v.keyGame('Enter');
  await v.settle(1500, 2);
  await v.clickGame(AT.found);
  await v.settle(1500, 2);
  await v.pointGame(AT.physical, [330, 60]);
}

// ---- How to edit a saved game (and a data set) ----
async function editorVideo() {
  const v = await recorder('editor');
  const { p } = v;
  await v.open();
  await v.say('How to edit a saved game', 'its players, or your own database for new games', 3800);
  await v.point('#go');
  await v.say('Start the game as usual', '', 2000);
  await v.click('#go');
  await v.offscreen(async () => { await v.boot(); await v.uploadSave(); await v.click('#saves-close'); });
  await v.point('#saves-open');
  await v.say('The editor is in Saved games', 'the button at the top of the page', 2600);
  await v.click('#saves-open');
  const row = `#saves-list tr:has-text("${SAVE.name}")`;
  await p.waitForSelector(`${row} button[data-edit]`);
  await v.say('Each saved game has an Edit button', '', 2200);
  await v.click(`${row} button[data-edit]`);
  await p.waitForSelector('#editor[open] .ed-row', { timeout: 120000 });
  await v.say('The editor lists the players of the save, best first', 'Search by a player\'s name or by a club', 3600);
  await v.type('#editor-search', 'del piero');
  await p.waitForFunction(() => /piero/i.test(document.querySelector('.ed-row')?.textContent ?? ''), null, { timeout: 60000 });
  await v.say('Pick the player', '', 1400);
  await v.click('.ed-row');
  await p.waitForSelector('#editor-form [data-f="Pace"]');
  await v.say('His ability, positions, attributes and hidden attributes', 'Attributes go from 1 to 20, ability from 1 to 200', 3800);
  await v.type('[data-f="ca"]', '190');
  await v.type('[data-f="Pace"]', '20');
  await v.say('A changed field is marked', 'change as many as you like', 2600);
  await v.click('#editor-apply');
  await v.say('Apply keeps the changes of this player', 'then pick another, or save', 3000);
  await v.point('#editor-save');
  await v.say('Save writes them into the saved game', 'The save as it was is kept too, as an older copy', 3400);
  await v.click('#editor-save');
  await p.waitForFunction(() => /^Saved:/.test(document.getElementById('editor-note').textContent), null, { timeout: 120000 });
  await v.pause(1200);
  await v.click('#editor-close');
  // The edited save in the game: Play, then Find.
  await v.click('#saves-open');
  await v.point(`${row} button[data-play]`);
  await v.say('Play it: the game loads the edited save', '', 2400);
  await v.click(`${row} button[data-play]`);
  await v.offscreen(() => v.settle(8000, 6));
  await v.pointGame(AT.find, [110, 50]);
  await v.say('In the game, Find the player', '', 2200);
  await findPlayer(v, 'Del Piero');
  await v.say('He has his new ratings', 'Pace 20, as set in the editor', 4200);
  // A data set: the Editor button.
  await v.point('#editor-open');
  await v.say('The Editor button edits data sets too', 'your own copy of the game\'s database, for new games', 3600);
  await v.click('#editor-open');
  await p.waitForSelector('#editor[open] #editor-new');
  await v.say('Make one from the game\'s own data', 'give it a name', 2400);
  await v.type('#editor-new-name', 'My database');
  await v.click('#editor-new');
  await p.waitForSelector('#editor .ed-row', { timeout: 180000 });
  await v.type('#editor-search', 'del piero');
  await p.waitForFunction(() => /piero/i.test(document.querySelector('.ed-row')?.textContent ?? ''), null, { timeout: 60000 });
  await v.click('.ed-row');
  await p.waitForSelector('#editor-form [data-f="club"]');
  await v.say('Move a player to another club, change his contract or ratings', '', 2800);
  await v.type('[data-f="club"]', 'AC Milan (Italy)');
  await v.click('#editor-apply');
  await v.point('#editor-add');
  await v.say('Add a player creates a new one', '', 2400);
  await v.click('#editor-save');
  await p.waitForFunction(() => /^Saved:/.test(document.getElementById('editor-note').textContent), null, { timeout: 180000 });
  await v.pause(800);
  await v.click('#editor-close');
  await v.click('#saves-open');
  await v.click('#saves .tabs [data-tab="sets"]');
  await p.waitForSelector('#sets-list [data-set-use]');
  await v.point('#sets-list [data-set-use]');
  await v.say('To play it: Saved games, Databases, Use', 'then start a new game: it uses your database', 4000);
  await v.click('#saves-close');
  await v.say('Have fun!', SITE, 3200);
  await v.finish('editor.mp4');
}

// ---- How to use the app ----
async function appVideo() {
  const v = await recorder('app');
  const { p } = v;
  await v.open();
  await v.say('How to play in your browser', 'the whole game, free, nothing to install', 3800);
  await v.point('#go');
  await v.say('The first visit downloads the game files into this browser', 'once; the next visits start at once', 3600);
  await v.click('#go');
  await v.pause(2500);
  await v.offscreen(() => v.boot());
  await v.pointGame([640, 420]);
  await v.say('The game runs in this tab', 'mouse and keys as in the original; on a touch screen, tap and long press', 3800);
  await v.point('#saves-open');
  await v.say('Your saved games stay in this browser', 'Saved games lists them', 2600);
  await v.click('#saves-open');
  await v.point('#saves-upload');
  await v.say('Upload brings a save in', 'a .sav file from the PC game, or a .zip of saves', 3000);
  const [chooser] = await Promise.all([p.waitForEvent('filechooser'), v.click('#saves-upload')]);
  await chooser.setFiles(SAVE);
  await p.waitForSelector(`#saves-list tr:has-text("${SAVE.name}")`, { timeout: 60000 });
  await v.say('It is in the list, ready to play', '', 2200);
  await v.point('#saves-list button[data-kind="save"]');
  await v.say('Download keeps a copy as a file', 'for the PC game, or to bring it to another device', 3200);
  await v.point('#saves-all');
  await v.say('Download all keeps a copy of everything', 'a browser may delete a site\'s data by itself: keep copies', 3800);
  await v.point('#saves-list button[data-play]');
  await v.say('Play loads a save at once', '', 2200);
  await v.click('#saves-list button[data-play]');
  await v.offscreen(() => v.settle(8000, 6));
  await v.say('Your game, where you left it', '', 3000);
  await v.pointGame(AT.options, [110, 50]);
  await v.say('Save in the game as usual: Game Options, Save Game', 'the save goes to Saved games', 3600);
  await v.click('#saves-open');
  await v.click('#saves .tabs [data-tab="tactics"]');
  await v.say('Tactics: the ones you saved in the game', 'download them, or upload a .tct from the PC game or a friend', 3800);
  await v.click('#saves .tabs [data-tab="sets"]');
  await p.waitForSelector('#sets-lib button[data-lib-get]', { timeout: 30000 });
  await v.say('Databases: play another season', 'free databases to download, or your own', 3600);
  await v.point('#sets-lib button[data-lib-get]');
  await v.say('Get one from the list', '', 2200);
  await v.click('#sets-lib button[data-lib-get]');
  await p.waitForSelector('#sets-season', { timeout: 180000 });
  await v.point('[data-set-go]');
  await v.say('Use it for your next new game', 'its season is chosen for you; the next new game is played with it', 4000);
  await v.click('[data-set-go]');
  await v.pause(1200);
  if (await p.isVisible('#saves[open]')) await v.click('#saves-close');
  if (await p.isVisible('#fullscreen')) {
    await v.point('#fullscreen');
    await v.say('Full screen gives the game the whole screen', '', 2600);
  }
  await v.click('#account-open');
  await p.waitForSelector('#account[open]');
  await v.say('Cloud Saves: your saves on every computer, iPad and phone', 'an optional membership; the game itself is free', 4000);
  await v.point('#account-signin');
  await v.say('Sign in with Google, then become a member', 'each save goes to the cloud, and comes to your other devices', 4000);
  await v.click('#account-close');
  await v.click('#help-open');
  await p.waitForSelector('#help[open]');
  await v.say('Help has the controls, the touch gestures and tips', '', 3200);
  if (await p.isVisible('#help-report-open')) {
    await v.point('#help-report-open');
    await v.say('Something wrong? Send to the developer', 'your note, the log and your save', 3600);
  }
  await v.click('#help-close').catch(() => p.keyboard.press('Escape'));
  await v.say('On an iPhone or iPad: Share, Add to Home Screen', 'it then opens like an app', 3800);
  await v.say('Have fun!', SITE, 3200);
  await v.finish('app.mp4');
}

try {
  const which = process.argv[3];
  if (!which || which === 'editor') await editorVideo();
  if (!which || which === 'app') await appVideo();
} catch (err) {
  if (current) await current.p.screenshot({ path: join(OUT, `${current.name}-failed.png`) }).catch(() => {});
  throw err;
} finally {
  await browser.close();
  rmSync(tmp, { recursive: true, force: true });
}
