// The relay: one Durable Object for each online game (named by the game code).
// It passes binary frames between the host and up to 16 human managers and reads none of them.
// It uses the WebSocket Hibernation API, so an idle game costs no run time.
//
// Host socket:    binary frame = 1 channel byte + payload. Channel 0-15 is one manager, 255 is all.
//                 Text frames are control messages: relay -> host {"t":"open","ch":n} and
//                 {"t":"close","ch":n}; host -> relay {"t":"kick","ch":n}.
// Manager socket: binary frame = payload only. The relay adds the channel byte for the host.
// Both:           the text frame "ping" gets "pong" without waking the object.
//
// When the current host connection closes with a normal close (1000), the game stopped hosting: the relay asks the
// lobby to drop the game and closes itself (item 216). Any other close (a dropped line, 1006) keeps both, so the host
// can connect again.
//
// Stored: the game code, the relay key, the id of the current host connection and used ticket ids.
// No game data and no internet address. An hourly alarm removes them when the lobby no longer
// lists the game and no host is connected, so a relay that the lobby forgot does not stay for good.
import { DurableObject } from "cloudflare:workers";
import { b64url, fromB64url, hmac, randomBytes, sameBytes } from "./crypto";
import { fail } from "./http";
import { TICKET_SECONDS } from "./lobby";
import { MAX_MANAGERS } from "./validate";

export const MAX_FRAME_BYTES = 1024 * 1024;
export const ALL_MANAGERS = 255;

export const CLOSE_HOST_LEFT = 4000;
export const CLOSE_REPLACED = 4001;
export const CLOSE_KICKED = 4003;
export const CLOSE_GAME_CLOSED = 4004;

export const CHECK_EVERY_MS = 60 * 60 * 1000;

type Attachment = { r: "h"; id: string } | { r: "m"; ch: number };

export class Relay extends DurableObject<Env> {
  /** The clock. Tests replace it. */
  now: () => number = () => Date.now();
  /** Test hook: when set, the relay records the header names and values of each request. */
  seenHeaders?: string[];
  /** Test hook: counts the messages that woke the object (a "ping" answered by the runtime does not). */
  handled = 0;

  // Memory copies of stored values. They are empty again after the object wakes from hibernation.
  private key: Uint8Array | null | undefined;
  private hostConn: string | null | undefined;

  constructor(ctx: DurableObjectState, env: Env) {
    super(ctx, env);
    ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair("ping", "pong"));
  }

  private async getKey(): Promise<Uint8Array | null> {
    if (this.key === undefined) this.key = (await this.ctx.storage.get<Uint8Array>("key")) ?? null;
    return this.key;
  }

  private async getHostConn(): Promise<string | null> {
    if (this.hostConn === undefined) this.hostConn = (await this.ctx.storage.get<string>("hostConn")) ?? null;
    return this.hostConn;
  }

  private static async hostToken(key: Uint8Array): Promise<string> {
    return b64url(await hmac(key, "host"));
  }

  // Calls from the lobby.

  /** Makes the relay key and returns the host token, or null when this code already has a relay. */
  async setup(code: string): Promise<string | null> {
    if ((await this.getKey()) !== null) return null;
    const key = randomBytes(32);
    await this.ctx.storage.put({ key, code });
    await this.ctx.storage.setAlarm(this.now() + CHECK_EVERY_MS);
    this.key = key;
    return Relay.hostToken(key);
  }

  async verifyHost(token: string): Promise<boolean> {
    const key = await this.getKey();
    const given = fromB64url(token);
    return key !== null && given !== null && sameBytes(given, fromB64url(await Relay.hostToken(key))!);
  }

  /** A one-use ticket that lets one human manager connect within TICKET_SECONDS. */
  async mintTicket(): Promise<string | null> {
    const key = await this.getKey();
    if (key === null) return null;
    const now = this.now();
    const old = await this.ctx.storage.list<number>({ prefix: "n:" });
    const expired = [...old].filter(([, exp]) => exp <= now).map(([k]) => k);
    if (expired.length > 0) await this.ctx.storage.delete(expired);
    const expiry = now + TICKET_SECONDS * 1000;
    const nonce = b64url(randomBytes(12));
    return `${expiry}.${nonce}.${b64url(await hmac(key, `m|${expiry}|${nonce}`))}`;
  }

  /** Closes every connection and forgets the key. The game is no longer open. */
  async teardown(): Promise<void> {
    for (const ws of this.ctx.getWebSockets("m")) safeClose(ws, CLOSE_HOST_LEFT, "host_left");
    for (const ws of this.ctx.getWebSockets("h")) safeClose(ws, CLOSE_GAME_CLOSED, "game_closed");
    await this.ctx.storage.deleteAlarm();
    await this.ctx.storage.deleteAll();
    this.key = null;
    this.hostConn = null;
  }

  /** Every hour: forget the relay if no host is connected and the lobby does not list the game. */
  async alarm(): Promise<void> {
    const code = await this.ctx.storage.get<string>("code");
    let keep = (await this.currentHost()) !== null;
    if (!keep && code !== undefined) {
      try {
        keep = await this.env.LOBBY.get(this.env.LOBBY.idFromName("lobby")).isOpen(code);
      } catch {
        keep = true; // the lobby is not available; ask again next hour
      }
    }
    if (keep) await this.ctx.storage.setAlarm(this.now() + CHECK_EVERY_MS);
    else await this.teardown();
  }

  // Connections.

  private async currentHost(): Promise<WebSocket | null> {
    const id = await this.getHostConn();
    if (id === null) return null;
    for (const ws of this.ctx.getWebSockets("h")) {
      const a = ws.deserializeAttachment() as Attachment;
      if (a.r === "h" && a.id === id && ws.readyState === WebSocket.OPEN) return ws;
    }
    return null;
  }

  private openManagers(): WebSocket[] {
    return this.ctx.getWebSockets("m").filter((ws) => ws.readyState === WebSocket.OPEN);
  }

  /** Channels in use. A closing socket keeps its channel until its close event tells the host. */
  private usedChannels(): number[] {
    return this.ctx
      .getWebSockets("m")
      .filter((ws) => ws.readyState !== WebSocket.CLOSED)
      .map((ws) => (ws.deserializeAttachment() as Attachment & { r: "m" }).ch);
  }

  private async checkTicket(ticket: string, key: Uint8Array): Promise<string | null> {
    const parts = ticket.split(".");
    if (parts.length !== 3 || !/^\d{1,16}$/.test(parts[0])) return null;
    const [exp, nonce, sig] = parts;
    const expiry = Number(exp);
    if (expiry <= this.now() || expiry > this.now() + TICKET_SECONDS * 1000) return null;
    const given = fromB64url(sig);
    if (given === null || !sameBytes(given, await hmac(key, `m|${expiry}|${nonce}`))) return null;
    if ((await this.ctx.storage.get(`n:${nonce}`)) !== undefined) return null;
    return nonce;
  }

  async fetch(request: Request): Promise<Response> {
    if (this.seenHeaders) request.headers.forEach((v, k) => this.seenHeaders!.push(`${k}: ${v}`));
    if (request.headers.get("Upgrade")?.toLowerCase() !== "websocket") return fail("upgrade_required");
    const key = await this.getKey();
    if (key === null) return fail("unknown_game");
    const url = new URL(request.url);
    const role = url.searchParams.get("role");

    if (role === "host") {
      if (!(await this.verifyHost(url.searchParams.get("token") ?? ""))) return fail("not_host");
      const old = await this.currentHost();
      const id = b64url(randomBytes(12));
      await this.ctx.storage.put("hostConn", id);
      this.hostConn = id;
      if (old !== null) safeClose(old, CLOSE_REPLACED, "replaced");
      const pair = new WebSocketPair();
      this.ctx.acceptWebSocket(pair[1], ["h"]);
      pair[1].serializeAttachment({ r: "h", id } satisfies Attachment);
      // A new host connection learns the managers that are still connected.
      for (const ws of this.openManagers()) {
        pair[1].send(JSON.stringify({ t: "open", ch: (ws.deserializeAttachment() as Attachment & { r: "m" }).ch }));
      }
      return new Response(null, { status: 101, webSocket: pair[0] });
    }

    if (role === "manager") {
      const nonce = await this.checkTicket(url.searchParams.get("ticket") ?? "", key);
      if (nonce === null) return fail("bad_ticket");
      const host = await this.currentHost();
      if (host === null) return fail("host_not_connected");
      const used = new Set(this.usedChannels());
      let ch = 0;
      while (used.has(ch)) ch++;
      if (ch >= MAX_MANAGERS) return fail("relay_full");
      await this.ctx.storage.put(`n:${nonce}`, this.now() + TICKET_SECONDS * 1000);
      const pair = new WebSocketPair();
      this.ctx.acceptWebSocket(pair[1], ["m", `m${ch}`]);
      pair[1].serializeAttachment({ r: "m", ch } satisfies Attachment);
      host.send(JSON.stringify({ t: "open", ch }));
      return new Response(null, { status: 101, webSocket: pair[0] });
    }

    return fail("bad_request", { field: "role" });
  }

  async webSocketMessage(ws: WebSocket, message: string | ArrayBuffer): Promise<void> {
    this.handled++;
    const a = ws.deserializeAttachment() as Attachment;
    if (a.r === "m") {
      if (typeof message === "string") return safeClose(ws, 1003, "binary_only");
      if (message.byteLength > MAX_FRAME_BYTES) return safeClose(ws, 1009, "too_big");
      const host = await this.currentHost();
      if (host === null) return safeClose(ws, CLOSE_HOST_LEFT, "host_left");
      const out = new Uint8Array(message.byteLength + 1);
      out[0] = a.ch;
      out.set(new Uint8Array(message), 1);
      host.send(out);
      return;
    }

    if (a.id !== (await this.getHostConn())) return; // an old host connection that is closing
    if (typeof message === "string") {
      let msg: unknown;
      try {
        msg = JSON.parse(message);
      } catch {
        return;
      }
      const m = msg as { t?: unknown; ch?: unknown };
      if (m && m.t === "kick" && Number.isInteger(m.ch)) {
        for (const target of this.ctx.getWebSockets(`m${m.ch}`)) safeClose(target, CLOSE_KICKED, "kicked");
      }
      return;
    }
    if (message.byteLength > MAX_FRAME_BYTES + 1) return safeClose(ws, 1009, "too_big");
    if (message.byteLength < 1) return;
    const bytes = new Uint8Array(message);
    const ch = bytes[0];
    const payload = bytes.subarray(1);
    const targets = ch === ALL_MANAGERS ? this.openManagers() : ch < MAX_MANAGERS ? this.ctx.getWebSockets(`m${ch}`) : [];
    for (const t of targets) {
      try {
        t.send(payload);
      } catch {
        // The manager is leaving. Its close event tells the host.
      }
    }
  }

  async webSocketClose(ws: WebSocket, code: number, reason: string): Promise<void> {
    safeClose(ws, 1000, "");
    await this.left(ws, code === 1000);
  }

  async webSocketError(ws: WebSocket): Promise<void> {
    safeClose(ws, 1011, "error");
    await this.left(ws, false);
  }

  /** A connection closed. ended: a normal close (1000), which a host sends only when its game stops hosting. */
  private async left(ws: WebSocket, ended: boolean) {
    const a = ws.deserializeAttachment() as Attachment | null;
    if (a === null) return;
    if (a.r === "h") {
      if (a.id !== (await this.getHostConn())) return; // an old, replaced host connection
      await this.ctx.storage.delete("hostConn");
      this.hostConn = null;
      for (const m of this.ctx.getWebSockets("m")) safeClose(m, CLOSE_HOST_LEFT, "host_left");
      if (ended) {
        const code = await this.ctx.storage.get<string>("code");
        if (code !== undefined) {
          try {
            await this.env.LOBBY.get(this.env.LOBBY.idFromName("lobby")).hostLeft(code);
          } catch {
            // The lobby is not available: it drops the game when its heartbeats stop, as before.
          }
        }
        await this.teardown();
      }
      return;
    }
    const host = await this.currentHost();
    if (host !== null) {
      try {
        host.send(JSON.stringify({ t: "close", ch: a.ch }));
      } catch {
        // The host is leaving too.
      }
    }
  }
}

function safeClose(ws: WebSocket, code: number, reason: string) {
  try {
    ws.close(code, reason);
  } catch {
    // Already closed.
  }
}
