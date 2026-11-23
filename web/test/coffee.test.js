// Tests of the Buy Me a Coffee membership parts of billing.mjs: the signature, and what a membership event gives.
// Run: cd web && npm test
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { applyMembership, checkCoffeeSignature, memberKey, membershipOf } from '../billing.mjs';
import { coffeeFromEnv, createCloud } from '../cloud.mjs';
import { levelsFromEnv } from '../levels.mjs';
import { createS3 } from '../s3.mjs';
import { createStats } from '../stats.mjs';
import { createApp } from '../server.mjs';
import { coffeeEvent, fakeGoogle, fakeS3, signCoffee } from './fakes.mjs';

const SECRET = 'bmc_signing_secret_test';
const DAY = 86400e3, SESSION = 's'.repeat(40), ADMIN = 'an-admin-token-of-16-chars';
let tmp, s3fake, google, base, server, logs;

before(async () => {
  tmp = mkdtempSync(join(tmpdir(), 'cm0102-coffee-test-'));
  mkdirSync(join(tmp, 'game'));
  writeFileSync(join(tmp, 'manifest.tsv'), '');
  s3fake = await fakeS3();
  google = await fakeGoogle();
  logs = [];
  const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const cloud = createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: SESSION, google: google.google,
    price: '$5 a month', log: (t) => logs.push(t),
    coffee: { secret: SECRET, url: 'https://buymeacoffee.com/someone/membership', levels: levelsFromEnv({}) } });
  server = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv'), cloud,
    stats: createStats(), adminToken: ADMIN,
    site: { seller: 'A. Seller', contact: 'help@example.com', price: '$5 a month' } });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  base = `http://127.0.0.1:${server.address().port}`;
});

after(() => { server.close(); s3fake.close(); google.close(); rmSync(tmp, { recursive: true, force: true }); });

async function signIn(user) {
  google.user = user;
  const start = await fetch(`${base}/auth/google/start`, { redirect: 'manual' });
  const oauth = start.headers.getSetCookie()[0].split(';')[0];
  const atGoogle = await fetch(start.headers.get('location'), { redirect: 'manual' });
  const back = await fetch(atGoogle.headers.get('location'), { redirect: 'manual', headers: { cookie: oauth } });
  const cookie = back.headers.getSetCookie().filter((c) => c.startsWith('cm_session=')).map((c) => c.split(';')[0]).join('; ');
  return {
    me: async () => (await fetch(`${base}/api/me`, { headers: { cookie } })).json(),
    send: (method, path) => fetch(`${base}${path}`, { method, headers: { cookie, 'x-cm': '1' } }),
  };
}
const hook = (event, { secret = SECRET, raw = JSON.stringify(event) } = {}) => fetch(`${base}/api/coffee/webhook`,
  { method: 'POST', body: raw, headers: { 'content-type': 'application/json', 'x-signature-sha256': signCoffee(raw, secret) } });

test('the signature: the hex HMAC of the raw body, and nothing else', () => {
  const raw = JSON.stringify(coffeeEvent('membership.started'));
  assert.equal(checkCoffeeSignature(signCoffee(raw, SECRET), raw, SECRET), null);
  assert.equal(checkCoffeeSignature(signCoffee(raw, SECRET).toUpperCase(), Buffer.from(raw), SECRET), null);
  assert.match(checkCoffeeSignature(undefined, raw, SECRET), /no x-signature-sha256/);
  assert.match(checkCoffeeSignature(signCoffee(raw, 'other'), raw, SECRET), /does not match/);
  assert.match(checkCoffeeSignature(signCoffee(raw, SECRET), raw + ' ', SECRET), /does not match/);
  assert.match(checkCoffeeSignature('abc', raw, SECRET), /does not match/);
});

test('the member key is the same for any case and spaces of an email, and does not hold the email', () => {
  assert.equal(memberKey(' Fan@Example.COM '), memberKey('fan@example.com'));
  assert.notEqual(memberKey('fan@example.com'), memberKey('other@example.com'));
  assert.doesNotMatch(memberKey('fan@example.com'), /fan|example/);
  assert.match(memberKey('fan@example.com'), /^members\/[0-9a-f]{64}\.json$/);
});

test('what a membership gives: active runs to the period end and three days; canceled or paused ends it', () => {
  const now = Date.parse('2026-11-17T12:00:00Z'), end = Math.floor(now / 1000) + 30 * 86400;
  const on = membershipOf(coffeeEvent('membership.started', { end }).data, now);
  assert.equal(on.on, true);
  assert.equal(on.until, new Date(end * 1000 + 3 * DAY).toISOString());
  assert.equal(on.cancelAt, null);
  const leaving = membershipOf(coffeeEvent('membership.updated', { end, atPeriodEnd: true }).data, now);
  assert.deepEqual([leaving.on, leaving.until, leaving.cancelAt], [true, new Date(end * 1000).toISOString(), new Date(end * 1000).toISOString()]);
  // Canceled at the period's end: on to the end of what was paid; canceled at once: off.
  const later = membershipOf(coffeeEvent('membership.cancelled', { status: 'canceled', end, atPeriodEnd: true }).data, now);
  assert.deepEqual([later.on, later.until], [true, new Date(end * 1000).toISOString()]);
  assert.equal(membershipOf(coffeeEvent('membership.cancelled', { status: 'canceled', end }).data, now).on, false);
  assert.equal(membershipOf(coffeeEvent('membership.paused', { status: 'paused', end }).data, now).on, false);
  // Times in milliseconds are read as such too.
  assert.equal(membershipOf({ ...coffeeEvent('membership.started').data, current_period_end: end * 1000 }, now).until,
    new Date(end * 1000 + 3 * DAY).toISOString());
});

test('a membership record: written from an event, an older event of the same membership changes nothing', () => {
  const now = Date.parse('2026-11-17T12:00:00Z'), t = Math.floor(now / 1000);
  const first = applyMembership(null, coffeeEvent('membership.started', { created: t }), now);
  assert.equal(first.email, 'fan@example.com');
  assert.deepEqual([first.on, first.id, first.level, first.levelId, first.status, first.eventAt], [true, 501, 'Squad Player', 2, 'active', t]);
  const paused = applyMembership(first, coffeeEvent('membership.paused', { status: 'paused', created: t + 60 }), now);
  assert.equal(paused.on, false);
  assert.equal(applyMembership(paused, coffeeEvent('membership.updated', { created: t + 30 }), now), null);
  // Another membership of the same email (joined again) is taken whatever its time.
  assert.equal(applyMembership(paused, coffeeEvent('membership.started', { id: 777, created: t }), now).on, true);
});

test('the settings: a signing secret and an https membership page, from BMC_MEMBERSHIP_URL or the coffee page', () => {
  assert.equal(coffeeFromEnv({}), null);
  assert.equal(coffeeFromEnv({ BMC_WEBHOOK_SECRET: 'x' }), null, 'no page to send players to');
  assert.equal(coffeeFromEnv({ BMC_WEBHOOK_SECRET: 'x', CM_COFFEE_URL: 'http://buymeacoffee.com/a' }), null, 'not https');
  assert.deepEqual(coffeeFromEnv({ BMC_WEBHOOK_SECRET: 'x', CM_COFFEE_URL: 'https://buymeacoffee.com/a/' }),
    { secret: 'x', url: 'https://buymeacoffee.com/a/membership', levels: { names: ['supporter', 'squad player', 'club legend'], legacyCloud: null } });
  assert.deepEqual(coffeeFromEnv({ BMC_WEBHOOK_SECRET: 'x', BMC_MEMBERSHIP_URL: 'https://buymeacoffee.com/a/membership/9', BMC_CLOUD_LEVELS: ' Squad Player, 7 ,' }),
    { secret: 'x', url: 'https://buymeacoffee.com/a/membership/9', levels: { names: null, legacyCloud: ['squad player', '7'] } });
});

test('join, cancel at the period end, pause: the webhooks of Buy Me a Coffee turn Cloud Saves on and off by email', async () => {
  const out = await (await fetch(`${base}/api/me`)).json();
  assert.deepEqual(out.offer, { price: '$5 a month', free: false, url: 'https://buymeacoffee.com/someone/membership', coffee: true, paddle: null });
  const b = await signIn({ sub: '7000001', email: 'Fan@Example.com', email_verified: true, name: 'A Fan' });
  assert.equal((await b.me()).user.cloud, false);
  // Refused: no signature, another secret.
  const raw = JSON.stringify(coffeeEvent('membership.started'));
  assert.equal((await fetch(`${base}/api/coffee/webhook`, { method: 'POST', body: raw })).status, 401);
  assert.equal((await hook(coffeeEvent('membership.started'), { secret: 'other' })).status, 401);
  assert.equal((await b.me()).user.cloud, false);
  // A test event of the dashboard, and a coffee: taken, nothing changes.
  assert.deepEqual(await (await hook(coffeeEvent('membership.started', { live: false }))).json(), { test: true });
  assert.deepEqual(await (await hook({ ...coffeeEvent('donation.created'), data: { supporter_email: 'fan@example.com' } })).json(), { ignored: 'donation.created' });
  assert.equal((await b.me()).user.cloud, false);
  // Joined (the email in another case): Cloud Saves on, and the account dialog has the end of the period.
  const t = Math.floor(Date.now() / 1000), end = t + 30 * 86400;
  assert.equal((await hook(coffeeEvent('membership.started', { email: 'fan@EXAMPLE.com ', created: t, end }))).status, 200);
  let me = (await b.me()).user;
  assert.deepEqual([me.cloud, me.cloudFrom, me.cancelAt], [true, 'membership', null]);
  assert.equal(me.until, new Date(end * 1000 + 3 * DAY).toISOString());
  assert.equal((await b.send('GET', '/api/saves')).status, 200);
  assert.ok(s3fake.objects.has(memberKey('fan@example.com')));
  assert.doesNotMatch(logs.join('\n'), /fan@example/i, 'no email in the log');
  // Canceled for the end of the period: on to that end.
  await hook(coffeeEvent('membership.updated', { created: t + 10, end, atPeriodEnd: true }));
  me = (await b.me()).user;
  assert.deepEqual([me.cloud, me.cancelAt], [true, new Date(end * 1000).toISOString()]);
  // A retry of an older event changes nothing; a pause ends them.
  assert.deepEqual(await (await hook(coffeeEvent('membership.started', { created: t - 10, end }))).json(), { ignored: 'older than the last event' });
  await hook(coffeeEvent('membership.paused', { status: 'paused', created: t + 20, end }));
  me = (await b.me()).user;
  assert.equal(me.cloud, false);
  assert.equal((await b.send('GET', '/api/saves')).status, 403);
});

test('a level that gives no Cloud Saves, and a member who signs in after joining', async () => {
  // Joined before the account existed: the membership waits for the email.
  await hook(coffeeEvent('membership.started', { email: 'early@example.com', id: 610 }));
  const early = await signIn({ sub: '7000002', email: 'early@example.com', email_verified: true, name: 'Early' });
  assert.equal((await early.me()).user.cloud, true);
  // The Supporter level: a member, but no Cloud Saves; the page is told so.
  await hook(coffeeEvent('membership.started', { email: 'small@example.com', id: 620, level: 'Supporter', levelId: 1 }));
  const small = await signIn({ sub: '7000003', email: 'small@example.com', email_verified: true, name: 'Small' });
  const me = (await small.me()).user;
  assert.equal(me.cloud, false);
  assert.deepEqual(me.member, { level: 'Supporter', on: false, until: me.member.until, siteLevel: 'supporter' });
  // A running membership must be canceled before the account goes; one canceled lets it go, and its record with it.
  assert.equal((await early.send('DELETE', '/api/account')).status, 409);
  await hook(coffeeEvent('membership.cancelled', { email: 'early@example.com', id: 610, status: 'canceled', created: Math.floor(Date.now() / 1000) + 5 }));
  assert.equal((await early.send('DELETE', '/api/account')).status, 204);
  assert.ok(!s3fake.objects.has(memberKey('early@example.com')));
});

test('the admin page: every membership the server was told of, and the last notification of Buy Me a Coffee', async () => {
  const members = async (headers = { authorization: `Bearer ${ADMIN}` }) => fetch(`${base}/api/admin/members`, { headers });
  assert.equal((await members({})).status, 401, 'only for an admin');
  const t = Math.floor(Date.now() / 1000);
  await hook(coffeeEvent('membership.started', { email: 'New@Example.com', id: 630, created: t, end: t + 30 * 86400 }));
  let got = await (await members()).json();
  assert.equal(got.on, true);
  assert.deepEqual(got.levels, { names: ['supporter', 'squad player', 'club legend'], legacyCloud: null });
  const fresh = got.members.find((m) => m.email === 'new@example.com');
  assert.deepEqual([fresh.level, fresh.status, fresh.cloud, fresh.levelGives], ['Squad Player', 'active', true, true]);
  assert.equal(fresh.until, new Date((t + 30 * 86400) * 1000 + 3 * DAY).toISOString());
  const small = got.members.find((m) => m.email === 'small@example.com');
  assert.deepEqual([small.level, small.siteLevel, small.cloud, small.levelGives, small.known], ['Supporter', 'supporter', false, false, true], 'a level that gives no Membership');
  assert.equal(got.members[0].email, 'new@example.com', 'the newest first');
  assert.deepEqual([got.last.type, got.last.result], ['membership.started', 'Membership on (Squad Player)']);
  // A one-off coffee: not a membership, and the page says so; a refused one says why.
  await hook({ ...coffeeEvent('donation.created'), data: { supporter_email: 'once@example.com' } });
  got = await (await members()).json();
  assert.equal(got.last.type, 'donation.created');
  assert.match(got.last.result, /not a membership/);
  assert.ok(!got.members.some((m) => m.email === 'once@example.com'));
  await hook(coffeeEvent('membership.started'), { secret: 'other' });
  got = await (await members()).json();
  assert.match(got.last.result, /refused.*does not match/);
  assert.doesNotMatch(JSON.stringify(got.last), /@/, 'no email in the last notification');
});

const admin = (body) => fetch(`${base}/api/admin/cloud`, { method: 'POST', body: JSON.stringify(body),
  headers: { 'content-type': 'application/json', 'x-cm': '1', authorization: `Bearer ${ADMIN}` } });

test('levels: a Club Legend, a Squad Player, a Supporter, an unknown level', async () => {
  const t = Math.floor(Date.now() / 1000);
  const four = [['legend@example.com', 'Club Legend', 701, 'legend', true], ['squad@example.com', 'Squad Player', 702, 'squad', true],
    ['sup@example.com', 'Supporter', 703, 'supporter', false], ['gold@example.com', 'Gold', 704, 'supporter', false]];
  for (const [email, level, id, want, cloud] of four) {
    await hook(coffeeEvent('membership.started', { email, id, level, levelId: id, created: t }));
    const b = await signIn({ sub: String(8000000 + id), email, email_verified: true, name: level });
    const me = (await b.me()).user;
    assert.deepEqual([me.level, me.cloud], [want, cloud], level);
  }
  const got = await (await fetch(`${base}/api/admin/members`, { headers: { authorization: `Bearer ${ADMIN}` } })).json();
  assert.equal(got.members.find((m) => m.email === 'gold@example.com').known, false);
  assert.equal(got.members.find((m) => m.email === 'legend@example.com').siteLevel, 'legend');
  assert.match(got.last.result, /not in CM_LEVELS/, 'the last one, Gold');
});

test('a gift and a paid level: the higher counts; taking the gift back leaves the paid one', async () => {
  const b = await signIn({ sub: '8000702', email: 'squad@example.com', email_verified: true, name: 'Squad' });
  assert.equal((await b.me()).user.level, 'squad');
  let r = await admin({ id: 'g:8000702', level: 'legend' });
  assert.equal(r.status, 200);
  const plan = (await r.json()).plan;
  assert.deepEqual([plan.level, plan.givenLevel, plan.paidLevel, plan.given], ['legend', 'legend', 'squad', true]);
  assert.equal((await b.me()).user.level, 'legend');
  assert.equal((await admin({ id: 'g:8000702', level: 'supporter' })).status, 400, 'Supporter is not given');
  r = await admin({ id: 'g:8000702', level: null });
  const me = (await b.me()).user;
  assert.deepEqual([me.level, me.cloud], ['squad', true]);
  // The first version's body still works: on true is Squad Player.
  assert.equal((await (await admin({ id: 'g:8000703', on: true })).json()).plan.level, 'squad');
  await admin({ id: 'g:8000703', on: false });
});

test('BMC_CLOUD_LEVELS alone still works: its levels are Squad Player, the others Supporter', async () => {
  const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const old = createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: SESSION, google: google.google, log: () => {},
    coffee: { secret: SECRET, url: 'https://buymeacoffee.com/someone/membership', levels: levelsFromEnv({ BMC_CLOUD_LEVELS: 'Squad Player' }) } });
  assert.equal((await old.planOf({ sub: '8000701', email: 'legend@example.com' }, true)).level, 'supporter');
  assert.equal((await old.planOf({ sub: '8000702', email: 'squad@example.com' }, true)).level, 'squad');
});
