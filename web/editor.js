// The save editor on the page (web/README.md, "Edit a saved game"): Saved games, Edit, then a search for a player,
// his fields, and Save. The model is savedit.js. The file goes back to the user folder under its own name, the
// file as it was to backups/ first, so the game's Load Game lists the edited save and Saved games the old one.
import { ATTRIBUTES, POSITIONS, SaveGame } from './savedit.js';

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const GROUPS = ['Technical', 'Mental', 'Physical', 'Goalkeeping', 'Hidden'];
const SHORT_POS = ['GK', 'SW', 'D', 'DM', 'M', 'AM', 'F', 'WB', 'R', 'L', 'C', 'FR'];
const pad = (n) => String(n).padStart(2, '0');

export function mountEditor({ FS, sync, log, toast, dir, isRunning, onSaved = () => {} }) {
  const dialog = $('editor'), list = $('editor-results'), form = $('editor-form'), note = $('editor-note');
  const search = $('editor-search'), saveButton = $('editor-save'), body = dialog.querySelector('.ed-body');
  let game = null, file = null, edited = new Set(), current = null, backedUp = false;
  const say = (text, bad = false) => { note.textContent = text; note.className = bad ? 'small bad' : 'small'; };
  const dirty = () => { saveButton.disabled = edited.size === 0; saveButton.textContent = edited.size ? `Save the game file (${edited.size} changed)` : 'Save the game file'; };

  function results() {
    const found = game.search(search.value, 80);
    list.innerHTML = found.length ? found.map((x) => `<button class="ed-row${edited.has(x.row) ? ' edited' : ''}${current === x.row ? ' on' : ''}" data-row="${x.row}">
        <b>${esc(x.name)}</b><span>${esc(x.club || 'No club')} · ${esc(x.pos)} · ${x.born || ''}</span><i>${x.ca}/${x.pa}</i></button>`).join('')
      : '<p class="small">No player of that name.</p>';
  }

  function show(row) {
    current = row;
    const x = game.players().find((p) => p.row === row);
    const f = game.read(row);
    const num = (name, value, min, max, label = name) => `<label><span>${esc(label)}</span>
      <input type="number" inputmode="numeric" min="${min}" max="${max}" data-f="${esc(name)}" value="${value}" data-was="${value}"></label>`;
    const born = f.born ? `${f.born.year}-${pad(f.born.month)}-${pad(f.born.day)}` : '';
    form.innerHTML = `<div class="ed-title"><button class="link ed-back" type="button">← Players</button>
        <h3>${esc(x.name)}</h3><p class="small">${esc(x.club || 'No club')} · ${esc(x.nation)}</p></div>
      <section><h4>Ability and age</h4><div class="ed-grid">
        ${num('ca', f.ca, 1, 200, 'Current ability')}${num('pa', f.pa, 1, 200, 'Potential ability')}
        <label><span>Born</span><input type="date" data-f="born" value="${born}" data-was="${born}" min="1940-01-01" max="2015-12-31"></label>
      </div><p class="small">Ability is 1 to 200. Attributes keep the numbers you see when you change it.</p></section>
      <section><h4>Positions <span class="small">0 to 20; 20 is natural</span></h4><div class="ed-grid pos">
        ${f.positions.map((v, i) => num(`pos${i}`, v, 0, 20, SHORT_POS[i]).replace('<span>', `<span title="${POSITIONS[i]}">`)).join('')}
      </div></section>
      ${GROUPS.map((g) => `<section><h4>${g}${g === 'Hidden' ? ' <span class="small">the game does not show these</span>' : ''}</h4><div class="ed-grid">
        ${ATTRIBUTES.filter((a) => a.group === g).map((a) => num(a.name, f.attrs[a.name], 1, 20)).join('')}</div></section>`).join('')}
      <div class="row"><button class="primary" id="editor-apply" type="button">Apply to ${esc(x.name)}</button>
        <button id="editor-reset" type="button">Undo changes</button></div>`;
    body.classList.add('detail');
    form.scrollTop = 0;
    form.querySelector('.ed-back').onclick = () => { body.classList.remove('detail'); current = null; results(); };
    $('editor-reset').onclick = () => show(row);
    $('editor-apply').onclick = () => apply(row);
    form.oninput = (e) => { const i = e.target.closest('[data-f]'); if (i) i.classList.toggle('changed', i.value !== i.dataset.was); };
    results();
  }

  function apply(row) {
    const f = game.read(row);
    const get = (k) => form.querySelector(`[data-f="${CSS.escape(k)}"]`).value;
    f.ca = Number(get('ca'));
    f.pa = Number(get('pa'));
    f.positions = f.positions.map((_, i) => Number(get(`pos${i}`)));
    for (const a of ATTRIBUTES) f.attrs[a.name] = Number(get(a.name));
    const b = get('born');
    f.born = /^\d{4}-\d{2}-\d{2}$/.test(b) ? { year: +b.slice(0, 4), month: +b.slice(5, 7), day: +b.slice(8, 10) } : null;
    const missed = game.write(row, f);
    edited.add(row);
    dirty();
    show(row);
    const m = Object.entries(missed);
    say(m.length ? `Applied. ${m.map(([k, v]) => `${k} shows ${v}`).join(', ')}: with this ability the game cannot show it as asked. Save the game file to keep the changes.`
      : 'Applied. Save the game file to keep the changes.', m.length > 0);
  }

  async function save() {
    saveButton.disabled = true;
    say('Writing the game file…');
    await new Promise((r) => setTimeout(r, 30));          // the message shows before the work
    try {
      const path = `${dir}/${file}`;
      if (!backedUp) {                                     // the file as it was, once per opening
        try { FS.mkdir(`${dir}/backups`); } catch { /* there already */ }
        FS.writeFile(`${dir}/backups/${file.replace(/\.sav$/i, '')} (before edit).sav`, FS.readFile(path));
        backedUp = true;
      }
      FS.writeFile(path, game.toBytes());
      sync();
      log(`page: ${file}: ${edited.size} player(s) edited`);
      say(`Saved: ${edited.size} player(s) changed in ${file}. The file as it was is in Saved games, Older copies.${isRunning() ? ' Load the save in the game to play with the changes.' : ''}`);
      edited = new Set();
      dirty();
      results();
      onSaved(file);
    } catch (e) {
      say(`The game file was not written: ${e.message}`, true);
      dirty();
    }
  }

  async function open(name) {
    file = name;
    edited = new Set();
    backedUp = false;
    current = null;
    $('editor-file').textContent = name;
    body.classList.remove('detail');
    form.innerHTML = '';
    list.innerHTML = '<p class="small">Opening the saved game…</p>';
    search.value = '';
    say(isRunning() ? 'If this save is loaded in the game now, save your changes here, then load it again in the game. Saving in the game first would write over them.' : '');
    dirty();
    dialog.showModal();
    await new Promise((r) => setTimeout(r, 30));
    try {
      game = new SaveGame(FS.readFile(`${dir}/${name}`));
      log(`page: ${name} opened in the editor: ${game.players().length} players`);
      results();
      search.focus();
    } catch (e) {
      game = null;
      list.innerHTML = '';
      say(`This file cannot be edited: ${e.message}`, true);
    }
  }

  let timer = 0;
  search.oninput = () => { clearTimeout(timer); timer = setTimeout(() => game && results(), 120); };
  list.onclick = (e) => { const b = e.target.closest('[data-row]'); if (b) show(Number(b.dataset.row)); };
  saveButton.onclick = save;
  $('editor-close').onclick = () => {
    if (edited.size && !confirm(`${edited.size} player(s) changed and not saved. Close without saving?`)) return;
    dialog.close();
    game = null;
  };
  dialog.addEventListener('cancel', (e) => { if (edited.size) { e.preventDefault(); $('editor-close').click(); } });
  return { open };
}
