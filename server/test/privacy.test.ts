import { abortAllDurableObjects, runInDurableObject } from "cloudflare:test";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import type { Relay } from "../src/relay";
import wranglerConfig from "../wrangler.jsonc?raw";
import { call, hostSocket, joinTicket, json, managerSocket, register, relayStub, setLobbyClock, withLobby, resetObjects } from "./helpers";

const HOST_IP = "203.0.113.77";
const JOIN_IP = "2001:db8::4242";

const logged: string[] = [];
beforeEach(async () => {
  await resetObjects();
  logged.length = 0;
  for (const level of ["log", "info", "warn", "error", "debug", "trace"] as const) {
    vi.spyOn(console, level).mockImplementation((...args: unknown[]) => {
      logged.push(args.map((a) => (a instanceof Error ? `${a.message} ${a.stack}` : String(a))).join(" "));
    });
  }
});
afterEach(async () => {
  vi.restoreAllMocks();
  await abortAllDurableObjects();
});

describe("internet addresses", () => {
  it("are in no log line, stored record or answer, through a full game", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const g = await register({ password: "pw" }, HOST_IP);
    const host = await hostSocket(g.code, g.hostToken);
    const ticket = await joinTicket(g.code, { password: "pw" }, JOIN_IP);
    const m = await managerSocket(g.code, ticket);
    await host.nextJson();
    m.send([1, 2, 3]);
    await host.nextBytes();

    const answers: string[] = [JSON.stringify(g.data)];
    answers.push(await (await call("/v1/games", { ip: JOIN_IP })).text());
    answers.push(await (await call(`/v1/games/${g.code}`, { ip: JOIN_IP })).text());
    answers.push(await (await call("/v1/games", { method: "POST", ip: HOST_IP, rawBody: "{" })).text());
    answers.push(
      await (await call(`/v1/games/${g.code}/join`, { method: "POST", ip: JOIN_IP, body: { version: "0.7.0", name: "x", password: "no" } })).text(),
    );
    await register({}, HOST_IP);
    answers.push(JSON.stringify((await register({}, HOST_IP)).data));

    const lobbyStored = JSON.stringify(await withLobby(async (_inst, state) => [...(await state.storage.list()).entries()]));
    expect(lobbyStored).toContain(g.code);
    for (const k of await withLobby((inst) => inst.addressKeys())) expect(lobbyStored).not.toContain(k);
    const relayStored = await runInDurableObject(relayStub(g.code), async (_inst: Relay, state) => [
      ...(await state.storage.list()).entries(),
    ]);
    const lobbyMemory = await withLobby((inst) => inst.debugDump());

    for (const ip of [HOST_IP, JOIN_IP]) {
      for (const line of logged) expect(line).not.toContain(ip);
      for (const a of answers) expect(a).not.toContain(ip);
      expect(JSON.stringify(relayStored)).not.toContain(ip);
      expect(lobbyStored).not.toContain(ip);
      expect(lobbyMemory).not.toContain(ip);
    }
  });

  it("are gone from memory when the game closes or expires", async () => {
    const t0 = 1_800_000_000_000;
    await setLobbyClock(t0);
    const a = await register({}, HOST_IP);
    await register({}, HOST_IP);
    await call(`/v1/games/${a.code}`, { method: "DELETE", token: a.hostToken });
    expect(await withLobby((inst) => inst.addressCount())).toBe(1);
    await setLobbyClock(t0 + 90_000);
    await call("/v1/games");
    expect(await withLobby((inst) => inst.addressCount())).toBe(0);
  });

  it("are not passed to the relay", async () => {
    const g = await register({}, HOST_IP);
    await runInDurableObject(relayStub(g.code), (inst: Relay) => {
      inst.seenHeaders = [];
    });
    await hostSocket(g.code, g.hostToken);
    const seen = await runInDurableObject(relayStub(g.code), (inst: Relay) => inst.seenHeaders!.join("\n"));
    expect(seen).toContain("upgrade");
    expect(seen).not.toContain(HOST_IP);
    expect(seen.toLowerCase()).not.toContain("cf-connecting-ip");
  });
});

describe("Wrangler config", () => {
  it("turns Workers Logs off", () => {
    const config = JSON.parse(wranglerConfig.replace(/^\s*\/\/.*$/gm, ""));
    expect(config.observability).toEqual({ enabled: false });
    expect(config.logpush).toBeFalsy();
    expect(config.tail_consumers).toBeUndefined();
  });
});
