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
