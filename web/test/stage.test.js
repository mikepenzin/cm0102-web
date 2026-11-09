// Tests of web/deploy/stage.mjs: the staged folder, its brotli copies, the kept fly.toml, the refusals, and the staged
// server running as the Dockerfile starts it. Run: cd web && npm test
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createHash, randomBytes } from 'node:crypto';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import http from 'node:http';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { brotliDecompressSync } from 'node:zlib';
import { stage } from '../deploy/stage.mjs';

let tmp, opts;
const sha = (b) => createHash('sha256').update(b).digest('hex');

before(() => {
  tmp = mkdtempSync(join(tmpdir(), 'cm0102-stage-test-'));
  const game = join(tmp, 'game'), build = join(tmp, 'build-web');
  mkdirSync(join(game, 'Data'), { recursive: true });
  mkdirSync(join(build, 'seasons'), { recursive: true });
  const exe = Buffer.from('MZ'.padEnd(20000, 'this compresses well '));
  const staff = randomBytes(5000);                       // does not compress: no .br is kept
  writeFileSync(join(game, 'cm0102.exe'), exe);
  writeFileSync(join(game, 'Data', 'staff.dat'), staff);
  writeFileSync(join(game, 'extra.txt'), 'not in the manifest, not staged');
  const manifest = join(tmp, 'manifest.tsv');
  writeFileSync(manifest, `# sha256\tsize\tpath\n${sha(exe)}\t${exe.length}\tcm0102.exe\n${sha(staff)}\t${staff.length}\tData/staff.dat\n`);
  writeFileSync(join(build, 'cm0102_recomp.js'), '// engine glue '.repeat(500));
  writeFileSync(join(build, 'cm0102_recomp.wasm'), Buffer.concat([Buffer.from([0, 0x61, 0x73, 0x6d]), Buffer.alloc(8000)]));
  for (const t of ['rules', 'names', 'clubs', 'club_records']) writeFileSync(join(build, 'seasons', `${t}.tsv`), `${t}\n`);
  writeFileSync(join(build, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=jspi\n');
  opts = { game, build, out: join(tmp, 'stage'), manifest };
});

after(() => rmSync(tmp, { recursive: true, force: true }));

test('the staged folder: server, page, engine, tables, clean files, brotli copies, Docker and Fly files', async () => {
  const r = await stage(opts);
  const o = opts.out;
  for (const p of ['web/server.mjs', 'web/index.html', 'web/loader.js', 'web/saves.js', 'tools/clean_files/manifest.tsv', 'game/cm0102.exe',
    'game/Data/staff.dat', 'build-web/cm0102_recomp.js', 'build-web/cm0102_recomp.wasm', 'build-web/seasons/rules.tsv',
    'Dockerfile', 'fly.toml']) assert.ok(existsSync(join(o, p)), p);
  assert.equal(existsSync(join(o, 'game/extra.txt')), false, 'only manifest files');
  assert.ok(existsSync(join(o, 'game/cm0102.exe.br')), 'a file that compresses gets a .br');
  assert.equal(existsSync(join(o, 'game/Data/staff.dat.br')), false, 'a file that does not, gets none');
  assert.match(readFileSync(join(o, 'build-web/CMakeCache.txt'), 'utf8'), /CM0102_WEB_SUSPEND:STRING=jspi/);
  assert.ok(r.sent < r.raw);
});

test('a second stage keeps the fly.toml that fly launch wrote', async () => {
  writeFileSync(join(opts.out, 'fly.toml'), 'app = "mine"\n');
  const r = await stage(opts);
  assert.equal(r.keptToml, true);
  assert.equal(readFileSync(join(opts.out, 'fly.toml'), 'utf8'), 'app = "mine"\n');
});

test('the staged server runs as the Dockerfile starts it, and sends brotli', async () => {
  const o = opts.out;
  const port = 8000 + Math.floor(Math.random() * 900);
  const server = spawn(process.execPath, [join(o, 'web/server.mjs')], { env: { ...process.env, HOST: '127.0.0.1', PORT: String(port),
    CM_GAME_DIR: join(o, 'game'), CM_WEB_BUILD: join(o, 'build-web'), CM_WEB_SKIP_VERIFY: '1' }, stdio: ['ignore', 'pipe', 'pipe'] });
  try {
    await new Promise((resolveUp, reject) => {
      server.stdout.on('data', (d) => { if (/open http/.test(d)) resolveUp(); });
      server.on('exit', (c) => reject(new Error(`server ended ${c}`)));
    });
    const info = await (await fetch(`http://127.0.0.1:${port}/files.json`)).json();
    assert.equal(info.engine, true);
    assert.deepEqual(info.missing, []);
    const body = await new Promise((resolveGet) => http.get(`http://127.0.0.1:${port}/game/cm0102.exe`,
      { headers: { 'accept-encoding': 'br' } }, (res) => {
        const chunks = [];
        res.on('data', (c) => chunks.push(c));
        res.on('end', () => resolveGet({ enc: res.headers['content-encoding'], data: Buffer.concat(chunks) }));
      }));
    assert.equal(body.enc, 'br');
    assert.ok(brotliDecompressSync(body.data).toString().startsWith('MZthis compresses well'));
  } finally {
    server.kill();
  }
});

test('the stage refuses a game file that is not the clean file, and a missing engine', async () => {
  const staff = join(opts.game, 'Data', 'staff.dat');
  const good = readFileSync(staff);
  const bad = Buffer.from(good);
  bad[0] ^= 0xff;                                          // the same size, another file
  writeFileSync(staff, bad);
  await assert.rejects(stage(opts), /not the clean files.*Data\/staff\.dat/);
  writeFileSync(staff, good);
  rmSync(join(opts.build, 'cm0102_recomp.wasm'));
  await assert.rejects(stage(opts), /engine is not built/);
});

test('the stage names the folder problem: no folder, or the files one level deeper', async () => {
  await assert.rejects(stage({ ...opts, game: join(tmp, 'nowhere') }), /nowhere does not exist/);
  await assert.rejects(stage({ ...opts, game: tmp }), (err) => err.message.includes(`but ${join(tmp, 'game')} has`));
});
