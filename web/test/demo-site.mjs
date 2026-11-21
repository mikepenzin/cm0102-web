// The site of the screens review (screens.mjs): the server with the stand-in
// engine, the game's own database as a small data set (fakes.mjs), accounts with the stand-ins of Google and the store,
// and a database library of three examples. startDemoSite() → { BASE, tmp, s3, close }.
import { copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { tmpdir } from 'node:os';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createApp } from '../server.mjs';
import { createCloud } from '../cloud.mjs';
import { createS3 } from '../s3.mjs';
import { createStats, statsStore } from '../stats.mjs';
import { createLibrary } from '../library.mjs';
import { writeZip } from '../zip.js';
import { fakeDataSet, fakeGoogle, fakeS3 } from './fakes.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));

export async function startDemoSite({ adminToken } = {}) {
  // The site, as the checks make it: the game's own database as a small data set, the stand-in engine.
  const tmp = mkdtempSync(join(tmpdir(), 'cm0102-videos-'));
  const game = join(tmp, 'game'), build = join(tmp, 'build-web');
  mkdirSync(join(game, 'Data'), { recursive: true });
  mkdirSync(join(build, 'seasons'), { recursive: true });
  const exe = Buffer.from('MZ the game');
  writeFileSync(join(game, 'cm0102.exe'), exe);
  const sha = (b) => createHash('sha256').update(b).digest('hex');
  let manifest = `${sha(exe)}\t${exe.length}\tcm0102.exe\n`;
  for (const [name, bytes] of Object.entries(fakeDataSet())) {
    writeFileSync(join(game, 'Data', name), bytes);
    manifest += `${sha(bytes)}\t${bytes.length}\tData/${name}\n`;
  }
  writeFileSync(join(tmp, 'manifest.tsv'), manifest);
  for (const t of ['rules', 'names', 'clubs', 'club_records']) writeFileSync(join(build, 'seasons', `${t}.tsv`), 'x\n');
  copyFileSync(join(HERE, 'fake-emscripten.js'), join(build, 'cm0102_recomp.js'));
  writeFileSync(join(build, 'cm0102_recomp.wasm'), Buffer.from([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0]));
  writeFileSync(join(build, 'CMakeCache.txt'), 'CM0102_WEB_SUSPEND:STRING=asyncify\n');
  // Accounts (signed out in the videos: the plans and the sign-in), as sold on the site.
  const s3fake = await fakeS3(), google = await fakeGoogle();
  const coffee = process.env.CM_COFFEE_URL || 'https://buymeacoffee.com';
  const s3 = createS3({ endpoint: s3fake.endpoint, bucket: s3fake.bucket, accessKeyId: 'tid_test', secretAccessKey: 'tsec_test', pathStyle: true });
  const cloud = createCloud({ s3,
    clientId: google.clientId, clientSecret: google.clientSecret, sessionSecret: 'v'.repeat(40), google: google.google, price: '£5 a month',
    coffee: { secret: 'video', url: `${coffee.replace(/\/+$/, '')}/membership`, levels: [] }, log: () => {} });
  // The database library, with three databases as the admin page would add them (library.mjs: the index and the zips).
  const library = createLibrary({ s3 });
  const libEntries = [];
  for (const [i, [name, season, note]] of [['2024/25 squads', 2024, 'Example: a database of the 2024/25 season'],
    ['2018/19 squads', 2018, 'Example: a database of the 2018/19 season'], ['2011/12 squads', 2011, 'Example: a database of the 2011/12 season']].entries()) {
    const zip = await writeZip(Object.entries(fakeDataSet()).map(([n, data]) => ({ name: `${name.replace('/', '-')}/${n}`, data })));
    const id = `00000000000000a${i}`;
    await s3.put(`library/${id}.zip`, Buffer.from(zip), { type: 'application/zip' });
    libEntries.push({ id, name: name.replace('/', '-'), season, note, size: zip.length, rawSize: zip.length, added: new Date().toISOString() });
  }
  await s3.put('library/index.json', Buffer.from(JSON.stringify({ entries: libEntries })), { type: 'application/json' });
  const server = createApp({ gameDir: game, buildDir: build, manifestPath: join(tmp, 'manifest.tsv'), cloud, library,
    stats: createStats({ store: statsStore(null) }), statsMode: 'optout', adminToken,
    site: { seller: process.env.CM_SELLER || '', price: '£5 a month', coffee, youtube: process.env.CM_YOUTUBE_URL } });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  const BASE = `http://127.0.0.1:${server.address().port}`;
  const close = async () => {
    server.close();
    s3fake.close();
    google.close();
    rmSync(tmp, { recursive: true, force: true });
  };
  return { BASE, tmp, s3, google, close };
}
