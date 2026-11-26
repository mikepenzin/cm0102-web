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

test('the saved game\'s search: every name of a player, accents aside, though the list shows his common name', async () => {
  const g = new SaveGame(await fakeSaveGame([
    { first: 'Paulo', second: 'da Silva Mendonça', common: 'Mendonça', ca: 120, pa: 150, gk: 1, born: [1980, 3, 3], club: 0, fill: 10 },
    { first: 'Marcelo', second: 'Mendonça de Mattos', common: 'Marcelo Mattos', ca: 187, pa: 194, gk: 1, born: [1984, 2, 10], club: 0, fill: 12 },
  ]));
  const names = (text) => g.search(text).map((p) => p.name);
  assert.deepEqual(names('mendonca'), ['Mendonça', 'Marcelo Mattos'], 'the name as the list shows it first, then the better player');
  assert.deepEqual(names('Paulo Mendonca'), ['Mendonça'], 'his first name and his common name');
  assert.deepEqual(names('paulo silva'), ['Mendonça'], 'his first and second names');
});

test('nationality: the nations of the players, and a search of one nation (with or without a name)', async () => {
  const g = new SaveGame(await fakeSaveGame([
    { first: 'Alessandro', second: 'Del Piero', ca: 181, pa: 190, gk: 1, born: [1974, 11, 9], club: 0, fill: 15 },
    { first: 'Thierry', second: 'Henry', ca: 185, pa: 192, gk: 1, born: [1977, 8, 17], club: 0, fill: 15, nation: 1 },
  ]));
  assert.deepEqual(g.nationNames(), ['France', 'Italy']);
  assert.deepEqual(g.search('', 60, 'France').map((p) => p.name), ['Thierry Henry']);
  assert.deepEqual(g.search('del', 60, 'France'), []);
  assert.equal(g.search('', 60).length, 2, 'no nation: every player');
});

test('manager attributes: the twelve the game shows, from the staff row and the non-player record (0x543fd0)', async () => {
  const { managerAttributes, MANAGER_ATTRIBUTES } = await import('../savedit.js');
  // Klaus: ability 200, Coaching Outfield Players byte -11 (18), Discipline 5, Youngsters 7, Adaptability 13, Determination 8.
  const g = new SaveGame(await fakeSaveGame(undefined, { managers: [
    { first: 'Klaus', second: 'Kastelmann', adaptability: 13, determination: 8, np: { 0x10: -11, 0x14: 5, 0x22: 7, 0x21: 20, 0x17: -2 } },
    { first: 'Mike', second: 'Penzin', ca: 100, np: { 0x1b: 0 } }] }));
  assert.equal(MANAGER_ATTRIBUTES.length, 12);
  const k = managerAttributes(g, 3), by = Object.fromEntries(k.attrs.map((a) => [a.name, a.value]));
  assert.deepEqual([k.ca, k.pa], [200, 200]);
  assert.deepEqual([by.Adaptability, by.Determination, by['Coaching Outfield Players'], by['Level of Discipline'], by['Working With Youngsters']], [13, 8, 18, 5, 7]);
  assert.equal(by['Tactical Knowledge'], 20, 'a relative byte at ability 200');
  assert.equal(managerAttributes(g, 4).attrs.find((a) => a.name === 'Motivating').value, rating(0, 100), 'ability 100: the relative formula');
  assert.equal(managerAttributes(g, 0), null, 'a player has no non-player record');
});

test('club finances: the balance, the wage bill and the year blocks of finance.dat (0x59b3b0, 359 bytes a club)', async () => {
  const { clubFinances } = await import('../savedit.js');
  const g = new SaveGame(await fakeSaveGame(undefined, { finance: { balance: 59516431, wageBill: 889495, topWage: 65497,
    thisYear: { 0x00: 3000000, 0x08: 19491130, 0x20: 9000000, 0x38: 250000, 0x48: 5985625, 0x10: 120000, 0x18: 5000, 0x28: 40000, 0x2c: 60000 },
    lastYear: { 0x48: 4000000 } } }));
  const f = clubFinances(g, 0);
  assert.deepEqual([f.balance, f.wageBill, f.topWage], [59516431, 889495, 65497]);
  assert.deepEqual(f.thisYear, { bought: 3000000, sold: 19491130, otherCosts: 120000, fines: 5000, wages: 9000000, bonuses: 40000,
    signOn: 60000, prize: 250000, tv: 5985625 });
  assert.equal(f.lastYear.tv, 4000000);
  assert.equal(clubFinances(g, 7), null, 'no such club');
  assert.equal(clubFinances(new SaveGame(await fakeSaveGame()), 0), null, 'no finance.dat');
});

test('a club: nation, attendance, training and reputation of club.dat; balance, reputation and training written back', async () => {
  const { clubInfo, writeClub, clubFinances } = await import('../savedit.js');
  const g = new SaveGame(await fakeSaveGame(undefined, { finance: { balance: 1000000 } }));
  assert.deepEqual(clubInfo(g, 0), { id: 0, name: 'Juventus', nation: 'Italy', attendance: 41000, training: 18, reputation: 9200 });
  assert.equal(clubInfo(g, 1).name, 'Italy', 'a national team: nat_club.dat');
  assert.equal(clubInfo(g, 7), null, 'no such club');
  writeClub(g, 0, { balance: 25000000, reputation: 12000, training: 0 });
  const back = new SaveGame(await g.toBytes());
  assert.equal(clubFinances(back, 0).balance, 25000000, 'the balance in finance.dat');
  assert.deepEqual([clubInfo(back, 0).reputation, clubInfo(back, 0).training], [10000, 1], 'cut to the game\'s ranges: 1..10000, 1..20');
  writeClub(g, 0, { balance: -5e9 });
  assert.equal(clubFinances(g, 0).balance, -2000000000, 'the balance within a signed 32-bit cash figure');
});

test('non-players: the staff with a non-player record, by name and club; edits written as the game rates them', async () => {
  const { nonPlayers, managerAttributes, writeManagerAttributes } = await import('../savedit.js');
  const g = new SaveGame(await fakeSaveGame(undefined, { managers: [
    { first: 'Klaus', second: 'Kastelmann', adaptability: 13, determination: 8, np: { 0x10: -11, 0x14: 5 } },
    { first: 'Mike', second: 'Penzin', ca: 150 }] }));
  assert.deepEqual(nonPlayers(g).map((p) => [p.id, p.name]), [[3, 'Klaus Kastelmann'], [4, 'Mike Penzin']]);
  const before = managerAttributes(g, 3);
  const values = Object.fromEntries(before.attrs.map((a) => [a.name, a.value]));
  writeManagerAttributes(g, 3, { ca: 120, pa: 150, values: { ...values, Motivating: 7, Adaptability: 19, 'Level of Discipline': 16 } });
  const after = managerAttributes(g, 3), by = Object.fromEntries(after.attrs.map((a) => [a.name, a.value]));
  assert.deepEqual([after.ca, after.pa], [120, 150]);
  assert.deepEqual([by.Motivating, by.Adaptability, by['Level of Discipline']], [7, 19, 16], 'the new values');
  assert.equal(by['Coaching Outfield Players'], values['Coaching Outfield Players'], 'a relative one keeps its number at the new ability');
  assert.equal(by.Determination, 8);
  assert.equal(g.file.member('nonplayer.dat').changed && g.file.member('staff.dat').changed, true);
  const again = new SaveGame(g.file.toBytes());
  await again.file.load?.(['nonplayer.dat']);
  assert.equal(managerAttributes(again, 3).attrs.find((a) => a.name === 'Motivating').value, 7, 'saved and read again');
});
