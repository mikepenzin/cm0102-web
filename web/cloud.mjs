// Accounts and cloud saves (web/README.md, "Cloud saves"): sign in with Google, and the saves of an account in an S3
// store (Tigris on Fly.io). Without its settings the server runs as before: /api/me answers { auth: false } and the
// page shows no account.
//
// Who has cloud saves: CM_CLOUD_FOR=everyone gives them to every signed-in account; else the emails in CM_CLOUD_USERS
// (comma-separated) and the accounts whose record says plan "cloud" (accounts/<id>.json in the store, for a payment
// to set) have them. Everyone else keeps saves in the browser only, as before.
//
// The store holds:
//   accounts/<google id>.json            { sub, email, name, created, seen, plan, planUntil }
//   saves/<google id>/<name, base64url>.gz the save, gzip-compressed by the page; metadata raw-size and saved (ms)
import { createGunzip } from 'node:zlib';
import { GOOGLE, SignInError, googleFinish, googleStart, readCookies, sealer, setCookie } from './auth.mjs';
import { S3Error, s3FromEnv } from './s3.mjs';
import { SAVE_HEADER, checkSave } from './saves.js';

const SESSION = 'cm_session', OAUTH = 'cm_oauth';
const SESSION_DAYS = 30;
export const MAX_GZ = 64 << 20;            // what one upload may be, compressed
export const MAX_RAW = 256 << 20;          // and uncompressed (a save of the full database is some tens of MB)

const b64u = (s) => Buffer.from(s, 'utf8').toString('base64url');
const unb64u = (s) => Buffer.from(s, 'base64url').toString('utf8');

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

export function createCloud({ s3, clientId, clientSecret, sessionSecret, publicUrl = null, cloudFor = 'listed', cloudUsers = [],
  upgradeUrl = null, maxSaves = 30, google = GOOGLE, log = (t) => console.log(t) }) {
  const seal = sealer(sessionSecret);
  const users = new Set(cloudUsers.map((e) => e.trim().toLowerCase()).filter(Boolean));
  const accounts = new Map();               // google id -> { account, at }: the record, read at most once a minute

  const origin = (req) => publicUrl ?? `${String(req.headers['x-forwarded-proto'] ?? 'http').split(',')[0].trim()}://${req.headers.host}`;
  const secure = (req) => origin(req).startsWith('https:');
  const sessionOf = (req) => seal.open('s', readCookies(req)[SESSION]);
  const accountKey = (sub) => `accounts/${sub}.json`;
  const saveKey = (sub, name) => `saves/${sub}/${b64u(name)}.gz`;

  async function account(sub, fresh = false) {
    const c = accounts.get(sub);
    if (!fresh && c && Date.now() - c.at < 60000) return c.account;
    const buf = await s3.getBuffer(accountKey(sub));
    const a = buf ? JSON.parse(buf.toString()) : null;
    accounts.set(sub, { account: a, at: Date.now() });
    return a;
  }

  async function hasCloud(user) {
    if (cloudFor === 'everyone' || users.has(user.email)) return true;
    let a = null;
    try { a = await account(user.sub); } catch (err) { log(`cloud: account ${user.sub} not read: ${err.message}`); }
    return a?.plan === 'cloud' && (!a.planUntil || Date.parse(a.planUntil) > Date.now());
  }

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
      const record = { sub: user.sub, email: user.email, name: user.name, created: had?.created ?? now, seen: now,
        plan: had?.plan ?? 'free', planUntil: had?.planUntil ?? null };
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
    if (!user) { send(res, 200, { auth: true, user: null }); return; }
    const cloud = await hasCloud(user);
    setCookie(res, SESSION, seal.seal('s', user, SESSION_DAYS * 86400), { maxAge: SESSION_DAYS * 86400, secure: secure(req) });
    send(res, 200, { auth: true, user: { email: user.email, name: user.name, picture: user.picture, cloud },
      upgradeUrl: cloud ? null : upgradeUrl, maxSaves });
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

  async function deleteAccount(req, res, user) {
    for (const o of await s3.list(`saves/${user.sub}/`)) await s3.delete(o.key);
    await s3.delete(accountKey(user.sub));
    accounts.delete(user.sub);
    setCookie(res, SESSION, null, { secure: secure(req) });
    log(`cloud: account ${user.sub} deleted`);
    send(res, 204);
  }

  async function handle(req, res, url) {
    const path = url.pathname, method = req.method;
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
    if (path === '/api/account' && method === 'DELETE') {
      if (!user) { send(res, 401, { error: 'not signed in' }); return; }
      return deleteAccount(req, res, user);
    }
    if (path === '/api/saves' || path.startsWith('/api/saves/')) {
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
    async handle(req, res, url) {
      try {
        await handle(req, res, url);
      } catch (err) {
        if (res.headersSent) { res.destroy(err); return; }
        if (err.status === 413) { send(res, 413, { error: `a save may be ${MAX_GZ >> 20} MB at most, compressed` }); return; }
        log(`cloud: ${req.method} ${url.pathname}: ${err.message}`);
        send(res, err instanceof S3Error || err.code === 'ECONNREFUSED' || /S3/.test(err.message) ? 502 : 500,
          { error: err instanceof S3Error ? 'the cloud store did not take the request: try again later' : 'the server failed: try again later' });
      }
    },
  };
}

// The cloud of the environment, or null and the settings that are missing.
export function cloudFromEnv(env = process.env, log) {
  const missing = [];
  for (const k of ['GOOGLE_CLIENT_ID', 'GOOGLE_CLIENT_SECRET', 'CM_SESSION_SECRET', 'AWS_ENDPOINT_URL_S3', 'AWS_ACCESS_KEY_ID',
    'AWS_SECRET_ACCESS_KEY', 'BUCKET_NAME']) if (!env[k]) missing.push(k);
  if (env.CM_SESSION_SECRET && env.CM_SESSION_SECRET.length < 32) missing.push('CM_SESSION_SECRET (32 characters or more)');
  if (missing.length) return { cloud: null, missing };
  const cloud = createCloud({
    s3: s3FromEnv(env), clientId: env.GOOGLE_CLIENT_ID, clientSecret: env.GOOGLE_CLIENT_SECRET, sessionSecret: env.CM_SESSION_SECRET,
    publicUrl: env.CM_PUBLIC_URL ? env.CM_PUBLIC_URL.replace(/\/+$/, '') : null,
    cloudFor: env.CM_CLOUD_FOR === 'everyone' ? 'everyone' : 'listed', cloudUsers: (env.CM_CLOUD_USERS ?? '').split(','),
    upgradeUrl: env.CM_UPGRADE_URL || null, maxSaves: Number(env.CM_CLOUD_MAX_SAVES) || 30, log,
  });
  return { cloud, missing };
}
