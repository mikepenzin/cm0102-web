// The mock engine: a hardcoded stand-in for the original engine behind the Game API (v0 of the concept design).
//
// It has no football logic worth the name. Every outcome is scripted or a seeded roll, so the app can be tested
// against each kind of interaction it will have with the real engine:
//   reads        getState, getSquad, getPlayer, getTransferTargets, getInbox, getFixtures, getMatch, getTable
//   actions      selectTeam, bid, reply, continue, playMatch, save, load, newGame
//   refusals     the engine checks every action and refuses with a code; the app holds no rules
//   late answers a bid is answered 1 or 2 game days later, in the inbox
//   own events   on day 5 another club bids for one of your players; injuries heal; news arrives
//   busy         while days or a match run, every call is refused with "busy", as the real engine is in its day loop
//   live match   commentary lines come as events, then the result
//   save, load   the whole state as one blob, as a save is the game
// Every answer carries the step (a counter that only grows) and the game date, so the app can drop a stale one.
import { CLUB, newWorld } from './data.js';

export const API_VERSION = 0;

export class EngineError extends Error {
  constructor(code, message) {
    super(message);
    this.code = code;
  }
}

const DAY_MS = 86400000;
const START = Date.UTC(2001, 6, 14);
const MORALE = ['Very poor', 'Very poor', 'Poor', 'Poor', 'Okay', 'Okay', 'Good', 'Good', 'Superb', 'Superb'];
const ORDER = { GK: 0, D: 1, M: 2, F: 3 };
const sleep = (ms) => (ms > 0 ? new Promise((r) => setTimeout(r, ms)) : Promise.resolve());
export const money = (n) => `£${String(n).replace(/\B(?=(\d{3})+(?!\d))/g, ',')}`;

const READS = ['getState', 'getSquad', 'getPlayer', 'getTransferTargets', 'getInbox', 'getFixtures', 'getMatch',
  'getTable'];
const ACTIONS = ['selectTeam', 'bid', 'reply', 'continue', 'playMatch', 'save', 'load', 'newGame'];

export class MockEngine {
  // dayMs: real time per game day; tickMs: real time between commentary lines. 0 in the tests.
  constructor({ dayMs = 0, tickMs = 0, onEvent = () => {} } = {}) {
    this.dayMs = dayMs;
    this.tickMs = tickMs;
    this.onEvent = onEvent;
    this.busy = false;
    this.step = 1;
    this.s = newWorld();
  }

  // The one entry point: {step, date, result}, or an EngineError.
  async call(method, params) {
    if (!READS.includes(method) && !ACTIONS.includes(method)) throw new EngineError('unknown_method', `no call ${method}`);
    if (this.busy) throw new EngineError('busy', 'The engine is playing days or a match. Wait for it to finish.');
    const result = await this[`api_${method}`](params ?? {});
    return { step: this.step, date: this.date(), result };
  }

  date(day = this.s.day) { return new Date(START + day * DAY_MS).toISOString().slice(0, 10); }

  emit(event, data) { this.onEvent(event, { step: this.step, date: this.date(), ...data }); }

  // A seeded roll (mulberry32) whose state is part of the save.
  rand() {
    let t = (this.s.rng = (this.s.rng + 0x6d2b79f5) >>> 0);
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  }

  ours() { return Object.values(this.s.players).filter((p) => p.club === CLUB); }

  clubName(id) { return this.s.clubs[id].name; }

  matchToday() {
    return this.s.fixtures.find((f) => f.day === this.s.day && !f.played && (f.home === CLUB || f.away === CLUB)) ?? null;
  }

  news(kind, title, text, choices = null, extra = {}) {
    const item = { id: `n${this.s.nextId++}`, day: this.s.day, kind, title, text, choices, answered: null, ...extra };
    this.s.inbox.push(item);
    if (choices) this.decisionArrived = true;
    this.emit('news', { item: this.newsView(item) });
    return item;
  }

  // Views: what the app may see. Hidden values (ability) never leave the engine.
  playerView(p) {
    return {
      id: p.id, name: p.name, position: p.position, age: p.age, club: this.clubName(p.club),
      attributes: { ...p.attributes }, condition: p.condition, morale: MORALE[p.morale - 1],
      status: p.injuredDays > 0 ? `Injured, ${p.injuredDays} days` : p.condition < 75 ? 'Tired' : 'Fit',
      injuredDays: p.injuredDays, contract: { wage: p.wage, ends: p.contractEnds }, value: p.value,
      selected: this.s.selection.includes(p.id), appearances: p.apps, goals: p.goals,
    };
  }

  newsView(i) {
    return { id: i.id, date: this.date(i.day), kind: i.kind, title: i.title, text: i.text, choices: i.choices,
      answered: i.answered };
  }

  fixtureView(f) {
    return {
      id: f.id, date: this.date(f.day), home: this.clubName(f.home), away: this.clubName(f.away),
      competition: f.competition, played: f.played, score: f.score, scorers: f.scorers,
      ours: f.home === CLUB || f.away === CLUB, today: f.day === this.s.day,
    };
  }

  // ---- reads ----

  async api_getState() {
    const s = this.s, today = this.matchToday();
    return {
      apiVersion: API_VERSION, club: this.clubName(CLUB), money: s.money, squadSize: this.ours().length,
      selectionSize: s.selection.length,
      decisions: s.inbox.filter((i) => i.choices && !i.answered).length,
      matchToday: today ? this.fixtureView(today) : null,
    };
  }

  async api_getSquad() {
    return this.ours().sort((a, b) => ORDER[a.position] - ORDER[b.position] || a.id.localeCompare(b.id, 'en', { numeric: true }))
      .map((p) => this.playerView(p));
  }

  async api_getPlayer({ id }) {
    const p = this.s.players[id];
    if (!p) throw new EngineError('not_found', `No player ${id}.`);
    return this.playerView(p);
  }

  async api_getTransferTargets() {
    return Object.values(this.s.players).filter((p) => p.target).map((p) => {
      const bid = this.s.bids.filter((b) => b.playerId === p.id).at(-1);
      return {
        ...this.playerView(p), asking: p.asking,
        yourBid: bid ? { fee: bid.fee, status: bid.status, answerDate: this.date(bid.answerDay) } : null,
      };
    });
  }

  async api_getInbox() { return [...this.s.inbox].reverse().map((i) => this.newsView(i)); }

  async api_getFixtures() { return this.s.fixtures.map((f) => this.fixtureView(f)); }

  async api_getMatch({ id }) {
    const f = this.s.fixtures.find((x) => x.id === id);
    if (!f) throw new EngineError('not_found', `No match ${id}.`);
    return { ...this.fixtureView(f), commentary: f.commentary };
  }

  async api_getTable() {
    const rows = Object.fromEntries(this.s.league.map((c) => [c, { club: this.clubName(c), played: 0, won: 0, drawn: 0,
      lost: 0, for: 0, against: 0, points: 0 }]));
    for (const f of this.s.fixtures) {
      if (!f.played || f.competition !== 'League') continue;
      const [h, a] = f.score, H = rows[f.home], A = rows[f.away];
      H.played++; A.played++; H.for += h; H.against += a; A.for += a; A.against += h;
      if (h > a) { H.won++; A.lost++; H.points += 3; } else if (h < a) { A.won++; H.lost++; A.points += 3; }
      else { H.drawn++; A.drawn++; H.points++; A.points++; }
    }
    return Object.values(rows).sort((x, y) => y.points - x.points || (y.for - y.against) - (x.for - x.against) ||
      y.for - x.for || x.club.localeCompare(y.club));
  }

  // ---- actions ----

  checkSelection(ids) {
    if (!Array.isArray(ids) || ids.length !== 11 || new Set(ids).size !== 11)
      throw new EngineError('invalid_selection', 'Pick exactly 11 different players.');
    for (const id of ids) {
      const p = this.s.players[id];
      if (!p || p.club !== CLUB) throw new EngineError('invalid_selection', `${p ? p.name : id} is not in your squad.`);
      if (p.injuredDays > 0) throw new EngineError('invalid_selection', `${p.name} is injured.`);
    }
    if (!ids.some((id) => this.s.players[id].position === 'GK'))
      throw new EngineError('invalid_selection', 'The eleven needs a goalkeeper.');
  }

  async api_selectTeam({ ids }) {
    this.checkSelection(ids);
    this.s.selection = [...ids];
    this.step++;
    return { selection: [...ids] };
  }

  async api_bid({ playerId, fee }) {
    const p = this.s.players[playerId];
    if (!p || !p.target) throw new EngineError('not_found', `${playerId} is not on the transfer list.`);
    if (!Number.isInteger(fee) || fee <= 0) throw new EngineError('bad_params', 'The fee must be a whole number above 0.');
    if (this.s.bids.some((b) => b.playerId === playerId && b.status === 'waiting'))
      throw new EngineError('bid_pending', `Your bid for ${p.name} still waits for an answer.`);
    if (fee > this.s.money) throw new EngineError('not_enough_money', `You have ${money(this.s.money)}.`);
    const bid = { id: `b${this.s.nextId++}`, playerId, fee, madeDay: this.s.day, status: 'waiting',
      answerDay: this.s.day + (fee >= 0.8 * p.asking ? 2 : 1) };
    this.s.bids.push(bid);
    this.step++;
    return { bidId: bid.id, answerDate: this.date(bid.answerDay) };
  }

  async api_reply({ newsId, choice }) {
    const item = this.s.inbox.find((i) => i.id === newsId);
    if (!item) throw new EngineError('not_found', `No news item ${newsId}.`);
    if (!item.choices) throw new EngineError('no_choice', 'This item asks nothing.');
    if (item.answered) throw new EngineError('already_answered', `You answered "${item.answered}".`);
    if (!item.choices.includes(choice)) throw new EngineError('bad_choice', `Choose one of: ${item.choices.join(', ')}.`);
    if (item.kind === 'bid-accepted' || item.kind === 'bid-counter') {
      const bid = this.s.bids.find((b) => b.id === item.bidId), p = this.s.players[bid.playerId];
      if (choice === item.choices[0]) {
        const fee = item.kind === 'bid-counter' ? p.asking : bid.fee;
        if (fee > this.s.money) throw new EngineError('not_enough_money', `You have ${money(this.s.money)}.`);
        this.s.money -= fee;
        p.club = CLUB;
        p.target = false;
        bid.status = 'signed';
        this.news('signed', `${p.name} joins for ${money(fee)}`, `${p.name} signs until 2004 on ${money(p.wage)} a week.`);
      } else {
        bid.status = 'withdrawn';
      }
    } else if (item.kind === 'ai-bid') {
      const p = this.s.players[item.playerId];
      if (choice === 'Accept') {
        if (p.club !== CLUB) throw new EngineError('gone', `${p.name} is no longer your player.`);
        this.s.money += item.fee;
        p.club = item.buyer;
        this.s.selection = this.s.selection.filter((id) => id !== p.id);
        this.news('sold', `${p.name} leaves for ${this.clubName(item.buyer)}`,
          `${money(item.fee)} is paid. Your eleven now has ${this.s.selection.length} players: pick again.`);
      } else {
        p.morale = Math.max(1, p.morale - 2);
        this.news('unhappy', `${p.name} is unhappy`, `${p.name} wanted the move. His morale drops.`);
      }
    }
    item.answered = choice;
    this.step++;
    return { answered: choice };
  }

  async api_continue({ days = 1, toMatch = false }) {
    if (!Number.isInteger(days) || days < 1 || days > 60) throw new EngineError('bad_params', 'days: 1 to 60.');
    const pending = this.matchToday();
    if (pending) throw new EngineError('match_pending', `Today's match against ${this.opponent(pending)} comes first.`);
    this.busy = true;
    try {
      let n = 0, stopped = 'days';
      this.decisionArrived = false;
      for (;;) {
        await sleep(this.dayMs);
        this.nextDay();
        n++;
        this.emit('progress', { daysDone: n });
        if (this.matchToday()) { stopped = 'match'; break; }
        if (this.decisionArrived) { stopped = 'decision'; break; }
        if (n >= (toMatch ? 60 : days)) break;
      }
      this.step++;
      const today = this.matchToday();
      return { stopped, daysPlayed: n, matchToday: today ? this.fixtureView(today) : null };
    } finally {
      this.busy = false;
    }
  }

  opponent(f) { return this.clubName(f.home === CLUB ? f.away : f.home); }

  nextDay() {
    const s = this.s;
    for (const f of s.fixtures) if (f.day === s.day && !f.played) this.playOthers(f);
    s.day++;
    for (const p of this.ours()) {
      p.condition = Math.min(100, p.condition + 10);
      if (p.injuredDays > 0 && --p.injuredDays === 0)
        this.news('fit', `${p.name} is fit again`, `${p.name} has recovered and can be picked.`);
    }
    for (const b of s.bids) if (b.status === 'waiting' && b.answerDay === s.day) this.answerBid(b);
    const shaw = s.players.p10;
    if (s.day === 5 && !s.aiBidDone && shaw.club === CLUB) {
      s.aiBidDone = true;
      this.news('ai-bid', `Kingsmead United bid for ${shaw.name}`,
        `They offer ${money(800000)}. ${shaw.name} would like to talk to them.`, ['Accept', 'Refuse'],
        { playerId: shaw.id, fee: 800000, buyer: 'ku' });
    }
  }

  answerBid(b) {
    const p = this.s.players[b.playerId], seller = this.clubName(p.club);
    if (b.fee >= p.asking) {
      b.status = 'accepted';
      this.news('bid-accepted', `${seller} accept your bid for ${p.name}`,
        `They take ${money(b.fee)}. ${p.name} agrees to ${money(p.wage)} a week until 2004.`, ['Sign him', 'Withdraw'],
        { bidId: b.id });
    } else if (b.fee >= 0.8 * p.asking) {
      b.status = 'counter';
      this.news('bid-counter', `${seller} want more for ${p.name}`,
        `They turn down ${money(b.fee)} and ask for ${money(p.asking)}.`, [`Pay ${money(p.asking)}`, 'Walk away'],
        { bidId: b.id });
    } else {
      b.status = 'refused';
      this.news('bid-refused', `${seller} reject your bid for ${p.name}`, `${money(b.fee)} is far below what they want.`);
    }
  }

  // A match between two other clubs: a few seeded rolls, no commentary.
  playOthers(f) {
    if (f.home === CLUB || f.away === CLUB) return;
    const goals = (str) => { let g = 0; for (let i = 0; i < 10; i++) if (this.rand() < 0.12 * str / 12) g++; return g; };
    f.score = [goals(this.s.clubs[f.home].strength + 0.5), goals(this.s.clubs[f.away].strength)];
    f.played = true;
  }

  strength(ids) {
    const each = ids.map((id) => {
      const p = this.s.players[id], vals = Object.values(p.attributes);
      return (vals.reduce((a, b) => a + b, 0) / vals.length) * (0.6 + 0.4 * p.condition / 100);
    });
    return each.reduce((a, b) => a + b, 0) / each.length;
  }

  async api_playMatch({ id }) {
    const f = this.s.fixtures.find((x) => x.id === id);
    if (!f) throw new EngineError('not_found', `No match ${id}.`);
    if (f.home !== CLUB && f.away !== CLUB) throw new EngineError('not_your_match', 'Your club does not play in it.');
    if (f.played) throw new EngineError('already_played', 'This match is over.');
    if (f.day !== this.s.day) throw new EngineError('not_today', `It is on ${this.date(f.day)}.`);
    try { this.checkSelection(this.s.selection); }
    catch (err) { throw new EngineError('invalid_selection', `${err.message} Pick your eleven again.`); }
    this.busy = true;
    try {
      const plan = this.simulate(f);
      for (const line of plan.lines) {
        await sleep(this.tickMs);
        this.emit('commentary', { matchId: id, ...line });
      }
      this.commit(f, plan);
      this.step++;
      this.emit('result', { matchId: id, score: f.score });
      return this.fixtureView(f);
    } finally {
      this.busy = false;
    }
  }

  // The whole match, decided before a line is shown; commit() writes it to the game once the last line is out.
  simulate(f) {
    const home = f.home === CLUB, ids = [...this.s.selection], opp = this.opponent(f);
    const us = this.strength(ids) + (home ? 0.5 : 0), them = this.s.clubs[home ? f.away : f.home].strength + (home ? 0 : 0.5);
    const attackers = ids.filter((id) => ['M', 'F'].includes(this.s.players[id].position));
    let pool = attackers.flatMap((id) => (this.s.players[id].position === 'F' ? [id, id] : [id]));
    let ours = 0, theirs = 0, injured = null;
    const scorers = [], lines = [];
    const score = () => (home ? `${ours}–${theirs}` : `${theirs}–${ours}`);
    lines.push({ minute: 0, text: `Kick-off: ${this.clubName(f.home)} against ${this.clubName(f.away)}.`, score: score() });
    for (let minute = 5; minute <= 90; minute += 5) {
      if (minute === 60 && !this.s.injuryDone) {
        injured = ids.find((id) => this.s.players[id].position === 'M') ?? null;
        if (injured) {
          lines.push({ minute, text: `${this.s.players[injured].name} goes down injured and is replaced.`, score: score() });
          pool = pool.filter((id) => id !== injured);   // off the pitch: he neither shoots nor scores
        }
      }
      const forUs = this.rand() < us / (us + them), conv = 0.2 * (forUs ? us : them) / 12, roll = this.rand();
      if (roll < conv) {
        if (forUs) {
          ours++;
          const who = pool.at(Math.floor(this.rand() * pool.length));
          scorers.push(who);
          lines.push({ minute, text: `GOAL! ${this.s.players[who].name} scores.`, score: score(), goal: 'us' });
        } else {
          theirs++;
          lines.push({ minute, text: `Goal for ${opp}.`, score: score(), goal: 'them' });
        }
      } else if (roll < conv + 0.25) {
        lines.push({ minute, text: forUs ? `${this.s.players[pool.at(Math.floor(this.rand() * pool.length))].name} shoots, and the keeper saves.`
          : `${opp} go close.`, score: score() });
      }
      if (minute === 45) lines.push({ minute, text: `Half-time, ${score()}.`, score: score() });
    }
    lines.push({ minute: 90, text: `Full time: ${this.clubName(f.home)} ${score()} ${this.clubName(f.away)}.`, score: score() });
    return { lines, scorers, injured, ours, theirs, home, ids };
  }

  commit(f, plan) {
    f.score = plan.home ? [plan.ours, plan.theirs] : [plan.theirs, plan.ours];
    f.played = true;
    f.commentary = plan.lines;
    f.scorers = plan.scorers.map((id) => this.s.players[id].name);
    const delta = plan.ours > plan.theirs ? 1 : plan.ours < plan.theirs ? -1 : 0;
    for (const id of plan.ids) {
      const p = this.s.players[id];
      p.condition = Math.max(35, p.condition - 30);
      p.morale = Math.min(10, Math.max(1, p.morale + delta));
      p.apps++;
    }
    for (const id of plan.scorers) this.s.players[id].goals++;
    if (plan.injured) {
      this.s.injuryDone = true;
      const p = this.s.players[plan.injured];
      p.injuredDays = 20;
      this.news('injury', `${p.name} is out for 20 days`, `${p.name} was hurt in the ${f.competition.toLowerCase()} and stays in your eleven until you change it.`);
    }
    const [h, a] = f.score;
    this.news('result', `${this.clubName(f.home)} ${h}–${a} ${this.clubName(f.away)}`,
      f.scorers.length ? `Your scorers: ${f.scorers.join(', ')}.` : 'Your side did not score.');
  }

  async api_save() { return { blob: JSON.stringify({ apiVersion: API_VERSION, state: this.s }) }; }

  async api_load({ blob }) {
    let saved;
    try { saved = JSON.parse(blob); } catch { throw new EngineError('bad_save', 'This is not a save.'); }
    if (!saved || saved.apiVersion !== API_VERSION || !saved.state) throw new EngineError('bad_save', 'This save is from another version.');
    this.s = saved.state;
    this.step++;              // the step goes on growing, so the app treats the loaded game as new
    return { loaded: true };
  }

  async api_newGame() {
    this.s = newWorld();
    this.step++;
    return { started: true };
  }
}
