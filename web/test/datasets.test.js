// Tests of data sets as files (zip.js, datasets.js) and in the cloud (cloud.mjs /api/datasets).
// Run: cd web && npm test
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { crc32, writeZip, zipEntries, zipRead } from '../zip.js';
import { chooseForNextGame, cleanName, dataSetsInZip, uniqueName } from '../datasets.js';
import { createCloud, inspectSetZip } from '../cloud.mjs';
import { createS3 } from '../s3.mjs';
import { createApp } from '../server.mjs';
import { fakeDataSet, fakeGoogle, fakeS3 } from './fakes.mjs';

const setZip = (name, files = fakeDataSet()) => writeZip(Object.entries(files).map(([f, data]) => ({ name: `${name}/${f}`, data })));

test('zip: written, read back, a damaged byte found', async () => {
  assert.equal(crc32(new TextEncoder().encode('123456789')), 0xcbf43926, 'the CRC-32 check value');
  const big = new Uint8Array(200000).map((_, i) => (i % 97 === 0 ? i & 0xff : 7));
  const zip = await writeZip([{ name: 'a/big.bin', data: big }, { name: 'a/small.txt', data: new TextEncoder().encode('hi') }]);
  const entries = zipEntries(zip);
  assert.deepEqual(entries.map((e) => [e.name, e.size, e.method]), [['a/big.bin', 200000, 8], ['a/small.txt', 2, 0]], 'deflated, or stored when that is smaller');
  assert.deepEqual(await zipRead(zip, entries[0]), big);
  const bad = zip.slice();
  bad[entries[0].offset + 30 + 9 + 20] ^= 0xff;                 // a byte inside the deflated data
  await assert.rejects(zipRead(bad, zipEntries(bad)[0]), /damaged|bytes|invalid|incorrect|unexpected/i);
  assert.throws(() => zipEntries(new Uint8Array(100)), /not a zip/);
});

test('the data sets in a zip: one per folder with the 20 files; junk and half sets left out', async () => {
  const files = fakeDataSet();
  const two = await writeZip([
    ...Object.entries(files).map(([f, data]) => ({ name: `Databases/December 2025/${f}`, data })),
    ...Object.entries(files).map(([f, data]) => ({ name: `Databases/My edit/${f.toUpperCase()}`, data })),
    { name: '__MACOSX/Databases/My edit/._staff.dat', data: new Uint8Array(4) },
    { name: 'Half/staff.dat', data: files['staff.dat'] },
  ]);
  const sets = await dataSetsInZip(two, 'fallback');
  assert.deepEqual(sets.map((s) => s.name).sort(), ['December 2025', 'My edit'], 'the folder names; any case of the file names');
  assert.deepEqual(sets[0].files['index.dat'], files['index.dat']);
  const withExtras = await writeZip([...Object.entries(files).map(([f, data]) => ({ name: `Update/${f}`, data })),
    { name: 'Update/player_setup.cfg', data: new Uint8Array(9) }, { name: 'Update/442_default.pct', data: new Uint8Array(5) },
    { name: 'Update/logo.RGN', data: new Uint8Array(7) }, { name: 'Update/readme.txt', data: new Uint8Array(3) }]);
  const [u] = await dataSetsInZip(withExtras, 'x');
  assert.deepEqual(Object.keys(u.files).filter((f) => !f.endsWith('.dat')).sort(), ['442_default.pct', 'player_setup.cfg'],
    'the settings and tactics files stay; pictures and others do not');
  const inData = await writeZip(Object.entries(files).map(([f, data]) => ({ name: `December 2025/Data/${f}`, data })));
  assert.deepEqual((await dataSetsInZip(inData, 'x')).map((s) => s.name), ['December 2025'], 'a Data folder takes the name above it');
  const top = await writeZip(Object.entries(files).map(([f, data]) => ({ name: f, data })));
  assert.deepEqual((await dataSetsInZip(top, 'From the zip name')).map((s) => s.name), ['From the zip name']);
  await assert.rejects(dataSetsInZip(await writeZip([{ name: 'x/staff.dat', data: files['staff.dat'] }]), 'x'), /no folder of the zip holds/);
});

test('names, and the choice for the next new game', async () => {
  assert.equal(cleanName(' a/b:c* '), 'a b c');
  assert.equal(cleanName('...'), 'Data set');
  assert.equal(uniqueName('Set', ['set', 'Set (2)']), 'Set (3)');
  // An engine with the runtime call: it does the choice.
  const calls = [];
  const M = { _rt_web_choose_data_set: () => 1, ccall: (...a) => { calls.push(a); return 1; } };
  assert.equal(await chooseForNextGame(M, null, '/user', 'My edit', 2001), 'now');
  assert.deepEqual(calls[0], ['rt_web_choose_data_set', 'number', ['string', 'number'], ['My edit', 2001]]);
  await assert.rejects(() => chooseForNextGame({ ...M, ccall: () => 0 }, null, '/user', 'x', 2001), /did not take/);
  // With the vault on: the call goes through ccall's async form.
  const asyncCalls = [];
  const A = { _rt_web_choose_data_set: () => 1, chooseAsync: true, ccall: (...a) => { asyncCalls.push(a); return Promise.resolve(1); } };
  assert.equal(await chooseForNextGame(A, null, '/user', 'My edit', 2025), 'now');
  assert.deepEqual(asyncCalls[0][4], { async: true });
  // An older engine: the settings file gets the lines, for the next start.
  const fs = new Map([['/user/settings.txt', '# settings\ndisplay = widescreen\ndatabase = 3.9.68\n']]);
  const FS = { readFile: (p) => { if (!fs.has(p)) throw new Error('ENOENT'); return fs.get(p); }, writeFile: (p, t) => fs.set(p, t) };
  assert.equal(await chooseForNextGame({}, FS, '/user', 'My edit', 2025), 'next start');
  assert.equal(fs.get('/user/settings.txt'), '# settings\ndisplay = widescreen\ndatabase = folder:My edit\nseason = 2025/26\n');
});

let tmp, s3fake, google, base, server;
before(async () => {
  tmp = mkdtempSync(join(tmpdir(), 'cm0102-sets-test-'));
  mkdirSync(join(tmp, 'game'));
  writeFileSync(join(tmp, 'manifest.tsv'), '');
  s3fake = await fakeS3();
  google = await fakeGoogle();
  const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const cloud = createCloud({ s3, clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'q'.repeat(40), google: google.google,
    cloudUsers: ['sets@example.com'], maxSets: 2, log: () => {} });
  server = createApp({ gameDir: join(tmp, 'game'), buildDir: join(tmp, 'build'), manifestPath: join(tmp, 'manifest.tsv'), cloud });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  base = `http://127.0.0.1:${server.address().port}`;
});
after(() => { server.close(); s3fake.close(); google.close(); rmSync(tmp, { recursive: true, force: true }); });

test('data sets in the cloud: store, list, fetch, conflict, checks, limit, account deletion', async () => {
  google.user = { sub: '9000009', email: 'sets@example.com', email_verified: true, name: 'Sets' };
  const start = await fetch(`${base}/auth/google/start`, { redirect: 'manual' });
  const oauth = start.headers.getSetCookie()[0].split(';')[0];
  const back = await fetch((await fetch(start.headers.get('location'), { redirect: 'manual' })).headers.get('location'), { redirect: 'manual', headers: { cookie: oauth } });
  const cookie = back.headers.getSetCookie().find((c) => c.startsWith('cm_session=')).split(';')[0];
  const send = (method, path, body, h = {}) => fetch(`${base}${path}`, { method, body, headers: { cookie, 'x-cm': '1', ...h } });
  const zip = await setZip('My edit');
  assert.deepEqual(inspectSetZip(zip).why, null);
  assert.equal(inspectSetZip(zip).rawSize, Object.values(fakeDataSet()).reduce((a, b) => a + b.length, 0));
  let r = await send('PUT', `/api/datasets/${encodeURIComponent('My edit')}`, zip, { 'x-base-etag': 'none', 'x-save-time': '1762800000000' });
  assert.equal(r.status, 200, await r.clone().text());
  const first = await r.json();
  const list = await (await send('GET', '/api/datasets')).json();
  assert.deepEqual(list.sets.map((s) => [s.name, s.etag, s.saved]), [['My edit', first.etag, 1762800000000]]);
  assert.equal(list.maxSets, 2);
  r = await send('GET', `/api/datasets/${encodeURIComponent('My edit')}`);
  assert.deepEqual(new Uint8Array(await r.arrayBuffer()), zip);
  assert.equal((await send('PUT', `/api/datasets/${encodeURIComponent('My edit')}`, zip, { 'x-base-etag': '"old"' })).status, 409);
  assert.equal((await send('PUT', '/api/datasets/x', await writeZip([{ name: 'a.txt', data: new Uint8Array(3) }]))).status, 422);
  assert.equal((await send('PUT', '/api/datasets/x', new Uint8Array([1, 2, 3]))).status, 422);
  assert.equal((await send('PUT', '/api/datasets/..%2Fx', zip)).status, 400);
  assert.equal((await send('PUT', '/api/datasets/Second', zip)).status, 200);
  assert.equal((await send('PUT', '/api/datasets/Third', zip)).status, 507, 'two at most here');
  assert.equal((await send('DELETE', '/api/datasets/Second')).status, 204);
  assert.equal((await (await send('GET', '/api/datasets')).json()).sets.length, 1);
  assert.equal((await send('DELETE', '/api/account')).status, 204);
  assert.equal([...s3fake.objects.keys()].some((k) => k.includes('9000009')), false, 'the data sets go with the account');
});
