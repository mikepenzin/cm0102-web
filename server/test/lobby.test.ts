import { abortAllDurableObjects, evictDurableObject } from "cloudflare:test";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { call, freshIp, gameBody, json, lobbyStub, register, setLobbyClock, withLobby, resetObjects } from "./helpers";

beforeEach(async () => {
  await resetObjects();
});
afterEach(async () => {
  await abortAllDurableObjects();
});

describe("register", () => {
  it("returns a game code, a host token and the heartbeat times", async () => {
    const { res, data } = await register();
    expect(res.status).toBe(201);
    expect(data.code).toMatch(/^[23456789ABCDEFGHJKMNPQRSTUVWXYZ]{8}$/);
    expect(typeof data.hostToken).toBe("string");
    expect(data.heartbeatSeconds).toBe(30);
    expect(data.expireSeconds).toBe(90);
  });

  it("puts the game in the list with every public field and no private field", async () => {
    const { code } = await register({ password: "secret" });
    const data = await json(await call("/v1/games"));
    const game = data.games.find((g: any) => g.code === code);
    expect(game).toEqual({
      code,
      name: "Sunday League",
      hostName: "Max",
      version: "0.7.0",
      database: "Original 2001/02",
      season: 2001,
      day: "2001-08-11",
      humanClubs: ["Arsenal", "Scunthorpe United"],
      awayClubs: [],
      freeSeats: 14,
      managersOnline: 2,
      pingMs: 0,
      password: true,
    });
    expect(JSON.stringify(data)).not.toContain("secret");
    expect(JSON.stringify(data)).not.toContain("gameId");
  });

  it("shows one game by its code", async () => {
    const { code } = await register();
    const res = await call(`/v1/games/${code}`);
    expect(res.status).toBe(200);
    expect((await json(res)).game.code).toBe(code);
    const missing = await call(`/v1/games/ZZZZZZZZ`);
    expect(missing.status).toBe(404);
    expect((await json(missing)).error).toBe("unknown_game");
  });

  it.each([
    ["gameId", ""],
    ["gameId", "x".repeat(200)],
    ["name", ""],
    ["name", "   "],
    ["name", "x".repeat(41)],
    ["name", "tab\there"],
    ["name", "emoji \u{1F600}"],
    ["hostName", 7],
    ["version", ""],
    ["season", 1800],
    ["season", "2001"],
    ["day", "11/08/2001"],
    ["humanClubs", "Arsenal"],
    ["humanClubs", Array(17).fill("Club")],
    ["freeSeats", -1],
    ["freeSeats", 17],
    ["managersOnline", 1.5],
    ["pingMs", 10000],
    ["password", "x".repeat(65)],
  ])("rejects a bad %s", async (field, value) => {
    const res = await call("/v1/games", { method: "POST", body: gameBody({ [field]: value }), ip: freshIp() });
    expect(res.status).toBe(400);
    expect(await json(res)).toMatchObject({ error: "bad_request", field });
  });

  it("accepts names with Latin-1 letters", async () => {
    const { res } = await register({ name: "Liga Española", hostName: "Zoë" });
    expect(res.status).toBe(201);
  });
});

describe("word filter", () => {
  it.each([
    ["database", "Shit Database"],
    ["humanClubs", ["Arsenal", "Wanker Wanderers"]],
  ])("rejects a rude %s, because the list shows it", async (field, value) => {
    const res = await call("/v1/games", { method: "POST", body: gameBody({ [field]: value }), ip: freshIp() });
    expect(res.status).toBe(400);
    expect(await json(res)).toMatchObject({ error: "name_not_allowed", field });
  });

  it.each(["name", "hostName"])("rejects a rude %s", async (field) => {
    const res = await call("/v1/games", { method: "POST", body: gameBody({ [field]: "F.U.C.K league" }), ip: freshIp() });
    expect(res.status).toBe(400);
    expect(await json(res)).toEqual(expect.objectContaining({ error: "name_not_allowed", field }));
  });

  it("rejects a rude manager name at the join step", async () => {
    const { code } = await register();
    const res = await call(`/v1/games/${code}/join`, {
      method: "POST",
      ip: freshIp(),
      body: { version: "0.7.0", name: "sh1t" },
    });
    expect(res.status).toBe(400);
    expect(await json(res)).toMatchObject({ error: "name_not_allowed", field: "name" });
  });
});

describe("heartbeat and expiry", () => {
  it("drops a game with no heartbeat for 90 seconds", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const { code } = await register();
    await setLobbyClock(t0 + 89_999);
    expect((await json(await call("/v1/games"))).games.map((g: any) => g.code)).toContain(code);
    await setLobbyClock(t0 + 90_000);
    expect((await json(await call("/v1/games"))).games.map((g: any) => g.code)).not.toContain(code);
  });

  it("keeps a game that sends a heartbeat every 30 seconds, and updates its fields", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const { code, hostToken } = await register();
    for (let i = 1; i <= 6; i++) {
      await setLobbyClock(t0 + i * 30_000);
      const res = await call(`/v1/games/${code}/heartbeat`, {
        method: "POST",
        token: hostToken,
        body: { day: "2001-08-18", managersOnline: 3, freeSeats: 13, season: 2001, humanClubs: ["Arsenal"] },
      });
      expect(res.status).toBe(200);
      expect(await json(res)).toEqual({ ok: true });
    }
    const game = (await json(await call(`/v1/games/${code}`))).game;
    expect(game).toMatchObject({ day: "2001-08-18", managersOnline: 3, freeSeats: 13, humanClubs: ["Arsenal"] });
  });

  it("lists the Away clubs of a heartbeat, and none by default", async () => {
    const { code, hostToken } = await register();
    expect((await json(await call(`/v1/games/${code}`))).game.awayClubs).toEqual([]);
    const res = await call(`/v1/games/${code}/heartbeat`, {
      method: "POST",
      token: hostToken,
      body: { humanClubs: ["Arsenal", "Scunthorpe United"], awayClubs: ["Scunthorpe United"], managersOnline: 1 },
    });
    expect(res.status).toBe(200);
    const game = (await json(await call(`/v1/games/${code}`))).game;
    expect(game).toMatchObject({ awayClubs: ["Scunthorpe United"], managersOnline: 1 });
    const bad = await call(`/v1/games/${code}/heartbeat`, { method: "POST", token: hostToken, body: { awayClubs: "Arsenal" } });
    expect(bad.status).toBe(400);
  });

  it("lists the ping of a heartbeat, 0 by default, and refuses one out of range", async () => {
    const { code, hostToken } = await register();
    expect((await json(await call(`/v1/games/${code}`))).game.pingMs).toBe(0);
    const res = await call(`/v1/games/${code}/heartbeat`, { method: "POST", token: hostToken, body: { pingMs: 120 } });
    expect(res.status).toBe(200);
    expect((await json(await call(`/v1/games/${code}`))).game.pingMs).toBe(120);
    for (const v of [-1, 10000, 1.5, "80"]) {
      const bad = await call(`/v1/games/${code}/heartbeat`, { method: "POST", token: hostToken, body: { pingMs: v } });
      expect(bad.status).toBe(400);
    }
  });

  it("changes the password of a game at a heartbeat", async () => {
    const { code, hostToken } = await register();
    const beat = (body: object) => call(`/v1/games/${code}/heartbeat`, { method: "POST", token: hostToken, body });
    expect((await beat({ password: "Sunday 7" })).status).toBe(200);
    expect((await json(await call(`/v1/games/${code}`))).game.password).toBe(true);
    expect((await beat({ managersOnline: 2 })).status).toBe(200);
    expect((await json(await call(`/v1/games/${code}`))).game.password).toBe(true);
    expect((await beat({ password: "" })).status).toBe(200);
    expect((await json(await call(`/v1/games/${code}`))).game.password).toBe(false);
    expect((await beat({ password: "x".repeat(65) })).status).toBe(400);
  });

  it("accepts a heartbeat with no body", async () => {
    const { code, hostToken } = await register();
    const res = await call(`/v1/games/${code}/heartbeat`, { method: "POST", token: hostToken });
    expect(res.status).toBe(200);
  });

  it("answers unknown_game to a heartbeat after expiry, so the host registers again", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const { code, hostToken } = await register();
    await setLobbyClock(t0 + 91_000);
    const res = await call(`/v1/games/${code}/heartbeat`, { method: "POST", token: hostToken });
    expect(res.status).toBe(404);
    expect((await json(res)).error).toBe("unknown_game");
  });

  it("after expiry, a restore answers not_host and a new registration works", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const first = await register();
    await setLobbyClock(t0 + 91_000);
    expect((await call(`/v1/games/${first.code}/heartbeat`, { method: "POST", token: first.hostToken })).status).toBe(404);
    const restore = await register({ gameId: first.body.gameId, code: first.code, hostToken: first.hostToken }, first.ip);
    expect(restore.res.status).toBe(401);
    expect(restore.data.error).toBe("not_host");
    const fresh = await register({ gameId: first.body.gameId }, first.ip);
    expect(fresh.res.status).toBe(201);
    expect(fresh.code).not.toBe(first.code);
  });

  it("refuses a heartbeat and a close without the host token", async () => {
    const { code } = await register();
    for (const token of [undefined, "wrong"]) {
      const hb = await call(`/v1/games/${code}/heartbeat`, { method: "POST", token });
      expect(hb.status).toBe(401);
      expect((await json(hb)).error).toBe("not_host");
      const del = await call(`/v1/games/${code}`, { method: "DELETE", token });
      expect(del.status).toBe(401);
    }
  });

  it("rejects a bad field in a heartbeat", async () => {
    const { code, hostToken } = await register();
    const res = await call(`/v1/games/${code}/heartbeat`, { method: "POST", token: hostToken, body: { freeSeats: 99 } });
    expect(res.status).toBe(400);
    expect((await json(res)).field).toBe("freeSeats");
  });
});

describe("close", () => {
  it("removes the game from the list", async () => {
    const { code, hostToken } = await register();
    const res = await call(`/v1/games/${code}`, { method: "DELETE", token: hostToken });
    expect(res.status).toBe(200);
    expect((await json(await call("/v1/games"))).games.map((g: any) => g.code)).not.toContain(code);
    expect((await call(`/v1/games/${code}`)).status).toBe(404);
  });
});

describe("at most 2 open games for each internet address", () => {
  it("refuses a third game from one address and allows it after a close", async () => {
    const ip = freshIp();
    const a = await register({}, ip);
    const b = await register({}, ip);
    expect(a.res.status).toBe(201);
    expect(b.res.status).toBe(201);
    const c = await register({}, ip);
    expect(c.res.status).toBe(429);
    expect(c.data.error).toBe("too_many_games");
    expect((await register({}, freshIp())).res.status).toBe(201);
    await call(`/v1/games/${a.code}`, { method: "DELETE", token: a.hostToken });
    expect((await register({}, ip)).res.status).toBe(201);
  });

  it("counts IPv6 addresses by their /64 prefix", async () => {
    expect((await register({}, "2001:db8:1:2::1")).res.status).toBe(201);
    expect((await register({}, "2001:db8:1:2:aaaa:bbbb:cccc:dddd")).res.status).toBe(201);
    const third = await register({}, "2001:0db8:0001:0002:ffff::9");
    expect(third.res.status).toBe(429);
    expect(third.data.error).toBe("too_many_games");
    expect((await register({}, "2001:db8:1:3::1")).res.status).toBe(201);
  });

  it("refuses new games when 300 are open", async () => {
    for (let i = 0; i < 300; i++) expect((await register()).res.status).toBe(201);
    const over = await register();
    expect(over.res.status).toBe(503);
    expect(over.data.error).toBe("lobby_full");
  }, 30_000);

  it("frees the address when a game expires", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const ip = freshIp();
    await register({}, ip);
    await register({}, ip);
    await setLobbyClock(t0 + 90_000);
    expect((await register({}, ip)).res.status).toBe(201);
  });
});

describe("one open copy of a game", () => {
  it("refuses a second host of the same save and names the open copy", async () => {
    const first = await register();
    const again = await register({ gameId: first.body.gameId });
    expect(again.res.status).toBe(409);
    expect(again.data).toMatchObject({ error: "game_already_open", code: first.code });
    expect(again.data.hostToken).toBeUndefined();
  });

  it("allows the save again after the first copy closes", async () => {
    const first = await register();
    await call(`/v1/games/${first.code}`, { method: "DELETE", token: first.hostToken });
    expect((await register({ gameId: first.body.gameId })).res.status).toBe(201);
  });

  it("lets the host register its own game again with its code and token", async () => {
    const first = await register();
    const again = await register({ gameId: first.body.gameId, code: first.code, hostToken: first.hostToken });
    expect(again.res.status).toBe(200);
    expect(again.data).toMatchObject({ code: first.code, hostToken: first.hostToken });
  });

  it("restores a game with the same code after the lobby forgets it", async () => {
    const first = await register();
    await withLobby((inst) => inst.forgetAll());
    const again = await register({ gameId: first.body.gameId, code: first.code, hostToken: first.hostToken });
    expect(again.res.status).toBe(200);
    expect(again.data.code).toBe(first.code);
    const wrong = await register({ code: first.code, hostToken: "nope" });
    expect(wrong.res.status).toBe(401);
  });
});

describe("join step", () => {
  it("compares the version and names the host's version", async () => {
    const { code } = await register({ version: "0.7.0" });
    const res = await call(`/v1/games/${code}/join`, { method: "POST", ip: freshIp(), body: { version: "0.6.0", name: "Ann" } });
    expect(res.status).toBe(409);
    expect(await json(res)).toMatchObject({ error: "version_mismatch", hostVersion: "0.7.0" });
  });

  it("checks the password on the server", async () => {
    const { code } = await register({ password: "open sesame" });
    const none = await call(`/v1/games/${code}/join`, { method: "POST", ip: freshIp(), body: { version: "0.7.0", name: "Ann" } });
    expect(none.status).toBe(403);
    expect((await json(none)).error).toBe("wrong_password");
    const bad = await call(`/v1/games/${code}/join`, {
      method: "POST",
      ip: freshIp(),
      body: { version: "0.7.0", name: "Ann", password: "open sesamE" },
    });
    expect(bad.status).toBe(403);
    const good = await call(`/v1/games/${code}/join`, {
      method: "POST",
      ip: freshIp(),
      body: { version: "0.7.0", name: "Ann", password: "open sesame" },
    });
    expect(good.status).toBe(200);
    const data = await json(good);
    expect(typeof data.ticket).toBe("string");
    expect(data.expireSeconds).toBe(60);
  });

  it("does not keep the password in plain text", async () => {
    await register({ password: "plain-text-check" });
    const dump = await withLobby((inst) => inst.debugDump());
    expect(dump).not.toContain("plain-text-check");
  });

  const tryJoin = (code: string, ip: string, password: string) =>
    call(`/v1/games/${code}/join`, { method: "POST", ip, body: { version: "0.7.0", name: "Ann", password } });

  it("slows down password guessing from one address, and lets others join", async () => {
    const { code } = await register({ password: "right" });
    const guesser = freshIp();
    for (let i = 0; i < 5; i++) expect((await tryJoin(code, guesser, `guess${i}`)).status).toBe(403);
    const sixth = await tryJoin(code, guesser, "guess5");
    expect(sixth.status).toBe(429);
    expect((await json(sixth)).error).toBe("too_many_attempts");
    expect((await tryJoin(code, guesser, "right")).status).toBe(429);
    expect((await tryJoin(code, freshIp(), "right")).status).toBe(200);
  });

  // workerd may run these one after another. The lobby counts each guess before its hash anyway.
  it("counts guesses sent at the same time", async () => {
    const { code } = await register({ password: "right" });
    const guesser = freshIp();
    const results = await Promise.all([...Array(10).keys()].map((i) => tryJoin(code, guesser, `g${i}`)));
    expect(results.filter((r) => r.status === 403).length).toBe(5);
    expect(results.filter((r) => r.status === 429).length).toBe(5);
  });

  it("limits guesses from all addresses together to 30 a minute", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const { code } = await register({ password: "right" });
    for (let i = 0; i < 30; i++) expect((await tryJoin(code, freshIp(), `g${i}`)).status).toBe(403);
    expect((await tryJoin(code, freshIp(), "right")).status).toBe(429);
    await setLobbyClock(t0 + 60_000);
    expect((await tryJoin(code, freshIp(), "right")).status).toBe(200);
  });

  it("answers unknown_game for a game that is not open", async () => {
    const res = await call(`/v1/games/ABCDEFGH/join`, { method: "POST", ip: freshIp(), body: { version: "0.7.0", name: "Ann" } });
    expect(res.status).toBe(404);
    expect((await json(res)).error).toBe("unknown_game");
  });
});

describe("eviction: the runtime removes an idle lobby from memory", () => {
  const heartbeat = (code: string, token: string, ip: string) =>
    call(`/v1/games/${code}/heartbeat`, { method: "POST", token, ip, body: { day: "2001-09-01" } });

  it("keeps every open game, its host token and its password", async () => {
    const g = await register({ password: "pw" });
    await evictDurableObject(lobbyStub());
    expect((await heartbeat(g.code, g.hostToken, g.ip)).status).toBe(200);
    expect((await json(await call("/v1/games"))).games.map((x: any) => x.code)).toContain(g.code);
    const join = (password: string) =>
      call(`/v1/games/${g.code}/join`, { method: "POST", ip: freshIp(), body: { version: "0.7.0", name: "Ann", password } });
    expect((await join("nope")).status).toBe(403);
    expect((await join("pw")).status).toBe(200);
  });

  it("keeps the one open copy of a save", async () => {
    const g = await register();
    await evictDurableObject(lobbyStub());
    const again = await register({ gameId: g.body.gameId });
    expect(again.res.status).toBe(409);
    expect(again.data.code).toBe(g.code);
  });

  it("keeps heartbeat updates that are 60 seconds old", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const g = await register();
    await setLobbyClock(t0 + 60_000);
    expect((await heartbeat(g.code, g.hostToken, g.ip)).status).toBe(200);
    await evictDurableObject(lobbyStub());
    expect((await json(await call(`/v1/games/${g.code}`))).game.day).toBe("2001-09-01");
  });

  it("writes a heartbeat at most once a minute, also when a field changed", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const g = await register();
    const stored = () => withLobby(async (_i, state) => (await state.storage.get<any>(`g:${g.code}`)) as any);
    await setLobbyClock(t0 + 30_000);
    await heartbeat(g.code, g.hostToken, g.ip);
    expect((await stored()).lastSeen).toBe(t0);
    expect((await stored()).info.day).toBe("2001-08-11");
    await setLobbyClock(t0 + 60_000);
    await heartbeat(g.code, g.hostToken, g.ip);
    expect((await stored()).lastSeen).toBe(t0 + 60_000);
    expect((await stored()).info.day).toBe("2001-09-01");
  });

  it("after an eviction, takes the latest time a game can have been seen", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const g = await register();
    await setLobbyClock(t0 + 30_000);
    await heartbeat(g.code, g.hostToken, g.ip); // not written: the stored time is t0
    await evictDurableObject(lobbyStub()); // the load runs at t0 + 30 s
    await setLobbyClock(t0 + 119_000);
    expect((await json(await call("/v1/games"))).games.map((x: any) => x.code)).toContain(g.code);
    await setLobbyClock(t0 + 120_000);
    expect((await json(await call("/v1/games"))).games.map((x: any) => x.code)).not.toContain(g.code);
  });

  it("deletes more than 128 expired games in groups", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    for (let i = 0; i < 130; i++) await register();
    const sizes: number[] = [];
    await withLobby((_inst, state) => {
      const del = state.storage.delete.bind(state.storage);
      (state.storage as any).delete = (k: string | string[]) => {
        sizes.push(Array.isArray(k) ? k.length : 1);
        return del(k as any);
      };
    });
    await setLobbyClock(t0 + 90_000);
    await call("/v1/games");
    expect(Math.max(...sizes)).toBeLessThanOrEqual(128);
    expect((await withLobby(async (_i, state) => (await state.storage.list({ prefix: "g:" })).size))).toBe(0);
  }, 30_000);

  it("still drops a game with no heartbeat for 90 seconds", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const g = await register();
    await evictDurableObject(lobbyStub());
    await setLobbyClock(t0 + 89_000);
    expect((await json(await call("/v1/games"))).games.map((x: any) => x.code)).toContain(g.code);
    await setLobbyClock(t0 + 90_000);
    expect((await json(await call("/v1/games"))).games.map((x: any) => x.code)).not.toContain(g.code);
    expect((await withLobby(async (_i, state) => [...(await state.storage.list()).keys()]))).toEqual([]);
  });

  it("learns each host's address again from its next heartbeat, for the limit of 2", async () => {
    const ip = freshIp();
    const a = await register({}, ip);
    const b = await register({}, ip);
    await evictDurableObject(lobbyStub());
    expect(await withLobby((inst) => inst.addressCount())).toBe(0);
    await heartbeat(a.code, a.hostToken, ip);
    await heartbeat(b.code, b.hostToken, ip);
    const third = await register({}, ip);
    expect(third.res.status).toBe(429);
    expect(third.data.error).toBe("too_many_games");
  });

  it("stores no internet address and no address hash", async () => {
    const ip = "192.0.2.123";
    const g = await register({ password: "pw" }, ip);
    await heartbeat(g.code, g.hostToken, ip);
    const keys = await withLobby((inst) => inst.addressKeys());
    expect(keys.length).toBe(1);
    const stored = await withLobby(async (_i, state) => JSON.stringify([...(await state.storage.list()).entries()]));
    expect(stored).toContain(g.code);
    expect(stored).not.toContain(ip);
    expect(stored).not.toContain("192.0.2");
    for (const k of keys) expect(stored).not.toContain(k);
    expect(stored).not.toContain('"pw"');
  });
});
