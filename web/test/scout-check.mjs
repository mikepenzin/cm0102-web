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
const players = FIRST.map((first, i) => ({ first, second: 'Scoutson', ca: 120 + i, pa: 190 - i, gk: 1, born: [i < 2 ? 1983 : 1981, 1, 1], club: i === 2 ? -1 : 0, fill: 6 + i }));   // the first two are 18: Wonderkids
// Two human managers, Klaus and Mike, with empty Short Lists (shortlist.js).
const saveBytes = await fakeSaveGame(players, { managers: [{ first: 'Klaus', second: 'Kastelmann' }, { first: 'Mike', second: 'Penzin' }] });
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
    check('a player with no club: his row says No club', /Cassiano Scoutson[\s\S]*No club/.test(await p.textContent('#scout-results')));
    // The tabs by the keys: arrows move between them, as tabs do.
    await p.focus('#editor-tabs [data-pane="scout"]');
    await p.keyboard.press('ArrowLeft');
    const left = await p.evaluate(() => ({ sel: document.querySelector('#editor-tabs [aria-selected="true"]')?.dataset.pane, focus: document.activeElement?.dataset?.pane }));
    await p.keyboard.press('ArrowRight');
    const right = await p.evaluate(() => ({ sel: document.querySelector('#editor-tabs [aria-selected="true"]')?.dataset.pane, focus: document.activeElement?.dataset?.pane,
      panel: document.getElementById(document.activeElement?.getAttribute('aria-controls') ?? '')?.getAttribute('role') }));
    check('the tabs: arrow keys choose and focus them; a tab names its panel', left.sel === 'players' && left.focus === 'players' && right.sel === 'scout' && right.focus === 'scout' && right.panel === 'tabpanel',
      JSON.stringify({ left, right }));
    await p.waitForSelector('#scout-results .ed-row');
    // A filter typed key by key searches once, after the pause.
    await p.evaluate(() => { window.__redraws = 0; new MutationObserver(() => { window.__redraws++; }).observe(document.getElementById('scout-results'), { childList: true }); });
    await p.selectOption('#scout-filters [data-attr="1"]', 'Pace');
    await p.waitForTimeout(500);
    await p.evaluate(() => { window.__redraws = 0; });
    await p.type('#scout-filters [data-min="1"]', '12', { delay: 60 });
    await p.waitForTimeout(500);
    const redraws = await p.evaluate(() => window.__redraws);
    check('a filter typed key by key searches once, after a pause', redraws === 1, `${redraws} redraws`);
    await p.fill('#scout-filters [data-min="1"]', '');
    await p.selectOption('#scout-filters [data-attr="1"]', '');
    await p.waitForTimeout(500);
    await p.selectOption('#scout-filters [data-attr="0"]', 'Finishing');
    await p.fill('#scout-filters [data-min="0"]', String(MIN));
    await p.waitForFunction((n) => new RegExp(`${n} more player`).test(document.getElementById('scout-results').textContent), fifteenPlus - 3, { timeout: 5000 }).catch(() => {});
    await shot(p, 'free-laptop');
    {
      const shownNow = await rowNames(p), htmlNow = await pageHtml(p);
      const out = NAMES.filter((n) => !shownNow.includes(n));
      check('after a filter: no player but the shown ones is anywhere in the page', out.every((n) => !htmlNow.includes(n.split(' ')[0])), out.filter((n) => htmlNow.includes(n.split(' ')[0])).join());
    }
    check(`a filter (Finishing ${MIN} or more) narrows the matches to ${fifteenPlus}`, new RegExp(`${fifteenPlus - 3} more player`).test(await p.textContent('#scout-results')), await p.textContent('#scout-results'));
    await p.click('#scout-results .ed-row');
    await p.waitForSelector('#editor-form [data-f="Pace"]');
    check('a result opens the player\'s form', (await p.textContent('#editor-form h3')).includes('Scoutson'));
    // Apply in the Scout: the player read again in place, not the whole save (no "Reading the players").
    await p.evaluate(() => { window.__reread = false; new MutationObserver(() => { if (/Reading the players/.test(document.getElementById('scout-results').textContent)) window.__reread = true; })
      .observe(document.getElementById('scout-results'), { childList: true, subtree: true }); });
    await p.fill('[data-f="pa"]', '199');
    await p.press('[data-f="pa"]', 'Tab');
    await p.waitForTimeout(400);
    const after = await rowNames(p);
    check('Apply in the Scout: the matches follow at once, the save not read again', !(await p.evaluate(() => window.__reread)) && after.length > 0,
      `${after.join()} reread=${await p.evaluate(() => window.__reread)}`);
    // A language chosen with a player open: the Scout's filters in it too.
    const ageLabel = () => p.evaluate(() => document.querySelector('#scout-filters [data-k="ageMin"]').closest('.pair').previousElementSibling.textContent);
    const ageBefore = await ageLabel();   // the label of the age fields, wherever the rows above it are
    await p.evaluate(async () => { const m = await import('/i18n.js'); await m.setLanguage('de', { keep: false }); });
    await p.waitForTimeout(300);
    const ageAfter = [await ageLabel(), await p.evaluate(async () => (await import('/i18n.js')).t('Age'))];
    check('a language chosen with a player open: the Scout\'s filters change too', ageBefore === 'Age' && ageAfter[0] === ageAfter[1] && ageAfter[0] !== 'Age', JSON.stringify(ageAfter));
    await p.evaluate(async () => { const m = await import('/i18n.js'); await m.setLanguage('en', { keep: false }); });
    await p.click('#scout-invite');
    check('the invitation opens the Account dialog', await p.isVisible('#account[open]'));
    // The player joins while the Scout is open: the account changes (the sign-in window's message), the Scout shows all.
    await ctx.unroute('**/api/me');
    await ctx.route('**/api/me', (r) => r.fulfill({ contentType: 'application/json', body: JSON.stringify(ME.member) }));
    await p.evaluate(() => new BroadcastChannel('cm-account').postMessage({ ok: true }));
    await p.waitForFunction(() => document.querySelectorAll('#scout-results .ed-row').length > 3, null, { timeout: 8000 }).catch(() => {});
    check('the account becomes a member while the Scout is open: it shows every match at once, no invitation',
      (await rowNames(p)).length === fifteenPlus && !(await p.$('#scout-invite')), (await rowNames(p)).join());
    // The Wonderkids preset: 19 or younger, potential 180 or more, the other filters cleared, and it searches at once.
    if (await p.isVisible('#account[open]')) await p.click('#account-close');          // the invitation's dialog, from above
    await p.click('#scout-filters [data-preset="wonderkids"]');
    await p.waitForFunction(() => document.querySelectorAll('#scout-results .ed-row').length === 2, null, { timeout: 5000 }).catch(() => {});
    const preset = await p.evaluate(() => ({ age: document.querySelector('#scout-filters [data-k="ageMax"]').value,
      pa: document.querySelector('#scout-filters [data-k="paMin"]').value, attr: document.querySelector('#scout-filters [data-attr="0"]').value }));
    check('Wonderkids: 19 or younger with potential 180+, the other filters cleared, its two players',
      preset.age === '19' && preset.pa === '180' && preset.attr === '' && (await rowNames(p)).join() === NAMES.slice(0, 2).join(), JSON.stringify({ preset, rows: await rowNames(p) }));
    await shot(p, 'wonderkids');
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
  {
    // A first visit offline: the account not known, nothing kept. 3 matches, the count, and no button (the Account
    // dialog would be empty).
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p = await editorOn(ctx, null);
    await scoutTab(p);
    check('the account not known, nothing kept: 3 matches, the count, no button', (await rowNames(p)).length === 3
      && /7 more players match/.test(await p.textContent('#scout-results')) && !(await p.$('#scout-invite')));
    await ctx.close();
  }
  // ---- Scout to shortlist (shortlist.js): a member puts a found player on a manager's Short List. ----
  {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p = await editorOn(ctx, ME.member);
    await scoutTab(p);
    await p.waitForSelector('#scout-filters [data-sl-manager]', { timeout: 10000 }).catch(() => {});
    const options = await p.$$eval('#scout-filters [data-sl-manager] option', (o) => o.map((x) => x.textContent));
    check('two managers: "Shortlist for" names them both', options.join() === 'Klaus Kastelmann,Mike Penzin', options.join());
    const star = (i) => p.locator('#scout-results .scout-item').nth(i).locator('.sl-star');
    await star(0).click();
    check('a star puts the player on Klaus\'s shortlist, a change to save', await star(0).getAttribute('aria-pressed') === 'true'
      && /\(1 changed\)/.test(await p.textContent('#editor-save')), `${await star(0).getAttribute('aria-pressed')} ${await p.textContent('#editor-save')}`);
    await p.selectOption('#scout-filters [data-sl-manager]', { label: 'Mike Penzin' });
    await p.waitForTimeout(300);
    check('Mike\'s shortlist is his own: the star is off', await star(0).getAttribute('aria-pressed') === 'false');
    await star(1).click();
    await star(1).click();
    check('a second click takes him off again: no change left for Mike', await star(1).getAttribute('aria-pressed') === 'false'
      && /\(1 changed\)/.test(await p.textContent('#editor-save')), await p.textContent('#editor-save'));
    // The player's panel says it too.
    await p.locator('#scout-results .scout-item').nth(0).locator('.ed-row').click();
    await p.waitForSelector('#editor-shortlist:not([hidden])');
    check('the panel, for Mike: + Shortlist', (await p.textContent('#editor-shortlist')).trim() === '+ Shortlist', await p.textContent('#editor-shortlist'));
    // Saved, then read from the file: it holds Klaus's list.
    await p.click('#editor-save');
    await p.waitForFunction(() => /Saved/.test(document.getElementById('editor-note').textContent), null, { timeout: 15000 });
    const bytes = await p.evaluate(() => Array.from(window.Module.FS.readFile('/user/Scouting 2001-07-29.sav')));
    const { scoutManager } = await import('../shortlist.js');
    const lists = scoutManager(new SaveGame(new Uint8Array(bytes)));
    const first = g.players().find((x) => x.name === NAMES[0]);
    check('saved: the file has him on Klaus\'s Short List, none on Mike\'s', [...lists.ids(lists.managers[0].id)].join() === String(first.row)
      && lists.ids(lists.managers[1].id).size === 0, JSON.stringify([[...lists.ids(lists.managers[0].id)], [...lists.ids(lists.managers[1].id)]]));
    await shot(p, 'shortlist');
    await ctx.close();
  }
  {
    // A free player: the star invites to Membership. A member while the feature is early and he is no Club Legend:
    // the locked line, no stars.
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p = await editorOn(ctx, ME.free);
    await scoutTab(p);
    await p.locator('#scout-results .scout-item').nth(0).locator('.sl-star').click();
    check('free: the star opens the Account dialog (Membership), nothing changes', await p.isVisible('#account[open]')
      && !/changed/.test(await p.textContent('#editor-save')));
    await ctx.close();
    const ctx2 = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const day = new Date(Date.now() + 5 * 86400e3).toISOString().slice(0, 10);
    const p2 = await editorOn(ctx2, { ...ME.member, early: { legend: false, features: { shortlist: day } } });
    await scoutTab(p2);
    await p2.waitForSelector('#scout-results .early-locked', { timeout: 10000 }).catch(() => {});
    check('early access: a member who is no Club Legend sees the locked line and no stars', await p2.locator('#scout-results .early-locked').count() === 1
      && await p2.locator('#scout-results .sl-star').count() === 0 && await p2.locator('#scout-filters [data-sl-manager]').count() === 0);
    await ctx2.close();
  }
  check('no page errors', errors.length === 0, errors.join(' | '));
} finally {
  await browser.close();
  server.close();
  rmSync(tmp, { recursive: true, force: true });
}
console.log(ok ? 'scout check: all ok' : 'scout check: FAILED');
process.exit(ok ? 0 : 1);
