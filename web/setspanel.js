// The data sets in the Saved games dialog (web/README.md, "Data sets"), as the saves are there: each with Use (for
// the next new game, in a season), Edit, Download (a zip) and Delete, and Upload for a zip or the 20 files. With
// Cloud Saves, a Cloud column and Remove, as for the saves. The work is in datasets.js.
import { whenCell } from './saves.js';
import { onLanguage, t, tn } from './i18n.js';
import { badgeHtml, entryState, lockedHtml } from './early.js';
import { DATA_SET_FILES } from './dataset.js';
import { FIRST_SEASON, LAST_SEASON, chooseForNextGame, cleanName, dataSetZip, dataSetsInZip, deleteDataSet, fromLibrary, listDataSets,
  markFromLibrary, seasonText, uniqueName, writeDataSet } from './datasets.js';

// An uploaded data set starts on the latest season the game has (an update of the current season, most often); the
// game measures its own year from its dates of birth and moves it to the season chosen (season_dates.c).

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const MB = (n) => `${(n / 1048576).toFixed(0)} MB`;
const CLOUD = { cloud: () => t('In the cloud'), up: () => t('Saving…'), here: () => t('Only here'), waiting: () => t('Not yet') };
const why = (e) => t(e.key ?? e.message, e.vars);   // an error in the page's language (one from the server as it came)

// root: the user folder; module(): the engine's Module; cloud(): the data sets' sync (datasets.js) or null; edit(name),
// create(): the editor; changed(): after a data set came or went (the cloud sync runs). library: the server has the
// database library (library.mjs): Get a database lists it. early(): { signedIn, offer } for a database still in early
// access (early.js), signIn(): the account's way in.
export function mountSetsPanel({ FS, root, sync, log, toast, module, cloud = () => null, edit, create, changed = () => {}, library = false,
  early = () => ({ signedIn: true, offer: null }), signIn = () => {} }) {
  const box = $('sets-list'), note = $('sets-note'), input = $('sets-file'), lib = $('sets-lib');
  const say = (text, bad = false) => { note.textContent = text; note.className = bad ? 'small bad' : 'small'; };
  let shown = [], using = null, watched = null;
  let entries = [], askedAt = 0;                      // the library's list (showLibrary), and when it was asked

  const render = () => { drawOwn(); showLibrary(); };
  // The player's own data sets (the library's list is drawn by showLibrary).
  function drawOwn() {
    shown = listDataSets(FS, root);
    const c = cloud();
    if (c && c !== watched) { watched = c; c.onChange(() => { if ($('saves').open) render(); }); }
    if (!shown.length) {
      box.innerHTML = `<p class="small">${esc(entries.length ? t('No data sets yet. Get one from the list below, upload one, or make one from the game\'s data (New data set).')
        : t('No data sets yet. Upload one, or make one from the game\'s data (New data set).'))}</p>`;
      return;
    }
    box.innerHTML = `<table><tr><th>${esc(t('Data set'))}</th><th class="num">${esc(t('Size'))}</th><th>${esc(t('Changed'))}</th>${c ? `<th>${esc(t('Cloud'))}</th>` : ''}<th></th></tr>
      ${shown.map((s, i) => {
        const st = c?.status(s.name);
        return `<tr><td>${esc(s.name)}</td><td class="num">${MB(s.size)}</td>${whenCell(s.mtime)}
        ${c ? (fromLibrary(s) ? `<td class="cloud">${esc(t('From the library'))}</td>`
          : `<td class="cloud ${st}">${esc(CLOUD[st]())}${st === 'cloud' ? ` <button class="link" data-set-remove="${i}">${esc(t('Remove'))}</button>` : ''}</td>`) : ''}
        <td class="acts"><div class="btns"><button class="primary" data-set-use="${i}">${esc(t('Use'))}</button><button data-set-edit="${i}">${esc(t('Edit'))}</button>
          <button data-set-download="${i}">${esc(t('Download'))}</button><button class="link danger" data-set-delete="${i}">${esc(t('Delete'))}</button></div></td></tr>
        ${using === s.name ? `<tr class="use"><td colspan="${c ? 5 : 4}"><div class="use-row"><label>${esc(t('Season of the next new game'))}
          <select id="sets-season">${Array.from({ length: LAST_SEASON - FIRST_SEASON + 1 }, (_, k) => FIRST_SEASON + k)
            .map((y) => `<option value="${y}"${y === (s.season ?? FIRST_SEASON) ? ' selected' : ''}>${seasonText(y)}</option>`).join('')}</select></label>
          <button class="primary" data-set-go="${i}">${esc(t('Use for my next new game'))}</button></div>
          <span class="small">${esc(t('A season database that the game knows keeps its own season. A copy of the game\'s data is 2001/02; a newer database moves to the season you choose, its players keeping their ages.'))}</span></td></tr>` : ''}`;
      }).join('')}</table>`;
  }

  async function upload(files) {
    const done = [], refused = [];
    const taken = () => listDataSets(FS, root).map((s) => s.name);
    const zips = files.filter((f) => /\.zip$/i.test(f.name)), dats = files.filter((f) => /\.dat$/i.test(f.name));
    // Anything else was once left out without a word, so a database as a .rar (as many are shared) seemed to do nothing.
    for (const f of files.filter((x) => !/\.(zip|dat)$/i.test(x.name))) {
      refused.push(/\.(rar|7z)$/i.test(f.name)
        ? t('{file}: a .rar or .7z cannot be read here. Unpack it (on a phone, a file manager app can), then upload the folder as a .zip, or its .dat files.', { file: f.name })
        : t('{file}: a data set uploads as a .zip, or as its .dat files.', { file: f.name }));
    }
    for (const f of zips) {
      say(t('Reading {name}…', { name: f.name }));
      try {
        for (const set of await dataSetsInZip(new Uint8Array(await f.arrayBuffer()), f.name.replace(/\.zip$/i, ''))) {
          const name = uniqueName(set.name, taken());
          writeDataSet(FS, root, name, set.files, { season: LAST_SEASON });
          done.push(name);
        }
      } catch (e) { refused.push(t('{file}: {why}', { file: f.name, why: why(e) })); }
    }
    if (dats.length) {
      const have = new Map(dats.map((f) => [f.name.toLowerCase(), f]));
      const lacking = DATA_SET_FILES.filter((n) => !have.has(n));
      if (lacking.length) {
        refused.push(tn(lacking.length, { one: 'The {total} database files go together: {files} is missing.', other: 'The {total} database files go together: {files} are missing.' },
          { total: DATA_SET_FILES.length, files: lacking.join(', ') }));
      }
      else {
        const asked = prompt(t('A name for this data set:'), 'Uploaded data set');
        if (asked !== null) {
          const name = uniqueName(cleanName(asked), taken());
          try {
            const files = {};
            for (const n of DATA_SET_FILES) files[n] = new Uint8Array(await have.get(n).arrayBuffer());
            writeDataSet(FS, root, name, files, { season: LAST_SEASON });
            done.push(name);
          } catch (e) { refused.push(t('{file}: {why}', { file: name, why: why(e) })); }
        }
      }
    }
    if (done.length) { sync(); log(`page: data set(s) uploaded: ${done.join(', ')}`); changed(); }
    render();
    say([done.length ? t('Uploaded {names}. Use one to start a new game with it.', { names: done.join(', ') }) : '', ...refused].filter(Boolean).join('\n'), refused.length > 0 && !done.length);
  }

  box.addEventListener('click', async (e) => {
    const b = e.target.closest('button');
    if (!b) return;
    const s = (k) => shown[Number(b.dataset[k])];
    if (b.dataset.setUse !== undefined) { using = using === s('setUse').name ? null : s('setUse').name; render(); return; }
    if (b.dataset.setGo !== undefined) {
      const set = s('setGo'), season = Number($('sets-season').value);
      try {
        const how = await chooseForNextGame(module(), FS, root, set.name, season);
        sync();                                   // settings.txt to the browser's storage (the engine's call does not wait for it)
        using = null;
        render();
        const chosen = { name: set.name, season: seasonText(season) };
        say(how === 'now' ? t('The next new game starts {name} in {season}: Start New Game on the game\'s first screen starts it. The button at the top left of that screen shows the new choice the next time the screen opens.', chosen)
          : t('{name} in {season} is chosen for the next new game from the next start of this page. Reload the page (after saving your game) to use it now.', chosen));
        log(`page: ${set.name} chosen for the next new game, ${seasonText(season)} (${how})`);
      } catch (err) { say(t('{name} was not chosen: {why}', { name: set.name, why: why(err) }), true); }
      return;
    }
    if (b.dataset.setEdit !== undefined) { $('saves').close(); edit(s('setEdit').name); return; }
    if (b.dataset.setDownload !== undefined) {
      const set = s('setDownload');
      say(t('Packing {name}…', { name: set.name }));
      try {
        const url = URL.createObjectURL(new Blob([await dataSetZip(FS, root, set.name)], { type: 'application/zip' }));
        const a = document.createElement('a');
        a.href = url;
        a.download = `${set.name}.zip`;
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(url), 60000);
        say(t('Downloaded {name}.zip. The desktop Bosman Edition takes it too: unpack it into Databases in its save folder.', { name: set.name }));
      } catch (err) { say(t('{name} was not packed: {why}', { name: set.name, why: why(err) }), true); }
      return;
    }
    if (b.dataset.setDelete !== undefined) {
      const set = s('setDelete'), c = cloud();
      if (!confirm(c ? t('Delete the data set {name} here and in the cloud? Games started with it keep running; their saves hold their own data.', { name: set.name })
        : t('Delete the data set {name}? Games started with it keep running; their saves hold their own data.', { name: set.name }))) return;
      try {
        deleteDataSet(FS, root, set.name);
        await c?.deleted(set.name);
        sync();
        say(t('{name} is deleted.', { name: set.name }));
      } catch (err) { say(t('{name} was not deleted: {why}', { name: set.name, why: why(err) }), true); }
      render();
      return;
    }
    if (b.dataset.setRemove !== undefined) {
      const set = s('setRemove');
      try { await cloud().remove(set.name); say(t('{name} is no longer in the cloud. It stays in this browser.', { name: set.name })); }
      catch (err) { say(t('{name} was not removed: {why}', { name: set.name, why: why(err) }), true); }
      render();
    }
  });
  // ---- Get a database: the library of the server (library.mjs), each database for a season. ----
  // The library's list, always on the Databases tab under the player's own (asked again a minute after the last time).
  const head = $('sets-lib-head');
  // A database's title card (library.mjs, rgn.mjs: the release's intro picture), or its season in a box of the same
  // size, so the names line up. Decorative: the name beside it says what it is.
  const card = (e) => (e.card
    ? `<img class="lib-card" src="/api/library/${encodeURIComponent(e.id)}/card" width="64" height="48" alt="" loading="lazy" decoding="async">`
    : `<span class="lib-card none" aria-hidden="true">${seasonText(e.season)}</span>`);
  // The first of the tab's steps: get one from the library when it has some, else upload or make one (also on a
  // server with no library, where drawLibrary never runs).
  const drawStep = () => { $('sets-step1').innerHTML = entries.length ? t('<b>Get</b> a database below, or upload your own') : t('<b>Upload</b> a database, or make one from the game\'s data'); };
  function drawLibrary() {
    const have = new Set(listDataSets(FS, root).map((s) => s.library?.id).filter(Boolean));
    head.hidden = lib.hidden = !entries.length;
    head.innerHTML = `${esc(t('Available to download'))} <span>${esc(t('· {databases}, free', { databases: tn(entries.length, { one: '{n} database', other: '{n} databases' }) }))}</span>`;
    drawStep();
    lib.innerHTML = entries.length ? `<table><tr><th>${esc(t('Database'))}</th><th>${esc(t('Season'))}</th><th class="num">${esc(t('Size'))}</th><th></th></tr>
      ${entries.map((e, i) => { const state = entryState(e);      // early access (early.js): locked, or a Club Legend's badge
        return `<tr${state === 'locked' ? ' class="lib-locked"' : ''}><td><div class="lib-db">${card(e)}<div><span data-raw>${esc(e.name)}</span>${state === 'early' ? ` ${badgeHtml()}` : ''}${e.note ? `<div class="small" data-raw>${esc(e.note)}</div>` : ''}
          ${state === 'locked' ? lockedHtml(e.early, early()) : ''}</div></div></td><td>${seasonText(e.season)}</td>
        <td class="num">${MB(e.size)}</td><td class="acts"><div class="btns">${state === 'locked' ? ''
          : `<button class="primary" data-lib-get="${i}">${esc(have.has(e.id) ? t('Get again') : t('Get'))}</button>`}</div></td></tr>`; }).join('')}</table>` : '';
  }
  async function showLibrary() {
    if (!library) return;
    drawLibrary();
    if (Date.now() - askedAt < 60000) return;
    askedAt = Date.now();
    try {
      const r = await fetch('/api/library', { cache: 'no-store' });
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      entries = (await r.json()).entries;
    } catch (err) { askedAt = 0; log(`page: library: the list did not come (${err.message})`); }
    if (!shown.length) drawOwn();                     // its "No data sets yet" names the list below
    drawLibrary();
  }
  // One database: its zip (straight from the store where it can, else through the server), into Databases with its
  // season, then Use opens with that season.
  async function getFromLibrary(e) {
    const show = (got) => say(t('Getting {name}: {pct}%', { name: e.name, pct: Math.min(100, Math.floor((100 * got) / Math.max(1, e.size))) }));
    show(0);
    const read = async (r) => {
      if (r.status === 403) { const j = await r.json().catch(() => ({})); throw Object.assign(new Error(j.error ?? 'HTTP 403'), { key: j.key, vars: j.vars }); }
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      const out = new Uint8Array(e.size), reader = r.body.getReader();
      let at = 0;
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        if (at + value.length > out.length) throw Object.assign(new Error('more bytes than the library said'), { key: 'more bytes than the library said' });
        out.set(value, at);
        at += value.length;
        show(at);
      }
      if (at !== out.length) throw Object.assign(new Error('the download stopped before its end'), { key: 'the download stopped before its end' });
      return out;
    };
    let zip = null;
    try {
      const link = await (await fetch(`/api/library/${e.id}?direct=get`, { cache: 'no-store' })).json();
      if (link.direct) zip = await read(await fetch(link.url, { cache: 'no-store' }));
    } catch (err) { log(`page: library: ${e.name} not straight from the store (${err.message}): through the server`); }
    zip ??= await read(await fetch(`/api/library/${e.id}`, { cache: 'no-store' }));
    const [set] = await dataSetsInZip(zip, e.name);
    const name = uniqueName(set.name === e.name ? set.name : e.name, listDataSets(FS, root).map((s) => s.name));
    writeDataSet(FS, root, name, set.files, { season: Math.min(e.season, LAST_SEASON) });   // 2026/27: the game's last season
    markFromLibrary(FS, root, name, e.id);
    sync();
    changed();
    log(`page: ${name} came from the database library (${seasonText(e.season)}, ${MB(e.size)})`);
    return name;
  }
  lib.addEventListener('click', async (ev) => {
    if (ev.target.closest('[data-early-signin]')) { signIn(); return; }
    const b = ev.target.closest('button[data-lib-get]');
    if (!b) return;
    const e = entries[Number(b.dataset.libGet)];
    for (const x of lib.querySelectorAll('button')) x.disabled = true;
    try {
      const name = await getFromLibrary(e);
      using = name;                                    // Use opens, with its season chosen
      render();
      say(e.season > LAST_SEASON
        ? t('{name} is here: a {season} database. The game\'s latest season is {latest}, so a new game with it starts there, its players keeping their ages. Press Use for my next new game, then start a new game.',
          { name, season: seasonText(e.season), latest: seasonText(LAST_SEASON) })
        : t('{name} is here, for {season}. Press Use for my next new game, then start a new game.', { name, season: seasonText(e.season) }));
      box.closest('.dlg-body').scrollTop = 0;         // the new data set and its Use panel, in view
    } catch (err) {
      say(t('{name} did not come: {why}', { name: e.name, why: why(err) }), true);
      drawLibrary();
    }
  });
  $('sets-upload').onclick = () => input.click();
  $('sets-new').onclick = () => { $('saves').close(); create(); };
  input.onchange = async () => { const files = [...input.files]; input.value = ''; await upload(files); };
  drawStep();
  onLanguage(() => { drawOwn(); if (library) drawLibrary(); else drawStep(); });   // the lists in the new language (no new ask of the library)
  // The account changed (signed in, joined, became a Club Legend): the library's list again, its locks are the asker's.
  const refresh = () => { if (!library) return; askedAt = 0; showLibrary(); };
  return { render, say, refresh };
}
