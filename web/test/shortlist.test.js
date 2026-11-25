// Tests of shortlist.js: a human manager's Short List in a saved game (scout_manager.dat), read and changed as the game
// does (docs/research/2026-11-25-shortlist-in-save.md). Run: cd web && npm test
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { SaveGame, readDate } from '../savedit.js';
import { parseSearches, writeSearches, scoutManager } from '../shortlist.js';
import { fakeSaveGame } from './fakes.mjs';

// Two players (rows 0, 1), the coach (2), Klaus (3), Mike (4), a reserved place with no name (5).
const PLAYERS = [
  { first: 'Alessandro', second: 'Del Piero', ca: 181, pa: 190, gk: 1, born: [1974, 11, 9], club: 0, fill: 15 },
  { first: 'Gianluigi', second: 'Buffon', ca: 187, pa: 194, gk: 20, born: [1978, 1, 28], club: 0, fill: 12 },
];
const open = async (managers) => new SaveGame(await fakeSaveGame(PLAYERS, { managers }));
const KLAUS = 3, MIKE = 4;

test('the member reads and writes back byte for byte, a club report table and the tail kept', async () => {
  const g = await open([{ first: 'Klaus', second: 'Kastelmann', shortlist: [1], clubTable: true }, { first: 'Mike', second: 'Penzin' }]);
  const b = g.file.read('scout_manager.dat');
  const p = parseSearches(b);
  assert.equal(p.searches.length, 30, 'three places of ten lists');
  assert.deepEqual(Buffer.from(writeSearches(p)), Buffer.from(b));
});

test('the human managers: the places with a name, not the reserved ones', async () => {
  const sm = scoutManager(await open([{ first: 'Klaus', second: 'Kastelmann' }, { first: 'Mike', second: 'Penzin' }]));
  assert.deepEqual(sm.managers, [{ id: KLAUS, name: 'Klaus Kastelmann' }, { id: MIKE, name: 'Mike Penzin' }]);
  assert.equal(scoutManager(new SaveGame(await fakeSaveGame(PLAYERS))), null, 'no scout_manager.dat: no shortlist');
});

test('add: the entry as add_player builds it, in staff id order, count and capacity, the other manager untouched', async () => {
  const g = await open([{ first: 'Klaus', second: 'Kastelmann', shortlist: [1] }, { first: 'Mike', second: 'Penzin' }]);
  const sm = scoutManager(g);
  assert.deepEqual([...sm.ids(KLAUS)], [1]);
  assert.equal(sm.has(KLAUS, 0), false);
  assert.equal(sm.add(KLAUS, 0), true);
  assert.deepEqual([...sm.ids(KLAUS)], [0, 1], 'sorted by staff id');
  assert.deepEqual([...sm.ids(MIKE)], [], 'the other manager untouched');
  assert.equal(g.file.member('scout_manager.dat').changed, true);
  const p = parseSearches(g.file.read('scout_manager.dat')), list = p.searches.find((s) => s.manager === KLAUS && s.slot === 0);
  assert.deepEqual([list.count, list.capacity], [2, 250]);
  const e = list.results[0], row = g.staff.b.subarray(0, 24);
  assert.deepEqual([...e.subarray(0, 12)], [...row.subarray(4, 16)], 'the names');
  assert.deepEqual([...e.subarray(12, 16)], [...row.subarray(0, 4)], 'the id');
  assert.deepEqual([...e.subarray(16, 24)], [...row.subarray(16, 24)], 'the date of birth');
  assert.deepEqual(readDate(e, 0x18), { year: 2001, month: 7, day: 14 }, 'the date added: the game date');
  assert.deepEqual([...e.subarray(0x20, 0x2c)], new Array(12).fill(0));
  assert.equal(e[0x2c], 0xff, 'no note rating');
  assert.equal(sm.add(KLAUS, 0), false, 'already there');
});

test('the first add on an empty list sets capacity 250; a full list grows by 250', async () => {
  const g = await open([{ first: 'Klaus', second: 'Kastelmann' }, { first: 'Mike', second: 'Penzin', shortlist: [1], capacity: 1 }]);
  const sm = scoutManager(g);
  sm.add(KLAUS, 0);
  sm.add(MIKE, 0);
  const p = parseSearches(g.file.read('scout_manager.dat')), of = (m) => p.searches.find((s) => s.manager === m && s.slot === 0);
  assert.deepEqual([of(KLAUS).count, of(KLAUS).capacity], [1, 250]);
  assert.deepEqual([of(MIKE).count, of(MIKE).capacity], [2, 251]);
});

test('remove sets the removed bit; adding again clears it, one entry', async () => {
  const g = await open([{ first: 'Klaus', second: 'Kastelmann', shortlist: [0, 1] }, { first: 'Mike', second: 'Penzin' }]);
  const sm = scoutManager(g);
  assert.equal(sm.remove(KLAUS, 1), true);
  assert.equal(sm.has(KLAUS, 1), false);
  assert.equal(sm.remove(KLAUS, 1), false, 'not there any more');
  let list = parseSearches(g.file.read('scout_manager.dat')).searches.find((s) => s.manager === KLAUS && s.slot === 0);
  assert.deepEqual([list.count, list.results[1][0x20] & 1], [2, 1], 'kept, marked removed, as contains_player reads it');
  assert.equal(sm.add(KLAUS, 1), true);
  list = parseSearches(g.file.read('scout_manager.dat')).searches.find((s) => s.manager === KLAUS && s.slot === 0);
  assert.deepEqual([list.count, list.results[1][0x20] & 1], [2, 0]);
  const removedAtStart = scoutManager(await open([{ first: 'Klaus', second: 'Kastelmann', shortlist: [0], removed: [1] }, { first: 'Mike', second: 'Penzin' }]));
  assert.deepEqual([...removedAtStart.ids(KLAUS)], [0], 'a removed entry is not on the list');
});

test('the save written and read again keeps the change', async () => {
  const g = await open([{ first: 'Klaus', second: 'Kastelmann' }, { first: 'Mike', second: 'Penzin' }]);
  scoutManager(g).add(MIKE, 1);
  const again = new SaveGame(g.file.toBytes());
  assert.deepEqual([...scoutManager(again).ids(MIKE)], [1]);
  assert.deepEqual(Buffer.from(again.file.read('scout_manager.dat')), Buffer.from(g.file.read('scout_manager.dat')),
    'the member whole, to its last byte: the header names its new size, which the game reads');
});

// ---- Scout requests (docs/research/2026-11-25-scout-request-in-save.md): Klaus's club 100, Mike's 101. ----
const reqs = (g) => {
  const { rest } = parseSearches(g.file.read('scout_manager.dat')), dv = new DataView(rest.buffer, rest.byteOffset, rest.byteLength);
  const n = dv.getInt32(0, true);
  return { n, list: Array.from({ length: n }, (_, k) => [dv.getInt32(4 + k * 17, true), dv.getInt32(8 + k * 17, true), rest[20 + k * 17]]),
    last: [...rest.subarray(4 + n * 17)], at: (k) => rest.subarray(4 + k * 17, 21 + k * 17) };
};
const withReqs = (requests, managers = [{ first: 'Klaus', second: 'Kastelmann' }, { first: 'Mike', second: 'Penzin' }]) =>
  fakeSaveGame(PLAYERS, { managers, requests }).then((b) => new SaveGame(b));

test('requests: read, their club, the member written back byte for byte', async () => {
  const g = await withReqs([{ club: 100, player: 1, reports: 2 }]);
  const sm = scoutManager(g);
  assert.equal(sm.clubOf(KLAUS), 100);
  assert.equal(sm.clubOf(MIKE), 101);
  assert.equal(sm.requested(KLAUS, 1), true);
  assert.equal(sm.requested(MIKE, 1), false, 'another club asked, not Mike\'s');
  assert.deepEqual(Buffer.from(writeSearches(parseSearches(g.file.read('scout_manager.dat')))), Buffer.from(g.file.read('scout_manager.dat')));
});

test('request: the record as add_scout_request builds it, in order of player then club, the last-list bytes kept', async () => {
  const g = await withReqs([{ club: 100, player: 1 }]);
  const sm = scoutManager(g);
  assert.equal(sm.request(MIKE, 1), true);
  assert.equal(sm.request(KLAUS, 0), true);
  const r = reqs(g);
  assert.deepEqual(r.list, [[100, 0, 0], [100, 1, 0], [101, 1, 0]], 'player 0 first; for player 1, club 100 before 101');
  assert.deepEqual(readDate(r.at(0), 8), { year: 2001, month: 7, day: 14 }, 'the day asked: the game date');
  assert.deepEqual(r.last, [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 2]);
  assert.equal(sm.request(KLAUS, 0), false, 'asked already');
  assert.equal(g.file.member('scout_manager.dat').changed, true);
});

test('request: none for a player at the manager\'s own club, none for a manager without a club; the first in a save with none', async () => {
  const g = await withReqs([], [{ first: 'Klaus', second: 'Kastelmann', club: 0 }, { first: 'Mike', second: 'Penzin', club: null }]);
  const sm = scoutManager(g);
  assert.equal(sm.canRequest(KLAUS, 0), false, 'Del Piero plays for club 0, Klaus\'s');
  assert.equal(sm.request(KLAUS, 0), false);
  assert.equal(sm.canRequest(MIKE, 0), false, 'Mike has no club');
  assert.equal(sm.request(MIKE, 0), false);
  assert.equal(reqs(g).n, 0);
  const h = await withReqs([]);
  assert.equal(scoutManager(h).request(KLAUS, 1), true);
  assert.deepEqual(reqs(h).list, [[100, 1, 0]]);
});

test('unrequest: the record goes, the order kept; written and read again', async () => {
  const g = await withReqs([{ club: 100, player: 0 }, { club: 100, player: 1 }, { club: 101, player: 1 }]);
  const sm = scoutManager(g);
  assert.equal(sm.unrequest(KLAUS, 1), true);
  assert.equal(sm.unrequest(KLAUS, 1), false, 'not there any more');
  assert.deepEqual(reqs(g).list, [[100, 0, 0], [101, 1, 0]]);
  const again = new SaveGame(g.file.toBytes());
  assert.equal(scoutManager(again).requested(MIKE, 1), true);
  assert.equal(scoutManager(again).requested(KLAUS, 1), false);
});

test('a request taken back and asked again is the one it was: its day and reports kept', async () => {
  const g = await withReqs([{ club: 100, player: 1, reports: 1, date: { year: 2001, month: 6, day: 2 } }]);
  const before = Buffer.from(g.file.read('scout_manager.dat'));
  const sm = scoutManager(g);
  assert.equal(sm.unrequest(KLAUS, 1), true);
  assert.equal(sm.request(KLAUS, 1), true);
  assert.deepEqual(Buffer.from(g.file.read('scout_manager.dat')), before);
});

test('a shortlist add and a request in one session: both kept, written and read again', async () => {
  const g = await withReqs([]);
  const sm = scoutManager(g);
  sm.add(KLAUS, 0); sm.request(MIKE, 1); sm.add(MIKE, 1);
  const again = scoutManager(new SaveGame(g.file.toBytes()));
  assert.deepEqual([[...again.ids(KLAUS)], [...again.ids(MIKE)], again.requested(MIKE, 1), again.requested(KLAUS, 1)], [[0], [1], true, false]);
});

test('no request without the game date (it would look a year old and be dropped)', async () => {
  const g = await withReqs([]);
  g.file.member('general.dat').data = new Uint8Array(4000);              // the date at 0xf85 blank
  assert.equal(scoutManager(g).request(KLAUS, 1), false);
});
