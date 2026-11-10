// Tests of the data set editor's model (dataset.js) on a small data set in the game's format (test/fakes.mjs).
// Run: cd web && npm test
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ATTRIBUTES, DataSet, DataSetError, nameBytes } from '../dataset.js';
import { dataSetNameProblem } from '../editor.js';
import { fakeDataSet } from './fakes.mjs';

const squad = (ds, club) => { const r = ds.row(0, club); return Array.from({ length: 50 }, (_, k) => new DataView(r.buffer, r.byteOffset).getInt32(0xd7 + 4 * k, true)); };
const reopen = (ds, files) => new DataSet({ ...files, ...ds.output() });
const fields = (over = {}) => ({ first: 'Marco', second: 'Testa', common: '', born: { year: 1984, month: 5, day: 12 }, nation: 0, nation2: -2, club: 0,
  contractEnd: { year: 2005, month: 6, day: 30 }, ca: 150, pa: 190, positions: [1, 1, 1, 1, 15, 20, 18, 1, 1, 1, 20, 1],
  attrs: Object.fromEntries(ATTRIBUTES.map((a) => [a.name, a.name === 'Pace' ? 19 : 0])), ...over });

test('the players of a data set: names, clubs, nations, the coach left out', () => {
  const ds = new DataSet(fakeDataSet());
  assert.deepEqual(ds.players().map((p) => [p.name, p.club, p.nation, p.born, p.ca]),
    [['Alessandro Del Piero', 'Juventus', 'Italy', 1974, 183], ['Gianluigi Buffon', 'Juventus', 'Italy', 1978, 187], ['Mario Rossi', '', 'Italy', 1980, 60]]);
  const f = ds.read(0);
  assert.deepEqual([f.first, f.second, f.common, f.club, f.ca, f.attrs.Pace, f.attrs.Determination, f.contractEnd], ['Alessandro', 'Del Piero', '', 0, 183, 12, 14, null]);
  assert.deepEqual(ds.clubs().map((c) => c.name), ['AC Milan', 'Juventus']);
});

test('edit: attributes as they are (0 random), names, a move to another club keeps the squad lists packed', () => {
  const files = fakeDataSet(), ds = new DataSet(files);
  const f = ds.read(0);
  Object.assign(f, { common: 'Pinturicchio', club: 1, ca: 195, contractEnd: { year: 2006, month: 6, day: 30 } });
  f.attrs.Finishing = 20; f.attrs.Pace = 0;
  ds.write(0, f);
  const g = reopen(ds, files);
  const back = g.read(0);
  assert.deepEqual([back.common, back.club, back.ca, back.attrs.Finishing, back.attrs.Pace, back.contractEnd], ['Pinturicchio', 1, 195, 20, 0, { year: 2006, month: 6, day: 30 }]);
  assert.deepEqual(squad(g, 0).slice(0, 2), [1, -1], 'out of Juventus, the list closes up');
  assert.deepEqual(squad(g, 1).slice(0, 2), [0, -1], 'into Milan');
  assert.equal(g.players()[0].name, 'Pinturicchio');
  assert.equal(g.count(15), 2, 'a new common name');
});

test('add: rows at the ends of their tables, the tables after them move, the club lists the player', () => {
  const files = fakeDataSet(), ds = new DataSet(files);
  const row = ds.add(fields({ first: 'Mario', second: 'Testa' }));
  assert.equal(row, 4);
  const out = ds.output();
  assert.deepEqual(Object.keys(out).sort(), ['club.dat', 'first_names.dat', 'index.dat', 'second_names.dat', 'staff.dat'],
    'first names: the count of Mario goes up');
  assert.equal(out['staff.dat'].length, files['staff.dat'].length + 157 + 70);
  const g = new DataSet({ ...files, ...out });
  assert.deepEqual([g.entry(6).count, g.entry(9).offset, g.entry(10).offset, g.entry(10).count], [5, 5 * 157, 5 * 157 + 68, 4]);
  const p = g.players().find((x) => x.row === 4);
  assert.deepEqual([p.name, p.club, p.data, p.ca], ['Mario Testa', 'Juventus', 3, 150]);
  assert.deepEqual(squad(g, 0).slice(0, 3), [0, 1, 4]);
  assert.equal(g.read(1).attrs.Pace, 12, 'the rows that were there read as before');
  const s = g.row(6, 4), dv = new DataView(s.buffer, s.byteOffset);
  assert.deepEqual([dv.getInt32(0, true), s[0x3d], s[0x5f], dv.getInt32(0x91, true), dv.getInt32(0x95, true), dv.getInt32(0x99, true), dv.getInt32(0x61, true), dv.getInt32(0x1e, true)],
    [4, 11, 2, 3, -1, -1, -1, -2], 'id, player job, player flags, links, no preferences, no second nation');
  assert.equal(g.read(4).first, 'Mario', 'an existing first name is shared');
  assert.equal(g.count(13), 5, 'so no new first name');
  assert.equal(g.row(13, 4)[0x3b], 2, 'its count is two now');
  assert.equal(g.count(14), 6, 'a new surname');
});

test('refusals: a full club, a name the game cannot show, no name; nothing is left behind', () => {
  const files = fakeDataSet(), ds = new DataSet(files);
  for (let k = 0; k < 50; k++) ds.add(fields({ second: `Player${k}`, club: 1 }));
  const n = ds.count(6);
  assert.throws(() => ds.add(fields({ second: 'One too many', club: 1 })), /has 50 players already/);
  assert.equal(ds.count(6), n, 'the refused player was not added');
  assert.equal(ds.players().filter((p) => p.club === 'AC Milan').length, 50);
  assert.throws(() => ds.add(fields({ second: 'Kowalczyk ł' })), /cannot show/);
  assert.throws(() => ds.add(fields({ first: '', second: '' })), /needs a first name/);
  assert.throws(() => nameBytes('x'.repeat(51)), /longer than 50/);
  assert.ok(ds.add(fields({ first: '', second: '', common: 'Pelé' })) > 0, 'a common name alone will do; é is Latin-1');
  assert.throws(() => new DataSet({ ...files, 'staff.dat': new Uint8Array(10) }), DataSetError);
});

test('data set names for the Databases folder', () => {
  const taken = new Set(['my data set']);
  assert.equal(dataSetNameProblem('Serie A 2026 edit', taken), null);
  assert.match(dataSetNameProblem('My Data Set', taken), /exists/);
  assert.match(dataSetNameProblem('May 2026', taken), /season/);
  assert.match(dataSetNameProblem('a/b', taken), /cannot have/);
  assert.match(dataSetNameProblem('  ', taken), /Give/);
});
