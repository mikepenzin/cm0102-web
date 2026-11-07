// The games: each step of a game runs in its engine, one step at a time, and ends with a save.
//
// An engine stays warm while its game is in use and stops after idleMs without a step; the next step loads the
// latest save into a new engine. So the server holds a process only for games that people play now, and a game
// survives a crash of its engine or of the server (it goes on from its last save).
import { copyFileSync, mkdirSync, rmSync } from 'node:fs';
import { basename, join } from 'node:path';
import { Engine } from './engine.js';
import { readSave } from './savfile.js';

export class GameService {
  // engine: {program, gameDir, newGameSteps, loadSteps, env, turnTimeoutMs}. store: a Store.
  constructor({ store, engine, workDir, idleMs = 5 * 60 * 1000, log = () => {} }) {
    this.store = store;
    this.engineConfig = engine;
    this.workDir = workDir;
    this.idleMs = idleMs;
    this.log = log;
    this.warm = new Map();     // game id -> {engine, timer}
    this.queues = new Map();   // game id -> the promise of the last step
  }

  // Runs fn after the steps that wait for this game. A failed step leaves the game failed and the queue free.
  #enqueue(id, fn) {
    const before = this.queues.get(id) ?? Promise.resolve();
    const run = before.catch(() => {}).then(fn);
    this.queues.set(id, run);
    run.finally(() => { if (this.queues.get(id) === run) this.queues.delete(id); }).catch(() => {});
    return run;
  }

  busy(id) { return this.queues.has(id); }

  // A new game: an engine makes it with the new game steps; the first save follows at once.
  create(name) {
    const game = this.store.createGame(name);
    const done = this.#enqueue(game.id, () => this.#step(game.id, 'new', null, async (engine, turn) => turn));
    return { game, done };
  }

  // Plays days days and saves.
  advance(id, days) {
    this.store.update(id, { status: 'busy' });
    return this.#enqueue(id, () => this.#step(id, 'advance', days, (engine) => engine.advance(days)));
  }

  async #step(id, action, days, run) {
    const started = Date.now();
    try {
      const { engine, turn: first, loaded } = await this.#engineFor(id);
      const turn = await run(engine, first);
      const file = await engine.save(`web ${id}`);
      const save = readSave(file);       // checks that the engine wrote a whole save container
      const key = this.store.putSave(id, file);
      rmSync(file, { force: true });     // the user folder keeps no save: the next load picks the right one
      const game = this.store.update(id, {
        status: 'ready', error: null, year: turn.year, month: turn.month, day: turn.day, club: turn.club,
        save_file: key, save_bytes: save.fileSize, save_parts: save.parts.length,
      });
      const date = `${turn.day}.${turn.month}.${turn.year}`;
      this.store.addStep(id, action, { days, ms: Date.now() - started, result: `${date}${loaded ? ' (loaded)' : ''}` });
      this.#touch(id);
      return game;
    } catch (err) {
      this.log(`game ${id}: ${action} failed: ${err.message}`);
      this.store.addStep(id, action, { days, ms: Date.now() - started, result: `failed: ${err.message.split('\n')[0]}` });
      this.store.update(id, { status: 'failed', error: err.message.slice(0, 2000) });
      await this.#stop(id);
      throw err;
    }
  }

  // The warm engine of the game, or a new one that makes the game or loads its latest save.
  async #engineFor(id) {
    const w = this.warm.get(id);
    if (w && !w.engine.exited) return { engine: w.engine, turn: await w.engine.waitTurn(), loaded: false };
    if (w) await this.#stop(id);
    const game = this.store.game(id);
    const work = join(this.workDir, `${id}-${Date.now()}`);
    mkdirSync(work, { recursive: true });
    const c = this.engineConfig;
    let steps = c.newGameSteps;
    if (game.save_file) {
      copyFileSync(this.store.savePath(game.save_file), join(work, 'web.sav'));  // the only save: the first row
      steps = c.loadSteps;
    }
    const engine = new Engine({
      program: c.program, gameDir: c.gameDir, work, steps, env: c.env, turnTimeoutMs: c.turnTimeoutMs,
    });
    this.log(`game ${id}: engine ${engine.pid} ${game.save_file ? 'loads ' + game.save_file : 'makes a new game'}`);
    this.warm.set(id, { engine, timer: null });
    const turn = await engine.waitTurn();
    if (game.save_file) this.store.addStep(id, 'load', { result: `${turn.day}.${turn.month}.${turn.year}` });
    return { engine, turn, loaded: Boolean(game.save_file) };
  }

  #touch(id) {
    const w = this.warm.get(id);
    if (!w) return;
    clearTimeout(w.timer);
    w.timer = setTimeout(() => {
      this.#enqueue(id, () => this.#stop(id)).catch(() => {});
    }, this.idleMs);
    w.timer.unref?.();
  }

  // Stops the engine and removes its work folder. Its log stays in logs/ under the work folder's name.
  async #stop(id) {
    const w = this.warm.get(id);
    if (!w) return;
    this.warm.delete(id);
    clearTimeout(w.timer);
    await w.engine.quit();
    const logs = join(this.workDir, '..', 'logs');
    mkdirSync(logs, { recursive: true });
    try { copyFileSync(w.engine.logPath, join(logs, `${basename(w.engine.work)}.log`)); } catch { /* no log yet */ }
    w.engine.removeWork();
    this.log(`game ${id}: engine stopped`);
  }

  warmCount() { return this.warm.size; }

  async close() {
    await Promise.allSettled([...this.queues.values()]);
    await Promise.all([...this.warm.keys()].map((id) => this.#stop(id)));
  }
}
