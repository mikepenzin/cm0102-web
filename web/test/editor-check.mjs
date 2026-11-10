// The browser check of the save editor (editor.js, savedit.js): the page with the stand-in engine, a small saved
// game in the game's format (fakes.mjs) in the user folder, Saved games, Edit, a search, a change, Save.
// Needs Playwright: NODE_PATH=$(npm root -g) node web/test/editor-check.mjs [screenshot folder]
import { createRequire } from 'node:module';
import { copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { tmpdir } from 'node:os';
import { join, resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createApp } from '../server.mjs';
import { SaveGame } from '../savedit.js';
import { fakeSaveGame } from './fakes.mjs';

const require = createRequire(import.meta.url);
const { chromium } = require('playwright');
const HERE = dirname(fileURLToPath(import.meta.url));
const SHOTS = process.argv[2] ? resolve(process.argv[2]) : null;

const tmp = mkdtempSync(join(tmpdir(), 'cm0102-editor-check-'));
const game = join(tmp, 'game'), build = join(tmp, 'build-web');
mkdirSync(join(game, 'Data'), { recursive: true });
mkdirSync(join(build, 'seasons'), { recursive: true });
const exe = Buffer.from('MZ the game');
writeFileSync(join(game, 'cm0102.exe'), exe);
writeFileSync(join(tmp, 'manifest.tsv'), `${createHash('sha256').update(exe).digest('hex')}\t${exe.length}\tcm0102.exe\n`);
for (const t of ['rules', 'names', 'clubs', 'club_records']) writeFileSync(join(build, 'seasons', `${t}.tsv`), 'x\n');
copyFileSync(join(HERE, 'fake-emscripten.js'), join(build, 'cm0102_recomp.js'));
writeFileSync(join(build, 'cm0102_recomp.wasm'), Buffer.from([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0]));
writeFileSync(join(build, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=asyncify\n');
const server = createApp({ gameDir: game, buildDir: build, manifestPath: join(tmp, 'manifest.tsv') });
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const BASE = `http://127.0.0.1:${server.address().port}`;

let ok = true;
const check = (what, cond, detail = '') => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}${detail ? `: ${detail}` : ''}`); ok &&= Boolean(cond); };
const browser = await chromium.launch();
const shot = async (page, name) => { if (SHOTS) { mkdirSync(SHOTS, { recursive: true }); await page.screenshot({ path: join(SHOTS, `editor-${name}.png`) }); } };
const save = await fakeSaveGame();
const fileOf = (page, path) => page.evaluate((p) => { try { return Array.from(window.Module.FS.readFile(p)); } catch { return null; } }, path);
try {
  for (const [label, viewport, touch] of [['computer', { width: 1280, height: 820 }, false], ['phone', { width: 400, height: 820 }, true]]) {
    const ctx = await browser.newContext({ viewport, hasTouch: touch, isMobile: touch });
    const p = await ctx.newPage();
    const errors = [];
    p.on('pageerror', (e) => errors.push(e.message));
    p.on('dialog', (d) => d.accept());
    await p.goto(BASE);
    await p.click('#go');
    await p.waitForFunction(() => window.__fake, null, { timeout: 30000 });
    await p.evaluate((b) => window.Module.FS.writeFile('/user/Juventus 2001-07-29.sav', new Uint8Array(b)), Array.from(save));
    await p.click('#saves-open');
    const edits = await p.$$eval('#saves-list button[data-edit]', (bs) => bs.length);
    check(`${label}: Saved games has an Edit button for each save`, edits === 2, `${edits}`);
    await p.click('#saves-list tr:has-text("Juventus 2001-07-29.sav") button[data-edit]');
    await p.waitForSelector('#editor[open] .ed-row');
    check(`${label}: Edit opens the save, its players best first`, (await p.$$eval('.ed-row b', (b) => b.map((x) => x.textContent))).join() === 'Gianluigi Buffon,Alessandro Del Piero');
    await p.fill('#editor-search', 'del pie');
    await p.waitForFunction(() => document.querySelectorAll('.ed-row').length === 1);
    await p.click('.ed-row');
    await p.waitForSelector('#editor-form [data-f="Pace"]');
    check(`${label}: the player's fields: ability, positions, the attributes in groups, the hidden ones`,
      (await p.inputValue('[data-f="ca"]')) === '181' && (await p.inputValue('[data-f="Pace"]')) === '15'
        && (await p.inputValue('[data-f="Determination"]')) === '16' && (await p.inputValue('[data-f="born"]')) === '1974-11-09'
        && (await p.$$('#editor-form section')).length === 7);
    if (touch) check('phone: the list gives way to the player, with a way back', !(await p.isVisible('#editor-search')) && await p.isVisible('.ed-back'));
    await p.fill('[data-f="Pace"]', '20');
    await p.fill('[data-f="ca"]', '195');
    await p.fill('[data-f="Heading"]', '1');
    await p.fill('[data-f="born"]', '1982-03-01');
    check(`${label}: a changed field is marked`, await p.$eval('[data-f="Pace"]', (i) => i.classList.contains('changed')));
    await shot(p, `${label}-form`);
    await p.click('#editor-apply');
    const note = await p.textContent('#editor-note');
    check(`${label}: Apply tells what the game cannot show as asked`, /Heading shows \d+: with this ability the game cannot show it as asked/.test(note), note);
    check(`${label}: the Save button counts the changed players`, (await p.textContent('#editor-save')) === 'Save the game file (1 changed)');
    await p.click('#editor-save');
    await p.waitForFunction(() => /^Saved:/.test(document.getElementById('editor-note').textContent));
    const written = await fileOf(p, '/user/Juventus 2001-07-29.sav');
    const g = new SaveGame(new Uint8Array(written));
    const f = g.read(g.search('del piero')[0].row);
    check(`${label}: the file in the user folder holds the change`, f.ca === 195 && f.attrs.Pace === 20 && f.born.year === 1982 && f.attrs.Heading > 1,
      `CA ${f.ca}, Pace ${f.attrs.Pace}, born ${f.born.year}, Heading ${f.attrs.Heading}`);
    const old = await fileOf(p, '/user/backups/Juventus 2001-07-29 (before edit).sav');
    check(`${label}: the save as it was is kept in backups`, old && Buffer.from(old).equals(Buffer.from(save)));
    await shot(p, `${label}-saved`);
    await p.click('#editor-close');
    // The stand-in game's own save is not a real one: the editor says so.
    await p.click('#saves-open');
    await p.click('#saves-list tr:has-text("Harbour City") button[data-edit]');
    await p.waitForFunction(() => /cannot be edited/.test(document.getElementById('editor-note').textContent));
    check(`${label}: a file that is not a full saved game is refused`, /cannot be edited/.test(await p.textContent('#editor-note')));
    await p.click('#editor-close');
    check(`${label}: no page errors`, errors.length === 0, errors.join('; '));
    await ctx.close();
  }
} finally {
  await browser.close();
  server.close();
  rmSync(tmp, { recursive: true, force: true });
}
console.log(ok ? 'editor check: all ok' : 'editor check: FAILED');
process.exit(ok ? 0 : 1);
