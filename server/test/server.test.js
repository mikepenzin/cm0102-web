// Tests of the server with the stand-in engine (standin.c), which speaks the engine worker protocol with the
// runtime's own code. Needs gcc and python3. Run: cd server && npm test
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { mkdtempSync, rmSync, existsSync, readdirSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { start, configFromEnv } from '../src/server.js';
import { Engine } from '../src/engine.js';
import { readSave, readPart } from '../src/savfile.js';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, '..', '..');
let tmp, standin;

before(() => {
  tmp = mkdtempSync(join(tmpdir(), 'cm0102-server-test-'));
  standin = join(tmp, 'standin');
  execFileSync('gcc', ['-std=c11', '-D_DEFAULT_SOURCE', '-Wall', '-Wextra', '-Werror', '-O1',
    `-I${join(ROOT, 'src/recomp_rt/win')}`, join(HERE, 'standin.c'), join(ROOT, 'src/recomp_rt/win/worker_proto.c'),
    '-o', standin]);
});

after(() => rmSync(tmp, { recursive: true, force: true }));

function config(dataDir, extra = {}) {
  const c = configFromEnv({ CM_ENGINE: standin, CM_DATA: dataDir });
  c.engine.env = { STANDIN_DAY_MS: '1' };
  c.engine.turnTimeoutMs = 20000;
  return { ...c, ...extra };
}

async function listen(app) {
  await new Promise((r) => app.server.listen(0, '127.0.0.1', r));
  const base = `http://127.0.0.1:${app.server.address().port}`;
  const call = async (method, path, body) => {
    const r = await fetch(base + path, { method, headers: { 'Content-Type': 'application/json' }, body: body && JSON.stringify(body) });
    return { status: r.status, body: await r.json().catch(() => null), text: r.headers.get('content-type') };
  };
  return { base, call };
}

async function until(fn, ms = 20000) {
  const end = Date.now() + ms;
  for (;;) {
    const v = await fn();
    if (v) return v;
    if (Date.now() > end) throw new Error('timed out');
    await new Promise((r) => setTimeout(r, 25));
  }
}

const ready = (call, id) => until(async () => {
  const { body } = await call('GET', `/api/games/${id}`);
  return body.game.status !== 'starting' && !body.game.busy ? body : null;
});

test('the step files match the test fixtures', () => {
  const r = spawnSync('python3', ['-I', join(ROOT, 'server/steps/export_steps.py'), '--check'], { encoding: 'utf8' });
  assert.equal(r.status, 0, r.stdout + r.stderr);
});

test('the save reader agrees with tools/save/savfile.py', async () => {
  const work = join(tmp, 'savecheck');
  const engine = new Engine({ program: standin, work, env: { STANDIN_DAY_MS: '1' }, turnTimeoutMs: 10000 });
  await engine.waitTurn();
  await engine.advance(45);
  const file = await engine.save('check');
  await engine.quit();
  const save = readSave(file);
  const part = readPart(save, 'standin.dat');
  const py = spawnSync('python3', ['-I', '-c', `
import json, sys
sys.path.insert(0, sys.argv[1])
import savfile
s = savfile.Sav(sys.argv[2])
print(json.dumps({"version": s.version, "parts": [[m.name, m.offset, m.size] for m in s.members],
                  "data": s.read_member("standin.dat").hex()}))`, join(ROOT, 'tools/save'), file], { encoding: 'utf8' });
  assert.equal(py.status, 0, py.stderr);
  const want = JSON.parse(py.stdout);
  assert.equal(save.version, want.version);
  assert.deepEqual(save.parts.map((p) => [p.name, p.offset, p.size]), want.parts);
  assert.equal(part.toString('hex'), want.data);
  assert.equal(part.readInt16LE(0), 194 + 45);          // 28.8.2001: day 239 from 0
  assert.equal(part.readInt16LE(2), 2001);
});

test('a game: new, advance, idle stop, load, crash, restart', async () => {
  const data = join(tmp, 'data');
  let app = start(config(data, { idleMs: 400 }));
  let { call } = await listen(app);

  // The page and the errors.
  const page = await fetch(`http://127.0.0.1:${app.server.address().port}/`);
  assert.match(await page.text(), /CM 01\/02 Server/);
  assert.equal((await call('GET', '/api/games/99')).status, 404);
  assert.equal((await call('GET', '/nothing')).status, 404);

  // A new game: the stand-in starts on 14.7.2001 at Arsenal, and the first save follows.
  const made = await call('POST', '/api/games', { name: 'Test <one>' });
  assert.equal(made.status, 202);
  const id = made.body.game.id;
  let g = (await ready(call, id)).game;
  assert.equal(g.status, 'ready', g.error);
  assert.deepEqual([g.day, g.month, g.year, g.club, g.name], [14, 7, 2001, 'Arsenal', 'Test <one>']);
  assert.equal(g.save_parts, 1);
  assert.ok(existsSync(join(data, 'saves', g.save_file)));

  // Bad input.
  assert.equal((await call('POST', `/api/games/${id}/advance`, { days: 0 })).status, 400);
  assert.equal((await call('POST', `/api/games/${id}/advance`, { days: 401 })).status, 400);
  assert.equal((await call('POST', `/api/games/${id}/advance`, { days: 'x' })).status, 400);

  // Advance in the warm engine; a second step while one runs gets 409.
  assert.equal((await call('POST', `/api/games/${id}/advance`, { days: 20 })).status, 202);
  assert.equal((await call('POST', `/api/games/${id}/advance`, { days: 1 })).status, 409);
  g = (await ready(call, id)).game;
  assert.deepEqual([g.day, g.month, g.year], [3, 8, 2001]);
  assert.equal(app.games.warmCount(), 1);

  // After the idle time the engine stops; the next step loads the latest save into a new engine.
  await until(() => app.games.warmCount() === 0);
  assert.equal(readdirSync(join(data, 'work')).length, 0, 'the work folder of a stopped engine is removed');
  assert.ok(readdirSync(join(data, 'logs')).length >= 1, 'its log is kept');
  await call('POST', `/api/games/${id}/advance`, { days: 7 });
  let body = await ready(call, id);
  assert.deepEqual([body.game.day, body.game.month], [10, 8]);
  assert.ok(body.steps.some((s) => s.action === 'load' && s.result === '3.8.2001'), JSON.stringify(body.steps));

  // A crash of the engine in the middle of a step: the game is failed, the next step goes on from the last save.
  app.games.engineConfig.env = { STANDIN_DAY_MS: '50' };   // slow days: 300 take 15 s, the kill comes first
  await until(() => app.games.warmCount() === 0);           // so that a new engine with the slow days loads
  await call('POST', `/api/games/${id}/advance`, { days: 300 });
  await until(() => app.games.warm.get(id)?.engine.atTurn === false);
  app.games.warm.get(id).engine.child.kill('SIGKILL');
  g = (await ready(call, id)).game;
  assert.equal(g.status, 'failed');
  assert.match(g.error, /the engine ended \(signal SIGKILL\)/);
  assert.deepEqual([g.day, g.month], [10, 8], 'the read tables keep the last saved state');
  app.games.engineConfig.env = { STANDIN_DAY_MS: '1' };
  await call('POST', `/api/games/${id}/advance`, { days: 1 });
  g = (await ready(call, id)).game;
  assert.equal(g.status, 'ready', g.error);
  assert.deepEqual([g.day, g.month], [11, 8]);

  // A restart of the server: the game is still there and goes on from its save.
  await app.close();
  app = start(config(data, { idleMs: 60000 }));
  ({ call } = await listen(app));
  const list = (await call('GET', '/api/games')).body.games;
  assert.equal(list.length, 1);
  await call('POST', `/api/games/${id}/advance`, { days: 30 });
  g = (await ready(call, id)).game;
  assert.deepEqual([g.day, g.month, g.year, g.status], [10, 9, 2001, 'ready']);
  assert.equal(readdirSync(join(data, 'saves', String(id))).length, 3, 'three saves kept per game');
  await app.close();
});

test('an engine that ends before its first turn fails the game with its log', async () => {
  const data = join(tmp, 'data-bad');
  const c = config(data);
  c.engine.program = '/bin/false';
  const app = start(c);
  const { call } = await listen(app);
  const id = (await call('POST', '/api/games', { name: 'bad' })).body.game.id;
  const g = (await ready(call, id)).game;
  assert.equal(g.status, 'failed');
  assert.match(g.error, /the engine ended \(code 1\)/);
  assert.equal((await call('POST', `/api/games/${id}/advance`, { days: 1 })).status, 409);
  await app.close();
});
