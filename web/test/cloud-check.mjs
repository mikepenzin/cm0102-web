// The browser check of the accounts and cloud saves: the page (loader.js, account.js, saves.js) and the server
// (server.mjs, cloud.mjs) in Chromium, with the stand-ins of Google and the S3 store (fakes.mjs) and of the engine
// (fake-emscripten.js), and two small game files in place of the 760.
// Needs Playwright: NODE_PATH=$(npm root -g) node web/test/cloud-check.mjs [screenshot folder]
import { createRequire } from 'node:module';
import { copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { tmpdir } from 'node:os';
import { join, resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { gzipSync, gunzipSync } from 'node:zlib';
import { createApp } from '../server.mjs';
import { createCloud } from '../cloud.mjs';
import { createS3 } from '../s3.mjs';
import { fakeGoogle, fakeS3, fakeSaveBytes } from './fakes.mjs';

const require = createRequire(import.meta.url);
const { chromium } = require('playwright');
const HERE = dirname(fileURLToPath(import.meta.url));
const SHOTS = process.argv[2] ? resolve(process.argv[2]) : null;

const tmp = mkdtempSync(join(tmpdir(), 'cm0102-cloud-check-'));
const game = join(tmp, 'game'), build = join(tmp, 'build-web');
mkdirSync(join(game, 'Data'), { recursive: true });
mkdirSync(join(build, 'seasons'), { recursive: true });
const exe = Buffer.from('MZ the game'), dat = Buffer.alloc(5000, 3);
writeFileSync(join(game, 'cm0102.exe'), exe);
writeFileSync(join(game, 'Data/staff.dat'), dat);
const sha = (b) => createHash('sha256').update(b).digest('hex');
writeFileSync(join(tmp, 'manifest.tsv'), `${sha(exe)}\t${exe.length}\tcm0102.exe\n${sha(dat)}\t${dat.length}\tData/staff.dat\n`);
for (const t of ['rules', 'names', 'clubs', 'club_records']) writeFileSync(join(build, 'seasons', `${t}.tsv`), 'x\n');
copyFileSync(join(HERE, 'fake-emscripten.js'), join(build, 'cm0102_recomp.js'));
writeFileSync(join(build, 'cm0102_recomp.wasm'), Buffer.from([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0]));
writeFileSync(join(build, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=asyncify\n');

const s3fake = await fakeS3();
const google = await fakeGoogle();
const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
const logs = [];
const cloud = createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'z'.repeat(40),
  cloudUsers: ['player@example.com'], upgradeUrl: 'https://example.com/support', maxSaves: 10, google: google.google, log: (t) => logs.push(t) });
const server = createApp({ gameDir: game, buildDir: build, manifestPath: join(tmp, 'manifest.tsv'), cloud });
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const BASE = `http://127.0.0.1:${server.address().port}`;

let ok = true;
const check = (what, cond, detail = '') => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}${detail ? `: ${detail}` : ''}`); ok &&= Boolean(cond); };
const PLAYER = { sub: '4000004', email: 'player@example.com', email_verified: true, name: 'Alex Player' };
const keyOf = (name) => `saves/${PLAYER.sub}/${Buffer.from(name).toString('base64url')}.gz`;
const cloudBytes = (name) => { const o = s3fake.objects.get(keyOf(name)); return o ? gunzipSync(o.body) : null; };
// What another device does: a save straight into the store, as the server would put it.
const putFromElsewhere = (name, raw) => s3.put(keyOf(name), gzipSync(raw), { type: 'application/gzip', metadata: { 'raw-size': raw.length, saved: Date.now() } });
const localBytes = (page, name) => page.evaluate((n) => { try { return Array.from(window.Module.FS.readFile(`/user/${n}`)); } catch { return null; } }, name);
const localNames = (page) => page.evaluate(() => window.Module.FS.readdir('/user').filter((n) => n.endsWith('.sav')).sort());
const same = (arr, buf) => Array.isArray(arr) && Buffer.from(arr).equals(buf);
const toasts = (page) => page.evaluate(() => [...document.querySelectorAll('#toasts .toast')].map((t) => t.textContent).join(' | '));
const waitFor = async (what, fn, ms = 15000) => { const end = Date.now() + ms; while (Date.now() < end) { if (await fn()) return true; await new Promise((r) => setTimeout(r, 150)); } console.log(`  (waited ${ms} ms for ${what})`); return false; };

const browser = await chromium.launch();
const shot = async (page, name) => { if (SHOTS) { mkdirSync(SHOTS, { recursive: true }); await page.screenshot({ path: join(SHOTS, `cloud-${name}.png`) }); } };
try {
  google.user = PLAYER;
  const fromIpad = fakeSaveBytes(11);
  await putFromElsewhere('From iPad.sav', fromIpad);

  // ---- A computer: sign in from the start box, the cloud's save comes in, the game's saves go up. ----
  const a = await (await browser.newContext({ viewport: { width: 1200, height: 800 } })).newPage();
  const errors = [];
  a.on('pageerror', (e) => errors.push(e.message));
  a.on('dialog', (d) => d.accept());
  await a.goto(BASE);
  await a.waitForSelector('#go');
  check('signed out: the bar says Sign in', await a.isVisible('#account-open') && (await a.textContent('#account-open span')) === 'Sign in');
  check('the start box offers the sign-in', await a.isVisible('#go-signin'));
  await shot(a, 'start');
  await Promise.all([a.waitForURL(`${BASE}/`), a.click('#go-signin')]);
  await a.waitForSelector('#go');
  await waitFor('the account', async () => (await a.textContent('#account-open span')) === 'Alex');
  check('back from Google: signed in, the address is clean, the button names the player', (await a.textContent('#account-open span')) === 'Alex'
    && a.url() === `${BASE}/` && await a.evaluate(() => document.getElementById('account-open').classList.contains('on')));
  check('a message says so', /Signed in as player@example\.com\. Cloud saves are on\./.test(await toasts(a)), await toasts(a));
  await a.click('#go');
  await a.waitForFunction(() => window.__fake, null, { timeout: 30000 }).catch(async (e) => {
    console.log('LOG:', await a.textContent('#log'), '\nOVERLAY:', await a.textContent('#overlay'), '\nERRORS:', errors.join('; '));
    throw e;
  });
  check('before the game started, the cloud\'s save came into the user folder', same(await localBytes(a, 'From iPad.sav'), fromIpad));
  check('and a message says so', /1 saved game came from the cloud/.test(await toasts(a)), await toasts(a));
  // The stand-in engine wrote "Harbour City 2001-07-14.sav" when it started, as the game saves.
  const harbour = 'Harbour City 2001-07-14.sav';
  await waitFor('the save to go up', () => cloudBytes(harbour) !== null);
  check('the game\'s save went up to the cloud', same(await localBytes(a, harbour), cloudBytes(harbour)));
  await waitFor('the message', async () => /Saved to the cloud/.test(await toasts(a)));
  check('the messages: saved here, then in the cloud', /Saved in this browser/.test(await toasts(a)) && /Saved to the cloud/.test(await toasts(a)), await toasts(a));
  check('the cloud\'s save was not sent back up', s3fake.requests.filter((r) => r.startsWith('PUT') && r.includes(Buffer.from('From iPad.sav').toString('base64url'))).length === 1);
  await a.click('#saves-open');
  const cells = await a.$$eval('#saves-list td.cloud', (tds) => tds.map((t) => t.textContent.trim()));
  check('Saved games shows both in the cloud', cells.length === 2 && cells.every((c) => c.startsWith('In the cloud')), cells.join(', '));
  await shot(a, 'saves');

  // Remove one from the cloud: it stays here, and does not go up again.
  await a.click(`#saves-list button[data-remove="From iPad.sav"]`);
  await waitFor('the removal', () => !s3fake.objects.has(keyOf('From iPad.sav')));
  check('Remove takes it from the cloud and leaves it here', !s3fake.objects.has(keyOf('From iPad.sav')) && same(await localBytes(a, 'From iPad.sav'), fromIpad));
  check('it shows as Only here', (await a.$$eval('#saves-list td.cloud', (tds) => tds.map((t) => t.textContent.trim()))).includes('Only here'));
  await a.click('#saves-close');

  // Two devices at once: another device saves Harbour City, then this one saves it again. Both versions are kept.
  const elsewhere = fakeSaveBytes(22), here = fakeSaveBytes(33);
  await putFromElsewhere(harbour, elsewhere);
  await a.evaluate(([n, b]) => window.Module.FS.writeFile(`/user/${n}`, new Uint8Array(b)), [harbour, Array.from(here)]);
  const other = 'Harbour City 2001-07-14 (other device).sav';
  await waitFor('both versions', () => cloudBytes(other) !== null && cloudBytes(harbour)?.equals(here));
  check('changed on both sides: this device\'s save under its name, in both places', same(await localBytes(a, harbour), here) && cloudBytes(harbour)?.equals(here));
  check('the other device\'s save as "(other device)", in both places', same(await localBytes(a, other), elsewhere) && cloudBytes(other)?.equals(elsewhere));
  await waitFor('the message', async () => /was also saved on another device/.test(await toasts(a)));
  check('a message names the kept copy', /That version is kept as Harbour City 2001-07-14 \(other device\)\.sav/.test(await toasts(a)), await toasts(a));
  const removedAgain = s3fake.objects.has(keyOf('From iPad.sav'));
  check('the removed save did not go up again', !removedAgain);
  await a.click('#account-open');
  const dialogText = (await a.textContent('#account-body')).replace(/\s+/g, ' ');
  check('Account: the email, cloud saves on, the last sync', /Signed in as player@example\.com/.test(dialogText) && /Cloud saves are on/.test(dialogText) && /Last sync:/.test(dialogText), dialogText.slice(0, 200));
  await shot(a, 'account');
  await a.click('#account-close');

  // ---- An iPad (a second device): signs in, gets every cloud save before its game starts. ----
  const ipadCtx = await browser.newContext({ viewport: { width: 1180, height: 820 }, hasTouch: true, isMobile: true });
  const b = await ipadCtx.newPage();
  b.on('pageerror', (e) => errors.push(e.message));
  await b.goto(BASE);
  await b.waitForSelector('#go');
  await b.tap('#account-open');
  await b.waitForSelector('#account-signin');
  await Promise.all([b.waitForURL(`${BASE}/`), b.tap('#account-signin')]);
  await b.waitForSelector('#go');
  await b.tap('#go');
  await b.waitForFunction(() => window.__fake, null, { timeout: 30000 });
  check('the second device gets both versions of Harbour City before its game starts',
    same(await localBytes(b, harbour), here) && same(await localBytes(b, other), elsewhere));
  check('and not the save removed from the cloud', (await localBytes(b, 'From iPad.sav')) === null, (await localNames(b)).join(', '));
  // Sign out: the saves stay, the cloud column goes.
  await b.tap('#account-open');
  await b.tap('#account-signout');
  await waitFor('signed out', async () => (await b.textContent('#account-open span')) === 'Sign in');
  check('signed out: the button says Sign in, the saves stay', (await b.textContent('#account-open span')) === 'Sign in' && same(await localBytes(b, harbour), here));
  await b.tap('#account-close');
  await b.tap('#saves-open');
  check('Saved games has no cloud column after the sign-out', (await b.$$('#saves-list td.cloud')).length === 0);
  await b.tap('#saves-close');

  // ---- Sign in while the game runs: a small window, the game stays; new cloud saves come in, none is replaced. ----
  const c = await (await browser.newContext({ viewport: { width: 1200, height: 800 } })).newPage();
  c.on('pageerror', (e) => errors.push(e.message));
  await c.goto(BASE);
  await c.click('#go');
  await c.waitForFunction(() => window.__fake, null, { timeout: 30000 });
  const mine = await localBytes(c, harbour);                       // this device's own Harbour City, from its stand-in game
  await c.click('#account-open');
  const [popup] = await Promise.all([c.context().waitForEvent('page'), c.click('#account-signin')]);
  await popup.waitForLoadState().catch(() => {});
  await waitFor('the sign-in in the small window', async () => (await c.textContent('#account-open span')) === 'Alex');
  check('signed in from a small window while the game runs', (await c.textContent('#account-open span')) === 'Alex' && await c.evaluate(() => Boolean(window.__fake)));
  await waitFor('the other device\'s save', async () => same(await localBytes(c, other), elsewhere));
  check('a new save of the cloud came in', same(await localBytes(c, other), elsewhere));
  check('the save of this device was not replaced while the game runs', same(await localBytes(c, harbour), Buffer.from(mine)));
  // Its own Harbour City and the cloud's differ, never synced: kept twice. The cloud's comes in under a name that is
  // free in both places ("(other device)" is taken in the cloud already).
  const other2 = 'Harbour City 2001-07-14 (other device) (2).sav';
  await waitFor('both kept', () => cloudBytes(harbour)?.equals(Buffer.from(mine)) && cloudBytes(other2)?.equals(here));
  check('a device that signs in with its own version: both kept, under free names',
    same(await localBytes(c, other2), here) && cloudBytes(other2)?.equals(here) && cloudBytes(harbour)?.equals(Buffer.from(mine))
      && cloudBytes(other)?.equals(elsewhere), (await localNames(c)).join(', '));

  // ---- A free account: signed in, no cloud saves, a way to get them. ----
  google.user = { sub: '5000005', email: 'free@example.com', email_verified: true, name: 'Free Player' };
  const d = await (await browser.newContext()).newPage();
  await d.goto(`${BASE}/auth/google/start?mode=page`);
  await d.waitForSelector('#go');
  await waitFor('the account', async () => (await d.textContent('#account-open span')) === 'Free');
  await d.click('#account-open');
  const free = (await d.textContent('#account-body')).replace(/\s+/g, ' ');
  check('a free account: no cloud saves, and the link to get them', /Cloud saves are not on for this account/.test(free)
    && (await d.getAttribute('#account-body a.button', 'href')) === 'https://example.com/support', free.slice(0, 160));
  await shot(d, 'free');

  check('no page errors', errors.length === 0, errors.join('; '));
  check('the server logged no failure', !logs.some((l) => /fail|not written|not read/.test(l)), logs.join('; '));
} finally {
  await browser.close();
  server.close();
  s3fake.close();
  google.close();
  rmSync(tmp, { recursive: true, force: true });
}
console.log(ok ? 'cloud check: all ok' : 'cloud check: FAILED');
process.exit(ok ? 0 : 1);
