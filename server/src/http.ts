// Every answer is small JSON with "ok" or a known "error" code, so the game can always parse it.
// Answers never hold an internet address.

export type ErrorCode =
  | "bad_request"
  | "name_not_allowed"
  | "not_found"
  | "method_not_allowed"
  | "unknown_game"
  | "not_host"
  | "too_many_games"
  | "game_already_open"
  | "version_mismatch"
  | "wrong_password"
  | "too_many_attempts"
  | "bad_ticket"
  | "relay_full"
  | "host_not_connected"
  | "upgrade_required"
  | "lobby_full"
  | "full_today"
  | "unavailable";

/** English text the game can show. The game should prefer its own text for each code. */
const MESSAGES: Record<ErrorCode, string> = {
  bad_request: "The request was not valid",
  name_not_allowed: "That name is not allowed",
  not_found: "Not found",
  method_not_allowed: "Not allowed",
  unknown_game: "The game is no longer open",
  not_host: "Only the host can do this",
  too_many_games: "Your network already has 2 open online games",
  game_already_open: "This game is already open online",
  version_mismatch: "The host plays a different version of Bosman Edition",
  wrong_password: "Wrong password",
  too_many_attempts: "Too many wrong passwords, try again in a minute",
  bad_ticket: "The join step expired, join again",
  relay_full: "The game has 16 human managers",
  host_not_connected: "The host is not connected",
  upgrade_required: "This address needs a WebSocket connection",
  lobby_full: "The online game list is full, try again later",
  full_today: "Online games are full today, try again later",
  unavailable: "Online games are not available now, try again later",
};

const STATUS: Record<ErrorCode, number> = {
  bad_request: 400,
  name_not_allowed: 400,
  not_found: 404,
  method_not_allowed: 405,
  unknown_game: 404,
  not_host: 401,
  too_many_games: 429,
  game_already_open: 409,
  version_mismatch: 409,
  wrong_password: 403,
  too_many_attempts: 429,
  bad_ticket: 401,
  relay_full: 409,
  host_not_connected: 409,
  upgrade_required: 426,
  lobby_full: 503,
  full_today: 503,
  unavailable: 503,
};

export function json(data: unknown, status = 200, headers: Record<string, string> = {}): Response {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "Content-Type": "application/json; charset=utf-8", "X-Bosman-Api": "1", "Cache-Control": "no-store", ...headers },
  });
}

export function fail(error: ErrorCode, extra: Record<string, unknown> = {}, headers: Record<string, string> = {}): Response {
  return json({ error, message: MESSAGES[error], ...extra }, STATUS[error], headers);
}

/** A lobby answer that crosses from the Durable Object to the Worker. */
export type Result =
  | { ok: true; status?: number; body: Record<string, unknown> }
  | { ok: false; error: ErrorCode; extra?: Record<string, unknown> };

export function toResponse(r: Result): Response {
  return r.ok ? json(r.body, r.status ?? 200) : fail(r.error, r.extra);
}

export function err(error: ErrorCode, extra?: Record<string, unknown>): Result {
  return { ok: false, error, extra };
}
