// Bosman Edition lobby and relay (0.7.0, #45, slice 3). The API contract is in README.md.
// This Worker logs nothing, so no log line can hold an internet address (Q26).
import { err, fail, json, toResponse, type Result } from "./http";
import { cachedAnswer, forgetAnswers, refused, withinLimits } from "./flood";
import { joinPage } from "./join";
import { CODE_RE, FieldError, parseHeartbeat, parseJoin, parseRegister } from "./validate";

export { Lobby } from "./lobby";
export { Relay } from "./relay";

const MAX_BODY_BYTES = 4096;

class BadBody extends Error {}

async function readBody(request: Request, required: boolean): Promise<unknown> {
  const length = Number(request.headers.get("Content-Length") ?? "0");
  if (length > MAX_BODY_BYTES) throw new BadBody();
  const text = await request.text();
  if (text.length > MAX_BODY_BYTES) throw new BadBody();
  if (text.trim() === "") {
    if (required) throw new BadBody();
    return null;
  }
  try {
    return JSON.parse(text);
  } catch {
    throw new BadBody();
  }
}

function address(request: Request): string {
  return request.headers.get("CF-Connecting-IP") ?? "unknown";
}

function bearer(request: Request): string | null {
  const h = request.headers.get("Authorization");
  return h !== null && h.startsWith("Bearer ") ? h.slice(7) : null;
}

/** Seconds until 00:00 UTC, when the free plan's daily limits reset. */
export function secondsToMidnightUtc(now = Date.now()): number {
  const d = new Date(now);
  const midnight = Date.UTC(d.getUTCFullYear(), d.getUTCMonth(), d.getUTCDate() + 1);
  return Math.ceil((midnight - now) / 1000);
}

/**
 * True for a Durable Object error that comes from a used-up free plan limit.
 * Cloudflare does not document the exact text, so this matches the words it uses in its limit
 * errors. CPU and memory errors ("exceeded CPU time", "exceeded memory") do not match.
 */
export function isLimitError(e: unknown): boolean {
  const msg = e instanceof Error ? e.message : String(e);
  return /daily limit|daily request limit|free tier|quota|exceeded (the )?allowed|exceeded its daily/i.test(msg);
}

function lobby(env: Env) {
  return env.LOBBY.get(env.LOBBY.idFromName("lobby"));
}

async function route(request: Request, env: Env): Promise<Response> {
  const url = new URL(request.url);
  const parts = url.pathname.split("/").filter((p) => p !== "");
  const method = request.method;

  // Layer 1 against floods: the limit for each address comes before any other work (flood.ts).
  if (!(await withinLimits(request, env, parts))) return refused(parts[0] === "join");

  if (parts[0] === "join" && parts.length <= 2) {
    // The invite link page (slice 8). A lobby failure gives a page too, never JSON.
    if (method !== "GET" && method !== "HEAD") return fail("method_not_allowed");
    const res = await joinPage(parts[1], async (code) => {
      try {
        return await cachedAnswer(`get:${code}`, () => lobby(env).get(code));
      } catch (e) {
        return err(isLimitError(e) ? "full_today" : "unavailable");
      }
    }, () => secondsToMidnightUtc());
    return method === "HEAD" ? new Response(null, { status: res.status, headers: res.headers }) : res;
  }
  if (parts[0] !== "v1") return fail("not_found");
  if (parts.length === 2 && parts[1] === "health") {
    return method === "GET" ? json({ ok: true, api: 1 }) : fail("method_not_allowed");
  }
  if (parts[1] !== "games") return fail("not_found");

  if (parts.length === 2) {
    if (method === "GET") return toResponse(await cachedAnswer("list", () => lobby(env).list()));
    if (method === "POST") {
      const input = parseRegister(await readBody(request, true));
      const r = (await lobby(env).register(input, address(request))) as Result;
      if (r.ok) forgetAnswers(typeof r.body.code === "string" ? r.body.code : null);
      return toResponse(r);
    }
    return fail("method_not_allowed");
  }

  const code = parts[2];
  if (!CODE_RE.test(code) || parts.length > 4) return fail("unknown_game");

  if (parts.length === 3) {
    if (method === "GET") return toResponse(await cachedAnswer(`get:${code}`, () => lobby(env).get(code)));
    if (method === "DELETE") {
      const r = (await lobby(env).close(code, bearer(request))) as Result;
      if (r.ok) forgetAnswers(code);
      return toResponse(r);
    }
    return fail("method_not_allowed");
  }

  switch (parts[3]) {
    case "heartbeat":
      if (method !== "POST") return fail("method_not_allowed");
      const beat = (await lobby(env).heartbeat(code, bearer(request), parseHeartbeat(await readBody(request, false)), address(request))) as Result;
      if (beat.ok) forgetAnswers(code);
      return toResponse(beat);
    case "join":
      if (method !== "POST") return fail("method_not_allowed");
      return toResponse(await lobby(env).join(code, parseJoin(await readBody(request, true)), address(request)));
    case "relay": {
      if (method !== "GET") return fail("method_not_allowed");
      if (request.headers.get("Upgrade")?.toLowerCase() !== "websocket") return fail("upgrade_required");
      // Pass only what the relay needs. The relay never sees the internet address.
      const clean = new Request(`https://relay/?${url.searchParams}`, { headers: { Upgrade: "websocket" } });
      return env.RELAY.get(env.RELAY.idFromName(code)).fetch(clean);
    }
    default:
      return fail("not_found");
  }
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    try {
      return await route(request, env);
    } catch (e) {
      if (e instanceof BadBody) return fail("bad_request", { field: "body" });
      if (e instanceof FieldError) return fail(e.code, { field: e.field });
      if (isLimitError(e)) {
        const s = secondsToMidnightUtc();
        return fail("full_today", { retryAfterSeconds: s }, { "Retry-After": String(s) });
      }
      return fail("unavailable");
    }
  },
} satisfies ExportedHandler<Env>;
