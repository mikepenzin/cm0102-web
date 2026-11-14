// Tests of the payments (billing.mjs, and the Paddle parts of cloud.mjs) against stand-ins of Google, the S3 store
// and Paddle's API. Run: cd web && npm test
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { createHmac } from 'node:crypto';
import http from 'node:http';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { accountRef, applySubscription, checkPaddleSignature, openAccountRef, planOf } from '../billing.mjs';
import { createCloud } from '../cloud.mjs';
import { createS3 } from '../s3.mjs';
import { createApp } from '../server.mjs';
import { fakeGoogle, fakeS3, paddleEvent, signPaddle } from './fakes.mjs';

const SECRET = 'pdl_ntfset_test_secret', SESSION = 's'.repeat(40);
let tmp, s3fake, google, paddleApi, base, server, logs;
const portalCalls = [];

before(async () => {
  tmp = mkdtempSync(join(tmpdir(), 'cm0102-billing-test-'));
  mkdirSync(join(tmp, 'game'));
  writeFileSync(join(tmp, 'manifest.tsv'), '');
  s3fake = await fakeS3();
  google = await fakeGoogle();
  // Paddle's API: the customer portal session.
  paddleApi = http.createServer((req, res) => {
    let body = '';
    req.on('data', (c) => { body += c; });
    req.on('end', () => {
      portalCalls.push({ url: req.url, auth: req.headers.authorization, body: JSON.parse(body || '{}') });
      res.writeHead(201, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ data: { id: 'cpls_1', urls: { general: { overview: 'https://customer-portal.paddle.com/cpl_x?token=y' } } } }));
    });
  });
  await new Promise((r) => paddleApi.listen(0, '127.0.0.1', r));
  logs = [];
  const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const cloud = createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: SESSION, google: google.google,
    price: '$5 a month', log: (t) => logs.push(t),
    paddle: { env: 'sandbox', clientToken: 'test_client_token', priceIds: ['pri_cloud_month'], webhookSecret: SECRET, apiKey: 'pdl_sdbx_apikey',
      apiBase: `http://127.0.0.1:${paddleApi.address().port}` } });
  server = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv'), cloud,
    site: { seller: 'A. Seller', contact: 'help@example.com', price: '$5 a month' } });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  base = `http://127.0.0.1:${server.address().port}`;
});

after(() => { server.close(); paddleApi.close(); s3fake.close(); google.close(); rmSync(tmp, { recursive: true, force: true }); });

async function signIn(user) {
  google.user = user;
  const jar = [];
  const start = await fetch(`${base}/auth/google/start`, { redirect: 'manual' });
  const oauth = start.headers.getSetCookie()[0].split(';')[0];
  const atGoogle = await fetch(start.headers.get('location'), { redirect: 'manual' });
  const back = await fetch(atGoogle.headers.get('location'), { redirect: 'manual', headers: { cookie: oauth } });
  for (const c of back.headers.getSetCookie()) if (c.startsWith('cm_session=')) jar.push(c.split(';')[0]);
  const cookie = jar.join('; ');
  return {
    cookie,
    me: async () => (await fetch(`${base}/api/me`, { headers: { cookie } })).json(),
    send: (method, path) => fetch(`${base}${path}`, { method, headers: { cookie, 'x-cm': '1' } }),
  };
}

const hook = (event, { secret = SECRET, ts } = {}) => {
  const raw = JSON.stringify(event);
  return fetch(`${base}/api/paddle/webhook`, { method: 'POST', body: raw,
    headers: { 'content-type': 'application/json', 'paddle-signature': signPaddle(raw, secret, ts) } });
};

test('the Paddle signature: the right secret, a fresh time, any of the h1 of a rotation', () => {
  const raw = Buffer.from('{"a":1}');
  const ts = Math.floor(Date.now() / 1000);
  const h = createHmac('sha256', SECRET).update(`${ts}:{"a":1}`).digest('hex');
  assert.equal(checkPaddleSignature(`ts=${ts};h1=${h}`, raw, SECRET), null);
  assert.equal(checkPaddleSignature(`ts=${ts};h1=${'0'.repeat(64)};h1=${h}`, raw, SECRET), null, 'a second h1 while the secret rotates');
  assert.match(checkPaddleSignature(`ts=${ts};h1=${h}`, Buffer.from('{"a":2}'), SECRET), /does not match/);
  assert.match(checkPaddleSignature(`ts=${ts};h1=${h}`, raw, 'another'), /does not match/);
  assert.match(checkPaddleSignature(`ts=${ts - 3600};h1=${createHmac('sha256', SECRET).update(`${ts - 3600}:{"a":1}`).digest('hex')}`, raw, SECRET), /too old/);
  assert.match(checkPaddleSignature('', raw, SECRET), /no Paddle-Signature/);
  assert.match(checkPaddleSignature(`h1=${h}`, raw, SECRET), /without ts/);
});

test('the account mark of the checkout: only this server makes one that holds', () => {
  const ref = accountRef('1234567890', SESSION);
  assert.equal(openAccountRef(ref, SESSION), '1234567890');
  assert.equal(openAccountRef(ref, 'another secret of another server'), null);
  assert.equal(openAccountRef(ref.replace('1234567890', '1234567891'), SESSION), null, 'another account with the same mark');
  assert.equal(openAccountRef('1234567890', SESSION), null);
  assert.equal(openAccountRef(undefined, SESSION), null);
});

test('what a subscription gives: active to the end of its period and three days, due a week, canceled nothing', () => {
  const now = Date.parse('2026-11-10T12:00:00Z');
  const period = { starts_at: '2026-11-10T12:00:00Z', ends_at: '2026-12-10T12:00:00Z' };
  assert.deepEqual(planOf({ status: 'active', current_billing_period: period }, now), { plan: 'cloud', planUntil: '2026-12-13T12:00:00.000Z', cancelAt: null });
  assert.deepEqual(planOf({ status: 'active', current_billing_period: period, scheduled_change: { action: 'cancel', effective_at: '2026-12-10T12:00:00Z' } }, now),
    { plan: 'cloud', planUntil: '2026-12-10T12:00:00.000Z', cancelAt: '2026-12-10T12:00:00Z' }, 'canceled for the end of the period');
  assert.equal(planOf({ status: 'past_due', current_billing_period: { ends_at: '2026-11-09T00:00:00Z' } }, now).planUntil, '2026-11-17T12:00:00.000Z');
  assert.deepEqual(planOf({ status: 'canceled' }, now), { plan: 'free', planUntil: null, cancelAt: null });
  assert.equal(planOf({ status: 'paused' }, now).plan, 'free');
  // An older event than the last one of the subscription changes nothing.
  const rec = applySubscription({ sub: '1', given: { by: 'boss@example.com' } }, { occurred_at: '2026-11-10T12:00:05Z', data: { id: 'sub_1', status: 'canceled', customer_id: 'ctm_1' } }, now);
  assert.deepEqual(rec.given, { by: 'boss@example.com' }, 'an event of Paddle keeps Cloud Saves an admin gave');
  assert.equal(applySubscription(rec, { occurred_at: '2026-11-10T12:00:01Z', data: { id: 'sub_1', status: 'active', current_billing_period: period } }, now), null);
});

test('buy, renew, cancel: the webhooks of Paddle turn Cloud Saves on and off', async () => {
  const a = await signIn({ sub: '7000007', email: 'buyer@example.com', email_verified: true, name: 'Buyer' });
  let me = await a.me();
  assert.equal(me.user.cloud, false);
  assert.deepEqual(me.offer, { price: '$5 a month', free: false, url: null, paddle: { env: 'sandbox', token: 'test_client_token', priceId: 'pri_cloud_month' } });
  assert.equal(openAccountRef(me.user.ref, SESSION), '7000007', 'the page gets the mark for the checkout');
  assert.equal((await a.send('GET', '/api/saves')).status, 403);
  // Signed out: the offer too, so the page can show the price.
  assert.equal((await (await fetch(`${base}/api/me`)).json()).offer.price, '$5 a month');

  // Not from Paddle: refused.
  assert.equal((await hook(paddleEvent('subscription.created', { ref: me.user.ref }), { secret: 'not-it' })).status, 401);
  assert.equal((await a.me()).user.cloud, false);
  // Paddle: the subscription is created and active.
  let r = await hook(paddleEvent('subscription.created', { ref: me.user.ref, at: '2026-11-10T12:00:00Z' }));
  assert.equal(r.status, 200);
  me = await a.me();
  assert.equal(me.user.cloud, true);
  assert.equal(me.user.cloudFrom, 'subscription');
  assert.equal(me.user.manage, true);
  assert.ok(Date.parse(me.user.until) > Date.now() + 25 * 86400e3);
  assert.equal((await a.send('GET', '/api/saves')).status, 200, 'cloud saves work');
  const record = JSON.parse(s3fake.objects.get('accounts/7000007.json').body);
  assert.equal(record.paddle.subscriptionId, 'sub_test_1');
  assert.equal(record.paddle.customerId, 'ctm_test_1');
  assert.equal(record.email, 'buyer@example.com', 'the account record keeps its fields');

  // A later sign-in keeps the plan.
  const again = await signIn({ sub: '7000007', email: 'buyer@example.com', email_verified: true, name: 'Buyer' });
  assert.equal((await again.me()).user.cloud, true);

  // Manage subscription: a link into Paddle's portal for this customer and subscription.
  r = await a.send('POST', '/api/billing/portal');
  assert.equal(r.status, 200);
  assert.equal((await r.json()).url, 'https://customer-portal.paddle.com/cpl_x?token=y');
  assert.deepEqual(portalCalls.at(-1), { url: '/customers/ctm_test_1/portal-sessions', auth: 'Bearer pdl_sdbx_apikey', body: { subscription_ids: ['sub_test_1'] } });

  // Deleting the account while the subscription runs: refused, cancel first.
  r = await a.send('DELETE', '/api/account');
  assert.equal(r.status, 409);
  assert.match((await r.json()).error, /Cancel your Cloud Saves subscription first/);

  // Canceled for the end of the period: on until then.
  await hook(paddleEvent('subscription.updated', { ref: me.user.ref, at: '2026-11-11T12:00:00Z', cancelAt: new Date(Date.now() + 20 * 86400e3).toISOString() }));
  me = await a.me();
  assert.equal(me.user.cloud, true);
  assert.ok(me.user.cancelAt);
  // An event older than that one, delivered late: changes nothing.
  await hook(paddleEvent('subscription.updated', { ref: me.user.ref, at: '2026-11-10T13:00:00Z' }));
  assert.ok((await a.me()).user.cancelAt, 'the late event did not undo the cancel');
  // The cancel takes effect.
  await hook(paddleEvent('subscription.canceled', { ref: me.user.ref, at: '2026-12-10T12:00:00Z', status: 'canceled' }));
  me = await a.me();
  assert.equal(me.user.cloud, false);
  assert.equal((await a.send('GET', '/api/saves')).status, 403);
  // Now the account can be deleted.
  assert.equal((await a.send('DELETE', '/api/account')).status, 204);
});

test('a webhook for another price, without this server\'s mark, or not about a subscription changes nothing', async () => {
  const b = await signIn({ sub: '8000008', email: 'other@example.com', email_verified: true, name: 'Other' });
  const ref = (await b.me()).user.ref;
  assert.equal((await (await hook(paddleEvent('subscription.created', { ref, price: 'pri_something_else' }))).json()).ignored, 'another price');
  assert.equal((await (await hook(paddleEvent('subscription.created', { ref: '8000008.forged' }))).json()).ignored, 'no account');
  assert.equal((await (await hook({ event_type: 'transaction.completed', occurred_at: new Date().toISOString(), data: { id: 'txn_1' } })).json()).ignored, 'transaction.completed');
  assert.equal((await b.me()).user.cloud, false);
});

test('the pages Paddle checks: pricing, terms, refunds, privacy, with the seller, the contact and the price', async () => {
  for (const [page, want] of [['pricing', /\$5 a month/], ['terms', /Paddle\.com is the Merchant of Record for all our\s+orders/],
    ['refunds', /full refund within 14 days of any payment/], ['privacy', /Paddle/]]) {
    const r = await fetch(`${base}/${page}`);
    assert.equal(r.status, 200, page);
    const html = await r.text();
    assert.match(html, want, page);
    assert.doesNotMatch(html, /\{\{/, `${page}: every placeholder is filled`);
    if (page !== 'pricing') assert.match(html, /help@example\.com/, `${page}: the contact`);
  }
  assert.match(await (await fetch(`${base}/terms`)).text(), /A\. Seller/);
  assert.equal((await fetch(`${base}/terms.html`)).status, 200);
});
