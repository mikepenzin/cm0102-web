// The account and the cloud saves on the page (web/README.md, "Cloud saves"). The server (cloud.mjs) signs in with
// Google and keeps the saves of an account in its store; this file shows the account, and keeps the saves of the user
// folder and the cloud the same:
//
// - Before the game starts, the saves made on other devices come into the user folder, so Load Game lists them.
// - After each save, the new or changed save goes up.
// - A save changed on both sides since they were last the same (two devices at once) is kept twice, nothing is lost:
//   the other device's version under "<name> (other device).sav", this device's under its own name, in both places.
// - A save removed from the cloud (in Saved games) stays in this browser and does not go up again, unless it changes.
// - While the game runs, a save of the user folder is never replaced from the cloud: only new ones come in.
//
// What was last the same is kept in the user folder itself (.cloud-sync.json): per save, the cloud's ETag and the size
// and time of the local file at that moment.
import { SAVE_DIR, checkSave, listSaves, saveName } from './saves.js';

const STATE = `${SAVE_DIR}/.cloud-sync.json`;
const CHANGE = { 'x-cm': '1' };                   // what the server wants on a change (cloud.mjs)
const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const at = (name) => `/api/saves/${encodeURIComponent(name)}`;
const when = (t) => new Date(t).toLocaleString(undefined, { dateStyle: 'medium', timeStyle: 'short' });

// The account as the server sees it: { auth: false } (no accounts on this server), { auth: true, user: null } (signed
// out), or the user; null when the server could not be asked (no connection).
export async function fetchMe(timeoutMs = 8000) {
  try {
    const r = await fetch('/api/me', { cache: 'no-store', signal: AbortSignal.timeout(timeoutMs) });
    return r.ok ? await r.json() : null;
  } catch {
    return null;
  }
}

const gzip = (bytes) => new Response(new Blob([bytes]).stream().pipeThrough(new CompressionStream('gzip'))).blob();
const gunzip = async (stream) => new Uint8Array(await new Response(stream.pipeThrough(new DecompressionStream('gzip'))).arrayBuffer());

// A name for the other device's version of name, free among the names taken (lower case).
export function otherDeviceName(name, taken) {
  return saveName(`${name.slice(0, -4)} (other device).sav`, taken);
}

// The plan of one sync: what to do with each save, from the local files, the cloud's list and the state.
// local: Map name -> { size, mtime }, cloud: Map name -> { etag, rawSize, saved }, state: name -> { etag, size, mtime, removed }.
export function planSync(local, cloud, state, { replace = true } = {}) {
  const jobs = [], settle = {}, forget = [];
  for (const name of new Set([...local.keys(), ...cloud.keys(), ...Object.keys(state)])) {
    const l = local.get(name), c = cloud.get(name), e = state[name];
    const localChanged = Boolean(l) && (!e || e.size !== l.size || e.mtime !== l.mtime);
    if (!l && !c) { forget.push(name); continue; }
    if (!c) {                                                     // only here
      if (e && !localChanged) { if (!e.removed) settle[name] = { ...e, removed: true }; continue; }   // removed from the cloud
      jobs.push({ do: 'up', name, base: 'none' });
      continue;
    }
    if (!l) { jobs.push({ do: 'down', name }); continue; }        // only in the cloud: a new save from elsewhere
    if (!e) {
      // Both, never synced here (the first sign-in on a device with a copy): the same save when its length and time agree.
      if (c.rawSize === l.size && Math.abs((c.saved ?? 0) - l.mtime) < 2000) settle[name] = { etag: c.etag, size: l.size, mtime: l.mtime };
      else jobs.push({ do: 'both', name, cloud: c });
      continue;
    }
    const cloudChanged = c.etag !== e.etag;
    if (!localChanged && !cloudChanged) continue;
    if (!localChanged) { if (replace) jobs.push({ do: 'down', name }); continue; }
    if (!cloudChanged) { jobs.push({ do: 'up', name, base: e.etag }); continue; }
    jobs.push({ do: 'both', name, cloud: c });
  }
  return { jobs, settle, forget };
}

export function createCloudSync({ FS, sync, log, toast, account, onWrote = () => {} }) {
  let state = { account, saves: {} };
  try {
    const kept = JSON.parse(FS.readFile(STATE, { encoding: 'utf8' }));
    if (kept.account === account && kept.saves) state = kept;
  } catch { /* the first sync on this browser */ }
  let chain = Promise.resolve(), busy = new Set(), stopped = false, full = false, last = null, count = null, max = null;
  const listeners = new Set();
  const changed = () => { for (const f of listeners) f(); };
  const keep = () => { FS.writeFile(STATE, JSON.stringify(state)); sync(); };
  const stat = (name) => {
    const st = FS.stat(`${SAVE_DIR}/${name}`);
    return { size: st.size, mtime: +new Date(st.mtime) };
  };
  const fail = (what, r) => new Error(`${what}: HTTP ${r.status}`);

  async function up(name, base, signal) {
    const before = stat(name);
    const bytes = FS.readFile(`${SAVE_DIR}/${name}`);
    const r = await fetch(at(name), { method: 'PUT', body: await gzip(bytes), signal,
      headers: { ...CHANGE, 'x-base-etag': base, 'x-save-time': String(before.mtime) } });
    if (r.status === 409) return { conflict: (await r.json()).cloud };
    if (r.status === 507) {
      if (!full) toast((await r.json()).error, { kind: 'bad', ms: 8000 });
      full = true;
      return {};
    }
    if (r.status === 401 || r.status === 403) { stopped = true; return {}; }
    if (!r.ok) throw fail(`${name} up`, r);
    const j = await r.json();
    state.saves[name] = { etag: j.etag, size: bytes.length, mtime: before.mtime };
    log(`page: ${name} is in the cloud (${(j.size / 1048576).toFixed(1)} MB compressed)`);
    return { up: true };
  }

  // The cloud's save into the user folder as file (its own name, or another one).
  async function down(name, file, signal) {
    const r = await fetch(at(name), { cache: 'no-store', signal });
    if (!r.ok) throw fail(`${name} down`, r);
    const etag = r.headers.get('etag'), saved = Number(r.headers.get('x-saved')) || Date.now();
    const bytes = await gunzip(r.body);
    const why = checkSave(bytes);
    if (why) { log(`page: ${name} from the cloud was not taken: ${why}`); return null; }
    const path = `${SAVE_DIR}/${file}`;
    FS.writeFile(path, bytes);
    try { FS.utime(path, saved, saved); } catch { /* an older file system: the time of the copy */ }
    onWrote();
    return { etag, bytes, saved, ...stat(file) };
  }

  // Changed on both sides: the cloud's version comes in under another name and goes up under it, then this device's
  // version goes up under the name.
  async function keepBoth(name, cloudNow, cloudNames, signal) {
    const taken = new Set([...listSaves(FS).saves.map((f) => f.name), ...Object.keys(state.saves), ...cloudNames].map((n) => n.toLowerCase()));
    const other = otherDeviceName(name, taken);
    const theirs = await down(name, other, signal);
    if (!theirs) return;
    state.saves[other] = { etag: null, size: theirs.size, mtime: theirs.mtime };
    const o = await up(other, 'none', signal);
    if (o.conflict) return;                       // that name came up elsewhere meanwhile: the next sync sorts it out
    const mine = await up(name, theirs.etag ?? cloudNow?.etag ?? 'none', signal);
    if (mine.up) toast(`${name} was also saved on another device. That version is kept as ${other}.`, { ms: 9000 });
    log(`page: ${name} changed here and elsewhere: the other version is ${other}`);
  }

  // One sync with the cloud. replace: a save of the user folder may be replaced by a newer one of the cloud (not
  // while the game runs). onStep(text) tells what it does.
  async function once({ replace, signal, onStep = () => {} }) {
    if (stopped) return;
    const r = await fetch('/api/saves', { cache: 'no-store', signal });
    if (r.status === 401 || r.status === 403) { stopped = true; changed(); return; }
    if (!r.ok) throw fail('the cloud list', r);
    const { saves, maxSaves } = await r.json();
    count = saves.length;
    max = maxSaves;
    const local = new Map(listSaves(FS).saves.map((f) => [f.name, { size: f.size, mtime: +f.mtime }]));
    const cloud = new Map(saves.map((s) => [s.name, s]));
    const plan = planSync(local, cloud, state.saves, { replace });
    Object.assign(state.saves, plan.settle);
    for (const n of plan.forget) delete state.saves[n];
    let i = 0;
    for (const job of plan.jobs) {
      i++;
      if (full && job.do === 'up' && job.base === 'none') continue;
      busy.add(job.name);
      changed();
      onStep(`${job.do === 'down' ? 'Getting' : 'Saving'} ${job.name} (${i} of ${plan.jobs.length})`);
      try {
        if (job.do === 'down') {
          const got = await down(job.name, job.name, signal);
          if (got) state.saves[job.name] = { etag: got.etag, size: got.size, mtime: got.mtime };
        } else if (job.do === 'up') {
          const u = await up(job.name, job.base, signal);
          if (u.conflict) await keepBoth(job.name, u.conflict, cloud.keys(), signal);
        } else {
          await keepBoth(job.name, job.cloud, cloud.keys(), signal);
        }
      } finally {
        busy.delete(job.name);
        keep();
        changed();
      }
      if (stopped) break;
    }
    if (plan.jobs.length || Object.keys(plan.settle).length || plan.forget.length) keep();
    last = Date.now();
    if (plan.jobs.some((j) => j.do !== 'down')) count = null;     // the count changed; the next list tells it
    changed();
    return plan.jobs;
  }

  // Runs one sync after the ones before it. Resolves to the jobs done, or null when it failed (no connection).
  function run(opts = {}) {
    const job = chain.then(() => once(opts)).catch((err) => {
      if (err.name === 'AbortError' || err.name === 'TimeoutError') return null;
      log(`page: cloud saves: ${err.message ?? err}`);
      return null;
    });
    chain = job;
    return job;
  }

  return {
    run,
    stop() { stopped = true; changed(); },
    get stopped() { return stopped; },
    onChange(f) { listeners.add(f); return () => listeners.delete(f); },
    info: () => ({ last, count, max, full }),
    // How a save of the user folder stands: 'cloud', 'up' (on its way), 'here' (removed from the cloud) or 'waiting'.
    status(name, file) {
      if (busy.has(name)) return 'up';
      const e = state.saves[name];
      if (!e) return 'waiting';
      if (e.removed) return 'here';
      return e.size === file.size && e.mtime === +file.mtime ? 'cloud' : 'waiting';
    },
    async remove(name) {
      const r = await fetch(at(name), { method: 'DELETE', headers: CHANGE });
      if (!r.ok && r.status !== 404) throw fail(`${name} remove`, r);
      const f = listSaves(FS).saves.find((s) => s.name === name);
      state.saves[name] = f ? { etag: null, size: f.size, mtime: +f.mtime, removed: true } : undefined;
      if (!f) delete state.saves[name];
      full = false;
      count = count === null ? null : Math.max(0, count - 1);
      keep();
      changed();
    },
  };
}

// The Account button and dialog, and the sign-in. isRunning: whether a game runs (then the sign-in must not leave the
// page); leave(): the page is about to be left on purpose (no question before it).
export function mountAccount({ log, toast, isRunning, isStandalone, leave }) {
  const dialog = $('account'), body = $('account-body'), note = $('account-note'), button = $('account-open');
  let me = null, cloud = null;
  const listeners = new Set();
  const say = (text, bad = false) => { note.textContent = text; note.className = bad ? 'small bad' : 'small'; };

  function render() {
    const u = me?.user;
    button.hidden = !me?.auth;
    button.classList.toggle('on', Boolean(u?.cloud));
    button.querySelector('span').textContent = u ? (u.name || u.email).split(/[\s@]/)[0].slice(0, 14) : 'Sign in';
    button.setAttribute('aria-label', u ? `Account: ${u.email}` : 'Sign in');
    if (!me?.auth) { body.innerHTML = ''; return; }
    if (!u) {
      body.innerHTML = `<p>Sign in with Google to keep your saved games in the cloud: save on one computer or iPad, and go on with
        the same game on another.</p>
        <p class="small">Without an account the game is the same, and your saves stay in this browser.</p>
        <div class="row"><button class="primary" id="account-signin">Sign in with Google</button></div>`;
      $('account-signin').onclick = signIn;
      return;
    }
    const info = cloud?.info();
    body.innerHTML = `<p>Signed in as <b>${esc(u.email)}</b>.</p>${u.cloud
      ? `<p class="small">Cloud saves are on. Each save you make here goes to the cloud, and when the game starts, this browser
        gets the saves you made elsewhere.${info?.count != null ? ` ${info.count} of ${info.max} saves are in the cloud.` : ''}</p>
        <p class="small">${cloud?.stopped ? 'Cloud saves stopped: sign in again.' : info?.last ? `Last sync: ${esc(when(info.last))}.` : 'Not synced yet.'}</p>`
      : `<p class="small">Cloud saves are not on for this account. Your saves stay in this browser, and Saved games downloads
        them as files.</p>${me.upgradeUrl ? `<p><a class="button primary" href="${esc(me.upgradeUrl)}" target="_blank" rel="noopener">Get cloud saves</a></p>` : ''}`}
      <div class="row"><button id="account-signout">Sign out</button><button id="account-delete">Delete account…</button></div>`;
    $('account-signout').onclick = signOut;
    $('account-delete').onclick = deleteAccount;
  }

  function signIn() {
    if (!isRunning()) { leave(); location.href = '/auth/google/start?mode=page'; return; }
    const w = isStandalone() ? null : window.open('/auth/google/start?mode=popup', 'cm-sign-in', 'popup,width=500,height=680');
    if (w) { say('Finish the sign-in in the small window.'); return; }
    // An app on the home screen, or a blocked window: the sign-in leaves the page, so the game must be saved first.
    body.querySelector('.row').innerHTML = '<button class="primary" id="account-signin-leave">Saved: sign in now</button>';
    say('Signing in here leaves this page and starts it again. Save your game in the game first.');
    $('account-signin-leave').onclick = () => { leave(); location.href = '/auth/google/start?mode=page'; };
  }

  async function signOut() {
    await fetch('/auth/logout', { method: 'POST', headers: CHANGE }).catch(() => {});
    cloud?.stop();
    await refresh();
    say('Signed out. Your saves stay in this browser.');
  }

  async function deleteAccount() {
    if (!confirm('Delete your account and every save of it in the cloud? The saves in this browser stay.')) return;
    const r = await fetch('/api/account', { method: 'DELETE', headers: CHANGE }).catch(() => null);
    if (!r?.ok) { say('The account was not deleted: try again.', true); return; }
    cloud?.stop();
    await refresh();
    say('Your account and its cloud saves are deleted. The saves in this browser stay.');
    log('page: account deleted');
  }

  const key = (m) => `${m?.user?.email ?? ''}|${Boolean(m?.user?.cloud)}`;
  async function refresh() {
    const before = key(me);
    me = (await fetchMe()) ?? me;
    render();
    if (key(me) !== before) for (const f of listeners) f(me);
    return me;
  }

  button.onclick = () => { say(''); render(); dialog.showModal(); };
  $('account-close').onclick = () => dialog.close();
  // The small sign-in window tells this page when it is done (cloud.mjs, popupPage).
  try {
    new BroadcastChannel('cm-account').onmessage = async (e) => {
      if (!e.data?.ok) { say(`Not signed in: ${e.data?.why ?? 'unknown'}`, true); return; }
      await refresh();
      if (me?.user) toast(`Signed in as ${me.user.email}`);
    };
  } catch { /* no BroadcastChannel: the dialog's next opening shows the account */ }
  // Back from the sign-in that left the page.
  const q = new URLSearchParams(location.search);
  if (q.has('account')) {
    const ok = q.get('account') === 'signed-in', why = q.get('why');
    history.replaceState(null, '', '/');
    if (!ok) setTimeout(() => toast(`Not signed in: ${why ?? 'unknown'}`, { kind: 'bad', ms: 8000 }));
  }

  const ready = refresh();
  if (q.get('account') === 'signed-in') {
    ready.then((m) => { if (m?.user) toast(`Signed in as ${m.user.email}${m.user.cloud ? '. Cloud saves are on.' : ''}`, { ms: 6000 }); });
  }
  return {
    ready,                                       // resolves to the account, once asked
    get me() { return me; },
    setCloud(c) { cloud = c; c?.onChange(() => { if (dialog.open) render(); }); render(); },
    onAccountChange(f) { listeners.add(f); },
  };
}
