// The browser check of the Scout (scout.js, editor.js; web/README.md, "The Scout"): a saved game of ten players in
// the Editor, its Scout tab for a free player (3 rows, the rest held back and not in the page, the invitation), a
// filter, a result opened (and Back on a phone), a member (every row, offline too), signed out (Sign in), and no
// Scout tab on a data set. /api/me is played by Playwright routes.
// Needs Playwright: NODE_PATH=$(npm root -g) node web/test/scout-check.mjs [screenshot folder]
import { createRequire } from 'node:module';
import { copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { tmpdir } from 'node:os';
import { join, dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createApp } from '../server.mjs';
import { SaveGame } from '../savedit.js';
import { fakeDataSet, fakeSaveGame } from './fakes.mjs';

const require = createRequire(import.meta.url);
const { chromium, devices } = require('playwright');
const HERE = dirname(fileURLToPath(import.meta.url));
const SHOTS = process.argv[2] ? resolve(process.argv[2]) : null;
const shot = async (page, name) => { if (SHOTS) { mkdirSync(SHOTS, { recursive: true }); await page.screenshot({ path: join(SHOTS, `scout-${name}.png`) }); } };

// The site, as the editor check makes it: the game's database as a small data set, the stand-in engine.
const tmp = mkdtempSync(join(tmpdir(), 'cm0102-scout-check-'));
const game = join(tmp, 'game'), build = join(tmp, 'build-web');
mkdirSync(join(game, 'Data'), { recursive: true });
mkdirSync(join(build, 'seasons'), { recursive: true });
const exe = Buffer.from('MZ the game');
writeFileSync(join(game, 'cm0102.exe'), exe);
const sha = (b) => createHash('sha256').update(b).digest('hex');
let manifest = `${sha(exe)}\t${exe.length}\tcm0102.exe\n`;
for (const [name, bytes] of Object.entries(fakeDataSet())) {
  writeFileSync(join(game, 'Data', name), bytes);
  manifest += `${sha(bytes)}\t${bytes.length}\tData/${name}\n`;
}
writeFileSync(join(tmp, 'manifest.tsv'), manifest);
for (const t of ['rules', 'names', 'clubs', 'club_records']) writeFileSync(join(build, 'seasons', `${t}.tsv`), 'x\n');
copyFileSync(join(HERE, 'fake-emscripten.js'), join(build, 'cm0102_recomp.js'));
writeFileSync(join(build, 'cm0102_recomp.wasm'), Buffer.from([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0]));
writeFileSync(join(build, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=asyncify\n');
const server = createApp({ gameDir: game, buildDir: build, manifestPath: join(tmp, 'manifest.tsv') });
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const BASE = `http://127.0.0.1:${server.address().port}`;

let ok = true;
const check = (what, cond, detail = '') => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}${detail ? `: ${detail}` : ''}`); ok &&= Boolean(cond); };

// Ten players with names found nowhere else, PA 190 down to 181 (the order the Scout shows), Finishing apart.
const FIRST = ['Aurelio', 'Benedikt', 'Cassiano', 'Dorotheus', 'Eustachio', 'Florentin', 'Gaspard', 'Hilarion', 'Isidoro', 'Jovanni'];
const players = FIRST.map((first, i) => ({ first, second: 'Scoutson', ca: 120 + i, pa: 190 - i, gk: 1, born: [1981, 1, 1], club: 0, fill: 6 + i }));
const saveBytes = await fakeSaveGame(players);
const g = new SaveGame(saveBytes);
const finishing = g.players().map((p) => g.read(p.row).attrs.Finishing);
const MIN = [...finishing].sort((a, b) => b - a)[4];                       // five players reach it
const fifteenPlus = finishing.filter((v) => v >= MIN).length;
const NAMES = FIRST.map((f) => `${f} Scoutson`);

const ME = {
  free: { auth: true, user: { email: 'free@example.com', cloud: false }, offer: { price: '$5 a month', free: false, coffee: true, url: 'https://example.com/coffee' }, maxSaves: 30 },
  member: { auth: true, user: { email: 'member@example.com', cloud: true }, offer: { price: '$5 a month', free: false, coffee: true, url: 'https://example.com/coffee' }, maxSaves: 30 },
  out: { auth: true, user: null, offer: { price: '$5 a month', free: false, coffee: true, url: 'https://example.com/coffee' }, maxSaves: 30 },
};
const browser = await chromium.launch();
const errors = [];

// A page with /api/me answered as `me` (null: the request fails), the save in the user folder, the Editor on it.
async function editorOn(ctx, me) {
  await ctx.unroute('**/api/me').catch(() => {});
  await ctx.route('**/api/me', (r) => (me ? r.fulfill({ contentType: 'application/json', body: JSON.stringify(me) }) : r.abort()));
  // A member's cloud: nothing there (the sync finds no saves).
  await ctx.route('**/api/saves**', (r) => r.fulfill({ contentType: 'application/json', body: JSON.stringify({ saves: [], used: 0 }) }));
  const p = await ctx.newPage();
  p.on('pageerror', (e) => errors.push(e.message));
  p.on('dialog', (d) => d.accept());
  await p.goto(BASE);
  // The start box shows while game files are to be fetched; a later visit of the same browser starts at once.
  await p.waitForFunction(() => document.getElementById('go')?.offsetParent || window.__fake, null, { timeout: 30000 });
  if (await p.locator('#go').isVisible().catch(() => false)) await p.click('#go');
  await p.waitForFunction(() => window.__fake, null, { timeout: 30000 });
  await p.evaluate((b) => window.Module.FS.writeFile('/user/Scouting 2001-07-29.sav', new Uint8Array(b)), Array.from(saveBytes));
  await p.click('#saves-open');
  await p.click('#saves-list tr:has-text("Scouting 2001-07-29.sav") button[data-edit]');
  await p.waitForSelector('#editor[open] .ed-row');
  return p;
}
// Scout chosen at once, while the save is still opening.
async function scoutWhileOpening(ctx, me) {
  const p = await editorOn(ctx, me);
  await p.click('#editor-close');
  await p.click('#saves-open');
  await p.click('#saves-list tr:has-text("Scouting 2001-07-29.sav") button[data-edit]');
  await p.click('#editor-tabs [data-pane="scout"]');
  return p;
}
const scoutTab = async (p) => { await p.click('#editor-tabs [data-pane="scout"]'); await p.waitForSelector('#scout-results .ed-row'); };
const rowNames = (p) => p.$$eval('#scout-results .ed-row b', (b) => b.map((x) => x.textContent));
const pageHtml = (p) => p.evaluate(() => document.documentElement.outerHTML);

try {
  {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p = await editorOn(ctx, ME.free);
    check('a saved game in the Editor: tabs Players and Scout', (await p.$$eval('#editor-tabs [data-pane]', (b) => b.map((x) => x.dataset.pane))).join() === 'players,scout');
    await scoutTab(p);
    const shown = await rowNames(p), html = await pageHtml(p);
    check('free: the first 3 by potential, in full', shown.join() === NAMES.slice(0, 3).join(), shown.join());
    check('free: the rest held back, counted, with the invitation', /7 more players match/.test(await p.textContent('#scout-results'))
      && (await p.$$('#scout-results .scout-ghost')).length > 0 && (await p.textContent('#scout-invite')).trim() === 'Become a member', await p.textContent('#scout-results'));
    check('free: no held-back player is anywhere in the page', NAMES.slice(3).every((n) => !html.includes(n.split(' ')[0])), NAMES.slice(3).filter((n) => html.includes(n.split(' ')[0])).join());
    check('the Players list is empty while the Scout shows', (await p.$$('#editor-results .ed-row')).length === 0);
    await p.selectOption('#scout-filters [data-attr="0"]', 'Finishing');
    await p.fill('#scout-filters [data-min="0"]', String(MIN));
    await p.waitForFunction((n) => new RegExp(`${n} more player`).test(document.getElementById('scout-results').textContent), fifteenPlus - 3, { timeout: 5000 }).catch(() => {});
    await shot(p, 'free-laptop');
    check(`a filter (Finishing ${MIN} or more) narrows the matches to ${fifteenPlus}`, new RegExp(`${fifteenPlus - 3} more player`).test(await p.textContent('#scout-results')), await p.textContent('#scout-results'));
    await p.click('#scout-results .ed-row');
    await p.waitForSelector('#editor-form [data-f="Pace"]');
    check('a result opens the player\'s form', (await p.textContent('#editor-form h3')).includes('Scoutson'));
    await p.click('#scout-invite');
    check('the invitation opens the Account dialog', await p.isVisible('#account[open]'));
    // The player joins while the Scout is open: the account changes (the sign-in window's message), the Scout shows all.
    await ctx.unroute('**/api/me');
    await ctx.route('**/api/me', (r) => r.fulfill({ contentType: 'application/json', body: JSON.stringify(ME.member) }));
    await p.evaluate(() => new BroadcastChannel('cm-account').postMessage({ ok: true }));
    await p.waitForFunction(() => document.querySelectorAll('#scout-results .ed-row').length > 3, null, { timeout: 8000 }).catch(() => {});
    check('the account becomes a member while the Scout is open: it shows every match at once, no invitation',
      (await rowNames(p)).length === fifteenPlus && !(await p.$('#scout-invite')), (await rowNames(p)).join());
    await ctx.close();
  }
  {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p = await scoutWhileOpening(ctx, ME.free);
    await p.waitForSelector('#scout-results .ed-row', { timeout: 10000 }).catch(() => {});
    const html = await pageHtml(p);
    check('Scout chosen while the save opens: its matches show, and the hidden Players list stays empty',
      (await rowNames(p)).length === 3 && (await p.$$('#editor-results .ed-row')).length === 0 && NAMES.slice(3).every((n) => !html.includes(n.split(' ')[0])),
      `${(await rowNames(p)).length} matches, ${(await p.$$('#editor-results .ed-row')).length} in the Players list`);
    // A search typed in Players, then Scout before its pause ends: the Players list stays empty.
    await p.click('#editor-tabs [data-pane="players"]');
    await p.fill('#editor-search', 'scoutson');
    await p.click('#editor-tabs [data-pane="scout"]');
    await p.waitForTimeout(400);
    check('a search typed just before choosing Scout does not fill the hidden Players list', (await p.$$('#editor-results .ed-row')).length === 0);
    await ctx.close();
  }
  {
    const ctx = await browser.newContext({ ...devices['iPhone 13'] });
    const p = await editorOn(ctx, ME.free);
    await scoutTab(p);
    await p.selectOption('#scout-filters [data-attr="0"]', 'Finishing');
    await p.fill('#scout-filters [data-min="0"]', String(MIN));
    await p.waitForTimeout(600);
    await shot(p, 'free-phone');
    await p.click('#scout-results .ed-row');
    await p.waitForSelector('#editor-form [data-f="Pace"]');
    await p.click('.ed-back');
    await p.waitForSelector('#scout-results .ed-row');
    check('phone: Back returns to the results, the filter kept', (await p.inputValue('#scout-filters [data-attr="0"]')) === 'Finishing'
      && (await p.inputValue('#scout-filters [data-min="0"]')) === String(MIN) && await p.isVisible('#scout-results'));
    await ctx.close();
  }
  {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    let p = await editorOn(ctx, ME.member);
    await scoutTab(p);
    await shot(p, 'member-laptop');
    check('a member: every match, no invitation', (await rowNames(p)).length === 10 && !(await p.$('#scout-invite')));
    await p.close();
    p = await editorOn(ctx, null);
    await scoutTab(p);
    check('a member offline (/api/me fails): every match still (the kept answer)', (await rowNames(p)).length === 10);
    await ctx.close();
  }
  {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p = await editorOn(ctx, ME.out);
    await scoutTab(p);
    check('signed out: the invitation\'s button is Sign in', (await p.textContent('#scout-invite')).trim() === 'Sign in');
    await p.click('#editor-home-button');
    await p.fill('#editor-new-name', 'Scout set');
    await p.click('#editor-new');
    await p.waitForSelector('#editor .ed-row', { timeout: 30000 });
    check('a data set: no Scout tab', !(await p.isVisible('#editor-tabs')));
    await ctx.close();
  }
  check('no page errors', errors.length === 0, errors.join(' | '));
} finally {
  await browser.close();
  server.close();
  rmSync(tmp, { recursive: true, force: true });
}
console.log(ok ? 'scout check: all ok' : 'scout check: FAILED');
process.exit(ok ? 0 : 1);
