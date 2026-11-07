// The server's data: SQLite for the games and their steps, a folder for the save files (a stand-in for object
// storage such as S3 or R2). The engine owns the game world; these tables hold what the pages read (the date and
// the club after each step) and where the latest save is. Nothing here changes a game: changes go through the engine.
import { DatabaseSync } from 'node:sqlite';
import { copyFileSync, mkdirSync, readdirSync, rmSync, statSync } from 'node:fs';
import { join } from 'node:path';

const KEEP_SAVES = 3;              // saves kept per game: the latest and two before it

export class Store {
  constructor(dataDir) {
    this.dataDir = dataDir;
    this.savesDir = join(dataDir, 'saves');
    mkdirSync(this.savesDir, { recursive: true });
    this.db = new DatabaseSync(join(dataDir, 'games.db'));
    this.db.exec(`
      PRAGMA journal_mode = WAL;
      CREATE TABLE IF NOT EXISTS games (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL,
        status TEXT NOT NULL,            -- starting, ready, busy, failed
        year INTEGER, month INTEGER, day INTEGER,
        club TEXT,
        save_file TEXT,                  -- under saves/, the latest save
        save_bytes INTEGER,
        save_parts INTEGER,
        error TEXT,
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL
      );
      CREATE TABLE IF NOT EXISTS steps (
        id INTEGER PRIMARY KEY,
        game_id INTEGER NOT NULL REFERENCES games(id),
        at TEXT NOT NULL,
        action TEXT NOT NULL,            -- new, load, advance, save
        days INTEGER,
        ms INTEGER,
        result TEXT
      );
      CREATE INDEX IF NOT EXISTS steps_game ON steps(game_id, id);
    `);
    // A server that stopped in the middle of a step: those games wait for a load from their save.
    this.db.prepare("UPDATE games SET status = 'ready' WHERE status = 'busy' AND save_file IS NOT NULL").run();
    this.db.prepare("UPDATE games SET status = 'failed', error = 'the server stopped before the first save' " +
      "WHERE status IN ('starting', 'busy') AND save_file IS NULL").run();
  }

  createGame(name) {
    const now = new Date().toISOString();
    const r = this.db.prepare('INSERT INTO games (name, status, created_at, updated_at) VALUES (?, ?, ?, ?)')
      .run(name, 'starting', now, now);
    return this.game(Number(r.lastInsertRowid));
  }

  game(id) { return this.db.prepare('SELECT * FROM games WHERE id = ?').get(id) ?? null; }

  games() { return this.db.prepare('SELECT * FROM games ORDER BY id DESC').all(); }

  // fields: a subset of the columns of games.
  update(id, fields) {
    const keys = Object.keys(fields);
    const sets = keys.map((k) => `${k} = ?`).concat('updated_at = ?').join(', ');
    this.db.prepare(`UPDATE games SET ${sets} WHERE id = ?`)
      .run(...keys.map((k) => fields[k]), new Date().toISOString(), id);
    return this.game(id);
  }

  addStep(gameId, action, { days = null, ms = null, result = null } = {}) {
    this.db.prepare('INSERT INTO steps (game_id, at, action, days, ms, result) VALUES (?, ?, ?, ?, ?, ?)')
      .run(gameId, new Date().toISOString(), action, days, ms, result);
  }

  steps(gameId, limit = 20) {
    return this.db.prepare('SELECT * FROM steps WHERE game_id = ? ORDER BY id DESC LIMIT ?').all(gameId, limit);
  }

  // Copies a save that the engine wrote into the storage and returns its key (the path under saves/).
  putSave(gameId, file) {
    const dir = join(this.savesDir, String(gameId));
    mkdirSync(dir, { recursive: true });
    const name = `${Date.now()}.sav`;
    copyFileSync(file, join(dir, name));
    const all = readdirSync(dir).filter((n) => n.endsWith('.sav')).sort();
    for (const old of all.slice(0, Math.max(0, all.length - KEEP_SAVES))) rmSync(join(dir, old));
    return `${gameId}/${name}`;
  }

  savePath(key) { return join(this.savesDir, key); }

  saveBytes(key) { return statSync(this.savePath(key)).size; }

  close() { this.db.close(); }
}
