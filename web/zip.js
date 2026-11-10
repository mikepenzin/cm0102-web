// Zip files for the data sets (web/README.md, "Data sets"): a data set downloads as a zip of its 20 database files,
// and a zip that holds one or more data sets uploads. Deflate through the browser's CompressionStream; no library.
// The format: PKWARE APPNOTE 6.3 (local headers, central directory, end record); no Zip64, so each file and the zip
// stay under 4 GB, which a data set does by far.

const TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; }
  return t;
})();

export function crc32(bytes) {
  let c = 0xffffffff;
  for (let i = 0; i < bytes.length; i++) c = TABLE[(c ^ bytes[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

const through = async (bytes, stream) => new Uint8Array(await new Response(new Blob([bytes]).stream().pipeThrough(stream)).arrayBuffer());
const utf8 = new TextEncoder(), fromUtf8 = new TextDecoder();

// A zip of the files: [{ name (with folders, '/'), data: Uint8Array }]. Each file deflated, or stored when deflate
// does not make it smaller. date: the file time for every entry.
export async function writeZip(files, date = new Date()) {
  const dosTime = (date.getHours() << 11) | (date.getMinutes() << 5) | (date.getSeconds() >> 1);
  const dosDate = ((date.getFullYear() - 1980) << 9) | ((date.getMonth() + 1) << 5) | date.getDate();
  const parts = [], central = [];
  let at = 0;
  for (const f of files) {
    const name = utf8.encode(f.name), crc = crc32(f.data);
    let data = await through(f.data, new CompressionStream('deflate-raw')), method = 8;
    if (data.length >= f.data.length) { data = f.data; method = 0; }
    const local = new Uint8Array(30 + name.length), lv = new DataView(local.buffer);
    lv.setUint32(0, 0x04034b50, true); lv.setUint16(4, 20, true); lv.setUint16(6, 0x0800, true); lv.setUint16(8, method, true);
    lv.setUint16(10, dosTime, true); lv.setUint16(12, dosDate, true); lv.setUint32(14, crc, true);
    lv.setUint32(18, data.length, true); lv.setUint32(22, f.data.length, true); lv.setUint16(26, name.length, true);
    local.set(name, 30);
    const c = new Uint8Array(46 + name.length), cv = new DataView(c.buffer);
    cv.setUint32(0, 0x02014b50, true); cv.setUint16(4, 20, true); cv.setUint16(6, 20, true); cv.setUint16(8, 0x0800, true);
    cv.setUint16(10, method, true); cv.setUint16(12, dosTime, true); cv.setUint16(14, dosDate, true); cv.setUint32(16, crc, true);
    cv.setUint32(20, data.length, true); cv.setUint32(24, f.data.length, true); cv.setUint16(28, name.length, true);
    cv.setUint32(42, at, true);
    c.set(name, 46);
    parts.push(local, data);
    central.push(c);
    at += local.length + data.length;
  }
  const size = central.reduce((a, c) => a + c.length, 0);
  const end = new Uint8Array(22), ev = new DataView(end.buffer);
  ev.setUint32(0, 0x06054b50, true); ev.setUint16(8, files.length, true); ev.setUint16(10, files.length, true);
  ev.setUint32(12, size, true); ev.setUint32(16, at, true);
  const out = new Uint8Array(at + size + 22);
  let o = 0;
  for (const p of [...parts, ...central, end]) { out.set(p, o); o += p.length; }
  return out;
}

// The entries of a zip: [{ name, size, method, compressed, offset }] from its central directory. Throws on a file that
// is not a zip of this kind.
export function zipEntries(zip) {
  const dv = new DataView(zip.buffer, zip.byteOffset, zip.byteLength);
  let e = -1;
  for (let i = zip.length - 22; i >= Math.max(0, zip.length - 22 - 0xffff); i--) if (dv.getUint32(i, true) === 0x06054b50) { e = i; break; }
  if (e < 0) throw new Error('it is not a zip file');
  const count = dv.getUint16(e + 10, true), start = dv.getUint32(e + 16, true);
  if (count === 0xffff || start === 0xffffffff) throw new Error('the zip is in the Zip64 format, which this page does not read');
  const out = [];
  let p = start;
  for (let k = 0; k < count; k++) {
    if (p + 46 > zip.length || dv.getUint32(p, true) !== 0x02014b50) throw new Error('the zip is damaged');
    const flags = dv.getUint16(p + 8, true), method = dv.getUint16(p + 10, true);
    const compressed = dv.getUint32(p + 20, true), size = dv.getUint32(p + 24, true);
    const n = dv.getUint16(p + 28, true), x = dv.getUint16(p + 30, true), c = dv.getUint16(p + 32, true);
    const name = fromUtf8.decode(zip.subarray(p + 46, p + 46 + n));
    if (flags & 1) throw new Error(`${name} is encrypted`);
    out.push({ name, size, method, compressed, crc: dv.getUint32(p + 16, true), offset: dv.getUint32(p + 42, true) });
    p += 46 + n + x + c;
  }
  return out;
}

// The bytes of one entry.
export async function zipRead(zip, entry) {
  const dv = new DataView(zip.buffer, zip.byteOffset, zip.byteLength), o = entry.offset;
  if (dv.getUint32(o, true) !== 0x04034b50) throw new Error(`${entry.name}: the zip is damaged`);
  const from = o + 30 + dv.getUint16(o + 26, true) + dv.getUint16(o + 28, true);
  const raw = zip.subarray(from, from + entry.compressed);
  let data;
  if (entry.method === 0) data = raw.slice();
  else if (entry.method === 8) {
    try { data = await through(raw, new DecompressionStream('deflate-raw')); }
    catch { throw new Error(`${entry.name} is damaged: it does not unpack`); }
  }
  else throw new Error(`${entry.name} is packed in a way this page does not read (method ${entry.method})`);
  if (data.length !== entry.size) throw new Error(`${entry.name}: ${data.length} of ${entry.size} bytes`);
  if (crc32(data) !== entry.crc) throw new Error(`${entry.name} is damaged: its checksum does not match`);
  return data;
}
