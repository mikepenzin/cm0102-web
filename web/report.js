// Send to the developer (web/README.md, "Send to the developer"): after an error, or from Help, the player sends the
// engine log, a note and a save of their choice to the server (stats.mjs: send, saveLink, saveDone). The log goes
// first, so it arrives even when the save does not. The save goes packed (gzip) straight to the store, with a link the
// server signs, as the cloud saves do; when the store does not let the page in (its CORS rules: the first try on the
// live site failed so, 13 November 2026), it goes through the server (saveBody). The admin page lists the report.
import { SAVE_HEADER, listSaves, readBlob } from './saves.js';
import { toDisk } from './disk.js';
import { onLanguage, t } from './i18n.js';

export const REPORT_LOG_LINES = 2000;
// A read that starts this far into a save is a load: the Load Game list reads each save's header only, 270,336 bytes
// (the header in 4 KB reads), and a load reads the parts after it (a run of the real engine, 13 November 2026: a 46 MB
// save, the list read 270,336 bytes of each of two saves; the load opened the chosen one about 100 times, 68 MB).
const PAST_HEADER = SAVE_HEADER + 0x10000;
const LINE_MAX = 400;                                  // what the server keeps of a log line (stats.mjs)

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const mb = (n) => `${(n / 1048576).toFixed(n < 10485760 ? 1 : 0)} MB`;
const size = (n) => (n < 1048576 ? `${n === 0 ? 0 : Math.max(1, Math.round(n / 1024))} KB` : mb(n));
// What the report says of the page: its size, the device's memory (Chrome tells it, rounded), touch or mouse.
const pageInfo = () => `${innerWidth}x${innerHeight}, ${navigator.deviceMemory ? `${navigator.deviceMemory} GB of memory` : 'memory not told'}${
  matchMedia('(pointer: coarse)').matches ? ', touch' : ''}`;

async function post(path, body) {
  const r = await fetch(path, { method: 'POST', credentials: 'same-origin', headers: { 'content-type': 'application/json', 'x-cm': '1' },
    body: JSON.stringify(body) });
  let j = {};
  try { j = await r.json(); } catch { /* no body */ }
  if (!r.ok) throw j.error ? new Error(j.error) : Object.assign(new Error(`the server answered ${r.status}`), { key: 'the server answered {status}', vars: { status: r.status } });
  return j;
}

// The save packed with gzip, read from its Blob in pieces (a save of the vault from disk), into a file on the browser's
// disk where it can (disk.js): { blob, done, onDisk }, done() once it is sent. onBytes(n): read so far.
export function pack(blob, onBytes = () => {}) {
  let done = 0;
  const count = new TransformStream({ transform(chunk, c) { done += chunk.byteLength; onBytes(done); c.enqueue(chunk); } });
  return toDisk(blob.stream().pipeThrough(count).pipeThrough(new CompressionStream('gzip')));
}

// An upload with progress (fetch tells none for an upload). onPart(0..1). who: 'the store' or 'the server', for the
// messages; the server's own error text when it gives one. The page's own errors carry their English template as key
// and its values as vars, so the dialog shows them in the page's language and the log keeps the English.
function upload(method, url, headers, blob, onPart, who) {
  return new Promise((ok, bad) => {
    const x = new XMLHttpRequest();
    x.open(method, url);
    for (const [k, v] of Object.entries(headers)) x.setRequestHeader(k, v);
    x.upload.onprogress = (e) => { if (e.lengthComputable) onPart(e.loaded / e.total); };
    x.onload = () => {
      if (x.status >= 200 && x.status < 300) { ok(); return; }
      let why = null;
      try { why = JSON.parse(x.responseText).error; } catch { /* not the server's JSON */ }
      if (why) { bad(new Error(why)); return; }
      bad(Object.assign(new Error(`${who} answered ${x.status}`), who === 'the store'
        ? { key: 'the store answered {status}', vars: { status: x.status } } : { key: 'the server answered {status}', vars: { status: x.status } }));
    };
    x.onerror = () => bad(Object.assign(new Error(`the connection to ${who} failed`), who === 'the store'
      ? { key: 'the connection to the store failed' } : { key: 'the connection to the server failed' }));
    x.send(blob);
  });
}

// The save the game has in play: the last one it read from past its header (a load), or the last one it wrote in place
// (a save: temp_save_file.sav renamed). FS: the engine's FS, wrapped once.
export function watchSaveInPlay(FS) {
  let inPlay = null;
  const isSave = (p) => typeof p === 'string' && /^\/user\/[^/]+\.sav$/i.test(p) && !/\/temp_save_file\.sav$/i.test(p);
  const read = FS.read, rename = FS.rename;
  if (typeof read !== 'function' || typeof rename !== 'function') return () => null;
  FS.read = (stream, buffer, offset, length, position) => {
    const at = typeof position === 'number' ? position : stream?.position;
    const n = read(stream, buffer, offset, length, position);
    if (n > 0 && at >= PAST_HEADER && (stream.isSave ??= isSave(stream.path))) inPlay = stream.path;
    return n;
  };
  FS.rename = (from, to) => {
    const r = rename(from, to);
    if (isSave(to)) inPlay = to;
    return r;
  };
  return () => inPlay;
}

// Wires the dialog of index.html (#report). FS(): the engine's FS or null; lines(): the engine log; who(): what the
// statistics may add ({ anon, session, engine, vault, cloud }); inPlay(): the path of the save in play, or null.
// open({ message, where, files, shown }): files are Files the player gave the page that it refused (an upload): they are
// offered first, so the file that did not load can go to the developer as it is. message goes in the report as it is
// (English); shown(), when given, is the same in the page's language, for the dialog.
export function mountReport({ FS = () => null, lines = () => [], who = () => ({}), inPlay = () => null, log = () => {} }) {
  const dialog = $('report'), note = $('report-note'), choice = $('report-save'), state = $('report-state');
  const sendBtn = $('report-send');
  let context = { message: '', where: '' }, sending = false, offered = [];
  let unsent = null;              // a report whose log went and whose save did not: { sent, save }, for Send the save again
  // The dialog's state line: text() gives it in the page's language, again when the language changes.
  let stateText = () => '';
  const say = (text, bad = false) => { stateText = text; state.textContent = text(); state.className = bad ? 'small bad' : 'small'; };
  const errorWords = (err) => t(err?.key ?? err?.message ?? String(err), err?.vars);
  // The parts of an open dialog that the page writes, in the page's language (render: the save choice and the error).
  let render = () => {};
  const BUTTON = { send: () => t('Send'), sent: () => t('Sent'), again: () => t('Send the save again') };
  let button = 'send';
  const setButton = (b) => { button = b; sendBtn.textContent = BUTTON[b](); };

  function open({ message = '', where = '', files = [], shown = null } = {}) {
    if (sending) { dialog.showModal(); return; }
    context = { message, where };
    offered = [...files];
    let saves = [], fs = null;
    try { fs = FS(); saves = fs ? listSaves(fs).saves : []; } catch { /* no folder yet */ }
    // The save in play is chosen; with none, No save: a save goes only when the player picks one (privacy.html).
    const playing = inPlay();
    const pick = offered.length ? null : saves.some((f) => f.path === playing) ? playing : '';
    render = (chosen) => {
      choice.innerHTML = `<option value="">${fs || offered.length ? t('No save') : t('No save: the game did not start, so its saves cannot be read')}</option>${
        offered.map((f, i) => `<option value="file:${i}"${i === 0 ? ' selected' : ''}>${t('{name} ({size}), the file you uploaded', { name: esc(f.name), size: size(f.size) })}</option>`).join('')}${
        saves.map((f) => `<option value="${esc(f.path)}"${f.path === pick ? ' selected' : ''}>${f.path === playing ? t('{name} ({size}), in play', { name: esc(f.name), size: size(f.size) })
          : `${esc(f.name)} (${size(f.size)})`}</option>`).join('')}`;
      if (chosen !== undefined) choice.value = chosen;
      $('report-error').textContent = message ? t('The error: {message}', { message: typeof shown === 'function' ? shown() : message }) : '';
    };
    render();
    $('report-error').hidden = !message;
    unsent = null;
    say(() => '');
    sendBtn.disabled = false;
    setButton('send');
    dialog.showModal();
  }

  sendBtn.onclick = async () => {
    if (sending) return;
    sending = true;
    sendBtn.disabled = true;
    let save = null, sent = null, packed = null;
    if (unsent) ({ sent, save } = unsent);             // Send the save again: the same report, its save only
    else {
      const value = choice.value, fs = FS();
      if (value.startsWith('file:')) {
        const f = offered[Number(value.slice(5))];
        if (f) save = { name: f.name, size: f.size, blob: async () => f, uploaded: true };
      } else if (value && fs) {
        const f = listSaves(fs).saves.find((x) => x.path === value);
        if (f) save = { name: f.name, size: f.size, blob: () => readBlob(fs, f.path) };
      }
    }
    try {
      if (!sent) {
        say(() => t('Sending the log…'));
        // Each line as the server keeps it: a log of long lines (a stack is one) would not fit the request whole.
        sent = await post('/api/report/send', { ...who(), message: context.message, where: [context.where, pageInfo()].filter(Boolean).join('; '), note: note.value,
          log: lines().slice(-REPORT_LOG_LINES).map((l) => String(l).slice(0, LINE_MAX)) });
        log(`page: report sent (${sent.key})`);
      }
      if (save && !sent.saves) {
        const name = save.name;
        say(save.uploaded ? () => t('The log was sent. {name} was not: this server takes no saves. Send the file another way.', { name })
          : () => t('The log was sent. {name} was not: this server takes no saves. Download it in Saved games and send it another way.', { name }), true);
        return;
      }
      if (save) {
        const name = save.name;
        say(() => t('Packing {name}…', { name }));
        packed = await pack(await save.blob(), (n) => say(() => t('Packing {name}: {share}%', { name, share: Math.min(100, Math.floor((100 * n) / Math.max(1, save.size))) })));
        const gz = packed.blob;
        const link = await post('/api/report/save', { key: sent.key, token: sent.token, name: save.name, size: gz.size, rawSize: save.size });
        const onPart = (part) => say(() => t('Sending {name} ({size} packed): {share}%', { name, size: size(gz.size), share: Math.floor(100 * part) }));
        let direct = false;
        if (link.direct) {
          try {
            await upload('PUT', link.url, link.headers, gz, onPart, 'the store');
            direct = true;
          } catch (err) {
            // The store did not let this page in (no CORS rule for this site, or a wrong one): the server takes it.
            log(`page: report: the store did not take the save from this page (${err.message}): it goes through the server`);
          }
        }
        if (direct) await post('/api/report/save-done', { key: sent.key, token: sent.token });
        else {
          await upload('POST', `/api/report/save-body?key=${encodeURIComponent(sent.key)}&token=${encodeURIComponent(sent.token)}`,
            { 'content-type': 'application/gzip', 'x-cm': '1' }, gz, onPart, 'the server');
        }
        log(`page: report ${sent.key}: ${save.name} sent (${size(gz.size)} packed${direct ? '' : ', through the server'})`);
      }
      const name = save?.name;
      say(save ? () => t('Sent, with {name}. Thank you: the developer has it now.', { name }) : () => t('Sent. Thank you: the developer has it now.'));
      note.value = '';
      unsent = null;
      setButton('sent');
    } catch (err) {
      log(`page: report: ${err.message}`);
      const name = save?.name;
      const error = () => errorWords(err);
      if (!sent) say(() => t('Nothing was sent: {error}. Try again in a moment.', { error: error() }), true);
      else if (!save) say(() => t('The log was sent, but the save was not: {error}. Send the save again, or download it in Saved games and send it another way.', { error: error() }), true);
      else if (save.uploaded) say(() => t('The log was sent, but {name} was not: {error}. Send the save again, or send the file another way.', { name, error: error() }), true);
      else say(() => t('The log was sent, but {name} was not: {error}. Send the save again, or download it in Saved games and send it another way.', { name, error: error() }), true);
      if (sent && save) { unsent = { sent, save }; setButton('again'); }
      sendBtn.disabled = Boolean(sent && !save);
    } finally {
      sending = false;
      await packed?.done();
    }
  };
  $('report-close').onclick = () => dialog.close();
  // A language chosen while the dialog is open: its words in it at once, the choice and the note kept.
  onLanguage(() => {
    if (!dialog.open) return;
    render(choice.value);
    state.textContent = stateText();
    setButton(button);
  });
  return { open };
}
