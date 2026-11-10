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
import { fakeDataSet, fakeSaveGame } from './fakes.mjs';

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
// The game's own database: a small data set in its format, which a new data set copies.
const sha = (b) => createHash('sha256').update(b).digest('hex');
let manifest = `${sha(exe)}\t${exe.length}\tcm0102.exe\n`;
for (const [name, bytes] of Object.entries(fakeDataSet())) {
  writeFileSync(join(game, 'Data', name), bytes);
  manifest += `${sha(bytes)}\t${bytes.length}\tData/${name}\n`;
}
writeFileSync(join(tmp, 'manifest.tsv'), manifest);
const fakeStaffLength = fakeDataSet()['staff.dat'].length;
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

// The data set editor: the Editor button, a new data set from the game's data, an edit, a new player, Save, Delete.
async function dataSets(p, label) {
  await p.click('#editor-open');
  await p.waitForSelector('#editor[open] #editor-new');
  check(`${label}: the Editor lists the saved games and offers a new data set`, (await p.$$('[data-open-save]')).length === 2);
  await p.fill('#editor-new-name', 'May 2026');
  await p.click('#editor-new');
  check(`${label}: a name with a month and year is refused`, /season/.test(await p.textContent('#editor-note')));
  await p.fill('#editor-new-name', 'Test set');
  await p.click('#editor-new');
  await p.waitForSelector('#editor .ed-row', { timeout: 30000 });
  check(`${label}: New data set copies the game's data and opens it`,
    (await p.$$eval('.ed-row b', (b) => b.map((x) => x.textContent))).join() === 'Gianluigi Buffon,Alessandro Del Piero,Mario Rossi' && await p.isVisible('#editor-add'));
  await p.fill('#editor-search', 'del piero');
  await p.waitForFunction(() => document.querySelectorAll('.ed-row').length === 1);
  await p.click('.ed-row');
  await p.waitForSelector('#editor-form [data-f="first"]');
  check(`${label}: a data set player has his names, nation, club and contract too`, (await p.inputValue('[data-f="first"]')) === 'Alessandro'
    && (await p.inputValue('[data-f="club"]')) === 'Juventus (Italy)' && (await p.inputValue('[data-f="Pace"]')) === '12');
  await p.fill('[data-f="Finishing"]', '20');
  await p.fill('[data-f="club"]', 'AC Milan (Italy)');
  await p.fill('[data-f="common"]', 'Pinturicchio');
  await p.click('#editor-apply');
  check(`${label}: Apply`, /^Applied/.test(await p.textContent('#editor-note')), await p.textContent('#editor-note'));
  await p.click('#editor-add');
  await p.waitForSelector('#editor-form [data-f="first"]');
  await p.fill('[data-f="first"]', 'Marco');
  await p.fill('[data-f="second"]', 'Testa');
  await p.fill('[data-f="club"]', 'Juventus (Italy)');
  await p.fill('[data-f="Pace"]', '19');
  await p.fill('[data-f="ca"]', '150');
  await shot(p, 'new-player');
  await p.click('#editor-apply');
  check(`${label}: Add this player`, /^Added/.test(await p.textContent('#editor-note')), await p.textContent('#editor-note'));
  check(`${label}: two players changed`, (await p.textContent('#editor-save')) === 'Save the data set (2 changed)');
  await p.fill('[data-f="club"]', 'Atletico Nowhere');
  await p.click('#editor-apply');
  check(`${label}: a club that does not exist is refused`, /no club called/.test(await p.textContent('#editor-note')));
  await p.click('#editor-save');
  await p.waitForFunction(() => /^Saved:/.test(document.getElementById('editor-note').textContent), null, { timeout: 30000 });
  const back = await p.evaluate(async () => {
    const { DataSet, DATA_SET_FILES } = await import('/dataset.js');
    const FS = window.Module.FS, dir = '/user/Databases/Test set';
    const ds = new DataSet(Object.fromEntries(DATA_SET_FILES.map((f) => [f, FS.readFile(`${dir}/${f}`)])));
    return ds.players().map((x) => `${x.name}|${x.club}|${x.ca}`).sort();
  });
  check(`${label}: the data set's files hold the edit and the new player`,
    back.join() === 'Gianluigi Buffon|Juventus|187,Marco Testa|Juventus|150,Mario Rossi||60,Pinturicchio|AC Milan|183', back.join());
  const own = await p.evaluate(() => window.Module.FS.readFile('/game/Data/staff.dat').length);
  check(`${label}: the game's own data stays as it was`, own === fakeStaffLength, `${own}`);
  await shot(p, 'data-set');
  await p.click('#editor-home-button');
  await p.waitForSelector('[data-delete-set="Test set"]');
  await p.click('[data-delete-set="Test set"]');
  await p.waitForFunction(() => !document.querySelector('[data-open-set]'));
  check(`${label}: Delete takes the data set away`, !(await p.evaluate(() => { try { window.Module.FS.readdir('/user/Databases/Test set'); return true; } catch { return false; } })));
  await p.click('#editor-close');
}

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
    if (!touch) await dataSets(p, label);
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
