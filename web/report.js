// Send to the developer (web/README.md, "Send to the developer"): after an error, or from Help, the player sends the
// engine log, a note and a save of their choice to the server (stats.mjs: send, saveLink, saveDone). The log goes
// first, so it arrives even when the save does not. The save goes packed (gzip) straight to the store, with a link the
// server signs, as the cloud saves do; when the store does not let the page in (its CORS rules: the first try on the
// live site failed so, 13 November 2026), it goes through the server (saveBody). The admin page lists the report.
import { listSaves, readBlob } from './saves.js';

export const REPORT_LOG_LINES = 2000;
const HEADER = 0x416ec;                                // a save's container header (saves.js)
// A read that starts this far into a save is a load: the Load Game list reads each save's header only, 270,336 bytes
// (the header in 4 KB reads), and a load reads the parts after it (a run of the real engine, 13 November 2026: a 46 MB
// save, the list read 270,336 bytes of each of two saves; the load opened the chosen one about 100 times, 68 MB).
const PAST_HEADER = HEADER + 0x10000;

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
  if (!r.ok) throw new Error(j.error ?? `the server answered ${r.status}`);
  return j;
}

// The save packed with gzip, read from its Blob in pieces (a save of the vault from disk). onBytes(n): read so far.
export function pack(blob, onBytes = () => {}) {
  let done = 0;
  const count = new TransformStream({ transform(chunk, c) { done += chunk.byteLength; onBytes(done); c.enqueue(chunk); } });
  return new Response(blob.stream().pipeThrough(count).pipeThrough(new CompressionStream('gzip'))).blob();
}

// An upload with progress (fetch tells none for an upload). onPart(0..1). who: 'the store' or 'the server', for the
// messages; the server's own error text when it gives one.
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
      bad(new Error(why ?? `${who} answered ${x.status}`));
    };
    x.onerror = () => bad(new Error(`the connection to ${who} failed`));
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
// open({ message, where, files }): files are Files the player gave the page that it refused (an upload): they are
// offered first, so the file that did not load can go to the developer as it is.
export function mountReport({ FS = () => null, lines = () => [], who = () => ({}), inPlay = () => null, log = () => {} }) {
  const dialog = $('report'), note = $('report-note'), choice = $('report-save'), state = $('report-state');
  const sendBtn = $('report-send');
  let context = { message: '', where: '' }, sending = false, offered = [];
  const say = (text, bad = false) => { state.textContent = text; state.className = bad ? 'small bad' : 'small'; };

  function open({ message = '', where = '', files = [] } = {}) {
    if (sending) { dialog.showModal(); return; }
    context = { message, where };
    offered = [...files];
    let saves = [], fs = null;
    try { fs = FS(); saves = fs ? listSaves(fs).saves : []; } catch { /* no folder yet */ }
    const playing = inPlay();
    const pick = offered.length ? null : saves.some((f) => f.path === playing) ? playing : saves[0]?.path ?? '';
    choice.innerHTML = `<option value="">${fs || offered.length ? 'No save' : 'No save: the game did not start, so its saves cannot be read'}</option>${
      offered.map((f, i) => `<option value="file:${i}"${i === 0 ? ' selected' : ''}>${esc(f.name)} (${size(f.size)}), the file you uploaded</option>`).join('')}${
      saves.map((f) => `<option value="${esc(f.path)}"${f.path === pick ? ' selected' : ''}>${esc(f.name)} (${size(f.size)})${f.path === playing ? ', in play' : ''}</option>`).join('')}`;
    $('report-error').textContent = message ? `The error: ${message}` : '';
    $('report-error').hidden = !message;
    say('');
    sendBtn.disabled = false;
    sendBtn.textContent = 'Send';
    dialog.showModal();
  }

  sendBtn.onclick = async () => {
    if (sending) return;
    sending = true;
    sendBtn.disabled = true;
    const value = choice.value, fs = FS();
    let save = null;
    if (value.startsWith('file:')) {
      const f = offered[Number(value.slice(5))];
      if (f) save = { name: f.name, size: f.size, blob: async () => f, uploaded: true };
    } else if (value && fs) {
      const f = listSaves(fs).saves.find((x) => x.path === value);
      if (f) save = { name: f.name, size: f.size, blob: () => readBlob(fs, f.path) };
    }
    const elsewhere = () => (save?.uploaded ? 'Send the file another way.' : 'Download it in Saved games and send it another way.');
    let sent = null;
    try {
      say('Sending the log…');
      sent = await post('/api/report/send', { ...who(), message: context.message, where: [context.where, pageInfo()].filter(Boolean).join('; '), note: note.value,
        log: lines().slice(-REPORT_LOG_LINES) });
      log(`page: report sent (${sent.key})`);
      if (save && !sent.saves) {
        say(`The log was sent. ${save.name} was not: this server takes no saves. ${elsewhere()}`, true);
        return;
      }
      if (save) {
        say(`Packing ${save.name}…`);
        const gz = await pack(await save.blob(), (n) => say(`Packing ${save.name}: ${Math.min(100, Math.floor((100 * n) / Math.max(1, save.size)))}%`));
        const link = await post('/api/report/save', { key: sent.key, token: sent.token, name: save.name, size: gz.size, rawSize: save.size });
        const onPart = (part) => say(`Sending ${save.name} (${size(gz.size)} packed): ${Math.floor(100 * part)}%`);
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
      say(`Sent${save ? `, with ${save.name}` : ''}. Thank you: the developer has it now.`);
      note.value = '';
      sendBtn.textContent = 'Sent';
    } catch (err) {
      log(`page: report: ${err.message}`);
      say(sent ? `The log was sent, but ${save?.name ?? 'the save'} was not: ${err.message}. ${elsewhere()}`
        : `Nothing was sent: ${err.message}. Try again in a moment.`, true);
      if (!sent) sendBtn.disabled = false;
    } finally {
      sending = false;
    }
  };
  $('report-close').onclick = () => dialog.close();
  return { open };
}
