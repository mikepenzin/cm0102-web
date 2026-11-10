// Tests of the accounts and cloud saves (cloud.mjs, auth.mjs, s3.mjs) against a stand-in Google and S3 store
// (test/fakes.mjs). Run: cd web && npm test
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { gunzipSync, gzipSync } from 'node:zlib';
import { sealer, googleStart } from '../auth.mjs';
import { createCloud, cloudFromEnv, validSaveName } from '../cloud.mjs';
import { createS3, signV4 } from '../s3.mjs';
import { createApp } from '../server.mjs';
import { fakeGoogle, fakeS3, fakeSaveBytes } from './fakes.mjs';

let tmp, s3fake, google, base, server, logs;

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
  server = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv'), cloud });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  base = `http://127.0.0.1:${server.address().port}`;
});

after(() => { server.close(); s3fake.close(); google.close(); rmSync(tmp, { recursive: true, force: true }); });

// A browser, as far as these tests need one: cookies of the site, and redirects followed by hand.
function browser() {
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
    get: (path, headers = {}) => fetch(path.startsWith('http') ? path : `${base}${path}`, { redirect: 'manual', headers: { cookie: cookie(), ...headers } }).then(keep),
    send: (method, path, body, headers = {}) => fetch(`${base}${path}`, { method, body, redirect: 'manual',
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
  assert.deepEqual(await (await b.get('/api/me')).json(), { auth: true, user: null });
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
  assert.equal(me.upgradeUrl, 'https://example.com/support');
  const record = JSON.parse(s3fake.objects.get('accounts/1000001.json').body);
  assert.equal(record.plan, 'free');
  assert.equal(record.email, 'player@example.com');
  assert.equal((await b.get('/api/saves')).status, 403, 'no cloud saves on the free plan');
  assert.equal((await b.send('PUT', '/api/saves/a.sav', gzipSync(fakeSaveBytes()))).status, 403);
  // Signing out.
  assert.equal((await b.send('POST', '/auth/logout')).status, 204);
  assert.deepEqual(await (await b.get('/api/me')).json(), { auth: true, user: null });
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

test('cloud saves: store, list, fetch, conflicts, limits and checks', async () => {
  google.user = { sub: '2000002', email: 'listed@example.com', email_verified: true, name: 'Listed' };
  const b = browser();
  await b.signIn();
  const me = await (await b.get('/api/me')).json();
  assert.equal(me.user.cloud, true, 'a listed email has cloud saves');
  assert.equal(me.upgradeUrl, null);
  assert.deepEqual(await (await b.get('/api/saves')).json(), { saves: [], maxSaves: 3 });

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
  assert.deepEqual(await (await b.get('/api/me')).json(), { auth: true, user: null });
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
    goneThere: { etag: '"1"', size: 10, mtime: 100 },
    goneButChanged: { etag: '"1"', size: 10, mtime: 100 },
    removed: { etag: null, size: 10, mtime: 100, removed: true },
    gone: { etag: '"1"', size: 10, mtime: 100 },
  };
  const local = L([['same', 10, 100], ['upNow', 11, 200], ['downNow', 10, 100], ['both', 12, 300], ['goneThere', 10, 100],
    ['goneButChanged', 13, 400], ['removed', 10, 100], ['newHere', 5, 50], ['firstTimeSame', 20, 1000], ['firstTimeOther', 21, 1000]]);
  const cloud = C([['same', '"1"'], ['upNow', '"1"'], ['downNow', '"2"'], ['both', '"2"'], ['newThere', '"9"'],
    ['firstTimeSame', '"5"', 20, 1500], ['firstTimeOther', '"6"', 22, 1000]]);
  let p = planSync(local, cloud, state);
  assert.deepEqual(kinds(p), ['both both', 'both firstTimeOther', 'down downNow', 'down newThere', 'up goneButChanged none',
    'up newHere none', 'up upNow "1"'].sort());
  assert.deepEqual(p.settle.goneThere, { ...state.goneThere, removed: true }, 'removed from the cloud elsewhere: kept here, not sent again');
  assert.deepEqual(p.settle.firstTimeSame, { etag: '"5"', size: 20, mtime: 1000 }, 'the same length and time: the same save');
  assert.deepEqual(p.forget, ['gone']);
  // While the game runs a save here is never replaced: a newer one in the cloud waits for the next start.
  p = planSync(local, cloud, state, { replace: false });
  assert.ok(!kinds(p).includes('down downNow') && kinds(p).includes('down newThere'));
  assert.equal(otherDeviceName('Arsenal.sav', new Set()), 'Arsenal (other device).sav');
  assert.equal(otherDeviceName('Arsenal.sav', new Set(['arsenal (other device).sav'])), 'Arsenal (other device) (2).sav');
});
