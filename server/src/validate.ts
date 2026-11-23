// Field checks for the lobby API. A failed check names the field, so the game can show it.
import { nameAllowed } from "./filter";

export class FieldError extends Error {
  constructor(
    readonly field: string,
    readonly code: "bad_request" | "name_not_allowed" = "bad_request",
  ) {
    super(field);
  }
}

/** Printable Latin-1 only, so the game's text code can draw every character. */
const LATIN1_TEXT = /^[\x20-\x7e\xa0-\xff]+$/;

export const MAX_MANAGERS = 16;
export const MAX_PING_MS = 9999;

export interface PublicInfo {
  name: string;
  hostName: string;
  version: string;
  database: string;
  season: number;
  day: string;
  humanClubs: string[];
  /** The clubs of humanClubs whose human manager is Away: their connection dropped, and the club is on holiday. */
  awayClubs: string[];
  freeSeats: number;
  managersOnline: number;
  /** The highest ping of the host's human managers in ms, 0 when none is known (0.7.0 slice 7). */
  pingMs: number;
}

export interface RegisterInput extends PublicInfo {
  gameId: string;
  password: string | null;
  code: string | null;
  hostToken: string | null;
}

export type HeartbeatInput = Partial<Pick<PublicInfo, "season" | "day" | "humanClubs" | "awayClubs" | "freeSeats" | "managersOnline" | "pingMs">> & {
  /** A new game password, or null to remove it. Absent: no change. */
  password?: string | null;
};

export interface JoinInput {
  version: string;
  name: string;
  password: string | null;
}

export const CODE_RE = /^[23456789ABCDEFGHJKMNPQRSTUVWXYZ]{8}$/;

function obj(body: unknown): Record<string, unknown> {
  if (typeof body !== "object" || body === null || Array.isArray(body)) throw new FieldError("body");
  return body as Record<string, unknown>;
}

function text(o: Record<string, unknown>, field: string, max: number): string {
  const v = o[field];
  if (typeof v !== "string") throw new FieldError(field);
  const t = v.trim();
  if (t.length === 0 || t.length > max || !LATIN1_TEXT.test(t)) throw new FieldError(field);
  return t;
}

/** Text that other players see: the word filter applies. */
function playerName(o: Record<string, unknown>, field: string): string {
  const t = text(o, field, 40);
  if (!nameAllowed(t)) throw new FieldError(field, "name_not_allowed");
  return t;
}

function int(o: Record<string, unknown>, field: string, min: number, max: number): number {
  const v = o[field];
  if (typeof v !== "number" || !Number.isInteger(v) || v < min || v > max) throw new FieldError(field);
  return v;
}

function version(o: Record<string, unknown>): string {
  const v = o.version;
  if (typeof v !== "string" || !/^[0-9A-Za-z.+-]{1,24}$/.test(v)) throw new FieldError("version");
  return v;
}

function day(o: Record<string, unknown>): string {
  const v = o.day;
  if (typeof v !== "string" || !/^\d{4}-(0[1-9]|1[0-2])-(0[1-9]|[12]\d|3[01])$/.test(v)) throw new FieldError("day");
  return v;
}

function clubs(o: Record<string, unknown>, field: "humanClubs" | "awayClubs" = "humanClubs"): string[] {
  const v = o[field];
  if (!Array.isArray(v) || v.length > MAX_MANAGERS) throw new FieldError(field);
  return v.map((_, i) => {
    try {
      return text(v as unknown as Record<string, unknown>, String(i), 40);
    } catch {
      throw new FieldError(field);
    }
  }).map((club) => {
    if (!nameAllowed(club)) throw new FieldError(field, "name_not_allowed");
    return club;
  });
}

function password(o: Record<string, unknown>): string | null {
  const v = o.password;
  if (v === undefined || v === null || v === "") return null;
  if (typeof v !== "string" || v.length > 64) throw new FieldError("password");
  return v;
}

function optional<T>(o: Record<string, unknown>, field: string, read: () => T): T | null {
  return o[field] === undefined || o[field] === null ? null : read();
}

export function parseRegister(body: unknown): RegisterInput {
  const o = obj(body);
  const gameId = o.gameId;
  if (typeof gameId !== "string" || !/^[A-Za-z0-9._:-]{8,128}$/.test(gameId)) throw new FieldError("gameId");
  return {
    gameId,
    name: playerName(o, "name"),
    hostName: playerName(o, "hostName"),
    version: version(o),
    database: playerName(o, "database"),
    season: int(o, "season", 1900, 2200),
    day: day(o),
    humanClubs: clubs(o),
    awayClubs: o.awayClubs === undefined ? [] : clubs(o, "awayClubs"),
    freeSeats: int(o, "freeSeats", 0, MAX_MANAGERS),
    managersOnline: int(o, "managersOnline", 0, MAX_MANAGERS),
    pingMs: o.pingMs === undefined ? 0 : int(o, "pingMs", 0, MAX_PING_MS),
    password: password(o),
    code: optional(o, "code", () => {
      if (typeof o.code !== "string" || !CODE_RE.test(o.code)) throw new FieldError("code");
      return o.code;
    }),
    hostToken: optional(o, "hostToken", () => {
      if (typeof o.hostToken !== "string" || o.hostToken.length > 128) throw new FieldError("hostToken");
      return o.hostToken;
    }),
  };
}

export function parseHeartbeat(body: unknown): HeartbeatInput {
  if (body === null || body === undefined) return {};
  const o = obj(body);
  const out: HeartbeatInput = {};
  if (o.season !== undefined) out.season = int(o, "season", 1900, 2200);
  if (o.day !== undefined) out.day = day(o);
  if (o.humanClubs !== undefined) out.humanClubs = clubs(o);
  if (o.awayClubs !== undefined) out.awayClubs = clubs(o, "awayClubs");
  if (o.freeSeats !== undefined) out.freeSeats = int(o, "freeSeats", 0, MAX_MANAGERS);
  if (o.managersOnline !== undefined) out.managersOnline = int(o, "managersOnline", 0, MAX_MANAGERS);
  if (o.pingMs !== undefined) out.pingMs = int(o, "pingMs", 0, MAX_PING_MS);
  if (o.password !== undefined) out.password = password(o);
  return out;
}

export function parseJoin(body: unknown): JoinInput {
  const o = obj(body);
  return { version: version(o), name: playerName(o, "name"), password: password(o) };
}
