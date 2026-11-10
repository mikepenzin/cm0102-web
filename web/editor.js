// The editors on the page (web/README.md, "Edit a saved game", "Edit a data set"). One dialog: a home that lists the
// saved games and the data sets, then a search for a player, his fields, and Save.
// - A saved game (savedit.js): the file goes back to the user folder under its own name, the file as it was to
//   backups/ first, so Load Game lists the edited save and Saved games the old one.
// - A data set (dataset.js): a folder of Databases in the user folder with the 20 database files, which the game lists
//   under Database when a new game starts (src/recomp_rt/win/dbfolders.h). A new one is a copy of the game's own data.
import { ATTRIBUTES, POSITIONS, SaveGame } from './savedit.js';
import { ATTRIBUTES as DATA_ATTRIBUTES, DATA_SET_FILES, DataSet } from './dataset.js';
import { listSaves } from './saves.js';

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const GROUPS = ['Technical', 'Mental', 'Physical', 'Goalkeeping', 'Hidden'];
const SHORT_POS = ['GK', 'SW', 'D', 'DM', 'M', 'AM', 'F', 'WB', 'R', 'L', 'C', 'FR'];
const pad = (n) => String(n).padStart(2, '0');
const isoDate = (d) => (d ? `${d.year}-${pad(d.month)}-${pad(d.day)}` : '');
const fromIso = (v) => (/^\d{4}-\d{2}-\d{2}$/.test(v) ? { year: +v.slice(0, 4), month: +v.slice(5, 7), day: +v.slice(8, 10) } : null);
const MB = (n) => `${(n / 1048576).toFixed(0)} MB`;

// A name for a data set folder: what dbfolders_path takes (no path part), and no month and year, which would make
// the game list it as a season.
export function dataSetNameProblem(name, taken) {
  const n = String(name ?? '').trim();
  if (!n) return 'Give the data set a name.';
  if (n.length > 60) return 'The name is too long: 60 letters at most.';
  if (/[\\/:*?"<>|]/.test(n) || n.startsWith('.')) return 'The name cannot have \\ / : * ? " < > | or start with a dot.';
  if (/\b(jan|feb|mar|apr|may|jun|jul|aug|sep|sept|oct|nov|dec|january|february|march|april|june|july|august|september|october|november|december|spring|autumn)\b.*\b\d{4}\b/i.test(n))
    return 'Leave the month and year out of the name: the game would take it for a season.';
  if (taken.has(n.toLowerCase())) return 'A data set of that name exists.';
  return null;
}

export function mountEditor({ FS, sync, log, toast, dir, gameData = '/game/Data', isRunning, onSaved = () => {} }) {
  const dialog = $('editor'), list = $('editor-results'), form = $('editor-form'), note = $('editor-note');
  const search = $('editor-search'), saveButton = $('editor-save'), body = dialog.querySelector('.ed-body');
  const home = $('editor-home'), title = $('editor-title'), fileLabel = $('editor-file');
  const DB_DIR = `${dir}/Databases`;
  let model = null, kind = null, name = null, edited = new Set(), current = null, backedUp = false, clubs = null, nations = null;
  const say = (text, bad = false) => { note.textContent = text; note.className = bad ? 'small bad' : 'small'; };
  const dirty = () => {
    saveButton.hidden = !model;
    saveButton.disabled = edited.size === 0;
    const what = kind === 'set' ? 'Save the data set' : 'Save the game file';
    saveButton.textContent = edited.size ? `${what} (${edited.size} changed)` : what;
  };
  // The clubs for the club field of a data set (13,000 names), once it is open.
  const fillClubs = () => { $('editor-clubs').innerHTML = clubs ? clubs.map((c) => `<option value="${esc(clubLabel(c))}">`).join('') : ''; };
  const later = () => new Promise((r) => setTimeout(r, 30));          // a message shows before long work

  // ---- The data sets in the user folder ----

  function dataSets() {
    let names = [];
    try { names = FS.readdir(DB_DIR).filter((n) => n !== '.' && n !== '..'); } catch { return []; }
    return names.filter((n) => {
      try { const have = new Set(FS.readdir(`${DB_DIR}/${n}`).map((f) => f.toLowerCase())); return DATA_SET_FILES.every((f) => have.has(f)); }
      catch { return false; }
    }).sort((a, b) => a.localeCompare(b));
  }

  function gameFiles() {
    const out = {};
    const have = new Map(FS.readdir(gameData).map((f) => [f.toLowerCase(), f]));
    for (const f of DATA_SET_FILES) {
      if (!have.has(f)) throw new Error(`the game's data has no ${f}`);
      out[have.get(f)] = `${gameData}/${have.get(f)}`;
    }
    return out;
  }

  // ---- The home: what can be edited ----

  function showHome() {
    model = null; kind = null; edited = new Set(); current = null;
    title.textContent = 'Editor';
    fileLabel.textContent = '';
    body.hidden = true;
    home.hidden = false;
    dirty();
    const saves = listSaves(FS).saves, sets = dataSets();
    home.innerHTML = `<section><h4>A saved game</h4><p class="small">Change the players of a career: ability, attributes, positions, age.
        Load the save in the game afterwards.</p>
      ${saves.length ? `<div class="ed-files">${saves.map((f) => `<div class="ed-file"><span>${esc(f.name)}</span>
        <button data-open-save="${esc(f.name)}">Edit</button></div>`).join('')}</div>` : '<p class="small">No saved games yet.</p>'}</section>
      <section><h4>A data set, for new games</h4><p class="small">Your own copy of the game's database: change players or add new ones.
        When you start a new game, choose it under Database. Each data set takes about 60 MB of this browser's storage.</p>
      ${sets.length ? `<div class="ed-files">${sets.map((n) => `<div class="ed-file"><span>${esc(n)}</span>
        <button data-open-set="${esc(n)}">Edit</button><button class="link" data-delete-set="${esc(n)}">Delete</button></div>`).join('')}</div>` : ''}
      <div class="row"><input id="editor-new-name" type="text" placeholder="Name, for example My data set" maxlength="60" autocomplete="off">
        <button class="primary" id="editor-new">New data set</button></div></section>`;
    $('editor-new').onclick = () => createSet($('editor-new-name').value);
  }

  async function createSet(raw) {
    const n = String(raw ?? '').trim();
    const why = dataSetNameProblem(n, new Set(dataSets().map((x) => x.toLowerCase())));
    if (why) { say(why, true); return; }
    say('Copying the game\'s database…');
    await later();
    try {
      const files = gameFiles();
      try { FS.mkdir(DB_DIR); } catch { /* there already */ }
      FS.mkdir(`${DB_DIR}/${n}`);
      for (const [f, path] of Object.entries(files)) FS.writeFile(`${DB_DIR}/${n}/${f}`, FS.readFile(path));
      sync();
      log(`page: data set ${n} made from the game's data`);
      await openSet(n);
      say(`${n} is ready. Change players or add new ones, then save it.`);
    } catch (e) {
      say(`The data set was not made: ${e.message}`, true);
    }
  }

  function deleteSet(n) {
    if (!confirm(`Delete the data set ${n}? Games started with it keep running; their saves hold their own data.`)) return;
    try {
      for (const f of FS.readdir(`${DB_DIR}/${n}`)) if (f !== '.' && f !== '..') FS.unlink(`${DB_DIR}/${n}/${f}`);
      FS.rmdir(`${DB_DIR}/${n}`);
      sync();
      say(`${n} is deleted.`);
    } catch (e) { say(`${n} was not deleted: ${e.message}`, true); }
    showHome();
  }

  // ---- Opening ----

  function showEditing(label, what) {
    home.hidden = true;
    body.hidden = false;
    body.classList.remove('detail');
    title.textContent = what;
    fileLabel.textContent = label;
    form.innerHTML = '';
    search.value = '';
    $('editor-add').hidden = kind !== 'set';
    dirty();
  }

  async function openSave(file) {
    kind = 'save'; name = file; edited = new Set(); backedUp = false; current = null;
    if (!dialog.open) dialog.showModal();
    showEditing(file, 'Edit a saved game');
    list.innerHTML = '<p class="small">Opening the saved game…</p>';
    say(isRunning() ? 'If this save is loaded in the game now, save your changes here, then load it again in the game. Saving in the game first would write over them.' : '');
    await later();
    try {
      model = new SaveGame(FS.readFile(`${dir}/${file}`));
      log(`page: ${file} opened in the editor: ${model.players().length} players`);
      dirty();
      results();
      search.focus();
    } catch (e) {
      model = null;
      list.innerHTML = '';
      say(`This file cannot be edited: ${e.message}`, true);
    }
  }

  async function openSet(n) {
    kind = 'set'; name = n; edited = new Set(); current = null; clubs = null; nations = null;
    if (!dialog.open) dialog.showModal();
    showEditing(n, 'Edit a data set');
    list.innerHTML = '<p class="small">Opening the data set…</p>';
    say('');
    await later();
    try {
      const have = new Map(FS.readdir(`${DB_DIR}/${n}`).map((f) => [f.toLowerCase(), f]));
      const files = {};
      for (const f of DATA_SET_FILES) files[have.get(f)] = FS.readFile(`${DB_DIR}/${n}/${have.get(f)}`);
      model = new DataSet(files);
      clubs = model.clubs();
      nations = model.nations();
      fillClubs();
      log(`page: data set ${n} opened in the editor: ${model.players().length} players`);
      dirty();
      results();
      search.focus();
    } catch (e) {
      model = null;
      list.innerHTML = '';
      say(`This data set cannot be edited: ${e.message}`, true);
    }
  }

  // ---- The players ----

  function results() {
    const found = model.search(search.value, 80);
    list.innerHTML = found.length ? found.map((x) => `<button class="ed-row${edited.has(x.row) ? ' edited' : ''}${current === x.row ? ' on' : ''}" data-row="${x.row}">
        <b>${esc(x.name)}</b><span>${esc(x.club || 'No club')} · ${esc(x.pos)} · ${x.born || ''}</span><i>${x.ca}/${x.pa}</i></button>`).join('')
      : '<p class="small">No player of that name.</p>';
  }

  const num = (key, value, min, max, label = key) => `<label><span>${esc(label)}</span>
    <input type="number" inputmode="numeric" min="${min}" max="${max}" data-f="${esc(key)}" value="${value}" data-was="${value}"></label>`;
  const text = (key, value, label, wide = false) => `<label class="${wide ? 'wide' : ''}"><span>${esc(label)}</span>
    <input type="text" maxlength="50" data-f="${esc(key)}" value="${esc(value)}" data-was="${esc(value)}" autocomplete="off" spellcheck="false"></label>`;
  const date = (key, value, label) => `<label><span>${esc(label)}</span><input type="date" data-f="${key}" value="${value}" data-was="${value}" min="1940-01-01" max="2015-12-31"></label>`;
  const clubLabel = (c) => `${c.name}${c.nation >= 0 && model.nationName(c.nation) ? ` (${model.nationName(c.nation)})` : ''}`;
  const select = (key, value, options, label) => `<label class="wide"><span>${esc(label)}</span><select data-f="${key}" data-was="${value}">
    ${options.map(([v, t]) => `<option value="${v}"${v === value ? ' selected' : ''}>${esc(t)}</option>`).join('')}</select></label>`;

  // row: a player's row, or null for a new player.
  function show(row) {
    current = row;
    const set = kind === 'set';
    const attrs = set ? DATA_ATTRIBUTES : ATTRIBUTES;
    const f = row === null ? newPlayer() : model.read(row);
    const x = row === null ? null : model.players().find((p) => p.row === row);
    const head = row === null ? '<h3>A new player</h3><p class="small">Attributes left at 0 are random: the game sets them when a new game starts.</p>'
      : `<h3>${esc(x.name)}</h3><p class="small">${esc(x.club || 'No club')} · ${esc(x.nation)}</p>`;
    const clubNow = set ? clubs.find((c) => c.id === f.club) : null;
    const who = set ? `<section><h4>Name and club</h4><div class="ed-grid">
        ${text('first', f.first, 'First name')}${text('second', f.second, 'Surname')}${text('common', f.common, 'Known as (optional)')}
        ${select('nation', f.nation, nations.map((n) => [n.id, n.name]), 'Nationality')}
        ${select('nation2', f.nation2 >= 0 ? f.nation2 : -2, [[-2, 'None'], ...nations.map((n) => [n.id, n.name])], 'Second nationality')}
        <label class="wide"><span>Club</span><input type="text" list="editor-clubs" data-f="club" value="${esc(clubNow ? clubLabel(clubNow) : '')}"
          data-was="${esc(clubNow ? clubLabel(clubNow) : '')}" placeholder="No club" autocomplete="off" spellcheck="false"></label>
        ${date('contractEnd', isoDate(f.contractEnd), 'Contract ends')}
      </div></section>` : '';
    form.innerHTML = `<div class="ed-title"><button class="link ed-back" type="button">← Players</button>${head}</div>
      ${who}
      <section><h4>Ability and age</h4><div class="ed-grid">
        ${num('ca', f.ca, set ? 0 : 1, 200, 'Current ability')}${num('pa', f.pa, set ? -2 : 1, 200, 'Potential ability')}
        ${date('born', isoDate(f.born), 'Born')}
      </div><p class="small">${set ? 'Ability is 1 to 200; 0, and for potential -1 and -2, are values the data has for players the game sets up itself.'
        : 'Ability is 1 to 200. Attributes keep the numbers you see when you change it.'}</p></section>
      <section><h4>Positions <span class="small">0 to 20; 20 is natural</span></h4><div class="ed-grid pos">
        ${f.positions.map((v, i) => num(`pos${i}`, v, 0, 20, SHORT_POS[i]).replace('<span>', `<span title="${POSITIONS[i]}">`)).join('')}
      </div></section>
      ${GROUPS.map((g) => `<section><h4>${g}${g === 'Hidden' ? ' <span class="small">the game does not show these</span>' : ''}${set && g === 'Technical' ? ' <span class="small">0 is random</span>' : ''}</h4><div class="ed-grid">
        ${attrs.filter((a) => a.group === g).map((a) => num(a.name, f.attrs[a.name], set ? 0 : 1, 20)).join('')}</div></section>`).join('')}
      <div class="row"><button class="primary" id="editor-apply" type="button">${row === null ? 'Add this player' : `Apply to ${esc(x.name)}`}</button>
        <button id="editor-reset" type="button">${row === null ? 'Clear' : 'Undo changes'}</button></div>`;
    body.classList.add('detail');
    form.scrollTop = 0;
    form.querySelector('.ed-back').onclick = () => { body.classList.remove('detail'); current = null; results(); };
    $('editor-reset').onclick = () => show(row);
    $('editor-apply').onclick = () => apply(row);
    form.oninput = (e) => { const i = e.target.closest('[data-f]'); if (i) i.classList.toggle('changed', i.value !== i.dataset.was); };
    if (set && row === null) {                        // the club's nation is a good guess for a new player's
      const club = form.querySelector('[data-f="club"]');
      club.onchange = () => { const c = clubByLabel(club.value); if (c && c.nation >= 0) form.querySelector('[data-f="nation"]').value = String(c.nation); };
    }
    results();
  }

  function newPlayer() {
    const zero = Object.fromEntries(DATA_ATTRIBUTES.map((a) => [a.name, 0]));
    return { first: '', second: '', common: '', born: { year: 1983, month: 1, day: 1 }, nation: nations[0]?.id ?? 0, nation2: -2, club: -1,
      contractEnd: { year: 2004, month: 6, day: 30 }, ca: 100, pa: 140, positions: [1, 1, 1, 1, 20, 1, 1, 1, 1, 1, 20, 1], attrs: zero };
  }

  function clubByLabel(v) {
    const t = String(v ?? '').trim();
    if (!t) return null;
    return clubs.find((c) => clubLabel(c) === t) ?? clubs.find((c) => c.name === t) ?? undefined;
  }

  function apply(row) {
    const get = (k) => form.querySelector(`[data-f="${CSS.escape(k)}"]`).value;
    const f = row === null ? newPlayer() : model.read(row);
    f.ca = Number(get('ca'));
    f.pa = Number(get('pa'));
    f.positions = f.positions.map((_, i) => Number(get(`pos${i}`)));
    for (const a of kind === 'set' ? DATA_ATTRIBUTES : ATTRIBUTES) f.attrs[a.name] = Number(get(a.name));
    const born = fromIso(get('born'));
    if (born) f.born = born; else if (kind === 'save') f.born = null;
    try {
      if (kind === 'set') {
        f.first = get('first'); f.second = get('second'); f.common = get('common');
        f.nation = Number(get('nation')); f.nation2 = Number(get('nation2'));
        const c = clubByLabel(get('club'));
        if (c === undefined) { say(`There is no club called "${get('club')}". Pick one from the list, or leave it empty for no club.`, true); return; }
        f.club = c ? c.id : -1;
        f.contractEnd = fromIso(get('contractEnd'));
        const r = row === null ? model.add(f) : (model.write(row, f), row);
        edited.add(r);
        dirty();
        show(r);
        say(`${row === null ? 'Added' : 'Applied'}. Save the data set to keep the changes.`);
        return;
      }
      const missed = model.write(row, f);
      edited.add(row);
      dirty();
      show(row);
      const m = Object.entries(missed);
      say(m.length ? `Applied. ${m.map(([k, v]) => `${k} shows ${v}`).join(', ')}: with this ability the game cannot show it as asked. Save the game file to keep the changes.`
        : 'Applied. Save the game file to keep the changes.', m.length > 0);
    } catch (e) {
      say(e.message[0].toUpperCase() + e.message.slice(1), true);
    }
  }

  // ---- Saving ----

  async function save() {
    saveButton.disabled = true;
    say(kind === 'set' ? 'Writing the data set…' : 'Writing the game file…');
    await later();
    try {
      if (kind === 'set') {
        const out = model.output();
        let bytes = 0;
        for (const [f, b] of Object.entries(out)) { FS.writeFile(`${DB_DIR}/${name}/${f}`, b); bytes += b.length; }
        sync();
        const changed = edited.size;
        log(`page: data set ${name}: ${changed} player(s) changed, ${Object.keys(out).length} files (${MB(bytes)}) written`);
        await openSet(name);                          // the saved files, read again: the new rows are now the files' own
        say(`Saved: ${changed} player(s) changed in ${name}. Start a new game and choose ${name} under Database to play with it.`);
        return;
      }
      const path = `${dir}/${name}`;
      if (!backedUp) {
        try { FS.mkdir(`${dir}/backups`); } catch { /* there already */ }
        FS.writeFile(`${dir}/backups/${name.replace(/\.sav$/i, '')} (before edit).sav`, FS.readFile(path));
        backedUp = true;
      }
      FS.writeFile(path, model.toBytes());
      sync();
      log(`page: ${name}: ${edited.size} player(s) edited`);
      say(`Saved: ${edited.size} player(s) changed in ${name}. The file as it was is in Saved games, Older copies.${isRunning() ? ' Load the save in the game to play with the changes.' : ''}`);
      edited = new Set();
      dirty();
      results();
      onSaved(name);
    } catch (e) {
      say(`Nothing was written: ${e.message}`, true);
      dirty();
    }
  }

  // ---- Wiring ----

  let timer = 0;
  search.oninput = () => { clearTimeout(timer); timer = setTimeout(() => model && results(), 120); };
  list.onclick = (e) => { const b = e.target.closest('[data-row]'); if (b) show(Number(b.dataset.row)); };
  home.onclick = (e) => {
    const s = e.target.closest('[data-open-save]'), d = e.target.closest('[data-open-set]'), x = e.target.closest('[data-delete-set]');
    if (s) openSave(s.dataset.openSave);
    else if (d) openSet(d.dataset.openSet);
    else if (x) deleteSet(x.dataset.deleteSet);
  };
  $('editor-add').onclick = () => { if (model && kind === 'set') show(null); };
  $('editor-home-button').onclick = () => {
    if (edited.size && !confirm(`${edited.size} player(s) changed and not saved. Leave without saving?`)) return;
    say('');
    showHome();
  };
  saveButton.onclick = save;
  $('editor-close').onclick = () => {
    if (edited.size && !confirm(`${edited.size} player(s) changed and not saved. Close without saving?`)) return;
    dialog.close();
    model = null;
  };
  dialog.addEventListener('cancel', (e) => { if (edited.size) { e.preventDefault(); $('editor-close').click(); } });
  return {
    openSave,
    openSet,
    open() { if (!dialog.open) dialog.showModal(); say(''); showHome(); },
  };
}
