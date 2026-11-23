// Layer 1 against call floods: a limit for each internet address at the start of the Worker,
// before any Durable Object call, and a short-lived copy of the list in the Worker's memory.
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import worker from "../src/index";
import { ANSWER_CACHE_MS, CALL_LIMIT, WRITE_LIMIT, WRITE_LIMIT_48, setAnswerCacheMs } from "../src/flood";
import wranglerConfig from "../wrangler.jsonc?raw";
import { BASE, call, freshIp, gameBody, json, register, resetObjects } from "./helpers";

beforeEach(async () => {
  await resetObjects();
  setAnswerCacheMs(ANSWER_CACHE_MS);
});
afterEach(() => {
  vi.restoreAllMocks();
});

/** The game's "not available" answer: 503, error unavailable, from this API (lobby_http.c: LH_UNAVAILABLE). */
async function expectRefused(res: Response) {
  expect(res.status).toBe(503);
  expect(res.headers.get("X-Bosman-Api")).toBe("1");
  expect(res.headers.get("Retry-After")).toBe("60");
  const data = await json(res);
  expect(data.error).toBe("unavailable");
}

describe("limit for each internet address (Workers Rate Limiting binding)", () => {
  it(`refuses the call after ${CALL_LIMIT} calls a minute from one address, and lets other addresses through`, async () => {
    const ip = freshIp();
    for (let i = 0; i < CALL_LIMIT; i++) expect((await call("/v1/games", { ip })).status, `call ${i + 1}`).toBe(200);
    await expectRefused(await call("/v1/games", { ip }));
    await expectRefused(await call("/v1/health", { ip }));
    expect((await call("/v1/games", { ip: freshIp() })).status).toBe(200);
  });

  it("counts an IPv6 address by its first 64 bits", async () => {
    for (let i = 0; i < CALL_LIMIT; i++) await call("/v1/health", { ip: `2001:db8:77:0:0:0:0:${(i + 1).toString(16)}` });
    await expectRefused(await call("/v1/health", { ip: "2001:db8:77::ffff" }));
    expect((await call("/v1/health", { ip: "2001:db8:78::1" })).status).toBe(200);
  });

  it(`refuses a register, join or close after ${WRITE_LIMIT} a minute from one address`, async () => {
    const host = await register();
    const ip = freshIp();
    const join = () => call(`/v1/games/${host.code}/join`, { method: "POST", ip, body: { version: "0.7.0", name: "Ann" } });
    for (let i = 0; i < WRITE_LIMIT; i++) expect((await join()).status, `join ${i + 1}`).toBe(200);
    await expectRefused(await join());
    await expectRefused(await call("/v1/games", { method: "POST", ip, body: gameBody() }));
    await expectRefused(await call(`/v1/games/${host.code}`, { method: "DELETE", ip, token: host.hostToken }));
    expect((await call("/v1/games", { ip })).status).toBe(200);
  });

  it(`counts the writes of one IPv6 /48 site together: ${WRITE_LIMIT_48} a minute`, async () => {
    const host = await register();
    const join = (ip: string) =>
      call(`/v1/games/${host.code}/join`, { method: "POST", ip, body: { version: "0.7.0", name: "Ann" } });
    for (let i = 0; i < WRITE_LIMIT_48; i++) expect((await join(`2001:db8:99:${i.toString(16)}::1`)).status, `join ${i + 1}`).toBe(200);
    await expectRefused(await join("2001:db8:99:ffff::1"));
    expect((await call("/v1/games", { ip: "2001:db8:99:ffff::1" })).status).toBe(200);
    expect((await join("2001:db8:9a:1::1")).status).toBe(200);
  }, 30_000);

  it("refuses a relay connection too, before the relay object is asked", async () => {
    const ip = freshIp();
    for (let i = 0; i < CALL_LIMIT; i++) await call("/v1/health", { ip });
    const res = await call("/v1/games/ABCDEFGH/relay?role=host&token=x", { ip });
    await expectRefused(res);
  });

  it("refuses the invite page with a short page, not a game answer", async () => {
    const ip = freshIp();
    for (let i = 0; i < CALL_LIMIT; i++) await call("/v1/health", { ip });
    const res = await call("/join/ABCDEFGH", { ip });
    expect(res.status).toBe(503);
    expect(res.headers.get("Retry-After")).toBe("60");
  });

  it("lets a busy home through: 2 hosts and joiners behind one address, a list every 5 s from 4 games", async () => {
    // In one minute from one address: 2 registers, 8 heartbeats, 6 joins, and 4 games that each ask for the list
    // every 5 s (the most: the game keeps its list 5 s, LIST_CACHE_MS in net_lobby.c): 48 lists. 64 calls, 8 writes.
    const ip = freshIp();
    const hosts = [];
    for (let i = 0; i < 2; i++) hosts.push(await register({}, ip));
    for (const h of hosts) for (let i = 0; i < 4; i++) {
      expect((await call(`/v1/games/${h.code}/heartbeat`, { method: "POST", ip, token: h.hostToken })).status).toBe(200);
    }
    for (let i = 0; i < 6; i++) {
      const res = await call(`/v1/games/${hosts[i % 2].code}/join`, { method: "POST", ip, body: { version: "0.7.0", name: "Ann" } });
      expect(res.status).toBe(200);
    }
    for (let i = 0; i < 48; i++) expect((await call("/v1/games", { ip })).status).toBe(200);
  });
});

describe("Wrangler config", () => {
  it("has the limits of the code, for 60 s", () => {
    const config = JSON.parse(wranglerConfig.replace(/^\s*\/\/.*$/gm, ""));
    const byName = Object.fromEntries(config.ratelimits.map((r: any) => [r.name, r.simple]));
    expect(byName.RATE_CALLS).toEqual({ limit: CALL_LIMIT, period: 60 });
    expect(byName.RATE_WRITES).toEqual({ limit: WRITE_LIMIT, period: 60 });
    expect(byName.RATE_WRITES_48).toEqual({ limit: WRITE_LIMIT_48, period: 60 });
  });
});

describe("the refusal does no work", () => {
  /** An env whose limiter refuses and whose Durable Objects count each use. */
  function refusingEnv(keys: string[]) {
    let used = 0;
    const ns = {
      idFromName: () => {
        used++;
        return {};
      },
      get: () => {
        used++;
        return {};
      },
    };
    const limiter = {
      limit: async ({ key }: { key: string }) => {
        keys.push(key);
        return { success: false };
      },
    };
    return { env: { LOBBY: ns, RELAY: ns, RATE_CALLS: limiter, RATE_WRITES: limiter } as unknown as Env, used: () => used };
  }

  it.each([
    ["GET", "/v1/games"],
    ["POST", "/v1/games"],
    ["POST", "/v1/games/ABCDEFGH/join"],
    ["POST", "/v1/games/ABCDEFGH/heartbeat"],
    ["GET", "/v1/games/ABCDEFGH/relay"],
    ["GET", "/join/ABCDEFGH"],
  ])("%s %s reads no body and asks no Durable Object", async (method, path) => {
    const keys: string[] = [];
    const { env, used } = refusingEnv(keys);
    const body = new ReadableStream({
      pull() {
        throw new Error("the body was read");
      },
    });
    const init: RequestInit = { method, headers: { "CF-Connecting-IP": "203.0.113.200", Upgrade: "websocket" } };
    if (method === "POST") Object.assign(init, { body, duplex: "half" });
    const res = await worker.fetch(new Request(`${BASE}${path}`, init), env);
    expect(res.status).toBe(503);
    expect(used()).toBe(0);
    expect(keys.length).toBeGreaterThan(0);
  });

  it("gives the limiter no internet address, only a hash that changes each day", async () => {
    const keys: string[] = [];
    const { env } = refusingEnv(keys);
    const ip = "203.0.113.200";
    await worker.fetch(new Request(`${BASE}/v1/games`, { headers: { "CF-Connecting-IP": ip } }), env);
    await worker.fetch(new Request(`${BASE}/v1/games`, { headers: { "CF-Connecting-IP": "2001:db8:5:6::1" } }), env);
    for (const k of keys) {
      expect(k).toMatch(/^[0-9a-f]{32}$/);
      expect(k).not.toContain("203");
    }
    expect(keys[0]).not.toBe(keys[1]);
    const salted: string[] = [];
    const s = refusingEnv(salted);
    await worker.fetch(
      new Request(`${BASE}/v1/games`, { headers: { "CF-Connecting-IP": ip } }),
      Object.assign(s.env, { RATE_KEY_SALT: "secret" }),
    );
    expect(salted[0]).toMatch(/^[0-9a-f]{32}$/);
    expect(salted[0]).not.toBe(keys[0]);
    vi.useFakeTimers({ toFake: ["Date"] });
    vi.setSystemTime(Date.now() + 86_400_000);
    await worker.fetch(new Request(`${BASE}/v1/games`, { headers: { "CF-Connecting-IP": ip } }), env);
    vi.useRealTimers();
    expect(keys[2]).not.toBe(keys[0]);
  });

  it("lets calls through when the limiter is missing or fails, and does not crash", async () => {
    const res = await worker.fetch(new Request(`${BASE}/v1/health`, { headers: { "CF-Connecting-IP": "203.0.113.201" } }), {
      RATE_CALLS: { limit: async () => { throw new Error("limiter down"); } },
    } as unknown as Env);
    expect(res.status).toBe(200);
    const res2 = await worker.fetch(new Request(`${BASE}/v1/health`, { headers: { "CF-Connecting-IP": "203.0.113.201" } }), {} as Env);
    expect(res2.status).toBe(200);
  });
});

describe("the Worker's copy of the list", () => {
  it("answers repeated list calls from memory for 2 seconds", async () => {
    const t0 = Date.now();
    vi.spyOn(Date, "now").mockImplementation(() => t0);
    let lobbyCalls = 0;
    const env = {
      LOBBY: {
        idFromName: () => ({}),
        get: () => ({
          list: async () => {
            lobbyCalls++;
            return { ok: true, body: { games: [{ code: `GAME${lobbyCalls}` }], heartbeatSeconds: 30 } };
          },
        }),
      },
    } as unknown as Env;
    const list = async () => (await json(await worker.fetch(new Request(`${BASE}/v1/games`), env))).games[0].code;
    for (let i = 0; i < 20; i++) expect(await list()).toBe("GAME1");
    expect(lobbyCalls).toBe(1);
    vi.spyOn(Date, "now").mockImplementation(() => t0 + 1_999);
    expect(await list()).toBe("GAME1");
    vi.spyOn(Date, "now").mockImplementation(() => t0 + 2_000);
    expect(await list()).toBe("GAME2");
    expect(lobbyCalls).toBe(2);
  });

  it("shows a new game at once and a closed game as gone at once, through the same Worker", async () => {
    const ip = freshIp();
    expect((await json(await call("/v1/games", { ip }))).games).toEqual([]);
    const g = await register({}, ip);
    expect((await json(await call("/v1/games", { ip }))).games.map((x: any) => x.code)).toContain(g.code);
    expect((await call(`/v1/games/${g.code}`, { ip })).status).toBe(200);
    await call(`/v1/games/${g.code}/heartbeat`, { method: "POST", ip, token: g.hostToken, body: { day: "2001-12-01" } });
    expect((await json(await call(`/v1/games/${g.code}`, { ip }))).game.day).toBe("2001-12-01");
    await call(`/v1/games/${g.code}`, { method: "DELETE", ip, token: g.hostToken });
    expect((await call(`/v1/games/${g.code}`, { ip })).status).toBe(404);
    expect((await call(`/join/${g.code}`, { ip })).status).toBe(404);
    expect((await json(await call("/v1/games", { ip }))).games).toEqual([]);
  });

  it("does not keep a game-not-open answer", async () => {
    let n = 0;
    const env = {
      LOBBY: {
        idFromName: () => ({}),
        get: () => ({
          get: async () => (++n === 1 ? { ok: false, error: "unknown_game" } : { ok: true, body: { game: { code: "ABCDEFGH" } } }),
        }),
      },
    } as unknown as Env;
    expect((await worker.fetch(new Request(`${BASE}/v1/games/ABCDEFGH`), env)).status).toBe(404);
    expect((await worker.fetch(new Request(`${BASE}/v1/games/ABCDEFGH`), env)).status).toBe(200);
  });

  it("does not keep a failed list answer", async () => {
    let lobbyCalls = 0;
    const env = {
      LOBBY: {
        idFromName: () => ({}),
        get: () => ({
          list: async () => {
            lobbyCalls++;
            if (lobbyCalls === 1) throw new Error("internal error");
            return { ok: true, body: { games: [], heartbeatSeconds: 30 } };
          },
        }),
      },
    } as unknown as Env;
    expect((await worker.fetch(new Request(`${BASE}/v1/games`), env)).status).toBe(503);
    expect((await worker.fetch(new Request(`${BASE}/v1/games`), env)).status).toBe(200);
  });

  it("holds no internet address and no host token", async () => {
    const ip = "198.18.7.7";
    const g = await register({}, ip);
    const text = await (await call("/v1/games", { ip })).text();
    expect(text).not.toContain(ip);
    expect(text).not.toContain(g.hostToken);
  });
});
