// Tests of the web app's server: which files it lists and serves, and which it refuses. Run: cd web && npm test
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { createApp, inventory, verify } from '../server.mjs';

let tmp, gameDir, buildDir, manifestPath, base, server;

before(async () => {
  tmp = mkdtempSync(join(tmpdir(), 'cm0102-web-test-'));
  gameDir = join(tmp, 'game');
  buildDir = join(tmp, 'build-web');
  mkdirSync(join(gameDir, 'Data'), { recursive: true });
  mkdirSync(join(buildDir, 'seasons'), { recursive: true });
  writeFileSync(join(gameDir, 'cm0102.exe'), 'MZexe');
  writeFileSync(join(gameDir, 'Data', 'staff.dat'), 'abc');
  writeFileSync(join(gameDir, 'Data', 'club.dat'), 'too long');
  writeFileSync(join(gameDir, 'secret.txt'), 'not in the manifest');
  manifestPath = join(tmp, 'manifest.tsv');
  writeFileSync(manifestPath, ['# sha256\tsize\tpath', 'x\t5\tcm0102.exe', 'x\t3\tData/staff.dat', 'x\t4\tData/club.dat',
    'x\t9\tData/nation.dat', ''].join('\n'));
  writeFileSync(join(buildDir, 'cm0102_recomp.js'), '// engine');
  writeFileSync(join(buildDir, 'cm0102_recomp.wasm'), Buffer.from([0, 0x61, 0x73, 0x6d]));
  writeFileSync(join(buildDir, 'seasons', 'rules.tsv'), 'season\n');
  writeFileSync(join(buildDir, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=jspi\n');
  server = createApp({ gameDir, buildDir, manifestPath });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  base = `http://127.0.0.1:${server.address().port}`;
});

after(() => { server.close(); rmSync(tmp, { recursive: true, force: true }); });

test('the inventory: the manifest files of the right size, the season tables, the engine', () => {
  const inv = inventory({ gameDir, buildDir, manifestPath });
  assert.deepEqual(inv.files.map((f) => f.path), ['cm0102.exe', 'Data/staff.dat', 'Seasons/rules.tsv']);
  assert.deepEqual(inv.missing.map((m) => m.path),
    ['Data/club.dat', 'Data/nation.dat', 'Seasons/names.tsv', 'Seasons/clubs.tsv', 'Seasons/club_records.tsv']);
  assert.equal(inv.missing.find((m) => m.path === 'Data/club.dat').have, 8, 'a file of another size is missing too');
  assert.equal(inv.suspend, 'jspi');
  assert.ok(inv.engine.js && inv.engine.wasm);
});

test('files.json tells the page what to load and what is missing', async () => {
  const r = await fetch(`${base}/files.json`);
  assert.equal(r.status, 200);
  const info = await r.json();
  assert.equal(info.engine, true);
  assert.equal(info.totalBytes, 5 + 3 + 7);
  assert.equal(info.missing.length, 5);
  assert.equal(info.suspend, 'jspi');
  assert.equal(info.files.some((f) => 'disk' in f), false, 'no server paths leave the server');
});

test('listed files are served, everything else is refused', async () => {
  const exe = await fetch(`${base}/game/cm0102.exe`);
  assert.equal(exe.status, 200);
  assert.equal(await exe.text(), 'MZexe');
  assert.equal((await fetch(`${base}/game/Seasons/rules.tsv`)).status, 200);
  for (const path of ['/game/secret.txt', '/game/Data/club.dat', '/game/../../etc/passwd', '/game/%2e%2e/secret.txt',
    '/engine/other.js', '/CMakeCache.txt', '/server.mjs']) {
    assert.equal((await fetch(`${base}${path}`)).status, 404, path);
  }
  assert.equal((await fetch(`${base}/files.json`, { method: 'POST' })).status, 405);
});

test('the engine and the page: types and revalidation', async () => {
  const wasm = await fetch(`${base}/engine/cm0102_recomp.wasm`);
  assert.equal(wasm.headers.get('content-type'), 'application/wasm', 'streaming compile needs this type');
  const etag = wasm.headers.get('etag');
  assert.ok(etag);
  assert.equal((await fetch(`${base}/engine/cm0102_recomp.wasm`, { headers: { 'If-None-Match': etag } })).status, 304);
  const page = await fetch(`${base}/`);
  assert.match(page.headers.get('content-type'), /text\/html/);
  assert.match(await page.text(), /loader\.js/);
  assert.match((await fetch(`${base}/loader.js`)).headers.get('content-type'), /javascript/);
});

test('hosting: a .br copy goes to a browser that takes br, and a versioned request may be kept for good', async () => {
  const { brotliCompressSync, brotliDecompressSync } = await import('node:zlib');
  writeFileSync(join(gameDir, 'cm0102.exe.br'), brotliCompressSync(Buffer.from('MZexe')));
  const info = await (await fetch(`${base}/files.json`)).json();
  const exe = info.files.find((f) => f.path === 'cm0102.exe');
  assert.equal(exe.v, 'x', 'the version is the start of the SHA-256 in the manifest');
  const raw = (await import('node:http')).get;
  const get = (path, headers) => new Promise((resolveGet) => raw(`${base}${path}`, { headers }, (r) => {
    const chunks = [];
    r.on('data', (c) => chunks.push(c));
    r.on('end', () => resolveGet({ status: r.statusCode, headers: r.headers, body: Buffer.concat(chunks) }));
  }));
  const br = await get(`/game/cm0102.exe?v=${exe.v}`, { 'accept-encoding': 'gzip, br' });
  assert.equal(br.headers['content-encoding'], 'br');
  assert.equal(brotliDecompressSync(br.body).toString(), 'MZexe');
  assert.match(br.headers['cache-control'], /immutable/);
  assert.equal(br.headers.vary, 'Accept-Encoding');
  const plain = await get('/game/cm0102.exe', { 'accept-encoding': 'gzip' });
  assert.equal(plain.headers['content-encoding'], undefined);
  assert.equal(plain.body.toString(), 'MZexe');
  assert.equal(plain.headers['cache-control'], 'no-cache', 'no version, no keeping');
  const stale = await get('/game/cm0102.exe?v=old', {});
  assert.equal(stale.headers['cache-control'], 'no-cache', 'another version is never kept for good');
  rmSync(join(gameDir, 'cm0102.exe.br'));
});

test('verify: a file whose SHA-256 is not the clean file is named', async () => {
  const { createHash } = await import('node:crypto');
  const good = createHash('sha256').update('MZexe').digest('hex');
  const m = join(tmp, 'manifest-hash.tsv');
  writeFileSync(m, `${good}\t5\tcm0102.exe\n${'0'.repeat(64)}\t3\tData/staff.dat\n`);
  const inv = inventory({ gameDir, buildDir, manifestPath: m });
  assert.deepEqual(await verify(inv, m), ['Data/staff.dat']);
});
