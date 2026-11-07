// One engine worker: the recompiled game (or the test stand-in) in its own process, driven over the engine worker
// protocol (src/recomp_rt/win/worker_proto.h). The game gets the link as file descriptor 3 (RECOMP_WORKER_FD=3).
//
// Why a process per game: the game's state is one set of globals, its screen loop never returns, and a fatal stop
// ends the process (rt.c). A crash ends one game, never the server.
import { spawn } from 'node:child_process';
import { mkdirSync, openSync, closeSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

export class EngineError extends Error {}

export class Engine {
  // program: the game (cm0102_recomp) or the stand-in. gameDir: the clean files (the game's --game), or null.
  // work: an empty folder for this run, the game's user folder (saves, settings.txt, log).
  // steps: the RECOMP_SCRIPT steps that load a save or make a new game. env: extra environment.
  constructor({ program, gameDir = null, work, steps = [], env = {}, turnTimeoutMs = 20 * 60 * 1000 }) {
    this.work = work;
    this.turnTimeoutMs = turnTimeoutMs;
    this.logPath = join(work, 'engine.log');
    this.pending = [];          // replies to commands, in order
    this.turnWaiters = [];
    this.lastTurn = null;
    this.atTurn = false;
    this.exited = null;
    this.buf = '';

    mkdirSync(join(work, 'home'), { recursive: true });
    // Classic mode: the steps click at 800x600 positions. faster_days: the holiday screens end in their first pass.
    writeFileSync(join(work, 'settings.txt'), 'display = classic\nfaster_days = on\n');
    const log = openSync(this.logPath, 'a');
    const args = gameDir ? ['--game', gameDir] : [];
    this.child = spawn(program, args, {
      cwd: work,
      stdio: ['ignore', log, log, 'pipe'],
      env: {
        ...process.env,
        HOME: join(work, 'home'),
        SDL_VIDEODRIVER: 'dummy',
        SDL_AUDIODRIVER: 'dummy',
        RECOMP_NO_BOX: '1',
        RECOMP_USER_DIR: work,
        RECOMP_WORKER_FD: '3',
        RECOMP_SCRIPT: steps.join('; '),
        ...env,
      },
    });
    closeSync(log);
    this.link = this.child.stdio[3];
    this.link.setEncoding('utf8');
    this.link.on('data', (chunk) => this.#onData(chunk));
    this.link.on('error', () => {});                   // the exit handler reports the end
    this.child.on('exit', (code, signal) => this.#onExit(code, signal));
    this.child.on('error', (err) => this.#onExit(null, null, err));
  }

  get pid() { return this.child.pid; }

  #onData(chunk) {
    this.buf += chunk;
    let nl;
    while ((nl = this.buf.indexOf('\n')) >= 0) {
      const line = this.buf.slice(0, nl);
      this.buf = this.buf.slice(nl + 1);
      let msg;
      try { msg = JSON.parse(line); } catch { this.#fail(new EngineError(`not JSON from the engine: ${line.slice(0, 200)}`)); return; }
      if (msg.event === 'turn') {
        this.atTurn = true;
        this.lastTurn = { year: msg.year, month: msg.month, day: msg.day, club: msg.club };
        for (const w of this.turnWaiters.splice(0)) { clearTimeout(w.timer); w.resolve(this.lastTurn); }
      } else {
        const p = this.pending.shift();
        if (!p) { this.#fail(new EngineError(`an answer nobody asked for: ${line.slice(0, 200)}`)); return; }
        if (msg.ok) p.resolve(msg); else p.reject(new EngineError(msg.error || 'the engine refused'));
      }
    }
  }

  #logTail() {
    try { return readFileSync(this.logPath, 'utf8').split('\n').slice(-12).join('\n'); } catch { return ''; }
  }

  #fail(err) {
    for (const p of this.pending.splice(0)) p.reject(err);
    for (const w of this.turnWaiters.splice(0)) { clearTimeout(w.timer); w.reject(err); }
  }

  #onExit(code, signal, err) {
    if (this.exited) return;
    this.exited = { code, signal };
    this.atTurn = false;
    const why = err ? err.message : signal ? `signal ${signal}` : `code ${code}`;
    this.#fail(new EngineError(`the engine ended (${why}). Last lines of its log:\n${this.#logTail()}`));
  }

  // The next human turn where the game waits for the server: the date and the club.
  waitTurn() {
    if (this.atTurn) return Promise.resolve(this.lastTurn);
    if (this.exited) return Promise.reject(new EngineError('the engine has ended'));
    return new Promise((resolve, reject) => {
      const w = { resolve, reject };
      w.timer = setTimeout(() => {
        this.turnWaiters = this.turnWaiters.filter((x) => x !== w);
        reject(new EngineError(`no human turn after ${this.turnTimeoutMs} ms`));
      }, this.turnTimeoutMs);
      this.turnWaiters.push(w);
    });
  }

  #command(line) {
    if (this.exited) return Promise.reject(new EngineError('the engine has ended'));
    if (!this.atTurn) return Promise.reject(new EngineError('the engine is not at a human turn'));
    return new Promise((resolve, reject) => {
      this.pending.push({ resolve, reject });
      this.link.write(line + '\n');
    });
  }

  state() { return this.#command('state'); }

  // Saves under name (letters, digits, space, - _ .) and returns the host path of the file.
  async save(name) { return (await this.#command(`save ${name}`)).file; }

  // Plays days on holiday and returns the turn where the game waits again.
  async advance(days) {
    const reply = this.#command(`advance ${days}`);
    this.atTurn = false;           // the game goes on once it has answered
    await reply;
    return this.waitTurn();
  }

  async quit() {
    if (this.exited) return;
    const ended = new Promise((resolve) => this.child.once('exit', resolve));
    if (this.atTurn) { try { await this.#command('quit'); } catch { /* it ended anyway */ } }
    else this.child.kill('SIGTERM');
    const timer = setTimeout(() => this.child.kill('SIGKILL'), 10000);
    await ended;
    clearTimeout(timer);
  }

  // The work folder, once the process has ended.
  removeWork() { rmSync(this.work, { recursive: true, force: true }); }
}
