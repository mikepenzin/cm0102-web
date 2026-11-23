// Tests of levels.mjs: the site's four levels, and how Buy Me a Coffee's level names map onto them (CM_LEVELS).
// Run: cd web && npm test
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { LEVELS, atLeast, higher, levelsFromEnv, paidLevel, rank } from '../levels.mjs';

test('rank, atLeast, higher', () => {
  assert.deepEqual(LEVELS, ['free', 'supporter', 'squad', 'legend']);
  assert.equal(atLeast('legend', 'squad'), true);
  assert.equal(atLeast('squad', 'squad'), true);
  assert.equal(atLeast('supporter', 'squad'), false);
  assert.equal(higher('squad', 'legend'), 'legend');
  assert.equal(higher('legend', 'free'), 'legend');
  assert.equal(rank('nonsense'), 0);
});

test('levelsFromEnv: CM_LEVELS, the default, and BMC_CLOUD_LEVELS alone', () => {
  assert.deepEqual(levelsFromEnv({}), { names: ['supporter', 'squad player', 'club legend'], legacyCloud: null });
  assert.deepEqual(levelsFromEnv({ CM_LEVELS: ' Fan , Pro ,VIP,' }), { names: ['fan', 'pro', 'vip'], legacyCloud: null });
  assert.deepEqual(levelsFromEnv({ BMC_CLOUD_LEVELS: 'Squad Player,Club Legend' }), { names: null, legacyCloud: ['squad player', 'club legend'] });
  assert.deepEqual(levelsFromEnv({ CM_LEVELS: 'a,b,c', BMC_CLOUD_LEVELS: 'b' }), { names: ['a', 'b', 'c'], legacyCloud: null });
});

test('paidLevel: by name or id, lowest to highest; an unknown name is a Supporter', () => {
  const cfg = levelsFromEnv({});
  assert.deepEqual(paidLevel({ level: 'Supporter' }, cfg), { level: 'supporter', known: true });
  assert.deepEqual(paidLevel({ level: 'squad PLAYER' }, cfg), { level: 'squad', known: true });
  assert.deepEqual(paidLevel({ level: ' Club Legend ' }, cfg), { level: 'legend', known: true });
  assert.deepEqual(paidLevel({ level: 'Gold', levelId: 9 }, cfg), { level: 'supporter', known: false });
  assert.deepEqual(paidLevel({ level: 'x', levelId: 2 }, levelsFromEnv({ CM_LEVELS: '1,2,3' })), { level: 'squad', known: true });
  assert.deepEqual(paidLevel({ level: 'x', levelId: 7 }, levelsFromEnv({ CM_LEVELS: '1,2,3,7' })), { level: 'legend', known: true });
  const legacy = levelsFromEnv({ BMC_CLOUD_LEVELS: 'Squad Player' });
  assert.deepEqual(paidLevel({ level: 'Club Legend' }, legacy), { level: 'supporter', known: true });
  assert.deepEqual(paidLevel({ level: 'Squad Player' }, legacy), { level: 'squad', known: true });
  assert.deepEqual(paidLevel({ level: 'Anything' }, { names: null, legacyCloud: [] }), { level: 'squad', known: true });
});

test('a level name as Buy Me a Coffee sends it, with an emoji: the letters count, not the symbols', () => {
  const cfg = levelsFromEnv({});
  assert.deepEqual(paidLevel({ level: '🏆 Club Legend', levelId: 352068 }, cfg), { level: 'legend', known: true });
  assert.deepEqual(paidLevel({ level: '⚽ Squad Player' }, cfg), { level: 'squad', known: true });
  assert.deepEqual(paidLevel({ level: '☕️ Supporter!' }, cfg), { level: 'supporter', known: true });
  assert.deepEqual(paidLevel({ level: 'Squad Player' }, levelsFromEnv({ CM_LEVELS: '☕ Supporter, ⚽ Squad Player, 🏆 Club Legend' })), { level: 'squad', known: true });
  assert.deepEqual(paidLevel({ level: '🏆 Club Legend' }, levelsFromEnv({ BMC_CLOUD_LEVELS: 'Squad Player,Club Legend' })), { level: 'squad', known: true }, 'the older setting too');
});
