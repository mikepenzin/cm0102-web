// Play (web/README.md, "Play"): the Play button of a save in Saved games. The engine loads the save from its start
// screen as a player does (src/recomp_rt/win/play.c: Restore Saved Game, the name in the File entry, Ok). With a game
// in play the page asks first and restarts the game as Restart Game does; the name waits in sessionStorage over the
// reload (a few minutes at most), and the page asks the engine once the game has started again.
import { t } from './i18n.js';

export const PLAY_KEY = 'cm0102-play';
const WAIT_MS = 5 * 60000;          // a name older than this in sessionStorage is from a start that never came

// play.c's codes (PLAY_*): 0 Ok; the others say why not (for the log; didNotOpen has them for the player).
const WHY = [
  null,
  'its name has characters the game\'s File entry does not take (" * / < > | { } [ ] ~ : \\ ?). Download it, then upload it under another name',
  'its name is too long for the game',
  'a game is in play: press Play again to close it first',
  'the game\'s start screen did not come',
  'Restore Saved Game did not open',
  'the game\'s File entry did not take its name (too long for the box?). Download it, then upload it under a shorter name',
  null,                              // 7: the player closed Restore Saved Game: no message
  'another save is opening',
];

// What the player reads when a save did not open (code: play.c's, as WHY), in the page's language.
function didNotOpen(name, code) {
  switch (code) {
    case 1: return t('{name} did not open: its name has characters the game\'s File entry does not take (" * / < > | { } [ ] ~ : \\ ?). Download it, then upload it under another name.', { name });
    case 2: return t('{name} did not open: its name is too long for the game.', { name });
    case 3: return t('{name} did not open: a game is in play: press Play again to close it first.', { name });
    case 4: return t('{name} did not open: the game\'s start screen did not come.', { name });
    case 5: return t('{name} did not open: Restore Saved Game did not open.', { name });
    case 6: return t('{name} did not open: the game\'s File entry did not take its name (too long for the box?). Download it, then upload it under a shorter name.', { name });
    case 8: return t('{name} did not open: another save is opening.', { name });
    default: return t('{name} did not open: the game answered {code}.', { name, code });
  }
}

// module(): the engine's Module; restart(): Restart Game's path (loader.js, rtRestart); isRunning(): the game runs
// (not stopped, not restarting). Returns { can, play, resume }.
export function mountPlay({ module, log, toast, restart, isRunning = () => true, ask = (q) => confirm(q) }) {
  let asked = null;
  const M = () => module();
  const can = () => typeof M()?._rt_web_play === 'function' && typeof M()?.ccall === 'function';
  const say = (name, code) => {
    const why = WHY[code];
    log(`page: play ${name ?? 'The save'}: ${why ?? (code === 7 ? 'Restore Saved Game was closed' : `the game answered ${code}`)}`);
    if (why || WHY[code] === undefined) {
      const text = didNotOpen(name ?? t('The save'), code);
      toast(code === 3 || code === 8 ? text : `${text} ${t('Restore Saved Game on the game\'s start screen opens it too.')}`, { ms: 9000, kind: 'bad' });
    }
  };
  // Asks the engine; true when the play is on its way.
  const send = (name) => {
    const code = M().ccall('rt_web_play', 'number', ['string'], [name]);
    if (typeof code === 'number' && code >= 0) { say(name, code); return false; }
    asked = name;
    log(`page: play ${name}: the game opens it from its start screen`);
    toast(t('Opening {name}…', { name }));
    return true;
  };
  M().rtPlay = (code) => {
    if (!code) { log(`page: play ${asked ?? 'The save'}: Ok pressed, the game loads it`); return; }
    say(asked, code);
  };

  // Returns true when the save is on its way (Saved games may close).
  function play(name) {
    if (!can()) {
      toast(t('This version of the game opens a save only from Restore Saved Game on its start screen.'), { ms: 6000 });
      return false;
    }
    if (!isRunning()) { toast(t('The game is not running now: it opens a save once it has started.'), { ms: 6000 }); return false; }
    if (M()._rt_web_in_game?.()) {
      if (!ask(t('Open {name}? The game in play closes: what you did since your last save is lost.', { name }))) return false;
      try { sessionStorage.setItem(PLAY_KEY, JSON.stringify({ name, at: Date.now() })); } catch (e) {
        toast(t('{name} cannot open from here now ({error}). Use Restore Saved Game on the game\'s start screen.', { name, error: e.message }), { ms: 6000 });
        return false;
      }
      log(`page: play ${name}: the game in play closes first`);
      restart();
      return true;
    }
    return send(name);
  }

  // After a restart for a play: the save the player chose, once.
  function resume() {
    let w = null;
    try { w = JSON.parse(sessionStorage.getItem(PLAY_KEY) ?? 'null'); sessionStorage.removeItem(PLAY_KEY); } catch { /* none */ }
    if (!w?.name || !(Date.now() - w.at < WAIT_MS)) return;
    if (!can() || !isRunning()) { log(`page: play ${w.name}: the game did not start, so it was not opened`); return; }
    try { send(w.name); } catch (e) { log(`page: play ${w.name}: ${e.message}`); }
  }

  return { can, play, resume };
}
