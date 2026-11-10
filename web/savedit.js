// The save editor's model (web/README.md, "Edit a saved game"): a CM 01/02 saved game opened, its players read and
// changed, and the save written again. It runs in the page and in Node (the tests); it uses no DOM.
//
// Facts of the original, cm0102.exe 3.9.68 (docs/research/2026-11-10-save-editor.md has them with the reads):
// - The container (zip.cpp): a header of 0x416ec bytes, int version, int, int count, then 1000 entries of 0x10c
//   bytes (int offset, int size, char name[260]); version 2 and 4 compress each member with a byte run code
//   (decoder 0x932c30, encoder 0x932e10). tools/save/savfile.py is the same in Python.
// - The save holds the database of the loaded game as members: index.dat (22 entries of 0x43 bytes after 8 bytes)
//   and one member per table, rows from offset 0: staff.dat in format 2 (110 bytes), player.dat (70), the three name
//   tables (60), club.dat and nat_club.dat (581), nation.dat (290).
// - A shown attribute is what 0x543540 (staff, player data, number 1..31) returns. Some bytes are the rating as
//   it is; others are stored relative to current ability and converted by 0x544380 (or 0x5442f0, which takes a
//   tenth of the ability, for some attributes depending on the goalkeeper rating at +0x0f).
// - Dates are Date.cpp's: the day of the year from 0 (1 January), the year, and a leap flag (FUN_00549f70).

const HEADER = 0x416ec, ENTRY = 0x10c, MAX_ENTRIES = 1000;
const INDEX_ENTRIES = 22, INDEX_ENTRY = 0x43;
export const STAFF_SIZE = 110, PLAYER_SIZE = 70, NAME_SIZE = 60, CLUB_SIZE = 581, NATION_SIZE = 290;

// ---- The container ----

// The member's bytes from the byte run code: a byte below 0x81 is itself, a byte b of 0x81 or more repeats the
// next byte b - 0x80 times (0x932c30). used: the bytes of raw that the member took.
export function decodeRuns(raw, size) {
  const out = new Uint8Array(size);
  let i = 0, o = 0;
  while (o < size && i < raw.length) {
    const b = raw[i++];
    if (b < 0x81) out[o++] = b;
    else {
      if (i >= raw.length) break;
      const v = raw[i++], n = Math.min(b - 0x80, size - o);
      out.fill(v, o, o + n);
      o += n;
    }
  }
  if (o < size) throw new Error(`a member ends early: ${o} of ${size} bytes`);
  return { data: out, used: i };
}

// The encoder 0x932e10: runs of up to 0x7f bytes; a single byte below 0x81 stays as it is.
export function encodeRuns(data) {
  const out = new Uint8Array(data.length * 2 + 16);
  let i = 0, o = 0;
  const n = data.length;
  while (i < n) {
    const b = data[i];
    let j = i + 1;
    while (j < n && data[j] === b && j - i < 0x7f) j++;
    const count = j - i;
    if (count < 2 && b < 0x81) out[o++] = b;
    else { out[o++] = count + 0x80; out[o++] = b; }
    i = j;
  }
  return out.slice(0, o);
}

export class SaveFile {
  constructor(bytes) {
    this.blob = bytes;
    const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    if (bytes.length < HEADER) throw new Error('it is too short to be a saved game');
    this.version = dv.getInt32(0, true);
    const count = dv.getInt32(8, true);
    if (![1, 2, 3, 4].includes(this.version) || count < 1 || count > MAX_ENTRIES) throw new Error('it is not a saved game of CM 01/02');
    this.compressed = this.version === 2 || this.version === 4;
    this.members = [];
    for (let k = 0; k < count; k++) {
      const o = 12 + k * ENTRY;
      const offset = dv.getUint32(o, true), size = dv.getUint32(o + 4, true);
      let end = o + 8;
      while (end < o + ENTRY && bytes[end] !== 0) end++;
      const name = String.fromCharCode(...bytes.subarray(o + 8, end));
      if (offset < HEADER || offset > bytes.length) throw new Error(`the saved game is damaged: ${name} starts outside the file`);
      this.members.push({ index: k, offset, size, name, raw: null, data: null, changed: false });
    }
    const order = [...this.members].sort((a, b) => a.offset - b.offset);
    order.forEach((m, i) => { m.raw = bytes.subarray(m.offset, i + 1 < order.length ? order[i + 1].offset : bytes.length); });
  }

  member(name) {
    const m = this.members.find((x) => x.name.toLowerCase() === name.toLowerCase());
    if (!m) throw new Error(`the saved game has no ${name}`);
    return m;
  }

  has(name) { return this.members.some((x) => x.name.toLowerCase() === name.toLowerCase()); }

  // The member's bytes, decoded once; the array is the editor's to change (put marks it changed).
  read(name) {
    const m = this.member(name);
    if (!m.data) m.data = this.compressed ? decodeRuns(m.raw, m.size).data : m.raw.slice(0, m.size);
    return m.data;
  }

  changed(name) { this.member(name).changed = true; }

  // The whole file: the header with the new offsets, then the members in their order. A member that did not
  // change keeps its bytes as they were.
  toBytes() {
    const parts = [], header = this.blob.slice(0, HEADER);
    const dv = new DataView(header.buffer);
    let at = HEADER;
    for (const m of [...this.members].sort((a, b) => a.offset - b.offset)) {
      const raw = m.changed ? (this.compressed ? encodeRuns(m.data) : m.data) : m.raw;
      dv.setUint32(12 + m.index * ENTRY, at, true);
      parts.push(raw);
      at += raw.length;
    }
    const out = new Uint8Array(at);
    out.set(header, 0);
    let o = HEADER;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }
}

// ---- Dates (Date.cpp) ----

const MONTH_START = [-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333];
const MONTH_START_LEAP = [-1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334];
const isLeap = (y) => !(y % 2 === 1 || y % 4 !== 0) && (y % 100 !== 0 || y % 400 === 0);

// { year, month 1..12, day 1..31 } of a date record, or null for no date (a year of 1900 or before).
export function readDate(b, o) {
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const day = dv.getInt16(o, true), year = dv.getUint16(o + 2, true);
  if (year <= 1900) return null;
  const t = dv.getUint32(o + 4, true) ? MONTH_START_LEAP : MONTH_START;
  let m = 0;
  for (let k = 11; k >= 0; k--) if (t[k] < day) { m = k; break; }
  return { year, month: m + 1, day: day - t[m] };
}

export function writeDate(b, o, { year, month, day }) {
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const leap = isLeap(year);
  dv.setInt16(o, (leap ? MONTH_START_LEAP : MONTH_START)[month - 1] + day, true);
  dv.setUint16(o + 2, year, true);
  dv.setUint32(o + 4, leap ? 1 : 0, true);
}

// ---- Attributes (0x543540) ----

// The rating of a relative byte r with ability a: 0x544380 with a = current ability, 0x5442f0 with a tenth of it.
export function rating(r, a) {
  const x = 0.05 * (2 * r + a) + 10;
  const y = (x + x * x * 0.1) * (1 / 3) + 0.5;           // in the game's order: x * x, then * 0.1 (0x5435f0)
  if (y <= 1) return 1;
  if (y >= 20) return 20;
  return Math.trunc(y);
}

// kind: raw (the byte is the rating), rel (0x544380), gk20 (0x5442f0's tenth when the goalkeeper rating is 20),
// gk15 (the tenth when it is below 15). from: 'player' (the player data) or 'staff' (the staff row).
// The 31 shown attributes in the game's numbering (the case of the jump table 0x543a94), then the bytes that the
// game does not show.
export const ATTRIBUTES = [
  ['Acceleration', 0x1b, 'raw', 'Physical'], ['Aggression', 0x1c, 'raw', 'Mental'], ['Agility', 0x1d, 'raw', 'Physical'],
  ['Anticipation', 0x1e, 'rel', 'Mental'], ['Balance', 0x1f, 'raw', 'Physical'], ['Bravery', 0x20, 'raw', 'Mental'],
  ['Creativity', 0x43, 'gk20', 'Mental'], ['Crossing', 0x23, 'gk20', 'Technical'], ['Decisions', 0x24, 'rel', 'Mental'],
  ['Determination', 0x58, 'raw', 'Mental', 'staff'], ['Dribbling', 0x26, 'gk20', 'Technical'],
  ['Finishing', 0x27, 'gk20', 'Technical'], ['Flair', 0x28, 'raw', 'Mental'], ['Handling', 0x2a, 'gk15', 'Goalkeeping'],
  ['Heading', 0x2b, 'rel', 'Technical'], ['Influence', 0x2f, 'raw', 'Mental'], ['Jumping', 0x2e, 'raw', 'Physical'],
  ['Long Shots', 0x31, 'rel', 'Technical'], ['Marking', 0x32, 'gk20', 'Technical'], ['Off The Ball', 0x33, 'gk20', 'Mental'],
  ['Pace', 0x36, 'raw', 'Physical'], ['Passing', 0x37, 'rel', 'Technical'], ['Positioning', 0x39, 'rel', 'Mental'],
  ['Reflexes', 0x3a, 'gk15', 'Goalkeeping'], ['Set Pieces', 0x29, 'raw', 'Technical'], ['Stamina', 0x3c, 'raw', 'Physical'],
  ['Strength', 0x3d, 'raw', 'Physical'], ['Tackling', 0x3e, 'rel', 'Technical'], ['Teamwork', 0x3f, 'raw', 'Mental'],
  ['Technique', 0x40, 'raw', 'Technical'], ['Work Rate', 0x44, 'raw', 'Mental'],
  // Not shown by the game. One On Ones, Penalties and Throw Ins are stored relative, as the shown relative ones.
  ['Consistency', 0x21, 'raw', 'Hidden'], ['Corners', 0x22, 'raw', 'Hidden'], ['Dirtiness', 0x25, 'raw', 'Hidden'],
  ['Important Matches', 0x2c, 'raw', 'Hidden'], ['Injury Proneness', 0x2d, 'raw', 'Hidden'], ['Left Foot', 0x30, 'raw', 'Hidden'],
  ['Natural Fitness', 0x34, 'raw', 'Hidden'], ['One On Ones', 0x35, 'rel', 'Hidden'], ['Penalties', 0x38, 'rel', 'Hidden'],
  ['Right Foot', 0x3b, 'raw', 'Hidden'], ['Throw Ins', 0x41, 'rel', 'Hidden'], ['Versatility', 0x42, 'raw', 'Hidden'],
].map(([name, off, kind, group, from = 'player']) => ({ name, off, kind, group, from }));

const GK = 0x0f;    // the goalkeeper rating among the positions
const tenth = (kind, gk) => (kind === 'gk20' && gk === 20) || (kind === 'gk15' && gk < 15);

// The rating that the game shows for a stored byte.
export function shown(attr, stored, ability, gk) {
  if (attr.kind === 'raw') return stored;
  const r = stored << 24 >> 24;                             // a signed byte
  return rating(r, tenth(attr.kind, gk) ? ability * 0.1 : ability);
}

// The byte to store for a rating to show; null when no byte gives it (then the nearest that can be shown is
// returned as { byte, shows }). Of the bytes that give it, the middle one, so that a small change of ability later
// moves the rating as little as it can.
export function storedFor(attr, want, ability, gk) {
  if (attr.kind === 'raw') return { byte: Math.max(1, Math.min(20, want)), shows: Math.max(1, Math.min(20, want)) };
  const a = tenth(attr.kind, gk) ? ability * 0.1 : ability;
  const hits = [];
  let best = null;
  for (let r = -128; r <= 127; r++) {
    const v = rating(r, a);
    if (v === want) hits.push(r);
    if (!best || Math.abs(v - want) < Math.abs(best.shows - want)) best = { r, shows: v };
  }
  const r = hits.length ? hits[hits.length >> 1] : best.r;
  return { byte: r & 0xff, shows: hits.length ? want : best.shows };
}

export const POSITIONS = ['Goalkeeper', 'Sweeper', 'Defender', 'Defensive Midfielder', 'Midfielder', 'Attacking Midfielder',
  'Attacker', 'Wing Back', 'Right', 'Left', 'Centre', 'Free Role'];
const SHORT = ['GK', 'SW', 'D', 'DM', 'M', 'AM', 'F', 'WB'];

// The positions as a short text, the ratings of 15 or more, with the sides (as the squad list does in spirit).
export function positionText(pos) {
  const p = SHORT.filter((_, i) => pos[i] >= 15);
  const sides = ['R', 'L', 'C'].filter((_, i) => pos[8 + i] >= 15).join('');
  return p.length ? `${p.join('/')}${sides && !(p.length === 1 && p[0] === 'GK') ? ` ${sides}` : ''}` : '-';
}

// ---- The saved game's tables ----

const latin1 = (b, o, n) => { let e = o; while (e < o + n && b[e]) e++; return String.fromCharCode(...b.subarray(o, e)); };
const fold = (s) => s.normalize('NFKD').replace(/[̀-ͯ]/g, '').toLowerCase();

export class SaveGame {
  constructor(bytes) {
    this.file = new SaveFile(bytes);
    const index = this.file.read('index.dat');
    if (index.length < 8 + INDEX_ENTRIES * INDEX_ENTRY) throw new Error('the saved game has no table index');
    this.tables = {};
    const dv = new DataView(index.buffer, index.byteOffset, index.byteLength);
    for (let k = 0; k < INDEX_ENTRIES; k++) {
      const o = 8 + k * INDEX_ENTRY;
      this.tables[dv.getInt32(o + 0x33, true)] = { file: latin1(index, o, 0x33), count: dv.getInt32(o + 0x37, true) };
    }
    const table = (id, size) => {
      const t = this.tables[id];
      if (!t || !this.file.has(t.file)) throw new Error(`the saved game has no table ${id}`);
      const b = this.file.read(t.file);
      if (b.length !== t.count * size) throw new Error(`${t.file} holds ${b.length} bytes, not ${t.count} rows of ${size}`);
      return { name: t.file, b, count: t.count, dv: new DataView(b.buffer, b.byteOffset, b.byteLength) };
    };
    this.staff = table(6, STAFF_SIZE);
    this.player = table(10, PLAYER_SIZE);
    const names = (id) => { const t = table(id, NAME_SIZE); return Array.from({ length: t.count }, (_, i) => latin1(t.b, i * NAME_SIZE, 51)); };
    this.firstNames = names(13);
    this.secondNames = names(14);
    this.commonNames = names(15);
    this.clubs = new Map();
    for (const id of [0, 1]) {
      const t = table(id, CLUB_SIZE);
      for (let i = 0; i < t.count; i++) this.clubs.set(t.dv.getInt32(i * CLUB_SIZE, true), latin1(t.b, i * CLUB_SIZE + 4, 51));
    }
    const n = table(4, NATION_SIZE);
    this.nations = new Map();
    for (let i = 0; i < n.count; i++) this.nations.set(n.dv.getInt32(i * NATION_SIZE, true), latin1(n.b, i * NATION_SIZE + 4, 51));
    this.list = null;
  }

  // Every player of the save: { row (staff), data (player row), name, club, nation, born, ca, pa, pos }.
  players() {
    if (this.list) return this.list;
    const s = this.staff, p = this.player, out = [];
    for (let i = 0; i < s.count; i++) {
      const o = i * STAFF_SIZE;
      const d = s.dv.getInt32(o + 0x61, true);
      if (d < 0 || d >= p.count) continue;
      const name = (k, list) => { const id = s.dv.getInt32(o + k, true); return id >= 0 && id < list.length ? list[id] : ''; };
      const common = name(0x0c, this.commonNames);
      const full = common || `${name(0x04, this.firstNames)} ${name(0x08, this.secondNames)}`.trim();
      const po = d * PLAYER_SIZE;
      const born = readDate(s.b, o + 0x10);
      out.push({ row: i, data: d, name: full || '(no name)', key: fold(full), club: this.clubs.get(s.dv.getInt32(o + 0x39, true)) ?? '',
        nation: this.nations.get(s.dv.getInt32(o + 0x1a, true)) ?? '', born: born?.year ?? s.dv.getUint16(o + 0x18, true),
        ca: p.dv.getInt16(po + 5, true), pa: p.dv.getInt16(po + 7, true), pos: positionText(p.b.subarray(po + 0x0f, po + 0x1b)) });
    }
    this.list = out;
    return out;
  }

  // Players whose name has every word of the text (accents and case aside), the best known first.
  search(text, limit = 60) {
    const words = fold(text).split(/\s+/).filter(Boolean);
    const all = this.players();
    const hit = words.length ? all.filter((x) => words.every((w) => x.key.includes(w) || fold(x.club).includes(w))) : all;
    return [...hit].sort((a, b) => b.ca - a.ca).slice(0, limit);
  }

  // The fields of one player as the editor shows them.
  read(row) {
    const s = this.staff, p = this.player, o = row * STAFF_SIZE;
    const d = s.dv.getInt32(o + 0x61, true), po = d * PLAYER_SIZE;
    const ca = p.dv.getInt16(po + 5, true), gk = p.b[po + GK];
    const attrs = {};
    for (const a of ATTRIBUTES) {
      const b = a.from === 'staff' ? s.b[o + a.off] : p.b[po + a.off];
      attrs[a.name] = shown(a, b, ca, gk);
    }
    return { ca, pa: p.dv.getInt16(po + 7, true), positions: [...p.b.subarray(po + 0x0f, po + 0x1b)], attrs,
      born: readDate(s.b, o + 0x10), birthYear: s.dv.getUint16(o + 0x18, true) };
  }

  // Writes the fields (as read gives them, changed). A relative attribute keeps its byte when it still shows the
  // same with the new ability and positions; else it gets the byte that shows the rating asked for. Returns the
  // attributes that could not be shown as asked: { name: shows }.
  write(row, f) {
    const s = this.staff, p = this.player, o = row * STAFF_SIZE;
    const d = s.dv.getInt32(o + 0x61, true), po = d * PLAYER_SIZE;
    const ca = clamp(f.ca, 1, 200), pa = clamp(f.pa, 1, 200);
    const positions = f.positions.map((v) => clamp(v, 0, 20));
    const gk = positions[0];
    const missed = {};
    for (const a of ATTRIBUTES) {
      const at = a.from === 'staff' ? o + a.off : po + a.off;
      const b = a.from === 'staff' ? s.b : p.b;
      const want = clamp(f.attrs[a.name], 1, 20);
      if (shown(a, b[at], ca, gk) === want) continue;
      const r = storedFor(a, want, ca, gk);
      b[at] = r.byte;
      if (r.shows !== want) missed[a.name] = r.shows;
    }
    p.dv.setInt16(po + 5, ca, true);
    p.dv.setInt16(po + 7, pa, true);
    p.b.set(positions, po + 0x0f);
    if (f.born) {
      writeDate(s.b, o + 0x10, f.born);
      s.dv.setUint16(o + 0x18, f.born.year, true);
    }
    this.file.changed(this.staff.name);
    this.file.changed(this.player.name);
    this.list = null;
    return missed;
  }

  toBytes() { return this.file.toBytes(); }
}

const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, Math.round(Number(v)) || lo));
