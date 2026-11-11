// Tests of the statistics (web/stats.mjs) and their routes in the server: heartbeats, the link of a browser to an
// account, the days, error reports, the admin's access. Run: cd web && npm test
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { browserOf, createStats } from '../stats.mjs';
import { createApp } from '../server.mjs';

const A = '11111111-2222-4333-8444-555555555555', B = '99999999-2222-4333-8444-555555555555';
const memoryStore = () => { const m = new Map(); return { m, get: async (k) => m.get(k) ?? null, put: async (k, v) => { m.set(k, v); },
  list: async (p) => [...m.keys()].filter((k) => k.startsWith(p)).map((key) => ({ key })), delete: async (k) => { m.delete(k); } }; };
const state = (n) => ({ saves: { n, bytes: n * 1000 }, older: { n: 1, bytes: 500 }, sets: { n: 0, bytes: 0 } });

test('heartbeats: play time, visits and days per person; an anonymous browser that signs in counts with its account', async () => {
  let t = Date.parse('2026-11-10T10:00:00Z');
  const s = createStats({ now: () => t });
  assert.equal(await s.beat({ anon: 'nope' }, null), 'no browser id');
  assert.equal(await s.beat({ anon: A, session: 's1', seconds: 60, state: state(3), engine: 'v1' }, null, 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 Chrome/142.0.0.0 Safari/537.36'), null);
  await s.beat({ anon: A, session: 's1', seconds: 9999 }, null);          // at most 180 s a beat
  let p = s.data.people[`a:${A}`];
  assert.equal(p.seconds, 240);
  assert.equal(p.sessions, 1);
  assert.equal(p.days, 1);
  assert.deepEqual(p.saves, { n: 3, bytes: 3000 });
  assert.equal(p.browser, 'Chrome 142 · Mac');
  t += 86400000;                                                          // the next day, a new visit, signed in
  await s.beat({ anon: A, session: 's2', seconds: 30, cloud: true }, { sub: '42', email: 'Mike@Example.com', name: 'Mike' });
  const g = s.data.people['g:42'];
  assert.equal(g.kind, 'account');
  assert.equal(g.cloud, true);
  assert.deepEqual(g.devices, [`a:${A}`]);
  assert.equal(s.data.people[`a:${A}`].account, 'g:42', 'the browser points at the account');
  await s.beat({ anon: B, session: 's3', seconds: 10, state: state(2) }, null);
  const sum = await s.summary();
  assert.equal(sum.totals.saves, 2, 'saves of the people: the linked browser\'s count with its account, not twice');
  assert.equal(sum.totals.savesBytes, 2000);
  assert.equal(sum.totals.hours, 280 / 3600, 'hours of every record');
  assert.equal(sum.totals.people, 2, 'the account and the other browser; the first browser counts with the account');
  assert.equal(sum.totals.accounts, 1);
  assert.equal(sum.totals.anonymous, 1);
  assert.equal(sum.totals.activeToday, 2);
  assert.equal(sum.days.length, 2);
  assert.equal(sum.days[0].active, 1, 'yesterday: one browser');
  assert.equal(sum.days[0].seconds, 240);
  assert.equal(sum.days[0].new, 1);
  assert.equal(sum.days[1].new, 2);
  // An account that is deleted goes; its browser no longer points at it.
  await s.forget('42');
  assert.equal(s.data.people['g:42'], undefined);
  assert.equal(s.data.people[`a:${A}`].account, undefined);
  p = (await s.summary()).totals;
  assert.equal(p.people, 2);
});

test('the store: written when changed, read back, pruned after 400 days; reports kept as files', async () => {
  let t = Date.parse('2026-01-01T00:00:00Z');
  const store = memoryStore();
  const s = createStats({ store, now: () => t });
  assert.equal(await s.flush(), false, 'nothing to write');
  await s.beat({ anon: A, session: 'x', seconds: 60, state: state(1) }, null);
  const { key } = await s.report({ anon: A, message: 'The game stopped: RuntimeError', log: ['a', 'b'], engine: 'v1' }, null);
  assert.match(key, /^reports\/2026-01-01\/\d+-[a-z0-9]+\.json$/);
  assert.deepEqual((await s.reportFile(key)).log, ['a', 'b']);
  assert.equal(await s.reportFile('../accounts/x.json'), null, 'only report files');
  assert.equal(await s.flush(), true);
  const again = createStats({ store, now: () => t });
  await again.ready;
  assert.equal(again.data.people[`a:${A}`].seconds, 60);
  assert.equal((await again.summary()).reports.length, 1);
  assert.equal(again.data.people[`a:${A}`].reports, 1);
  t += 401 * 86400000;
  await again.beat({ anon: B, session: 'y', seconds: 1 }, null);
  await again.flush();
  assert.equal(again.data.people[`a:${A}`], undefined, 'idle for 400 days: gone');
  assert.equal(store.m.has(key), false, 'the report file went after 90 days');
  assert.equal((await again.summary()).reports.length, 0);
  assert.ok(again.data.people[`a:${B}`]);
});

test('browserOf: the browser and the system in a few words', () => {
  assert.equal(browserOf('Mozilla/5.0 (iPad; CPU OS 18_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Mobile/15E148 Safari/604.1'), 'Safari 18 · iPad');
  assert.equal(browserOf('Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/142.0.0.0 Safari/537.36 Edg/142.0.0.0'), 'Edge 142 · Windows');
  assert.equal(browserOf('Mozilla/5.0 (X11; Linux x86_64; rv:140.0) Gecko/20100101 Firefox/140.0'), 'Firefox 140 · Linux');
});

test('the routes: a beat only from the page, the admin data only for an admin', async () => {
  const tmp = mkdtempSync(join(tmpdir(), 'cm0102-stats-test-'));
  const gameDir = join(tmp, 'game'), buildDir = join(tmp, 'build');
  mkdirSync(gameDir); mkdirSync(buildDir);
  writeFileSync(join(gameDir, 'cm0102.exe'), 'MZ');
  const manifestPath = join(tmp, 'm.tsv');
  writeFileSync(manifestPath, 'x\t2\tcm0102.exe\n');
  const stats = createStats();
  const signedIn = (req) => (req.headers.cookie === 'admin' ? { sub: '1', email: 'boss@example.com' } : req.headers.cookie === 'user' ? { sub: '2', email: 'p@example.com' } : null);
  const cloud = { userOf: signedIn, handle: (req, res) => { res.writeHead(404); res.end(); } };
  const server = createApp({ gameDir, buildDir, manifestPath, stats, cloud, admins: ['Boss@example.com'], adminToken: 'a-token-of-16-chars-or-more' });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  const base = `http://127.0.0.1:${server.address().port}`;
  try {
    const beat = (headers, body = { anon: A, session: 's', seconds: 60 }) => fetch(`${base}/api/stats/beat`, { method: 'POST', headers: { 'content-type': 'application/json', ...headers }, body: JSON.stringify(body) });
    assert.equal((await beat({})).status, 403, 'without the header of the page');
    assert.equal((await beat({ 'x-cm': '1', origin: 'https://evil.example' })).status, 403, 'from another site');
    assert.equal((await beat({ 'x-cm': '1' })).status, 204);
    assert.equal((await beat({ 'x-cm': '1', cookie: 'user' })).status, 204);
    assert.equal((await beat({ 'x-cm': '1' }, { anon: 'x' })).status, 400);
    const files = await (await fetch(`${base}/files.json`)).json();
    assert.deepEqual(files.stats, { mode: 'optout' });
    assert.equal((await fetch(`${base}/api/admin/stats`)).status, 401);
    assert.equal((await fetch(`${base}/api/admin/stats`, { headers: { cookie: 'user' } })).status, 403);
    const asAdmin = await fetch(`${base}/api/admin/stats`, { headers: { cookie: 'admin' } });
    assert.equal(asAdmin.status, 200);
    const sum = await asAdmin.json();
    assert.equal(sum.totals.people, 1, 'the browser signed in: one person');
    assert.equal(sum.totals.hours, 120 / 3600, 'its play before and after the sign-in');
    assert.equal((await fetch(`${base}/api/admin/stats`, { headers: { authorization: 'Bearer a-token-of-16-chars-or-more' } })).status, 200);
    assert.equal((await fetch(`${base}/api/admin/stats`, { headers: { authorization: 'Bearer wrong' } })).status, 403);
    const page = await fetch(`${base}/admin`);
    assert.equal(page.status, 200);
    assert.match(await page.text(), /CM 01\/02 admin/);
    const rep = await fetch(`${base}/api/report`, { method: 'POST', headers: { 'content-type': 'application/json', 'x-cm': '1' }, body: JSON.stringify({ anon: A, message: 'stopped', log: ['x'] }) });
    assert.equal(rep.status, 201);
    assert.equal((await stats.summary()).reports[0].message, 'stopped');
  } finally {
    server.close();
    rmSync(tmp, { recursive: true, force: true });
  }
});
