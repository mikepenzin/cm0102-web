// The web server: a JSON API over the games and one page. Run: node server/src/server.js (see server/README.md).
//
//   GET  /api/games                 the games, newest first
//   POST /api/games {name}          a new game (202: it is being made; poll the game)
//   GET  /api/games/:id             one game and its last steps
//   POST /api/games/:id/advance {days}   play 1 to 400 days (202; 409 while the game has a step running)
//   GET  /healthz                   {ok, warm}: the count of running engines
//
// No logins yet: it listens on 127.0.0.1 unless HOST says otherwise.
import { createServer } from 'node:http';
import { readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { GameService } from './games.js';
import { Store } from './store.js';

const HERE = dirname(fileURLToPath(import.meta.url));
const PAGE = join(HERE, '..', 'public', 'index.html');
const MAX_BODY = 10 * 1024;

function send(res, status, body) {
  const text = JSON.stringify(body);
  res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' });
  res.end(text);
}

function readBody(req) {
  return new Promise((resolveBody, reject) => {
    let size = 0;
    const chunks = [];
    req.on('data', (c) => {
      size += c.length;
      if (size > MAX_BODY) { reject(Object.assign(new Error('body too large'), { status: 413 })); req.destroy(); }
      else chunks.push(c);
    });
    req.on('end', () => {
      if (!chunks.length) return resolveBody({});
      try { resolveBody(JSON.parse(Buffer.concat(chunks).toString('utf8'))); }
      catch { reject(Object.assign(new Error('the body is not JSON'), { status: 400 })); }
    });
    req.on('error', reject);
  });
}

function view(game, games) {
  return game && { ...game, busy: games.busy(game.id) };
}

export function createApp({ store, games }) {
  return createServer(async (req, res) => {
    try {
      const url = new URL(req.url, 'http://localhost');
      const parts = url.pathname.split('/').filter(Boolean);
      if (req.method === 'GET' && url.pathname === '/') {
        res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
        return res.end(readFileSync(PAGE));
      }
      if (req.method === 'GET' && url.pathname === '/healthz') return send(res, 200, { ok: true, warm: games.warmCount() });
      if (parts[0] !== 'api' || parts[1] !== 'games') return send(res, 404, { error: 'not found' });

      if (parts.length === 2 && req.method === 'GET')
        return send(res, 200, { games: store.games().map((g) => view(g, games)) });
      if (parts.length === 2 && req.method === 'POST') {
        const body = await readBody(req);
        const name = String(body.name ?? '').trim().slice(0, 60) || 'New game';
        const { game, done } = games.create(name);
        done.catch(() => {});            // the game row shows the failure
        return send(res, 202, { game: view(game, games) });
      }

      const id = Number(parts[2]);
      const game = Number.isInteger(id) ? store.game(id) : null;
      if (!game) return send(res, 404, { error: 'no such game' });
      if (parts.length === 3 && req.method === 'GET')
        return send(res, 200, { game: view(game, games), steps: store.steps(id) });
      if (parts.length === 4 && parts[3] === 'advance' && req.method === 'POST') {
        const body = await readBody(req);
        const days = Number(body.days);
        if (!Number.isInteger(days) || days < 1 || days > 400) return send(res, 400, { error: 'days: 1 to 400' });
        if (games.busy(id)) return send(res, 409, { error: 'the game has a step running' });
        if (game.status === 'failed' && !game.save_file) return send(res, 409, { error: 'the game was never saved' });
        games.advance(id, days).catch(() => {});
        return send(res, 202, { game: view(store.game(id), games) });
      }
      return send(res, 404, { error: 'not found' });
    } catch (err) {
      send(res, err.status ?? 500, { error: err.message });
    }
  });
}

function readSteps(name) {
  return readFileSync(join(HERE, '..', 'steps', name), 'utf8').split('\n').map((s) => s.trim())
    .filter((s) => s && !s.startsWith('#'));
}

// The configuration from the environment.
export function configFromEnv(env = process.env) {
  if (!env.CM_ENGINE) throw new Error('CM_ENGINE must name the game (build/cm0102_recomp) or the test stand-in');
  return {
    dataDir: resolve(env.CM_DATA ?? join(HERE, '..', 'data')),
    port: Number(env.PORT ?? 8080),
    host: env.HOST ?? '127.0.0.1',
    idleMs: Number(env.CM_IDLE_MS ?? 5 * 60 * 1000),
    engine: {
      program: resolve(env.CM_ENGINE),
      gameDir: env.CM_GAME_DIR ? resolve(env.CM_GAME_DIR) : null,
      newGameSteps: readSteps('new-game.txt'),
      loadSteps: readSteps('load.txt'),
      turnTimeoutMs: Number(env.CM_TURN_TIMEOUT_MS ?? 20 * 60 * 1000),
      env: {},
    },
  };
}

export function start(config) {
  const store = new Store(config.dataDir);
  const games = new GameService({
    store, engine: config.engine, workDir: join(config.dataDir, 'work'), idleMs: config.idleMs,
    log: (line) => console.log(`${new Date().toISOString()} ${line}`),
  });
  const server = createApp({ store, games });
  const close = async () => {
    server.close();
    await games.close();
    store.close();
  };
  return { server, store, games, close };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const config = configFromEnv();
  if (!config.engine.gameDir) console.log('CM_GAME_DIR is not set: fine for the stand-in, the real game needs it');
  const app = start(config);
  app.server.listen(config.port, config.host, () => console.log(`listening on http://${config.host}:${config.port}`));
  for (const sig of ['SIGINT', 'SIGTERM']) process.once(sig, async () => { await app.close(); process.exit(0); });
}
