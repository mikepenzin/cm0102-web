// Stages the web app for hosting (web/deploy/README.md): one folder with the server, the page, the browser build of
// the engine, the season tables and the clean game files, each also brotli-compressed when that saves space, plus
// the Dockerfile and fly.toml. `fly deploy` in that folder puts it on Fly.io.
//
//   node web/deploy/stage.mjs --game ~/cm0102-game [--build build-web] [--out web/deploy/stage]
//
// It stops when a game file is missing or is not the clean file (size and SHA-256 against
// tools/clean_files/manifest.tsv), or when the engine is not built. A fly.toml already in the stage folder stays
// (fly launch wrote the app name into it). A file that did not change since the last stage is left as it is, with its
// time and its brotli copy, so a deploy rebuilds and uploads only the layers whose files changed (Dockerfile).
import { copyFileSync, createReadStream, createWriteStream, existsSync, mkdirSync, readdirSync, readFileSync, rmSync,
  statSync, utimesSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { pipeline } from 'node:stream/promises';
import { fileURLToPath } from 'node:url';
import { constants, createBrotliCompress } from 'node:zlib';
import { PAGE_FILES, SERVER_FILES, findGameDirs, inventory, verify } from '../server.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, '..', '..');
const KEEP_BR = 0.9;          // a .br is kept when it is at most 90% of the file

function args(argv) {
  const out = { build: join(ROOT, 'build-web'), out: join(HERE, 'stage'), manifest: join(ROOT, 'tools/clean_files/manifest.tsv') };
  for (let i = 0; i < argv.length; i += 2) {
    const key = argv[i].replace(/^--/, '');
    if (!['game', 'build', 'out', 'manifest'].includes(key) || argv[i + 1] === undefined) throw new Error(`unknown argument ${argv[i]}`);
    out[key] = resolve(argv[i + 1]);
  }
  if (!out.game) throw new Error('--game <folder with cm0102.exe and Data> is needed');
  return out;
}

const mb = (n) => `${(n / 1048576).toFixed(1)} MB`;

async function brotli(file) {
  const br = `${file}.br`;
  await pipeline(createReadStream(file), createBrotliCompress({ params: {
    [constants.BROTLI_PARAM_QUALITY]: 5, [constants.BROTLI_PARAM_SIZE_HINT]: statSync(file).size } }), createWriteStream(br));
  const size = statSync(file).size, packed = statSync(br).size;
  if (packed > size * KEEP_BR) { rmSync(br); return size; }
  return packed;
}

// Copies from to to, unless to is that file already (the same size and modification time). An unchanged file keeps
// its time, so Fly's builder sees it unchanged and does not rebuild or upload its layer again (Dockerfile).
function update(from, to) {
  const s = statSync(from);
  if (existsSync(to)) {
    const d = statSync(to);
    if (d.size === s.size && Math.abs(d.mtimeMs - s.mtimeMs) < 2) return false;   // utimes rounds through seconds
  }
  mkdirSync(dirname(to), { recursive: true });
  copyFileSync(from, to);
  utimesSync(to, s.atime, s.mtime);
  return true;
}

// Writes text to path, unless path holds that text already.
function updateText(path, text) {
  if (existsSync(path) && readFileSync(path, 'utf8') === text) return false;
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, text);
  return true;
}

function walk(dir) {
  let entries;
  try { entries = readdirSync(dir, { withFileTypes: true }); } catch { return []; }
  return entries.flatMap((e) => (e.isDirectory() ? walk(join(dir, e.name)) : [join(dir, e.name)]));
}

export async function stage(opts) {
  if (!existsSync(join(opts.game, 'cm0102.exe'))) {
    if (!existsSync(opts.game)) throw new Error(`${opts.game} does not exist. Use --game with the folder that holds cm0102.exe and Data (web/README.md, step 3).`);
    const found = findGameDirs(opts.game).filter((d) => d !== opts.game);
    throw new Error(found.length ? `${opts.game} has no cm0102.exe, but ${found[0]} has: use --game "${found[0]}"`
      : `${opts.game} has no cm0102.exe, and no folder under it has one (web/README.md, step 3).`);
  }
  const inv = inventory({ gameDir: opts.game, buildDir: opts.build, manifestPath: opts.manifest });
  if (inv.missing.length) throw new Error(`${inv.missing.length} files are missing or of another size, first: ${inv.missing.slice(0, 5).map((m) => m.path).join(', ')}`);
  if (!inv.engine.js || !inv.engine.wasm) throw new Error(`the engine is not built in ${opts.build} (web/README.md, step 2)`);
  const bad = await verify(inv, opts.manifest);
  if (bad.length) throw new Error(`${bad.length} files are not the clean files, first: ${bad.slice(0, 5).join(', ')}`);

  mkdirSync(opts.out, { recursive: true });
  const want = new Set(), changed = new Set(), served = [];
  const put = (from, to, serve) => {
    want.add(to);
    if (update(from, to)) changed.add(to);
    if (serve) served.push(to);
  };
  for (const name of [...SERVER_FILES, ...PAGE_FILES]) put(join(ROOT, 'web', name), join(opts.out, 'web', name), false);
  put(opts.manifest, join(opts.out, 'tools/clean_files/manifest.tsv'), false);
  for (const f of inv.files) {
    const to = f.path.startsWith('Seasons/') ? join(opts.out, 'build-web/seasons', f.path.slice(8)) : join(opts.out, 'game', f.path);
    put(f.disk, to, true);
  }
  for (const ext of ['js', 'wasm']) put(inv.engine[ext].disk, join(opts.out, 'build-web', `cm0102_recomp.${ext}`), true);
  for (const [to, text] of [[join(opts.out, 'build-web/CMakeCache.txt'), `CM0102_WEB_SUSPEND:STRING=${inv.suspend ?? 'jspi'}\n`],
    [join(opts.out, 'Dockerfile'), readFileSync(join(HERE, 'Dockerfile'), 'utf8')]]) {
    want.add(to);
    if (updateText(to, text)) changed.add(to);
  }
  const flyToml = join(opts.out, 'fly.toml');
  const keptToml = existsSync(flyToml);
  if (!keptToml) copyFileSync(join(HERE, 'fly.toml'), flyToml);
  want.add(flyToml);

  // A brotli copy for each new or changed file; an unchanged file keeps the copy it has (or has none, when it did
  // not compress).
  let raw = 0, sent = 0, compressed = 0;
  for (const file of served) {
    const br = `${file}.br`;
    if (changed.has(file) || !existsSync(file)) { rmSync(br, { force: true }); await brotli(file); compressed++; }
    if (existsSync(br)) want.add(br);
    raw += statSync(file).size;
    sent += existsSync(br) ? statSync(br).size : statSync(file).size;
  }
  // What the stage no longer makes (a file gone from the list, an old .br, a stray file) leaves the folder.
  let removed = 0;
  for (const sub of ['web', 'tools', 'game', 'build-web']) {
    for (const p of walk(join(opts.out, sub))) if (!want.has(p)) { rmSync(p); removed++; }
  }
  return { files: served.length, changed: changed.size, compressed, removed, raw, sent, keptToml, out: opts.out };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const opts = args(process.argv.slice(2));
    console.log(`staging ${opts.game} and ${opts.build} into ${opts.out}…`);
    const r = await stage(opts);
    console.log(`${r.files} files, ${mb(r.raw)}; a first visit downloads ${mb(r.sent)} (brotli)`);
    console.log(`since the last stage: ${r.changed} files changed, ${r.compressed} compressed, ${r.removed} removed`);
    console.log(r.keptToml ? 'kept the fly.toml of the stage folder' : 'new fly.toml: run fly launch once (web/deploy/README.md)');
    console.log(`next: cd ${r.out} && fly deploy`);
  } catch (err) {
    console.error(`stage: ${err.message}`);
    process.exit(1);
  }
}
