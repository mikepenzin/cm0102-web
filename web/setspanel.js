// The data sets in the Saved games dialog (web/README.md, "Data sets"), as the saves are there: each with Use (for
// the next new game, in a season), Edit, Download (a zip) and Delete, and Upload for a zip or the 20 files. With
// Cloud Saves, a Cloud column and Remove, as for the saves. The work is in datasets.js.
import { DATA_SET_FILES } from './dataset.js';
import { FIRST_SEASON, LAST_SEASON, chooseForNextGame, cleanName, dataSetZip, dataSetsInZip, deleteDataSet, listDataSets,
  seasonText, uniqueName, writeDataSet } from './datasets.js';

// An uploaded data set starts on the latest season the game has (an update of the current season, most often); the
// game measures its own year from its dates of birth and moves it to the season chosen (season_dates.c).

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const MB = (n) => `${(n / 1048576).toFixed(0)} MB`;
const when = (t) => new Date(t).toLocaleString(undefined, { dateStyle: 'medium', timeStyle: 'short' });
const CLOUD = { cloud: 'In the cloud', up: 'Saving…', here: 'Only here', waiting: 'Not yet' };

// root: the user folder; module(): the engine's Module; cloud(): the data sets' sync (datasets.js) or null; edit(name),
// create(): the editor; changed(): after a data set came or went (the cloud sync runs).
export function mountSetsPanel({ FS, root, sync, log, toast, module, cloud = () => null, edit, create, changed = () => {} }) {
  const box = $('sets-list'), note = $('sets-note'), input = $('sets-file');
  const say = (text, bad = false) => { note.textContent = text; note.className = bad ? 'small bad' : 'small'; };
  let shown = [], using = null, watched = null;

  function render() {
    shown = listDataSets(FS, root);
    const c = cloud();
    if (c && c !== watched) { watched = c; c.onChange(() => { if ($('saves').open) render(); }); }
    if (!shown.length) { box.innerHTML = '<p class="small">No data sets yet. Make one from the game\'s data (New data set), or upload one.</p>'; return; }
    box.innerHTML = `<table><tr><th>Data set</th><th class="num">Size</th><th>Changed</th>${c ? '<th>Cloud</th>' : ''}<th></th></tr>
      ${shown.map((s, i) => {
        const st = c?.status(s.name);
        return `<tr><td>${esc(s.name)}</td><td class="num">${MB(s.size)}</td><td>${esc(when(s.mtime))}</td>
        ${c ? `<td class="cloud ${st}">${CLOUD[st]}${st === 'cloud' ? ` <button class="link" data-set-remove="${i}">Remove</button>` : ''}</td>` : ''}
        <td class="acts"><button class="primary" data-set-use="${i}">Use</button><button data-set-edit="${i}">Edit</button>
          <button data-set-download="${i}">Download</button><button class="link" data-set-delete="${i}">Delete</button></td></tr>
        ${using === s.name ? `<tr class="use"><td colspan="${c ? 5 : 4}"><label>Season of the next new game
          <select id="sets-season">${Array.from({ length: LAST_SEASON - FIRST_SEASON + 1 }, (_, k) => FIRST_SEASON + k)
            .map((y) => `<option value="${y}"${y === (s.season ?? FIRST_SEASON) ? ' selected' : ''}>${seasonText(y)}</option>`).join('')}</select></label>
          <button class="primary" data-set-go="${i}">Use for my next new game</button>
          <span class="small">A season database that the game knows keeps its own season. A copy of the game's data is 2001/02;
          a newer database moves to the season you choose, its players keeping their ages.</span></td></tr>` : ''}`;
      }).join('')}</table>`;
  }

  async function upload(files) {
    const done = [], refused = [];
    const taken = () => listDataSets(FS, root).map((s) => s.name);
    const zips = files.filter((f) => /\.zip$/i.test(f.name)), dats = files.filter((f) => /\.dat$/i.test(f.name));
    for (const f of zips) {
      say(`Reading ${f.name}…`);
      try {
        for (const set of await dataSetsInZip(new Uint8Array(await f.arrayBuffer()), f.name.replace(/\.zip$/i, ''))) {
          const name = uniqueName(set.name, taken());
          writeDataSet(FS, root, name, set.files, { season: LAST_SEASON });
          done.push(name);
        }
      } catch (e) { refused.push(`${f.name}: ${e.message}`); }
    }
    if (dats.length) {
      const have = new Map(dats.map((f) => [f.name.toLowerCase(), f]));
      const lacking = DATA_SET_FILES.filter((n) => !have.has(n));
      if (lacking.length) refused.push(`The ${DATA_SET_FILES.length} database files go together: ${lacking.join(', ')} ${lacking.length === 1 ? 'is' : 'are'} missing.`);
      else {
        const asked = prompt('A name for this data set:', 'Uploaded data set');
        if (asked !== null) {
          const name = uniqueName(cleanName(asked), taken());
          try {
            const files = {};
            for (const n of DATA_SET_FILES) files[n] = new Uint8Array(await have.get(n).arrayBuffer());
            writeDataSet(FS, root, name, files, { season: LAST_SEASON });
            done.push(name);
          } catch (e) { refused.push(`${name}: ${e.message}`); }
        }
      }
    }
    if (done.length) { sync(); log(`page: data set(s) uploaded: ${done.join(', ')}`); changed(); }
    render();
    say([done.length ? `Uploaded ${done.join(', ')}. Use one to start a new game with it.` : '', ...refused].filter(Boolean).join('\n'), refused.length > 0 && !done.length);
  }

  box.addEventListener('click', async (e) => {
    const b = e.target.closest('button');
    if (!b) return;
    const s = (k) => shown[Number(b.dataset[k])];
    if (b.dataset.setUse !== undefined) { using = using === s('setUse').name ? null : s('setUse').name; render(); return; }
    if (b.dataset.setGo !== undefined) {
      const set = s('setGo'), season = Number($('sets-season').value);
      try {
        const how = chooseForNextGame(module(), FS, root, set.name, season);
        using = null;
        render();
        say(how === 'now' ? `The next new game starts ${set.name} in ${seasonText(season)}: Start New Game on the game's first screen starts it. The button at the top left of that screen shows the new choice the next time the screen opens.`
          : `${set.name} in ${seasonText(season)} is chosen for the next new game from the next start of this page. Reload the page (after saving your game) to use it now.`);
        log(`page: ${set.name} chosen for the next new game, ${seasonText(season)} (${how})`);
      } catch (err) { say(`${set.name} was not chosen: ${err.message}`, true); }
      return;
    }
    if (b.dataset.setEdit !== undefined) { $('saves').close(); edit(s('setEdit').name); return; }
    if (b.dataset.setDownload !== undefined) {
      const set = s('setDownload');
      say(`Packing ${set.name}…`);
      try {
        const url = URL.createObjectURL(new Blob([await dataSetZip(FS, root, set.name)], { type: 'application/zip' }));
        const a = document.createElement('a');
        a.href = url;
        a.download = `${set.name}.zip`;
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(url), 60000);
        say(`Downloaded ${set.name}.zip. The desktop Bosman Edition takes it too: unpack it into Databases in its save folder.`);
      } catch (err) { say(`${set.name} was not packed: ${err.message}`, true); }
      return;
    }
    if (b.dataset.setDelete !== undefined) {
      const set = s('setDelete'), c = cloud();
      if (!confirm(`Delete the data set ${set.name}${c ? ' here and in the cloud' : ''}? Games started with it keep running; their saves hold their own data.`)) return;
      try {
        deleteDataSet(FS, root, set.name);
        await c?.deleted(set.name);
        sync();
        say(`${set.name} is deleted.`);
      } catch (err) { say(`${set.name} was not deleted: ${err.message}`, true); }
      render();
      return;
    }
    if (b.dataset.setRemove !== undefined) {
      const set = s('setRemove');
      try { await cloud().remove(set.name); say(`${set.name} is no longer in the cloud. It stays in this browser.`); }
      catch (err) { say(`${set.name} was not removed: ${err.message}`, true); }
      render();
    }
  });
  $('sets-upload').onclick = () => input.click();
  $('sets-new').onclick = () => { $('saves').close(); create(); };
  input.onchange = async () => { const files = [...input.files]; input.value = ''; await upload(files); };
  return { render, say };
}
