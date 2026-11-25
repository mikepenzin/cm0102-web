// A human manager's Short List in a saved game (docs/research/2026-11-25-shortlist-in-save.md): the member
// scout_manager.dat holds ten lists for each of sixteen human manager places (SCOUT_MANAGER::save 0x7f8020), and list 0
// of a manager is his Short List (scout_manager_write_list_name 0x7fc150). It is read and changed as the game does:
// an entry as PLAYER_SEARCH::add_player 0x7af720 builds it, kept sorted by staff id, the capacity at least the count
// (PLAYER_SEARCH::load 0x7b17e0 allocates capacity entries), a removal by the removed bit (contains_player 0x7af600).
// It runs in the page and in Node (the tests); it uses no DOM.
import { readDate, writeDate, STAFF_SIZE } from './savedit.js';

export const MEMBER = 'scout_manager.dat';
const RESULT = 45;
// The fields of one search as PLAYER_SEARCH::save 0x7b2360 writes them before the count: 490 bytes, the capacity last.
const HEAD = 494, CAPACITY_AT = 490, TAIL = 0x99 + 0x11 + 12, CLUB_AT = 4 + 1 + 0x16b + 0x18, CLUB_TABLE = 0x51a8;
const REMOVED = 1, GROW = 250, GAME_DATE = 0xf85;          // general.dat: the game's date (save_game_data 0x82e430)

const i32 = (b, o) => new DataView(b.buffer, b.byteOffset, b.byteLength).getInt32(o, true);
const set32 = (b, o, v) => new DataView(b.buffer, b.byteOffset, b.byteLength).setInt32(o, v, true);

// { searches: [{ manager, slot, head, capacity, count, results: [Uint8Array 45], tail }], rest }; each search's head is
// its 494 bytes before the count, tail the bytes after its results (with the club report table when it has one).
export function parseSearches(b) {
  const n = i32(b, 0), searches = [];
  let at = 4;
  for (let k = 0; k < n; k++) {
    const head = b.slice(at, at + HEAD); at += HEAD;
    const count = i32(b, at); at += 4;
    if (count < 0 || at + count * RESULT > b.length) throw Object.assign(new Error('the scout lists of the saved game are damaged'), { key: 'the scout lists of the saved game are damaged' });
    const results = [];
    for (let i = 0; i < count; i++, at += RESULT) results.push(b.slice(at, at + RESULT));
    const tailSize = TAIL + (i32(head, CLUB_AT) > -1 ? CLUB_TABLE : 0);
    const tail = b.slice(at, at + tailSize); at += tailSize;
    searches.push({ manager: i32(head, 0), slot: head[4], head, capacity: i32(head, CAPACITY_AT), count, results, tail });
  }
  return { searches, rest: b.slice(at) };
}

export function writeSearches({ searches, rest }) {
  const size = 4 + searches.reduce((a, s) => a + HEAD + 4 + s.results.length * RESULT + s.tail.length, 0) + rest.length;
  const out = new Uint8Array(size);
  set32(out, 0, searches.length);
  let at = 4;
  for (const s of searches) {
    set32(s.head, CAPACITY_AT, s.capacity);
    out.set(s.head, at); at += HEAD;
    set32(out, at, s.results.length); at += 4;
    for (const r of s.results) { out.set(r, at); at += RESULT; }
    out.set(s.tail, at); at += s.tail.length;
  }
  out.set(rest, at);
  return out;
}

// The Short Lists of a SaveGame, or null when it has none (no scout_manager.dat, or no human manager).
export function scoutManager(game) {
  if (!game.file.has(MEMBER)) return null;
  const parsed = parseSearches(game.file.read(MEMBER));
  const s = game.staff;
  const nameOf = (id) => {
    if (id < 0 || id >= s.count) return '';
    const o = id * STAFF_SIZE, n = (k, list) => { const x = s.dv.getInt32(o + k, true); return x >= 0 && x < list.length ? list[x] : ''; };
    return (n(0x0c, game.commonNames) || `${n(0x04, game.firstNames)} ${n(0x08, game.secondNames)}`).trim();
  };
  // A place is a human manager when his staff row has a name; the reserved places have none.
  const managers = [...new Set(parsed.searches.map((x) => x.manager))].map((id) => ({ id, name: nameOf(id) })).filter((m) => m.name);
  if (!managers.length) return null;
  const list = (manager) => parsed.searches.find((x) => x.manager === manager && x.slot === 0) ?? null;
  const find = (l, staffId) => l.results.findIndex((r) => i32(r, 0x0c) === staffId);
  const save = () => {
    const m = game.file.member(MEMBER);
    m.data = writeSearches(parsed);
    game.file.changed(MEMBER);
  };
  return {
    managers,
    ids(manager) { const l = list(manager); return new Set(l ? l.results.filter((r) => !(r[0x20] & REMOVED)).map((r) => i32(r, 0x0c)) : []); },
    has(manager, staffId) { const l = list(manager), i = l ? find(l, staffId) : -1; return i >= 0 && !(l.results[i][0x20] & REMOVED); },
    // Adds a player (his staff id) to the manager's Short List: false when he is there already.
    add(manager, staffId) {
      const l = list(manager);
      if (!l || staffId < 0 || staffId >= s.count) return false;
      const i = find(l, staffId);
      if (i >= 0) {
        if (!(l.results[i][0x20] & REMOVED)) return false;
        l.results[i][0x20] &= ~REMOVED;
        save();
        return true;
      }
      // The entry: the row's names, id and date of birth (the record key 0x7e4920), the game's date, zeros, no rating.
      const r = new Uint8Array(RESULT), o = staffId * STAFF_SIZE;
      r.set(s.b.subarray(o + 4, o + 16), 0);
      r.set(s.b.subarray(o, o + 4), 12);
      r.set(s.b.subarray(o + 16, o + 24), 16);
      const today = readDate(game.file.read('general.dat'), GAME_DATE);
      if (today) writeDate(r, 0x18, today);
      r[0x2c] = 0xff;
      const at = l.results.findIndex((x) => i32(x, 0x0c) > staffId);
      l.results.splice(at < 0 ? l.results.length : at, 0, r);
      if (l.results.length > l.capacity) l.capacity += GROW;
      l.count = l.results.length;
      save();
      return true;
    },
    // Takes a player off the manager's Short List (the removed bit, as the game reads it): false when he is not on it.
    remove(manager, staffId) {
      const l = list(manager), i = l ? find(l, staffId) : -1;
      if (i < 0 || l.results[i][0x20] & REMOVED) return false;
      l.results[i][0x20] |= REMOVED;
      save();
      return true;
    },
  };
}
