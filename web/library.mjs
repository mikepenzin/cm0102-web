// The database library (web/README.md, "Database library"): data sets that the admin offers to every player, free
// and with no account, each for a season of the game. A player gets one into the Databases of the user folder from
// Saved games, Data sets, Get a database (setspanel.js), and Use starts the next new game with it in its season.
//
// The store holds:
//   library/index.json             { entries: [{ id, name, season, note, early, size, rawSize, added }] }; early: the day
//                                  (YYYY-MM-DD) before which only Club Legends get it (early.mjs), or null
//   library/<id>.zip               the data set: its 20 database files (and the Data files it brings), as a zip
//   library/<id>.png               its title card (rgn.mjs): the release's intro picture, small, for the page only
//   library/incoming/<id>.zip      an upload of the admin page, until it is checked
// The admin page uploads a zip (straight to the store with a signed link where the pages go direct, else through this
// server), then names it: the server checks that it holds the 20 database files (cloud.mjs inspectSetZip) and only then
// lists it. A player's download goes straight from the store where it can, else through this server.
import { randomBytes } from 'node:crypto';
import { inspectSetZip, validSetName } from './cloud.mjs';
import { cardOfZip } from './rgn.mjs';
import { isEarly, validDay } from './early.mjs';

export const MAX_LIBRARY_ZIP = 128 << 20;
export const CARDS_AT_ONCE = 6;                     // zips read by one call of the admin's Make their pictures            // a data set with its extra files, zipped (the 2001/02 data: some 20 MB)
// The season a database is for: the game's seasons (datasets.js, seasons_db.h, 2001/02 to 2025/26), and one more for a
// database of the season after the game's last (an update of October 2026 is 2026/27): Use starts that one in the game's
// last season, its players keeping their ages (setspanel.js).
export const FIRST_SEASON = 2001, LAST_SEASON = 2026;
const INDEX = 'library/index.json';
const zipKey = (id) => `library/${id}.zip`, incomingKey = (id) => `library/incoming/${id}.zip`, cardKey = (id) => `library/${id}.png`;
const validId = (id) => typeof id === 'string' && /^[0-9a-f]{16}$/.test(id);

function send(res, status, body) {
  res.writeHead(status, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
  res.end(body === undefined ? '' : JSON.stringify(body));
}
async function readBody(req, limit) {
  const chunks = [];
  let n = 0;
  for await (const c of req) {
    n += c.length;
    if (n > limit) throw Object.assign(new Error('too large'), { status: 413 });
    chunks.push(c);
  }
  return Buffer.concat(chunks);
}
const readJson = async (req) => { try { return JSON.parse((await readBody(req, 16 << 10)).toString() || '{}'); } catch (e) { if (e.status) throw e; return null; } };

// What the admin gives an entry: { name, season, note }, cleaned, or why not.
export function entryFields(body) {
  const name = typeof body?.name === 'string' ? body.name.trim() : '';
  if (!validSetName(name)) return { why: 'a name of 1 to 60 letters, without \\ / : * ? " < > | and not starting with a dot' };
  const season = Number(body?.season);
  if (!Number.isInteger(season) || season < FIRST_SEASON || season > LAST_SEASON) return { why: `a season from ${FIRST_SEASON} to ${LAST_SEASON} (its first year)` };
  const note = String(body?.note ?? '').replace(/[\u0000-\u0008\u000b-\u001f\u007f]/g, '').trim().slice(0, 500);
  const early = body?.early == null || body.early === '' ? null : body.early;
  if (early !== null && !validDay(early)) return { why: 'early: a day YYYY-MM-DD, or none' };
  return { why: null, fields: { name, season, note, early } };
}

// presign(method, key, opts): a link of the store at the address the pages reach (cloud.mjs presign); isDirect():
// whether the pages go straight to the store now.
// levelOf(req): the level of the player who asks (levels.mjs): a database still early is a Club Legend's only.
export function createLibrary({ s3, presign = (m, k, o) => s3.presign?.(m, k, o), isDirect = () => false, log = () => {}, levelOf = async () => 'free' }) {
  let cache = null, cachedAt = 0;
  async function index(fresh = false) {
    if (!fresh && cache && Date.now() - cachedAt < 30000) return cache;
    const buf = await s3.getBuffer(INDEX);
    cache = buf ? JSON.parse(buf.toString()) : { entries: [] };
    cachedAt = Date.now();
    return cache;
  }
  async function writeIndex(ix) {
    await s3.put(INDEX, Buffer.from(JSON.stringify(ix)), { type: 'application/json' });
    cache = ix;
    cachedAt = Date.now();
  }
  const listed = (ix) => [...ix.entries].sort((a, b) => b.season - a.season || a.name.localeCompare(b.name));
  // The title card of a database's zip into the store (rgn.mjs); true when it has one.
  async function makeCard(id, zip) {
    const png = await cardOfZip(zip);
    if (png) await s3.put(cardKey(id), Buffer.from(png), { type: 'image/png' });
    return Boolean(png);
  }

  // For everyone: the list, and a database (a signed link of the store, or the zip through this server).
  async function handle(req, res, url) {
    const path = url.pathname;
    if (req.method !== 'GET' && req.method !== 'HEAD') { req.resume(); send(res, 405, { error: 'GET only' }); return; }
    // A database still early: its day for everyone, and locked for a player below Club Legend.
    const legend = async () => (await levelOf(req).catch(() => 'free')) === 'legend';
    const shown = (e, isLegend) => {
      const { early, ...rest } = e;
      return isEarly(early) ? { ...rest, early, ...(isLegend ? {} : { locked: true }) } : rest;
    };
    if (path === '/api/library') {
      const entries = listed(await index()), isLegend = entries.some((e) => isEarly(e.early)) && await legend();
      send(res, 200, { entries: entries.map((e) => shown(e, isLegend)) });
      return;
    }
    const [id, part] = path.slice('/api/library/'.length).split('/');
    const entry = validId(id) ? (await index()).entries.find((e) => e.id === id) : null;
    if (!entry) { send(res, 404, { error: 'no such database in the library' }); return; }
    if (part === 'card') {
      const r = entry.card ? await s3.get(cardKey(id)) : null;
      if (!r) { send(res, 404, { error: 'this database has no picture' }); return; }
      // The picture of an id never changes (a new upload is a new id): a week in the browser's cache.
      res.writeHead(200, { 'Content-Type': 'image/png', 'Content-Length': r.headers['content-length'], 'Cache-Control': 'public, max-age=604800' });
      r.pipe(res);
      return;
    }
    if (part !== undefined) { send(res, 404, { error: 'not found' }); return; }
    if (isEarly(entry.early) && !await legend()) {
      send(res, 403, { error: `Club Legends get this database until ${entry.early}`, key: 'Club Legends get this database until {date}', vars: { date: entry.early } });
      return;
    }
    if (url.searchParams.get('direct') === 'get') {
      send(res, 200, isDirect() ? { direct: true, url: presign('GET', zipKey(id)).url, size: entry.size } : { direct: false });
      return;
    }
    const r = await s3.get(zipKey(id));
    if (!r) { send(res, 404, { error: 'the store has lost this database' }); return; }
    res.writeHead(200, { 'Content-Type': 'application/zip', 'Content-Length': r.headers['content-length'], 'Cache-Control': 'no-store' });
    r.pipe(res);
  }

  // For the admin (server.mjs checks who asks): POST /api/admin/library { size } starts an upload; PUT
  // /api/admin/library/<id>/body sends it through this server; POST /api/admin/library/<id>/done { name, season, note }
  // checks and lists it; PATCH /api/admin/library/<id> { name, season, note } changes them; DELETE takes it away.
  async function admin(req, res, url) {
    const parts = url.pathname.slice('/api/admin/library'.length).split('/').filter(Boolean);
    const id = parts[0], step = parts[1], method = req.method;
    if (!id) {
      if (method === 'GET') {
        // Uploads that were never named (a page closed in between) go after a day.
        for (const o of await s3.list('library/incoming/').catch(() => [])) if (Date.parse(o.modified) < Date.now() - 86400e3) await s3.delete(o.key).catch(() => {});
        send(res, 200, { entries: listed(await index(true)), max: MAX_LIBRARY_ZIP });
        return;
      }
      if (method !== 'POST') { req.resume(); send(res, 405, { error: 'GET or POST' }); return; }
      const body = await readJson(req);
      const size = Math.floor(Number(body?.size));
      if (!(size > 0) || size > MAX_LIBRARY_ZIP) { send(res, 413, { error: `a database may be ${MAX_LIBRARY_ZIP >> 20} MB at most, zipped` }); return; }
      const fresh = randomBytes(8).toString('hex');
      if (isDirect()) {
        const link = presign('PUT', incomingKey(fresh), { headers: { 'content-type': 'application/zip', 'content-length': size } });
        const { 'content-length': _, ...headers } = link.headers;
        send(res, 200, { id: fresh, direct: true, url: link.url, headers: Object.fromEntries(Object.entries(headers).map(([k, v]) => [k, String(v)])) });
      } else send(res, 200, { id: fresh, direct: false });
      return;
    }
    // POST /api/admin/library/cards: the title card of each database that has none yet (those added before the
    // pictures came), one zip at a time, CARDS_AT_ONCE a call so that no request runs for minutes. { made, none, left }:
    // the names of those that got one and of those with no picture, and how many are still to read (call again).
    if (id === 'cards' && !step && method === 'POST') {
      req.resume();
      const ix = await index(true), made = [], none = [];
      for (const e of ix.entries) {
        if (e.card !== undefined) continue;
        if (made.length + none.length >= CARDS_AT_ONCE) break;
        const zip = await s3.getBuffer(zipKey(e.id));
        e.card = zip ? await makeCard(e.id, zip) : false;
        (e.card ? made : none).push(e.name);
      }
      await writeIndex(ix);
      if (made.length || none.length) log(`library: pictures made for ${made.length}, ${none.length} without one`);
      send(res, 200, { made, none, left: ix.entries.filter((e) => e.card === undefined).length });
      return;
    }
    if (!validId(id)) { req.resume(); send(res, 400, { error: 'not a library id' }); return; }
    if (step === 'body' && method === 'PUT') {
      if (Number(req.headers['content-length']) > MAX_LIBRARY_ZIP) { req.resume(); send(res, 413, { error: `a database may be ${MAX_LIBRARY_ZIP >> 20} MB at most, zipped` }); return; }
      await s3.put(incomingKey(id), await readBody(req, MAX_LIBRARY_ZIP), { type: 'application/zip' });
      send(res, 204);
      return;
    }
    if (step === 'done' && method === 'POST') {
      const { why: bad, fields } = entryFields(await readJson(req));
      if (bad) { send(res, 400, { error: bad }); return; }
      const h = await s3.head(incomingKey(id));
      if (!h) { send(res, 404, { error: 'no upload of that id: send the zip first' }); return; }
      const zip = h.size > MAX_LIBRARY_ZIP ? null : await s3.getBuffer(incomingKey(id));
      const { why, rawSize } = zip ? inspectSetZip(zip) : { why: `it is larger than ${MAX_LIBRARY_ZIP >> 20} MB` };
      if (why) { await s3.delete(incomingKey(id)); send(res, 422, { error: `not a data set: ${why}` }); return; }
      await s3.copy(incomingKey(id), zipKey(id), { metadata: { 'raw-size': rawSize }, type: 'application/zip' });
      await s3.delete(incomingKey(id));
      const card = await makeCard(id, zip).catch(() => false);
      const ix = await index(true);
      const entry = { id, ...fields, size: h.size, rawSize, card, added: new Date().toISOString() };
      await writeIndex({ ...ix, entries: [...ix.entries.filter((e) => e.id !== id), entry] });
      log(`library: ${fields.name} (${fields.season}) added, ${Math.round(h.size / 1048576)} MB`);
      send(res, 201, entry);
      return;
    }
    if (!step && method === 'PATCH') {
      const ix = await index(true), old = ix.entries.find((e) => e.id === id);
      if (!old) { req.resume(); send(res, 404, { error: 'no such database in the library' }); return; }
      const { why, fields } = entryFields({ ...old, ...(await readJson(req)) });
      if (why) { send(res, 400, { error: why }); return; }
      const entry = { ...old, ...fields };
      await writeIndex({ ...ix, entries: ix.entries.map((e) => (e.id === id ? entry : e)) });
      send(res, 200, entry);
      return;
    }
    if (!step && method === 'DELETE') {
      const ix = await index(true), old = ix.entries.find((e) => e.id === id);
      await writeIndex({ ...ix, entries: ix.entries.filter((e) => e.id !== id) });
      await s3.delete(zipKey(id));
      if (old?.card) await s3.delete(cardKey(id)).catch(() => {});
      if (old) log(`library: ${old.name} (${old.season}) removed`);
      send(res, 204);
      return;
    }
    req.resume();
    send(res, 404, { error: 'not found' });
  }

  return { handle, admin, index };
}
