// Accounts and cloud saves (web/README.md, "Cloud saves"): sign in with Google, and the saves of an account in an S3
// store (Tigris on Fly.io). Without its settings the server runs as before: /api/me answers { auth: false } and the
// page shows no account.
//
// Who has cloud saves: CM_CLOUD_FOR=everyone gives them to every signed-in account; else the emails in CM_CLOUD_USERS
// (comma-separated) and the accounts whose record says plan "cloud" until planUntil (accounts/<id>.json in the store:
// set by a Paddle subscription, billing.mjs, or by hand) have them. Everyone else keeps saves in the browser only.
//
// The store holds:
//   accounts/<google id>.json            { sub, email, name, created, seen, plan, planUntil }
//   saves/<google id>/<name, base64url>.gz the save, gzip-compressed by the page; metadata raw-size and saved (ms)
import { createGunzip } from 'node:zlib';
import { GOOGLE, SignInError, googleFinish, googleStart, readCookies, sealer, setCookie } from './auth.mjs';
import { PADDLE_API, accountRef, applySubscription, checkPaddleSignature, openAccountRef, portalUrl } from './billing.mjs';
import { S3Error, s3FromEnv } from './s3.mjs';
import { SAVE_HEADER, checkSave } from './saves.js';
import { DATA_SET_FILES } from './dataset.js';
import { zipEntries } from './zip.js';

const SESSION = 'cm_session', OAUTH = 'cm_oauth';
const SESSION_DAYS = 30;
export const MAX_GZ = 64 << 20;            // what one upload may be, compressed
export const MAX_RAW = 256 << 20;          // and uncompressed (a save of the full database is some tens of MB)
export const MAX_SET_ZIP = 48 << 20;       // a data set as a zip (the 3.9.68 data: about 61 MB, some 20 MB zipped)

const b64u = (s) => Buffer.from(s, 'utf8').toString('base64url');
const unb64u = (s) => Buffer.from(s, 'base64url').toString('utf8');

// A data set's name as the store takes it: a folder name of Databases (datasets.js cleanName).
export function validSetName(name) {
  return typeof name === 'string' && name.length >= 1 && name.length <= 60 && !/[\u0000-\u001f\u007f\\/:*?"<>|]/.test(name)
    && name.trim() === name && !name.startsWith('.');
}

// What a zip of a data set holds: null and its size, or why it is not one (the 20 files in one folder).
export function inspectSetZip(zip) {
  let entries;
  try { entries = zipEntries(zip); } catch (e) { return { why: e.message }; }
  const by = new Map();
  for (const e of entries) {
    const i = e.name.lastIndexOf('/'), base = e.name.slice(i + 1).toLowerCase();
    if (DATA_SET_FILES.includes(base)) by.set(base, e);
  }
  if (by.size !== DATA_SET_FILES.length) return { why: `it does not hold the ${DATA_SET_FILES.length} database files` };
  return { why: null, rawSize: [...by.values()].reduce((a, e) => a + e.size, 0) };
}

// A save's name as the store takes it: a .sav file name of the save folder, without a folder part.
export function validSaveName(name) {
  return typeof name === 'string' && name.length >= 5 && name.length <= 120 && /\.sav$/i.test(name)
    && !/[\u0000-\u001f\\/:*?"<>|]/.test(name) && name.trim() === name && name.toLowerCase() !== 'temp_save_file.sav';
}

function send(res, status, body, headers = {}) {
  const text = body === undefined ? '' : JSON.stringify(body);
  res.writeHead(status, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store', ...headers });
  res.end(text);
}

function readBody(req, limit) {
  return new Promise((resolveBody, reject) => {
    const chunks = [];
    let n = 0, over = false;
    req.on('data', (c) => { n += c.length; if (n > limit) over = true; else chunks.push(c); });
    req.on('end', () => (over ? reject(Object.assign(new Error('too large'), { status: 413 })) : resolveBody(Buffer.concat(chunks))));
    req.on('error', reject);
  });
}

// What the gzip body holds: its length and whether it is a saved game of CM 01/02 (the header of saves.js).
function inspectSave(gz) {
  return new Promise((resolveInspect) => {
    const g = createGunzip();
    const head = [];
    let total = 0, headLen = 0, settled = false;
    const done = (v) => { if (!settled) { settled = true; resolveInspect(v); } };
    g.on('data', (c) => {
      total += c.length;
      if (headLen < SAVE_HEADER) { head.push(c); headLen += c.length; }
      if (total > MAX_RAW) { done({ why: `it is larger than ${MAX_RAW >> 20} MB` }); g.destroy(); }
    });
    g.on('end', () => {
      const h = Buffer.concat(head);
      done({ why: checkSave(new Uint8Array(h.buffer, h.byteOffset, h.length), total), total });
    });
    g.on('error', () => done({ why: 'the upload is not gzip data' }));
    g.end(gz);
  });
}

const popupPage = (ok, why) => `<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>${ok ? 'Signed in' : 'Not signed in'}</title><body style="font:16px system-ui;background:#0b0d10;color:#e8eaed;padding:24px">
<p>${ok ? 'Signed in. This window closes by itself.' : `Not signed in: ${String(why).replace(/[&<>"']/g, '')}`}</p>
<script>try { new BroadcastChannel('cm-account').postMessage(${JSON.stringify({ ok, why: ok ? null : String(why) })}); } catch {}
${ok ? 'setTimeout(() => window.close(), 300);' : ''}</script>`;

// paddle: { env, clientToken, priceIds, webhookSecret, apiKey, apiBase } to sell Cloud Saves with Paddle, or null.
// price: how the page names the price ("$5 a month").
export function createCloud({ s3, clientId, clientSecret, sessionSecret, publicUrl = null, cloudFor = 'listed', cloudUsers = [],
  upgradeUrl = null, maxSaves = 30, maxSets = 5, google = GOOGLE, paddle = null, price = '$5 a month', log = (t) => console.log(t),
  onAccountDeleted = () => {}, uploadBudget = 160 << 20, isAdmin = () => false }) {
  const seal = sealer(sessionSecret);
  // The uploads in memory at once (readBody holds each whole, for its check and its signature): a save is at most
  // MAX_GZ, a data set MAX_SET_ZIP. Past uploadBudget an upload is turned away with 503 and goes at the next sync, so a
  // few players saving at once cannot run the server's small machine out of memory (web/deploy/README.md).
  let held = 0;
  const hold = (req, res, max) => {
    const n = Math.min(Number(req.headers['content-length']) || max, max);
    if (held + n > uploadBudget && held > 0) {
      req.resume();
      res.writeHead(503, { 'Content-Type': 'application/json', 'Retry-After': '30', 'Cache-Control': 'no-store' });
      res.end(JSON.stringify({ error: 'the server is busy with other uploads: it goes at the next sync' }));
      return null;
    }
    held += n;
    let freed = false;
    return () => { if (!freed) { freed = true; held -= n; } };
  };
  const users = new Set(cloudUsers.map((e) => e.trim().toLowerCase()).filter(Boolean));
  const accounts = new Map();               // google id -> { account, at }: the record, read at most once a minute

  const origin = (req) => publicUrl ?? `${String(req.headers['x-forwarded-proto'] ?? 'http').split(',')[0].trim()}://${req.headers.host}`;
  const secure = (req) => origin(req).startsWith('https:');
  const sessionOf = (req) => seal.open('s', readCookies(req)[SESSION]);
  const accountKey = (sub) => `accounts/${sub}.json`;
  const saveKey = (sub, name) => `saves/${sub}/${b64u(name)}.gz`;
  const setKey = (sub, name) => `sets/${sub}/${b64u(name)}.zip`;

  async function account(sub, fresh = false) {
    const c = accounts.get(sub);
    if (!fresh && c && Date.now() - c.at < 60000) return c.account;
    const buf = await s3.getBuffer(accountKey(sub));
    const a = buf ? JSON.parse(buf.toString()) : null;
    accounts.set(sub, { account: a, at: Date.now() });
    return a;
  }

  // { cloud, from: 'subscription' | 'given' | null, record }.
  async function planOf(user) {
    let a = null;
    try { a = await account(user.sub); } catch (err) { log(`cloud: account ${user.sub} not read: ${err.message}`); }
    if (cloudFor === 'everyone' || users.has(user.email)) return { cloud: true, from: 'given', record: a };
    const on = a?.plan === 'cloud' && (!a.planUntil || Date.parse(a.planUntil) > Date.now());
    return { cloud: on, from: on ? (a.paddle ? 'subscription' : 'given') : null, record: a };
  }
  const hasCloud = async (user) => (await planOf(user)).cloud;

  // What the page offers: the price, and how to buy (Paddle's checkout, or a link).
  const offer = () => cloudFor === 'everyone'
    ? { price: 'Free with an account', free: true, paddle: null, url: null }
    : { price, free: false, url: upgradeUrl,
      paddle: paddle ? { env: paddle.env, token: paddle.clientToken, priceId: paddle.priceIds[0] } : null };

  function finishSignIn(res, mode, ok, why = null) {
    if (mode === 'popup') {
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
      res.end(popupPage(ok, why));
    } else {
      res.writeHead(303, { Location: ok ? '/?account=signed-in' : `/?account=error&why=${encodeURIComponent(why)}`, 'Cache-Control': 'no-store' });
      res.end();
    }
  }

  async function signInStart(req, res, url) {
    const mode = url.searchParams.get('mode') === 'popup' ? 'popup' : 'page';
    const start = googleStart({ clientId, redirectUri: `${origin(req)}/auth/google/callback`, google });
    setCookie(res, OAUTH, seal.seal('o', { state: start.state, verifier: start.verifier, mode }, 600), { maxAge: 600, secure: secure(req) });
    res.writeHead(302, { Location: start.url, 'Cache-Control': 'no-store' });
    res.end();
  }

  async function signInCallback(req, res, url) {
    const o = seal.open('o', readCookies(req)[OAUTH]);
    setCookie(res, OAUTH, null, { secure: secure(req) });
    const mode = o?.mode ?? 'page';
    if (url.searchParams.get('error')) { finishSignIn(res, mode, false, url.searchParams.get('error') === 'access_denied' ? 'cancelled' : url.searchParams.get('error')); return; }
    if (!o || !url.searchParams.get('state') || o.state !== url.searchParams.get('state')) {
      finishSignIn(res, mode, false, 'the sign-in took too long or came from another page: try again');
      return;
    }
    let user;
    try {
      user = await googleFinish({ code: url.searchParams.get('code') ?? '', verifier: o.verifier, clientId, clientSecret,
        redirectUri: `${origin(req)}/auth/google/callback`, google });
    } catch (err) {
      log(`cloud: sign-in failed: ${err.message}`);
      finishSignIn(res, mode, false, err instanceof SignInError ? err.message : 'Google could not be reached');
      return;
    }
    try {
      const had = await account(user.sub, true);
      const now = new Date().toISOString();
      const record = { plan: 'free', planUntil: null, ...had, sub: user.sub, email: user.email, name: user.name,
        created: had?.created ?? now, seen: now };              // a plan and a subscription stay as they were
      await s3.put(accountKey(user.sub), Buffer.from(JSON.stringify(record)), { type: 'application/json' });
      accounts.set(user.sub, { account: record, at: Date.now() });
      if (!had) log(`cloud: new account ${user.sub}`);
    } catch (err) {
      log(`cloud: account ${user.sub} not written: ${err.message}`);      // the sign-in still holds; the plan is read later
    }
    setCookie(res, SESSION, seal.seal('s', user, SESSION_DAYS * 86400), { maxAge: SESSION_DAYS * 86400, secure: secure(req) });
    finishSignIn(res, mode, true);
  }

  async function me(req, res) {
    const user = sessionOf(req);
    if (!user) { send(res, 200, { auth: true, user: null, offer: offer(), maxSaves }); return; }
    const p = await planOf(user);
    setCookie(res, SESSION, seal.seal('s', user, SESSION_DAYS * 86400), { maxAge: SESSION_DAYS * 86400, secure: secure(req) });
    send(res, 200, { auth: true, offer: offer(), maxSaves, user: {
      email: user.email, name: user.name, picture: user.picture, cloud: p.cloud, cloudFrom: p.from,
      until: p.from === 'subscription' ? p.record.planUntil : null, cancelAt: p.from === 'subscription' ? p.record.paddle?.cancelAt ?? null : null,
      manage: Boolean(p.record?.paddle?.customerId), ref: paddle ? accountRef(user.sub, sessionSecret) : null,
      admin: Boolean(isAdmin(String(user.email ?? '').toLowerCase())),   // the page shows the Admin panel button
    } });
  }

  // A notification of Paddle (billing.mjs). Only subscription events change a plan; the others are taken and left.
  async function paddleWebhook(req, res) {
    if (!paddle) { req.resume(); send(res, 404, { error: 'no payments here' }); return; }
    const raw = await readBody(req, 1 << 20);
    const why = checkPaddleSignature(req.headers['paddle-signature'], raw, paddle.webhookSecret);
    if (why) { log(`cloud: Paddle notification refused: ${why}`); send(res, 401, { error: why }); return; }
    let event;
    try { event = JSON.parse(raw.toString()); } catch { send(res, 400, { error: 'not JSON' }); return; }
    if (!String(event.event_type).startsWith('subscription.') || !event.data?.id) { send(res, 200, { ignored: event.event_type }); return; }
    const sub = openAccountRef(event.data.custom_data?.account, sessionSecret);
    if (!sub) { log(`cloud: Paddle ${event.event_type} ${event.data.id}: no account of this server in custom_data`); send(res, 200, { ignored: 'no account' }); return; }
    const prices = (event.data.items ?? []).map((i) => i.price?.id);
    if (!prices.some((id) => paddle.priceIds.includes(id))) {
      log(`cloud: Paddle ${event.event_type} ${event.data.id}: price ${prices.join(',')} is not one of PADDLE_PRICE_ID`);
      send(res, 200, { ignored: 'another price' });
      return;
    }
    const record = await account(sub, true);
    const next = applySubscription(record ?? { sub, email: null, created: new Date().toISOString() }, event);
    if (!next) { send(res, 200, { ignored: 'older than the last event' }); return; }
    await s3.put(accountKey(sub), Buffer.from(JSON.stringify(next)), { type: 'application/json' });
    accounts.set(sub, { account: next, at: Date.now() });
    log(`cloud: account ${sub}: ${event.event_type}, ${event.data.status}, plan ${next.plan}${next.planUntil ? ` until ${next.planUntil}` : ''}`);
    send(res, 200, { ok: true });
  }

  async function billingPortal(res, user) {
    const record = await account(user.sub, true);
    const customerId = record?.paddle?.customerId;
    if (!customerId) { send(res, 404, { error: 'no subscription on this account' }); return; }
    if (!paddle?.apiKey) { send(res, 501, { error: 'no portal on this server', url: 'https://paddle.net' }); return; }
    const url = await portalUrl({ apiKey: paddle.apiKey, apiBase: paddle.apiBase, customerId, subscriptionId: record.paddle.subscriptionId });
    send(res, 200, { url });
  }

  async function listSaves(user) {
    const objects = (await s3.list(`saves/${user.sub}/`)).filter((o) => o.key.endsWith('.gz'));
    const saves = await Promise.all(objects.map(async (o) => {
      const name = unb64u(o.key.slice(o.key.lastIndexOf('/') + 1, -3));
      const h = await s3.head(o.key);
      if (!h || !validSaveName(name)) return null;
      return { name, etag: h.etag, size: h.size, rawSize: Number(h.metadata['raw-size']) || null,
        saved: Number(h.metadata.saved) || Date.parse(o.modified) || null };
    }));
    return saves.filter(Boolean).sort((a, b) => (b.saved ?? 0) - (a.saved ?? 0));
  }

  async function putSave(req, res, user, name) {
    const length = Number(req.headers['content-length']);
    if (length > MAX_GZ) { req.resume(); send(res, 413, { error: `a save may be ${MAX_GZ >> 20} MB at most, compressed` }); return; }
    const free = hold(req, res, MAX_GZ);
    if (!free) return;
    try { await storeSave(req, res, user, name); } finally { free(); }
  }

  async function storeSave(req, res, user, name) {
    const body = await readBody(req, MAX_GZ);
    const { why, total } = await inspectSave(body);
    if (why) { send(res, 422, { error: `${name} was not stored: ${why}` }); return; }
    const key = saveKey(user.sub, name);
    const current = await s3.head(key);
    const base = req.headers['x-base-etag'];
    if (base !== undefined && (base === 'none' ? current !== null : current?.etag !== base)) {
      send(res, 409, { error: 'changed', cloud: current && { etag: current.etag, rawSize: Number(current.metadata['raw-size']) || null,
        saved: Number(current.metadata.saved) || null } });
      return;
    }
    if (!current && (await s3.list(`saves/${user.sub}/`)).length >= maxSaves) {
      send(res, 507, { error: `the cloud holds ${maxSaves} saves at most: remove one first` });
      return;
    }
    const savedAt = Number(req.headers['x-save-time']);
    const saved = Number.isFinite(savedAt) && savedAt > 0 ? Math.floor(savedAt) : Date.now();
    const etag = await s3.put(key, body, { type: 'application/gzip', metadata: { 'raw-size': total, saved } });
    send(res, 200, { name, etag, size: body.length, rawSize: total, saved });
  }

  async function getSave(res, user, name) {
    const r = await s3.get(saveKey(user.sub, name));
    if (!r) { send(res, 404, { error: `${name} is not in the cloud` }); return; }
    res.writeHead(200, { 'Content-Type': 'application/gzip', 'Content-Length': r.headers['content-length'], ETag: r.headers.etag,
      'X-Raw-Size': r.headers['x-amz-meta-raw-size'] ?? '', 'X-Saved': r.headers['x-amz-meta-saved'] ?? '', 'Cache-Control': 'no-store' });
    r.pipe(res);
  }

  // The data sets of an account: zips of the 20 database files (datasets.js), metadata raw-size and saved.
  async function dataSets(req, res, user, path, method) {
    if (path === '/api/datasets') {
      if (method !== 'GET') { send(res, 405, { error: 'GET only' }); return; }
      const objects = (await s3.list(`sets/${user.sub}/`)).filter((o) => o.key.endsWith('.zip'));
      const sets = (await Promise.all(objects.map(async (o) => {
        const name = unb64u(o.key.slice(o.key.lastIndexOf('/') + 1, -4)), h = await s3.head(o.key);
        return h && validSetName(name) ? { name, etag: h.etag, size: h.size, rawSize: Number(h.metadata['raw-size']) || null, saved: Number(h.metadata.saved) || null } : null;
      }))).filter(Boolean).sort((a, b) => a.name.localeCompare(b.name));
      send(res, 200, { sets, maxSets });
      return;
    }
    let name;
    try { name = decodeURIComponent(path.slice('/api/datasets/'.length)); } catch { name = null; }
    if (!validSetName(name)) { req.resume(); send(res, 400, { error: 'not a data set name' }); return; }
    const key = setKey(user.sub, name);
    if (method === 'GET') {
      const r = await s3.get(key);
      if (!r) { send(res, 404, { error: `${name} is not in the cloud` }); return; }
      res.writeHead(200, { 'Content-Type': 'application/zip', 'Content-Length': r.headers['content-length'], ETag: r.headers.etag,
        'X-Saved': r.headers['x-amz-meta-saved'] ?? '', 'Cache-Control': 'no-store' });
      r.pipe(res);
      return;
    }
    if (method === 'DELETE') { await s3.delete(key); send(res, 204); return; }
    if (method !== 'PUT') { send(res, 405, { error: 'GET, PUT or DELETE' }); return; }
    if (Number(req.headers['content-length']) > MAX_SET_ZIP) { req.resume(); send(res, 413, { error: `a data set may be ${MAX_SET_ZIP >> 20} MB at most, zipped` }); return; }
    const free = hold(req, res, MAX_SET_ZIP);
    if (!free) return;
    try { await storeSet(req, res, user, name, key); } finally { free(); }
  }

  async function storeSet(req, res, user, name, key) {
    const body = await readBody(req, MAX_SET_ZIP);
    const { why, rawSize } = inspectSetZip(body);
    if (why) { send(res, 422, { error: `${name} was not stored: ${why}` }); return; }
    const current = await s3.head(key), base = req.headers['x-base-etag'];
    if (base !== undefined && (base === 'none' ? current !== null : current?.etag !== base)) { send(res, 409, { error: 'changed' }); return; }
    if (!current && (await s3.list(`sets/${user.sub}/`)).length >= maxSets) {
      send(res, 507, { error: `the cloud holds ${maxSets} data sets at most: remove one first` });
      return;
    }
    const savedAt = Number(req.headers['x-save-time']);
    const saved = Number.isFinite(savedAt) && savedAt > 0 ? Math.floor(savedAt) : Date.now();
    const etag = await s3.put(key, body, { type: 'application/zip', metadata: { 'raw-size': rawSize, saved } });
    send(res, 200, { name, etag, size: body.length, rawSize, saved });
  }

  async function deleteAccount(req, res, user) {
    // A subscription that runs on would be paid for an account that is gone: it is canceled first.
    const record = await account(user.sub, true);
    const p = record?.paddle;
    if (p && ['active', 'trialing', 'past_due'].includes(p.status) && !p.cancelAt) {
      send(res, 409, { error: 'Cancel your Cloud Saves subscription first (Manage subscription), then delete the account.' });
      return;
    }
    for (const o of await s3.list(`saves/${user.sub}/`)) await s3.delete(o.key);
    for (const o of await s3.list(`sets/${user.sub}/`)) await s3.delete(o.key);
    await s3.delete(accountKey(user.sub));
    accounts.delete(user.sub);
    setCookie(res, SESSION, null, { secure: secure(req) });
    log(`cloud: account ${user.sub} deleted`);
    try { await onAccountDeleted(user.sub); } catch (err) { log(`cloud: after the deletion of ${user.sub}: ${err.message}`); }
    send(res, 204);
  }

  async function handle(req, res, url) {
    const path = url.pathname, method = req.method;
    if (path === '/api/paddle/webhook' && method === 'POST') return paddleWebhook(req, res);    // signed by Paddle instead
    // A change comes only from this site's page: a header that a form or another site cannot send without asking
    // first, and the Origin of this site when the browser names one. The cookies are SameSite=Lax as well.
    if (method !== 'GET' && method !== 'HEAD') {
      const from = req.headers.origin;
      if (req.headers['x-cm'] !== '1' || (from && from !== origin(req))) { req.resume(); send(res, 403, { error: 'not from this site' }); return; }
    }
    if (path === '/api/me' && method === 'GET') return me(req, res);
    if (path === '/auth/google/start' && method === 'GET') return signInStart(req, res, url);
    if (path === '/auth/google/callback' && method === 'GET') return signInCallback(req, res, url);
    if (path === '/auth/logout' && method === 'POST') { setCookie(res, SESSION, null, { secure: secure(req) }); send(res, 204); return; }

    const user = sessionOf(req);
    if (path === '/api/billing/portal' && method === 'POST') {
      if (!user) { send(res, 401, { error: 'not signed in' }); return; }
      return billingPortal(res, user);
    }
    if (path === '/api/account' && method === 'DELETE') {
      if (!user) { send(res, 401, { error: 'not signed in' }); return; }
      return deleteAccount(req, res, user);
    }
    if (path === '/api/datasets' || path.startsWith('/api/datasets/')) {
      if (!user) { req.resume(); send(res, 401, { error: 'not signed in' }); return; }
      if (!(await hasCloud(user))) { req.resume(); send(res, 403, { error: 'cloud saves are not on this account\'s plan' }); return; }
      return dataSets(req, res, user, path, method);
    }
    if (path === '/api/saves' || path === '/api/saves/' || path.startsWith('/api/saves/')) {
      if (!user) { req.resume(); send(res, 401, { error: 'not signed in' }); return; }
      if (!(await hasCloud(user))) { req.resume(); send(res, 403, { error: 'cloud saves are not on this account\'s plan' }); return; }
      if (path === '/api/saves') {
        if (method !== 'GET') { send(res, 405, { error: 'GET only' }); return; }
        send(res, 200, { saves: await listSaves(user), maxSaves });
        return;
      }
      let name;
      try { name = decodeURIComponent(path.slice('/api/saves/'.length)); } catch { name = null; }
      if (!validSaveName(name)) { req.resume(); send(res, 400, { error: 'not a save name' }); return; }
      if (method === 'GET') return getSave(res, user, name);
      if (method === 'PUT') return putSave(req, res, user, name);
      if (method === 'DELETE') { await s3.delete(saveKey(user.sub, name)); send(res, 204); return; }
      send(res, 405, { error: 'GET, PUT or DELETE' });
      return;
    }
    send(res, 404, { error: 'not found' });
  }

  return {
    // The account signed in on this request ({ sub, email, name }), or null: statistics and the admin page.
    userOf: (req) => sessionOf(req),
    async handle(req, res, url) {
      try {
        await handle(req, res, url);
      } catch (err) {
        if (res.headersSent) { res.destroy(err); return; }
        if (err.status === 413) { send(res, 413, { error: url.pathname.startsWith('/api/datasets') ? `a data set may be ${MAX_SET_ZIP >> 20} MB at most, zipped` : `a save may be ${MAX_GZ >> 20} MB at most, compressed` }); return; }
        log(`cloud: ${req.method} ${url.pathname}: ${err.message}`);
        send(res, err instanceof S3Error || err.code === 'ECONNREFUSED' || /S3/.test(err.message) ? 502 : 500,
          { error: err instanceof S3Error ? 'the cloud store did not take the request: try again later' : 'the server failed: try again later' });
      }
    },
  };
}

// The cloud of the environment, or null and the settings that are missing.
// isAdmin(email): whether the account may open the admin page (server.mjs: CM_ADMIN_EMAILS).
export function cloudFromEnv(env = process.env, log, { onAccountDeleted, isAdmin } = {}) {
  const missing = [];
  for (const k of ['GOOGLE_CLIENT_ID', 'GOOGLE_CLIENT_SECRET', 'CM_SESSION_SECRET', 'AWS_ENDPOINT_URL_S3', 'AWS_ACCESS_KEY_ID',
    'AWS_SECRET_ACCESS_KEY', 'BUCKET_NAME']) if (!env[k]) missing.push(k);
  if (env.CM_SESSION_SECRET && env.CM_SESSION_SECRET.length < 32) missing.push('CM_SESSION_SECRET (32 characters or more)');
  if (missing.length) return { cloud: null, missing };
  const cloud = createCloud({
    s3: s3FromEnv(env), clientId: env.GOOGLE_CLIENT_ID, clientSecret: env.GOOGLE_CLIENT_SECRET, sessionSecret: env.CM_SESSION_SECRET,
    publicUrl: env.CM_PUBLIC_URL ? env.CM_PUBLIC_URL.replace(/\/+$/, '') : null,
    cloudFor: env.CM_CLOUD_FOR === 'everyone' ? 'everyone' : 'listed', cloudUsers: (env.CM_CLOUD_USERS ?? '').split(','),
    upgradeUrl: env.CM_UPGRADE_URL || null, maxSaves: Number(env.CM_CLOUD_MAX_SAVES) || 30, maxSets: Number(env.CM_CLOUD_MAX_SETS) || 5, log,
    price: env.CM_PRICE || '$5 a month', paddle: paddleFromEnv(env), onAccountDeleted, isAdmin,
    uploadBudget: (Number(env.CM_UPLOAD_BUDGET_MB) || 160) << 20,
  });
  return { cloud, missing };
}

// Paddle's settings, or null (no checkout) when one of the three it needs is missing.
export function paddleFromEnv(env = process.env) {
  if (!env.PADDLE_CLIENT_TOKEN || !env.PADDLE_PRICE_ID || !env.PADDLE_WEBHOOK_SECRET) return null;
  const sandbox = env.PADDLE_ENV === 'sandbox';
  return { env: sandbox ? 'sandbox' : 'production', clientToken: env.PADDLE_CLIENT_TOKEN,
    priceIds: env.PADDLE_PRICE_ID.split(',').map((x) => x.trim()).filter(Boolean), webhookSecret: env.PADDLE_WEBHOOK_SECRET,
    apiKey: env.PADDLE_API_KEY || null, apiBase: PADDLE_API[sandbox ? 'sandbox' : 'production'] };
}
