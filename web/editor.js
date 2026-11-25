// The editors on the page (web/README.md, "Edit a saved game", "Edit a data set"). One dialog: a home that lists the
// saved games and the data sets, then a search for a player, his fields, and Save.
// - A saved game (savedit.js): the file goes back to the user folder under its own name, the file as it was to
//   backups/ first, so Load Game lists the edited save and Saved games the old one.
// - A data set (dataset.js): a folder of Databases in the user folder with the 20 database files, which the game lists
//   under Database when a new game starts (src/recomp_rt/win/dbfolders.h). A new one is a copy of the game's own data.
import { ATTRIBUTES, POSITIONS, SaveGame } from './savedit.js';
import { ATTRIBUTES as DATA_ATTRIBUTES, DATA_SET_FILES, DataSet, dateShift } from './dataset.js';
import { copyFile, listSaves, readBlob, readBytes, storeSave } from './saves.js';
import { noteSeason } from './datasets.js';
import { onLanguage, t, tn } from './i18n.js';
import { freeCut, presetFilters, scout, scoutIndex, scoutUpdate } from './scout.js';

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const GROUPS = ['Technical', 'Mental', 'Physical', 'Goalkeeping', 'Hidden'];
const SHORT_POS = ['GK', 'SW', 'D', 'DM', 'M', 'AM', 'F', 'WB', 'R', 'L', 'C', 'FR'];
const pad = (n) => String(n).padStart(2, '0');
const isoDate = (d) => (d ? `${d.year}-${pad(d.month)}-${pad(d.day)}` : '');
const fromIso = (v) => (/^\d{4}-\d{2}-\d{2}$/.test(v) ? { year: +v.slice(0, 4), month: +v.slice(5, 7), day: +v.slice(8, 10) } : null);
const MB = (n) => `${(n / 1048576).toFixed(0)} MB`;
// An error in the page's language: the modules the server imports too give their English as key and vars.
const reason = (e) => t(e.key ?? String(e.message), e.vars);

// The names the editor shows for the groups, attributes and positions (savedit.js keeps them in English), in the
// page's language; built when shown, so a new language shows at the next render.
const groupNames = () => ({ Technical: t('Technical'), Mental: t('Mental'), Physical: t('Physical'), Goalkeeping: t('Goalkeeping'), Hidden: t('Hidden') });
const positionNames = () => ({
  Goalkeeper: t('Goalkeeper'), Sweeper: t('Sweeper'), Defender: t('Defender'), 'Defensive Midfielder': t('Defensive Midfielder'),
  Midfielder: t('Midfielder'), 'Attacking Midfielder': t('Attacking Midfielder'), Attacker: t('Attacker'), 'Wing Back': t('Wing Back'),
  Right: t('Right'), Left: t('Left'), Centre: t('Centre'), 'Free Role': t('Free Role'),
});
const attributeNames = () => ({
  Acceleration: t('Acceleration'), Aggression: t('Aggression'), Agility: t('Agility'), Anticipation: t('Anticipation'),
  Balance: t('Balance'), Bravery: t('Bravery'), Creativity: t('Creativity'), Crossing: t('Crossing'), Decisions: t('Decisions'),
  Determination: t('Determination'), Dribbling: t('Dribbling'), Finishing: t('Finishing'), Flair: t('Flair'), Handling: t('Handling'),
  Heading: t('Heading'), Influence: t('Influence'), Jumping: t('Jumping'), 'Long Shots': t('Long Shots'), Marking: t('Marking'),
  'Off The Ball': t('Off The Ball'), Pace: t('Pace'), Passing: t('Passing'), Positioning: t('Positioning'), Reflexes: t('Reflexes'),
  'Set Pieces': t('Set Pieces'), Stamina: t('Stamina'), Strength: t('Strength'), Tackling: t('Tackling'), Teamwork: t('Teamwork'),
  Technique: t('Technique'), 'Work Rate': t('Work Rate'), Consistency: t('Consistency'), Corners: t('Corners'), Dirtiness: t('Dirtiness'),
  'Important Matches': t('Important Matches'), 'Injury Proneness': t('Injury Proneness'), 'Left Foot': t('Left Foot'),
  'Natural Fitness': t('Natural Fitness'), 'One On Ones': t('One On Ones'), Penalties: t('Penalties'), 'Right Foot': t('Right Foot'),
  'Throw Ins': t('Throw Ins'), Versatility: t('Versatility'),
});
// A player's name from the data as it is; the data's own stand-in for none in the page's language.
const playerName = (x) => (x.name === '(no name)' ? t('(no name)') : x.name);

// A name for a data set folder: what dbfolders_path takes (no path part), and no month and year, which would make
// the game list it as a season.
export function dataSetNameProblem(name, taken) {
  const n = String(name ?? '').trim();
  if (!n) return t('Give the data set a name.');
  if (n.length > 60) return t('The name is too long: 60 letters at most.');
  if (/[\\/:*?"<>|]/.test(n) || n.startsWith('.')) return t('The name cannot have \\ / : * ? " < > | or start with a dot.');
  if (/\b(jan|feb|mar|apr|may|jun|jul|aug|sep|sept|oct|nov|dec|january|february|march|april|june|july|august|september|october|november|december|spring|autumn)\b.*\b\d{4}\b/i.test(n))
    return t('Leave the month and year out of the name: the game would take it for a season.');
  if (taken.has(n.toLowerCase())) return t('A data set of that name exists.');
  return null;
}

// onSetChange(name, 'made' | 'saved' | 'deleted'): a data set changed (the cloud sync of datasets.js follows).
// access: 'full' or 'free' for the Scout (scout.js, scoutAccess); inviteKind: its invitation's button ('signin',
// 'member', 'free', 'none'); invite: opens the Account dialog. onPageKey: keyboard.js's, for the tabs' arrow keys (its
// guard keeps the page's keys from the elements' own listeners).
export function mountEditor({ FS, sync, log, toast, dir, gameData = '/game/Data', isRunning, onSaved = () => {}, onSetChange = () => {},
  access = () => 'full', inviteKind = () => 'signin', invite = () => {}, onPageKey = null }) {
  const dialog = $('editor'), list = $('editor-results'), form = $('editor-form'), note = $('editor-note');
  const search = $('editor-search'), saveButton = $('editor-save'), body = dialog.querySelector('.ed-body');
  const home = $('editor-home'), title = $('editor-title'), fileLabel = $('editor-file');
  const tabs = $('editor-tabs'), scoutBox = $('editor-scout'), filtersBox = $('scout-filters'), found = $('scout-results');
  const DB_DIR = `${dir}/Databases`;
  let model = null, kind = null, name = null, edited = new Set(), current = null, backedUp = false, clubs = null, nations = null;
  // The Scout of a saved game: the pane shown ('players' or 'scout'), the players read once (scoutIndex), the filters
  // as typed (kept from one save to the next).
  let pane = 'players', index = null;
  const filters = { ageMin: '', ageMax: '', position: '', paMin: '', caMin: '', attrs: [0, 1, 2].map(() => ({ name: '', min: '' })) };
  // text: the message, or a function that gives it (said again in a new language).
  let said = null;
  const say = (text, bad = false) => {
    said = [text, bad];
    note.textContent = typeof text === 'function' ? text() : text;
    note.className = bad ? 'small bad' : 'small';
  };
  const dirty = () => {
    saveButton.hidden = !model || edited.size === 0;       // shown once there is a change to save
    saveButton.disabled = edited.size === 0;
    const n = edited.size;
    if (kind === 'set') saveButton.textContent = n ? tn(n, { one: 'Save the data set ({n} changed)', other: 'Save the data set ({n} changed)' }) : t('Save the data set');
    else saveButton.textContent = n ? tn(n, { one: 'Save the game file ({n} changed)', other: 'Save the game file ({n} changed)' }) : t('Save the game file');
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
      if (!have.has(f)) throw Object.assign(new Error(`the game's data has no ${f}`), { key: 'the game\'s data has no {file}', vars: { file: f } });
      out[have.get(f)] = `${gameData}/${have.get(f)}`;
    }
    return out;
  }

  // ---- The home: what can be edited ----

  function showHome() {
    model = null; kind = null; edited = new Set(); current = null; index = null;
    title.textContent = t('Squad Hub');
    fileLabel.textContent = '';
    body.hidden = true;
    home.hidden = false;
    $('editor-home-button').hidden = true;            // the start itself: no way back to it
    dirty();
    const saves = listSaves(FS).saves, sets = dataSets();
    const edit = esc(t('Edit')), del = esc(t('Delete'));
    home.innerHTML = `<section><h4>${esc(t('A saved game'))}</h4><p class="small">${esc(t('Change the players of a career: ability, attributes, positions, age. Load the save in the game afterwards.'))}</p>
      ${saves.length ? `<div class="ed-files">${saves.map((f) => `<div class="ed-file"><span>${esc(f.name)}</span>
        <button data-open-save="${esc(f.name)}">${edit}</button></div>`).join('')}</div>` : `<p class="small">${esc(t('No saved games yet.'))}</p>`}</section>
      <section><h4>${esc(t('A data set, for new games'))}</h4><p class="small">${esc(t('Your own copy of the game\'s database: change players or add new ones. To play with one: Saved games, Databases, Use, then start a new game (or choose it under Database on the game\'s new-game screen). Each data set takes about 60 MB of this browser\'s storage.'))}</p>
      ${sets.length ? `<div class="ed-files">${sets.map((n) => `<div class="ed-file"><span>${esc(n)}</span>
        <button data-open-set="${esc(n)}">${edit}</button><button class="link danger" data-delete-set="${esc(n)}">${del}</button></div>`).join('')}</div>` : ''}
      <div class="row"><input id="editor-new-name" type="text" placeholder="${esc(t('Name, for example My data set'))}" aria-label="${esc(t('Name of the new data set'))}" maxlength="60" autocomplete="off">
        <button class="primary" id="editor-new">${esc(t('New data set'))}</button></div></section>`;
    $('editor-new').onclick = () => createSet($('editor-new-name').value);
  }

  async function createSet(raw) {
    const n = String(raw ?? '').trim();
    const taken = new Set(dataSets().map((x) => x.toLowerCase()));
    if (dataSetNameProblem(n, taken)) { say(() => dataSetNameProblem(n, taken), true); return; }
    say(() => t('Copying the game\'s database…'));
    await later();
    try {
      const files = gameFiles();
      try { FS.mkdir(DB_DIR); } catch { /* there already */ }
      FS.mkdir(`${DB_DIR}/${n}`);
      for (const [f, path] of Object.entries(files)) FS.writeFile(`${DB_DIR}/${n}/${f}`, await readBytes(FS, path));
      noteSeason(FS, dir, n, 2001);                   // the game's own data is the 2001/02 data
      sync();
      log(`page: data set ${n} made from the game's data`);
      onSetChange(n, 'made');
      await openSet(n);
      say(() => t('{name} is ready. Change players or add new ones, then save it.', { name: n }));
    } catch (e) {
      say(() => t('The data set was not made: {why}', { why: reason(e) }), true);
    }
  }

  function deleteSet(n) {
    if (!confirm(t('Delete the data set {name}? Games started with it keep running; their saves hold their own data.', { name: n }))) return;
    try {
      for (const f of FS.readdir(`${DB_DIR}/${n}`)) if (f !== '.' && f !== '..') FS.unlink(`${DB_DIR}/${n}/${f}`);
      FS.rmdir(`${DB_DIR}/${n}`);
      sync();
      onSetChange(n, 'deleted');
      say(() => t('{name} is deleted.', { name: n }));
    } catch (e) { say(() => t('{name} was not deleted: {why}', { name: n, why: reason(e) }), true); }
    showHome();
  }

  // ---- Opening ----

  // A save or a data set open: its own name is the title.
  function showEditing(label) {
    home.hidden = true;
    $('editor-home-button').hidden = false;
    body.hidden = false;
    body.classList.remove('detail');
    title.textContent = label;
    fileLabel.textContent = '';
    form.innerHTML = '';
    search.value = '';
    $('editor-add').hidden = kind !== 'set';
    index = null;
    tabs.hidden = kind !== 'save';                  // the Scout is a saved game's only
    setPane('players', false);
    dirty();
  }

  async function openSave(file) {
    kind = 'save'; name = file; edited = new Set(); backedUp = false; current = null;
    if (!dialog.open) dialog.showModal();
    showEditing(file);
    list.innerHTML = `<p class="small">${esc(t('Opening the saved game…'))}</p>`;
    say(isRunning() ? () => t('Loaded in the game now? Save here, then load it again in the game: saving in the game first would undo these changes.') : '');
    await later();
    try {
      // From a Blob: the header and the tables the editor uses are read, not the whole save (690 MB for some players).
      model = await SaveGame.open(await readBlob(FS, `${dir}/${file}`));
      log(`page: ${file} opened in the editor: ${model.players().length} players`);
      dirty();
      redrawList();                                   // the Scout, if it was chosen while the save opened
      if (pane === 'players') search.focus();
    } catch (e) {
      model = null;
      list.innerHTML = '';
      say(() => t('This file cannot be edited: {why}', { why: reason(e) }), true);
    }
  }

  // The game's own data (the 3.9.68 files at gameData), read once: what the shift of a data set is measured against.
  let baseData = null;
  async function ownData() {
    if (baseData) return baseData;
    const have = new Map(FS.readdir(gameData).map((f) => [f.toLowerCase(), f]));
    if (!DATA_SET_FILES.every((f) => have.has(f))) return null;
    const files = {};
    for (const f of DATA_SET_FILES) files[f] = await readBytes(FS, `${gameData}/${have.get(f)}`);
    baseData = new DataSet(files);
    return baseData;
  }

  async function openSet(n) {
    kind = 'set'; name = n; edited = new Set(); current = null; clubs = null; nations = null;
    if (!dialog.open) dialog.showModal();
    showEditing(n);
    list.innerHTML = `<p class="small">${esc(t('Opening the data set…'))}</p>`;
    say('');
    await later();
    try {
      const have = new Map(FS.readdir(`${DB_DIR}/${n}`).map((f) => [f.toLowerCase(), f]));
      const files = {};
      for (const f of DATA_SET_FILES) files[have.get(f)] = await readBytes(FS, `${DB_DIR}/${n}/${have.get(f)}`);
      model = new DataSet(files);
      // The dates as the game uses them: an Update Team database keeps them years early (dataset.js, dateShift).
      try {
        const base = await ownData();
        if (base) model.shift = dateShift(model, base);
      } catch (e) { log(`page: the dates of ${n} were not measured: ${e.message}`); }
      clubs = model.clubs();
      nations = model.nations();
      fillClubs();
      log(`page: data set ${n} opened in the editor: ${model.players().length} players${model.shift ? `, its dates ${model.shift} years early (the data of ${model.season}/${String(model.season + 1).slice(2)})` : ''}`);
      if (model.shift) {
        const shift = model.shift, season = `${model.season}/${String(model.season + 1).slice(2)}`;
        say(() => tn(shift, {
          one: 'Dates are shown as the game uses them for {season}. This data keeps them {n} years earlier, as Update Team databases do; the editor turns them back when it saves.',
          other: 'Dates are shown as the game uses them for {season}. This data keeps them {n} years earlier, as Update Team databases do; the editor turns them back when it saves.',
        }, { season }));
      }
      dirty();
      results();
      search.focus();
    } catch (e) {
      model = null;
      list.innerHTML = '';
      say(() => t('This data set cannot be edited: {why}', { why: reason(e) }), true);
    }
  }

  // ---- The players ----

  // The list of the pane shown: the players (search) or the Scout's matches.
  const redrawList = () => (pane === 'scout' && model ? runScout() : results());

  function results() {
    const found = model.search(search.value, 80);
    list.innerHTML = found.length ? found.map((x) => `<button class="ed-row${edited.has(x.row) ? ' edited' : ''}${current === x.row ? ' on' : ''}" data-row="${x.row}">
        <b>${esc(playerName(x))}</b><span>${esc(x.club || t('No club'))} · ${esc(x.pos)} · ${x.born || ''}</span><i>${x.ca}/${x.pa}</i></button>`).join('')
      : `<p class="small">${esc(t('No player of that name.'))}</p>`;
  }

  // ---- The Scout (scout.js): a saved game's players by age, position, potential, ability and attributes ----

  // show: draw the pane now (false while a save is still opening).
  function setPane(which, show = true) {
    pane = which;
    clearTimeout(timer);                              // a search typed in Players does not fill the list behind the Scout
    for (const b of tabs.querySelectorAll('[data-pane]')) { b.setAttribute('aria-selected', String(b.dataset.pane === pane)); b.tabIndex = b.dataset.pane === pane ? 0 : -1; }
    const scouting = pane === 'scout';
    dialog.querySelector('.ed-search').hidden = scouting;
    list.hidden = scouting;
    scoutBox.hidden = !scouting;
    if (scouting) { list.innerHTML = ''; drawFilters(); }      // no list of players in the page while the Scout shows
    else found.innerHTML = '';
    if (show && model) redrawList();
  }

  const box = (key, value, max, label) => `<input type="number" inputmode="numeric" min="0" max="${max}" data-k="${key}" value="${esc(value)}" aria-label="${esc(label)}">`;
  function drawFilters() {
    const posName = positionNames(), attrName = attributeNames();
    const attrOptions = (chosen) => `<option value="">${esc(t('An attribute'))}</option>${ATTRIBUTES.map((a) => `<option value="${esc(a.name)}"${a.name === chosen ? ' selected' : ''}>${esc(attrName[a.name] ?? a.name)}</option>`).join('')}`;
    // The Wonderkids preset (scout.js): John Locke's School search, 19 or younger with potential 180 or more.
    filtersBox.innerHTML = `<span>${esc(t('Presets'))}</span><span class="pair"><button type="button" class="small" data-preset="wonderkids">${esc(t('Wonderkids'))}</button>
        <span class="small">${esc(t('19 or younger, potential 180 or more'))}</span></span>
      <span>${esc(t('Age'))}</span><span class="pair">${box('ageMin', filters.ageMin, 60, t('Age from'))} – ${box('ageMax', filters.ageMax, 60, t('Age to'))}</span>
      <span>${esc(t('Position'))}</span><span class="pair"><select data-k="position" aria-label="${esc(t('Position'))}"><option value="">${esc(t('Any position'))}</option>
        ${POSITIONS.map((p, i) => `<option value="${i}"${String(i) === String(filters.position) ? ' selected' : ''}>${esc(posName[p] ?? p)}</option>`).join('')}</select></span>
      <span>${esc(t('Potential ≥'))}</span><span class="pair">${box('paMin', filters.paMin, 200, t('Lowest potential ability'))}
        <span>${esc(t('Ability ≥'))}</span>${box('caMin', filters.caMin, 200, t('Lowest current ability'))}</span>
      ${filters.attrs.map((a, i) => `<span>${i === 0 ? esc(t('Attributes')) : ''}</span><span class="pair"><select data-attr="${i}" aria-label="${esc(t('An attribute'))}">${attrOptions(a.name)}</select>
        ≥ <input type="number" inputmode="numeric" min="1" max="20" data-min="${i}" value="${esc(a.min)}" aria-label="${esc(t('Lowest value'))}"></span>`).join('')}`;
  }

  let scoutTimer = 0, building = null;
  async function runScout() {
    if (!model || kind !== 'save') return;
    if (!index) {
      found.innerHTML = `<p class="small">${esc(t('Reading the players of this save…'))}</p>`;
      const m = model;
      building ??= later().then(() => { if (model === m) index = scoutIndex(m); }).finally(() => { building = null; });
      await building;
      if (!index || pane !== 'scout') return;
    }
    const r = scout(index, { ...filters, attrs: filters.attrs.filter((a) => a.name) });
    const level = access(), cut = freeCut(r, level), attrName = attributeNames();
    // The player's name, club and position codes are the save's own (data-raw: the language check leaves them).
    const rows = cut.rows.map((x) => `<button class="ed-row${current === x.row ? ' on' : ''}" data-row="${x.row}"><b data-raw>${esc(playerName(x))}</b>
      <span>${esc(t('{age} years', { age: x.age }))} · ${x.club ? `<bdi data-raw>${esc(x.club)}</bdi>` : esc(t('No club'))} · <bdi data-raw>${esc(x.pos)}</bdi>${Object.entries(x.values).map(([k, v]) => ` · ${esc(attrName[k] ?? k)} ${v}`).join('')}</span><i>${x.ca}/${x.pa}</i></button>`).join('');
    let after = '';
    if (!r.total) after = `<p class="small">${esc(t('No player matches these filters.'))}</p>`;
    else if (level === 'free' && cut.held) {
      const button = { signin: t('Sign in'), member: t('Become a member'), free: t('Sign in with Google, free') }[inviteKind()];
      after = `${'<div class="scout-ghost" aria-hidden="true"></div>'.repeat(Math.min(cut.held, 5))}<div class="scout-held"><p>${esc(tn(cut.held, {
        one: 'The Scout is part of Membership: {n} more player matches.', other: 'The Scout is part of Membership: {n} more players match.' }))}</p>
        ${button ? `<button class="primary" id="scout-invite" type="button">${esc(button)}</button>` : ''}</div>`;
    } else if (cut.held) after = `<p class="small">${esc(tn(cut.held, { one: 'And {n} more: narrow the filters to see them.', other: 'And {n} more: narrow the filters to see them.' }))}</p>`;
    found.innerHTML = rows + after;
  }

  const num = (key, value, min, max, label = key) => `<label><span>${esc(label)}</span>
    <input type="number" inputmode="numeric" min="${min}" max="${max}" data-f="${esc(key)}" value="${value}" data-was="${value}"${max === 20 ? ` data-band="${band(value)}"` : ''}></label>`;
  const text = (key, value, label, wide = false) => `<label class="${wide ? 'wide' : ''}"><span>${esc(label)}</span>
    <input type="text" maxlength="50" data-f="${esc(key)}" value="${esc(value)}" data-was="${esc(value)}" autocomplete="off" spellcheck="false"></label>`;
  const date = (key, value, label) => `<label><span>${esc(label)}</span><input type="date" data-f="${key}" value="${value}" data-was="${value}" min="1940-01-01" max="2015-12-31"></label>`;
  const clubLabel = (c) => `${c.name}${c.nation >= 0 && model.nationName(c.nation) ? ` (${model.nationName(c.nation)})` : ''}`;
  const select = (key, value, options, label) => `<label class="wide"><span>${esc(label)}</span><select data-f="${key}" data-was="${value}">
    ${options.map(([v, shown]) => `<option value="${v}"${v === value ? ' selected' : ''}>${esc(shown)}</option>`).join('')}</select></label>`;

  // row: a player's row, or null for a new player.
  function show(row) {
    current = row;
    const set = kind === 'set';
    const attrs = set ? DATA_ATTRIBUTES : ATTRIBUTES;
    const groupName = groupNames(), attrName = attributeNames(), posName = positionNames();
    const f = row === null ? newPlayer() : model.read(row);
    const x = row === null ? null : model.players().find((p) => p.row === row);
    const head = row === null
      ? `<div class="ed-name"><h3>${esc(t('A new player'))}</h3><span class="spacer"></span><button class="primary" id="editor-apply" type="button">${esc(t('Add this player'))}</button>
        <button id="editor-reset" type="button">${esc(t('Clear'))}</button></div><p class="small">${esc(t('Attributes left at 0 are random: the game sets them when a new game starts.'))}</p>`
      : `<div class="ed-name"><h3 id="editor-player">${esc(playerName(x))}</h3><span class="spacer"></span><button id="editor-reset" type="button" hidden>${esc(t('Undo changes'))}</button></div>
        <p class="small" id="editor-player-from">${esc(x.club || t('No club'))} · ${esc(x.nation)}</p>`;
    const clubNow = set ? clubs.find((c) => c.id === f.club) : null;
    const who = set ? `<section><h4>${esc(t('Name and club'))}</h4><div class="ed-grid">
        ${text('first', f.first, t('First name'))}${text('second', f.second, t('Surname'))}${text('common', f.common, t('Known as (optional)'))}
        ${select('nation', f.nation, nations.map((n) => [n.id, n.name]), t('Nationality'))}
        ${select('nation2', f.nation2 >= 0 ? f.nation2 : -2, [[-2, t('None')], ...nations.map((n) => [n.id, n.name])], t('Second nationality'))}
        <label class="wide"><span>${esc(t('Club'))}</span><input type="text" list="editor-clubs" data-f="club" value="${esc(clubNow ? clubLabel(clubNow) : '')}"
          data-was="${esc(clubNow ? clubLabel(clubNow) : '')}" placeholder="${esc(t('No club'))}" autocomplete="off" spellcheck="false"></label>
        ${date('contractEnd', isoDate(f.contractEnd), t('Contract ends'))}
      </div></section>` : '';
    form.innerHTML = `<div class="ed-title"><button class="link ed-back" type="button">${esc(t('← Players'))}</button>${head}</div>
      ${who}
      <section><h4>${esc(t('Ability and age'))}</h4><div class="ed-grid">
        ${num('ca', f.ca, set ? 0 : 1, 200, t('Current ability'))}${num('pa', f.pa, set ? -2 : 1, 200, t('Potential ability'))}
        ${date('born', isoDate(f.born), t('Born'))}
      </div><p class="small">${esc(set ? t('Ability is 1 to 200; 0, and for potential -1 and -2, are values the data has for players the game sets up itself.')
        : t('Ability is 1 to 200. Attributes keep the numbers you see when you change it.'))}</p></section>
      <section><h4>${esc(t('Positions'))} <span class="small">${esc(t('0 to 20; 20 is natural'))}</span></h4><div class="ed-grid pos">
        ${f.positions.map((v, i) => num(`pos${i}`, v, 0, 20, SHORT_POS[i]).replace('<span>', `<span title="${esc(posName[POSITIONS[i]] ?? POSITIONS[i])}">`)).join('')}
      </div></section>
      <div class="ed-attrs">${GROUPS.map((g) => `<section><h4>${esc(groupName[g])}${g === 'Hidden' ? ` <span class="small">${esc(t('the game does not show these'))}</span>` : ''}${set && g === 'Technical' ? ` <span class="small">${esc(t('0 is random'))}</span>` : ''}</h4><div class="ed-grid">
        ${attrs.filter((a) => a.group === g).map((a) => num(a.name, f.attrs[a.name], set ? 0 : 1, 20, attrName[a.name] ?? a.name)).join('')}</div></section>`).join('')}</div>`;
    body.classList.add('detail');
    form.scrollTop = 0;
    form.querySelector('.ed-back').onclick = () => { body.classList.remove('detail'); current = null; if (pane === 'scout') runScout(); else results(); };
    if (row === null) {
      form.onchange = null;                           // the form is the one of the player shown before: not his any more
      $('editor-reset').onclick = () => show(null);
      $('editor-apply').onclick = () => apply(null);
    } else {
      const was = f, wasEdited = edited.has(row);
      $('editor-reset').onclick = () => {
        try { model.write(row, was); } catch { /* as it was read: it writes */ }
        if (!wasEdited) edited.delete(row);
        if (index) scoutUpdate(index, model, row);
        dirty();
        show(row);
        say(() => t('{name} is as he was.', { name: playerName(x) }));
      };
      // A change counts when its field is left (the change event), as in any form: no Apply to find.
      form.onchange = (e) => { if (e.target.closest('[data-f]')) apply(row); };
    }
    form.oninput = (e) => {
      const i = e.target.closest('[data-f]');
      if (!i) return;
      i.classList.toggle('changed', i.value !== i.dataset.was);
      if (i.dataset.band) i.dataset.band = band(i.value);
    };
    if (set && row === null) {                        // the club's nation is a good guess for a new player's
      const club = form.querySelector('[data-f="club"]');
      club.onchange = () => { const c = clubByLabel(club.value); if (c && c.nation >= 0) form.querySelector('[data-f="nation"]').value = String(c.nation); };
    }
    redrawList();
  }

  function newPlayer() {
    const zero = Object.fromEntries(DATA_ATTRIBUTES.map((a) => [a.name, 0]));
    const k = model?.shift ?? 0;            // an 18-year-old with three years of contract, in the data's season
    return { first: '', second: '', common: '', born: { year: 1983 + k, month: 1, day: 1 }, nation: nations[0]?.id ?? 0, nation2: -2, club: -1,
      contractEnd: { year: 2004 + k, month: 6, day: 30 }, ca: 100, pa: 140, positions: [1, 1, 1, 1, 20, 1, 1, 1, 1, 1, 20, 1], attrs: zero };
  }

  function clubByLabel(v) {
    const s = String(v ?? '').trim();
    if (!s) return null;
    return clubs.find((c) => clubLabel(c) === s) ?? clubs.find((c) => c.name === s) ?? undefined;
  }

  // A rating's band, for its colour: the game's 1-20 scale; 0 (random in a data set) none.
  function band(v) {
    const n = Number(v);
    return !(n > 0) ? 'none' : n <= 5 ? 'low' : n <= 10 ? 'mid' : n <= 15 ? 'good' : 'high';
  }

  // After a change of a shown player: the fields the game shows otherwise (a relative attribute at a new ability)
  // take the value it shows; the name and club in the head follow; Undo shows.
  function refresh(row) {
    const f = model.read(row);
    const set = kind === 'set';
    for (const a of set ? DATA_ATTRIBUTES : ATTRIBUTES) {
      const i = form.querySelector(`[data-f="${CSS.escape(a.name)}"]`);
      if (i && document.activeElement !== i && Number(i.value) !== f.attrs[a.name]) {
        i.value = f.attrs[a.name];
        i.classList.toggle('changed', i.value !== i.dataset.was);
        if (i.dataset.band) i.dataset.band = band(i.value);
      }
    }
    const x = model.players().find((p) => p.row === row);
    if (x) {
      $('editor-player').textContent = playerName(x);
      $('editor-player-from').textContent = `${x.club || t('No club')} · ${x.nation}`;
    }
    $('editor-reset').hidden = !edited.has(row);
    redrawList();
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
        if (c === undefined) {
          const club = get('club');
          say(() => t('There is no club called "{club}". Pick one from the list, or leave it empty for no club.', { club }), true);
          return;
        }
        f.club = c ? c.id : -1;
        f.contractEnd = fromIso(get('contractEnd'));
        const r = row === null ? model.add(f) : (model.write(row, f), row);
        edited.add(r);
        dirty();
        if (row === null) show(r); else refresh(r);
        say(row === null ? () => t('Added. Save the data set to keep the changes.') : () => t('Applied. Save the data set to keep the changes.'));
        return;
      }
      const missed = model.write(row, f);
      edited.add(row);
      if (index) scoutUpdate(index, model, row);      // the Scout reads him again, not the whole save
      dirty();
      refresh(row);
      const m = Object.entries(missed);
      say(m.length ? () => {
        const attrName = attributeNames();
        const list = m.map(([k, v]) => t('{name} shows {value}', { name: attrName[k] ?? k, value: v })).join(', ');
        return t('Applied. {list}: with this ability the game cannot show it as asked. Save the game file to keep the changes.', { list });
      } : () => t('Applied. Save the game file to keep the changes.'), m.length > 0);
    } catch (e) {
      say(() => { const why = reason(e); return why[0].toUpperCase() + why.slice(1); }, true);
    }
  }

  // ---- Saving ----

  async function save() {
    saveButton.disabled = true;
    say(kind === 'set' ? () => t('Writing the data set…') : () => t('Writing the game file…'));
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
        onSetChange(name, 'saved');
        const setName = name;
        say(() => tn(changed, {
          one: 'Saved: {n} player changed in {name}. To play with it: Saved games, Databases, Use.',
          other: 'Saved: {n} players changed in {name}. To play with it: Saved games, Databases, Use.',
        }, { name: setName }));

        return;
      }
      const path = `${dir}/${name}`;
      // The save as it was is copied to the older copy (in the browser's storage, a copy of its record: nothing is
      // read), and the edited one is stored over it as a Blob of the members as they were and the changed tables. The
      // save keeps its name all the while: when it was renamed away first, a cloud sync in between found it missing
      // and brought the cloud's version back over the edit.
      if (!backedUp) {
        try { FS.mkdir(`${dir}/backups`); } catch { /* there already */ }
        await copyFile(FS, path, `${dir}/backups/${name.replace(/\.sav$/i, '')} (before edit).sav`);
        backedUp = true;
      }
      const why = await storeSave(FS, name, model.toBlob());
      if (why) throw new Error(why);
      sync();
      log(`page: ${name}: ${edited.size} player(s) edited`);
      const changed = edited.size, file = name, running = isRunning();
      say(() => tn(changed, {
        one: 'Saved: {n} player changed in {name}. The file as it was is in Saved games, Older copies.',
        other: 'Saved: {n} players changed in {name}. The file as it was is in Saved games, Older copies.',
      }, { name: file }) + (running ? ` ${t('Load the save in the game to play with the changes.')}` : ''));
      edited = new Set();
      dirty();
      redrawList();
      onSaved(name);
    } catch (e) {
      say(() => t('Nothing was written: {why}', { why: reason(e) }), true);
      dirty();
    }
  }

  // ---- Wiring ----

  let timer = 0;
  search.oninput = () => { clearTimeout(timer); timer = setTimeout(() => model && pane === 'players' && results(), 120); };
  list.onclick = (e) => { const b = e.target.closest('[data-row]'); if (b) show(Number(b.dataset.row)); };
  tabs.onclick = (e) => { const b = e.target.closest('[data-pane]'); if (b && b.dataset.pane !== pane) setPane(b.dataset.pane); };
  const tabKey = (e) => {
    if (!(e.target instanceof Node) || !tabs.contains(e.target)) return;
    const all = [...tabs.querySelectorAll('[data-pane]')], at = all.findIndex((b) => b.dataset.pane === pane);
    const to = { ArrowRight: at + 1, ArrowLeft: at - 1, Home: 0, End: all.length - 1 }[e.key];
    if (to === undefined) return;
    e.preventDefault();
    const b = all[(to + all.length) % all.length];
    if (b.dataset.pane !== pane) setPane(b.dataset.pane);
    b.focus();
  };
  if (onPageKey) onPageKey(tabKey); else tabs.addEventListener('keydown', tabKey);
  found.onclick = (e) => {
    if (e.target.closest('#scout-invite')) { invite(); return; }
    const b = e.target.closest('[data-row]');
    if (b) show(Number(b.dataset.row));
  };
  // A preset: its filters in place of the ones set, and the Scout searches at once.
  filtersBox.onclick = (e) => {
    const b = e.target.closest('[data-preset]'), f = b && presetFilters(b.dataset.preset);
    if (!f) return;
    Object.assign(filters, f);
    clearTimeout(scoutTimer);
    drawFilters();
    runScout();
  };
  // A filter changed: the Scout searches again after a pause in the typing.
  filtersBox.oninput = (e) => {
    const el = e.target;
    if (el.dataset.k) filters[el.dataset.k] = el.value;
    else if (el.dataset.attr) filters.attrs[Number(el.dataset.attr)].name = el.value;
    else if (el.dataset.min) filters.attrs[Number(el.dataset.min)].min = el.value;
    clearTimeout(scoutTimer);
    scoutTimer = setTimeout(() => runScout(), 250);
  };
  home.onclick = (e) => {
    const s = e.target.closest('[data-open-save]'), d = e.target.closest('[data-open-set]'), x = e.target.closest('[data-delete-set]');
    if (s) openSave(s.dataset.openSave);
    else if (d) openSet(d.dataset.openSet);
    else if (x) deleteSet(x.dataset.deleteSet);
  };
  $('editor-add').onclick = () => { if (model && kind === 'set') show(null); };
  $('editor-home-button').onclick = () => {
    if (edited.size && !confirm(tn(edited.size, { one: '{n} player changed and not saved. Leave without saving?', other: '{n} players changed and not saved. Leave without saving?' }))) return;
    say('');
    showHome();
  };
  saveButton.onclick = save;
  $('editor-close').onclick = () => {
    if (edited.size && !confirm(tn(edited.size, { one: '{n} player changed and not saved. Close without saving?', other: '{n} players changed and not saved. Close without saving?' }))) return;
    dialog.close();
    model = null;
    index = null;
  };
  dialog.addEventListener('cancel', (e) => { if (edited.size) { e.preventDefault(); $('editor-close').click(); } });
  // A new language: what the open editor shows, rendered again (the typed values of an open player kept).
  onLanguage(() => {
    if (!dialog.open) return;
    if (!home.hidden) {
      const typed = $('editor-new-name')?.value ?? '';
      showHome();
      $('editor-new-name').value = typed;
    } else {
      dirty();
      if (model && body.classList.contains('detail')) {
        if (pane === 'scout') drawFilters();
        const typed = [...form.querySelectorAll('[data-f]')].map((i) => [i.dataset.f, i.value]);
        show(current);
        for (const [k, v] of typed) {
          const i = form.querySelector(`[data-f="${CSS.escape(k)}"]`);
          if (i) { i.value = v; i.classList.toggle('changed', v !== i.dataset.was); }
        }
      } else if (model) {
        if (pane === 'scout') drawFilters();
        redrawList();
      }
    }
    if (said) say(...said);
  });

  return {
    openSave,
    openSet,
    // The account changed (account.js): the Scout's matches for the account as it is now.
    redraw() { if (dialog.open && model && pane === 'scout') runScout(); },
    open() { if (!dialog.open) dialog.showModal(); say(''); showHome(); },
  };
}
