// Stages the web app for hosting (web/deploy/README.md): one folder with the server, the page, the browser build of
// the engine, the season tables and the clean game files, each also brotli-compressed when that saves space, plus
// the Dockerfile and fly.toml. `fly deploy` in that folder puts it on Fly.io.
//
//   node web/deploy/stage.mjs --game ~/cm0102-game [--build build-web] [--out web/deploy/stage]
//
// It stops when a game file is missing or is not the clean file (size and SHA-256 against
// tools/clean_files/manifest.tsv), or when the engine is not built. A fly.toml already in the stage folder stays
// (fly launch wrote the app name into it); everything else is made again.
import { copyFileSync, createReadStream, createWriteStream, existsSync, mkdirSync, readdirSync, readFileSync, rmSync,
  statSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { pipeline } from 'node:stream/promises';
import { fileURLToPath } from 'node:url';
import { constants, createBrotliCompress } from 'node:zlib';
import { findGameDirs, inventory, verify } from '../server.mjs';

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

function copy(from, to) {
  mkdirSync(dirname(to), { recursive: true });
  copyFileSync(from, to);
  return to;
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

  const flyToml = join(opts.out, 'fly.toml');
  const keptToml = existsSync(flyToml) ? readFileSync(flyToml) : null;
  rmSync(opts.out, { recursive: true, force: true });
  mkdirSync(opts.out, { recursive: true });

  const served = [];
  for (const name of ['server.mjs', 'index.html', 'loader.js', 'saves.js', 'package.json']) copy(join(ROOT, 'web', name), join(opts.out, 'web', name));
  copy(opts.manifest, join(opts.out, 'tools/clean_files/manifest.tsv'));
  for (const f of inv.files) {
    const to = f.path.startsWith('Seasons/') ? join(opts.out, 'build-web/seasons', f.path.slice(8)) : join(opts.out, 'game', f.path);
    served.push(copy(f.disk, to));
  }
  for (const ext of ['js', 'wasm']) served.push(copy(inv.engine[ext].disk, join(opts.out, 'build-web', `cm0102_recomp.${ext}`)));
  writeFileSync(join(opts.out, 'build-web/CMakeCache.txt'), `CM0102_WEB_SUSPEND:STRING=${inv.suspend ?? 'jspi'}\n`);
  copy(join(HERE, 'Dockerfile'), join(opts.out, 'Dockerfile'));
  if (keptToml) writeFileSync(flyToml, keptToml);
  else copy(join(HERE, 'fly.toml'), flyToml);

  let raw = 0, sent = 0;
  for (const file of served) {
    raw += statSync(file).size;
    sent += await brotli(file);
  }
  return { files: served.length, raw, sent, keptToml: Boolean(keptToml), out: opts.out };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const opts = args(process.argv.slice(2));
    console.log(`staging ${opts.game} and ${opts.build} into ${opts.out}…`);
    const r = await stage(opts);
    console.log(`${r.files} files, ${mb(r.raw)}; a first visit downloads ${mb(r.sent)} (brotli)`);
    console.log(r.keptToml ? 'kept the fly.toml of the stage folder' : 'new fly.toml: run fly launch once (web/deploy/README.md)');
    console.log(`next: cd ${r.out} && fly deploy`);
  } catch (err) {
    console.error(`stage: ${err.message}`);
    process.exit(1);
  }
}
