// Tests of early.mjs (early access: Club Legends get a new feature until its day) and of its routes.
// Run: cd web && npm test
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { addDays, createEarly, isEarly, todayUTC, validDay } from '../early.mjs';
import { createCloud } from '../cloud.mjs';
import { createS3 } from '../s3.mjs';
import { createStats } from '../stats.mjs';
import { createApp } from '../server.mjs';
import { fakeGoogle, fakeS3 } from './fakes.mjs';

const ADMIN = 'an-admin-token-of-16-chars';
const NOV23 = Date.parse('2026-11-23T15:23:00Z');
let s3fake, s3, google, tmp;
before(async () => {
  s3fake = await fakeS3();
  google = await fakeGoogle();
  s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  tmp = mkdtempSync(join(tmpdir(), 'cm0102-early-test-'));
  mkdirSync(join(tmp, 'game'));
  writeFileSync(join(tmp, 'manifest.tsv'), '');
});
after(() => { s3fake.close(); google.close(); rmSync(tmp, { recursive: true, force: true }); });

test('a day opens at 00:00 UTC', () => {
  assert.equal(todayUTC(NOV23), '2026-11-23');
  assert.equal(addDays('2026-11-23', 14), '2026-12-07');
  assert.equal(addDays('2026-12-25', 10), '2027-01-04');
  assert.equal(isEarly('2026-12-07', Date.parse('2026-12-06T23:59:59Z')), true);
  assert.equal(isEarly('2026-12-07', Date.parse('2026-12-07T00:00:00Z')), false);
  assert.equal(isEarly(null), false);
  assert.equal(validDay('2026-12-07'), true);
  for (const bad of ['7 Dec', '2026-13-01', '2026-02-30', '', null, 20261207]) assert.equal(validDay(bad), false, String(bad));
});

test('a feature first seen gets 14 days, written once; set changes the day or opens it now', async () => {
  const features = [{ key: 'x', name: 'X' }];
  const a = createEarly({ s3, features, now: () => NOV23 });
  assert.deepEqual(await a.dates(), { x: '2026-12-07' });
  const b = createEarly({ s3, features, now: () => NOV23 + 86400e3 });       // the next day, another machine
  assert.deepEqual(await b.dates(), { x: '2026-12-07' });
  assert.deepEqual(await b.early(), { x: '2026-12-07' });
  await b.set('x', '2027-01-01');
  assert.deepEqual(await b.early(), { x: '2027-01-01' });
  await b.set('x', null);
  assert.deepEqual(await b.early(), {}, 'open now: today');
  assert.equal((await b.dates()).x, '2026-11-24');
  await assert.rejects(b.set('nope', null), /no such feature/);
  await assert.rejects(b.set('x', 'soon'), /a day/);
});

test('a failed read writes nothing, and the feature counts as open', async () => {
  let puts = 0;
  const broken = { getBuffer: async () => { throw new Error('store down'); }, put: async () => { puts++; } };
  const e = createEarly({ s3: broken, features: [{ key: 'y', name: 'Y' }], now: () => NOV23, log: () => {} });
  assert.deepEqual(await e.dates(), {});
  assert.deepEqual(await e.early(), {});
  assert.equal(puts, 0);
});

test('/api/me says what is early, signed in or not; the admin routes change a day', async () => {
  const features = [{ key: 'scout2', name: 'Scout 2' }];
  const early = createEarly({ s3, features });
  const cloud = createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 's'.repeat(40), google: google.google,
    log: () => {}, early, isAdmin: (e) => e === 'boss@example.com' });
  const server = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv'), cloud, early,
    stats: createStats(), adminToken: ADMIN, admins: ['boss@example.com'] });
  const bare = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv'), early });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  await new Promise((r) => bare.listen(0, '127.0.0.1', r));
  const base = `http://127.0.0.1:${server.address().port}`, bareBase = `http://127.0.0.1:${bare.address().port}`;
  try {
    const day = addDays(todayUTC(), 14);
    assert.deepEqual((await (await fetch(`${base}/api/me`)).json()).early, { legend: false, features: { scout2: day } }, 'signed out');
    assert.deepEqual(await (await fetch(`${bareBase}/api/me`)).json(), { auth: false, early: { legend: false, features: { scout2: day } } }, 'no accounts');
    google.user = { sub: '5000001', email: 'boss@example.com', email_verified: true, name: 'Boss' };
    const start = await fetch(`${base}/auth/google/start`, { redirect: 'manual' });
    const oauth = start.headers.getSetCookie()[0].split(';')[0];
    const atGoogle = await fetch(start.headers.get('location'), { redirect: 'manual' });
    const back = await fetch(atGoogle.headers.get('location'), { redirect: 'manual', headers: { cookie: oauth } });
    const cookie = back.headers.getSetCookie().filter((c) => c.startsWith('cm_session=')).map((c) => c.split(';')[0]).join('; ');
    assert.deepEqual((await (await fetch(`${base}/api/me`, { headers: { cookie } })).json()).early, { legend: true, features: { scout2: day } }, 'an admin');
    // The admin routes.
    const auth = { authorization: `Bearer ${ADMIN}` };
    const put = (body, headers = { ...auth, 'x-cm': '1' }) => fetch(`${base}/api/admin/early`, { method: 'PUT', body: JSON.stringify(body), headers: { 'content-type': 'application/json', ...headers } });
    assert.equal((await fetch(`${base}/api/admin/early`)).status, 401);
    let got = await (await fetch(`${base}/api/admin/early`, { headers: auth })).json();
    assert.deepEqual(got.features, [{ key: 'scout2', name: 'Scout 2', day, early: true }]);
    assert.equal(got.today, todayUTC());
    assert.equal((await put({ key: 'scout2', day: null }, { ...auth, origin: 'https://evil.example', 'x-cm': '1' })).status, 403, 'from another site');
    assert.equal((await put({ key: 'nope', day: null })).status, 404);
    assert.equal((await put({ key: 'scout2', day: '7 Dec' })).status, 400);
    got = await (await put({ key: 'scout2', day: null })).json();
    assert.deepEqual(got.features, [{ key: 'scout2', name: 'Scout 2', day: todayUTC(), early: false }]);
    assert.deepEqual((await (await fetch(`${base}/api/me`)).json()).early.features, {}, 'open to all now');
  } finally {
    server.close();
    bare.close();
  }
});
