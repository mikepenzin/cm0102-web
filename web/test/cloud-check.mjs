// The browser check of the accounts and cloud saves: the page (loader.js, account.js, saves.js) and the server
// (server.mjs, cloud.mjs) in Chromium, with the stand-ins of Google and the S3 store (fakes.mjs) and of the engine
// (fake-emscripten.js), and two small game files in place of the 760.
// Needs Playwright: NODE_PATH=$(npm root -g) node web/test/cloud-check.mjs [screenshot folder]
import { createRequire } from 'node:module';
import net from 'node:net';
import { copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { tmpdir } from 'node:os';
import { join, resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { gzipSync, gunzipSync } from 'node:zlib';
import { createApp } from '../server.mjs';
import { createCloud } from '../cloud.mjs';
import { createLibrary } from '../library.mjs';
import { createStats, statsStore } from '../stats.mjs';
import { createS3 } from '../s3.mjs';
import { coffeeEvent, fakeDataSet, fakeGoogle, fakeS3, fakeSaveBytes, paddleEvent, signCoffee, signPaddle } from './fakes.mjs';
import { writeZip } from '../zip.js';

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
  cloudUsers: ['player@example.com'], maxSaves: 10, maxPacked: 2 << 20, google: google.google, log: (t) => logs.push(t), price: '$5 a month',
  isAdmin: (email) => email === 'player@example.com', storeTtl: 0,
  paddle: { env: 'sandbox', clientToken: 'test_token', priceIds: ['pri_cloud_month'], webhookSecret: 'pdl_ntfset_check', apiKey: null, apiBase: null } });
// Paddle.js, as far as the page uses it: Initialize, and a checkout that completes at once. The test then sends the
// webhook that Paddle would send.
const FAKE_PADDLE = `window.Paddle = {
  Environment: { set(e) { window.__paddleEnv = e; } },
  Initialize(o) { window.__paddleInit = o; },
  Checkout: { open(o) { window.__paddleOpen = o; setTimeout(() => window.__paddleInit.eventCallback({ name: 'checkout.completed', data: {} }), 200); } },
};`;
const library = createLibrary({ s3, presign: (m, k, o) => cloud.presign(m, k, o), isDirect: () => cloud.isDirect(), log: (t) => logs.push(t) });
const server = createApp({ gameDir: game, buildDir: build, manifestPath: join(tmp, 'manifest.tsv'), cloud, stats: createStats({ store: statsStore(s3) }), statsMode: 'optout', library,
  admins: ['player@example.com'] });
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const BASE = `http://127.0.0.1:${server.address().port}`;

let ok = true;
const check = (what, cond, detail = '') => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}${detail ? `: ${detail}` : ''}`); ok &&= Boolean(cond); };
const PLAYER = { sub: '4000004', email: 'player@example.com', email_verified: true, name: 'Alex Player' };
const keyOf = (name) => `saves/${PLAYER.sub}/${Buffer.from(name).toString('base64url')}.gz`;
const cloudBytes = (name) => { const o = s3fake.objects.get(keyOf(name)); return o ? gunzipSync(o.body) : null; };
// What another device does: a save straight into the store, as the server would put it.
const putFromElsewhere = (name, raw, saved = Date.now()) => s3.put(keyOf(name), gzipSync(raw), { type: 'application/gzip', metadata: { 'raw-size': raw.length, saved } });
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
  const startBox = (await a.innerText('#overlay')).replace(/\s+/g, ' ');
  check('the start box says the game is free, and Cloud Saves an optional add-on at $5 a month',
    /Start the game Free · no account, no payment/.test(startBox) && /Optional: Membership · \$5 a month/.test(startBox) && !(await a.$('#overlay .plans')), startBox.slice(0, 300));
  check('and the links to pricing, terms, refunds and privacy', (await a.$$eval('#overlay .legal a', (as) => as.map((x) => x.getAttribute('href')))).join() === '/pricing,/terms,/refunds,/privacy');
  await shot(a, 'start');
  await Promise.all([a.waitForURL(`${BASE}/`), a.click('#go-signin')]);
  await a.waitForSelector('#go');
  await waitFor('the account', async () => (await a.textContent('#account-open span')) === 'Alex');
  check('back from Google: signed in, the address is clean, the button names the player', (await a.textContent('#account-open span')) === 'Alex'
    && a.url() === `${BASE}/` && await a.evaluate(() => document.getElementById('account-open').classList.contains('on')));
  check('a message says so', /Signed in as player@example\.com\. Cloud Saves are on\./.test(await toasts(a)), await toasts(a));
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
  check('straight to the store: the page sent it to incoming/ with a signed link, the server copied it in place',
    s3fake.requests.some((r) => /^PUT \/[^/]+\/incoming\/4000004\/\d+-[0-9a-f]{12}\.gz\?.*X-Amz-Signature=/.test(r))
      && ![...s3fake.objects.keys()].some((k) => k.startsWith('incoming/')), s3fake.requests.filter((r) => r.startsWith('PUT')).slice(0, 3).join(' · ').slice(0, 300));
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

  // Two devices, the save here the newer: another device sent an older version of Harbour City (saved an hour ago), then
  // this one saves it. The version here goes up over the cloud's, and nothing comes down (a player: "if the browser's
  // version is newer than the cloud's, use it and update the cloud", 21 November 2026).
  const olderThere = fakeSaveBytes(44), newerHere = fakeSaveBytes(55);
  const getsBefore = s3fake.requests.filter((r) => r.startsWith('GET') && r.includes(keyOf(harbour).split('/').pop())).length;
  await putFromElsewhere(harbour, olderThere, Date.now() - 3600e3);
  await a.evaluate(([n, b]) => window.Module.FS.writeFile(`/user/${n}`, new Uint8Array(b)), [harbour, Array.from(newerHere)]);
  await waitFor('the newer save here to go up', () => cloudBytes(harbour)?.equals(newerHere));
  const getsAfter = s3fake.requests.filter((r) => r.startsWith('GET') && r.includes(keyOf(harbour).split('/').pop())).length;
  check('changed on both sides, the save here the newer: it goes up over the cloud\'s, nothing comes down, no second copy',
    cloudBytes(harbour)?.equals(newerHere) && same(await localBytes(a, harbour), newerHere) && getsAfter === getsBefore
      && !(await localNames(a)).some((n) => /other device/.test(n)), `${getsAfter - getsBefore} downloads; ${(await localNames(a)).join(', ')}`);

  // Two devices at once, the other device's save the newer: another device saves Harbour City (its time a minute ahead),
  // while this one saves it too. Both versions are kept, so neither device's game is lost.
  const elsewhere = fakeSaveBytes(22), here = fakeSaveBytes(33);
  await putFromElsewhere(harbour, elsewhere, Date.now() + 60000);
  await a.evaluate(([n, b]) => window.Module.FS.writeFile(`/user/${n}`, new Uint8Array(b)), [harbour, Array.from(here)]);
  const other = 'Harbour City 2001-07-14 (other device).sav';
  await waitFor('both versions', () => cloudBytes(other) !== null && cloudBytes(harbour)?.equals(here));
  check('changed on both sides: this device\'s save under its name, in both places', same(await localBytes(a, harbour), here) && cloudBytes(harbour)?.equals(here));
  check('the other device\'s save as "(other device)", in both places', same(await localBytes(a, other), elsewhere) && cloudBytes(other)?.equals(elsewhere));
  await waitFor('the message', async () => /was also saved on another device/.test(await toasts(a)));
  check('a message names the kept copy', /That version is kept as Harbour City 2001-07-14 \(other device\)\.sav/.test(await toasts(a)), await toasts(a));
  const removedAgain = s3fake.objects.has(keyOf('From iPad.sav'));
  check('the removed save did not go up again', !removedAgain);

  // The store has a bad moment: it fails a save 4 times. The server tries it 3 times and answers 502; the page tries
  // again after 3 s, and it goes up.
  const writeSave = (n, b) => a.evaluate(([name, bytes]) => window.Module.FS.writeFile(`/user/${name}`, new Uint8Array(bytes)), [n, Array.from(b)]);
  const flaky = 'Flaky 2001-08-01.sav', flakyBytes = fakeSaveBytes(44);
  Object.assign(s3fake.faults, { failPuts: 4, failMatch: new RegExp(Buffer.from(flaky).toString('base64url')) });
  await writeSave(flaky, flakyBytes);
  await waitFor('the save the store failed', () => cloudBytes(flaky) !== null, 40000);
  check('a save the store failed 4 times still goes up: the server tries 3 times, the page again after a pause',
    cloudBytes(flaky)?.equals(flakyBytes) && /Flaky 2001-08-01\.sav up: HTTP 502; again in 3 s/.test(await a.textContent('#log')));
  // A save the store fails every time: the save beside it still goes up, and a message says that one waits.
  const stuck = 'Stuck 2001-08-02.sav', fine = 'Fine 2001-08-02.sav';
  Object.assign(s3fake.faults, { failPuts: 1000, failMatch: new RegExp(Buffer.from(stuck).toString('base64url')) });
  await writeSave(stuck, fakeSaveBytes(45));
  await writeSave(fine, fakeSaveBytes(46));
  await waitFor('the message about the save that waits', async () => /not in the cloud yet: the cloud did not take it this time/.test(await toasts(a)), 60000);
  check('one save the cloud does not take: the other goes up, and the message names the one that waits',
    cloudBytes(fine) !== null && cloudBytes(stuck) === null && /Stuck 2001-08-02\.sav is not in the cloud yet/.test(await toasts(a)), await toasts(a));
  check('and no "not reached" message: the cloud was reached', !/was not reached|no connection/.test(await toasts(a)));
  s3fake.faults.failPuts = 0;
  // A data set uploaded here goes up to the cloud too.
  const setZipBytes = await writeZip(Object.entries(fakeDataSet()).map(([f, data]) => ({ name: `My squads/${f}`, data })));
  await a.click('#saves-open');
  await a.setInputFiles('#sets-file', { name: 'My squads.zip', mimeType: 'application/zip', buffer: Buffer.from(setZipBytes) });
  const setKey = `sets/${PLAYER.sub}/${Buffer.from('My squads').toString('base64url')}.zip`;
  await waitFor('the data set to go up', () => s3fake.objects.has(setKey), 20000);
  check('an uploaded data set goes up to the cloud', s3fake.objects.has(setKey));
  check('straight to the store as well: the zip went to incoming/ with a signed link',
    s3fake.requests.some((r) => /^PUT \/[^/]+\/incoming\/4000004\/\d+-[0-9a-f]{12}\.zip\?.*X-Amz-Signature=/.test(r))
      && JSON.stringify(s3fake.objects.get(setKey)?.meta ?? {}).includes('raw-size'));
  await waitFor('the cloud column', async () => /In the cloud/.test(await a.textContent('#sets-list')));
  check('Data sets shows it in the cloud', /In the cloud/.test(await a.textContent('#sets-list')));
  await a.click('#saves-close');
  await a.click('#account-open');
  const dialogText = (await a.textContent('#account-body')).replace(/\s+/g, ' ');
  check('Account: the email, cloud saves on, the last sync', /Signed in as player@example\.com/.test(dialogText) && /Cloud Saves are on/.test(dialogText) && /Given to this account/.test(dialogText) && /Last sync:/.test(dialogText), dialogText.slice(0, 200));
  await shot(a, 'account');
  // An admin (CM_ADMIN_EMAILS): the dialog has the Admin panel button, which opens the panel in a tab of its own.
  const [panel] = await Promise.all([a.context().waitForEvent('page'), a.click('#account-admin')]);
  await panel.waitForFunction(() => !/Loading/.test(document.getElementById('main')?.textContent ?? 'Loading'), null, { timeout: 15000 });
  const panelText = (await panel.textContent('main')).replace(/\s+/g, ' ');
  check('an admin: Admin panel opens the statistics in a new tab, signed in', panel.url() === `${BASE}/admin` && !/Sign in with Google|not an admin/.test(panelText) && /Saves/.test(panelText), panelText.slice(0, 120));
  await panel.close();
  await a.click('#account-close');
  // The store stops letting the page in (its CORS rules changed): the page goes through the server for the rest of the
  // visit, and the save still goes up.
  s3fake.faults.corsGone = true;
  const through = 'Through 2001-08-03.sav', throughBytes = fakeSaveBytes(47);
  const before = s3fake.requests.length;
  await writeSave(through, throughBytes);
  await waitFor('the save through the server', () => cloudBytes(through)?.equals(throughBytes), 40000);
  check('the store refuses the page: the save goes up through the server, and the log says why',
    cloudBytes(through)?.equals(throughBytes) && /did not answer this page directly/.test(await a.textContent('#log'))
      && /Through 2001-08-03\.sav is in the cloud \([\d.]+ MB compressed, through the server\)/.test(await a.textContent('#log'))
      && s3fake.requests.slice(before).some((r) => r.startsWith('PUT') && r.includes(Buffer.from(through).toString('base64url')) && !r.includes('X-Amz-Signature')));
  s3fake.faults.corsGone = false;
  // The server is pointed at another bucket (BUCKET_NAME or its key changed): the saves of this page are not in it.
  // The store's id tells the page, and its saves go up again instead of showing as Only here.
  for (const k of [...s3fake.objects.keys()]) if (k.startsWith(`saves/${PLAYER.sub}/`)) s3fake.objects.delete(k);
  s3fake.objects.delete('meta/store.json');
  await writeSave('Trigger 2001-08-04.sav', fakeSaveBytes(48));            // a save, so the page syncs
  await waitFor('the saves back in the new store', () => cloudBytes(harbour) !== null && cloudBytes(through) !== null, 60000);
  check('another store: the saves already sent before go up to it again', cloudBytes(harbour) !== null && cloudBytes(through)?.equals(throughBytes)
    && /the cloud store is another one/.test(await a.textContent('#log')));
  await a.click('#saves-open');
  const after = await a.$$eval('#saves-list td.cloud', (tds) => tds.map((t) => t.textContent.trim()));
  check('and Saved games shows them in the cloud, not Only here', after.length > 0 && !after.includes('Only here'), after.join(', '));
  // With the cloud column and the Play button, a window of 820 once squeezed the Saved column to a letter a line (a
  // player's screenshot, 16 November 2026): each date keeps its width, the day over the time.
  await a.setViewportSize({ width: 820, height: 800 });
  const dates = await a.$$eval('#saves-list > table tr', (trs) => trs.filter((tr) => tr.querySelector('td')).map((tr) => tr.children[2])
    .map((t) => [Math.round(t.getBoundingClientRect().width), t.querySelectorAll('.small').length]));
  check('with the cloud column, a window of 820: each save\'s date keeps its width, the day over the time',
    dates.length > 0 && dates.every(([w, timeLine]) => w >= 80 && timeLine === 1), JSON.stringify(dates.slice(0, 3)));
  await a.setViewportSize({ width: 1200, height: 800 });
  check('a save removed from the old store goes to the new one too: the removal was of the old one', cloudBytes('From iPad.sav') !== null);
  await a.click('#saves-list button[data-remove="From iPad.sav"]');      // removed again, for the second device below
  await waitFor('the removal', () => !s3fake.objects.has(keyOf('From iPad.sav')));
  await a.click('#saves-close');

  // Tactics (tacticsync.js): a tactic uploaded here goes into the account's record in the cloud.
  const tct = (version) => { const t = Buffer.alloc(1476); t.writeUInt32LE(version, 0); t.writeUInt32LE(0xf2fffff2, 1472); return t; };
  const cloudTactics = () => { const o = s3fake.objects.get(`tactics/${PLAYER.sub}.json`); return o ? JSON.parse(Buffer.from(o.body).toString()) : null; };
  await a.click('#saves-open');
  await a.setInputFiles('#tactics-file', [{ name: 'Cloud 4-4-2.tct', mimeType: 'application/octet-stream', buffer: tct(0x98ec5e) }]);
  await waitFor('the tactic in the cloud', () => cloudTactics()?.tactics?.some((t) => t.name === 'Cloud 4-4-2.tct'), 20000);
  check('a tactic uploaded here goes to the cloud, as it is',
    cloudTactics()?.tactics?.some((t) => t.name === 'Cloud 4-4-2.tct' && Buffer.from(t.data, 'base64').equals(tct(0x98ec5e))), JSON.stringify(cloudTactics())?.slice(0, 120));
  await a.click('#saves-close');

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
  const bLog = await b.textContent('#log');
  check('and its log says each one came straight from the store, and how this browser keeps it', /Harbour City 2001-07-14\.sav came from the cloud \([\d.]+ MB, straight from the store, unpacked whole\)/.test(bLog)
    && /the browser's storage keeps references to files: a save is copied by the browser before it is stored/.test(bLog),
    bLog.split('\n').filter((l) => /came from the cloud/.test(l)).join(' / '));
  check('and not the save removed from the cloud', (await localBytes(b, 'From iPad.sav')) === null, (await localNames(b)).join(', '));
  await waitFor('the data set from the cloud', () => b.evaluate(() => { try { return window.Module.FS.readdir('/user/Databases/My squads').length > 20; } catch { return false; } }), 20000);
  const setHere = await b.evaluate(async () => {
    const { DataSet, DATA_SET_FILES } = await import('/dataset.js');
    try { return new DataSet(Object.fromEntries(DATA_SET_FILES.map((f) => [f, window.Module.FS.readFile(`/user/Databases/My squads/${f}`)]))).players().length; }
    catch (e) { return String(e); }
  });
  check('the data set comes to the second device once its game is up', setHere === 3, String(setHere));
  const tacticsHere = () => b.evaluate(() => { try { return window.Module.FS.readdir('/user/tactics').filter((n) => n.endsWith('.tct')); } catch { return []; } });
  await waitFor('the tactic from the cloud', async () => (await tacticsHere()).includes('Cloud 4-4-2.tct'), 20000);
  const tacticBytes = await b.evaluate(() => { try { return Array.from(window.Module.FS.readFile('/user/tactics/Cloud 4-4-2.tct')); } catch { return null; } });
  check('the tactic of the other device comes here', tacticBytes !== null && Buffer.from(tacticBytes).equals(tct(0x98ec5e)), (await tacticsHere()).join(', '));
  // Deleted here: the cloud's record keeps the removal, so the other device's copy goes at its next sync too.
  b.on('dialog', (d) => d.accept());
  await b.tap('#saves-open');
  await b.tap('#saves .tabs [data-tab="tactics"]');
  await b.tap('#tactics-list button[data-tactic-delete="0"]');
  await waitFor('the removal in the cloud', () => cloudTactics()?.removed?.['Cloud 4-4-2.tct'] > 0, 20000);
  check('a tactic deleted here leaves the cloud, with the time of its removal',
    !cloudTactics()?.tactics?.some((t) => t.name === 'Cloud 4-4-2.tct') && cloudTactics()?.removed?.['Cloud 4-4-2.tct'] > 0, JSON.stringify(cloudTactics()));
  await b.tap('#saves-close');
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
  await c.click('#saves-open');
  check('Saved games tells a player without an account about Cloud Saves', await c.isVisible('#saves-promo')
    && /Membership keeps your saves on all your devices too, and its Scout finds players: \$5 a month/.test(await c.textContent('#saves-promo')));
  await c.click('#saves-promo button');
  check('and its button opens the plans', await c.isVisible('#account') && await c.isVisible('#account-body .plans'));
  await c.click('#account-close');
  await c.click('#account-open');
  const [popup] = await Promise.all([c.context().waitForEvent('page'), c.click('#account-signin')]);
  await popup.waitForLoadState().catch(() => {});
  await waitFor('the sign-in in the small window', async () => (await c.textContent('#account-open span')) === 'Alex');
  check('signed in from a small window while the game runs', (await c.textContent('#account-open span')) === 'Alex' && await c.evaluate(() => Boolean(window.__fake)));
  await waitFor('the other device\'s save', async () => same(await localBytes(c, other), elsewhere));
  check('a new save of the cloud came in', same(await localBytes(c, other), elsewhere));
  check('the save of this device was not replaced while the game runs', same(await localBytes(c, harbour), Buffer.from(mine)));
  // Its own Harbour City and the cloud's differ, never synced, and the one here is the newer (its game wrote it at
  // this start): it goes up over the cloud's, and nothing comes down for it (the store keeps the cloud's earlier versions).
  const other2 = 'Harbour City 2001-07-14 (other device) (2).sav';
  await waitFor('the newer one here up', () => cloudBytes(harbour)?.equals(Buffer.from(mine)));
  check('a device that signs in with its own, newer version: it goes up over the cloud\'s, no second copy',
    cloudBytes(harbour)?.equals(Buffer.from(mine)) && same(await localBytes(c, harbour), Buffer.from(mine)) && !cloudBytes(other2)
      && !(await localNames(c)).includes(other2) && cloudBytes(other)?.equals(elsewhere), (await localNames(c)).join(', '));

  // ---- A free account buys Cloud Saves while its game runs: Paddle's checkout, the webhook, the saves go up. ----
  google.user = { sub: '5000005', email: 'free@example.com', email_verified: true, name: 'Free Player' };
  const dctx = await browser.newContext({ viewport: { width: 1200, height: 800 } });
  await dctx.route('https://cdn.paddle.com/paddle/v2/paddle.js', (route) => route.fulfill({ contentType: 'text/javascript', body: FAKE_PADDLE }));
  const d = await dctx.newPage();
  d.on('pageerror', (e) => errors.push(e.message));
  await d.goto(`${BASE}/auth/google/start?mode=page`);
  await d.waitForSelector('#go');
  await waitFor('the account', async () => (await d.textContent('#account-open span')) === 'Free');
  check('the start box of a free account: its plan, and the way to Cloud Saves', /Free plan/.test(await d.textContent('#overlay')) && await d.isVisible('#go-plans'));
  await d.click('#go');
  await d.waitForFunction(() => window.__fake, null, { timeout: 30000 });
  await d.click('#account-open');
  const free = (await d.innerText('#account-body')).replace(/\s+/g, ' ');
  check('a free account sees its plan, the two plans and the price', /Free plan/.test(free) && /Membership \$5 a month/.test(free) && /The Scout/.test(free)
    && (await d.textContent('#account-buy')) === 'Become a member · $5 a month', free.slice(0, 200));
  check('and the refund and renewal terms by the button', /cancel any time\. A full refund within 14 days/.test(free));
  await shot(d, 'free');
  await d.click('#account-buy');
  await d.waitForFunction(() => window.__paddleOpen, null, { timeout: 10000 });
  const opened = await d.evaluate(() => ({ open: window.__paddleOpen, env: window.__paddleEnv, token: window.__paddleInit?.token, dialog: document.getElementById('account').open }));
  check('the checkout opens with the price, the email and this account\'s mark; the dialog gives way to it',
    opened.open.items[0].priceId === 'pri_cloud_month' && opened.open.customer.email === 'free@example.com' && /^5000005\./.test(opened.open.customData.account)
      && opened.env === 'sandbox' && opened.token === 'test_token' && opened.dialog === false, JSON.stringify(opened).slice(0, 300));
  // Paddle's webhook, a moment after the payment.
  await new Promise((r) => setTimeout(r, 1500));
  const raw = JSON.stringify(paddleEvent('subscription.created', { ref: opened.open.customData.account }));
  const hooked = await fetch(`${BASE}/api/paddle/webhook`, { method: 'POST', body: raw, headers: { 'paddle-signature': signPaddle(raw, 'pdl_ntfset_check') } });
  check('the webhook is taken', hooked.status === 200);
  await waitFor('Membership on', async () => /You are a member: Cloud Saves are on/.test(await toasts(d)), 20000);
  check('the page turns Membership on by itself', /Payment done/.test(await toasts(d)) && /You are a member: Cloud Saves are on/.test(await toasts(d)), await toasts(d));
  const paidKey = `saves/5000005/${Buffer.from(harbour).toString('base64url')}.gz`;
  await waitFor('the save to go up', () => s3fake.objects.has(paidKey));
  check('the save of its running game goes up at once', s3fake.objects.has(paidKey));
  await d.click('#account-open');
  const paid = (await d.innerText('#account-body')).replace(/\s+/g, ' ');
  check('Account: Cloud Saves, renews, paid until, Manage subscription', /· Member\b/.test(paid) && /Renews by itself; paid until/.test(paid)
    && await d.isVisible('#account-manage'), paid.slice(0, 220));
  await shot(d, 'paid');
  await d.click('#account-close');

  // ---- An admin gives Cloud Saves to a free account (the admin page), and takes them back. ----
  google.user = { sub: '6000006', email: 'gift@example.com', email_verified: true, name: 'Gift Player' };
  const gctx = await browser.newContext({ viewport: { width: 1200, height: 800 } });
  const g = await gctx.newPage();
  g.on('pageerror', (e) => errors.push(e.message));
  await g.goto(`${BASE}/auth/google/start?mode=page`);
  await g.waitForSelector('#go');
  await waitFor('the account', async () => (await g.textContent('#account-open span')) === 'Gift');
  await g.waitForTimeout(4000);                                       // its first heartbeat: the admin page lists it
  const adm = await a.context().newPage();
  adm.on('dialog', (dlg) => dlg.accept());                           // "Give Squad Player to …?"
  await adm.goto(`${BASE}/admin#people`);
  await adm.waitForSelector('[data-give="g:6000006"][data-give-level="squad"]', { timeout: 15000 });
  const row = () => adm.locator('#people tr', { hasText: 'gift@example.com' }).innerText();
  check('the admin page: a free account has the Give buttons, the admin\'s own says Club Legend · admin',
    /Free/.test(await row()) && (await adm.locator('#people tr', { hasText: 'player@example.com' }).first().innerText()).includes('Club Legend · admin'), (await row()).replace(/\s+/g, ' '));
  await adm.click('[data-give="g:6000006"][data-give-level="squad"]');
  await adm.waitForSelector('[data-take-back="g:6000006"]', { timeout: 10000 });
  check('Give: the row says Squad Player · gift, with Take back', /Squad Player · gift/.test(await row()), (await row()).replace(/\s+/g, ' '));
  await shot(adm, 'admin-given');
  await g.reload();
  await g.waitForSelector('#go');
  await g.click('#account-open');
  const gift = (await g.innerText('#account-body')).replace(/\s+/g, ' ');
  check('the player, at the next page load: Cloud Saves are on, given to this account', /· Member\b/.test(gift) && /Given to this account/.test(gift), gift.slice(0, 200));
  await g.click('#account-close');
  await adm.click('[data-take-back="g:6000006"]');
  await adm.waitForSelector('[data-give="g:6000006"][data-give-level="squad"]', { timeout: 10000 });
  await g.reload();
  await g.waitForSelector('#go');
  await g.click('#account-open');
  check('Take back: the player is on the free plan again', /Free plan/.test(await g.innerText('#account-body')));
  await g.click('#account-close');
  await adm.close();
  await gctx.close();

  // ---- Send to the developer, from Help: the log, a note and a save; the save goes packed straight to the store. ----
  await a.click('#help-open');
  check('Help offers Send to the developer', await a.isVisible('#help-report-open'));
  await a.click('#help-report-open');
  await a.selectOption('#report-save', '/user/Harbour City 2001-07-14.sav');
  await a.fill('#report-note', 'My save does not load.');
  await shot(a, 'report-dialog');
  const sentBytes = await a.evaluate(() => Array.from(window.Module.FS.readFile('/user/Harbour City 2001-07-14.sav')));
  await a.click('#report-send');
  await a.waitForFunction(() => /^Sent|was not|Nothing/.test(document.getElementById('report-state').textContent), null, { timeout: 20000 }).catch(() => {});
  const reportSaid = await a.textContent('#report-state');
  check('Send: the log and the save go, and the dialog says so', /^Sent, with Harbour City 2001-07-14\.sav\. Thank you/.test(reportSaid), reportSaid);
  await a.click('#report-close');
  const adminData = await a.evaluate(async () => (await fetch('/api/admin/stats', { credentials: 'same-origin' })).json());
  const sentReport = adminData.reports.find((r) => r.sent);
  const full = await a.evaluate(async (key) => (await fetch(`/api/admin/report?key=${encodeURIComponent(key)}`, { credentials: 'same-origin' })).json(), sentReport?.key ?? '');
  const got = full.saveUrl ? gunzipSync(Buffer.from(await (await fetch(full.saveUrl)).arrayBuffer())) : Buffer.alloc(0);
  check('the admin data: the report with the note, the account, the log, and the save as it was sent',
    sentReport?.note === 'My save does not load.' && full.id === 'g:4000004' && full.log.length > 10 && full.save?.done === true
      && full.save.rawSize === sentBytes.length && got.equals(Buffer.from(sentBytes)) && full.token === undefined,
    JSON.stringify({ ...full, log: full.log?.length, saveUrl: Boolean(full.saveUrl) }).slice(0, 400));
  // The admin page: the report is marked as sent, and its save downloads unpacked, under its own name.
  const adm2 = await a.context().newPage();
  await adm2.goto(`${BASE}/admin#errors`);
  await adm2.waitForSelector('.tag.sent', { timeout: 10000 }).catch(() => {});
  await adm2.click('.tag.sent + a');
  await adm2.waitForSelector('#report-save-get', { timeout: 10000 }).catch(() => {});
  const [dl] = await Promise.all([adm2.waitForEvent('download', { timeout: 15000 }).catch(() => null), adm2.click('#report-save-get')]);
  const dlPath = dl ? await dl.path() : null;
  const { readFileSync } = await import('node:fs');
  check('the admin page: Sent by the player, the note, and Download the save gives the save itself',
    /Sent by the player/.test(await adm2.innerText('.reports')) && /My save does not load\./.test(await adm2.innerText('#report'))
      && dl?.suggestedFilename() === 'Harbour City 2001-07-14.sav' && Boolean(dlPath) && readFileSync(dlPath).equals(Buffer.from(sentBytes)),
    `${dl?.suggestedFilename()} ${dlPath ? readFileSync(dlPath).length : 'no file'}`);
  await shot(adm2, 'admin-sent-report');
  // A bucket that does not let the admin page in (its CORS rules): the button says to use the packed link.
  s3fake.faults.corsGone = true;
  await adm2.click('#report-save-get');
  await adm2.waitForFunction(() => !document.getElementById('report-save-get').disabled, null, { timeout: 10000 }).catch(() => {});
  const refusedGet = await adm2.textContent('#report-head');
  s3fake.faults.corsGone = false;
  check('the admin page: a bucket that refuses the page, and Download the save points to the packed (.gz) link',
    /use the packed \(\.gz\) link/.test(refusedGet), refusedGet);
  await adm2.close();
  // An upload that is refused (an empty save, as an empty Download made one) offers the same, with that file: it goes
  // as it is, and the admin data says it was empty.
  await a.click('#saves-open');
  await a.setInputFiles('#saves-file', { name: 'St-Etienne 2002-02-13.sav', mimeType: 'application/octet-stream', buffer: Buffer.alloc(0) });
  await a.waitForSelector('#saves-report', { timeout: 10000 }).catch(() => {});
  await a.click('#saves-report');
  await a.click('#report-send');
  await a.waitForFunction(() => /^Sent|was not|Nothing/.test(document.getElementById('report-state').textContent), null, { timeout: 20000 }).catch(() => {});
  const emptySaid = await a.textContent('#report-state');
  const adminData2 = await a.evaluate(async () => (await fetch('/api/admin/stats', { credentials: 'same-origin' })).json());
  const emptyReport = adminData2.reports.find((r) => /the file is empty/.test(r.message));
  const emptyFull = await a.evaluate(async (key) => (await fetch(`/api/admin/report?key=${encodeURIComponent(key)}`, { credentials: 'same-origin' })).json(), emptyReport?.key ?? '');
  const emptyGot = emptyFull.saveUrl ? gunzipSync(Buffer.from(await (await fetch(emptyFull.saveUrl)).arrayBuffer())) : null;
  check('a refused upload: Send to the developer sends that file, and the admin data has it (0 bytes)',
    /^Sent, with St-Etienne 2002-02-13\.sav\./.test(emptySaid) && emptyFull.save?.done === true && emptyFull.save.rawSize === 0 && emptyGot?.length === 0
      && emptyFull.where?.startsWith('Saved games'), `${emptySaid} | ${JSON.stringify(emptyFull.save)}`);
  await a.click('#report-close');
  // A store that does not let the page in (its CORS rules missing or wrong, as on the live site at the first try): the
  // save goes through the server, and arrives all the same.
  s3fake.faults.corsGone = true;
  await a.click('#help-open');
  await a.click('#help-report-open');
  await a.selectOption('#report-save', '/user/Harbour City 2001-07-14.sav');
  const viaBytes = await a.evaluate(() => Array.from(window.Module.FS.readFile('/user/Harbour City 2001-07-14.sav')));
  await a.click('#report-send');
  await a.waitForFunction(() => /^Sent|was not|Nothing/.test(document.getElementById('report-state').textContent), null, { timeout: 20000 }).catch(() => {});
  const viaSaid = await a.textContent('#report-state');
  s3fake.faults.corsGone = false;
  await a.click('#report-close');
  const adminData3 = await a.evaluate(async () => (await fetch('/api/admin/stats', { credentials: 'same-origin' })).json());
  const viaFull = await a.evaluate(async (key) => (await fetch(`/api/admin/report?key=${encodeURIComponent(key)}`, { credentials: 'same-origin' })).json(), adminData3.reports.find((r) => r.sent)?.key ?? '');
  const viaGot = viaFull.saveUrl ? gunzipSync(Buffer.from(await (await fetch(viaFull.saveUrl)).arrayBuffer())) : Buffer.alloc(0);
  check('a store that does not let the page in: the save goes through the server and arrives whole',
    /^Sent, with Harbour City 2001-07-14\.sav\./.test(viaSaid) && /the store did not take the save from this page .* it goes through the server/.test(await a.textContent('#log'))
      && viaFull.save?.done === true && viaGot.equals(Buffer.from(viaBytes)), `${viaSaid} | ${JSON.stringify(viaFull.save)}`);
  // The server could not store the save (the store failed): the log went, and Send the save again sends the save only,
  // with the same report, not a second one.
  s3fake.faults.corsGone = true;
  Object.assign(s3fake.faults, { failPuts: 5, failMatch: /\.sav\.gz$/ });
  await a.click('#help-open');
  await a.click('#help-report-open');
  await a.selectOption('#report-save', '/user/Harbour City 2001-07-14.sav');
  await a.click('#report-send');
  await a.waitForFunction(() => /^Sent|was not|Nothing/.test(document.getElementById('report-state').textContent), null, { timeout: 20000 }).catch(() => {});
  const failedOnce = await a.textContent('#report-state'), againLabel = await a.textContent('#report-send');
  Object.assign(s3fake.faults, { failPuts: 0, failMatch: /./ });
  const sentBefore = (await a.evaluate(async () => (await fetch('/api/admin/stats', { credentials: 'same-origin' })).json())).reports.filter((r) => r.sent).length;
  await a.click('#report-send', { timeout: 5000 }).catch(() => {});
  await a.waitForFunction(() => /^Sent/.test(document.getElementById('report-state').textContent), null, { timeout: 20000 }).catch(() => {});
  const retried = await a.textContent('#report-state');
  const afterRetry = (await a.evaluate(async () => (await fetch('/api/admin/stats', { credentials: 'same-origin' })).json())).reports.filter((r) => r.sent);
  s3fake.faults.corsGone = false;
  await a.click('#report-close');
  check('a save the server could not store: Send the save again sends it, with the same report',
    /The log was sent, but Harbour City 2001-07-14\.sav was not/.test(failedOnce) && againLabel === 'Send the save again'
      && /^Sent, with Harbour City 2001-07-14\.sav\./.test(retried) && afterRetry.length === sentBefore && afterRetry[0].save?.name === 'Harbour City 2001-07-14.sav',
    `${failedOnce} | ${againLabel} | ${retried} | ${afterRetry.length} vs ${sentBefore}`);

  // ---- A save too large for the cloud (a player's log, 15 November 2026: "Hertha BSC 2011-12-11.sav up: HTTP 413"):
  // the 413 stopped the whole sync, so the saves after it never went up, and it was packed and sent again each minute.
  // This server takes 2 MB packed (maxPacked): the big save is not sent at all, the others go, Saved games says so. ----
  const bigRequests = [];
  a.on('request', (r) => { if (/Big%202011/.test(r.url()) && r.method() !== 'GET') bigRequests.push(`${r.method()} ${r.url().slice(-40)}`); });
  await a.evaluate(async () => {
    const names = (await (await fetch('/api/saves')).json()).saves.map((s) => s.name).filter((n) => !/^Harbour City/.test(n));
    for (const n of names.slice(0, Math.max(0, names.length - 3))) await fetch(`/api/saves/${encodeURIComponent(n)}`, { method: 'DELETE', headers: { 'x-cm': '1' } });
    const FS = window.Module.FS;
    const save = (n) => { const s = new Uint8Array(0x416ec + n), dv = new DataView(s.buffer); dv.setInt32(0, 2, true); dv.setInt32(8, 1, true); dv.setUint32(12, 0x416ec, true); return s; };
    FS.writeFile('/user/Small after big.sav', save(1000));
    const big = save(3 << 20);
    for (let i = 0x416ec; i < big.length; i += 65536) crypto.getRandomValues(big.subarray(i, Math.min(i + 65536, big.length)));
    FS.writeFile('/user/Big 2011-12-11.sav', big);                 // the newest: the first of the sync's jobs
  });
  const inCloud = async (name) => (await a.evaluate(async () => (await (await fetch('/api/saves')).json()).saves.map((s) => s.name))).includes(name);
  // The message shows for 12 seconds: it is read when it comes (read at the end, it had sometimes gone already).
  let bigToast = '';
  await waitFor('the too large message', async () => { const t = await toasts(a); if (/too large for Cloud Saves/.test(t)) bigToast = t; return Boolean(bigToast); }, 30000);
  await waitFor('the small save in the cloud', () => inCloud('Small after big.sav'), 30000);
  await a.evaluate(() => { const s = new Uint8Array(0x416ec + 500), dv = new DataView(s.buffer); dv.setInt32(0, 2, true); dv.setInt32(8, 1, true); dv.setUint32(12, 0x416ec, true); window.Module.FS.writeFile('/user/Another after big.sav', s); });
  await waitFor('the next save in the cloud', () => inCloud('Another after big.sav'), 30000);
  await a.click('#saves-open');
  const bigRow = (await a.locator('#saves-list tr', { hasText: 'Big 2011-12-11' }).innerText()).replace(/\s+/g, ' ');
  await a.click('#saves-close');
  check('a save too large for the cloud: the others still go up, it is not sent, and Saved games and a message say so',
    await inCloud('Small after big.sav') && await inCloud('Another after big.sav') && !(await inCloud('Big 2011-12-11.sav'))
      && bigRequests.length === 0 && /Too large for the cloud/.test(bigRow) && /Big 2011-12-11\.sav is too large for Cloud Saves/.test(bigToast),
    `${bigRow} | requests: ${bigRequests.join(', ')} | ${bigToast}`);

  // ---- Sold as a Buy Me a Coffee membership (another server of the same store): join in another tab, the webhook,
  // Cloud Saves turn on in the running game; the account dialog points to the membership. ----
  const coffeeCloud = createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'z'.repeat(40),
    maxSaves: 10, maxPacked: 2 << 20, google: google.google, log: (t) => logs.push(t), price: '$5 a month', storeTtl: 0,
    coffee: { secret: 'bmc_check', url: 'https://buymeacoffee.com/someone/membership', levels: [] } });
  const coffeeServer = createApp({ gameDir: game, buildDir: build, manifestPath: join(tmp, 'manifest.tsv'), cloud: coffeeCloud });
  await new Promise((r) => coffeeServer.listen(0, '127.0.0.1', r));
  const CBASE = `http://127.0.0.1:${coffeeServer.address().port}`;
  try {
    google.user = { sub: '8000008', email: 'member@example.com', email_verified: true, name: 'Member Player' };
    const mctx = await browser.newContext({ viewport: { width: 1200, height: 800 } });
    await mctx.route('https://buymeacoffee.com/**', (route) => route.fulfill({ contentType: 'text/html', body: '<title>membership</title>' }));
    const m = await mctx.newPage();
    m.on('pageerror', (e) => errors.push(e.message));
    await m.goto(`${CBASE}/auth/google/start?mode=page`);
    await m.waitForSelector('#go');
    await waitFor('the account', async () => (await m.textContent('#account-open span')) === 'Member');
    await m.click('#go');
    await m.waitForFunction(() => window.__fake, null, { timeout: 30000 });
    await m.click('#account-open');
    const offerText = (await m.innerText('#account-body')).replace(/\s+/g, ' ');
    check('membership: the button, the price and the email to join with', (await m.textContent('#account-buy')) === 'Become a member · $5 a month'
      && /become a member on Buy Me a Coffee: join with this account's email, member@example\.com/i.test(offerText) && !/Paddle/.test(offerText), offerText.slice(0, 300));
    await shot(m, 'member-offer');
    const [tab] = await Promise.all([mctx.waitForEvent('page'), m.click('#account-buy')]);
    check('the membership page opens in another tab', tab.url() === 'https://buymeacoffee.com/someone/membership', tab.url());
    const raw = JSON.stringify(coffeeEvent('membership.started', { email: 'member@example.com', id: 880 }));
    const hooked = await fetch(`${CBASE}/api/coffee/webhook`, { method: 'POST', body: raw, headers: { 'x-signature-sha256': signCoffee(raw, 'bmc_check') } });
    check('the membership webhook is taken', hooked.status === 200);
    await waitFor('Cloud Saves on', async () => /Thank you for joining/.test(await toasts(m)), 20000);
    check('the page turns Cloud Saves on by itself after joining', /Thank you for joining/.test(await toasts(m)), await toasts(m));
    const memberKeyOf = `saves/8000008/${Buffer.from(harbour).toString('base64url')}.gz`;
    await waitFor('the save to go up', () => s3fake.objects.has(memberKeyOf));
    check('a member\'s running game: its save goes up', s3fake.objects.has(memberKeyOf));
    if (!(await m.evaluate(() => document.getElementById('account').open))) await m.click('#account-open');
    const memberText = (await m.innerText('#account-body')).replace(/\s+/g, ' ');
    check('Account of a member: Cloud Saves, renews, Your membership', /· Member\b/.test(memberText) && /Renews by itself; paid until/.test(memberText)
      && await m.isVisible('#account-member') && !(await m.isVisible('#account-manage').catch(() => false)), memberText.slice(0, 260));
    await shot(m, 'member');
    await mctx.close();
  } finally { coffeeServer.close(); }

  // ---- The store's configured address refuses the page (as fly.storage.tigris.dev did, 17 November 2026) and another
  // address of the same bucket lets it in (as t3.storage.dev): the server finds it at its start, and a save of the
  // game goes from the browser straight to the store at that address. ----
  const freePort = await new Promise((r) => { const t = net.createServer().listen(0, '127.0.0.1', () => { const { port } = t.address(); t.close(() => r(port)); }); });
  const LBASE = `http://127.0.0.1:${freePort}`, fakePort = new URL(s3fake.endpoint).port;
  const storeAt = (host) => createS3({ endpoint: `http://${host}:${fakePort}`, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  s3fake.faults.corsHost = '127.0.0.1';                 // only the address 127.0.0.1 lets pages in; localhost does not
  const wayCloud = createCloud({ s3: storeAt('localhost'), clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'z'.repeat(40),
    cloudFor: 'everyone', maxSaves: 10, maxPacked: 2 << 20, google: google.google, log: (t) => logs.push(t), storeTtl: 0, origins: [LBASE],
    linkStores: [{ s3: storeAt('127.0.0.1'), name: 'the second address' }], recheck: [] });
  const wayServer = createApp({ gameDir: game, buildDir: build, manifestPath: join(tmp, 'manifest.tsv'), cloud: wayCloud });
  await new Promise((r) => wayServer.listen(freePort, '127.0.0.1', r));
  try {
    check('the configured address refused, another passed: the server takes it', await wayCloud.directReady === true && wayCloud.linkName() === 'the second address'
      && logs.some((l) => /the links name the second address, in place of the configured address, which did not pass/.test(l)), logs.slice(-3).join(' | '));
    google.user = { sub: '9000009', email: 'way@example.com', email_verified: true, name: 'Way Player' };
    const wctx = await browser.newContext({ viewport: { width: 1200, height: 800 } });
    const w = await wctx.newPage();
    w.on('pageerror', (e) => errors.push(e.message));
    const storeRequests = [];
    w.on('request', (r) => { if (r.url().includes(`:${fakePort}/`)) storeRequests.push(`${r.method()} ${new URL(r.url()).host}`); });
    await w.goto(`${LBASE}/auth/google/start?mode=page`);
    await w.waitForSelector('#go');
    await w.click('#go');
    await w.waitForFunction(() => window.__fake, null, { timeout: 30000 });
    const wayName = 'Second Way 2001-09-01.sav', wayBytes = fakeSaveBytes(61);
    await w.evaluate(([name, bytes]) => window.Module.FS.writeFile(`/user/${name}`, new Uint8Array(bytes)), [wayName, Array.from(wayBytes)]);
    const wayKey = `saves/9000009/${Buffer.from(wayName).toString('base64url')}.gz`;
    await waitFor('the save to go up the second way', () => s3fake.objects.has(wayKey), 30000);
    const wayLog = await w.textContent('#log');
    check('a save goes from the browser straight to the store at the address that passed, not through the server',
      s3fake.objects.has(wayKey) && gunzipSync(s3fake.objects.get(wayKey).body).equals(wayBytes)
        && storeRequests.some((r) => r === `PUT 127.0.0.1:${fakePort}`) && !storeRequests.some((r) => r.includes('localhost'))
        && !/did not answer this page directly/.test(wayLog) && /Second Way 2001-09-01\.sav is in the cloud \([\d.]+ MB compressed, straight to the store\)/.test(wayLog), `${storeRequests.join(', ')} | ${wayLog.split('\n').filter((l) => /cloud|direct/.test(l)).slice(-4).join(' / ')}`);
    await wctx.close();
  } finally { s3fake.faults.corsHost = null; wayServer.close(); }

  // ---- A device whose tab the system closes while a cloud save comes at the start (an iPhone, 18 November 2026: a
  // 446 MB save, closed at every start, then "A problem repeatedly occurred"). The next start says so, leaves that save
  // in the cloud on this device and starts the game; Account brings it with "Get it here". ----
  {
    google.user = PLAYER;
    const kctx = await browser.newContext({ viewport: { width: 1200, height: 800 } });
    const k = await kctx.newPage();
    k.on('pageerror', (e) => errors.push(e.message));
    const held = 'Fine 2001-08-02.sav', heldB64 = Buffer.from(held).toString('base64url');   // one that only the cloud has (the stand-in game makes Harbour City)
    const isHeld = (url) => { const u = new URL(url); return u.pathname.endsWith(`${heldB64}.gz`) || u.pathname === `/api/saves/${encodeURIComponent(held)}`; };
    await kctx.route(isHeld, () => {});           // never answered: the tab is closed while it comes
    await k.goto(`${BASE}/auth/google/start?mode=page`);
    await k.waitForSelector('#go');
    await k.click('#go');
    const waiting = await waitFor('the start to get Fine', async () => (await k.textContent('#overlay').catch(() => '')).includes(`Getting ${held}`), 30000);
    const mark = await k.evaluate(() => JSON.parse(localStorage.getItem('cm0102-visit') ?? 'null'));
    const crashed = k.waitForEvent('crash', { timeout: 15000 }).then(() => true, () => false);
    (await kctx.newCDPSession(k)).send('Page.crash').catch(() => {});
    const didCrash = await crashed;
    await k.close().catch(() => {});
    const k2 = await kctx.newPage();
    k2.on('pageerror', (e) => errors.push(e.message));
    k2.on('dialog', (d) => d.accept());
    await k2.goto(`${BASE}/`);
    await k2.waitForSelector('#go', { timeout: 30000 });
    const box = (await k2.textContent('#overlay')).replace(/\s+/g, ' ');
    check('the tab closed while a cloud save came: the next start says which, and that it stays in the cloud here',
      waiting && didCrash && mark?.cloudFile === held && /stopped suddenly on your last visit/.test(box)
        && box.includes(`${held} stays in the cloud on this device for now, so the game can start`), `${JSON.stringify(mark)} | ${box.slice(0, 300)}`);
    await k2.click('#go');
    await k2.waitForFunction(() => window.__fake, null, { timeout: 30000 });
    await k2.waitForTimeout(1500);
    const others = await localNames(k2);
    check('and the game starts with the other cloud saves, without that one',
      !others.includes(held) && others.length > 0 && /stays in the cloud on this device/.test(await k2.textContent('#log')), others.join(', '));
    await kctx.unroute(isHeld);
    await k2.click('#account-open');
    const acct = (await k2.innerText('#account-body')).replace(/\s+/g, ' ');
    const offered = acct.includes(held) && await k2.isVisible('button[data-kept]');
    await k2.click('button[data-kept]');
    await waitFor('Fine to come', async () => (await localNames(k2)).includes(held), 30000);
    check('Account lists it with Get it here, which brings it to this device', offered && same(await localBytes(k2, held), cloudBytes(held))
      && !(await k2.innerText('#account-body')).includes('Left in the cloud on this device'), acct.slice(0, 300));
    await kctx.close();
  }

  // ---- The database library: the admin adds a database on the admin page; a player gets it in Saved games, Data sets,
  // Get a database, and Use opens with its season. It is the library's: no copy goes to the player's cloud. ----
  {
    google.user = PLAYER;                                        // an admin (isAdmin) with Cloud Saves
    const lctx = await browser.newContext({ viewport: { width: 1280, height: 860 } });
    const adminPage = await lctx.newPage();
    adminPage.on('pageerror', (e) => errors.push(e.message));
    await adminPage.goto(`${BASE}/auth/google/start?mode=page`);
    await adminPage.waitForSelector('#go');
    await adminPage.goto(`${BASE}/admin#library`);
    await adminPage.waitForSelector('#library:not([hidden]) #lib-file', { timeout: 20000 });
    const dbZip = Buffer.from(await writeZip(Object.entries(fakeDataSet()).map(([name, data]) => ({ name: `Update 2011/${name}`, data }))));
    await adminPage.setInputFiles('#lib-file', { name: 'Update 2011.zip', mimeType: 'application/zip', buffer: dbZip });
    const named = await adminPage.inputValue('#lib-name');
    await adminPage.selectOption('#lib-season', '2011');
    await adminPage.fill('#lib-note', 'Squads of December 2011');
    await adminPage.fill('#lib-early', '');                          // for everyone at once (early-check covers the day)
    await adminPage.click('#lib-add');
    await waitFor('the library entry', async () => /Update 2011/.test(await adminPage.textContent('#library table').catch(() => '')), 20000);
    const adminRow = (await adminPage.textContent('#library table').catch(() => '')).replace(/\s+/g, ' ');
    check('the admin page: a zip, its name from the file, a season and a note go into the library',
      named === 'Update 2011' && ['Update 2011', '2011/12', 'Squads of December 2011'].every((t) => adminRow.includes(t)), adminRow.slice(0, 200));
    // A zip that is not a data set: refused, and said so.
    await adminPage.setInputFiles('#lib-file', { name: 'notes.zip', mimeType: 'application/zip', buffer: Buffer.from(await writeZip([{ name: 'a.txt', data: new Uint8Array(3) }])) });
    await adminPage.click('#lib-add');
    await waitFor('the refusal', async () => /Not added/.test(await adminPage.textContent('#lib-msg')), 20000);
    check('a zip that is not a data set is refused on the admin page', /not a data set: it does not hold the 20 database files/.test(await adminPage.textContent('#lib-msg')),
      await adminPage.textContent('#lib-msg'));
    const p = await lctx.newPage();
    p.on('pageerror', (e) => errors.push(e.message));
    await p.goto(`${BASE}/`);
    await p.waitForSelector('#go');
    await p.click('#go');
    await p.waitForFunction(() => window.__fake, null, { timeout: 30000 });
    await p.click('#saves-open');
    await p.click('#saves .tabs [data-tab="sets"]');
    // The tabs by the keys (WAI-ARIA tabs): Left, Home and End move the choice and the focus; one tab in the Tab order.
    const keys = [];
    for (const k of ['ArrowLeft', 'Home', 'ArrowLeft', 'End']) {
      await p.keyboard.press(k);
      keys.push(await p.evaluate(() => [document.getElementById('saves').dataset.tab, document.activeElement?.dataset.tab,
        [...document.querySelectorAll('#saves .tabs [data-tab]')].filter((b) => b.tabIndex === 0).length].join('/')));
    }
    check('the tabs of Saved games go by the arrow keys, Home and End, with one of them in the Tab order',
      keys.join(' ') === 'tactics/tactics/1 saves/saves/1 sets/sets/1 sets/sets/1', keys.join(' '));
    await p.waitForSelector('#sets-lib button[data-lib-get]', { timeout: 20000 });
    check('the Databases tab names its two lists with headings one level under the dialog\'s',
      await p.evaluate(() => [...document.querySelectorAll('#sets h3.sets-head')].filter((h) => h.offsetParent).length) === 2);
    const offer = (await p.textContent('#sets-lib')).replace(/\s+/g, ' ');
    await p.click('#sets-lib button[data-lib-get]');
    await waitFor('the database to come', async () => /Update 2011 is here/.test(await p.textContent('#sets-note')), 30000);
    const got = await p.evaluate(() => {
      const FS = window.Module.FS, dir = '/user/Databases/Update 2011';
      let files = [];
      try { files = FS.readdir(dir); } catch { /* none */ }
      return { files: files.filter((f) => f.endsWith('.dat')).length, meta: (() => { try { return JSON.parse(FS.readFile(`${dir}/.web.json`, { encoding: 'utf8' })); } catch { return null; } })(),
        season: document.getElementById('sets-season')?.value ?? null,
        cloud: [...document.querySelectorAll('#sets-list tr')].find((tr) => tr.cells[0]?.textContent === 'Update 2011')?.querySelector('td.cloud')?.textContent ?? null };
    });
    check('Get a database lists it with its season, size and note; Get brings it, with its season, and Use opens with that season',
      ['Update 2011', 'Squads of December 2011', '2011/12'].every((t) => offer.includes(t)) && got.files === 20 && got.meta?.season === 2011 && Boolean(got.meta?.library?.id)
        && got.season === '2011' && got.cloud === 'From the library', `${offer.slice(0, 160)} | ${JSON.stringify(got).slice(0, 300)}`);
    await p.waitForTimeout(2500);                                 // a set sync runs after a data set came
    const inCloud = [...s3fake.objects.keys()].filter((k) => k.startsWith(`sets/${PLAYER.sub}/`) && k.includes(Buffer.from('Update 2011').toString('base64url')));
    check('a database from the library takes none of the player\'s places in the cloud', inCloud.length === 0, inCloud.join(', '));
    // Aligned: the library's Get sits at the right edge, as the data sets' buttons do; text and buttons share a line.
    const lined = await p.evaluate(() => {
      const right = (sel) => document.querySelector(sel)?.getBoundingClientRect().right;
      const row = document.querySelector('#sets-lib tr:nth-child(2)'), name = row.cells[0].getBoundingClientRect(), get = row.querySelector('button').getBoundingClientRect();
      return { get: right('#sets-lib button[data-lib-get]'), del: right('#sets-list button[data-set-delete]'), head: !document.getElementById('sets-lib-head').hidden,
        again: row.querySelector('button').textContent, middle: Math.abs((name.top + name.bottom) / 2 - (get.top + get.bottom) / 2) };
    });
    check('the library shows by itself under the data sets, its Get lined up with their buttons and its text with its button',
      lined.head && Math.abs(lined.get - lined.del) < 1.5 && lined.middle < 4 && lined.again === 'Get again', JSON.stringify(lined));
    await shot(p, 'databases');
    // A library of many databases on a laptop's window (a player's screenshot, 19 November 2026: the library's list took
    // the room, and the player's own data set and its Use panel were squeezed to a strip that scrolled unseen). The
    // player's own part shows whole, the library's list gets what is left, three rows at least, and scrolls.
    const added = await adminPage.evaluate(async (zip) => {
      const body = new Uint8Array(zip), h = { 'content-type': 'application/json', 'x-cm': '1' }, out = [];
      for (let y = 2012; y <= 2018; y++) {
        const start = await (await fetch('/api/admin/library', { method: 'POST', headers: h, body: JSON.stringify({ size: body.length }) })).json();
        const put = await fetch(`/api/admin/library/${start.id}/body`, { method: 'PUT', headers: { 'content-type': 'application/zip', 'x-cm': '1' }, body });
        const done = await fetch(`/api/admin/library/${start.id}/done`, { method: 'POST', headers: h, body: JSON.stringify({ name: `October ${y} Data Update`, season: y }) });
        out.push(`${put.status}/${done.status}`);
      }
      return out.join(' ');
    }, Array.from(dbZip));
    await p.setViewportSize({ width: 1470, height: 790 });
    await p.click('#saves-close');
    await p.evaluate(() => { const real = Date.now; Date.now = () => real() + 61000; });     // the list is asked again after a minute
    await p.click('#saves-open');
    await p.click('#saves .tabs [data-tab="sets"]');
    const eight = await waitFor('the eight databases', async () => (await p.locator('#sets-lib button[data-lib-get]').count()) === 8, 20000);
    if (!eight) console.log(`  (the library: added ${added}; listed ${await p.locator('#sets-lib button[data-lib-get]').count()})`);
    if (!(await p.isVisible('#sets-season'))) await p.click('#sets-list tr:has-text("Update 2011") button[data-set-use]');   // its Use panel, open
    await p.waitForSelector('#sets-season');
    const room = await p.evaluate(() => {
      const own = document.getElementById('sets-list'), lib = document.getElementById('sets-lib'), body = own.closest('.dlg-body');
      const b = body.getBoundingClientRect(), l = lib.getBoundingClientRect(), use = own.querySelector('tr.use').getBoundingClientRect();
      const rows = [...lib.querySelectorAll('tr')].slice(1).filter((tr) => { const r = tr.getBoundingClientRect(); return r.top >= l.top - 1 && r.bottom <= l.bottom + 1; }).length;
      const close = document.getElementById('saves-close').getBoundingClientRect();
      return { ownWhole: own.scrollHeight <= own.clientHeight + 1, useInView: use.top >= b.top && use.bottom <= b.bottom + 1, libRows: rows,
        libScrolls: lib.scrollHeight > lib.clientHeight + 1, bodyFits: body.scrollHeight <= body.clientHeight + 1, over: body.scrollHeight - body.clientHeight,
        stepsOneLine: new Set([...document.querySelectorAll('#sets .steps li')].map((li) => Math.round(li.getBoundingClientRect().top))).size === 1, closeInView: close.bottom <= innerHeight,
        head: document.getElementById('sets-lib-head').textContent, step: document.getElementById('sets-step1').textContent,
        ownAboveLib: own.getBoundingClientRect().bottom <= l.top };
    });
    check('many databases on a laptop: the player\'s own data sets and the Use panel show whole, the library below them takes the room that is left and scrolls, and nothing else does',
      room.ownWhole && room.useInView && room.libRows >= 2 && room.libScrolls && room.bodyFits && room.closeInView && room.ownAboveLib && room.stepsOneLine
        && /8 databases, free/.test(room.head) && /Get a database below/.test(room.step), JSON.stringify(room));
    await shot(p, 'databases-many');
    // On a phone held upright (the screens review, 19 November 2026): each row a card, every button of every list on
    // the screen (the table's columns once hid Play, Download and Get past the right edge), and one scroll, the dialog's.
    await p.setViewportSize({ width: 390, height: 844 });
    await p.waitForTimeout(300);
    const phone = await p.evaluate(() => {
      const rows = [...document.querySelectorAll('#sets-lib tr, #sets-list tr')].filter((tr) => tr.querySelector('button'));
      const buttons = rows.flatMap((tr) => [...tr.querySelectorAll('button')]);
      const right = Math.max(...buttons.map((b) => b.getBoundingClientRect().right));
      const lists = [...document.querySelectorAll('#sets .dlg-list')].map((l) => l.scrollHeight - l.clientHeight);
      const libRows = [...document.querySelectorAll('#sets-lib tr')].filter((tr) => tr.querySelector('button'));
      return { rows: rows.length, right, width: innerWidth, listsScroll: lists.some((d) => d > 1),
        cards: libRows.every((tr) => tr.querySelector('.lib-card')), getBeside: libRows.every((tr) => {
          const g = tr.querySelector('[data-lib-get]').getBoundingClientRect(), size = tr.querySelector('td.num').getBoundingClientRect();
          return Math.abs((g.top + g.bottom) / 2 - (size.top + size.bottom) / 2) < 12; }),
        headHidden: getComputedStyle(document.querySelector('#sets-lib tr')).display === 'none' };
    });
    check('a phone: every list\'s buttons on the screen, each row a card (a library one with its picture or season box, Get beside its size), and only the dialog scrolls',
      phone.rows >= 9 && phone.right <= phone.width && !phone.listsScroll && phone.headHidden && phone.cards && phone.getBeside, JSON.stringify(phone));
    await shot(p, 'databases-phone');
    await lctx.close();
  }

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
