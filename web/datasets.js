// The data sets of the user folder (web/README.md, "Data sets"): folders of Databases with the 20 database files,
// which the game lists when a new game starts (src/recomp_rt/win/dbfolders.h). Here: the list, a zip of one to
// download, the upload of a zip (one or more data sets) or of the 20 files, Delete, the choice for the next new game,
// and the sync with the cloud for an account with Cloud Saves (the same rules as the saves, account.js).
import { DATA_SET_FILES, DataSet } from './dataset.js';
import { otherDeviceName, planSync } from './account.js';
import { writeZip, zipEntries, zipRead } from './zip.js';

export const FIRST_SEASON = 2001, LAST_SEASON = 2025;          // the seasons the game has (seasons_db.h)
const META = '.web.json';                                       // the page's note in a folder: { season }
const CHANGE = { 'x-cm': '1' };

export const seasonText = (y) => `${y}/${String((y + 1) % 100).padStart(2, '0')}`;
const dirOf = (root, name) => `${root}/Databases/${name}`;
const lower = (list) => new Set(list.map((n) => n.toLowerCase()));

// A free folder name: base, or base (2), (3)...
export function uniqueName(base, taken) {
  const t = lower([...taken]);
  if (!t.has(base.toLowerCase())) return base;
  for (let i = 2; ; i++) if (!t.has(`${base} (${i})`.toLowerCase())) return `${base} (${i})`;
}

// A name the folder and the game's option can hold (season_screen.c folder_fault, dbfolders_path).
export function cleanName(raw) {
  const n = String(raw ?? '').replace(/[\\/:*?"<>|\u0000-\u001f\u007f]/g, ' ').replace(/\s+/g, ' ').trim().replace(/^\.+/, '').slice(0, 60).trim();
  return n || 'Data set';
}

function filesOf(FS, dir) {
  try { return FS.readdir(dir).filter((f) => f !== '.' && f !== '..'); } catch { return null; }
}

// The data sets: [{ name, size (bytes of the 20 files), mtime (the latest of them, ms), season (the page's note) }].
export function listDataSets(FS, root) {
  const names = filesOf(FS, `${root}/Databases`) ?? [];
  const out = [];
  for (const name of names) {
    const files = filesOf(FS, dirOf(root, name));
    if (!files) continue;
    const have = new Map(files.map((f) => [f.toLowerCase(), f]));
    if (!DATA_SET_FILES.every((f) => have.has(f))) continue;
    let size = 0, mtime = 0;
    for (const f of DATA_SET_FILES) {
      const st = FS.stat(`${dirOf(root, name)}/${have.get(f)}`);
      size += st.size;
      mtime = Math.max(mtime, +new Date(st.mtime));
    }
    let season = null;
    try { season = JSON.parse(FS.readFile(`${dirOf(root, name)}/${META}`, { encoding: 'utf8' })).season ?? null; } catch { /* none */ }
    out.push({ name, size, mtime, season });
  }
  return out.sort((a, b) => a.name.localeCompare(b.name));
}

export function readDataSet(FS, root, name) {
  const have = new Map(filesOf(FS, dirOf(root, name)).map((f) => [f.toLowerCase(), f]));
  return Object.fromEntries(DATA_SET_FILES.map((f) => [have.get(f), FS.readFile(`${dirOf(root, name)}/${have.get(f)}`)]));
}

export function noteSeason(FS, root, name, season) {
  FS.writeFile(`${dirOf(root, name)}/${META}`, JSON.stringify({ season }));
}

// A new folder with the files ({ name: bytes }); the files are checked as a data set first.
export function writeDataSet(FS, root, name, files, { season = null, time = null } = {}) {
  new DataSet(files);                                           // throws for files that are not a data set
  try { FS.mkdir(`${root}/Databases`); } catch { /* there already */ }
  try { FS.mkdir(dirOf(root, name)); } catch { /* replaced */ }
  for (const [f, b] of Object.entries(files)) {
    FS.writeFile(`${dirOf(root, name)}/${f}`, b);
    if (time) try { FS.utime(`${dirOf(root, name)}/${f}`, time, time); } catch { /* an older file system */ }
  }
  if (season) noteSeason(FS, root, name, season);
}

export function deleteDataSet(FS, root, name) {
  for (const f of filesOf(FS, dirOf(root, name)) ?? []) FS.unlink(`${dirOf(root, name)}/${f}`);
  FS.rmdir(dirOf(root, name));
}

// The zip to download: a folder of the data set's name with its 20 files, as the game's Databases folder takes it.
export async function dataSetZip(FS, root, name) {
  const files = readDataSet(FS, root, name);
  return writeZip(Object.entries(files).map(([f, data]) => ({ name: `${name}/${f}`, data })));
}

// The data sets in a zip: each folder (or the zip's top) that holds the 20 files. [{ name, files }]; name: the folder's,
// or fallback for files at the top.
export async function dataSetsInZip(bytes, fallback) {
  const entries = zipEntries(bytes).filter((e) => !e.name.endsWith('/') && !/(^|\/)__MACOSX\//.test(e.name) && !/(^|\/)\._/.test(e.name));
  const groups = new Map();
  for (const e of entries) {
    const i = e.name.lastIndexOf('/'), folder = i < 0 ? '' : e.name.slice(0, i), base = e.name.slice(i + 1).toLowerCase();
    if (!DATA_SET_FILES.includes(base)) continue;
    if (!groups.has(folder)) groups.set(folder, new Map());
    groups.get(folder).set(base, e);
  }
  const out = [];
  for (const [folder, g] of groups) {
    if (g.size !== DATA_SET_FILES.length) continue;
    const files = {};
    for (const f of DATA_SET_FILES) files[f] = await zipRead(bytes, g.get(f));
    // The folder's name; for a folder called Data (as a game folder has it), the folder above it.
    const parts = folder ? folder.split('/') : [];
    if (parts.length && parts[parts.length - 1].toLowerCase() === 'data') parts.pop();
    out.push({ name: cleanName(parts.length ? parts[parts.length - 1] : fallback), files });
  }
  if (!out.length) throw new Error(`no folder of the zip holds the ${DATA_SET_FILES.length} database files (${DATA_SET_FILES.slice(0, 3).join(', ')} and the others)`);
  return out;
}

// The choice for the next new game: the runtime does what Choose Season does (season_screen.c,
// rt_web_choose_data_set). An engine built before that call: the settings file gets the two lines, and the choice
// counts from the next start of the page. Returns 'now', 'next start' or throws.
export function chooseForNextGame(M, FS, root, name, season) {
  if (typeof M?._rt_web_choose_data_set === 'function') {
    if (M.ccall('rt_web_choose_data_set', 'number', ['string', 'number'], [name, season]) === 1) return 'now';
    throw new Error('the game did not take this data set');
  }
  const path = `${root}/settings.txt`;
  let text = '';
  try { text = FS.readFile(path, { encoding: 'utf8' }); } catch { /* none yet */ }
  if (typeof text !== 'string') text = new TextDecoder().decode(text);
  const set = (t, key, value) => (new RegExp(`^\\s*${key}\\s*=.*$`, 'm').test(t) ? t.replace(new RegExp(`^\\s*${key}\\s*=.*$`, 'm'), `${key} = ${value}`)
    : `${t}${t && !t.endsWith('\n') ? '\n' : ''}${key} = ${value}\n`);
  FS.writeFile(path, set(set(text, 'database', `folder:${name}`), 'season', seasonText(season)));
  return 'next start';
}

// ---- The cloud ----

const at = (name) => `/api/datasets/${encodeURIComponent(name)}`;
const STATE = '.cloud-sets.json';

export function createSetSync({ FS, root, sync, log, toast, account }) {
  let state = { account, sets: {} };
  try {
    const kept = JSON.parse(FS.readFile(`${root}/${STATE}`, { encoding: 'utf8' }));
    if (kept.account === account && kept.sets) state = kept;
  } catch { /* the first sync */ }
  let chain = Promise.resolve(), stopped = false, full = false, busy = new Set();
  const listeners = new Set();
  const changed = () => { for (const f of listeners) f(); };
  const keep = () => { FS.writeFile(`${root}/${STATE}`, JSON.stringify(state)); sync(); };
  const local = () => new Map(listDataSets(FS, root).map((s) => [s.name, { size: s.size, mtime: s.mtime }]));

  async function up(name, base) {
    const l = local().get(name);
    const r = await fetch(at(name), { method: 'PUT', body: await dataSetZip(FS, root, name),
      headers: { ...CHANGE, 'x-base-etag': base, 'x-save-time': String(l.mtime) } });
    if (r.status === 409) return { conflict: true };
    if (r.status === 507 || r.status === 413) { if (!full) toast((await r.json()).error, { kind: 'bad', ms: 8000 }); full = true; return {}; }
    if (r.status === 401 || r.status === 403) { stopped = true; return {}; }
    if (!r.ok) throw new Error(`${name} up: HTTP ${r.status}`);
    const j = await r.json();
    state.sets[name] = { etag: j.etag, size: l.size, mtime: l.mtime };
    log(`page: data set ${name} is in the cloud`);
    return { up: true };
  }

  async function down(name, as) {
    const r = await fetch(at(name), { cache: 'no-store' });
    if (!r.ok) throw new Error(`${name} down: HTTP ${r.status}`);
    const etag = r.headers.get('etag'), saved = Number(r.headers.get('x-saved')) || Date.now();
    const [set] = await dataSetsInZip(new Uint8Array(await r.arrayBuffer()), as);
    writeDataSet(FS, root, as, set.files, { time: saved });
    const l = local().get(as);
    log(`page: data set ${name} came from the cloud${as !== name ? ` as ${as}` : ''}`);
    return { etag, ...l };
  }

  async function keepBoth(name, cloudNames) {
    const other = otherDeviceName(`${name}.sav`, lower([...local().keys(), ...cloudNames].map((n) => `${n}.sav`))).slice(0, -4);
    await down(name, other);
    state.sets[other] = { etag: null, ...local().get(other) };
    if ((await up(other, 'none')).conflict) return;
    const mine = await up(name, (await (await fetch('/api/datasets', { cache: 'no-store' })).json()).sets.find((s) => s.name === name)?.etag ?? 'none');
    if (mine.up) toast(`${name} was also changed on another device. That version is kept as ${other}.`, { ms: 9000 });
  }

  async function once() {
    if (stopped) return [];
    const r = await fetch('/api/datasets', { cache: 'no-store' });
    if (r.status === 401 || r.status === 403) { stopped = true; changed(); return []; }
    if (!r.ok) throw new Error(`the cloud's data sets: HTTP ${r.status}`);
    const { sets } = await r.json();
    const cloud = new Map(sets.map((s) => [s.name, { etag: s.etag, rawSize: s.rawSize, saved: s.saved }]));
    const plan = planSync(local(), cloud, state.sets, { replace: true });
    Object.assign(state.sets, plan.settle);
    for (const n of plan.forget) delete state.sets[n];
    for (const job of plan.jobs) {
      busy.add(job.name);
      changed();
      try {
        if (job.do === 'down') { const got = await down(job.name, job.name); state.sets[job.name] = { etag: got.etag, size: got.size, mtime: got.mtime }; }
        else if (job.do === 'up') { if (!(full && job.base === 'none') && (await up(job.name, job.base)).conflict) await keepBoth(job.name, cloud.keys()); }
        else await keepBoth(job.name, cloud.keys());
      } finally { busy.delete(job.name); keep(); changed(); }
      if (stopped) break;
    }
    keep();
    changed();
    return plan.jobs;
  }

  function run() {
    const job = chain.then(once).catch((err) => { log(`page: cloud data sets: ${err.message ?? err}`); return null; });
    chain = job;
    return job;
  }

  return {
    run,
    stop() { stopped = true; changed(); },
    onChange(f) { listeners.add(f); },
    status(name) {
      if (busy.has(name)) return 'up';
      const e = state.sets[name], l = local().get(name);
      if (!e) return 'waiting';
      if (e.removed) return 'here';
      return l && e.size === l.size && e.mtime === l.mtime ? 'cloud' : 'waiting';
    },
    async remove(name) {
      const r = await fetch(at(name), { method: 'DELETE', headers: CHANGE });
      if (!r.ok && r.status !== 404) throw new Error(`${name}: HTTP ${r.status}`);
      const l = local().get(name);
      if (l) state.sets[name] = { etag: null, ...l, removed: true }; else delete state.sets[name];
      full = false;
      keep();
      changed();
    },
    // A data set deleted here: out of the cloud too, so that it does not come back.
    async deleted(name) {
      if (state.sets[name] && !state.sets[name].removed) await fetch(at(name), { method: 'DELETE', headers: CHANGE }).catch(() => {});
      delete state.sets[name];
      keep();
    },
  };
}
