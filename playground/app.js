// The playground app: screens that only show what the engine answers and send what the player does. No game rules
// live here; every check is the engine's, and its refusals are shown as they come.
import { EngineClient } from './engine-client.js';

const engine = new EngineClient(new Worker(new URL('./engine-worker.js', import.meta.url), { type: 'module' }));
const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const money = (n) => `£${String(n).replace(/\B(?=(\d{3})+(?!\d))/g, ',')}`;
const day = (iso) => { const [y, m, d] = iso.split('-').map(Number); return `${d}.${m}.${y}`; };
const SAVE_KEY = 'cm0102-playground-save';

let tab = 'squad', openPlayer = null, live = null, running = 0;

// ---- the traffic log ----

engine.onLog = (x) => {
  const div = document.createElement('div');
  div.className = `${x.kind}${x.stale ? ' stale' : ''}`;
  let head, body;
  if (x.kind === 'call') { head = `→ #${x.id} ${x.method}`; body = x.params; }
  else if (x.kind === 'answer') { head = `← #${x.id} ${x.method} · step ${x.step} · ${day(x.date)}`; body = x.result; }
  else if (x.kind === 'refused') { head = `✕ #${x.id} ${x.method}: ${x.error.code}`; body = x.error; }
  else if (x.kind === 'event') { head = `⚡ ${x.event} · step ${x.data.step}${x.data.text ? ` · ${x.data.text}` : ''}`; body = x.data; }
  else head = x.text;
  let json = body && Object.keys(body).length ? JSON.stringify(body, null, 1) : '';
  if (json.length > 2500) json = `${json.slice(0, 2500)}…`;
  div.innerHTML = json ? `<details><summary>${esc(head)}</summary><pre>${esc(json)}</pre></details>` : esc(head);
  $('log').prepend(div);
  while ($('log').childElementCount > 300) $('log').lastElementChild.remove();
};

// ---- the frame: header, banner, busy ----

function banner(kind, text) {
  $('banner').innerHTML = text ? `<div class="banner ${kind}">${esc(text)}</div>` : '';
}

function refusedBanner(err) {
  banner('refused', err.code === 'busy' ? 'The engine is busy: it refused the call. Wait for it to finish.'
    : `The engine refused: ${err.message} (${err.code})`);
}

function setRunning(delta, text = '') {
  running += delta;
  $('busy').textContent = running > 0 ? text || $('busy').textContent : '';
}

async function header() {
  const { result: s, date } = await engine.call('getState');
  $('club').textContent = s.club;
  $('facts').innerHTML = `<b>${day(date)}</b> · ${money(s.money)} · eleven ${s.selectionSize}/11`;
  $('decisions').innerHTML = s.decisions ? `<span class="badge">${s.decisions}</span>` : '';
}

async function refresh() {
  try {
    await header();
    await render();
  } catch (err) {
    refusedBanner(err);
  }
}

// ---- the screens ----

const short = (k) => k.slice(0, 3).replace(/^./, (c) => c.toUpperCase());
const statusClass = (s) => `st-${s.split(',')[0]}`;

async function squadView() {
  const { result: squad } = await engine.call('getSquad');
  const rows = squad.map((p) => `<tr>
    <td><input type="checkbox" data-pick="${p.id}" ${p.selected ? 'checked' : ''} aria-label="Pick ${esc(p.name)}" style="width:auto"></td>
    <td><button class="link" data-action="player" data-id="${p.id}">${esc(p.name)}</button></td>
    <td>${p.position}</td><td class="num">${p.age}</td>
    <td>${Object.entries(p.attributes).map(([k, v]) => `${short(k)} ${v}`).join(' · ')}</td>
    <td><span class="cond"><i style="width:${p.condition}%"></i></span>${p.condition}%</td>
    <td>${esc(p.morale)}</td><td class="${statusClass(p.status)}">${esc(p.status)}</td></tr>`).join('');
  return `<h2>Squad</h2>
    <p class="note">Tick eleven and press Save eleven. The engine checks the eleven; this page does not.</p>
    <div class="scroll"><table><tr><th>XI</th><th>Name</th><th>Pos</th><th class="num">Age</th><th>Attributes</th>
    <th>Condition</th><th>Morale</th><th>Status</th></tr>${rows}</table></div>
    <div class="choices"><button class="primary" data-action="save-eleven">Save eleven</button></div>
    ${openPlayer ? await playerCard(openPlayer) : ''}`;
}

async function playerCard(id) {
  const { result: p } = await engine.call('getPlayer', { id });
  return `<div class="card"><div class="row"><b>${esc(p.name)}</b> · ${p.position} · ${p.age} · ${esc(p.club)}
    <button class="link" data-action="close-player" style="float:right">Close</button></div>
    <div class="note" style="margin:4px 0 0">Value ${money(p.value)} · ${money(p.contract.wage)} a week until ${day(p.contract.ends)} ·
    ${p.appearances} games, ${p.goals} goals · condition ${p.condition}% · morale ${esc(p.morale)} · ${esc(p.status)}</div>
    <div class="attrs">${Object.entries(p.attributes).map(([k, v]) => `<span><span>${esc(k)}</span><b>${v}</b></span>`).join('')}</div></div>`;
}

async function transfersView() {
  const { result: targets } = await engine.call('getTransferTargets');
  const rows = targets.map((t) => {
    const b = t.yourBid;
    const bid = b ? `${money(b.fee)}: ${b.status}${b.status === 'waiting' ? `, answer on ${day(b.answerDate)}` : ''}` : '—';
    return `<tr><td><button class="link" data-action="player" data-id="${t.id}">${esc(t.name)}</button></td><td>${t.position}</td>
      <td class="num">${t.age}</td><td>${esc(t.club)}</td><td class="num">${money(t.value)}</td><td class="num">${money(t.asking)}</td>
      <td>${esc(bid)}</td><td><input inputmode="numeric" data-fee="${t.id}" placeholder="Fee" aria-label="Fee for ${esc(t.name)}">
      <button data-action="bid" data-id="${t.id}">Bid</button></td></tr>`;
  }).join('');
  return `<h2>Transfers</h2>
    <p class="note">Bid what you like. The engine answers in the inbox one or two game days later: at the asking price it accepts,
    a little below it counters, far below it refuses.</p>
    <div class="scroll"><table><tr><th>Name</th><th>Pos</th><th class="num">Age</th><th>Club</th><th class="num">Value</th>
    <th class="num">Asking</th><th>Your bid</th><th></th></tr>${rows || '<tr><td colspan="8">Nobody is on the list.</td></tr>'}</table></div>
    ${openPlayer ? await playerCard(openPlayer) : ''}`;
}

async function inboxView() {
  const { result: items } = await engine.call('getInbox');
  return `<h2>Inbox</h2>${items.map((i) => `<div class="news">
    <div class="when">${day(i.date)} · ${esc(i.kind)}</div><div class="title">${esc(i.title)}</div><div>${esc(i.text)}</div>
    ${i.choices && !i.answered ? `<div class="choices">${i.choices.map((c) =>
      `<button data-action="reply" data-id="${i.id}" data-choice="${esc(c)}">${esc(c)}</button>`).join('')}</div>` : ''}
    ${i.answered ? `<div class="answered">You answered: ${esc(i.answered)}</div>` : ''}</div>`).join('')}`;
}

async function matchesView() {
  const { result: fixtures } = await engine.call('getFixtures');
  const rows = fixtures.map((f) => `<tr${f.ours ? '' : ' style="color:var(--muted)"'}><td>${day(f.date)}</td><td>${esc(f.home)}</td>
    <td>${esc(f.away)}</td><td>${esc(f.competition)}</td><td class="num">${f.played ? f.score.join('–') : '—'}</td>
    <td>${f.ours && f.today && !f.played ? `<button class="primary" data-action="play" data-id="${f.id}">Play match</button>`
      : f.ours && f.played ? `<button class="link" data-action="commentary" data-id="${f.id}">Commentary</button>` : ''}</td></tr>`).join('');
  return `<h2>Matches</h2>
    <p class="note">On match day, play the match. The commentary comes from the engine line by line; the result changes the players.</p>
    <div class="scroll"><table><tr><th>Date</th><th>Home</th><th>Away</th><th>Competition</th><th class="num">Score</th><th></th></tr>${rows}</table></div>
    <div id="live" class="live"></div>`;
}

async function tableView() {
  const { result: rows } = await engine.call('getTable');
  return `<h2>League table</h2><div class="scroll"><table><tr><th>#</th><th>Club</th><th class="num">P</th><th class="num">W</th>
    <th class="num">D</th><th class="num">L</th><th class="num">F</th><th class="num">A</th><th class="num">Pts</th></tr>
    ${rows.map((r, i) => `<tr><td>${i + 1}</td><td>${esc(r.club)}</td><td class="num">${r.played}</td><td class="num">${r.won}</td>
    <td class="num">${r.drawn}</td><td class="num">${r.lost}</td><td class="num">${r.for}</td><td class="num">${r.against}</td>
    <td class="num"><b>${r.points}</b></td></tr>`).join('')}</table></div>`;
}

const VIEWS = { squad: squadView, transfers: transfersView, inbox: inboxView, matches: matchesView, table: tableView };

async function render() {
  for (const b of document.querySelectorAll('nav button')) b.setAttribute('aria-selected', String(b.dataset.tab === tab));
  $('view').innerHTML = await VIEWS[tab]();
  if (tab === 'matches') drawLive();
}

function drawLive() {
  const el = $('live');
  if (!el || !live) return;
  el.innerHTML = `<div class="note">${esc(live.home)} against ${esc(live.away)}${live.done ? ' · full time' : ' · live'}</div>
    <div class="score">${esc(live.home)} ${esc(live.score)} ${esc(live.away)}</div>
    <div class="lines">${live.lines.map((l) => `<div class="${l.goal ? `goal-${l.goal}` : ''}">${l.minute}' ${esc(l.text)}</div>`).join('')}</div>`;
  const box = el.querySelector('.lines');
  if (box) box.scrollTop = box.scrollHeight;
}

// ---- what the player does ----

engine.on('progress', (d) => { $('busy').textContent = `Playing days… ${day(d.date)}`; });
engine.on('commentary', (d) => {
  if (!live || live.matchId !== d.matchId) return;
  live.lines.push(d);
  live.score = d.score;
  drawLive();
});
engine.on('result', (d) => { if (live && live.matchId === d.matchId) { live.done = true; drawLive(); } });

for (const b of document.querySelectorAll('nav button')) b.onclick = () => { tab = b.dataset.tab; openPlayer = null; banner(); refresh(); };

for (const b of document.querySelectorAll('[data-continue]')) {
  b.onclick = async () => {
    const v = b.dataset.continue;
    banner();
    setRunning(1, 'Playing days…');
    try {
      const { result } = await engine.call('continue', v === 'match' ? { toMatch: true } : { days: Number(v) });
      if (result.stopped === 'match') {
        const m = result.matchToday;
        banner('match', `Match day: ${m.home} against ${m.away}. Play it in Matches.`);
        tab = 'matches';
      } else if (result.stopped === 'decision') {
        banner('decision', `Stopped after ${result.daysPlayed} day(s): a decision waits in your inbox.`);
        tab = 'inbox';
      } else {
        banner('info', `${result.daysPlayed} day(s) played.`);
      }
    } catch (err) {
      refusedBanner(err);
    } finally {
      setRunning(-1);
    }
    if (running === 0) await refresh();
  };
}

$('view').addEventListener('click', async (e) => {
  const b = e.target.closest('[data-action]');
  if (!b) return;
  const { action, id } = b.dataset;
  if (!['player', 'close-player', 'commentary'].includes(action)) banner();
  try {
    if (action === 'player') { openPlayer = id; await render(); return; }
    if (action === 'close-player') { openPlayer = null; await render(); return; }
    if (action === 'save-eleven') {
      const ids = [...document.querySelectorAll('[data-pick]:checked')].map((x) => x.dataset.pick);
      await engine.call('selectTeam', { ids });
      banner('info', 'The engine took your eleven.');
    } else if (action === 'bid') {
      const raw = document.querySelector(`[data-fee="${id}"]`).value.replace(/[£,\s]/g, '');
      const { result } = await engine.call('bid', { playerId: id, fee: Number(raw) });
      banner('info', `Bid sent. The answer comes on ${day(result.answerDate)}.`);
    } else if (action === 'reply') {
      await engine.call('reply', { newsId: id, choice: b.dataset.choice });
      banner('info', `You answered: ${b.dataset.choice}.`);
    } else if (action === 'play') {
      const { result: m } = await engine.call('getMatch', { id });
      live = { matchId: id, home: m.home, away: m.away, lines: [], score: '0–0', done: false };
      drawLive();
      setRunning(1, 'Match in progress…');
      try { await engine.call('playMatch', { id }); } finally { setRunning(-1); }
      banner('info', 'Full time. The result is in the inbox; condition and morale have changed in the squad.');
    } else if (action === 'commentary') {
      const { result: m } = await engine.call('getMatch', { id });
      live = { matchId: id, home: m.home, away: m.away, lines: m.commentary, score: m.score.join('–'), done: true };
      drawLive();
      return;
    }
  } catch (err) {
    refusedBanner(err);
    return;
  }
  if (running === 0) await refresh();
});

$('save').onclick = async () => {
  try {
    const { result } = await engine.call('save');
    try { localStorage.setItem(SAVE_KEY, result.blob); banner('info', `Saved in this browser (${Math.round(result.blob.length / 1024)} KB).`); }
    catch { banner('refused', 'This browser would not keep the save.'); }
  } catch (err) { refusedBanner(err); }
};

$('load').onclick = async () => {
  let blob = null;
  try { blob = localStorage.getItem(SAVE_KEY); } catch { /* storage blocked */ }
  if (!blob) { banner('refused', 'No save in this browser yet.'); return; }
  try { await engine.call('load', { blob }); live = null; banner('info', 'Loaded the save.'); await refresh(); }
  catch (err) { refusedBanner(err); }
};

$('new').onclick = async () => {
  try { await engine.call('newGame'); live = null; openPlayer = null; tab = 'squad'; banner('info', 'A new game.'); await refresh(); }
  catch (err) { refusedBanner(err); }
};

refresh();
