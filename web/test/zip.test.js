// Tests of the zip made of Blobs (web/zip.js, zipOfBlobs: Saved games, Download all). Run: cd web && npm test
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { crc32, crc32Blob, zipEntries, zipOfBlobs, zipRead } from '../zip.js';

test('zipOfBlobs: the files stored as they are, read back by zipRead, with their CRC', async () => {
  const big = new Uint8Array(200000).map((_, i) => (i * 7) & 255);
  const files = [
    { name: 'Juventus 2001-07-29.sav', blob: new Blob([big]) },
    { name: 'backups/Juventus 2001-07-29.1.sav', blob: new Blob([new Uint8Array([1, 2, 3])]) },
    { name: 'Databases/My edit/index.dat', blob: new Blob([]) },
  ];
  let seen = 0;
  const zip = await zipOfBlobs(files, { onBytes: (n) => { seen += n; } });
  assert.equal(seen, big.length + 3, 'every byte read once for the CRC');
  const bytes = new Uint8Array(await zip.arrayBuffer());
  const entries = zipEntries(bytes);
  assert.deepEqual(entries.map((e) => e.name), files.map((f) => f.name));
  assert.deepEqual([...await zipRead(bytes, entries[0])].slice(0, 10), [...big].slice(0, 10));
  assert.equal((await zipRead(bytes, entries[0])).length, big.length);
  assert.deepEqual([...await zipRead(bytes, entries[1])], [1, 2, 3]);
  assert.equal((await zipRead(bytes, entries[2])).length, 0);
  assert.equal(await crc32Blob(new Blob([big])), crc32(big));
});
