// Tests of the save editor's model (savedit.js) on a small saved game in the game's format (test/fakes.mjs).
// Run: cd web && npm test
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ATTRIBUTES, SaveFile, SaveGame, decodeRuns, encodeRuns, rating, readDate, shown, storedFor, writeDate } from '../savedit.js';
import { fakeSaveGame } from './fakes.mjs';

test('the run code: the encoder of the game, and back', () => {
  assert.deepEqual([...encodeRuns(new Uint8Array([1, 2, 2, 2, 0x90, 5]))], [1, 0x83, 2, 0x81, 0x90, 5], 'a single byte of 0x81 or more is a run of one');
  assert.deepEqual([...encodeRuns(new Uint8Array(300).fill(9))], [0xff, 9, 0xff, 9, 0xae, 9], 'runs of 0x7f at most');
  const data = new Uint8Array(5000).map((_, i) => (i * 7919) % 251 > 200 ? 0 : i % 256);
  assert.deepEqual(decodeRuns(encodeRuns(data), data.length).data, data);
  assert.throws(() => decodeRuns(new Uint8Array([1, 2]), 5), /ends early/);
});

test('dates as Date.cpp keeps them: day of the year from 0, the leap flag', () => {
  const b = new Uint8Array(8);
  writeDate(b, 0, { year: 1980, month: 2, day: 29 });
  assert.deepEqual([b[0] | b[1] << 8, b[2] | b[3] << 8, b[4]], [59, 1980, 1]);
  assert.deepEqual(readDate(b, 0), { year: 1980, month: 2, day: 29 });
  writeDate(b, 0, { year: 2001, month: 12, day: 31 });
  assert.deepEqual([b[0] | b[1] << 8, b[4]], [364, 0]);
  writeDate(b, 0, { year: 1974, month: 1, day: 1 });
  assert.equal(b[0] | b[1] << 8, 0);
  b[2] = 0x6c; b[3] = 0x07;                                 // 1900: no date
  assert.equal(readDate(b, 0), null);
});

test('ratings of relative bytes as 0x544380 and 0x5442f0, and the byte for a rating', () => {
  assert.equal(rating(0, 100), 13);
  assert.equal(rating(127, 1), 20);
  assert.equal(rating(-128, 200), 4, 'the lowest that ability 200 can show');
  const by = (n) => ATTRIBUTES.find((a) => a.name === n);
  assert.equal(shown(by('Finishing'), 0x88, 180, 1), rating(-120, 180), 'an outfield player: the whole ability');
  assert.equal(shown(by('Finishing'), 0x88, 180, 20), rating(-120, 18), 'a goalkeeper (rating 20): a tenth of it');
  assert.equal(shown(by('Handling'), 0x88, 180, 1), rating(-120, 18), 'Handling of an outfield player (below 15): a tenth');
  assert.equal(shown(by('Handling'), 0x88, 180, 15), rating(-120, 180));
  assert.equal(shown(by('Pace'), 17, 180, 1), 17, 'a raw byte is the rating');
  for (const a of ATTRIBUTES.filter((x) => x.kind !== 'raw'))
    for (const ca of [1, 50, 120, 200]) for (const gk of [1, 20]) for (let want = 1; want <= 20; want++) {
      const r = storedFor(a, want, ca, gk);
      assert.equal(shown(a, r.byte, ca, gk), r.shows, `${a.name} ${want} at ${ca}`);
      if (r.shows !== want) assert.ok(Math.abs(r.shows - want) <= Math.abs(shown(a, ca > 100 ? 0x80 : 0x7f, ca, gk) - want) + 20);
    }
  const r = storedFor(by('Heading'), 3, 195, 1);
  assert.deepEqual([r.shows], [4], 'out of reach: the nearest that shows');
});

test('the saved game: players, search, read, write; only the changed tables change', async () => {
  const bytes = await fakeSaveGame();
  const g = new SaveGame(bytes);
  assert.deepEqual(g.players().map((p) => [p.name, p.club, p.nation, p.born, p.ca]), [['Alessandro Del Piero', 'Juventus', 'Italy', 1974, 181],
    ['Gianluigi Buffon', 'Juventus', 'Italy', 1978, 187]], 'the coach has no player data');
  assert.deepEqual(g.search('buffon').map((p) => p.name), ['Gianluigi Buffon']);
  assert.deepEqual(g.search('juv').map((p) => p.name), ['Gianluigi Buffon', 'Alessandro Del Piero'], 'by club, best first');
  assert.deepEqual(g.toBytes(), bytes, 'nothing changed: the same bytes');
  const row = g.search('del piero')[0].row;
  const f = g.read(row);
  assert.equal(f.attrs.Pace, 15);
  assert.equal(f.attrs.Determination, 16, 'from the staff row');
  assert.deepEqual(f.born, { year: 1974, month: 11, day: 9 });
  Object.assign(f.attrs, { Pace: 20, Finishing: 19, Determination: 20 });
  f.ca = 195; f.pa = 200; f.positions[6] = 20; f.positions[5] = 18;
  f.born = { year: 1982, month: 3, day: 1 };
  assert.deepEqual(g.write(row, f), {});
  const out = g.toBytes();
  const h = new SaveGame(out);
  const back = h.read(h.search('del piero')[0].row);
  assert.deepEqual([back.ca, back.pa, back.attrs.Pace, back.attrs.Finishing, back.attrs.Determination, back.positions[5], back.born.year],
    [195, 200, 20, 19, 20, 18, 1982]);
  assert.equal(back.attrs.Heading, f.attrs.Heading, 'an attribute left alone keeps the number it showed, with the new ability too');
  const before = new SaveFile(bytes), after = new SaveFile(out);
  const differ = before.members.map((m) => m.name).filter((n) => !Buffer.from(before.read(n)).equals(Buffer.from(after.read(n))));
  assert.deepEqual(differ, ['staff.dat', 'player.dat']);
  assert.equal(h.read(h.search('buffon')[0].row).attrs.Pace, 12, 'the other player is as he was');
});

test('a saved game keeps the age at staff +0x18: only a new year of birth moves it', async () => {
  const g = new SaveGame(await fakeSaveGame());
  const row = g.search('del piero')[0].row;
  const f = g.read(row);
  assert.equal(f.age, 26, 'born 9 November 1974, a game of 14 July 2001');
  f.attrs.Pace = 20;
  g.write(row, f);
  assert.equal(g.read(row).age, 26, 'a change of ratings leaves the age (it once got 1974: "Age -74" in the game)');
  g.write(row, { ...g.read(row), born: { year: 1980, month: 11, day: 9 } });
  assert.equal(g.read(row).age, 20, 'six years younger');
});

test('a file that is not a saved game, or not one this editor knows', async () => {
  assert.throws(() => new SaveGame(new Uint8Array(100)), /too short/);
  const b = await fakeSaveGame();
  b[0] = 9;
  assert.throws(() => new SaveGame(b), /not a saved game/);
});

// A Blob that counts the bytes read from it (each slice read with arrayBuffer); its slices count into the same counter.
class CountingBlob extends Blob {
  constructor(parts, counter) { super(parts); this.counter = counter; }
  slice(a, b) { return new CountingBlob([super.slice(a, b)], this.counter); }
  async arrayBuffer() { this.counter.read += this.size; return super.arrayBuffer(); }
}
function countingBlob(bytes) {
  const counter = { read: 0 };
  return { blob: new CountingBlob([bytes], counter), counter };
}

test('a saved game from a Blob: only the header and the edited tables are read; the edited save is a Blob of the parts', async () => {
  const bytes = await fakeSaveGame();
  // Padding after the tables, as a large save has megabytes of members the editor never touches.
  const big = new Uint8Array(bytes.length + (8 << 20));
  big.set(bytes);
  const { blob, counter } = countingBlob(big);
  const g = await SaveGame.open(blob);
  assert.deepEqual(g.players().map((p) => p.name), ['Alessandro Del Piero', 'Gianluigi Buffon']);
  const tables = ['index.dat', 'staff.dat', 'player.dat', 'first_names.dat', 'second_names.dat', 'common_names.dat', 'club.dat',
    'nat_club.dat', 'nation.dat'];
  const f0 = new SaveFile(big);
  const need = 0x416ec + f0.members.filter((m) => tables.includes(m.name)).reduce((a, m) => a + m.raw.length, 0);
  assert.ok(counter.read <= need, `read ${counter.read} bytes, the header and the tables are ${need}`);
  assert.deepEqual(new Uint8Array(await (await g.toBlob()).arrayBuffer()), big, 'nothing changed: the same bytes');
  const row = g.search('del piero')[0].row;
  const f = g.read(row);
  f.ca = 199;
  g.write(row, f);
  const out = new Uint8Array(await (await g.toBlob()).arrayBuffer());
  const h = new SaveGame(out);
  assert.equal(h.read(h.search('del piero')[0].row).ca, 199);
  assert.deepEqual(new SaveGame(bytes).search('buffon').map((p) => p.ca), h.search('buffon').map((p) => p.ca));
  // The bytes path gives the same file.
  const viaBytes = new SaveGame(big);
  viaBytes.write(viaBytes.search('del piero')[0].row, f);
  assert.deepEqual(out, viaBytes.toBytes());
  await assert.rejects(SaveGame.open(new Blob([new Uint8Array(100)])), /too short/);
});
