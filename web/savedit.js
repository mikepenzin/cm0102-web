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
  if (o < size) throw Object.assign(new Error(`a member ends early: ${o} of ${size} bytes`), { key: 'a member ends early: {got} of {size} bytes', vars: { got: o, size } });
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

// A saved game's container. From bytes (the whole file), or with SaveFile.open from a Blob: then only the header is
// read at first, and a member's bytes when load asks for them, so a save of 690 MB is never in memory whole (the
// editor reads the header and its nine tables, a few MB); toBlob writes the file again from the members as they were
// (slices of the Blob) and the changed ones.
export class SaveFile {
  constructor(bytes) { this._init(bytes, bytes, bytes.length); }

  static async open(blob) {
    if (blob.size < HEADER) throw Object.assign(new Error('it is too short to be a saved game'), { key: 'it is too short to be a saved game' });
    const f = Object.create(SaveFile.prototype);
    f._init(new Uint8Array(await blob.slice(0, HEADER).arrayBuffer()), blob, blob.size);
    return f;
  }

  // header: the first HEADER bytes; source: the file, bytes or a Blob, of size bytes.
  _init(header, source, size) {
    if (size < HEADER || header.length < HEADER) throw Object.assign(new Error('it is too short to be a saved game'), { key: 'it is too short to be a saved game' });
    this.header = header;
    const dv = new DataView(header.buffer, header.byteOffset, header.byteLength);
    this.version = dv.getInt32(0, true);
    const count = dv.getInt32(8, true);
    if (![1, 2, 3, 4].includes(this.version) || count < 1 || count > MAX_ENTRIES) throw Object.assign(new Error('it is not a saved game of CM 01/02'), { key: 'it is not a saved game of CM 01/02' });
    this.compressed = this.version === 2 || this.version === 4;
    this.members = [];
    for (let k = 0; k < count; k++) {
      const o = 12 + k * ENTRY;
      const offset = dv.getUint32(o, true), msize = dv.getUint32(o + 4, true);
      let end = o + 8;
      while (end < o + ENTRY && header[end] !== 0) end++;
      const name = String.fromCharCode(...header.subarray(o + 8, end));
      if (offset < HEADER || offset > size) throw Object.assign(new Error(`the saved game is damaged: ${name} starts outside the file`), { key: 'the saved game is damaged: {name} starts outside the file', vars: { name } });
      this.members.push({ index: k, offset, size: msize, name, raw: null, data: null, changed: false });
    }
    // Each member's raw bytes run to the next member (or the end of the file): a view of the bytes, or a slice of the
    // Blob until load reads it.
    const order = [...this.members].sort((a, b) => a.offset - b.offset);
    const cut = source instanceof Uint8Array ? (a, b) => source.subarray(a, b) : (a, b) => source.slice(a, b);
    order.forEach((m, i) => { m.raw = cut(m.offset, i + 1 < order.length ? order[i + 1].offset : size); });
  }

  member(name) {
    const m = this.members.find((x) => x.name.toLowerCase() === name.toLowerCase());
    if (!m) throw Object.assign(new Error(`the saved game has no ${name}`), { key: 'the saved game has no {name}', vars: { name } });
    return m;
  }

  has(name) { return this.members.some((x) => x.name.toLowerCase() === name.toLowerCase()); }

  // The bytes of these members, from the Blob (SaveFile.open), before read.
  async load(names) {
    for (const n of names) {
      if (!this.has(n)) continue;
      const m = this.member(n);
      if (!(m.raw instanceof Uint8Array)) m.raw = new Uint8Array(await m.raw.arrayBuffer());
    }
  }

  // The member's bytes, decoded once; the array is the editor's to change (put marks it changed).
  read(name) {
    const m = this.member(name);
    if (!(m.raw instanceof Uint8Array)) throw Object.assign(new Error(`${name} of the saved game was not read`), { key: '{name} of the saved game was not read', vars: { name } });
    if (!m.data) m.data = this.compressed ? decodeRuns(m.raw, m.size).data : m.raw.slice(0, m.size);
    return m.data;
  }

  changed(name) { this.member(name).changed = true; }

  // The header with the new offsets, and the members in their order: a member that did not change keeps its bytes
  // (or its slice of the Blob) as they were.
  _parts() {
    const header = this.header.slice(0, HEADER), parts = [];
    const dv = new DataView(header.buffer);
    let at = HEADER;
    for (const m of [...this.members].sort((a, b) => a.offset - b.offset)) {
      const raw = m.changed ? (this.compressed ? encodeRuns(m.data) : m.data) : m.raw;
      dv.setUint32(12 + m.index * ENTRY, at, true);
      if (m.changed) dv.setUint32(16 + m.index * ENTRY, m.data.length, true);   // a member may change its size (shortlist.js)
      parts.push(raw);
      at += raw.length ?? raw.size;
    }
    return { header, parts, size: at };
  }

  // The whole file as bytes (a file from bytes).
  toBytes() {
    const { header, parts, size } = this._parts();
    if (parts.some((p) => !(p instanceof Uint8Array))) throw Object.assign(new Error('a saved game read from a Blob is written with toBlob'), { key: 'a saved game read from a Blob is written with toBlob' });
    const out = new Uint8Array(size);
    out.set(header, 0);
    let o = HEADER;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }

  // The whole file as a Blob: the members that did not change go as slices of the file, not through memory.
  toBlob() {
    const { header, parts } = this._parts();
    return new Blob([header, ...parts]);
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

// ---- A manager's attributes (docs/research/2026-11-26-manager-attributes.md) ----

// The twelve the game shows for a non-player (0x543b10 names them, 0x543fd0 rates them): from the staff row ('staff')
// or the non-player record ('np'), the byte as it is ('raw') or relative to the non-player's current ability ('rel').
export const MANAGER_ATTRIBUTES = [
  ['Adaptability', 'staff', 0x56, 'raw', 'Mental'], ['Coaching Goalkeepers', 'np', 0x11, 'rel', 'Coaching'],
  ['Coaching Outfield Players', 'np', 0x10, 'rel', 'Coaching'], ['Determination', 'staff', 0x58, 'raw', 'Mental'],
  ['Judging Player Ability', 'np', 0x17, 'rel', 'Knowledge'], ['Judging Player Potential', 'np', 0x18, 'rel', 'Knowledge'],
  ['Level of Discipline', 'np', 0x14, 'raw', 'Mental'], ['Man Management', 'np', 0x19, 'rel', 'Mental'],
  ['Motivating', 'np', 0x1b, 'rel', 'Mental'], ['Physiotherapy', 'np', 0x1e, 'rel', 'Coaching'],
  ['Tactical Knowledge', 'np', 0x21, 'rel', 'Knowledge'], ['Working With Youngsters', 'np', 0x22, 'raw', 'Coaching'],
].map(([name, from, off, kind, group]) => ({ name, from, off, kind, group }));
// The bytes the game keeps hidden (docs/research/2026-11-26-manager-editor-fields.md), raw 1..20: the playing style
// the staff profile puts in words (0x87ba8b..), and a chairman's (0x87b1eb..).
export const MANAGER_HIDDEN = [
  ['Attacking', 0x0e, 'Style'], ['Directness', 0x13, 'Style'], ['Free Roles', 0x15, 'Style'], ['Marking', 0x1a, 'Style'],
  ['Offsides', 0x1c, 'Style'], ['Pressing', 0x1f, 'Style'], ['Coaching Technique', 0x12, 'Style'],
  ['Business', 0x0f, 'Chairman'], ['Interference', 0x16, 'Chairman'], ['Patience', 0x1d, 'Chairman'], ['Resources', 0x20, 'Chairman'],
].map(([name, off, group]) => ({ name, off, group }));
// The preferred formation (+0x43, shown at 0x87b90d): 0 none, then these.
export const FORMATIONS = ['5-3-2 Sweeper', '5-3-2', '4-4-2', '4-3-3', '4-2-4', '3-5-2', '4-1-2-1-2', '4-5-1', '3-4-3'];
const NON_PLAYER_REPS = [['homeRep', 0x08], ['currentRep', 0x0a], ['worldRep', 0x0c]];   // int16 0..10000 (0x540810..)
const NON_PLAYER_SIZE = 68, NON_PLAYER_LINK = 0x69;   // a save's records (nonplayer.dat); a staff row's link to one

// A non-player's attributes in a SaveGame whose member nonplayer.dat is loaded: { ca, pa, attrs: [{ name, group,
// value }] }, or null for a staff member with no non-player record (a player).
export function managerAttributes(game, staffId) {
  const s = game.staff;
  if (staffId < 0 || staffId >= s.count || !game.file.has('nonplayer.dat')) return null;
  const o = staffId * STAFF_SIZE, k = s.dv.getInt32(o + NON_PLAYER_LINK, true), np = game.file.read('nonplayer.dat');
  const q = k * NON_PLAYER_SIZE;
  if (k < 0 || q + NON_PLAYER_SIZE > np.length) return null;
  const dv = new DataView(np.buffer, np.byteOffset, np.byteLength), ca = dv.getInt16(q + 4, true);
  const attrs = MANAGER_ATTRIBUTES.map((a) => {
    const b = a.from === 'staff' ? s.b[o + a.off] : np[q + a.off];
    return { name: a.name, group: a.group, value: a.kind === 'rel' ? rating((b << 24) >> 24, ca) : b };
  });
  return { ca, pa: dv.getInt16(q + 6, true), attrs, hidden: MANAGER_HIDDEN.map((a) => ({ name: a.name, group: a.group, value: np[q + a.off] })),
    ...Object.fromEntries(NON_PLAYER_REPS.map(([k, off]) => [k, dv.getInt16(q + off, true)])), formation: np[q + 0x43] };
}

// The jobs of a staff row (+0x3d), as `database_get_job_name` 0x53ad0f names them (jump table 0x53b384; 14 none).
export const STAFF_JOBS = ['Unemployed', 'Chairman', 'Managing Director', 'General Manager', 'Director of Football', 'Manager',
  'Assistant Manager', 'Reserve Manager', 'Coach', 'Scout', 'Physio', 'Player', 'Player/Manager', 'Player/Assistant Manager',
  undefined, 'Player/Coach', 'Retired Player', 'Media Pundit'];

// Every staff member with a non-player record (managers, coaches, scouts and the rest): { id, name, key (to search),
// club, job (STAFF_JOBS), ca (the record's current ability) }, read once for a SaveGame whose nonplayer.dat is loaded.
const nonPlayerLists = new WeakMap();
export function nonPlayers(game) {
  if (nonPlayerLists.has(game)) return nonPlayerLists.get(game);
  const s = game.staff, out = [];
  const np = game.file.has('nonplayer.dat') ? game.file.read('nonplayer.dat') : new Uint8Array(0);
  const records = np.length / NON_PLAYER_SIZE, npv = new DataView(np.buffer, np.byteOffset, np.byteLength);
  for (let i = 0; i < s.count; i++) {
    const o = i * STAFF_SIZE, k = s.dv.getInt32(o + NON_PLAYER_LINK, true);
    if (k < 0 || k >= records) continue;
    const name = (at, list) => { const id = s.dv.getInt32(o + at, true); return id >= 0 && id < list.length ? list[id] : ''; };
    const common = name(0x0c, game.commonNames), both = `${name(0x04, game.firstNames)} ${name(0x08, game.secondNames)}`.trim();
    if (!common && !both) continue;
    out.push({ id: i, name: common || both, key: fold(`${both} ${common}`), club: game.clubs.get(s.dv.getInt32(o + 0x39, true)) ?? '',
      job: s.b[o + 0x3d], ca: npv.getInt16(k * NON_PLAYER_SIZE + 4, true) });
  }
  nonPlayerLists.set(game, out);
  return out;
}

// Writes a non-player's ability (ca, pa: the record's +0x04 and +0x06) and his twelve attributes as the game shows them
// (values: { name: 1..20 }): an attribute as it is gets its byte, a relative one the byte that shows that number at
// the new ability (storedFor), so a change of ability keeps the numbers. Marks staff.dat and nonplayer.dat changed.
// Also { hidden: { name: 1..20 }, homeRep, currentRep, worldRep (0..10000), formation (0..9) }. Only a value that
// differs from what the game shows now is written, so a byte out of range that is not changed keeps its value.
export function writeManagerAttributes(game, staffId, { ca, pa, values, hidden = {}, formation, ...reps }) {
  const s = game.staff, o = staffId * STAFF_SIZE, k = s.dv.getInt32(o + NON_PLAYER_LINK, true);
  const np = game.file.read('nonplayer.dat'), q = k * NON_PLAYER_SIZE;
  if (k < 0 || q + NON_PLAYER_SIZE > np.length) throw Object.assign(new Error('no non-player record'), { key: 'no non-player record' });
  const dv = new DataView(np.buffer, np.byteOffset, np.byteLength);
  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, Math.round(Number(v)) || lo));
  if (formation !== undefined && !(Number.isInteger(Number(formation)) && formation >= 0 && formation <= FORMATIONS.length))
    throw Object.assign(new Error('no such formation'), { key: 'no such formation' });
  const was = managerAttributes(game, staffId), shownNow = Object.fromEntries(was.attrs.map((x) => [x.name, x.value]));
  const a = clamp(ca, 1, 200);
  if (Number(ca) !== was.ca) dv.setInt16(q + 4, a, true);
  if (Number(pa) !== was.pa) dv.setInt16(q + 6, clamp(pa, 1, 200), true);
  for (const attr of MANAGER_ATTRIBUTES) {
    if (!(attr.name in values)) continue;
    // A relative one shows another number at a new ability: it is written again then, to keep the number shown.
    if (Number(values[attr.name]) === shownNow[attr.name] && (attr.kind !== 'rel' || a === was.ca)) continue;
    const byte = storedFor({ kind: attr.kind }, clamp(values[attr.name], 1, 20), a).byte;
    if (attr.from === 'staff') s.b[o + attr.off] = byte; else np[q + attr.off] = byte;
  }
  for (const h of MANAGER_HIDDEN) {
    const v = hidden[h.name];
    if (v !== undefined && Number(v) !== np[q + h.off]) np[q + h.off] = clamp(v, 1, 20);
  }
  for (const [k, off] of NON_PLAYER_REPS) if (reps[k] !== undefined && Number(reps[k]) !== was[k]) dv.setInt16(q + off, clamp(reps[k], 0, 10000), true);
  if (formation !== undefined) np[q + 0x43] = Number(formation);
  game.file.changed('nonplayer.dat');
  game.file.changed(s.name);
}

// ---- A club's finances (docs/research/2026-11-26-club-finances.md) ----

const FINANCE_SIZE = 0x167;                     // FinanceTable::save_finance_dat 0x59b3b0: 359 bytes a club
// A year block (0x50 bytes): the fields the Finance functions add to (players bought and sold are 8 bytes).
const FINANCE_FIELDS = { bought: [0x00, 8], sold: [0x08, 8], otherCosts: [0x10, 4], fines: [0x18, 4], wages: [0x20, 4],
  bonuses: [0x28, 4], signOn: [0x2c, 4], prize: [0x38, 4], tv: [0x48, 4] };

// A club's finances in a SaveGame whose member finance.dat is loaded: { balance, wageBill, topWage, thisYear,
// lastYear } (a year: FINANCE_FIELDS' names), or null (no such club, no finance.dat).
export function clubFinances(game, clubId) {
  if (!game.file.has('finance.dat')) return null;
  const f = game.file.read('finance.dat'), q = clubId * FINANCE_SIZE;
  if (clubId < 0 || q + FINANCE_SIZE > f.length) return null;
  const dv = new DataView(f.buffer, f.byteOffset, f.byteLength);
  if (dv.getInt32(q + 0x08, true) !== clubId) return null;
  const year = (at) => Object.fromEntries(Object.entries(FINANCE_FIELDS).map(([k, [o, n]]) =>
    [k, n === 8 ? Number(dv.getBigInt64(q + at + o, true)) : dv.getInt32(q + at + o, true)]));
  return { balance: Number(dv.getBigInt64(q, true)), wageBill: dv.getInt32(q + 0x0c, true), topWage: dv.getInt32(q + 0x10, true),
    wageLimit: dv.getInt32(q + 0x14, true), bankrupt: f[q + 0x165],       // docs/research/2026-11-26-club-editor-fields.md
    thisYear: year(0x110), lastYear: year(0xc0) };
}

// ---- A club as Squad Hub edits it (docs/research/2026-11-26-club-editor-fields.md): club.dat or nat_club.dat
// (581 bytes), finance.dat (359), stadium.dat (78), city.dat (56), colour.dat (58). A record's id is its row; a link is
// a 4-byte id, -1 for none. ----

const KIT = ['homeText', 'homeBack', 'awayText', 'awayBack', 'thirdText', 'thirdBack'];   // club +0x83.. (0x53b640)
const STADIUM_SIZE = 78, CITY_SIZE = 56, COLOUR_SIZE = 58;
export const CLUB_MATCH_DAYS = [0, 1, 2, 3, 4, 5, 6];       // Monday .. Sunday (0x54a3f0); 255: none
export const CLUB_STATUS = [1, 2, 3];                       // Professional, Semi-Professional, Amateur (0x460ec6)

function clubRow(game, clubId) {
  for (const t of game.clubTables) {
    const first = t.count ? t.dv.getInt32(0, true) : 0, i = clubId - first;       // the ids follow the rows
    if (i >= 0 && i < t.count && t.dv.getInt32(i * CLUB_SIZE, true) === clubId) return { t, o: i * CLUB_SIZE };
  }
  return null;
}
// A member's record of `size` bytes by id (= row), or null: { b, dv, o }.
function rowOf(game, name, size, id) {
  if (id < 0 || !game.file.has(name)) return null;
  const b = game.file.read(name);
  if ((id + 1) * size > b.length) return null;
  return { b, dv: new DataView(b.buffer, b.byteOffset, b.byteLength), o: id * size };
}
const ids = (dv, o, n) => Array.from({ length: n }, (_, k) => dv.getInt32(o + 4 * k, true));

// Everything Squad Hub shows of a club, or null (no such club). { colours: true } adds colour.dat's list
// [{ id, name, rgb }]. The finance fields are absent when the save keeps no finances for the club; stadium and its
// city are null when the club names none.
// A club's nation name (club +0x53) from its record alone, '' when none (for labels: no other member needed).
export function clubNationName(game, clubId) {
  const r = clubRow(game, clubId);
  return r ? game.nations.get(r.t.dv.getInt32(r.o + 0x53, true)) ?? '' : '';
}
export function clubInfo(game, clubId, { colours = false } = {}) {
  const r = clubRow(game, clubId);
  if (!r) return null;
  const { t, o } = r, dv = t.dv, b = t.b;
  const st = rowOf(game, 'stadium.dat', STADIUM_SIZE, dv.getInt32(o + 0x69, true));
  const city = st && rowOf(game, 'city.dat', CITY_SIZE, st.dv.getInt32(st.o + 0x38, true));
  const f = clubFinances(game, clubId);
  const out = { id: clubId, name: latin1(b, o + 4, 51), nation: game.nations.get(dv.getInt32(o + 0x53, true)) ?? '',
    status: b[o + 0x64], training: b[o + 0x7f], reputation: dv.getInt16(o + 0x80, true), plc: b[o + 0x82] !== 0,
    linked: b[o + 0x244] !== 0, euroSeeding: dv.getInt8(o + 0x1df), attendance: dv.getInt32(o + 0x73, true),
    minAttendance: dv.getInt32(o + 0x77, true), maxAttendance: dv.getInt32(o + 0x7b, true), matchDay: b[o + 0x72],
    owns: b[o + 0x6d] !== 0, favourites: ids(dv, o + 0x9b, 3), disliked: ids(dv, o + 0xa7, 3), rivals: ids(dv, o + 0xb3, 3),
    ...(f ? { balance: f.balance, topWage: f.topWage, wageLimit: f.wageLimit, bankrupt: f.bankrupt } : {}),
    kit: Object.fromEntries(KIT.map((k, n) => [k, dv.getInt32(o + 0x83 + 4 * n, true)])),
    stadium: st ? { id: dv.getInt32(o + 0x69, true), name: latin1(st.b, st.o + 4, 51), capacity: st.dv.getInt32(st.o + 0x3c, true),
      seating: st.dv.getInt32(st.o + 0x40, true), expansion: st.dv.getInt32(st.o + 0x44, true), roof: st.b[st.o + 0x4c] !== 0,
      heating: st.b[st.o + 0x4d] !== 0,
      city: city ? { id: st.dv.getInt32(st.o + 0x38, true), name: latin1(city.b, city.o + 4, 26), attraction: city.dv.getInt8(city.o + 0x33) } : null } : null };
  if (colours) {
    const c = game.file.has('colour.dat') ? game.file.read('colour.dat') : new Uint8Array(0);
    out.colours = Array.from({ length: Math.floor(c.length / COLOUR_SIZE) }, (_, k) => ({ id: k, name: latin1(c, k * COLOUR_SIZE + 4, 51),
      rgb: [c[k * COLOUR_SIZE + 0x37], c[k * COLOUR_SIZE + 0x38], c[k * COLOUR_SIZE + 0x39]] }));
  }
  return out;
}

// Changes a club: each field of `patch` given is written, cut to the game's range. clubInfo's names: status (1..3),
// training (1..20), reputation (1..10000), plc, euroSeeding (-1..127), attendance, minAttendance, maxAttendance
// (0..250000), matchDay (0..6, 255 none), owns, favourites, disliked (3 staff ids), rivals (3 club ids), kit ({ homeText,
// ... }: colour ids), balance (-2e9..2e9, a signed 32-bit cash figure: 0x5986e3), wageLimit, bankrupt (0..2); of the
// club's stadium (shared by the clubs that name it): capacity, seating, expansion, roof, heating; of its city (shared):
// attraction (0..20). An id that names nothing is an error, and nothing is written.
export function writeClub(game, clubId, patch = {}) {
  const r = clubRow(game, clubId);
  const fail = ({ key }) => { throw Object.assign(new Error(key), { key }); };   // key: the English, a text of the tables
  if (!r) fail({ key: 'no such club' });
  const { t, o } = r, dv = t.dv, b = t.b;
  const cut = (v, lo, hi) => Math.min(hi, Math.max(lo, Math.round(Number(v) || 0)));
  const p = patch, has = (k) => p[k] !== undefined;
  // Checks first, so a bad id writes nothing.
  if (has('matchDay') && !CLUB_MATCH_DAYS.includes(Number(p.matchDay)) && Number(p.matchDay) !== 255) fail({ key: 'no such match day' });
  if (has('status') && !CLUB_STATUS.includes(Number(p.status))) fail({ key: 'no such professional status' });
  for (const k of ['favourites', 'disliked']) if (has(k) && p[k].some((id) => id !== -1 && !(id >= 0 && id < game.staff.count))) fail({ key: 'no such person' });
  if (has('rivals') && p.rivals.some((id) => id !== -1 && (id === clubId || !clubRow(game, id)))) fail({ key: 'no such club' });
  const nColours = game.file.has('colour.dat') ? Math.floor(game.file.read('colour.dat').length / COLOUR_SIZE) : 0;
  if (has('kit') && Object.entries(p.kit).some(([k, id]) => !KIT.includes(k) || (id !== -1 && !(id >= 0 && id < nColours)))) fail({ key: 'no such colour' });
  const fin = ['balance', 'wageLimit', 'bankrupt'].some(has);
  if (fin && !clubFinances(game, clubId)) fail({ key: 'the game keeps no finances for this club' });
  const st = rowOf(game, 'stadium.dat', STADIUM_SIZE, dv.getInt32(o + 0x69, true));
  if (['capacity', 'seating', 'expansion', 'roof', 'heating', 'attraction'].some(has) && !st) fail({ key: 'the club has no stadium' });
  const city = st && rowOf(game, 'city.dat', CITY_SIZE, st.dv.getInt32(st.o + 0x38, true));
  if (has('attraction') && !city) fail({ key: 'the stadium has no city' });

  let club = false;
  const byte = (off, v) => { b[o + off] = v; club = true; };
  const int = (off, v) => { dv.setInt32(o + off, v, true); club = true; };
  if (has('status')) byte(0x64, Number(p.status));
  if (has('training')) byte(0x7f, cut(p.training, 1, 20));
  if (has('reputation')) { dv.setInt16(o + 0x80, cut(p.reputation, 1, 10000), true); club = true; }
  if (has('plc')) byte(0x82, p.plc ? 1 : 0);
  if (has('euroSeeding')) { dv.setInt8(o + 0x1df, cut(p.euroSeeding, -1, 127)); club = true; }
  if (has('owns')) byte(0x6d, p.owns ? 1 : 0);
  if (has('matchDay')) byte(0x72, Number(p.matchDay));
  for (const [k, off] of [['attendance', 0x73], ['minAttendance', 0x77], ['maxAttendance', 0x7b]]) if (has(k)) int(off, cut(p[k], 0, 250000));
  for (const [k, off] of [['favourites', 0x9b], ['disliked', 0xa7], ['rivals', 0xb3]]) if (has(k)) p[k].slice(0, 3).forEach((id, n) => int(off + 4 * n, id));
  if (has('kit')) for (const [k, id] of Object.entries(p.kit)) int(0x83 + 4 * KIT.indexOf(k), id);
  if (club) game.file.changed(t.name);
  if (fin) {
    const f = rowOf(game, 'finance.dat', FINANCE_SIZE, clubId);
    if (has('balance')) f.dv.setBigInt64(f.o, BigInt(cut(p.balance, -2000000000, 2000000000)), true);
    if (has('wageLimit')) f.dv.setInt32(f.o + 0x14, cut(p.wageLimit, 0, 10000000), true);
    if (has('bankrupt')) f.b[f.o + 0x165] = cut(p.bankrupt, 0, 2);
    game.file.changed('finance.dat');
  }
  if (['capacity', 'seating', 'expansion', 'roof', 'heating'].some(has)) {
    for (const [k, off] of [['capacity', 0x3c], ['seating', 0x40], ['expansion', 0x44]]) if (has(k)) st.dv.setInt32(st.o + off, cut(p[k], 0, 250000), true);
    if (has('roof')) st.b[st.o + 0x4c] = p.roof ? 1 : 0;
    if (has('heating')) st.b[st.o + 0x4d] = p.heating ? 1 : 0;
    game.file.changed('stadium.dat');
  }
  if (has('attraction')) { city.dv.setInt8(city.o + 0x33, cut(p.attraction, 0, 20)); game.file.changed('city.dat'); }
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

// The tables of index.dat: id -> { file, count }.
function tablesOf(index) {
  if (index.length < 8 + INDEX_ENTRIES * INDEX_ENTRY) throw Object.assign(new Error('the saved game has no table index'), { key: 'the saved game has no table index' });
  const tables = {};
  const dv = new DataView(index.buffer, index.byteOffset, index.byteLength);
  for (let k = 0; k < INDEX_ENTRIES; k++) {
    const o = 8 + k * INDEX_ENTRY;
    tables[dv.getInt32(o + 0x33, true)] = { file: latin1(index, o, 0x33), count: dv.getInt32(o + 0x37, true) };
  }
  return tables;
}
const TABLES = [6, 10, 13, 14, 15, 0, 1, 4];       // staff, player, the three name tables, clubs, national clubs, nations

// ---- A player as the GK Save Game Editor shows him (docs/research/2026-11-26-player-editor-fields.md) ----

// The personality of the staff row (signed bytes 1..20); Determination (+0x58) is among ATTRIBUTES.
export const PERSONALITY = [['Adaptability', 0x56], ['Ambition', 0x57], ['Loyalty', 0x59], ['Pressure', 0x5a],
  ['Professionalism', 0x5b], ['Sportsmanship', 0x5c], ['Temperament', 0x5d]];
// contract.dat (0x4dc3a0): int n + n talks of 21 bytes, int n + n contracts of 80 bytes (+0 staff id, +4 club), then
// the rest. The contract of each staff id, the one with his club when he has two. Read once for a SaveGame.
const CONTRACT_SIZE = 80;
const contractLists = new WeakMap();
function contractsOf(game) {
  if (contractLists.has(game)) return contractLists.get(game);
  let out = null;
  if (game.file.has('contract.dat')) {
    const b = game.file.read('contract.dat'), dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
    const start = 4 + dv.getInt32(0, true) * 21 + 4, n = dv.getInt32(start - 4, true), at = new Map();
    if (start + n * CONTRACT_SIZE <= b.length) {
      for (let k = 0; k < n; k++) {
        const o = start + k * CONTRACT_SIZE, id = dv.getInt32(o, true);
        const club = id >= 0 && id < game.staff.count ? game.staff.dv.getInt32(id * STAFF_SIZE + 0x39, true) : -2;
        if (!at.has(id) || dv.getInt32(o + 4, true) === club) at.set(id, o);
      }
      out = { b, dv, at };
    }
  }
  contractLists.set(game, out);
  return out;
}
// A contract's bonuses (int32, -1 none), its transfer bits (+0x4e: 0x5444f2, 0x8f6923) and its clause flags (+0x1c..).
const BONUSES = [['goalBonus', 0x10], ['assistBonus', 0x14], ['cleanSheetBonus', 0x18]];
const LIST_BITS = { listed: 1, loanListed: 2, listedByRequest: 8 };
const CLAUSES = [['nonPromotion', 0x1c], ['minFee', 0x1d], ['nonPlaying', 0x1e], ['relegation', 0x1f], ['managerJob', 0x20]];
// preferences.dat (52 bytes, the staff row's +0x65; docs/research/2026-11-26-player-editor-fields.md, 0x547d77..):
// +0 the id, then three club ids each of favourite and disliked clubs, then three staff ids each of favourite and
// disliked staff, -1 for none.
const PREFS_SIZE = 52, PREFS = [['favClubs', 0x04, 'club'], ['dislikedClubs', 0x10, 'club'], ['favStaff', 0x1c, 'staff'], ['dislikedStaff', 0x28, 'staff']];
function prefsRow(game, row) {
  const k = game.staff.dv.getInt32(row * STAFF_SIZE + 0x65, true);
  if (k < 0 || !game.file.has('preferences.dat')) return null;
  const b = game.file.read('preferences.dat');
  if ((k + 1) * PREFS_SIZE > b.length) return null;
  return { b, dv: new DataView(b.buffer, b.byteOffset, b.byteLength), o: k * PREFS_SIZE };
}
const SQUAD_BITS = { clubMain: 1, clubReserve: 2, nationU21: 4, nationMain: 8 };   // staff +0x5e (0x7b0f30)

export class SaveGame {
  // bytes: the whole file, or a SaveFile (SaveGame.open).
  constructor(bytes) {
    this.file = bytes instanceof SaveFile ? bytes : new SaveFile(bytes);
    this.tables = tablesOf(this.file.read('index.dat'));
    const table = (id, size) => {
      const t = this.tables[id];
      if (!t || !this.file.has(t.file)) throw Object.assign(new Error(`the saved game has no table ${id}`), { key: 'the saved game has no table {id}', vars: { id } });
      const b = this.file.read(t.file);
      if (b.length !== t.count * size) throw Object.assign(new Error(`${t.file} holds ${b.length} bytes, not ${t.count} rows of ${size}`), { key: '{file} holds {bytes} bytes, not {rows} rows of {size}', vars: { file: t.file, bytes: b.length, rows: t.count, size } });

      return { name: t.file, b, count: t.count, dv: new DataView(b.buffer, b.byteOffset, b.byteLength) };
    };
    this.staff = table(6, STAFF_SIZE);
    this.player = table(10, PLAYER_SIZE);
    const names = (id) => { const t = table(id, NAME_SIZE); return Array.from({ length: t.count }, (_, i) => latin1(t.b, i * NAME_SIZE, 51)); };
    this.firstNames = names(13);
    this.secondNames = names(14);
    this.commonNames = names(15);
    this.clubs = new Map();
    this.clubTables = [0, 1].map((id) => table(id, CLUB_SIZE));
    for (const t of this.clubTables) {
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
      // The list shows the common name when there is one; the search finds him by every name he has.
      const common = name(0x0c, this.commonNames), both = `${name(0x04, this.firstNames)} ${name(0x08, this.secondNames)}`.trim();
      const full = common || both;
      const po = d * PLAYER_SIZE;
      const born = readDate(s.b, o + 0x10);
      out.push({ row: i, data: d, name: full || '(no name)', key: fold(`${both} ${common}`), club: this.clubs.get(s.dv.getInt32(o + 0x39, true)) ?? '',
        nation: this.nations.get(s.dv.getInt32(o + 0x1a, true)) ?? '', born: born?.year ?? '',
        ca: p.dv.getInt16(po + 5, true), pa: p.dv.getInt16(po + 7, true), pos: positionText(p.b.subarray(po + 0x0f, po + 0x1b)) });
    }
    this.list = out;
    return out;
  }

  // Players whose name has every word of the text (accents and case aside), the best known first.
  // The nations of the players, by name, sorted (the Nationality filter; nations is the id map).
  nationNames() { return [...new Set(this.players().map((x) => x.nation).filter(Boolean))].sort((a, b) => a.localeCompare(b)); }

  // nation: only the players of that nation (its name as nations() gives it); '' every nation.
  search(text, limit = 60, nation = '') {
    const words = fold(text).split(/\s+/).filter(Boolean);
    const all = nation ? this.players().filter((x) => x.nation === nation) : this.players();
    const hit = words.length ? all.filter((x) => words.every((w) => x.key.includes(w) || fold(x.club).includes(w))) : all;
    // A player whose name as the list shows it has the words comes first ("Mendonca": Mendonça before Marcelo Mattos,
    // who is Marcelo Mendonça de Mattos), then the best known.
    const shown = new Set(hit.filter((x) => words.every((w) => fold(x.name).includes(w))));
    return [...hit].sort((a, b) => shown.has(b) - shown.has(a) || b.ca - a.ca).slice(0, limit);
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
      born: readDate(s.b, o + 0x10), age: s.dv.getUint16(o + 0x18, true), more: this.readMore(row) };
  }

  // What else the GK editor shows of a player: { nation, nation2 (-1 none), caps, goals, value, clubMain, clubReserve,
  // nationU21, nationMain, euro (the registration state, +0x6d), morale, homeRep, currentRep, worldRep, personality
  // { name: value }, contract: { wage, start, end, number, status (squad status 0..7), goalBonus, assistBonus,
  // cleanSheetBonus (0: none), listed, loanListed, listedByRequest, nonPromotion, minFee, nonPlaying, relegation,
  // managerJob (the release clauses), fee (the release fee) } or null }.
  // A staff member with no player record (a manager, a coach) has only the staff row's and the contract's fields.
  readMore(row) {
    const s = this.staff, p = this.player, o = row * STAFF_SIZE, d = s.dv.getInt32(o + 0x61, true), po = d * PLAYER_SIZE;
    const c = contractsOf(this), q = c?.at.get(row), player = d >= 0 && d < p.count;
    return { nation: s.dv.getInt32(o + 0x1a, true), nation2: s.dv.getInt32(o + 0x1e, true), caps: s.b[o + 0x22], goals: s.b[o + 0x23],
      ...(player ? { value: s.dv.getInt32(o + 0x52, true), ...Object.fromEntries(Object.entries(SQUAD_BITS).map(([k, bit]) => [k, (s.b[o + 0x5e] & bit) !== 0])),
        euro: s.b[o + 0x6d], morale: p.b[po + 0x45], homeRep: p.dv.getInt16(po + 0x09, true), currentRep: p.dv.getInt16(po + 0x0b, true),
        worldRep: p.dv.getInt16(po + 0x0d, true) } : {}),
      prefs: (() => { const r = prefsRow(this, row); return r && Object.fromEntries(PREFS.map(([k, off]) => [k, [0, 1, 2].map((n) => r.dv.getInt32(r.o + off + 4 * n, true))])); })(),
      personality: Object.fromEntries(PERSONALITY.map(([n, off]) => [n, s.dv.getInt8(o + off)])),
      contract: q === undefined ? null : { wage: c.dv.getInt32(q + 0x0c, true), start: readDate(c.b, q + 0x25), end: readDate(c.b, q + 0x2d),
        number: Math.abs(c.dv.getInt8(q + 0x3a)), status: c.b[q + 0x4f] >> 4,
        ...Object.fromEntries(BONUSES.map(([k, off]) => [k, Math.max(0, c.dv.getInt32(q + off, true))])),   // -1: none, shown 0
        ...Object.fromEntries(Object.entries(LIST_BITS).map(([k, bit]) => [k, (c.b[q + 0x4e] & bit) !== 0])),
        ...Object.fromEntries(CLAUSES.map(([k, off]) => [k, c.b[q + off] !== 0])), fee: c.dv.getInt32(q + 0x21, true) } };
  }

  // Writes what readMore gives, changed: only the fields that differ, as the game keeps them (the doc's "Must be changed
  // together": the wage and the contract end in the contract and in the staff row; a new nation leaves the national
  // team; the squad number keeps its sign; the squad status its low nibble). The nation squads are the game's.
  writeMore(row, m) {
    const s = this.staff, p = this.player, o = row * STAFF_SIZE, po = s.dv.getInt32(o + 0x61, true) * PLAYER_SIZE;
    const was = this.readMore(row), cut = (v, lo, hi) => Math.min(hi, Math.max(lo, Math.round(Number(v) || 0)));
    const differs = (k) => m[k] !== undefined && was[k] !== undefined && m[k] !== was[k];   // a field this staff member has
    for (const k of ['nation', 'nation2']) {
      if (!differs(k)) continue;
      const id = Number(m[k]);
      if (!(this.nations.has(id) || (k === 'nation2' && id === -1))) throw Object.assign(new Error('no such nation'), { key: 'no such nation' });
      s.dv.setInt32(o + (k === 'nation' ? 0x1a : 0x1e), id, true);
      if (k === 'nation') { s.dv.setInt32(o + 0x24, -1, true); s.b[o + 0x28] = 0; s.b[o + 0x5e] &= ~(4 | 8); }
    }
    if (differs('caps')) s.b[o + 0x22] = cut(m.caps, 0, 255);
    if (differs('goals')) s.b[o + 0x23] = cut(m.goals, 0, 255);
    if (differs('value')) s.dv.setInt32(o + 0x52, cut(m.value, 0, 2000000000), true);
    for (const k of ['clubMain', 'clubReserve']) if (differs(k)) s.b[o + 0x5e] = m[k] ? s.b[o + 0x5e] | SQUAD_BITS[k] : s.b[o + 0x5e] & ~SQUAD_BITS[k];
    if (differs('morale')) p.b[po + 0x45] = cut(m.morale, 1, 20);
    for (const [k, off] of [['homeRep', 0x09], ['currentRep', 0x0b], ['worldRep', 0x0d]]) if (differs(k)) p.dv.setInt16(po + off, cut(m[k], 0, 10000), true);
    const pr = prefsRow(this, row);
    if (m.prefs && pr) {
      for (const [k, , kind] of PREFS) {
        const ids = m.prefs[k] ?? [];
        if (ids.some((id) => id !== -1 && !(kind === 'staff' ? id >= 0 && id < s.count : this.clubs.has(id))))
          throw Object.assign(new Error(kind === 'staff' ? 'no such person' : 'no such club'), { key: kind === 'staff' ? 'no such person' : 'no such club' });
      }
      let any = false;
      for (const [k, off] of PREFS) (m.prefs[k] ?? []).slice(0, 3).forEach((id, n) => {
        if (pr.dv.getInt32(pr.o + off + 4 * n, true) !== id) { pr.dv.setInt32(pr.o + off + 4 * n, id, true); any = true; }
      });
      if (any) this.file.changed('preferences.dat');
    }
    for (const [n, off] of PERSONALITY) if (m.personality && m.personality[n] !== undefined && m.personality[n] !== was.personality[n]) s.dv.setInt8(o + off, cut(m.personality[n], 1, 20));
    const c = contractsOf(this), q = c?.at.get(row), k = m.contract, w = was.contract;
    if (k && w && q !== undefined) {
      let any = false;
      if (k.wage !== w.wage) { const v = cut(k.wage, 0, 10000000); c.dv.setInt32(q + 0x0c, v, true); s.dv.setInt32(o + 0x4e, v, true); any = true; }
      const moved = (a, b) => a && (!b || a.year !== b.year || a.month !== b.month || a.day !== b.day);
      if (moved(k.end, w.end)) { writeDate(c.b, q + 0x2d, k.end); writeDate(s.b, o + 0x46, k.end); any = true; }
      if (moved(k.start, w.start)) { writeDate(c.b, q + 0x25, k.start); any = true; }
      for (const [f, off] of BONUSES) if (k[f] !== w[f]) { const v = cut(k[f], 0, 10000000); c.dv.setInt32(q + off, v > 0 ? v : -1, true); any = true; }
      for (const [f, off] of CLAUSES) if (k[f] !== w[f]) { c.b[q + off] = k[f] ? 1 : 0; any = true; }
      if (k.number !== w.number) { const sign = c.dv.getInt8(q + 0x3a) > 0 ? 1 : -1; c.dv.setInt8(q + 0x3a, sign * cut(k.number, 0, 50)); any = true; }
      if (k.status !== w.status) { c.b[q + 0x4f] = (cut(k.status, 0, 7) << 4) | (c.b[q + 0x4f] & 15); any = true; }
      for (const [f, bit] of Object.entries(LIST_BITS)) if (k[f] !== w[f]) { c.b[q + 0x4e] = k[f] ? c.b[q + 0x4e] | bit : c.b[q + 0x4e] & ~bit; any = true; }
      if (k.fee !== w.fee) { c.dv.setInt32(q + 0x21, cut(k.fee, 0, 2000000000), true); any = true; }
      if (any) this.file.changed('contract.dat');
    }
    this.file.changed(this.staff.name);               // called alone for a staff member with no player record
    if (was.morale !== undefined) this.file.changed(this.player.name);
    this.list = null;
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
    // In a saved game staff +0x18 is the age, not the year of birth (Del Piero, born 9.11.74, holds 26 on 14.7.01;
    // 1974 there showed "Age -74" in the game). A new year of birth moves the age by as many years.
    const was = readDate(s.b, o + 0x10);
    if (f.born && (!was || was.year !== f.born.year || was.month !== f.born.month || was.day !== f.born.day)) {
      writeDate(s.b, o + 0x10, f.born);
      if (was) s.dv.setUint16(o + 0x18, Math.max(0, s.dv.getUint16(o + 0x18, true) - (f.born.year - was.year)), true);
    }
    if (f.more) this.writeMore(row, f.more);
    this.file.changed(this.staff.name);
    this.file.changed(this.player.name);
    this.list = null;
    return missed;
  }

  // A save from a Blob (the vault's file on disk): its header, index.dat and the tables the editor uses.
  static async open(blob) {
    const file = await SaveFile.open(blob);
    await file.load(['index.dat']);
    const tables = tablesOf(file.read('index.dat'));
    await file.load([...TABLES.map((id) => tables[id]?.file).filter(Boolean), 'contract.dat', 'preferences.dat', 'nonplayer.dat']);
    return new SaveGame(file);
  }

  toBytes() { return this.file.toBytes(); }

  toBlob() { return this.file.toBlob(); }
}

const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, Math.round(Number(v)) || lo));
