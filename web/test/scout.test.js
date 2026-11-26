// The Scout's model (scout.js; docs/superpowers/specs/2026-11-22-scout-and-membership-design.md): a saved game's
// players by age, position, potential, ability and attributes; who sees them all; the free cut.
import test from 'node:test';
import assert from 'node:assert/strict';
import { SaveGame } from '../savedit.js';
import { scoutIndex, scoutUpdate, scout, scoutAccess, keptAccess, freeCut, inviteKind, WONDERKIDS, presetFilters } from '../scout.js';
import { fakeSaveGame } from './fakes.mjs';

// A player of the fake save: born on 1 January of the year that gives this age on the save's date (14 July 2001).
const P = (first, age, ca, pa, over = {}) => ({ first, second: 'Test', ca, pa, gk: 1, born: [2001 - age, 1, 1], club: 0, fill: 12, ...over });
const open = async (players) => new SaveGame(await fakeSaveGame(players));
const NONE = { ageMin: null, ageMax: null, position: null, paMin: null, caMin: null, attrs: [] };
const names = (r) => r.rows.map((x) => x.name);

test('scout: age, position, PA, CA and attributes filter; sorted by PA, then CA, then name; at most 100', async () => {
  const g = await open([P('Old', 30, 150, 120), P('Mid', 25, 140, 150), P('Ann', 21, 120, 170), P('Bob', 19, 130, 170),
    P('Kid', 18, 100, 190, { gk: 20 })]);
  const ix = scoutIndex(g);
  assert.deepEqual(names(scout(ix, NONE)), ['Kid Test', 'Bob Test', 'Ann Test', 'Mid Test', 'Old Test'], 'PA, then CA');
  assert.deepEqual(names(scout(ix, { ...NONE, ageMax: 21 })), ['Kid Test', 'Bob Test', 'Ann Test']);
  assert.deepEqual(names(scout(ix, { ...NONE, ageMin: 19, ageMax: 25 })), ['Bob Test', 'Ann Test', 'Mid Test']);
  assert.deepEqual(names(scout(ix, { ...NONE, position: 0 })), ['Kid Test'], 'the goalkeeper (rated 20 there)');
  assert.deepEqual(names(scout(ix, { ...NONE, position: 6 })), ['Bob Test', 'Ann Test', 'Mid Test', 'Old Test'], 'the attackers');
  assert.deepEqual(names(scout(ix, { ...NONE, paMin: 170, caMin: 125 })), ['Bob Test']);
  const row = g.players().find((x) => x.name === 'Bob Test').row;
  const fin = g.read(row).attrs.Finishing;
  const r = scout(ix, { ...NONE, attrs: [{ name: 'Finishing', min: fin }] });
  assert.ok(r.rows.every((x) => x.values.Finishing >= fin) && r.rows.some((x) => x.name === 'Bob Test'));
  assert.deepEqual(Object.keys(r.rows[0].values), ['Finishing'], 'only the filtered attributes');
  assert.deepEqual(names(scout(ix, { ...NONE, attrs: [{ name: 'Finishing', min: 21 }] })), []);
  const age = scout(ix, NONE).rows.find((x) => x.name === 'Bob Test');
  assert.deepEqual([age.age, age.ca, age.pa, age.club], [19, 130, 170, 'Juventus']);

  const many = await open(Array.from({ length: 150 }, (_, i) => P(`P${String(i).padStart(3, '0')}`, 20, 100, 150)));
  const all = scout(scoutIndex(many), NONE);
  assert.deepEqual([all.rows.length, all.total], [100, 150]);
  assert.equal(all.rows[0].name, 'P000 Test', 'a tie: by name');
});

test('scout: odd players do not break it', async () => {
  const g = await open([P('Neg', 20, 100, -1), P('Free', 22, 110, 140, { club: -1 }), P('Zero', 0, 90, 130)]);
  const ix = scoutIndex(g);
  assert.deepEqual(names(scout(ix, { ...NONE, paMin: 100 })), ['Free Test', 'Zero Test'], 'PA -1 is left out by a minimum');
  assert.ok(names(scout(ix, NONE)).includes('Neg Test'), 'and kept with no PA filter');
  assert.equal(scout(ix, NONE).rows.find((x) => x.name === 'Free Test').club, '');
  assert.deepEqual(names(scout(ix, { ...NONE, ageMin: 16 })), ['Free Test', 'Neg Test'], 'age 0 is left out by a minimum age');
});

test('scoutAccess: member, no accounts, nothing to buy, free, and the kept answer', () => {
  assert.equal(scoutAccess({ auth: true, user: { cloud: true } }, null), 'full');
  assert.equal(scoutAccess({ auth: false }, null), 'full', 'a server with no accounts: no one could pay');
  assert.equal(scoutAccess({ auth: true, user: null, offer: { price: 'x', free: false } }, null), 'full', 'nothing to buy');
  assert.equal(scoutAccess({ auth: true, user: null, offer: { price: '$5 a month', coffee: true, url: 'https://example.com' } }, '1'), 'free');
  assert.equal(scoutAccess({ auth: true, user: { cloud: false }, offer: { paddle: { env: 'sandbox' } } }, '1'), 'free', 'an answer beats the kept one');
  assert.equal(scoutAccess({ auth: true, user: null, offer: { free: true } }, null), 'free', 'free with an account: sign in');
  assert.equal(scoutAccess(null, '1'), 'full', 'offline: the kept answer');
  assert.equal(scoutAccess(null, '0'), 'free');
  assert.equal(scoutAccess(null, null), 'free');
  assert.equal(keptAccess({ auth: true, user: { cloud: true } }), '1');
  assert.equal(keptAccess({ auth: true, user: null, offer: { coffee: true } }), '0');
  assert.equal(keptAccess(null), null);
});

test('freeCut: 3 for free, all for full, held counted', () => {
  const r = (n) => ({ rows: Array.from({ length: Math.min(n, 100) }, (_, i) => ({ row: i })), total: n });
  assert.deepEqual([freeCut(r(47), 'free').rows.length, freeCut(r(47), 'free').held], [3, 44]);
  assert.deepEqual([freeCut(r(47), 'full').rows.length, freeCut(r(47), 'full').held], [47, 0]);
  assert.deepEqual([freeCut(r(2), 'free').rows.length, freeCut(r(2), 'free').held], [2, 0]);
  assert.deepEqual([freeCut(r(150), 'full').rows.length, freeCut(r(150), 'full').held], [100, 50], 'over 100: the rest held');
});

test('scout is fast: a save of 60,000 players indexed and searched within budget', async () => {
  const g = await open(Array.from({ length: 60000 }, (_, i) => P(`N${i}`, 16 + (i % 20), 50 + (i % 150), 80 + (i % 120), { fill: 5 + (i % 15) })));
  let t = performance.now();
  const ix = scoutIndex(g);
  const built = performance.now() - t;
  t = performance.now();
  const r = scout(ix, { ...NONE, ageMax: 21, paMin: 150, attrs: [{ name: 'Pace', min: 12 }, { name: 'Finishing', min: 10 }, { name: 'Teamwork', min: 8 }] });
  const searched = performance.now() - t;
  assert.ok(r.total > 0);
  assert.ok(built < 1500 && searched < 150, `index ${Math.round(built)} ms, search ${Math.round(searched)} ms`);
  console.log(`# scout: 60,000 players indexed in ${Math.round(built)} ms, searched in ${Math.round(searched)} ms`);
});

test('inviteKind: the Account dialog\'s own button for the account as it is', () => {
  assert.equal(inviteKind({ auth: true, user: null, offer: { coffee: true } }), 'signin', 'signed out: Sign in');
  assert.equal(inviteKind({ auth: true, user: { cloud: false }, offer: { coffee: true } }), 'member', 'signed in: Become a member');
  assert.equal(inviteKind({ auth: true, user: null, offer: { free: true } }), 'free', 'free with an account: Sign in with Google, free');
  assert.equal(inviteKind(null), 'none', 'not known (offline at a first visit): no button, the dialog would be empty');
});

test('scoutUpdate: an edited player is read again into the index, the same as a new index', async () => {
  const g = await open([P('Ann', 21, 120, 170), P('Bob', 19, 130, 160)]);
  const ix = scoutIndex(g);
  const row = g.players().find((x) => x.name === 'Bob Test').row;
  const f = g.read(row);
  f.pa = 195; f.attrs.Pace = 20;
  g.write(row, f);
  scoutUpdate(ix, g, row);
  const fresh = scoutIndex(g);
  const q = { ...NONE, attrs: [{ name: 'Pace', min: 1 }] };
  assert.deepEqual(scout(ix, q), scout(fresh, q));
  assert.equal(scout(ix, NONE).rows[0].name, 'Bob Test', 'his new potential puts him first');
});

test('the page\'s words: the Scout promises no more than the best 100, and Membership is not "a membership"', async () => {
  const { pageStrings } = await import('./i18n-strings.mjs');
  const texts = [...pageStrings().keys()];
  assert.deepEqual(texts.filter((s) => /Scout/.test(s) && /every (player|match)/.test(s)), [], 'the Scout shows the best 100, not every player');
  assert.deepEqual(texts.filter((s) => /Membership is an? (monthly )?(Buy Me a Coffee )?membership/i.test(s)), [], 'no sentence that says Membership is a membership');
});

test('the Wonderkids preset: 19 or younger with potential 180 or more (John Locke\'s School search), every other filter cleared', async () => {
  assert.deepEqual(WONDERKIDS, { ageMax: 19, paMin: 180 });
  const f = presetFilters('wonderkids');
  assert.deepEqual(f, { ageMin: '', ageMax: '19', position: '', nation: '', paMin: '180', caMin: '', attrs: [{ name: '', min: '' }, { name: '', min: '' }, { name: '', min: '' }] });
  assert.equal(presetFilters('nonsense'), null);
  const g = await open([P('Kid', 19, 100, 180), P('Old', 20, 120, 199), P('Low', 18, 110, 179), P('Top', 17, 90, 195)]);
  assert.deepEqual(names(scout(scoutIndex(g), { ...f, attrs: [] })), ['Top Test', 'Kid Test'], '19 and 180 count; 20, and 179, do not');
});

test('nationality: the Scout finds the players of one nation only', async () => {
  const g = new SaveGame(await fakeSaveGame([
    { first: 'Alessandro', second: 'Del Piero', ca: 181, pa: 190, gk: 1, born: [1974, 11, 9], club: 0, fill: 15 },
    { first: 'Thierry', second: 'Henry', ca: 185, pa: 192, gk: 1, born: [1977, 8, 17], club: 0, fill: 15, nation: 1 },
  ]));
  const index = scoutIndex(g);
  assert.deepEqual(scout(index, { nation: 'France' }).rows.map((r) => r.name), ['Thierry Henry']);
  assert.equal(scout(index, { nation: '' }).total, 2);
  assert.equal(scout(index, { nation: 'Brazil' }).total, 0);
});
