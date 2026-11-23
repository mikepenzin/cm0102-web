// Tests of the accounts and cloud saves (cloud.mjs, auth.mjs, s3.mjs) against a stand-in Google and S3 store
// (test/fakes.mjs). Run: cd web && npm test
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { randomFillSync } from 'node:crypto';
import http from 'node:http';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { gunzipSync, gzipSync } from 'node:zlib';
import { sealer, googleStart } from '../auth.mjs';
import { corsOrigins, createCloud, cloudFromEnv, validSaveName } from '../cloud.mjs';
import { createS3, signV4 } from '../s3.mjs';
import { createApp } from '../server.mjs';
import { fakeGoogle, fakeS3, fakeSaveBytes } from './fakes.mjs';

let tmp, s3fake, google, base, server, logs, cloudApi;

before(async () => {
  tmp = mkdtempSync(join(tmpdir(), 'cm0102-cloud-test-'));
  mkdirSync(join(tmp, 'game'));
  writeFileSync(join(tmp, 'manifest.tsv'), '');
  s3fake = await fakeS3({ pageSize: 2 });             // two keys a page: the listing must follow the continuation
  google = await fakeGoogle();
  logs = [];
  const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const cloud = createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'x'.repeat(40),
    cloudUsers: ['Listed@Example.com'], upgradeUrl: 'https://example.com/support', maxSaves: 3, google: google.google, log: (t) => logs.push(t) });
  cloudApi = cloud;
  server = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv'), cloud });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  base = `http://127.0.0.1:${server.address().port}`;
});

after(() => { server.close(); s3fake.close(); google.close(); rmSync(tmp, { recursive: true, force: true }); });

// A browser, as far as these tests need one: cookies of the site, and redirects followed by hand.
function browser(at = base) {
  const jar = new Map();
  const keep = (r) => {
    for (const c of r.headers.getSetCookie()) {
      const [pair, ...attrs] = c.split('; ');
      const [k, v] = [pair.slice(0, pair.indexOf('=')), pair.slice(pair.indexOf('=') + 1)];
      if (attrs.includes('Max-Age=0') || v === '') jar.delete(k); else jar.set(k, v);
    }
    return r;
  };
  const cookie = () => [...jar].map(([k, v]) => `${k}=${v}`).join('; ');
  return {
    jar,
    get: (path, headers = {}) => fetch(path.startsWith('http') ? path : `${at}${path}`, { redirect: 'manual', headers: { cookie: cookie(), ...headers } }).then(keep),
    send: (method, path, body, headers = {}) => fetch(`${at}${path}`, { method, body, redirect: 'manual',
      headers: { cookie: cookie(), 'x-cm': '1', ...headers } }).then(keep),
    // The whole sign-in: the site, Google, and back. Resolves to the last answer (the redirect to the page).
    async signIn(mode = 'page') {
      const start = await this.get(`/auth/google/start?mode=${mode}`);
      assert.equal(start.status, 302);
      const atGoogle = await fetch(start.headers.get('location'), { redirect: 'manual' });
      assert.equal(atGoogle.status, 302, 'Google sends the browser back');
      return this.get(atGoogle.headers.get('location'));
    },
  };
}

test('sealed tokens: open only unchanged, of their kind, and before they expire', () => {
  const s = sealer('a secret of the server, long enough');
  const t = s.seal('s', { sub: '1' }, 60);
  assert.deepEqual(s.open('s', t), { sub: '1' });
  assert.equal(s.open('o', t), null, 'a session is not a sign-in state');
  assert.equal(s.open('s', `${t.slice(0, -2)}AA`), null, 'a changed seal');
  const [body, mac] = t.split('.');
  const forged = Buffer.from(JSON.stringify({ k: 's', d: { sub: '2' }, exp: 9e9 })).toString('base64url');
  assert.equal(s.open('s', `${forged}.${mac}`), null, 'a changed body');
  assert.equal(sealer('another secret of another server').open('s', t), null);
  assert.equal(s.open('s', s.seal('s', {}, -5)), null, 'expired');
  assert.equal(s.open('s', `${body}`), null);
  assert.equal(s.open('s', undefined), null);
});

test('the Google URL: code flow with PKCE, the state and the scopes', () => {
  const st = googleStart({ clientId: 'cid', redirectUri: 'https://x.fly.dev/auth/google/callback' });
  const u = new URL(st.url);
  assert.equal(u.origin + u.pathname, 'https://accounts.google.com/o/oauth2/v2/auth');
  assert.equal(u.searchParams.get('response_type'), 'code');
  assert.equal(u.searchParams.get('scope'), 'openid email profile');
  assert.equal(u.searchParams.get('code_challenge_method'), 'S256');
  assert.equal(u.searchParams.get('state'), st.state);
  assert.ok(st.verifier.length >= 43 && st.state.length >= 32);
});

test('save names', () => {
  for (const ok of ['Arsenal.sav', 'Copa América (2).SAV', 'a b.sav']) assert.ok(validSaveName(ok), ok);
  for (const bad of ['x.sav/../../y.sav', 'a/b.sav', '.sav', 'notes.txt', 'temp_save_file.sav', ' a.sav', 'a\u0001.sav', 'x'.repeat(130) + '.sav', null])
    assert.equal(validSaveName(bad), false, String(bad));
});

test('without its settings the server has no accounts, and says so to the page', async () => {
  const { cloud, missing } = cloudFromEnv({});
  assert.equal(cloud, null);
  assert.ok(missing.includes('GOOGLE_CLIENT_ID') && missing.includes('BUCKET_NAME'));
  assert.ok(cloudFromEnv({ ...s3fake.env, GOOGLE_CLIENT_ID: 'a', GOOGLE_CLIENT_SECRET: 'b', CM_SESSION_SECRET: 'short' }).missing[0].startsWith('CM_SESSION_SECRET'));
  assert.ok(cloudFromEnv({ ...s3fake.env, GOOGLE_CLIENT_ID: 'a', GOOGLE_CLIENT_SECRET: 'b', CM_SESSION_SECRET: 'y'.repeat(32) }).cloud);
  const plain = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv') });
  await new Promise((r) => plain.listen(0, '127.0.0.1', r));
  const at = `http://127.0.0.1:${plain.address().port}`;
  assert.deepEqual(await (await fetch(`${at}/api/me`)).json(), { auth: false });
  assert.equal((await fetch(`${at}/api/saves`)).status, 404);
  assert.equal((await fetch(`${at}/auth/google/start`, { redirect: 'manual' })).status, 404);
  plain.close();
});

test('sign in with Google: the account, the session cookie, and a free plan', async () => {
  const b = browser();
  assert.deepEqual(await (await b.get('/api/me')).json(), { auth: true, user: null, offer: { price: '$5 a month', free: false, url: 'https://example.com/support', paddle: null }, maxSaves: 3 });
  google.user = { sub: '1000001', email: 'player@example.com', email_verified: true, name: 'A Player' };
  const back = await b.signIn();
  assert.equal(back.status, 303);
  assert.equal(back.headers.get('location'), '/?account=signed-in');
  assert.ok(b.jar.has('cm_session') && !b.jar.has('cm_oauth'), 'the session is set, the sign-in state is gone');
  const set = back.headers.getSetCookie().find((c) => c.startsWith('cm_session='));
  assert.match(set, /HttpOnly/);
  assert.match(set, /SameSite=Lax/);
  const me = await (await b.get('/api/me')).json();
  assert.equal(me.user.email, 'player@example.com');
  assert.equal(me.user.cloud, false);
  assert.equal(me.offer.url, 'https://example.com/support');
  assert.equal(me.user.cloudFrom, null);
  const record = JSON.parse(s3fake.objects.get('accounts/1000001.json').body);
  assert.equal(record.plan, 'free');
  assert.equal(record.email, 'player@example.com');
  assert.equal((await b.get('/api/saves')).status, 403, 'no cloud saves on the free plan');
  assert.equal((await b.send('PUT', '/api/saves/a.sav', gzipSync(fakeSaveBytes()))).status, 403);
  // Signing out.
  assert.equal((await b.send('POST', '/auth/logout')).status, 204);
  assert.deepEqual(await (await b.get('/api/me')).json(), { auth: true, user: null, offer: { price: '$5 a month', free: false, url: 'https://example.com/support', paddle: null }, maxSaves: 3 });
});

test('the sign-in refuses a wrong state, a cancel, an unverified email and a token for another app', async () => {
  const b = browser();
  const start = await b.get('/auth/google/start');
  const atGoogle = await fetch(start.headers.get('location'), { redirect: 'manual' });
  const back = new URL(atGoogle.headers.get('location'));
  back.searchParams.set('state', 'not-the-state');
  let r = await b.get(back.pathname + back.search);
  assert.match(r.headers.get('location'), /^\/\?account=error&why=the%20sign-in%20took%20too%20long/);
  assert.equal(b.jar.has('cm_session'), false);
  r = await browser().get('/auth/google/callback?error=access_denied&state=x');
  assert.match(r.headers.get('location'), /why=cancelled/);
  google.extra = { email_verified: false };
  r = await browser().signIn();
  assert.match(decodeURIComponent(r.headers.get('location')), /no verified email/);
  google.extra = { aud: 'another-app' };
  r = await browser().signIn();
  assert.match(decodeURIComponent(r.headers.get('location')), /for another app/);
  google.extra = { exp: Math.floor(Date.now() / 1000) - 3600 };
  r = await browser().signIn();
  assert.match(decodeURIComponent(r.headers.get('location')), /expired/);
  google.extra = {};
  // The sign-in in a small window, while a game runs: a page that tells the game's page, then closes.
  const pop = await browser().signIn('popup');
  assert.equal(pop.status, 200);
  const html = await pop.text();
  assert.match(html, /BroadcastChannel\('cm-account'\)/);
  assert.match(html, /"ok":true/);
});

test('a save larger than 64 MB packed goes through the server by way of its disk, and comes back the same', async () => {
  // Long careers make large saves (a player's 446 MB, 15 November 2026); the server once held each upload in memory
  // and took 64 MB packed at most. Random bytes do not pack, so this one is about 70 MB packed.
  google.user = { sub: '2000777', email: 'listed@example.com', email_verified: true, name: 'Big' };
  const b = browser();
  await b.signIn();
  const raw = Buffer.alloc(0x416ec + (70 << 20));
  raw.writeInt32LE(2, 0); raw.writeInt32LE(1, 8); raw.writeUInt32LE(0x416ec, 12);
  randomFillSync(raw, 0x416ec);
  const gz = gzipSync(raw, { level: 1 });
  assert.ok(gz.length > 64 << 20);
  const r = await b.send('PUT', `/api/saves/${encodeURIComponent('Hertha BSC 2011-12-11.sav')}`, gz, { 'x-base-etag': 'none', 'x-save-time': '1762770000000' });
  assert.equal(r.status, 200, await r.clone().text());
  assert.equal((await r.json()).rawSize, raw.length);
  const back = await b.get(`/api/saves/${encodeURIComponent('Hertha BSC 2011-12-11.sav')}`);
  assert.ok(gunzipSync(Buffer.from(await back.arrayBuffer())).equals(raw), 'the same bytes come back');
  const { readdirSync } = await import('node:fs');
  assert.equal(readdirSync(tmpdir()).filter((n) => n.startsWith('cm0102-up-')).length, 0, 'no file left on disk');
  await b.send('DELETE', `/api/saves/${encodeURIComponent('Hertha BSC 2011-12-11.sav')}`);
});

test('cloud saves: store, list, fetch, conflicts, limits and checks', async () => {
  google.user = { sub: '2000002', email: 'listed@example.com', email_verified: true, name: 'Listed' };
  const b = browser();
  await b.signIn();
  const me = await (await b.get('/api/me')).json();
  assert.equal(me.user.cloud, true, 'a listed email has cloud saves');
  assert.equal(me.user.admin, false, 'not an admin: no Admin panel button');
  assert.equal(me.user.cloudFrom, 'given');
  const empty = await (await b.get('/api/saves')).json();
  assert.deepEqual({ ...empty, store: undefined }, { saves: [], maxSaves: 3, maxPacked: 400 << 20, maxRaw: 1024 << 20, store: undefined },
    'the list tells the page what one save may be, so it sends none larger');
  assert.match(empty.store, /^[0-9a-f]{18}$/, 'the store\'s id (meta/store.json), the same in each list');
  assert.equal(JSON.parse(s3fake.objects.get('meta/store.json').body).id, empty.store);

  const raw = fakeSaveBytes(1), gz = gzipSync(raw);
  let r = await b.send('PUT', `/api/saves/${encodeURIComponent('Copa América.sav')}`, gz, { 'x-base-etag': 'none', 'x-save-time': '1762770000000' });
  assert.equal(r.status, 200, await r.clone().text());
  const first = await r.json();
  assert.equal(first.rawSize, raw.length);
  assert.equal(first.saved, 1762770000000);
  assert.ok([...s3fake.objects.keys()].includes(`saves/2000002/${Buffer.from('Copa América.sav').toString('base64url')}.gz`), 'the name is base64url in the key');
  const list = (await (await b.get('/api/saves')).json()).saves;
  assert.deepEqual(list.map((s) => [s.name, s.etag, s.rawSize, s.saved]), [['Copa América.sav', first.etag, raw.length, 1762770000000]]);

  r = await b.get(`/api/saves/${encodeURIComponent('Copa América.sav')}`);
  assert.equal(r.status, 200);
  assert.equal(r.headers.get('x-raw-size'), String(raw.length));
  assert.ok(gunzipSync(Buffer.from(await r.arrayBuffer())).equals(raw), 'the same bytes come back');

  // Another device saved since: a write on the old version is refused, with what the cloud has now.
  const raw2 = fakeSaveBytes(2), gz2 = gzipSync(raw2);
  r = await b.send('PUT', `/api/saves/${encodeURIComponent('Copa América.sav')}`, gz2, { 'x-base-etag': first.etag });
  const second = await r.json();
  r = await b.send('PUT', `/api/saves/${encodeURIComponent('Copa América.sav')}`, gzipSync(fakeSaveBytes(3)), { 'x-base-etag': first.etag });
  assert.equal(r.status, 409);
  assert.equal((await r.json()).cloud.etag, second.etag);
  r = await b.send('PUT', `/api/saves/${encodeURIComponent('Copa América.sav')}`, gzipSync(fakeSaveBytes(3)), { 'x-base-etag': 'none' });
  assert.equal(r.status, 409, 'none: only when there is no such save');

  // Not a save, not gzip, not a name, not from this site.
  r = await b.send('PUT', '/api/saves/x.sav', gzipSync(Buffer.from('hello')));
  assert.equal(r.status, 422);
  assert.match((await r.json()).error, /too short/);
  assert.equal((await b.send('PUT', '/api/saves/x.sav', Buffer.from('plain'))).status, 422);
  assert.equal((await b.send('PUT', '/api/saves/..%2F..%2Fx.sav', gz)).status, 400);
  assert.equal((await b.send('PUT', '/api/saves/x.sav', gz, { 'x-cm': '' })).status, 403, 'no X-CM header');
  assert.equal((await b.send('PUT', '/api/saves/x.sav', gz, { origin: 'https://evil.example' })).status, 403, 'another site');
  assert.equal((await b.send('PUT', '/api/saves/x.sav', gz, { cookie: '' })).status, 401, 'not signed in');

  // At most three saves; a fourth is refused, a change of one of the three is not.
  for (const n of ['b.sav', 'c.sav']) assert.equal((await b.send('PUT', `/api/saves/${n}`, gz)).status, 200);
  r = await b.send('PUT', '/api/saves/d.sav', gz);
  assert.equal(r.status, 507);
  assert.equal((await b.send('PUT', '/api/saves/c.sav', gz2)).status, 200);
  assert.equal((await (await b.get('/api/saves')).json()).saves.length, 3, 'the listing follows the continuation');

  assert.equal((await b.send('DELETE', '/api/saves/c.sav')).status, 204);
  assert.equal((await b.get('/api/saves/c.sav')).status, 404);
  assert.equal((await (await b.get('/api/saves')).json()).saves.length, 2);

  // Delete the account: its saves, its record and the session go.
  assert.equal((await b.send('DELETE', '/api/account')).status, 204);
  assert.equal([...s3fake.objects.keys()].some((k) => k.includes('2000002')), false);
  assert.deepEqual(await (await b.get('/api/me')).json(), { auth: true, user: null, offer: { price: '$5 a month', free: false, url: 'https://example.com/support', paddle: null }, maxSaves: 3 });
});

test('tactics in the cloud: one record per account, changed only from its last version, its files checked', async () => {
  google.user = { sub: '2000077', email: 'listed@example.com', email_verified: true, name: 'Listed' };
  const b = browser();
  await b.signIn();
  let r = await b.get('/api/tactics');
  assert.equal(r.status, 200);
  assert.deepEqual(await r.json(), { tactics: [], removed: {}, etag: null, maxTactics: 200 });
  const tct = Buffer.alloc(1476, 3).toString('base64');
  const put = (body, base) => b.send('PUT', '/api/tactics', JSON.stringify(body), { 'content-type': 'application/json', 'x-base-etag': base });
  const removedAt = Date.now() - 86400000;                // a day ago: removals older than 400 days are dropped
  r = await put({ tactics: [{ name: 'My 4-4-2.tct', data: tct, mtime: 1762770000000 }], removed: { 'Old.tct': removedAt } }, 'none');
  assert.equal(r.status, 200, await r.clone().text());
  const { etag } = await r.json();
  const got = await (await b.get('/api/tactics')).json();
  assert.deepEqual(got.tactics, [{ name: 'My 4-4-2.tct', data: tct, mtime: 1762770000000 }]);
  assert.deepEqual(got.removed, { 'Old.tct': removedAt });
  assert.equal(got.etag, etag);
  assert.ok(s3fake.objects.has('tactics/2000077.json'));
  assert.equal((await put({ tactics: [], removed: {} }, 'none')).status, 409, 'another device wrote it since');
  assert.equal((await put({ tactics: [], removed: {} }, '"stale"')).status, 409);
  for (const bad of [{ name: '../x.tct', data: tct, mtime: 1 }, { name: 'notes.txt', data: tct, mtime: 1 },
    { name: 'Big.tct', data: Buffer.alloc(9000).toString('base64'), mtime: 1 }]) {
    assert.equal((await put({ tactics: [bad], removed: {} }, etag)).status, 422, bad.name);
  }
  const many = Array.from({ length: 201 }, (_, i) => ({ name: `T${i}.tct`, data: tct, mtime: 1 }));
  assert.equal((await put({ tactics: many, removed: {} }, etag)).status, 413);
  assert.equal((await put({ tactics: [], removed: {} }, etag)).status, 200, 'all removed');
  google.user = { sub: '3000077', email: 'nocloud@example.com', email_verified: true, name: 'No cloud' };
  const other = browser();
  await other.signIn();
  assert.equal((await other.get('/api/tactics')).status, 403, 'an account without cloud saves');
  google.user = { sub: '2000077', email: 'listed@example.com', email_verified: true, name: 'Listed' };
  assert.equal((await b.send('DELETE', '/api/account')).status, 204);
  assert.ok(!s3fake.objects.has('tactics/2000077.json'), 'the account\'s deletion removes its tactics');
});

test('a plan of "cloud" in the account record gives cloud saves', async () => {
  const put = (key, obj) => createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true })
    .put(key, Buffer.from(JSON.stringify(obj)), { type: 'application/json' });
  await put('accounts/3000003.json', { sub: '3000003', email: 'payer@example.com', plan: 'cloud', planUntil: new Date(Date.now() + 86400e3).toISOString() });
  google.user = { sub: '3000003', email: 'payer@example.com', email_verified: true, name: 'Payer' };
  const b = browser();
  await b.signIn();
  assert.equal((await (await b.get('/api/me')).json()).user.cloud, true);
  assert.equal(JSON.parse(s3fake.objects.get('accounts/3000003.json').body).plan, 'cloud', 'the sign-in keeps the plan');
  await put('accounts/3000004.json', { sub: '3000004', email: 'lapsed@example.com', plan: 'cloud', planUntil: '2020-01-01T00:00:00Z' });
  google.user = { sub: '3000004', email: 'lapsed@example.com', email_verified: true, name: 'Lapsed' };
  const c = browser();
  await c.signIn();
  assert.equal((await (await c.get('/api/me')).json()).user.cloud, false, 'a plan that ended');

  // An admin gives Cloud Saves (the admin page): a file of its own, grants/<id>.json; the record stays as it was.
  assert.deepEqual(await cloudApi.planOfSub('3000004'), { level: 'free', cloud: false, from: null, given: false, givenLevel: null, givenAt: null, until: null,
    cancelAt: null, paidLevel: null, paidFrom: null, known: true, bmcLevel: null, admin: false });
  const given = await cloudApi.give('3000004', true, 'boss@example.com');
  assert.deepEqual([given.cloud, given.from, given.given], [true, 'given', true]);
  let me = (await (await c.get('/api/me')).json()).user;
  assert.equal(me.cloud, true);
  assert.equal(me.cloudFrom, 'given', 'the Account dialog says: given to this account');
  assert.equal(JSON.parse(s3fake.objects.get('grants/3000004.json').body).by, 'boss@example.com');
  assert.equal(JSON.parse(s3fake.objects.get('accounts/3000004.json').body).planUntil, '2020-01-01T00:00:00Z', 'the record is not touched');
  // The record written again from an old copy (as a sign-in or Paddle's event may): the gift stays.
  await createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true })
    .put('accounts/3000004.json', Buffer.from(JSON.stringify({ sub: '3000004', email: 'lapsed@example.com', plan: 'free', planUntil: null })), { type: 'application/json' });
  await c.signIn();
  // Another server (a second machine, or a restart) sees it at once: the admin page and /api/me read fresh.
  const other = createCloud({ s3: createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true }),
    clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'x'.repeat(40), google: google.google, log: () => {}, direct: false });
  assert.equal((await other.planOfSub('3000004')).given, true);
  me = (await (await c.get('/api/me')).json()).user;
  assert.equal(me.cloud, true, 'the gift outlives a record written again');
  assert.equal((await cloudApi.give('3000004', false, 'boss@example.com')).cloud, false);
  assert.equal(s3fake.objects.has('grants/3000004.json'), false);
  me = (await (await c.get('/api/me')).json()).user;
  assert.equal(me.cloud, false, 'taken back');
  assert.equal((await other.planOfSub('3000004')).given, false, 'the other server too');
  // A gift of the first version (given in the record) still counts, and Take back removes it.
  await createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true })
    .put('accounts/3000004.json', Buffer.from(JSON.stringify({ sub: '3000004', email: 'lapsed@example.com', plan: 'free', given: { by: 'x', at: 'y' } })), { type: 'application/json' });
  assert.equal((await cloudApi.planOfSub('3000004')).given, true);
  assert.equal((await cloudApi.give('3000004', false, 'boss@example.com')).given, false);
  assert.equal(JSON.parse(s3fake.objects.get('accounts/3000004.json').body).given, undefined);
  assert.equal(await cloudApi.give('9999999', true, 'boss@example.com'), null, 'an account that never signed in');
});

test('the store fails a save: the server sends it again twice, then answers 502', async () => {
  google.user = { sub: '2000088', email: 'listed@example.com', email_verified: true, name: 'Listed' };
  const b = browser();
  await b.signIn();
  const gz = gzipSync(fakeSaveBytes(8));
  Object.assign(s3fake.faults, { failPuts: 2, failMatch: /^saves\/2000088\// });
  let r = await b.send('PUT', '/api/saves/retry.sav', gz);
  assert.equal(r.status, 200, 'two failures: the third try is taken');
  assert.equal(s3fake.faults.failPuts, 0);
  s3fake.faults.failPuts = 3;
  r = await b.send('PUT', '/api/saves/retry.sav', gz);
  assert.equal(r.status, 502, 'three: the page is told, and tries again later');
  assert.match((await r.json()).error, /cloud store did not take/);
  s3fake.faults.failPuts = 0;
});

test('direct: a save goes to the store with a link, is checked, and takes its place; a bad one never does', async () => {
  google.user = { sub: '2000099', email: 'listed@example.com', email_verified: true, name: 'Listed' };
  const b = browser();
  await b.signIn();
  assert.equal(await cloudApi.directReady, true, 'the store took the CORS rules');
  const json = (o) => JSON.stringify(o);
  const ask = (name, how, body) => b.send('POST', `/api/saves/${encodeURIComponent(name)}?direct=${how}`, json(body), { 'content-type': 'application/json' });
  const raw = fakeSaveBytes(9), gz = gzipSync(raw);
  let r = await ask('Direct.sav', 'put', { size: gz.length, rawSize: raw.length, saved: 1762770000000, base: 'none' });
  assert.equal(r.status, 200);
  const link = await r.json();
  assert.equal(link.direct, true);
  assert.match(link.key, /^incoming\/2000099\/\d+-[0-9a-f]{12}\.gz$/);
  assert.deepEqual(link.headers, { 'content-type': 'application/gzip', 'x-amz-meta-raw-size': String(raw.length), 'x-amz-meta-saved': '1762770000000' });
  // The link takes only the length it was made for, and only these headers.
  assert.equal((await fetch(link.url, { method: 'PUT', body: Buffer.concat([gz, Buffer.from('x')]), headers: link.headers })).status, 403);
  assert.equal((await fetch(link.url, { method: 'PUT', body: gz, headers: { ...link.headers, 'x-amz-meta-saved': '1' } })).status, 403);
  assert.equal((await fetch(link.url, { method: 'PUT', body: gz, headers: link.headers })).status, 200);
  assert.equal((await ask('Direct.sav', 'done', { key: 'incoming/2000002/1-aaaaaaaaaaaa.gz', base: 'none' })).status, 400, 'another account\'s upload');
  r = await ask('Direct.sav', 'done', { key: link.key, base: 'none' });
  assert.equal(r.status, 200, await r.clone().text());
  const done = await r.json();
  assert.deepEqual([done.rawSize, done.saved, done.size], [raw.length, 1762770000000, gz.length]);
  const final = `saves/2000099/${Buffer.from('Direct.sav').toString('base64url')}.gz`;
  assert.ok(s3fake.objects.get(final).body.equals(gz), 'in its place');
  assert.equal(s3fake.objects.has(link.key), false, 'the upload is gone from incoming/');
  // A link to get it, and the same bytes from the store.
  r = await b.get(`/api/saves/${encodeURIComponent('Direct.sav')}?direct=get`);
  const got = await r.json();
  assert.deepEqual([got.etag, got.rawSize, got.saved], [done.etag, raw.length, 1762770000000]);
  assert.ok(gunzipSync(Buffer.from(await (await fetch(got.url)).arrayBuffer())).equals(raw));
  // Not a save: refused at done, and the save in its place stays as it was.
  const bad = gzipSync(Buffer.from('not a save at all, but long enough to read a header from it '.repeat(40)));
  const l2 = await (await ask('Direct.sav', 'put', { size: bad.length, rawSize: 2400, base: done.etag })).json();
  await fetch(l2.url, { method: 'PUT', body: bad, headers: l2.headers });
  r = await ask('Direct.sav', 'done', { key: l2.key, base: done.etag });
  assert.equal(r.status, 422);
  assert.ok(s3fake.objects.get(final).body.equals(gz), 'the save stays');
  assert.equal(s3fake.objects.has(l2.key), false);
  // Changed on another device between the link and the done: refused, the other device's save stays.
  const l3 = await (await ask('Direct.sav', 'put', { size: gz.length, rawSize: raw.length, base: done.etag })).json();
  await fetch(l3.url, { method: 'PUT', body: gz, headers: l3.headers });
  const other = gzipSync(fakeSaveBytes(10));
  await b.send('PUT', `/api/saves/${encodeURIComponent('Direct.sav')}`, other, { 'x-base-etag': done.etag });
  r = await ask('Direct.sav', 'done', { key: l3.key, base: done.etag });
  assert.equal(r.status, 409);
  assert.ok(s3fake.objects.get(final).body.equals(other));
  // Too large, refused before the link.
  assert.equal((await ask('Big.sav', 'put', { size: (400 << 20) + 1, rawSize: 1000, base: 'none' })).status, 413, 'past the limit (400 MB packed)');
});

test('direct: the server tries the way a page would go (a signed upload link, the browser\'s preflight from the site), and goes through itself when it fails', async () => {
  // On the live site the pages' uploads to the store failed ("the connection to the store failed") where the server had
  // set the rules (15 November 2026); the page cannot tell why. The server now finds out at its start, and says.
  const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const said = [];
  const make = (extra = {}) => createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'x'.repeat(40),
    cloudFor: 'everyone', google: google.google, log: (t) => said.push(t), origins: ['https://cm.example'], ...extra });
  assert.equal(await make().directReady, true, 'the rules set, the link taken');
  assert.match(said.join(' '), /checked from https:\/\/cm\.example: a signed upload link and the browser's preflight work/);
  said.length = 0;
  s3fake.faults.corsGone = true;                      // rules that no longer let the site in (set by hand, CM_DIRECT=on)
  assert.equal(await make({ direct: 'given' }).directReady, false, 'CM_DIRECT=on with rules that do not let the site in');
  assert.match(said.join(' '), /the bucket's CORS rules do not let https:\/\/cm\.example send .*go through this server/);
  s3fake.faults.corsGone = false;
  said.length = 0;
  s3fake.faults.rejectLinks = true;                   // a store that refuses the link's signature
  assert.equal(await make().directReady, false);
  assert.match(said.join(' '), /the store refused a signed upload link \(HTTP 403.*SignatureDoesNotMatch.*go through this server/);
  s3fake.faults.rejectLinks = false;
  assert.equal([...s3fake.objects.keys()].filter((k) => k.startsWith('meta/direct-check')).length, 0, 'the check leaves nothing in the store');
});

test('direct, later: the rules read back in the log, a check again later that turns direct on, and the other address tried', async () => {
  // The live site (17 November 2026): the rules were set, and the preflight still had no Access-Control-Allow-Origin.
  const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const said = [];
  const make = (extra = {}) => createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'x'.repeat(40),
    cloudFor: 'everyone', google: google.google, log: (t) => said.push(t), origins: ['https://cm.example'], ...extra });
  s3fake.faults.corsGone = true;
  const later = make({ recheck: [80] });
  assert.equal(await later.directReady, false);
  assert.match(said.join(' '), /do not let https:\/\/cm\.example send .*the bucket holds: origins https:\/\/cm\.example; methods GET, PUT, HEAD/);
  assert.match(said.join(' '), /checks again in 0 s/);
  s3fake.faults.corsGone = false;                     // the rules reach the store's every place
  await new Promise((r) => setTimeout(r, 400));
  assert.equal(later.isDirect(), true, 'a later check turns direct on');
  assert.match(said.join(' '), /now go straight between the browser and the store/);
  // Only another address of the store lets pages in (as t3.storage.dev where fly.storage.tigris.dev does not): the
  // pages' links name it, and the log says which.
  said.length = 0;
  const port = new URL(s3fake.endpoint).port;
  const at = (host) => createS3({ endpoint: `http://${host}:${port}`, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  s3fake.faults.corsHost = '127.0.0.1';
  const two = make({ s3: at('localhost'), linkStores: [{ s3: at('localhost'), name: 'a second way that fails too' }, { s3: at('127.0.0.1'), name: 'http://127.0.0.1' }], recheck: [] });
  assert.equal(await two.directReady, true, 'the address that works is taken');
  assert.equal(two.linkName(), 'http://127.0.0.1');
  assert.match(said.join(' '), /go straight between the browser and the store .*the links name http:\/\/127\.0\.0\.1, in place of the configured address, which did not pass/);
  // None works: each one's reason is logged.
  said.length = 0;
  s3fake.faults.corsHost = 'nowhere';
  const none = make({ s3: at('localhost'), linkStores: [{ s3: at('127.0.0.1'), name: 'http://127.0.0.1' }], recheck: [] });
  assert.equal(await none.directReady, false);
  assert.match(said.join(' '), /do not let https:\/\/cm\.example send .*go through this server.* and at http:\/\/127\.0\.0\.1: the bucket's CORS rules do not let/);
  s3fake.faults.corsHost = null;
  assert.equal([...s3fake.objects.keys()].filter((k) => k.startsWith('meta/direct-check')).length, 0, 'the checks leave nothing in the store');
});

test('direct off: a store that takes no CORS rules, or CM_DIRECT=off: the page is told to go through the server', async () => {
  s3fake.faults.noCors = true;
  const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const said = [];
  const cloud = createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'x'.repeat(40),
    cloudFor: 'everyone', google: google.google, log: (t) => said.push(t) });
  assert.equal(await cloud.directReady, false);
  assert.match(said.join(' '), /CORS rules were not set .*go through this server/);
  const byHand = createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'x'.repeat(40),
    cloudFor: 'everyone', google: google.google, log: (t) => said.push(t), direct: 'given' });
  assert.equal(await byHand.directReady, true, 'CM_DIRECT=on: the rules were set in the console; the server does not try');
  s3fake.faults.noCors = false;
  const two = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv'), cloud });
  await new Promise((r) => two.listen(0, '127.0.0.1', r));
  try {
    google.user = { sub: '2000111', email: 'off@example.com', email_verified: true, name: 'Off' };
    const b = browser(`http://127.0.0.1:${two.address().port}`);
    await b.signIn();
    const r = await b.send('POST', '/api/saves/x.sav?direct=put', JSON.stringify({ size: 10, rawSize: 10 }), { 'content-type': 'application/json' });
    assert.deepEqual(await r.json(), { direct: false });
  } finally { two.close(); }
  assert.deepEqual(corsOrigins({ FLY_APP_NAME: 'cm', CM_PUBLIC_URL: 'https://cm.example/' }), ['https://cm.example', 'https://cm.fly.dev']);
  assert.deepEqual(corsOrigins({}), ['*']);
  assert.deepEqual(corsOrigins({ CM_CORS_ORIGINS: 'https://a.example, https://b.example' }), ['https://a.example', 'https://b.example']);
});

test('the S3 store refuses a request with another secret (the stand-in checks signatures)', async () => {
  const wrong = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'not-it', pathStyle: true });
  await assert.rejects(wrong.put('x', Buffer.from('y')), /SignatureDoesNotMatch/);
  assert.match(signV4({ method: 'GET', path: '/', headers: { host: 'h', 'x-amz-date': '20260101T000000Z' }, payloadHash: 'e3b0', accessKeyId: 'a',
    secretAccessKey: 'b', region: 'auto' }), /^AWS4-HMAC-SHA256 Credential=a\/20260101\/auto\/s3\/aws4_request, SignedHeaders=host;x-amz-date, Signature=[0-9a-f]{64}$/);
});

test('the sync plan: what goes up, what comes down, what is kept twice', async () => {
  const { planSync, otherDeviceName } = await import('../account.js');
  const L = (entries) => new Map(entries.map(([n, size, mtime]) => [n, { size, mtime }]));
  const C = (entries) => new Map(entries.map(([n, etag, rawSize, saved]) => [n, { etag, rawSize, saved }]));
  const kinds = (p) => p.jobs.map((j) => `${j.do} ${j.name}${j.base ? ` ${j.base}` : ''}`).sort();
  const state = {
    same: { etag: '"1"', size: 10, mtime: 100 },
    upNow: { etag: '"1"', size: 10, mtime: 100 },
    downNow: { etag: '"1"', size: 10, mtime: 100 },
    both: { etag: '"1"', size: 10, mtime: 100 },
    newerHere: { etag: '"1"', size: 10, mtime: 100 },
    goneThere: { etag: '"1"', size: 10, mtime: 100 },
    goneButChanged: { etag: '"1"', size: 10, mtime: 100 },
    removed: { etag: null, size: 10, mtime: 100, removed: true },
    gone: { etag: '"1"', size: 10, mtime: 100 },
  };
  const local = L([['same', 10, 100], ['upNow', 11, 200], ['downNow', 10, 100], ['both', 12, 300], ['newerHere', 14, 600], ['goneThere', 10, 100],
    ['goneButChanged', 13, 400], ['removed', 10, 100], ['newHere', 5, 50], ['firstTimeSame', 20, 1000], ['firstTimeOther', 21, 1000]]);
  const cloud = C([['same', '"1"'], ['upNow', '"1"'], ['downNow', '"2"'], ['both', '"2"', 13, 400], ['newerHere', '"2"', 15, 500], ['newThere', '"9"'],
    ['firstTimeSame', '"5"', 20, 1500], ['firstTimeOther', '"6"', 22, 1000]]);
  let p = planSync(local, cloud, state);
  // Changed on both sides: kept twice when the cloud's is the newer (both), sent over it when the one here is (newerHere).
  assert.deepEqual(kinds(p), ['both both', 'both firstTimeOther', 'down downNow', 'down newThere', 'up goneButChanged none',
    'up newHere none', 'up newerHere "2"', 'up upNow "1"'].sort());
  assert.deepEqual(p.settle.goneThere, { ...state.goneThere, removed: true }, 'removed from the cloud elsewhere: kept here, not sent again');
  assert.deepEqual(p.settle.firstTimeSame, { etag: '"5"', size: 20, mtime: 1000 }, 'the same length and time: the same save');
  assert.deepEqual(p.forget, ['gone']);
  // While the game runs a save here is never replaced: a newer one in the cloud waits for the next start.
  p = planSync(local, cloud, state, { replace: false });
  assert.ok(!kinds(p).includes('down downNow') && kinds(p).includes('down newThere'));
  assert.equal(otherDeviceName('Arsenal.sav', new Set()), 'Arsenal (other device).sav');
  assert.equal(otherDeviceName('Arsenal.sav', new Set(['arsenal (other device).sav'])), 'Arsenal (other device) (2).sav');
});

test('the cloud\'s copy that this device sent itself is not taken for a change elsewhere (a reload downloaded it each time)', async () => {
  // An iPhone fetched its save from the cloud at every reload though its own copy was the newer one (21 November 2026):
  // an upload that went through after the tab was left, whose new etag the page never recorded, looked like a change
  // made on another device, and the save came down again (as "… (other device)") at the next start.
  const { planSync } = await import('../account.js');
  const L = (entries) => new Map(entries.map(([n, size, mtime]) => [n, { size, mtime }]));
  const C = (entries) => new Map(entries.map(([n, etag, rawSize, saved]) => [n, { etag, rawSize, saved }]));
  const state = {
    sentAsIs: { etag: '"1"', size: 10, mtime: 100 },                                   // the upload of 500 went, unrecorded
    sentThenSaved: { etag: '"1"', size: 10, mtime: 100, sending: { size: 20, mtime: 500 } },  // and saved again since
    elsewhere: { etag: '"1"', size: 10, mtime: 100, sending: { size: 20, mtime: 500 } },      // another device's, newer
  };
  const local = L([['sentAsIs', 20, 500], ['sentThenSaved', 30, 900], ['elsewhere', 30, 900]]);
  const cloud = C([['sentAsIs', '"2"', 20, 500], ['sentThenSaved', '"2"', 20, 500], ['elsewhere', '"3"', 25, 1000]]);
  for (const replace of [true, false]) {
    const p = planSync(local, cloud, state, { replace });
    assert.deepEqual(p.jobs.map((j) => `${j.do} ${j.name}${j.base ? ` ${j.base}` : ''}`).sort(), ['both elsewhere', 'up sentThenSaved "2"']);
    assert.deepEqual(p.settle.sentAsIs, { etag: '"2"', size: 20, mtime: 500 }, 'the cloud has this very file: nothing goes either way');
    assert.ok(p.jobs.every((j) => typeof j.why === 'string' && j.why), 'each job says why, for the log');
  }
});

test('a large save that went up less than 10 minutes ago waits; it goes when that time is past, or when the tab is left', async () => {
  const { planSync, BIG_SAVE, BIG_EVERY_MS } = await import('../account.js');
  assert.equal(BIG_SAVE, 100 << 20);
  assert.equal(BIG_EVERY_MS, 10 * 60000);
  const now = 50 * 60000, big = 690 << 20;
  const local = new Map([['Big.sav', { size: big + 1, mtime: 9 }], ['Small.sav', { size: 40 << 20, mtime: 9 }], ['New big.sav', { size: big, mtime: 9 }],
    ['Old big.sav', { size: big + 1, mtime: 9 }]]);
  const cloud = new Map([['Big.sav', { etag: '"1"' }], ['Small.sav', { etag: '"1"' }], ['Old big.sav', { etag: '"1"' }]]);
  const state = {
    'Big.sav': { etag: '"1"', size: big, mtime: 1, upAt: now - 3 * 60000 },          // up 3 minutes ago
    'Small.sav': { etag: '"1"', size: 1, mtime: 1, upAt: now - 60000 },
    'Old big.sav': { etag: '"1"', size: big, mtime: 1, upAt: now - 11 * 60000 },      // up 11 minutes ago
  };
  const names = (p) => p.jobs.map((j) => j.name).sort();
  let p = planSync(local, cloud, state, { replace: false, now });
  assert.deepEqual(names(p), ['New big.sav', 'Old big.sav', 'Small.sav'], 'a small save, a first upload and one past its time go');
  assert.deepEqual(p.held, ['Big.sav']);
  p = planSync(local, cloud, { ...state, 'Big.sav': { ...state['Big.sav'], upAt: now + 3600000 } }, { replace: false, now });
  assert.deepEqual(p.held, [], 'a clock set back an hour does not hold it');
  p = planSync(local, cloud, state, { replace: false, now, force: true });
  assert.deepEqual(names(p), ['Big.sav', 'New big.sav', 'Old big.sav', 'Small.sav'], 'the tab is left: everything goes');
  assert.deepEqual(p.held, []);
});

test('saves through the server go to its disk, two of the largest at once: a third waits for the next sync (503)', async () => {
  // A save no longer goes through memory (spool.mjs): what bounds the machine is its disk, two saves of maxPacked.
  const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const gz = gzipSync(fakeSaveBytes(7));
  const cloud = createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'x'.repeat(40),
    cloudFor: 'everyone', google: google.google, log: () => {}, maxPacked: gz.length + 10, isAdmin: (e) => e === 'budget@example.com' });
  const two = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv'), cloud });
  await new Promise((r) => two.listen(0, '127.0.0.1', r));
  const at = `http://127.0.0.1:${two.address().port}`;
  try {
    google.user = { sub: '2000077', email: 'budget@example.com', email_verified: true, name: 'Budget' };
    const b = browser(at);
    await b.signIn();
    assert.equal((await (await b.get('/api/me')).json()).user.admin, true, 'an admin: the page shows the Admin panel button');
    // Two uploads send part of their body and wait: each holds its size of the disk.
    const cookie = [...b.jar].map(([k, v]) => `${k}=${v}`).join('; ');
    const slow = (name) => {
      let status;
      const done = new Promise((resolveSlow) => {
        const req = http.request(`${at}/api/saves/${name}`, { method: 'PUT', headers: { cookie, 'x-cm': '1', 'content-length': gz.length,
          'content-type': 'application/gzip' } }, (res) => { status = res.statusCode; res.resume(); res.on('end', resolveSlow); });
        req.write(gz.subarray(0, 100));
        setTimeout(() => req.end(gz.subarray(100)), 400);
      });
      return done.then(() => status);
    };
    const first = slow('one.sav'), second = slow('two.sav');
    await new Promise((r) => setTimeout(r, 150));
    const busy = await b.send('PUT', '/api/saves/three.sav', gz);
    assert.equal(busy.status, 503, 'a third while two are on disk');
    assert.equal(busy.headers.get('retry-after'), '30');
    assert.deepEqual([await first, await second], [200, 200]);
    const again = await b.send('PUT', '/api/saves/three.sav', gz);
    assert.equal(again.status, 200, 'the disk is free again once they are stored');
  } finally {
    two.close();
  }
});

test('levels: an admin is a Club Legend, a gift kept before levels is Squad Player, CM_CLOUD_FOR=everyone is Squad Player', async () => {
  const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const make = (extra) => createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'x'.repeat(40), google: google.google, log: () => {}, ...extra });
  const c = make({ isAdmin: (e) => e === 'boss@example.com' });
  assert.deepEqual(((p) => [p.level, p.cloud])(await c.planOf({ sub: '9100001', email: 'Boss@Example.com' }, true)), ['legend', true]);
  await s3.put('grants/9100002.json', Buffer.from(JSON.stringify({ sub: '9100002', by: 'x', at: '2026-01-01T00:00:00Z' })), { type: 'application/json' });
  assert.equal((await c.planOf({ sub: '9100002', email: 'old@example.com' }, true)).level, 'squad');
  assert.equal((await c.planOf({ sub: '9100003', email: 'nobody@example.com' }, true)).level, 'free');
  assert.equal((await make({ cloudFor: 'everyone' }).planOf({ sub: '9100003', email: 'nobody@example.com' }, true)).level, 'squad');
  assert.equal((await c.planOf({ sub: '9100004', email: 'listed@example.com' }, true)).level, 'free');
  assert.equal((await make({ cloudUsers: ['Listed@Example.com'] }).planOf({ sub: '9100004', email: 'listed@example.com' }, true)).level, 'squad');
});
