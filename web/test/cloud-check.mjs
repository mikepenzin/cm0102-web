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
import { createStats, statsStore } from '../stats.mjs';
import { createS3 } from '../s3.mjs';
import { fakeDataSet, fakeGoogle, fakeS3, fakeSaveBytes, paddleEvent, signPaddle } from './fakes.mjs';
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
  cloudUsers: ['player@example.com'], maxSaves: 10, google: google.google, log: (t) => logs.push(t), price: '$5 a month',
  isAdmin: (email) => email === 'player@example.com', storeTtl: 0,
  paddle: { env: 'sandbox', clientToken: 'test_token', priceIds: ['pri_cloud_month'], webhookSecret: 'pdl_ntfset_check', apiKey: null, apiBase: null } });
// Paddle.js, as far as the page uses it: Initialize, and a checkout that completes at once. The test then sends the
// webhook that Paddle would send.
const FAKE_PADDLE = `window.Paddle = {
  Environment: { set(e) { window.__paddleEnv = e; } },
  Initialize(o) { window.__paddleInit = o; },
  Checkout: { open(o) { window.__paddleOpen = o; setTimeout(() => window.__paddleInit.eventCallback({ name: 'checkout.completed', data: {} }), 200); } },
};`;
const server = createApp({ gameDir: game, buildDir: build, manifestPath: join(tmp, 'manifest.tsv'), cloud, stats: createStats({ store: statsStore(s3) }), statsMode: 'optout',
  admins: ['player@example.com'] });
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
  const startBox = (await a.innerText('#overlay')).replace(/\s+/g, ' ');
  check('the start box says the game is free, and Cloud Saves an optional add-on at $5 a month',
    /Start the game Free · no account, no payment/.test(startBox) && /Optional: Cloud Saves · \$5 a month/.test(startBox) && !(await a.$('#overlay .plans')), startBox.slice(0, 300));
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
  check('a save removed from the old store goes to the new one too: the removal was of the old one', cloudBytes('From iPad.sav') !== null);
  await a.click('#saves-list button[data-remove="From iPad.sav"]');      // removed again, for the second device below
  await waitFor('the removal', () => !s3fake.objects.has(keyOf('From iPad.sav')));
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
  check('and not the save removed from the cloud', (await localBytes(b, 'From iPad.sav')) === null, (await localNames(b)).join(', '));
  await waitFor('the data set from the cloud', () => b.evaluate(() => { try { return window.Module.FS.readdir('/user/Databases/My squads').length > 20; } catch { return false; } }), 20000);
  const setHere = await b.evaluate(async () => {
    const { DataSet, DATA_SET_FILES } = await import('/dataset.js');
    try { return new DataSet(Object.fromEntries(DATA_SET_FILES.map((f) => [f, window.Module.FS.readFile(`/user/Databases/My squads/${f}`)]))).players().length; }
    catch (e) { return String(e); }
  });
  check('the data set comes to the second device once its game is up', setHere === 3, String(setHere));
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
    && /Cloud Saves keep your saves on all your devices too: \$5 a month/.test(await c.textContent('#saves-promo')));
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
  // Its own Harbour City and the cloud's differ, never synced: kept twice. The cloud's comes in under a name that is
  // free in both places ("(other device)" is taken in the cloud already).
  const other2 = 'Harbour City 2001-07-14 (other device) (2).sav';
  await waitFor('both kept', () => cloudBytes(harbour)?.equals(Buffer.from(mine)) && cloudBytes(other2)?.equals(here));
  check('a device that signs in with its own version: both kept, under free names',
    same(await localBytes(c, other2), here) && cloudBytes(other2)?.equals(here) && cloudBytes(harbour)?.equals(Buffer.from(mine))
      && cloudBytes(other)?.equals(elsewhere), (await localNames(c)).join(', '));

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
  check('a free account sees its plan, the two plans and the price', /Free plan/.test(free) && /Cloud Saves \$5 a month/.test(free)
    && (await d.textContent('#account-buy')) === 'Get Cloud Saves · $5 a month', free.slice(0, 200));
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
  await waitFor('Cloud Saves on', async () => /Cloud Saves are on\. Your saves go to the cloud/.test(await toasts(d)), 20000);
  check('the page turns Cloud Saves on by itself', /Payment done/.test(await toasts(d)) && /Cloud Saves are on\. Your saves go to the cloud/.test(await toasts(d)), await toasts(d));
  const paidKey = `saves/5000005/${Buffer.from(harbour).toString('base64url')}.gz`;
  await waitFor('the save to go up', () => s3fake.objects.has(paidKey));
  check('the save of its running game goes up at once', s3fake.objects.has(paidKey));
  await d.click('#account-open');
  const paid = (await d.innerText('#account-body')).replace(/\s+/g, ' ');
  check('Account: Cloud Saves, renews, paid until, Manage subscription', /· Cloud Saves/.test(paid) && /Renews every month; paid until/.test(paid)
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
  adm.on('dialog', (dlg) => dlg.accept());                           // "Give Cloud Saves to …?"
  await adm.goto(`${BASE}/admin`);
  await adm.waitForSelector('[data-give="g:6000006"]', { timeout: 15000 });
  const row = () => adm.locator('#people tr', { hasText: 'gift@example.com' }).innerText();
  check('the admin page: a free account has a Give button, the admin\'s own (server list) none',
    /free/.test(await row()) && (await adm.locator('#people tr', { hasText: 'player@example.com' }).first().innerText()).includes('given (server list)'), (await row()).replace(/\s+/g, ' '));
  await adm.click('[data-give="g:6000006"]');
  await adm.waitForSelector('[data-give="g:6000006"][data-on="0"]', { timeout: 10000 });
  check('Give: the row says given by an admin, with Take back', /given by an admin/.test(await row()), (await row()).replace(/\s+/g, ' '));
  await shot(adm, 'admin-given');
  await g.reload();
  await g.waitForSelector('#go');
  await g.click('#account-open');
  const gift = (await g.innerText('#account-body')).replace(/\s+/g, ' ');
  check('the player, at the next page load: Cloud Saves are on, given to this account', /· Cloud Saves/.test(gift) && /Given to this account/.test(gift), gift.slice(0, 200));
  await g.click('#account-close');
  await adm.click('[data-give="g:6000006"]');
  await adm.waitForSelector('[data-give="g:6000006"][data-on="1"]', { timeout: 10000 });
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
  await adm2.goto(`${BASE}/admin`);
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
  await adm2.close();

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
