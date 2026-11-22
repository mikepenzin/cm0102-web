// The engine's sleeps while the tab is hidden (web/ui.js, workerTimer; web/README.md "How it works"): they go through a
// worker's timer, which a background tab does not slow, and each resolves once, in its own time.
import test from 'node:test';
import assert from 'node:assert/strict';
import { workerTimer } from '../ui.js';

function fakeWorker() {
  const w = { posted: [], onmessage: null, postMessage(m) { this.posted.push(m); setTimeout(() => this.onmessage({ data: m.id }), m.ms); } };
  return w;
}

test('a hidden sleep: through one worker, made at the first sleep, each sleep resolving after its time', async () => {
  let made = 0, worker = null;
  const sleep = workerTimer(() => { made++; worker = fakeWorker(); return worker; });
  assert.equal(made, 0, 'no worker before the first sleep');
  const t0 = Date.now();
  const order = [];
  await Promise.all([sleep(30).then(() => order.push(30)), sleep(0).then(() => order.push(0)), sleep(1).then(() => order.push(1))]);
  assert.equal(made, 1);
  assert.deepEqual(order, [0, 1, 30]);
  assert.ok(Date.now() - t0 >= 25);
  assert.deepEqual(worker.posted.map((m) => m.ms), [30, 0, 1]);
  assert.equal(worker.posted.length, new Set(worker.posted.map((m) => m.id)).size, 'one id per sleep');
});

test('no worker in this browser: null, so the engine\'s own sleep runs', () => {
  const sleep = workerTimer(() => { throw new Error('no Worker'); });
  assert.equal(sleep(0), null);
  assert.equal(sleep(5), null);
});
