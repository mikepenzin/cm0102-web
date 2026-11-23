// The relay must be able to hibernate between messages, because Cloudflare bills run time
// while an object is awake or cannot hibernate (Durable Objects pricing, checked 2026-10-29).
import { evictDurableObject, runInDurableObject } from "cloudflare:test";
import { beforeEach, describe, expect, it } from "vitest";
import lobbySource from "../src/lobby.ts?raw";
import relaySource from "../src/relay.ts?raw";
import indexSource from "../src/index.ts?raw";
import type { Relay } from "../src/relay";
import { hostSocket, joinTicket, managerSocket, register, relayStub, resetObjects } from "./helpers";

beforeEach(resetObjects);

const handled = (code: string) => runInDurableObject(relayStub(code), (inst: Relay) => inst.handled);

describe("the relay can sleep", () => {
  it("answers a text ping without running its own code", async () => {
    const g = await register();
    const host = await hostSocket(g.code, g.hostToken);
    const m = await managerSocket(g.code, await joinTicket(g.code));
    await host.nextJson();
    const before = await handled(g.code);
    for (let i = 0; i < 5; i++) {
      host.send("ping");
      m.send("ping");
      expect(await host.next()).toBe("pong");
      expect(await m.next()).toBe("pong");
    }
    expect(await handled(g.code)).toBe(before);
    m.send([1]);
    await host.nextBytes();
    expect(await handled(g.code)).toBe(before + 1);
  });

  it("answers a ping after an eviction, with the sockets kept", async () => {
    const g = await register();
    const host = await hostSocket(g.code, g.hostToken);
    await evictDurableObject(relayStub(g.code));
    host.send("ping");
    expect(await host.next()).toBe("pong");
  });

  it.each([
    ["relay", relaySource],
    ["lobby", lobbySource],
    ["worker", indexSource],
  ])("the %s code starts no timer and accepts no socket outside the Hibernation API", (_name, source) => {
    const code = source.replace(/\/\/.*$/gm, "").replace(/\/\*[\s\S]*?\*\//g, "");
    expect(code).not.toMatch(/\bset(Timeout|Interval|Immediate)\s*\(/);
    expect(code).not.toMatch(/\.accept\s*\(\s*\)/);
    expect(code).not.toMatch(/\bfetch\s*\(\s*["'`]http/);
  });

  it("the relay sends no request of its own while it passes frames", () => {
    const code = relaySource.replace(/\/\/.*$/gm, "");
    const message = code.slice(code.indexOf("async webSocketMessage"), code.indexOf("async webSocketClose"));
    expect(message).not.toMatch(/\bfetch\s*\(|env\.LOBBY|storage\.(put|delete|setAlarm)/);
  });
});
