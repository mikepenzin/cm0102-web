// Tests of the database library (library.mjs and its routes in server.mjs) against the fake S3 store: the admin's
// upload (through the server, and straight to the store), the check of the zip, the list, a player's download, an edit
// and a removal. Run: cd web && npm test
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { CARDS_AT_ONCE, createLibrary, entryFields } from '../library.mjs';
import { createS3 } from '../s3.mjs';
import { addDays, todayUTC } from '../early.mjs';
import { createStats } from '../stats.mjs';
import { createApp } from '../server.mjs';
import { writeZip } from '../zip.js';
import { fakeDataSet, fakeRgn as rgn, fakeS3 } from './fakes.mjs';

const TOKEN = 't'.repeat(24);
let tmp, s3fake, s3, server, base, direct = false, level = 'free';
const logs = [];

before(async () => {
  tmp = mkdtempSync(join(tmpdir(), 'cm0102-library-test-'));
  mkdirSync(join(tmp, 'game'));
  writeFileSync(join(tmp, 'manifest.tsv'), '');
  s3fake = await fakeS3();
  s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const library = createLibrary({ s3, presign: (m, k, o) => s3.presign(m, k, o), isDirect: () => direct, log: (t) => logs.push(t), levelOf: async () => level });
  server = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv'),
    stats: createStats({ store: null }), adminToken: TOKEN, library });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  base = `http://127.0.0.1:${server.address().port}`;
});
after(() => { server.close(); s3fake.close(); rmSync(tmp, { recursive: true, force: true }); });

const admin = (path, { method = 'GET', body, headers = {} } = {}) => fetch(`${base}${path}`, { method, body: body === undefined ? undefined : typeof body === 'string' || body instanceof Uint8Array ? body : JSON.stringify(body),
  headers: { authorization: `Bearer ${TOKEN}`, ...(body && !(body instanceof Uint8Array) ? { 'content-type': 'application/json' } : {}), ...headers } });
const dataSetZip = async (folder = '2011 update', extra = []) => writeZip([...Object.entries(fakeDataSet()).map(([name, data]) => ({ name: `${folder}/${name}`, data })), ...extra]);
const list = async () => (await (await fetch(`${base}/api/library`)).json()).entries;

test('the fields of an entry: a folder name, a season of the game, a short note', () => {
  assert.deepEqual(entryFields({ name: ' 2011 update ', season: '2011', note: 'From the forum\u0007, by A.' }).fields, { name: '2011 update', season: 2011, note: 'From the forum, by A.', early: null });
  assert.match(entryFields({ name: 'a/b', season: 2011 }).why, /a name of 1 to 60/);
  assert.match(entryFields({ name: 'ok', season: 1999 }).why, /a season from 2001 to 2026/);
  assert.equal(entryFields({ name: 'ok', season: 2026 }).fields.season, 2026, '2026/27: a database after the game\'s last season');
  assert.match(entryFields({ name: 'ok', season: 2027 }).why, /a season/);
  assert.match(entryFields({ name: 'ok', season: 2011.5 }).why, /a season/);
  assert.equal(entryFields({ name: 'ok', season: 2025, note: 'x'.repeat(900) }).fields.note.length, 500);
  assert.equal(entryFields({ name: 'ok', season: 2025 }).fields.early, null, 'no day: open to all');
  assert.equal(entryFields({ name: 'ok', season: 2025, early: '' }).fields.early, null);
  assert.equal(entryFields({ name: 'ok', season: 2025, early: '2027-01-01' }).fields.early, '2027-01-01');
  assert.match(entryFields({ name: 'ok', season: 2025, early: 'soon' }).why, /early: a day YYYY-MM-DD, or none/);
});

test('an empty library; only an admin adds to it, and the page knows there is one', async () => {
  assert.deepEqual(await list(), []);
  assert.equal((await fetch(`${base}/api/admin/library`, { method: 'POST', body: '{"size":10}' })).status, 401);
  assert.equal((await (await fetch(`${base}/files.json`)).json()).library, true);
  assert.equal((await fetch(`${base}/api/library`, { method: 'POST' })).status, 405);
});

test('the admin uploads through the server, names it; everyone lists it and gets the same bytes', async () => {
  direct = false;
  const zip = await dataSetZip();
  const start = await (await admin('/api/admin/library', { method: 'POST', body: { size: zip.length } })).json();
  assert.equal(start.direct, false);
  assert.match(start.id, /^[0-9a-f]{16}$/);
  assert.equal((await admin(`/api/admin/library/${start.id}/body`, { method: 'PUT', body: zip, headers: { 'content-type': 'application/zip' } })).status, 204);
  const done = await admin(`/api/admin/library/${start.id}/done`, { method: 'POST', body: { name: '2011 update', season: 2011, note: 'Squads of December 2011' } });
  assert.equal(done.status, 201);
  const entries = await list();
  assert.deepEqual(entries.map((e) => [e.id, e.name, e.season, e.note, e.size]), [[start.id, '2011 update', 2011, 'Squads of December 2011', zip.length]]);
  assert.ok(entries[0].rawSize > 0 && entries[0].added);
  assert.ok(!s3fake.objects.has(`library/incoming/${start.id}.zip`), 'the upload left its incoming place');
  assert.deepEqual(await (await fetch(`${base}/api/library/${start.id}?direct=get`)).json(), { direct: false });
  const got = new Uint8Array(await (await fetch(`${base}/api/library/${start.id}`)).arrayBuffer());
  assert.ok(Buffer.from(got).equals(Buffer.from(zip)), 'the same zip');
  assert.match(logs.join(' '), /library: 2011 update \(2011\) added/);
});

test('straight to the store: a signed upload link for the admin, a signed download link for everyone', async () => {
  direct = true;
  const zip = await dataSetZip('2025 update');
  const start = await (await admin('/api/admin/library', { method: 'POST', body: { size: zip.length } })).json();
  assert.equal(start.direct, true);
  assert.equal((await fetch(start.url, { method: 'PUT', body: zip, headers: start.headers })).status, 200);
  assert.equal((await admin(`/api/admin/library/${start.id}/done`, { method: 'POST', body: { name: '2025 update', season: 2025 } })).status, 201);
  const entries = await list();
  assert.deepEqual(entries.map((e) => e.season), [2025, 2011], 'the newest season first');
  const link = await (await fetch(`${base}/api/library/${start.id}?direct=get`)).json();
  assert.equal(link.direct, true);
  assert.ok(Buffer.from(await (await fetch(link.url)).arrayBuffer()).equals(Buffer.from(zip)));
  direct = false;
});

test('a zip that is not a data set is refused and not listed; sizes and fields are checked', async () => {
  const junk = await writeZip([{ name: 'readme.txt', data: new TextEncoder().encode('hello') }]);
  const start = await (await admin('/api/admin/library', { method: 'POST', body: { size: junk.length } })).json();
  await admin(`/api/admin/library/${start.id}/body`, { method: 'PUT', body: junk });
  const r = await admin(`/api/admin/library/${start.id}/done`, { method: 'POST', body: { name: 'Junk', season: 2011 } });
  assert.equal(r.status, 422);
  assert.match((await r.json()).error, /not a data set: it does not hold the 20 database files/);
  assert.ok(!(await list()).some((e) => e.name === 'Junk'));
  assert.ok(!s3fake.objects.has(`library/incoming/${start.id}.zip`));
  assert.equal((await admin('/api/admin/library', { method: 'POST', body: { size: 200 << 20 } })).status, 413);
  assert.equal((await admin('/api/admin/library/0123456789abcdef/done', { method: 'POST', body: { name: 'x', season: 2011 } })).status, 404, 'nothing uploaded');
  assert.equal((await admin('/api/admin/library/0123456789abcdef/done', { method: 'POST', body: { name: 'x', season: 1990 } })).status, 400);
  assert.equal((await fetch(`${base}/api/library/nothere`)).status, 404);
});

test('an upload never named goes after a day, when the admin page lists the library', async () => {
  const old = 'library/incoming/00000000000000aa.zip';
  await s3.put(old, Buffer.from('x'), { type: 'application/zip' });
  s3fake.objects.get(old).modified = new Date(Date.now() - 2 * 86400e3).toISOString();
  await s3.put('library/incoming/00000000000000bb.zip', Buffer.from('y'), { type: 'application/zip' });
  assert.equal((await admin('/api/admin/library')).status, 200);
  assert.ok(!s3fake.objects.has(old) && s3fake.objects.has('library/incoming/00000000000000bb.zip'));
  await s3.delete('library/incoming/00000000000000bb.zip');
});

test('the admin changes an entry and takes one away; the page cannot', async () => {
  const [newest, older] = await list();
  const changed = await admin(`/api/admin/library/${older.id}`, { method: 'PATCH', body: { note: 'Squads of 1 December 2011', season: 2010 } });
  assert.equal(changed.status, 200);
  assert.deepEqual((await list()).find((e) => e.id === older.id).note, 'Squads of 1 December 2011');
  // From a page of another site, with a session but no token: refused (the token is the script's way in).
  assert.equal((await fetch(`${base}/api/admin/library/${newest.id}`, { method: 'DELETE' })).status, 401);
  assert.equal((await admin(`/api/admin/library/${newest.id}`, { method: 'DELETE' })).status, 204);
  assert.deepEqual((await list()).map((e) => e.id), [older.id]);
  assert.ok(!s3fake.objects.has(`library/${newest.id}.zip`), 'its zip is gone');
  assert.match(logs.join(' '), /library: 2025 update \(2025\) removed/);
});

test('the title card: made when a database comes, given to everyone, made later for older ones, gone with its database', async () => {
  direct = false;
  const logo = rgn(64, 48, (x) => (x < 32 ? 0xf800 : 0x001f));
  const upload = async (zip, name) => {
    const start = await (await admin('/api/admin/library', { method: 'POST', body: { size: zip.length } })).json();
    await admin(`/api/admin/library/${start.id}/body`, { method: 'PUT', body: zip });
    return (await (await admin(`/api/admin/library/${start.id}/done`, { method: 'POST', body: { name, season: 2014 } })).json());
  };
  const withCard = await upload(await dataSetZip('October 2014', [{ name: 'October 2014/logo.RGN', data: logo }]), 'October 2014');
  const without = await upload(await dataSetZip('Plain 2014'), 'Plain 2014');
  assert.deepEqual([withCard.card, without.card], [true, false]);
  const got = await fetch(`${base}/api/library/${withCard.id}/card`);
  assert.equal(got.status, 200);
  assert.equal(got.headers.get('content-type'), 'image/png');
  assert.match(got.headers.get('cache-control'), /public, max-age=\d+/);
  assert.deepEqual([...new Uint8Array(await got.arrayBuffer()).subarray(1, 4)], [80, 78, 71]);
  assert.equal((await fetch(`${base}/api/library/${without.id}/card`)).status, 404);
  assert.equal((await fetch(`${base}/api/library/${withCard.id}/nothing`)).status, 404);
  assert.equal((await list()).find((e) => e.id === withCard.id).card, true, 'the list says which have one');
  // A database added before the cards: no card field. The admin's Make their pictures reads its zip once.
  const ix = JSON.parse(Buffer.from(s3fake.objects.get('library/index.json').body).toString());
  delete ix.entries.find((e) => e.id === withCard.id).card;
  s3fake.objects.delete(`library/${withCard.id}.png`);
  await s3.put('library/index.json', Buffer.from(JSON.stringify(ix)), { type: 'application/json' });
  assert.equal((await fetch(`${base}/api/admin/library/cards`, { method: 'POST' })).status, 401, 'only the admin');
  const made = await (await admin('/api/admin/library/cards', { method: 'POST' })).json();
  assert.deepEqual(made, { made: ['October 2014'], none: [], left: 0 }, 'only those without one are read');
  assert.ok(s3fake.objects.has(`library/${withCard.id}.png`));
  assert.deepEqual(await (await admin('/api/admin/library/cards', { method: 'POST' })).json(), { made: [], none: [], left: 0 }, 'a second time: nothing to do');
  // Many of them: CARDS_AT_ONCE a call, and left says how many remain.
  const many = JSON.parse(Buffer.from(s3fake.objects.get('library/index.json').body).toString());
  for (let i = 0; i < CARDS_AT_ONCE + 2; i++) many.entries.push({ ...many.entries.find((e) => e.id === without.id), id: `00000000000000c${i}`, name: `Old ${i}` });
  for (const e of many.entries) if (e.id.startsWith('00000000000000c')) delete e.card;
  await s3.put('library/index.json', Buffer.from(JSON.stringify(many)), { type: 'application/json' });
  const first = await (await admin('/api/admin/library/cards', { method: 'POST' })).json();
  assert.deepEqual([first.made.length + first.none.length, first.left], [CARDS_AT_ONCE, 2]);
  const second = await (await admin('/api/admin/library/cards', { method: 'POST' })).json();
  assert.deepEqual([second.none.length, second.left], [2, 0], 'a database whose zip is missing: no card');
  for (let i = 0; i < CARDS_AT_ONCE + 2; i++) await admin(`/api/admin/library/00000000000000c${i}`, { method: 'DELETE' });
  assert.match(logs.join(' '), /library: pictures made for 1, 0 without one/);
  assert.equal((await admin(`/api/admin/library/${withCard.id}`, { method: 'DELETE' })).status, 204);
  assert.ok(!s3fake.objects.has(`library/${withCard.id}.png`), 'the card goes with its database');
  await admin(`/api/admin/library/${without.id}`, { method: 'DELETE' });
});

async function addDb(name, early) {
  const zip = await dataSetZip(name);
  const start = await (await admin('/api/admin/library', { method: 'POST', body: { size: zip.length } })).json();
  await admin(`/api/admin/library/${start.id}/body`, { method: 'PUT', body: zip, headers: { 'content-type': 'application/zip' } });
  const done = await admin(`/api/admin/library/${start.id}/done`, { method: 'POST', body: { name, season: 2026, note: '', early } });
  assert.equal(done.status, 201);
  return start.id;
}

test('an early database: listed, locked for a player below Club Legend, open for a Club Legend', async () => {
  direct = false;
  const day = addDays(todayUTC(), 3);
  const id = await addDb('Season 2026-27', day);
  level = 'squad';
  let e = (await list()).find((x) => x.id === id);
  assert.deepEqual([e.early, e.locked], [day, true]);
  const zip = await fetch(`${base}/api/library/${id}`);
  assert.equal(zip.status, 403);
  assert.deepEqual(await zip.json(), { error: `Club Legends get this database until ${day}`, key: 'Club Legends get this database until {date}', vars: { date: day } });
  assert.equal((await fetch(`${base}/api/library/${id}?direct=get`)).status, 403);
  level = 'legend';
  e = (await list()).find((x) => x.id === id);
  assert.deepEqual([e.early, e.locked], [day, undefined]);
  assert.equal((await fetch(`${base}/api/library/${id}`)).status, 200);
  assert.equal((await (await admin('/api/admin/library')).json()).entries.find((x) => x.id === id).early, day, 'the admin sees the day');
  level = 'free';
});

test('its day opens it to all; the admin sets and clears the day', async () => {
  const id = await addDb('Opens today', todayUTC());
  let e = (await list()).find((x) => x.id === id);
  assert.deepEqual([e.early, e.locked], [undefined, undefined]);
  assert.equal((await fetch(`${base}/api/library/${id}`)).status, 200, 'a free player, on its day');
  assert.equal((await admin(`/api/admin/library/${id}`, { method: 'PATCH', body: { early: '2099-01-01' } })).status, 200);
  e = (await list()).find((x) => x.id === id);
  assert.deepEqual([e.early, e.locked], ['2099-01-01', true]);
  assert.equal((await admin(`/api/admin/library/${id}`, { method: 'PATCH', body: { early: 'soon' } })).status, 400);
  assert.equal((await admin(`/api/admin/library/${id}`, { method: 'PATCH', body: { early: null } })).status, 200);
  e = (await list()).find((x) => x.id === id);
  assert.deepEqual([e.early, e.locked], [undefined, undefined]);
  // The picture of a locked database stays public: the page shows it on the locked row (no card here: 404, not 403).
  await admin(`/api/admin/library/${id}`, { method: 'PATCH', body: { early: '2099-01-01' } });
  assert.notEqual((await fetch(`${base}/api/library/${id}/card`)).status, 403);
});
