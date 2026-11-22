// Whether the last visit stopped without the page closing (web/README.md, "The page around the game"). iOS closes a
// tab that uses too much memory and then says "A problem repeatedly occurred on …" (an iPhone, 17 November 2026); the
// page itself sees nothing of it, and the next visit started as if nothing had happened. So while the game loads and
// runs, the page keeps a mark in localStorage with its last step ("downloading the game files", "Loading game data", "in
// the game"), and takes it off when it closes (pagehide) or stops on its own (an error box, another tab). A mark that
// the next visit finds is a sudden stop: the system closed the tab.

import { t } from './i18n.js';

const KEY = 'cm0102-visit';
const DAY = 86400e3;

// storage: localStorage, or a stand-in; it may be missing or throw (a private window), and then nothing is marked.
// The storage is the site's, shared by its tabs: each mark names its tab (id), a tab takes off only its own, and a
// visit reads the last one (takeLastStop) only when no other tab plays (loader.js asks the game's Web Lock first).
export function mountVisitMark({ storage = null, now = () => Date.now(), id = Math.random().toString(36).slice(2) } = {}) {
  const read = () => { try { return JSON.parse(storage?.getItem(KEY) ?? 'null'); } catch { return null; } };
  let mark = null, base = {};
  const write = () => { try { if (mark) storage?.setItem(KEY, JSON.stringify({ ...mark, id })); } catch { /* not kept */ } };
  const takeOff = () => { try { if (read()?.id === id) storage?.removeItem(KEY); } catch { /* nothing to take off */ } };
  return {
    // The last visit's mark when it stopped suddenly within a day: { step, at, device, engine }, else null. It is
    // taken off, so it is told once.
    takeLastStop() {
      const found = read();
      if (!found || found.id === id) return null;
      try { storage?.removeItem(KEY); } catch { /* told once anyway */ }
      if (typeof found.step !== 'string' || !(now() - Number(found.at) < DAY)) return null;
      const { id: _, ...last } = found;
      return last;
    },
    // The game starts loading: from now on a sudden stop is found by the next visit.
    begin(info = {}) { base = { ...info }; delete base.step; mark = { ...base, step: info.step ?? 'starting', at: now() }; write(); },
    // What it does now (only once begun); extra: what goes with this step only ({ cloudFile }: the cloud save coming).
    step(step, extra = {}) { if (!mark || mark.hidden) return; mark = { ...base, ...extra, step: String(step).slice(0, 120), at: now() }; write(); },
    // The page closes (pagehide) or stops on its own: no sudden stop. hide: a page that may come back (the browser's
    // back-forward cache) keeps its mark in memory, and resume() writes it again.
    end({ hide = false } = {}) { const kept = mark; mark = null; takeOff(); if (hide && kept) mark = { ...kept, hidden: true }; },
    resume() { if (mark?.hidden) { const { hidden, ...m } = mark; mark = { ...m, at: now() }; write(); } },
    get active() { return Boolean(mark) && !mark.hidden; },
  };
}

// The step of a mark in the page's language: the page's own steps; a stage of the game ("Loading game data") is the
// game's own text and stays as it is. The mark and the log keep the English.
function stepWords(step) {
  switch (step) {
    case 'starting': return t('starting');
    case 'downloading the game files': return t('downloading the game files');
    case 'reading the game files': return t('reading the game files');
    case 'starting the engine': return t('starting the engine');
    case 'in the game': return t('in the game');
    case 'sending saves to the cloud': return t('sending saves to the cloud');
    default: {
      const m = /^getting (.+) from the cloud$/.exec(step);
      return m ? t('getting {name} from the cloud', { name: m[1] }) : step;
    }
  }
}

// The words for the player: when, what step, and what most likely happened, in the page's language.
export function suddenStopText(last, { device = null, now = Date.now() } = {}) {
  const min = Math.max(1, Math.round((now - Number(last.at)) / 60000));
  const ago = min < 60 ? t('{n} min ago', { n: min }) : t('{n} h ago', { n: Math.round(min / 60) });
  const what = t('The game stopped suddenly on your last visit ({ago}, while {step}): the browser most likely closed the tab because it used too much memory.', { ago, step: stepWords(last.step) });
  return device === 'iPhone'
    ? `${what} ${t('An iPhone gives a tab less memory than the game can need; an iPad or a computer is the surer way to play. Closing other apps and tabs first can help.')}`
    : `${what} ${t('Closing other tabs and apps first can help.')}`;
}
