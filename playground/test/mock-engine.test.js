// Every kind of app-to-engine interaction, against the mock engine and the browser-side client.
// Run: cd playground && npm test
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { MockEngine, EngineError } from '../mock-engine.js';
import { EngineClient } from '../engine-client.js';

function engine(opts = {}) {
  const events = [];
  const e = new MockEngine({ ...opts, onEvent: (event, data) => events.push({ event, ...data }) });
  e.events = events;
  return e;
}

const call = async (e, method, params) => (await e.call(method, params)).result;

async function refused(promise, code) {
  await assert.rejects(promise, (err) => {
    assert.ok(err instanceof EngineError, err.message);
    assert.equal(err.code, code, err.message);
    return true;
  });
}

const decision = async (e, kind) => (await call(e, 'getInbox')).find((i) => i.kind === kind && !i.answered);

test('reads: the state, the squad and a player, with no hidden values', async () => {
  const e = engine();
  const r = await e.call('getState');
  assert.equal(r.date, '2001-07-14');
  assert.equal(r.result.club, 'Harbour City');
  assert.equal(r.result.money, 1500000);
  const squad = await call(e, 'getSquad');
  assert.equal(squad.length, 16);
  assert.deepEqual(squad.slice(0, 2).map((p) => p.position), ['GK', 'GK']);
  assert.equal(squad.filter((p) => p.selected).length, 11);
  for (const p of squad) assert.equal('ability' in p, false, 'a hidden value left the engine');
  const p = await call(e, 'getPlayer', { id: 'p13' });
  assert.equal(p.name, 'Jamie Calder');
  assert.equal(p.status, 'Fit');
  assert.equal('ability' in p, false);
  await refused(e.call('getPlayer', { id: 'nobody' }), 'not_found');
  await refused(e.call('fly'), 'unknown_method');
});

test('the engine refuses a bad eleven; the app holds no rules', async () => {
  const e = engine();
  await refused(e.call('selectTeam', { ids: ['p1', 'p2'] }), 'invalid_selection');
  const noKeeper = ['p3', 'p4', 'p5', 'p6', 'p7', 'p8', 'p9', 'p10', 'p11', 'p12', 'p13'];
  await refused(e.call('selectTeam', { ids: noKeeper }), 'invalid_selection');
  const notOurs = ['p1', 'p3', 'p4', 'p5', 'p6', 'p8', 'p9', 'p10', 'p11', 'p13', 't1'];
  await refused(e.call('selectTeam', { ids: notOurs }), 'invalid_selection');
  const step = e.step;
  const ok = ['p2', 'p3', 'p4', 'p5', 'p6', 'p8', 'p9', 'p10', 'p11', 'p13', 'p15'];
  assert.deepEqual((await call(e, 'selectTeam', { ids: ok })).selection, ok);
  assert.ok(e.step > step, 'an action moves the step on');
});

test('a bid at the asking price: answered two days later, then the player signs', async () => {
  const e = engine();
  await refused(e.call('bid', { playerId: 't1', fee: 2000000 }), 'not_enough_money');
  await refused(e.call('bid', { playerId: 'p13', fee: 1000 }), 'not_found');
  const bid = await call(e, 'bid', { playerId: 't1', fee: 1200000 });
  assert.equal(bid.answerDate, '2001-07-16');
  await refused(e.call('bid', { playerId: 't1', fee: 1200000 }), 'bid_pending');
  const run = await call(e, 'continue', { days: 7 });
  assert.equal(run.stopped, 'decision', 'the answer stops the days');
  assert.equal(run.daysPlayed, 2);
  const item = await decision(e, 'bid-accepted');
  assert.deepEqual(item.choices, ['Sign him', 'Withdraw']);
  assert.ok(e.events.some((x) => x.event === 'news' && x.item.id === item.id), 'the news came as an event too');
  await refused(e.call('reply', { newsId: item.id, choice: 'Maybe' }), 'bad_choice');
  await call(e, 'reply', { newsId: item.id, choice: 'Sign him' });
  await refused(e.call('reply', { newsId: item.id, choice: 'Sign him' }), 'already_answered');
  const squad = await call(e, 'getSquad');
  assert.ok(squad.some((p) => p.id === 't1'), 'he is in the squad');
  assert.equal((await call(e, 'getState')).money, 300000);
});

test('a low bid is refused, a near one gets a counter-offer', async () => {
  const e = engine();
  await call(e, 'bid', { playerId: 't2', fee: 300000 });
  await call(e, 'bid', { playerId: 't3', fee: 750000 });
  await call(e, 'continue', { days: 1 });
  assert.ok((await call(e, 'getInbox')).some((i) => i.kind === 'bid-refused'), 'refused after one day');
  const run = await call(e, 'continue', { days: 3 });
  assert.equal(run.stopped, 'decision');
  const counter = await decision(e, 'bid-counter');
  assert.deepEqual(counter.choices, ['Pay £900,000', 'Walk away']);
  await call(e, 'reply', { newsId: counter.id, choice: 'Pay £900,000' });
  assert.equal((await call(e, 'getState')).money, 600000);
  const targets = await call(e, 'getTransferTargets');
  assert.equal(targets.some((t) => t.id === 't3'), false, 'a signed player leaves the transfer list');
  assert.ok((await call(e, 'getSquad')).some((p) => p.id === 't3'), 'and joins the squad');
  assert.equal(targets.find((t) => t.id === 't2').yourBid.status, 'refused');
});

test('the engine acts on its own: another club bids for your player on day 5', async () => {
  const e = engine();
  const run = await call(e, 'continue', { days: 10 });
  assert.equal(run.stopped, 'decision');
  assert.equal((await e.call('getState')).date, '2001-07-19');
  const item = await decision(e, 'ai-bid');
  assert.match(item.title, /Elliot Shaw/);
  await call(e, 'reply', { newsId: item.id, choice: 'Accept' });
  const s = await call(e, 'getState');
  assert.equal(s.money, 2300000);
  assert.equal(s.selectionSize, 10, 'he left your eleven');
  assert.ok(!(await call(e, 'getSquad')).some((p) => p.id === 'p10'));
});

test('a match: continue stops on match day, commentary comes as events, the result changes the players', async () => {
  const e = engine();
  const ai = await call(e, 'continue', { toMatch: true });
  assert.equal(ai.stopped, 'decision');                     // the bid on day 5 stops the run first
  await call(e, 'reply', { newsId: (await decision(e, 'ai-bid')).id, choice: 'Refuse' });
  const run = await call(e, 'continue', { toMatch: true });
  assert.equal(run.stopped, 'match');
  assert.equal(run.matchToday.id, 'f1');
  await refused(e.call('continue', { days: 1 }), 'match_pending');
  await refused(e.call('playMatch', { id: 'f2' }), 'not_today');
  const before = e.events.length;
  const result = await call(e, 'playMatch', { id: 'f1' });
  const lines = e.events.slice(before).filter((x) => x.event === 'commentary');
  assert.ok(lines.length >= 4, `${lines.length} commentary lines`);
  assert.match(lines[0].text, /^Kick-off/);
  assert.match(lines.at(-1).text, /^Full time/);
  assert.ok(e.events.slice(before).some((x) => x.event === 'result'));
  assert.equal(result.played, true);
  assert.equal(result.score.length, 2);
  await refused(e.call('playMatch', { id: 'f1' }), 'already_played');
  const squad = await call(e, 'getSquad');
  const injured = squad.find((p) => p.injuredDays === 20);
  assert.equal(injured.name, 'Danny Wren', 'the scripted injury: the first midfielder of the eleven');
  assert.equal(injured.status, 'Injured, 20 days');
  assert.ok(squad.filter((p) => p.selected).every((p) => p.condition === 70), 'the eleven is tired');
  assert.ok(squad.filter((p) => p.selected).every((p) => p.appearances === 1));
  const match = await call(e, 'getMatch', { id: 'f1' });
  assert.equal(match.commentary.length, lines.length, 'the commentary is kept with the match');
  const off = lines.findIndex((l) => /goes down injured/.test(l.text));
  assert.ok(off > 0, 'the injury is in the commentary');
  assert.ok(!lines.slice(off + 1).some((l) => l.text.includes(injured.name)), 'nothing more from the injured player');
});

test('an injured player in the eleven: the engine refuses the next match until the eleven changes', async () => {
  const e = engine();
  await call(e, 'continue', { toMatch: true });
  await call(e, 'reply', { newsId: (await decision(e, 'ai-bid')).id, choice: 'Refuse' });
  await call(e, 'continue', { toMatch: true });
  await call(e, 'playMatch', { id: 'f1' });
  const run = await call(e, 'continue', { toMatch: true });
  assert.equal(run.stopped, 'match');
  assert.equal(run.matchToday.id, 'f2');
  assert.equal(run.matchToday.home, 'Harbour City');
  await refused(e.call('playMatch', { id: 'f2' }), 'invalid_selection');      // Danny Wren is still in the eleven
  const withWren = ['p1', 'p3', 'p4', 'p5', 'p6', 'p8', 'p9', 'p10', 'p11', 'p13', 'p14'];
  await refused(e.call('selectTeam', { ids: withWren }), 'invalid_selection');
  const withoutWren = ['p1', 'p3', 'p4', 'p5', 'p6', 'p12', 'p9', 'p10', 'p11', 'p13', 'p14'];
  await call(e, 'selectTeam', { ids: withoutWren });
  const played = await call(e, 'playMatch', { id: 'f2' });
  assert.equal(played.played, true);
  const next = await call(e, 'continue', { toMatch: true });
  assert.equal(next.matchToday.id, 'f4');
  assert.ok((await call(e, 'getInbox')).some((i) => i.kind === 'fit'), 'he is fit again on the way, on day 26');
  assert.equal((await call(e, 'getPlayer', { id: 'p8' })).status, 'Fit');
});

test('busy: while days run, every call is refused', async () => {
  const e = engine({ dayMs: 5 });
  const run = e.call('continue', { days: 3 });
  await refused(e.call('getState'), 'busy');
  await refused(e.call('selectTeam', { ids: [] }), 'busy');
  assert.equal((await run).result.daysPlayed, 3);
  assert.equal((await e.call('getState')).date, '2001-07-17');
});

test('save and load: the save is the game, and the step keeps growing', async () => {
  const e = engine();
  const { blob } = await call(e, 'save');
  await call(e, 'continue', { days: 3 });
  const stepBefore = e.step;
  await call(e, 'load', { blob });
  assert.equal((await e.call('getState')).date, '2001-07-14');
  assert.ok(e.step > stepBefore, 'a load is a new step, never an old one');
  await refused(e.call('load', { blob: '{"apiVersion":99,"state":{}}' }), 'bad_save');
  await refused(e.call('load', { blob: 'nonsense' }), 'bad_save');
});

test('the league table follows the played league matches', async () => {
  const e = engine();
  await call(e, 'continue', { toMatch: true });
  await call(e, 'reply', { newsId: (await decision(e, 'ai-bid')).id, choice: 'Refuse' });
  await call(e, 'continue', { toMatch: true });
  await call(e, 'playMatch', { id: 'f1' });
  await call(e, 'continue', { toMatch: true });
  const withoutWren = ['p1', 'p3', 'p4', 'p5', 'p6', 'p12', 'p9', 'p10', 'p11', 'p13', 'p14'];
  await call(e, 'selectTeam', { ids: withoutWren });
  await call(e, 'playMatch', { id: 'f2' });
  await call(e, 'continue', { days: 1 });                    // the other match of the day plays as the day ends
  const table = await call(e, 'getTable');
  assert.equal(table.length, 4);
  assert.deepEqual(table.map((r) => r.played).sort(), [1, 1, 1, 1]);
  assert.equal(table.reduce((a, r) => a + r.points, 0) >= 4, true);
});

test('the client: answers, refusals, events, and a stale answer is marked', async () => {
  const e = engine();
  const worker = {
    postMessage: async ({ id, method, params }) => {
      try { worker.onmessage({ data: { id, ok: true, ...(await e.call(method, params)) } }); }
      catch (err) { worker.onmessage({ data: { id, ok: false, error: { code: err.code, message: err.message } } }); }
    },
  };
  e.onEvent = (event, data) => worker.onmessage({ data: { event, data } });
  const client = new EngineClient(worker);
  const log = [];
  client.onLog = (x) => log.push(x.kind);
  const news = [];
  client.on('news', (d) => news.push(d.item.id));
  const s = await client.call('getState');
  assert.equal(s.result.club, 'Harbour City');
  assert.equal(s.stale, false);
  await assert.rejects(client.call('bid', { playerId: 't1', fee: 9e9 }), (err) => err.code === 'not_enough_money');
  await client.call('continue', { days: 10 });
  assert.ok(news.length >= 1, 'news events reach the listeners');
  worker.onmessage({ data: { id: 999, ok: true, step: 0, date: '2001-07-14', result: {} } }); // nobody waits for it
  client.pending.set(1000, { resolve: (r) => assert.equal(r.stale, true), reject: () => {}, method: 'getState' });
  worker.onmessage({ data: { id: 1000, ok: true, step: 1, date: '2001-07-14', result: {} } });
  assert.deepEqual([...new Set(log)].sort(), ['answer', 'call', 'event', 'refused']);
});
