// The Scout (web/README.md, "The Scout"; docs/superpowers/specs/2026-11-22-scout-and-membership-design.md): a saved
// game's players by age, position, potential (PA), ability (CA) and attributes, the best first. It uses no DOM and
// no i18n.js (the Editor shows what it finds). The values are what the Editor shows: savedit.js's read(), whose
// attributes are the game's own ratings (0x543540, cited in savedit.js), and the age at staff +0x18, which the game
// rewrites each day (0x53ab90, called at 0x5cc850; docs/research/2026-10-17-season-date-shifts.md).
import { ATTRIBUTES } from './savedit.js';

const NAMES = ATTRIBUTES.map((a) => a.name);
const A = NAMES.length;
export const SCOUT_LIMIT = 100;                 // the rows a search gives at most
export const FREE_ROWS = 3;                     // the rows a free player sees in full
const RATED = 15;                               // a position counts from this rating (as the Editor's position text)

// One player's numbers into the index at i.
function fill(index, i, f) {
  index.age[i] = f.age; index.ca[i] = f.ca; index.pa[i] = f.pa;
  index.pos.set(f.positions.slice(0, 12), i * 12);
  for (let k = 0; k < A; k++) index.values[i * A + k] = f.attrs[NAMES[k]] ?? 0;
}

// Every player of a saved game, read once: what a search filters on, in typed arrays. at: staff row -> place.
export function scoutIndex(game) {
  const list = [...game.players()], n = list.length;
  const index = { list, at: new Map(), age: new Int16Array(n), ca: new Int16Array(n), pa: new Int16Array(n),
    pos: new Uint8Array(n * 12), values: new Uint8Array(n * A) };
  list.forEach((p, i) => { index.at.set(p.row, i); fill(index, i, game.read(p.row)); });
  return index;
}

// A player changed in the Editor (Apply): only his place in the index read again, not the whole save.
export function scoutUpdate(index, game, row) {
  const i = index.at.get(row);
  if (i === undefined) return;
  index.list[i] = game.players().find((p) => p.row === row) ?? index.list[i];
  fill(index, i, game.read(row));
}

const set = (v) => v !== null && v !== undefined && v !== '' && Number.isFinite(Number(v));

// The players that match: { rows (at most SCOUT_LIMIT, sorted), total }. filters: { ageMin, ageMax, position (0..11
// into savedit's POSITIONS), paMin, caMin, attrs: [{ name, min }] }; a blank one leaves that out.
export function scout(index, filters = {}) {
  const { list, age, ca, pa, pos, values } = index;
  const ageMin = set(filters.ageMin) ? Number(filters.ageMin) : null, ageMax = set(filters.ageMax) ? Number(filters.ageMax) : null;
  const paMin = set(filters.paMin) ? Number(filters.paMin) : null, caMin = set(filters.caMin) ? Number(filters.caMin) : null;
  const position = set(filters.position) ? Number(filters.position) : null;
  const attrs = (filters.attrs ?? []).filter((a) => a && NAMES.includes(a.name)).slice(0, 3)
    .map((a) => ({ name: a.name, k: NAMES.indexOf(a.name), min: set(a.min) ? Number(a.min) : null }));
  const hit = [];
  for (let i = 0; i < list.length; i++) {
    if (ageMin !== null && age[i] < ageMin) continue;
    if (ageMax !== null && age[i] > ageMax) continue;
    if (paMin !== null && pa[i] < paMin) continue;
    if (caMin !== null && ca[i] < caMin) continue;
    if (position !== null && pos[i * 12 + position] < RATED) continue;
    if (attrs.some((a) => a.min !== null && values[i * A + a.k] < a.min)) continue;
    hit.push(i);
  }
  hit.sort((x, y) => pa[y] - pa[x] || ca[y] - ca[x] || list[x].name.localeCompare(list[y].name));
  const rows = hit.slice(0, SCOUT_LIMIT).map((i) => ({ row: list[i].row, name: list[i].name, club: list[i].club ?? '', pos: list[i].pos,
    age: age[i], ca: ca[i], pa: pa[i], values: Object.fromEntries(attrs.map((a) => [a.name, values[i * A + a.k]])) }));
  return { rows, total: hit.length };
}

// The Wonderkids preset (John Locke's "School search" patch of CM 01/02): players aged 19 or younger with potential
// ability 180 or more. presetFilters(name): the Scout's filters for a preset, the others cleared; null for no such preset.
export const WONDERKIDS = Object.freeze({ ageMax: 19, paMin: 180 });
export function presetFilters(name) {
  if (name !== 'wonderkids') return null;
  return { ageMin: '', ageMax: String(WONDERKIDS.ageMax), position: '', paMin: String(WONDERKIDS.paMin), caMin: '',
    attrs: [0, 1, 2].map(() => ({ name: '', min: '' })) };
}

// Whether anything can be bought here: an offer free with an account, or one sold (Buy Me a Coffee, Paddle, a link).
const buyable = (offer) => Boolean(offer && (offer.free || offer.coffee || offer.paddle || offer.url));

// Who sees the Scout whole: a member (me.user.cloud), and everyone where no one could pay (no accounts, or nothing to
// buy). me null (offline, not answered yet): the last answer kept (keptAccess), else free.
export function scoutAccess(me, kept) {
  if (!me) return kept === '1' ? 'full' : 'free';
  if (!me.auth) return 'full';
  if (me.user?.cloud) return 'full';
  return buyable(me.offer) ? 'free' : 'full';
}

// What to keep in localStorage (cm0102-member) from an answer of /api/me: '1' whole, '0' not, null nothing known.
export const keptAccess = (me) => (me ? (scoutAccess(me, null) === 'full' ? '1' : '0') : null);

// The rows a player sees: all of them for 'full', the first FREE_ROWS for 'free'; held: the matches not shown.
export function freeCut(result, access) {
  const rows = access === 'full' ? result.rows : result.rows.slice(0, FREE_ROWS);
  return { rows, held: result.total - rows.length };
}

// The invitation's button, the Account dialog's own for the account as it is: 'signin' (signed out), 'member'
// (signed in, not a member: Become a member), 'free' (free with an account: Sign in with Google, free), 'none' (the
// account not known, offline at a first visit: the Account dialog would be empty, so no button).
export function inviteKind(me) {
  if (!me) return 'none';
  if (me.offer?.free && !me.user) return 'free';
  return me.user ? 'member' : 'signin';
}
