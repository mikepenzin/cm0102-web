// The data set editor's model (web/README.md, "Edit a data set"): the database files that a new game loads, opened,
// their players changed or added, and the files written again. It runs in the page and in Node (the tests).
//
// Facts (docs/research/2026-11-10-data-set-editor.md has them with the reads):
// - index.dat: 8 bytes, then 22 entries of 0x43 bytes: file name, table, count, offset, format (read_index,
//   0x528734). A file may hold several tables one after another: in the 3.9.68 data staff.dat holds the staff (6),
//   table 8 (empty), the non-players (9) and the players (10).
// - The loader takes each table's rows from its offset and sizes its arrays from the counts (0x52990e); every id is
//   its row (the id pass 0x5315a0 links an id to that row). So a new row goes at the end of its table, with the id
//   of its row, and the tables after it in the same file move.
// - Staff rows in format 1 are 157 bytes (convert_staff 0x52caa1 reads them: names +4/+8/+0xc, born +0x10, year +0x18,
//   nation +0x1a, second nation +0x1e, club +0x39, job +0x3d, joined +0x3e, contract end +0x46, flags +0x5f,
//   preferences +0x61..+0x90, player row +0x91, non-player row +0x99); format 2 rows are 110 (the same up to +0x60,
//   player +0x61, preferences +0x65, non-player +0x69). Player rows are 70 bytes; attributes are 0..20, 0 random.
// - A club (581 bytes) lists its squad in 50 slots at +0xd7, -1 when empty, used slots first.
// - A name row (60 bytes): the name (51), its id +0x33, a nation +0x37, a count byte +0x3b.
import { ATTRIBUTES as SAVE_ATTRIBUTES, POSITIONS, positionText, readDate, writeDate } from './savedit.js';

const INDEX_ENTRIES = 22, INDEX_ENTRY = 0x43;
const ROW = { 0: 581, 1: 581, 2: 58, 3: 198, 4: 290, 5: 78, 7: 43, 8: 157, 9: 68, 10: 70, 11: 101, 12: 107, 13: 60, 14: 60,
  15: 60, 16: 107, 17: 17, 18: 58, 19: 26, 20: 26, 21: 56, 22: 52 };
export const DATA_SET_FILES = ['city.dat', 'club.dat', 'club_comp.dat', 'club_comp_history.dat', 'colour.dat', 'common_names.dat',
  'continent.dat', 'first_names.dat', 'index.dat', 'nat_club.dat', 'nation.dat', 'nation_comp.dat', 'nation_comp_history.dat',
  'officials.dat', 'second_names.dat', 'stadium.dat', 'staff.dat', 'staff_comp.dat', 'staff_comp_history.dat', 'staff_history.dat'];
const NO_DATE = { day: 31, year: 1900, leap: 1 };               // the data's "no date"
const GAME_START = { year: 2001, month: 7, day: 1 };            // joined, for a new player or a new club
const SQUAD = 0xd7, SQUAD_SLOTS = 50, TEAM = 0x1e0, TEAM_SLOTS = 20, CLUB_NATION = 0x53;
const PLAYER_JOB = 11;

// The attributes of the data: the same bytes as in a saved game, but each the rating itself (0 random).
export const ATTRIBUTES = SAVE_ATTRIBUTES.map((a) => ({ ...a, kind: 'raw' }));
export { POSITIONS };

const latin1 = (b, o, n) => { let e = o; while (e < o + n && b[e]) e++; return String.fromCharCode(...b.subarray(o, e)); };
const fold = (s) => s.normalize('NFKD').replace(/[̀-ͯ]/g, '').toLowerCase();
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, Math.round(Number(v)) || 0));

export class DataSetError extends Error {}

// A text for a name field: Latin-1 (the game's code page for names is 1252; these are its Latin-1 part), at most 50.
export function nameBytes(text) {
  const t = String(text ?? '').trim();
  if (t.length > 50) throw new DataSetError(`"${t}" is longer than 50 letters`);
  for (const ch of t) if (ch.charCodeAt(0) > 0xff || ch.charCodeAt(0) < 0x20) throw new DataSetError(`"${t}" has a letter the game cannot show: ${ch}`);
  return t;
}

export class DataSet {
  // files: { name (any case): Uint8Array } with the 20 database files.
  constructor(files) {
    this.files = new Map();
    for (const [name, bytes] of Object.entries(files)) this.files.set(name.toLowerCase(), { name, bytes });
    const missing = DATA_SET_FILES.filter((f) => !this.files.has(f));
    if (missing.length) throw new DataSetError(`the data set lacks ${missing.join(', ')}`);
    const index = this.files.get('index.dat').bytes;
    if (index.length < 8 + INDEX_ENTRIES * INDEX_ENTRY) throw new DataSetError('index.dat is too short');
    const dv = new DataView(index.buffer, index.byteOffset, index.byteLength);
    this.entries = [];
    for (let k = 0; k < INDEX_ENTRIES; k++) {
      const o = 8 + k * INDEX_ENTRY;
      this.entries.push({ pos: o, file: latin1(index, o, 0x33).toLowerCase(), table: dv.getInt32(o + 0x33, true),
        count: dv.getInt32(o + 0x37, true), offset: dv.getInt32(o + 0x3b, true), format: dv.getInt32(o + 0x3f, true) });
    }
    const staff = this.entry(6);
    if (staff.format !== 1 && staff.format !== 2) throw new DataSetError(`the staff table has format ${staff.format}`);
    this.staffSize = staff.format === 1 ? 157 : 110;
    this.link = staff.format === 1 ? { player: 0x91, nonPlayer: 0x99 } : { player: 0x61, nonPlayer: 0x69 };
    for (const e of this.entries) {
      if (e.count === 0 || !(e.table in ROW) && e.table !== 6) continue;
      const f = this.files.get(e.file), size = this.rowSize(e.table);
      if (!f || e.offset < 0 || e.count < 0 || e.offset + e.count * size > f.bytes.length)
        throw new DataSetError(`table ${e.table} (${e.count} rows of ${size} at ${e.offset}) runs past the end of ${e.file}`);
    }
    this.added = new Map();             // table -> [Uint8Array row]
    this.changedFiles = new Set();
    this.nameIndex = {};
    this.list = null;
  }

  entry(table) {
    const e = this.entries.find((x) => x.table === table);
    if (!e) throw new DataSetError(`index.dat has no table ${table}`);
    return e;
  }

  rowSize(table) { return table === 6 ? this.staffSize : ROW[table]; }
  count(table) { return this.entry(table).count + (this.added.get(table)?.length ?? 0); }

  // The bytes of a row, to read or change in place.
  row(table, i) {
    const e = this.entry(table), size = this.rowSize(table);
    if (i < 0 || i >= this.count(table)) throw new DataSetError(`row ${i} is not in table ${table}`);
    if (i >= e.count) return this.added.get(table)[i - e.count];
    return this.files.get(e.file).bytes.subarray(e.offset + i * size, e.offset + (i + 1) * size);
  }

  touch(table) { if (table >= 0) this.changedFiles.add(this.entry(table).file); this.list = null; }

  // ---- Reading ----

  name(table, id) { return id >= 0 && id < this.count(table) ? latin1(this.row(table, id), 0, 51) : ''; }

  clubName(id) {
    if (id < 0) return '';
    const own = this.entry(0).count;
    if (id < own) return latin1(this.row(0, id), 4, 51);
    return id - own < this.count(1) ? latin1(this.row(1, id - own), 4, 51) : '';
  }

  nationName(id) { return id >= 0 && id < this.count(4) ? latin1(this.row(4, id), 4, 51) : ''; }

  // The clubs a player can join (club.dat, not the national teams), by name.
  clubs() {
    const out = [];
    for (let i = 0; i < this.count(0); i++) {
      const r = this.row(0, i), name = latin1(r, 4, 51);
      if (name) out.push({ id: i, name, nation: i32(r, CLUB_NATION) });
    }
    return out.sort((a, b) => a.name.localeCompare(b.name));
  }

  nations() {
    const out = [];
    for (let i = 0; i < this.count(4); i++) { const n = this.nationName(i); if (n) out.push({ id: i, name: n }); }
    return out.sort((a, b) => a.name.localeCompare(b.name));
  }

  players() {
    if (this.list) return this.list;
    const out = [], nP = this.count(10);
    for (let i = 0; i < this.count(6); i++) {
      const s = this.row(6, i), d = i32(s, this.link.player);
      if (!(s[0x5f] & 2) || d < 0 || d >= nP) continue;
      const p = this.row(10, d);
      const common = this.name(15, i32(s, 0x0c));
      const full = common || `${this.name(13, i32(s, 0x04))} ${this.name(14, i32(s, 0x08))}`.trim();
      out.push({ row: i, data: d, name: full || '(no name)', key: fold(full), club: this.clubName(i32(s, 0x39)),
        nation: this.nationName(i32(s, 0x1a)), born: readDate(s, 0x10)?.year ?? u16(s, 0x18),
        ca: i16(p, 5), pa: i16(p, 7), pos: positionText(p.subarray(0x0f, 0x1b)) });
    }
    this.list = out;
    return out;
  }

  search(text, limit = 60) {
    const words = fold(text).split(/\s+/).filter(Boolean);
    const all = this.players();
    const hit = words.length ? all.filter((x) => words.every((w) => x.key.includes(w) || fold(x.club).includes(w))) : all;
    return [...hit].sort((a, b) => b.ca - a.ca || a.name.localeCompare(b.name)).slice(0, limit);
  }

  read(row) {
    const s = this.row(6, row), p = this.row(10, i32(s, this.link.player));
    const attrs = {};
    for (const a of ATTRIBUTES) attrs[a.name] = a.from === 'staff' ? s[a.off] : p[a.off];
    const end = readDate(s, 0x46);
    return { first: this.name(13, i32(s, 0x04)), second: this.name(14, i32(s, 0x08)), common: this.name(15, i32(s, 0x0c)),
      born: readDate(s, 0x10), nation: i32(s, 0x1a), nation2: i32(s, 0x1e), club: i32(s, 0x39), contractEnd: end,
      ca: i16(p, 5), pa: i16(p, 7), positions: [...p.subarray(0x0f, 0x1b)], attrs };
  }

  // ---- Writing ----

  // The id of a name in a name table: the row with that text, else a new row (nation, count 1).
  nameId(table, text, nation) {
    const t = nameBytes(text);
    if (!t && table === 15) return 0;                            // row 0 of the common names is empty: no common name
    if (!this.nameIndex[table]) {
      const m = new Map();
      for (let i = 0; i < this.count(table); i++) { const n = latin1(this.row(table, i), 0, 51); if (!m.has(n)) m.set(n, i); }
      this.nameIndex[table] = m;
    }
    const m = this.nameIndex[table];
    if (m.has(t)) {
      const r = this.row(table, m.get(t));
      if (r[0x3b] < 255) { r[0x3b]++; this.touch(table); }
      return m.get(t);
    }
    const id = this.count(table), r = new Uint8Array(60);
    for (let k = 0; k < t.length; k++) r[k] = t.charCodeAt(k);
    set32(r, 0x33, id);
    set32(r, 0x37, nation);
    r[0x3b] = 1;
    this.append(table, r);
    m.set(t, id);
    return id;
  }

  append(table, bytes) {
    if (!this.added.has(table)) this.added.set(table, []);
    this.added.get(table).push(bytes);
    this.touch(table);
    this.changedFiles.add('index.dat');
  }

  // Takes the staff id out of a club's squad (the slots stay packed) and its 20 selected players.
  leaveClub(club, id) {
    if (club < 0 || club >= this.count(0)) return;
    const c = this.row(0, club);
    const squad = [];
    for (let k = 0; k < SQUAD_SLOTS; k++) { const v = i32(c, SQUAD + 4 * k); if (v >= 0 && v !== id) squad.push(v); }
    for (let k = 0; k < SQUAD_SLOTS; k++) set32(c, SQUAD + 4 * k, k < squad.length ? squad[k] : -1);
    for (let k = 0; k < TEAM_SLOTS; k++) if (i32(c, TEAM + 4 * k) === id) set32(c, TEAM + 4 * k, -1);
    this.touch(0);
  }

  joinClub(club, id) {
    if (club < 0) return;
    if (club >= this.count(0)) throw new DataSetError('a player can join a club, not a national team');
    const c = this.row(0, club);
    for (let k = 0; k < SQUAD_SLOTS; k++) {
      const v = i32(c, SQUAD + 4 * k);
      if (v === id) return;
      if (v < 0) { set32(c, SQUAD + 4 * k, id); this.touch(0); return; }
    }
    throw new DataSetError(`${this.clubName(club)} has ${SQUAD_SLOTS} players already: the game takes no more`);
  }

  // Writes the fields of a player (as read gives them, changed).
  write(row, f) {
    const s = this.row(6, row), p = this.row(10, i32(s, this.link.player));
    if (!nameBytes(f.common) && !(nameBytes(f.first) && nameBytes(f.second))) throw new DataSetError('a player needs a first name and a surname, or a common name');
    // The club first: a full club stops the change before anything else moves.
    const club = Number(f.club), old = i32(s, 0x39);
    if (club !== old) {
      this.joinClub(club, row);
      this.leaveClub(old, row);
      set32(s, 0x39, club);
      s[0x3d] = club >= 0 ? PLAYER_JOB : 0;
      writeDate(s, 0x3e, GAME_START);
    }
    const nation = clamp(f.nation, 0, this.count(4) - 1);
    set32(s, 0x04, this.nameId(13, f.first, nation));
    set32(s, 0x08, this.nameId(14, f.second, nation));
    set32(s, 0x0c, this.nameId(15, f.common, nation));
    if (f.born) { writeDate(s, 0x10, f.born); set16(s, 0x18, f.born.year); }
    set32(s, 0x1a, nation);
    set32(s, 0x1e, f.nation2 >= 0 ? clamp(f.nation2, 0, this.count(4) - 1) : -2);
    if (f.contractEnd) writeDate(s, 0x46, f.contractEnd);
    else writeRaw(s, 0x46, NO_DATE);
    set16(p, 5, clamp(f.ca, 0, 200));
    set16(p, 7, clamp(f.pa, -2, 200));
    f.positions.forEach((v, k) => { p[0x0f + k] = clamp(v, 0, 20); });
    for (const a of ATTRIBUTES) (a.from === 'staff' ? s : p)[a.off] = clamp(f.attrs[a.name], 0, 20);
    this.touch(6);
    this.touch(10);
  }

  // A new player: a staff row and a player row at the ends of their tables, the defaults of most players of the data
  // (no second nation, no national team, no caps, value and wage 0, personality 0), then the fields. Returns its row.
  add(f) {
    if (this.entry(6).format !== 1) throw new DataSetError('players can be added to data sets in the format of the game\'s own data only');
    const id = this.count(6), data = this.count(10);
    const s = new Uint8Array(this.staffSize), p = new Uint8Array(70);
    set32(s, 0x00, id);
    writeRaw(s, 0x10, NO_DATE);
    set32(s, 0x1a, -2); set32(s, 0x1e, -2); set32(s, 0x24, -2); set32(s, 0x39, -1);
    writeRaw(s, 0x31, NO_DATE);
    writeRaw(s, 0x3e, NO_DATE);
    writeRaw(s, 0x46, NO_DATE);
    s[0x5e] = 1;                                               // as 122,683 of the 138,477 players of 3.9.68
    s[0x5f] = 2;                                               // player data, no non-player data
    for (let k = 0; k < 12; k++) set32(s, 0x61 + 4 * k, -1);   // no favourite clubs or people
    set32(s, 0x91, data);
    set32(s, 0x95, -1);
    set32(s, 0x99, -1);
    set32(p, 0, data);
    this.append(6, s);
    this.append(10, p);
    try {
      this.write(id, f);                                       // the club: the row starts with none, so it joins
    } catch (err) {                                            // a refused add leaves no row behind
      this.added.get(6).pop();
      this.added.get(10).pop();
      this.list = null;
      throw err;
    }
    return id;
  }

  // The files to write: each changed file rebuilt with its tables' new rows, and index.dat with the new counts and
  // offsets. { name: Uint8Array } with the names as the data set has them.
  output() {
    const out = {};
    const index = this.files.get('index.dat').bytes.slice();
    for (const file of this.changedFiles) {
      if (file === 'index.dat') continue;
      const f = this.files.get(file), tables = this.entries.filter((e) => e.file === file).sort((a, b) => a.offset - b.offset);
      const parts = [];
      let at = 0, size = 0;
      const push = (b) => { parts.push(b); size += b.length; };
      for (const e of tables) {
        if (e.offset > at) push(f.bytes.subarray(at, e.offset));          // bytes between tables stay
        const newOffset = size;
        const rows = e.count * this.rowSize(e.table);
        push(f.bytes.subarray(e.offset, e.offset + rows));
        for (const r of this.added.get(e.table) ?? []) push(r);
        at = Math.max(at, e.offset + rows);
        const dv = new DataView(index.buffer);
        dv.setInt32(e.pos + 0x37, this.count(e.table), true);
        dv.setInt32(e.pos + 0x3b, newOffset, true);
      }
      if (at < f.bytes.length) push(f.bytes.subarray(at));
      const b = new Uint8Array(size);
      let o = 0;
      for (const part of parts) { b.set(part, o); o += part.length; }
      out[f.name] = b;
    }
    out[this.files.get('index.dat').name] = index;
    return out;
  }
}

function i32(b, o) { return new DataView(b.buffer, b.byteOffset, b.byteLength).getInt32(o, true); }
function i16(b, o) { return new DataView(b.buffer, b.byteOffset, b.byteLength).getInt16(o, true); }
function u16(b, o) { return new DataView(b.buffer, b.byteOffset, b.byteLength).getUint16(o, true); }
function set32(b, o, v) { new DataView(b.buffer, b.byteOffset, b.byteLength).setInt32(o, v, true); }
function set16(b, o, v) { new DataView(b.buffer, b.byteOffset, b.byteLength).setInt16(o, v, true); }
function writeRaw(b, o, { day, year, leap }) {
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  dv.setInt16(o, day, true); dv.setUint16(o + 2, year, true); dv.setUint32(o + 4, leap, true);
}
