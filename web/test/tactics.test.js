// The tactic files of Saved games (web/tactics.js): which files are tactics, the plain version the game's Load needs,
// and the names uploads get.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readyTactic, tacticName } from '../tactics.js';

// A tactic as the game saves it: 1,476 bytes, version 0x98EC5E little-endian, a tail that is not the big-endian mark.
const tactic = ({ version = 0x98ec5e, size = 1476, big = false } = {}) => {
  const b = new Uint8Array(size), dv = new DataView(b.buffer);
  dv.setUint32(0, version, !big);
  dv.setUint32(size - 4, big ? 0xf1fffff1 : 0xf2fffff2, true);
  return b;
};

test('a tactic the game saved is taken as it is', () => {
  const b = tactic();
  const t = readyTactic(b);
  assert.equal(t.why, undefined);
  assert.equal(t.converted, undefined);
  assert.equal(t.bytes, b);
  for (const v of [0x98ec54, 0x98ec5a, 0x98ec5c, 0x98ec5e]) assert.equal(readyTactic(tactic({ version: v })).why, undefined);
});

test('a preset (version plus 0x075BCD15) gets the plain version, and nothing else changes', () => {
  const b = tactic({ version: 0x07f4b971 });              // 442_diamond_default.pct
  b[0x36] = 0x4d;                                           // a byte past the version stays
  const t = readyTactic(b);
  assert.equal(t.converted, true);
  assert.equal(new DataView(t.bytes.buffer).getUint32(0, true), 0x98ec5c);
  assert.equal(t.bytes[0x36], 0x4d);
  assert.equal(t.bytes.length, b.length);
  assert.equal(new DataView(b.buffer).getUint32(0, true), 0x07f4b971, 'the upload\'s own bytes are not changed');
});

test('a big-endian tactic (last word 0xF1FFFFF1) is read and converted in its own byte order', () => {
  assert.equal(readyTactic(tactic({ big: true })).why, undefined);
  const t = readyTactic(tactic({ version: 0x07f4b973, big: true }));
  assert.equal(t.converted, true);
  assert.equal(new DataView(t.bytes.buffer).getUint32(0, false), 0x98ec5e);
});

test('other files are refused with a reason', () => {
  assert.match(readyTactic(new Uint8Array(100)).why, /too short/);
  assert.match(readyTactic(new Uint8Array(0x20000)).why, /too large/);
  assert.match(readyTactic(tactic({ version: 0x12345678 })).why, /not a tactic/);
  const save = new Uint8Array(4096);
  new DataView(save.buffer).setInt32(0, 2, true);            // a saved game's container starts with a small version
  assert.match(readyTactic(save).why, /not a tactic/);
});

test('names: .tct kept, a preset\'s .pct becomes .tct, others refused, taken names numbered', () => {
  const none = new Set();
  assert.equal(tacticName('My 4-4-2.tct', none), 'My 4-4-2.tct');
  assert.equal(tacticName('C:\\Games\\CM\\tactics\\Attack.TCT', none), 'Attack.TCT');
  assert.equal(tacticName('442_diamond_default.pct', none), '442_diamond_default.tct');
  assert.equal(tacticName('notes.txt', none), null);
  assert.equal(tacticName('.tct', none), null);
  assert.equal(tacticName('a:b?.tct', none), 'a_b_.tct');
  assert.equal(tacticName('Attack.tct', new Set(['attack.tct', 'attack (2).tct'])), 'Attack (3).tct');
});
