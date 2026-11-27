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
const players = FIRST.map((first, i) => ({ first, second: 'Scoutson', ca: 120 + i, pa: 190 - i, gk: 1, born: [i < 2 ? 1983 : 1981, 1, 1], club: i === 2 ? -1 : 0, fill: 6 + i, nation: i === 3 || i === 7 ? 1 : 0 }));   // the first two are 18: Wonderkids; Dorotheus and Hilarion are French
// Two human managers, Klaus and Mike, with empty Short Lists (shortlist.js).
const saveBytes = await fakeSaveGame(players, { contracts: true, prefs: true, managers: [{ first: 'Klaus', second: 'Kastelmann' }, { first: 'Mike', second: 'Penzin' }],
  finance: { balance: 59516431, wageBill: 889495, topWage: 65497, thisYear: { 0x08: 19491130, 0x20: 33094379, 0x48: 5985625 }, lastYear: { 0x20: 51520409 } } });
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
    check('a saved game in the Editor: tabs Players, Scout, Managers and Clubs', (await p.$$eval('#editor-tabs [data-pane]', (b) => b.map((x) => x.dataset.pane))).join() === 'players,scout,managers,clubs');
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
  // ---- A player as the GK editor shows him (savedit.js readMore/writeMore): details, personality, contract ----
  {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p = await editorOn(ctx, ME.member);
    await p.click('#editor-tabs [data-pane="players"]');
    await p.fill('#editor-search', g.players()[1].name);
    await p.waitForSelector('#editor-results .ed-row', { timeout: 10000 });
    await p.locator('#editor-results .ed-row').first().click();
    await p.waitForSelector('#editor-form [data-f="c:wage"]', { timeout: 10000 }).catch(() => {});
    const shownName = (await p.textContent('#editor-player')).trim();
    const him = g.players().find((x) => x.name === shownName);
    check('a player: his contract, personality and details in the form', await p.inputValue('#editor-form [data-f="c:wage"]') === String(g.read(him.row).more.contract.wage)
      && await p.inputValue('#editor-form [data-f="p:Ambition"]') === String(g.read(him.row).more.personality.Ambition)
      && await p.inputValue('#editor-form [data-f="m:caps"]') === '5');
    const highColour = () => p.$eval('#editor-form .ed-attrs input[data-band="high"]', (i) => getComputedStyle(i).color);
    const green = await highColour();
    await p.selectOption('#ed-colours', 'red');
    check('a player: Red for the best paints the high values red, and stays chosen', await highColour() === 'rgb(255, 90, 79)' && green !== 'rgb(255, 90, 79)'
      && await p.evaluate(() => localStorage.getItem('cm0102-attribute-colours')) === 'red', `${green} -> ${await highColour()}`);
    await p.selectOption('#ed-colours', 'green');
    await p.selectOption('#editor-form [data-f="m:nation"]', '1');
    await p.fill('#editor-form [data-f="p:Ambition"]', '20');
    await p.locator('#editor-form [data-f="p:Ambition"]').dispatchEvent('change');
    await p.fill('#editor-form [data-f="c:wage"]', '75000');
    await p.locator('#editor-form [data-f="c:wage"]').dispatchEvent('change');
    await p.fill('#editor-form [data-f="pref:favClubs:1"]', 'ital');
    check('a player: a preference box offers the clubs that match', (await p.$$eval('#club-picks option', (o) => o.map((x) => x.value))).join() === 'Italy (Italy)');
    await p.fill('#editor-form [data-f="pref:favClubs:1"]', 'Italy (Italy)');
    await p.locator('#editor-form [data-f="pref:favClubs:1"]').dispatchEvent('change');
    await p.check('#editor-form [data-f="c:listed"]');
    await p.fill('#editor-form [data-f="c:goalBonus"]', '3000');
    await p.locator('#editor-form [data-f="c:goalBonus"]').dispatchEvent('change');
    check('a player: the changes count as one changed player', /\(1 changed\)/.test(await p.textContent('#editor-save')), await p.textContent('#editor-save'));
    await p.click('#editor-save');
    await p.waitForFunction(() => /Saved/.test(document.getElementById('editor-note').textContent), null, { timeout: 15000 });
    const saved = new SaveGame(new Uint8Array(await p.evaluate(() => Array.from(window.Module.FS.readFile('/user/Scouting 2001-07-29.sav')))));
    const m = saved.read(him.row).more;
    check('a player: saved, the file has his new nation, ambition, wage and listing', m.nation === 1 && m.personality.Ambition === 20
      && m.contract.wage === 75000 && m.contract.listed === true && m.contract.goalBonus === 3000 && m.prefs.favClubs.join() === '0,1,-1', JSON.stringify([m.nation, m.personality.Ambition, m.contract]));
    await ctx.close();
  }

  // ---- Clubs (savedit.js clubFinances): a club's finances, a member's ----
  {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p = await editorOn(ctx, ME.member);
    await p.click('#editor-tabs [data-pane="clubs"]');
    await p.fill('#clubs-search', 'juv');
    await p.waitForSelector('#clubs-results [data-club]', { timeout: 10000 }).catch(() => {});
    check('Clubs: a search finds the club', (await p.$$eval('#clubs-results [data-club]', (b) => b.map((x) => x.textContent.trim()))).join() === 'Juventus');
    await p.click('#clubs-results [data-club]');
    await p.waitForSelector('#editor-form [data-fin]', { timeout: 10000 }).catch(() => {});
    check('Clubs: the finances open in the right-hand panel, the club list stays', /Juventus/.test(await p.textContent('#editor-form h3'))
      && await p.locator('#clubs-results [data-club]').count() === 1);
    const fin = await p.$$eval('#editor-form [data-fin]', (v) => Object.fromEntries(v.map((x) => [x.dataset.fin, x.textContent.replace(/[^0-9-]/g, '')])));
    check('Clubs: the balance, the wage bill, the top wage, and this and last year\'s lines', fin.balance === '59516431' && fin.wageBill === '889495'
      && fin.topWage === '65497' && fin['thisYear.sold'] === '19491130' && fin['thisYear.wages'] === '33094379' && fin['lastYear.wages'] === '51520409', JSON.stringify(fin));
    // The club's record, and a change of the balance and the reputation saved with the game file.
    check('Clubs: the record: reputation and training facilities as the file has them', await p.inputValue('#editor-form [data-cf="reputation"]') === '9200'
      && await p.inputValue('#editor-form [data-cf="training"]') === '18' && await p.inputValue('#editor-form [data-cf="balance"]') === '59516431');
    for (const [k, v] of [['balance', '90000000'], ['reputation', '9900']]) {
      await p.fill(`#editor-form [data-cf="${k}"]`, v);
      await p.locator(`#editor-form [data-cf="${k}"]`).dispatchEvent('change');
    }
    await p.selectOption('#editor-form [data-cf="matchDay"]', '3');
    await p.selectOption('#editor-form [data-kit="awayBack"]', '0');
    await p.fill('#editor-form [data-cf="capacity"]', '70000');
    await p.locator('#editor-form [data-cf="capacity"]').dispatchEvent('change');
    await p.fill('#editor-form [data-pick="rivals:0"]', 'ital');
    check('Clubs: a rival\'s box offers the clubs that match', (await p.$$eval('#club-picks option', (o) => o.map((x) => x.value))).join() === 'Italy (Italy)',
      await p.$$eval('#club-picks option', (o) => o.map((x) => x.value).join()));
    await p.fill('#editor-form [data-pick="rivals:0"]', 'Italy (Italy)');
    await p.locator('#editor-form [data-pick="rivals:0"]').dispatchEvent('change');
    check('Clubs: the favourite shown by name, the stadium and its city', await p.inputValue('#editor-form [data-pick="favourites:0"]') === `${g.players().find((x) => x.row === 1).name} (Juventus)`
      && /Stadio delle Alpi/.test(await p.textContent('#editor-form')) && /Turin/.test(await p.textContent('#editor-form')), await p.inputValue('#editor-form [data-pick="favourites:0"]'));
    check('Clubs: the new balance in the finances, one change of the save', (await p.textContent('#editor-form [data-fin="balance"]')).replace(/[^0-9-]/g, '') === '90000000'
      && /\(1 changed\)/.test(await p.textContent('#editor-save')), await p.textContent('#editor-save'));
    await p.click('#editor-save');
    await p.waitForFunction(() => /Saved/.test(document.getElementById('editor-note').textContent), null, { timeout: 15000 });
    const saved = new SaveGame(new Uint8Array(await p.evaluate(() => Array.from(window.Module.FS.readFile('/user/Scouting 2001-07-29.sav')))));
    const { clubInfo, clubFinances } = await import('../savedit.js');
    const j = clubInfo(saved, 0);
    check('Clubs: saved, the file has the new balance, reputation, match day, kit, rival and capacity', clubFinances(saved, 0)?.balance === 90000000
      && j?.reputation === 9900 && j.matchDay === 3 && j.kit.awayBack === 0 && j.rivals[0] === 1 && j.stadium.capacity === 70000,
      JSON.stringify([clubFinances(saved, 0)?.balance, j?.reputation, j?.matchDay, j?.kit, j?.rivals, j?.stadium?.capacity]));
    await ctx.close();
    const c2 = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const free = await editorOn(c2, ME.free);
    await free.click('#editor-tabs [data-pane="clubs"]');
    await free.waitForTimeout(400);
    check('Clubs, a free player: no figures, the Membership invitation', await free.locator('#clubs-invite').count() === 1
      && await free.locator('#clubs-search').count() === 0);
    await c2.close();
  }
  // ---- Managers (savedit.js managerAttributes): each human manager's twelve attributes, a member's ----
  {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p = await editorOn(ctx, ME.member);
    await p.click('#editor-tabs [data-pane="managers"]');
    await p.waitForSelector('#editor-managers [data-mgr]', { timeout: 10000 }).catch(() => {});
    const names = await p.$$eval('#editor-managers [data-mgr] b', (h) => h.map((x) => x.textContent.trim()));
    check('Managers: a row for each human manager on the left', names.join() === 'Klaus Kastelmann,Mike Penzin', names.join());
    await p.locator('#editor-managers [data-mgr]').first().click();
    await p.waitForSelector('#editor-form [data-attr]', { timeout: 10000 }).catch(() => {});
    check('Managers: his attributes open in the right-hand panel', /Klaus Kastelmann/.test(await p.textContent('#editor-form h3')));
    const klaus = await p.$$eval('#editor-form [data-attr]', (v) => Object.fromEntries(v.map((x) => [x.dataset.attr, x.value])));
    check('Managers: the twelve attributes, Adaptability and Determination from the staff row, a relative one rated',
      Object.keys(klaus).length === 12 && klaus.Adaptability === '12' && klaus.Determination === '14' && klaus['Tactical Knowledge'] === '20', JSON.stringify(klaus));
    check('Managers: the search and the Players list are hidden', !(await p.isVisible('#editor-search')) && !(await p.isVisible('#editor-results')));
    // A search finds a manager; a change of an attribute is saved with the game file, as the game rates it.
    await p.selectOption('#managers-job', '5');
    await p.waitForTimeout(200);
    check('Staff: a job lists its staff best first, with club and job', (await p.$$eval('#managers-results [data-mgr] b', (b) => b.length)) === 2
      && /Manager$/.test((await p.textContent('#managers-results [data-mgr] span')).trim()), await p.textContent('#managers-results'));
    await p.selectOption('#managers-job', '');
    await p.fill('#managers-search', 'penzin');
    await p.waitForTimeout(300);
    check('Managers: a search finds any manager', (await p.$$eval('#managers-results [data-mgr] b', (b) => b.map((x) => x.textContent))).join() === 'Mike Penzin');
    await p.click('#managers-results [data-mgr]');
    await p.waitForSelector('#editor-form [data-attr="Motivating"]');
    check('Managers: his details and personality below the attributes, no player fields', await p.locator('#editor-form [data-f="m:nation"]').count() === 1
      && await p.locator('#editor-form [data-f="p:Ambition"]').count() === 1 && await p.locator('#editor-form [data-f="m:morale"]').count() === 0);
    await p.fill('#editor-form [data-attr="Motivating"]', '7');
    await p.locator('#editor-form [data-attr="Motivating"]').dispatchEvent('change');
    await p.fill('#editor-form [data-hid="Attacking"]', '18');
    await p.locator('#editor-form [data-hid="Attacking"]').dispatchEvent('change');
    await p.selectOption('#editor-form [data-mf="formation"]', '6');
    check('Managers: a change counts as a change of the save', /\(1 changed\)/.test(await p.textContent('#editor-save')), await p.textContent('#editor-save'));
    await p.click('#editor-save');
    await p.waitForFunction(() => /Saved/.test(document.getElementById('editor-note').textContent), null, { timeout: 15000 });
    const bytes = await p.evaluate(() => Array.from(window.Module.FS.readFile('/user/Scouting 2001-07-29.sav')));
    const { managerAttributes } = await import('../savedit.js');
    const saved = new SaveGame(new Uint8Array(bytes));
    check('Managers: saved, the file has the new Motivating', managerAttributes(saved, players.length + 2)?.attrs.find((x) => x.name === 'Motivating')?.value === 7);
    check('Managers: and his hidden Attacking and preferred formation', managerAttributes(saved, players.length + 2)?.hidden.find((x) => x.name === 'Attacking')?.value === 18
      && managerAttributes(saved, players.length + 2)?.formation === 6);
    await ctx.close();
    const c2 = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const free = await editorOn(c2, ME.free);
    await free.click('#editor-tabs [data-pane="managers"]');
    await free.waitForSelector('#editor-managers', { state: 'visible' });
    await free.waitForTimeout(400);
    check('Managers, a free player: no values, the Membership invitation', await free.locator('#editor-managers [data-attr]').count() === 0
      && await free.locator('#managers-invite').count() === 1);
    await c2.close();
    const c3 = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const day = new Date(Date.now() + 5 * 86400e3).toISOString().slice(0, 10);
    const early = await editorOn(c3, { ...ME.member, early: { legend: false, features: { 'manager-attributes': day } } });
    await early.click('#editor-tabs [data-pane="managers"]');
    await early.waitForTimeout(600);
    check('Managers, early for a member who is no Club Legend: the locked line, no values', await early.locator('#editor-managers .early-locked').count() === 1
      && await early.locator('#editor-managers [data-attr]').count() === 0);
    await c3.close();
  }
  // ---- Nationality (savedit.js nationNames, search; scout.js nation): in the Players list and in the Scout ----
  {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p = await editorOn(ctx, ME.member);
    await p.waitForSelector('#editor-nation:not([hidden])', { timeout: 10000 }).catch(() => {});
    const options = await p.$$eval('#editor-nation option', (o) => o.map((x) => x.textContent));
    check('Players: a Nationality list of the save\'s nations, Any nationality first', options.join() === 'Any nationality,France,Italy', options.join());
    await p.selectOption('#editor-nation', 'France');
    await p.waitForTimeout(400);
    const french = await p.$$eval('#editor-results .ed-row b', (b) => b.map((x) => x.textContent));
    check('Players: France lists only the French players', french.join() === 'Hilarion Scoutson,Dorotheus Scoutson' || french.join() === 'Dorotheus Scoutson,Hilarion Scoutson', french.join());
    await p.fill('#editor-search', 'Hilarion');
    await p.waitForTimeout(400);
    check('Players: a name and a nation together', (await p.$$eval('#editor-results .ed-row b', (b) => b.map((x) => x.textContent))).join() === 'Hilarion Scoutson');
    await scoutTab(p);
    await p.waitForSelector('#scout-filters [data-k="nation"]', { timeout: 10000 }).catch(() => {});
    await p.selectOption('#scout-filters [data-k="nation"]', 'France');
    await p.waitForTimeout(600);
    const scouted = await rowNames(p);
    check('Scout: Nationality France finds only the French players, best first', scouted.join() === 'Dorotheus Scoutson,Hilarion Scoutson', scouted.join());
    await ctx.close();
    const ctx2 = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const day = new Date(Date.now() + 5 * 86400e3).toISOString().slice(0, 10);
    const p2 = await editorOn(ctx2, { ...ME.member, early: { legend: false, features: { nationality: day } } });
    await p2.waitForTimeout(800);
    check('early access: no Nationality list for a member who is no Club Legend yet', !(await p2.isVisible('#editor-nation')));
    await scoutTab(p2);
    check('early access: no Nationality filter in the Scout either', await p2.locator('#scout-filters [data-k="nation"]').count() === 0);
    await ctx2.close();
  }
  // ---- Scout a player (shortlist.js, scout requests): the manager's club asks its scouts to watch him. ----
  {
    const ctx = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p = await editorOn(ctx, ME.member);
    await scoutTab(p);
    await p.waitForSelector('#scout-results .sr-eye', { timeout: 10000 }).catch(() => {});
    check('the manager\'s choice is labelled Manager', /Manager/.test(await p.textContent('#scout-filters')) && !/Shortlist for/.test(await p.textContent('#scout-filters')));
    const eye = (i) => p.locator('#scout-results .scout-item').nth(i).locator('.sr-eye');
    check('each row has a Scout button after the star', await p.locator('#scout-results .sr-eye').count() === await p.locator('#scout-results .sl-star').count()
      && await p.locator('#scout-results .sr-eye').count() > 0);
    await eye(0).click();
    check('Scout asks Klaus\'s club, a change to save', await eye(0).getAttribute('aria-pressed') === 'true'
      && /\(1 changed\)/.test(await p.textContent('#editor-save')) && /scouts? of|scouting/i.test(await p.textContent('#editor-note')), await p.textContent('#editor-note'));
    await p.selectOption('#scout-filters [data-sl-manager]', { label: 'Mike Penzin' });
    await p.waitForTimeout(300);
    check('Mike\'s club has not asked: the button is off', await eye(0).getAttribute('aria-pressed') === 'false');
    await eye(1).click();
    await eye(1).click();
    check('a second tap takes the request back: no change left for Mike', await eye(1).getAttribute('aria-pressed') === 'false'
      && /\(1 changed\)/.test(await p.textContent('#editor-save')), await p.textContent('#editor-save'));
    await p.locator('#scout-results .scout-item').nth(0).locator('.ed-row').click();
    await p.waitForSelector('#editor-scoutreq:not([hidden])');
    check('the panel, for Mike: + Scout', (await p.textContent('#editor-scoutreq')).trim() === '+ Scout', await p.textContent('#editor-scoutreq'));
    await p.click('#editor-scoutreq');
    check('the panel\'s button asks, and says so', (await p.textContent('#editor-scoutreq')).trim() === 'Scouting ✓' && await eye(0).getAttribute('aria-pressed') === 'true');
    await p.click('#editor-scoutreq');
    await p.click('#editor-save');
    await p.waitForFunction(() => /Saved/.test(document.getElementById('editor-note').textContent), null, { timeout: 15000 });
    const bytes = await p.evaluate(() => Array.from(window.Module.FS.readFile('/user/Scouting 2001-07-29.sav')));
    const { scoutManager } = await import('../shortlist.js');
    const lists = scoutManager(new SaveGame(new Uint8Array(bytes)));
    const first = g.players().find((x) => x.name === NAMES[0]);
    check('saved: Klaus\'s club asked for him, Mike\'s did not', lists.requested(lists.managers[0].id, first.row) && !lists.requested(lists.managers[1].id, first.row));
    await ctx.close();
  }
  {
    // A phone: each star sits in the middle of its button, the button as tall as its row.
    const ctx = await browser.newContext({ ...devices['iPhone 13'] });
    const p = await editorOn(ctx, ME.member);
    await scoutTab(p);
    await p.waitForSelector('#scout-results .sl-star');
    const off = await p.$$eval('#scout-results .scout-item', (items) => items.map((it) => {
      const r = it.querySelector('.ed-row').getBoundingClientRect();
      return Math.max(...[...it.querySelectorAll('.sl-star, .sr-eye')].map((b) => { const s = b.getBoundingClientRect();
      const range = document.createRange(); range.selectNodeContents(b);
      const g = range.getBoundingClientRect();
      return Math.max(Math.abs((g.left + g.right) / 2 - (s.left + s.right) / 2), Math.abs((g.top + g.bottom) / 2 - (s.top + s.bottom) / 2),
        Math.abs(s.top - r.top), Math.abs(s.bottom - r.bottom)); }));
    }));
    check('phone: every star and Scout button centred in its button, the button as tall as its row', off.length > 0 && off.every(Number.isFinite) && Math.max(...off) <= 1.5, JSON.stringify(off));
    const labels = await p.$$eval('#scout-filters > span:not(.pair):not(.small)', (spans) => spans.filter((x) => x.textContent.trim()).map((x) => {
      const ctl = x.nextElementSibling.firstElementChild ?? x.nextElementSibling, a = x.getBoundingClientRect(), b = ctl.getBoundingClientRect();
      return [x.textContent.trim(), Math.round(Math.abs((a.top + a.bottom) / 2 - (b.top + b.bottom) / 2))];
    }));
    check('phone: each filter label level with its first control', labels.length > 0 && labels.every(([, d]) => d <= 2), JSON.stringify(labels));
    await shot(p, 'shortlist-phone');
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
    await p.keyboard.press('Escape');
    await p.locator('#scout-results .scout-item').nth(0).locator('.sr-eye').click();
    check('free: Scout opens the Account dialog too, nothing changes', await p.isVisible('#account[open]')
      && !/changed/.test(await p.textContent('#editor-save')));
    await ctx.close();
    const ctx2 = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const day = new Date(Date.now() + 5 * 86400e3).toISOString().slice(0, 10);
    const p2 = await editorOn(ctx2, { ...ME.member, early: { legend: false, features: { shortlist: day, 'scout-request': day } } });
    await scoutTab(p2);
    await p2.waitForSelector('#scout-results .early-locked', { timeout: 10000 }).catch(() => {});
    check('early access: a member who is no Club Legend sees one locked line, no stars, no Scout buttons', await p2.locator('#scout-results .early-locked').count() === 1
      && await p2.locator('#scout-results .sl-star').count() === 0 && await p2.locator('#scout-results .sr-eye').count() === 0
      && await p2.locator('#scout-filters [data-sl-manager]').count() === 0);
    await ctx2.close();
    const ctx3 = await browser.newContext({ viewport: { width: 1280, height: 820 } });
    const p3 = await editorOn(ctx3, { ...ME.member, early: { legend: false, features: { 'scout-request': day } } });
    await scoutTab(p3);
    await p3.waitForSelector('#scout-results .early-locked', { timeout: 10000 }).catch(() => {});
    check('only Scout early: the stars and the manager stay, no Scout buttons', await p3.locator('#scout-results .sl-star').count() > 0
      && await p3.locator('#scout-results .sr-eye').count() === 0 && await p3.locator('#scout-filters [data-sl-manager]').count() === 1);
    await ctx3.close();
  }
  check('no page errors', errors.length === 0, errors.join(' | '));
} finally {
  await browser.close();
  server.close();
  rmSync(tmp, { recursive: true, force: true });
}
console.log(ok ? 'scout check: all ok' : 'scout check: FAILED');
process.exit(ok ? 0 : 1);
