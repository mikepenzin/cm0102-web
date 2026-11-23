import { abortAllDurableObjects, evictDurableObject, runDurableObjectAlarm, runInDurableObject } from "cloudflare:test";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import type { Relay } from "../src/relay";
import {
  call,
  hostSocket,
  joinTicket,
  managerSocket,
  openRelay,
  register,
  relayStub,
  setLobbyClock,
  withLobby,
  json,
  Socket, resetObjects } from "./helpers";

beforeEach(async () => {
  await resetObjects();
});
afterEach(async () => {
  await abortAllDurableObjects();
});

async function relayKeys(code: string): Promise<string[]> {
  return runInDurableObject(relayStub(code), async (_inst: Relay, state) => [...(await state.storage.list()).keys()]);
}

/** 1 when the lobby's list holds the game. */
async function listed(code: string): Promise<boolean> {
  const data = await json(await call("/v1/games"));
  return data.games.some((g: { code: string }) => g.code === code);
}

/** Polls test() for up to 3 s: the relay's close events and its call to the lobby come after the client's close. */
async function waitFor(test: () => Promise<boolean>): Promise<boolean> {
  for (let i = 0; i < 60; i++) {
    if (await test()) return true;
    await new Promise((r) => setTimeout(r, 50));
  }
  return false;
}

async function gameWithHost() {
  const g = await register();
  const host = await hostSocket(g.code, g.hostToken);
  return { ...g, host };
}

async function addManager(code: string, host: Socket): Promise<{ socket: Socket; ch: number }> {
  const socket = await managerSocket(code, await joinTicket(code));
  const open = await host.nextJson();
  expect(open.t).toBe("open");
  return { socket, ch: open.ch };
}

describe("relay", () => {
  it("passes binary frames both ways, unchanged, with the channel byte on the host side", async () => {
    const { code, host } = await gameWithHost();
    const m = await addManager(code, host);
    expect(m.ch).toBe(0);

    m.socket.send([4, 0, 0, 0, 0xde, 0xad, 0xbe, 0xef]);
    expect(await host.nextBytes()).toEqual([0, 4, 0, 0, 0, 0xde, 0xad, 0xbe, 0xef]);

    host.send([0, 1, 2, 3, 255, 0]);
    expect(await m.socket.nextBytes()).toEqual([1, 2, 3, 255, 0]);
  });

  it("sends a host frame to the right manager only, or to all with channel 255", async () => {
    const { code, host } = await gameWithHost();
    const a = await addManager(code, host);
    const b = await addManager(code, host);
    expect([a.ch, b.ch]).toEqual([0, 1]);

    host.send([1, 9, 9]);
    expect(await b.socket.nextBytes()).toEqual([9, 9]);
    await a.socket.expectSilence();

    host.send([255, 7]);
    expect(await a.socket.nextBytes()).toEqual([7]);
    expect(await b.socket.nextBytes()).toEqual([7]);

    b.socket.send([5]);
    expect(await host.nextBytes()).toEqual([1, 5]);
  });

  it("allows up to 16 managers and refuses the 17th", async () => {
    const { code, host } = await gameWithHost();
    const managers = [];
    for (let i = 0; i < 16; i++) managers.push(await addManager(code, host));
    expect(managers.map((m) => m.ch)).toEqual([...Array(16).keys()]);
    const { res } = await openRelay(code, `role=manager&ticket=${encodeURIComponent(await joinTicket(code))}`);
    expect(res.status).toBe(409);
    expect((await json(res)).error).toBe("relay_full");
  });

  it("tells the host when a manager leaves and reuses the channel", async () => {
    const { code, host } = await gameWithHost();
    const a = await addManager(code, host);
    await addManager(code, host);
    a.socket.ws.close(1000, "bye");
    expect(await host.nextJson()).toEqual({ t: "close", ch: 0 });
    const c = await addManager(code, host);
    expect(c.ch).toBe(0);
  });

  it("closes every manager with host_left when the host leaves", async () => {
    const { code, host } = await gameWithHost();
    const a = await addManager(code, host);
    const b = await addManager(code, host);
    host.ws.close(1000, "quit");
    expect(await a.socket.waitClosed()).toEqual({ code: 4000, reason: "host_left" });
    expect(await b.socket.waitClosed()).toEqual({ code: 4000, reason: "host_left" });
  });

  it("lets the host kick a manager", async () => {
    const { code, host } = await gameWithHost();
    const a = await addManager(code, host);
    host.send(JSON.stringify({ t: "kick", ch: a.ch }));
    expect(await a.socket.waitClosed()).toEqual({ code: 4003, reason: "kicked" });
  });

  it("keeps a kicked manager's channel until the host hears that it closed", async () => {
    const { code, host } = await gameWithHost();
    await addManager(code, host);
    const used = await runInDurableObject(relayStub(code), (inst: Relay, state) => {
      for (const ws of state.getWebSockets("m0")) ws.close(4003, "kicked");
      // The close event has not run yet, so the host does not know. Channel 0 must stay in use.
      return (inst as unknown as { usedChannels(): number[] }).usedChannels();
    });
    expect(used).toEqual([0]);
    expect(await host.nextJson()).toEqual({ t: "close", ch: 0 });
    const b = await addManager(code, host);
    expect(b.ch).toBe(0);
  });

  it("ignores a bad control message from the host", async () => {
    const { code, host } = await gameWithHost();
    const a = await addManager(code, host);
    host.send("not json");
    host.send(JSON.stringify({ t: "kick", ch: "x" }));
    host.send([]);
    host.send([200, 1]);
    await a.socket.expectSilence();
    expect(a.socket.closed).toBeNull();
    host.send([a.ch, 1]);
    expect(await a.socket.nextBytes()).toEqual([1]);
  });

  it("closes a manager that sends text", async () => {
    const { code, host } = await gameWithHost();
    const a = await addManager(code, host);
    a.socket.send("hello");
    expect((await a.socket.waitClosed()).code).toBe(1003);
  });

  it("answers ping with pong for keep-alive", async () => {
    const { code, host } = await gameWithHost();
    const a = await addManager(code, host);
    a.socket.send("ping");
    expect(await a.socket.next()).toBe("pong");
    host.send("ping");
    expect(await host.next()).toBe("pong");
  });

  it("refuses a host with a wrong token, and a manager with a bad, reused or expired ticket", async () => {
    const { code, hostToken, host } = await gameWithHost();
    const badHost = await openRelay(code, "role=host&token=wrong");
    expect(badHost.res.status).toBe(401);
    for (const q of ["role=manager&ticket=nonsense", "role=manager", "role=spectator", ""]) {
      const r = await openRelay(code, q);
      expect(r.res.status, q).toBeGreaterThanOrEqual(400);
      expect(r.res.status, q).toBeLessThan(500);
      expect(r.socket).toBeNull();
    }
    const ticket = await joinTicket(code);
    await managerSocket(code, ticket);
    await host.nextJson();
    const reused = await openRelay(code, `role=manager&ticket=${encodeURIComponent(ticket)}`);
    expect(reused.res.status).toBe(401);

    const late = await joinTicket(code);
    await runInDurableObject(relayStub(code), (inst: Relay) => {
      inst.now = () => Date.now() + 61_000;
    });
    const expired = await openRelay(code, `role=manager&ticket=${encodeURIComponent(late)}`);
    expect(expired.res.status).toBe(401);
    expect(hostToken).toBeTruthy();
  });

  it("answers the connection test's request for a code with no game, and stores nothing (0.7.0 slice 7)", async () => {
    const probe = await openRelay("22222222", "role=manager&ticket=connection-test");
    expect(probe.socket).toBeNull();
    expect(probe.res.status).toBe(404);
    expect(probe.res.headers.get("X-Bosman-Api")).toBe("1");
    expect(await json(probe.res)).toMatchObject({ error: "unknown_game" });
    expect(await relayKeys("22222222")).toEqual([]);
    // Without the Upgrade header the Worker answers first: the test reads this as a proxy that took it off.
    const stripped = await call("/v1/games/22222222/relay?role=manager&ticket=connection-test");
    expect(stripped.status).toBe(426);
    expect(await json(stripped)).toMatchObject({ error: "upgrade_required" });
  });

  it("does not accept a ticket from another game", async () => {
    const one = await gameWithHost();
    const two = await gameWithHost();
    const ticket = await joinTicket(one.code);
    const r = await openRelay(two.code, `role=manager&ticket=${encodeURIComponent(ticket)}`);
    expect(r.res.status).toBe(401);
  });

  it("refuses a manager while the host is not connected", async () => {
    const g = await register();
    const r = await openRelay(g.code, `role=manager&ticket=${encodeURIComponent(await joinTicket(g.code))}`);
    expect(r.res.status).toBe(409);
    expect((await json(r.res)).error).toBe("host_not_connected");
  });

  it("refuses a plain request without a WebSocket upgrade", async () => {
    const g = await register();
    const res = await call(`/v1/games/${g.code}/relay?role=host&token=${g.hostToken}`);
    expect(res.status).toBe(426);
  });

  it("replaces an old host connection with a new one and keeps the managers", async () => {
    const { code, hostToken, host } = await gameWithHost();
    const a = await addManager(code, host);
    const host2 = await hostSocket(code, hostToken);
    expect(await host2.nextJson()).toEqual({ t: "open", ch: a.ch });
    expect(await host.waitClosed()).toEqual({ code: 4001, reason: "replaced" });
    await a.socket.expectSilence();
    expect(a.socket.closed).toBeNull();
    a.socket.send([1]);
    expect(await host2.nextBytes()).toEqual([a.ch, 1]);
  });

  it("closes a frame larger than 1 MiB", async () => {
    const { code, host } = await gameWithHost();
    const a = await addManager(code, host);
    a.socket.ws.send(new Uint8Array(1024 * 1024 + 1));
    expect((await a.socket.waitClosed()).code).toBe(1009);
  });

  it("closes the relay and forgets its key when the host closes the game", async () => {
    const { code, hostToken, host } = await gameWithHost();
    const a = await addManager(code, host);
    await call(`/v1/games/${code}`, { method: "DELETE", token: hostToken });
    expect(await a.socket.waitClosed()).toEqual({ code: 4000, reason: "host_left" });
    expect((await host.waitClosed()).code).toBe(4004);
    const keys = await runInDurableObject(relayStub(code), async (_inst: Relay, state) => [...(await state.storage.list()).keys()]);
    expect(keys).toEqual([]);
    expect((await openRelay(code, `role=host&token=${encodeURIComponent(hostToken)}`)).res.status).toBe(404);
  });

  it("drops the game from the list at once when its host quits with a normal close, and closes the relay (item 216)", async () => {
    const { code, hostToken, host } = await gameWithHost();
    const a = await addManager(code, host);
    expect(await listed(code)).toBe(true);
    host.ws.close(1000, "quit");
    expect(await a.socket.waitClosed()).toEqual({ code: 4000, reason: "host_left" });
    expect(await waitFor(async () => !(await listed(code)))).toBe(true);
    expect(await waitFor(async () => (await relayKeys(code)).length === 0)).toBe(true);
    expect((await openRelay(code, `role=host&token=${encodeURIComponent(hostToken)}`)).res.status).toBe(404);
  });

  it("keeps the game and the relay when the host's connection drops, so the host can connect again (item 216)", async () => {
    const { code, hostToken, host } = await gameWithHost();
    host.ws.close(3000, "drop");
    await host.waitClosed();
    await new Promise((r) => setTimeout(r, 300));
    expect(await listed(code)).toBe(true);
    expect(await relayKeys(code)).toContain("key");
    const again = await hostSocket(code, hostToken);
    const b = await addManager(code, again);
    b.socket.send([7]);
    expect(await again.nextBytes()).toEqual([b.ch, 7]);
  });

  it("closes the relay when the lobby drops the game for no heartbeat", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const { code, host } = await gameWithHost();
    await setLobbyClock(t0 + 95_000);
    await call("/v1/games");
    expect((await host.waitClosed()).code).toBe(4004);
  });

  it("forgets a relay that the lobby no longer lists, at its hourly check", async () => {
    const g = await register();
    expect(await runDurableObjectAlarm(relayStub(g.code))).toBe(true);
    expect(await relayKeys(g.code)).toContain("key");
    await withLobby((inst) => inst.forgetAll());
    expect(await runDurableObjectAlarm(relayStub(g.code))).toBe(true);
    expect(await relayKeys(g.code)).toEqual([]);
  });

  it("keeps a relay with a connected host at its hourly check, even when the lobby forgot it", async () => {
    const { code } = await gameWithHost();
    await withLobby((inst) => inst.forgetAll());
    expect(await runDurableObjectAlarm(relayStub(code))).toBe(true);
    expect(await relayKeys(code)).toContain("key");
  });

  it("keeps working after an eviction: frames, a new manager, a used ticket, kick and host quit", async () => {
    const { code, hostToken, host } = await gameWithHost();
    const a = await addManager(code, host);
    const used = await joinTicket(code);
    const b = await managerSocket(code, used);
    expect((await host.nextJson()).ch).toBe(1);
    await evictDurableObject(relayStub(code));

    a.socket.send([1, 2]);
    expect(await host.nextBytes()).toEqual([0, 1, 2]);
    host.send([255, 9]);
    expect(await a.socket.nextBytes()).toEqual([9]);
    expect(await b.nextBytes()).toEqual([9]);

    expect((await openRelay(code, `role=manager&ticket=${encodeURIComponent(used)}`)).res.status).toBe(401);
    const c = await addManager(code, host);
    expect(c.ch).toBe(2);
    expect((await openRelay(code, "role=host&token=wrong")).res.status).toBe(401);

    await evictDurableObject(relayStub(code));
    host.send(JSON.stringify({ t: "kick", ch: 1 }));
    expect(await b.waitClosed()).toEqual({ code: 4003, reason: "kicked" });
    expect(await host.nextJson()).toEqual({ t: "close", ch: 1 });

    await evictDurableObject(relayStub(code));
    host.ws.close(1000, "quit");
    expect(await a.socket.waitClosed()).toEqual({ code: 4000, reason: "host_left" });
    expect(hostToken).toBeTruthy();
  });

  it("stores no game data: only the relay key and connection markers", async () => {
    const { code, host } = await gameWithHost();
    const a = await addManager(code, host);
    for (let i = 0; i < 5; i++) {
      a.socket.send([0x47, 0x41, 0x4d, 0x45, i]);
      await host.nextBytes();
    }
    const entries = await runInDurableObject(relayStub(code), async (_inst: Relay, state) => [
      ...(await state.storage.list()).entries(),
    ]);
    const keys = entries.map(([k]) => k);
    for (const k of keys) expect(k).toMatch(/^(code|key|hostConn|n:.+)$/);
    expect(JSON.stringify(entries)).not.toContain("GAME");
  });
});
