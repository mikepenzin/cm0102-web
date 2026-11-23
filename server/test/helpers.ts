import { expect } from "vitest";
import { exports } from "cloudflare:workers";
import { abortAllDurableObjects, env, runInDurableObject } from "cloudflare:test";
import type { Lobby } from "../src/lobby";
import { setAnswerCacheMs } from "../src/flood";

export const BASE = "https://lobby.test";

let ipCounter = 1;
/** A fresh documentation-range address (RFC 5737), so each test has its own per-address count. */
export function freshIp(): string {
  const n = ipCounter++;
  return `198.51.${Math.floor(n / 250)}.${(n % 250) + 1}`;
}

let gameCounter = 1;
export function freshGameId(): string {
  return `save-${Date.now().toString(16)}-${gameCounter++}`;
}

export async function call(
  path: string,
  init: { method?: string; body?: unknown; ip?: string; token?: string; rawBody?: string } = {},
): Promise<Response> {
  const headers: Record<string, string> = {};
  if (init.ip) headers["CF-Connecting-IP"] = init.ip;
  if (init.token) headers["Authorization"] = `Bearer ${init.token}`;
  let body: string | undefined;
  if (init.rawBody !== undefined) body = init.rawBody;
  else if (init.body !== undefined) body = JSON.stringify(init.body);
  if (body !== undefined) headers["Content-Type"] = "application/json";
  return exports.default.fetch(`${BASE}${path}`, { method: init.method ?? "GET", headers, body });
}

export async function json(res: Response): Promise<any> {
  expect(res.headers.get("Content-Type")).toContain("application/json");
  return res.json();
}

export function gameBody(over: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    gameId: freshGameId(),
    name: "Sunday League",
    hostName: "Max",
    version: "0.7.0",
    database: "Original 2001/02",
    season: 2001,
    day: "2001-08-11",
    humanClubs: ["Arsenal", "Scunthorpe United"],
    freeSeats: 14,
    managersOnline: 2,
    ...over,
  };
}

export async function register(over: Record<string, unknown> = {}, ip = freshIp()) {
  const body = gameBody(over);
  const res = await call("/v1/games", { method: "POST", body, ip });
  const data = await json(res);
  return { res, data, body, ip, code: data.code as string, hostToken: data.hostToken as string };
}

export function lobbyStub() {
  return env.LOBBY.get(env.LOBBY.idFromName("lobby"));
}

/** Sets the lobby's clock. Pass null to go back to the real clock. */
export async function setLobbyClock(ms: number | null): Promise<void> {
  await runInDurableObject(lobbyStub(), (inst: Lobby) => inst.setClock(ms));
}

export async function withLobby<R>(fn: (inst: Lobby, state: DurableObjectState) => R | Promise<R>): Promise<R> {
  return runInDurableObject(lobbyStub(), fn);
}

export function relayStub(code: string) {
  return env.RELAY.get(env.RELAY.idFromName(code));
}

/** A WebSocket with a queue of received messages. */
export class Socket {
  readonly messages: (string | ArrayBuffer)[] = [];
  closed: { code: number; reason: string } | null = null;
  private waiters: (() => void)[] = [];
  constructor(readonly ws: WebSocket) {
    ws.accept();
    (ws as unknown as { binaryType: string }).binaryType = "arraybuffer";
    ws.addEventListener("message", (e) => {
      this.messages.push(e.data as string | ArrayBuffer);
      this.wake();
    });
    ws.addEventListener("close", (e) => {
      this.closed = { code: e.code, reason: e.reason };
      this.wake();
    });
  }
  private wake() {
    const w = this.waiters;
    this.waiters = [];
    for (const f of w) f();
  }
  private async until(cond: () => boolean, what: string): Promise<void> {
    const deadline = Date.now() + 3000;
    while (!cond()) {
      if (Date.now() > deadline) throw new Error(`timed out waiting for ${what}`);
      await new Promise<void>((r) => {
        this.waiters.push(r);
        setTimeout(r, 50);
      });
    }
  }
  async next(): Promise<string | ArrayBuffer> {
    await this.until(() => this.messages.length > 0, "a message");
    return this.messages.shift()!;
  }
  async nextJson(): Promise<any> {
    const m = await this.next();
    expect(typeof m).toBe("string");
    return JSON.parse(m as string);
  }
  async nextBytes(): Promise<number[]> {
    const m = await this.next();
    expect(m).toBeInstanceOf(ArrayBuffer);
    return [...new Uint8Array(m as ArrayBuffer)];
  }
  async waitClosed(): Promise<{ code: number; reason: string }> {
    await this.until(() => this.closed !== null, "close");
    return this.closed!;
  }
  /** Waits a short time and checks that no message came. */
  async expectSilence(ms = 150): Promise<void> {
    await new Promise((r) => setTimeout(r, ms));
    expect(this.messages).toEqual([]);
  }
  send(data: string | number[]) {
    this.ws.send(typeof data === "string" ? data : new Uint8Array(data));
  }
}

export async function openRelay(code: string, query: string): Promise<{ res: Response; socket: Socket | null }> {
  const res = await exports.default.fetch(`${BASE}/v1/games/${code}/relay?${query}`, {
    headers: { Upgrade: "websocket" },
  });
  if (res.webSocket) return { res, socket: new Socket(res.webSocket) };
  // Read a refusal's body at once, so the request ends. A test may read it again.
  const body = await res.text();
  return { res: new Response(body, { status: res.status, headers: res.headers }), socket: null };
}

export async function hostSocket(code: string, hostToken: string): Promise<Socket> {
  const { res, socket } = await openRelay(code, `role=host&token=${encodeURIComponent(hostToken)}`);
  expect(res.status).toBe(101);
  return socket!;
}

export async function joinTicket(code: string, over: Record<string, unknown> = {}, ip = freshIp()): Promise<string> {
  const res = await call(`/v1/games/${code}/join`, {
    method: "POST",
    ip,
    body: { version: "0.7.0", name: "Guest", ...over },
  });
  const data = await json(res);
  expect(res.status, JSON.stringify(data)).toBe(200);
  return data.ticket;
}

export async function managerSocket(code: string, ticket: string): Promise<Socket> {
  const { res, socket } = await openRelay(code, `role=manager&ticket=${encodeURIComponent(ticket)}`);
  expect(res.status).toBe(101);
  return socket!;
}

/** Starts each test with fresh objects and an empty lobby. Stored games outlive an abort. */
export async function resetObjects(): Promise<void> {
  // Most tests change the lobby and read the list at once, so the Worker's 2 s copy is off (flood.test.ts turns it on).
  setAnswerCacheMs(0);
  await abortAllDurableObjects();
  await setLobbyClock(null);
  await withLobby((inst) => inst.forgetAll());
  await abortAllDurableObjects();
}
