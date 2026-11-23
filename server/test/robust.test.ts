import { abortAllDurableObjects } from "cloudflare:test";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import worker from "../src/index";
import { BASE, call, freshIp, gameBody, register, resetObjects } from "./helpers";

beforeEach(async () => {
  await resetObjects();
});
afterEach(async () => {
  await abortAllDurableObjects();
});

/** Every answer must be small JSON with "ok" or a known "error", so the game can always parse it. */
async function expectGameSafe(res: Response) {
  expect(res.headers.get("Content-Type")).toContain("application/json");
  expect(res.headers.get("X-Bosman-Api")).toBe("1");
  const text = await res.text();
  expect(text.length).toBeLessThan(64 * 1024);
  const data = JSON.parse(text);
  if (res.status < 300) return data;
  expect(typeof data.error).toBe("string");
  expect(typeof data.message).toBe("string");
  return data;
}

describe("no path crashes the game side", () => {
  it.each([
    ["GET", "/"],
    ["GET", "/nope"],
    ["GET", "/v1"],
    ["PUT", "/v1/games"],
    ["POST", "/v1/games/ABCDEFGH"],
    ["GET", "/v1/games/abc"],
    ["GET", "/v1/games/%00%ff"],
    ["GET", "/v1/games/ABCDEFGH/heartbeat"],
    ["POST", "/v1/games/ABCDEFGH/heartbeat"],
    ["DELETE", "/v1/games/ABCDEFGH"],
    ["GET", "/v1/games/ABCDEFGH/relay"],
    ["POST", "/join/ABCDEFGH"], // GET gives the invite link page (join.test.ts)
  ])("%s %s answers JSON", async (method, path) => {
    const res = await call(path, { method, ip: freshIp() });
    expect(res.status).toBeGreaterThanOrEqual(400);
    expect(res.status).toBeLessThan(500);
    await expectGameSafe(res);
  });

  it.each([
    ["not JSON", "{"],
    ["an array", "[]"],
    ["null", "null"],
    ["a string", '"x"'],
    ["too big", JSON.stringify({ name: "x".repeat(10_000) })],
  ])("a register body that is %s answers bad_request", async (_what, rawBody) => {
    const res = await call("/v1/games", { method: "POST", rawBody, ip: freshIp() });
    expect(res.status).toBe(400);
    expect((await expectGameSafe(res)).error).toBe("bad_request");
  });

  it("a join body that is not JSON answers bad_request", async () => {
    const { code } = await register();
    const res = await call(`/v1/games/${code}/join`, { method: "POST", rawBody: "%%%", ip: freshIp() });
    expect(res.status).toBe(400);
    await expectGameSafe(res);
  });

  it("the list stays small", async () => {
    const res = await call("/v1/games");
    const data = await expectGameSafe(res);
    expect(Array.isArray(data.games)).toBe(true);
    expect(data.heartbeatSeconds).toBe(30);
  });

  it("health answers without touching a Durable Object", async () => {
    const res = await worker.fetch(new Request(`${BASE}/v1/health`), brokenEnv("anything"));
    expect(res.status).toBe(200);
    expect(await expectGameSafe(res)).toMatchObject({ ok: true, api: 1 });
  });
});

/** An env whose Durable Objects throw, as they do when the free plan's daily limit is used up. */
function brokenEnv(message: string): Env {
  const failing = new Proxy(
    {},
    {
      get(_t, prop) {
        if (prop === "then") return undefined;
        return () => {
          throw new Error(message);
        };
      },
    },
  );
  const ns = {
    idFromName: () => ({}),
    get: () => failing,
  };
  return { LOBBY: ns, RELAY: ns } as unknown as Env;
}

describe("free plan: full today", () => {
  const limitMessages = [
    "Exceeded allowed volume of requests in Durable Objects free tier.",
    "Durable Object storage operation exceeded the daily limit",
    "Your account has exceeded its daily request limit",
  ];

  it.each(limitMessages)("answers full_today with the time to 00:00 UTC for '%s'", async (msg) => {
    for (const [method, path, body] of [
      ["GET", "/v1/games", undefined],
      ["POST", "/v1/games", JSON.stringify(gameBody())],
      ["POST", "/v1/games/ABCDEFGH/join", JSON.stringify({ version: "0.7.0", name: "Ann" })],
      ["POST", "/v1/games/ABCDEFGH/heartbeat", undefined],
    ] as const) {
      const res = await worker.fetch(
        new Request(`${BASE}${path}`, { method, body, headers: { Authorization: "Bearer x" } }),
        brokenEnv(msg),

      );
      expect(res.status, path).toBe(503);
      const data = await expectGameSafe(res);
      expect(data.error).toBe("full_today");
      expect(data.message).toBe("Online games are full today, try again later");
      const now = new Date();
      const midnight = Date.UTC(now.getUTCFullYear(), now.getUTCMonth(), now.getUTCDate() + 1);
      const expected = Math.ceil((midnight - now.getTime()) / 1000);
      expect(Math.abs(data.retryAfterSeconds - expected)).toBeLessThanOrEqual(2);
      expect(res.headers.get("Retry-After")).toBe(String(data.retryAfterSeconds));
    }
  });

  it("answers full_today for a relay connection too", async () => {
    const res = await worker.fetch(
      new Request(`${BASE}/v1/games/ABCDEFGH/relay?role=host&token=x`, { headers: { Upgrade: "websocket" } }),
      brokenEnv(limitMessages[0]),

    );
    expect(res.status).toBe(503);
    expect((await expectGameSafe(res)).error).toBe("full_today");
  });

  it.each(["Worker exceeded CPU time limit.", "Durable Object exceeded its memory limit and was reset."])(
    "answers unavailable for '%s'",
    async (msg) => {
      const res = await worker.fetch(new Request(`${BASE}/v1/games`), brokenEnv(msg));
      expect((await expectGameSafe(res)).error).toBe("unavailable");
    },
  );

  it("answers unavailable for any other server failure, with no detail", async () => {
    const res = await worker.fetch(new Request(`${BASE}/v1/games`), brokenEnv("internal error 203.0.113.9"));
    expect(res.status).toBe(503);
    const data = await expectGameSafe(res);
    expect(data.error).toBe("unavailable");
    expect(JSON.stringify(data)).not.toContain("203.0.113.9");
  });
});
