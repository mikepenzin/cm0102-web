// Reads the container of a saved game (.sav), as tools/save/savfile.py does. Format (zip.cpp of the original,
// read at 0x9326e0, written at 0x932b00; see savfile.py): a header of 0x416ec bytes with int version, int unknown,
// int part count and 1000 entries of 0x10c bytes (u32 offset, u32 size, char name[260]), then the parts. Versions
// 2 and 4 are compressed with a run code (decoder 0x932c30): a byte below 0x81 is itself, a byte b of 0x81 or more
// repeats the next byte b - 0x80 times.
import { openSync, readSync, closeSync, fstatSync } from 'node:fs';

export const HEADER_SIZE = 0x416ec;
const ENTRY_SIZE = 0x10c;
const MAX_ENTRIES = 1000;

export class SaveError extends Error {}

export function decode(raw, size) {
  const out = Buffer.alloc(size);
  let o = 0, i = 0;
  while (o < size && i < raw.length) {
    const b = raw[i++];
    if (b < 0x81) { out[o++] = b; continue; }
    if (i >= raw.length) break;
    const v = raw[i++];
    const n = Math.min(b - 0x80, size - o);
    out.fill(v, o, o + n);
    o += n;
  }
  if (o < size) throw new SaveError(`part ends early: ${o} of ${size} bytes`);
  return out;
}

// The parts of a save: [{name, offset, size}], with the version, without reading the parts themselves.
export function readSave(path) {
  const fd = openSync(path, 'r');
  try {
    const fileSize = fstatSync(fd).size;
    if (fileSize < HEADER_SIZE) throw new SaveError(`not a saved game: ${fileSize} bytes`);
    const header = Buffer.alloc(HEADER_SIZE);
    readSync(fd, header, 0, HEADER_SIZE, 0);
    const version = header.readInt32LE(0), count = header.readInt32LE(8);
    if (![1, 2, 3, 4].includes(version) || count < 0 || count > MAX_ENTRIES)
      throw new SaveError(`not a saved game: version ${version}, ${count} parts`);
    const parts = [];
    for (let k = 0; k < count; k++) {
      const o = 12 + k * ENTRY_SIZE;
      const nameBytes = header.subarray(o + 8, o + ENTRY_SIZE);
      const end = nameBytes.indexOf(0);
      parts.push({
        index: k,
        offset: header.readUInt32LE(o),          // unsigned: a save may pass 2 GB (tools/recomp/save_size.py)
        size: header.readUInt32LE(o + 4),
        name: nameBytes.subarray(0, end < 0 ? nameBytes.length : end).toString('latin1'),
      });
    }
    for (const p of parts)       // 0xffffffff: a part of a broken save past 2 GB (tools/recomp/save_size.py)
      if (p.offset > fileSize) throw new SaveError(`part ${p.name} starts after the end of the file`);
    return { path, version, compressed: version === 2 || version === 4, fileSize, parts };
  } finally {
    closeSync(fd);
  }
}

// The bytes of one part, decoded.
export function readPart(save, name) {
  const p = save.parts.find((x) => x.name.toLowerCase() === name.toLowerCase());
  if (!p) throw new SaveError(`no part ${name}`);
  const after = save.parts.filter((x) => x.offset > p.offset).map((x) => x.offset);
  const end = after.length ? Math.min(...after) : save.fileSize;
  const raw = Buffer.alloc(end - p.offset);
  const fd = openSync(save.path, 'r');
  try { readSync(fd, raw, 0, raw.length, p.offset); } finally { closeSync(fd); }
  return save.compressed ? decode(raw, p.size) : raw.subarray(0, p.size);
}
