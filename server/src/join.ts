// The invite link page (0.7.0, #45, slice 8): GET /join/<code>.
// A friend opens it in a browser, with or without the game. The page shows the game, a button that opens
// cm0102://join/<code> in Bosman Edition, the game code and a download link. It has no script, no cookie and no
// outside file, and the Worker logs nothing (Q26).
import type { Result } from "./http";
import { CODE_RE } from "./validate";

export const DOWNLOAD_URL = "https://github.com/maxevans60/cm0102-bosman-edition/releases/latest";

const HEADERS: Record<string, string> = {
  "Content-Type": "text/html; charset=utf-8",
  "Cache-Control": "no-store",
  "Content-Security-Policy":
    "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'",
  "Referrer-Policy": "no-referrer",
  "X-Content-Type-Options": "nosniff",
};

/** The code from the path, in capitals (a chat program may change its case), or null when it is not one. */
export function joinCode(part: string | undefined): string | null {
  if (part === undefined) return null;
  const code = part.toUpperCase();
  return CODE_RE.test(code) ? code : null;
}

export function escapeHtml(s: string): string {
  return s.replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
}

type Game = {
  code: string;
  name: string;
  hostName: string;
  version: string;
  database: string;
  season: number;
  day: string;
  humanClubs: string[];
  freeSeats: number;
  managersOnline: number;
  password: boolean;
};

const STYLE = `
:root{color-scheme:light dark;--bg:#f4f1e8;--fg:#1d1f1c;--muted:#5b5f57;--card:#fffdf7;--line:#d9d3c3;--accent:#1f5f3a;--accent-fg:#fff}
@media (prefers-color-scheme:dark){:root{--bg:#141713;--fg:#ecebe4;--muted:#a3a79c;--card:#1d211c;--line:#343a32;--accent:#5fb07c;--accent-fg:#0d120e}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif}
main{max-width:34rem;margin:0 auto;padding:2.5rem 16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:1.5rem}
h1{font-size:1.5rem;line-height:1.25;margin:0 0 .25rem}
.sub{color:var(--muted);margin:0 0 1.25rem}
dl{display:grid;grid-template-columns:auto 1fr;gap:.25rem 1rem;margin:0 0 1.5rem}
dt{color:var(--muted)}
dd{margin:0;overflow-wrap:anywhere}
.open{display:block;text-align:center;background:var(--accent);color:var(--accent-fg);text-decoration:none;font-weight:600;padding:.85rem 1rem;border-radius:8px}
.code{font:600 1.25rem/1 ui-monospace,Menlo,Consolas,monospace;letter-spacing:.15em}
p{margin:1rem 0 0}
small{display:block;color:var(--muted);margin-top:1.5rem}
a{color:var(--accent)}`;

function page(title: string, description: string, body: string): string {
  return `<!doctype html>
<html lang="en-GB">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="robots" content="noindex">
<title>${escapeHtml(title)}</title>
<meta property="og:site_name" content="CM 01/02: Bosman Edition">
<meta property="og:title" content="${escapeHtml(title)}">
<meta property="og:description" content="${escapeHtml(description)}">
<style>${STYLE}</style>
</head>
<body>
<main>
${body}
<small>The lobby keeps internet addresses in memory only while a game is open, and writes them to no log.</small>
</main>
</body>
</html>
`;
}

const DOWNLOAD = `<p>No Bosman Edition yet? It is free for Mac, Windows and Linux. <a href="${DOWNLOAD_URL}">Download it from GitHub</a>, then come back to this page.</p>`;

function gamePage(g: Game): string {
  const name = escapeHtml(g.name);
  const code = escapeHtml(g.code);
  const rows: [string, string][] = [
    ["Host", g.hostName],
    ["Season", `${g.season}/${String((g.season + 1) % 100).padStart(2, "0")}`],
    ["Game date", g.day],
    ["Database", g.database],
    ["Managers online", String(g.managersOnline)],
    ["Places left", String(g.freeSeats)],
    ["Version", `Bosman Edition ${g.version}`],
  ];
  if (g.humanClubs.length > 0) rows.splice(4, 0, ["Human clubs", g.humanClubs.join(", ")]);
  const dl = rows.map(([k, v]) => `<dt>${k}</dt><dd>${escapeHtml(v)}</dd>`).join("");
  const password = g.password ? `<p>This game has a password. Ask the host for it.</p>` : "";
  const body = `<div class="card">
<h1>Join ${name}</h1>
<p class="sub">A network game of CM 01/02: Bosman Edition.</p>
<dl>${dl}</dl>
<a class="open" href="cm0102://join/${code}">Open in Bosman Edition</a>
${password}
<p>If the button does nothing, start Bosman Edition ${escapeHtml(g.version)}, click Network Play, then Join Network Game, and pick ${name}. The game code is <span class="code">${code}</span>.</p>
${DOWNLOAD}
</div>`;
  return page(`Join ${g.name}`, `A network game of CM 01/02: Bosman Edition, hosted by ${g.hostName}.`, body);
}

function closedPage(text: string): string {
  const body = `<div class="card">
<h1>${escapeHtml(text)}</h1>
<p class="sub">Ask the host for a new invite link.</p>
${DOWNLOAD}
</div>`;
  return page(text, text, body);
}

/** The page for GET /join/<code>. get asks the lobby for one game. */
export async function joinPage(
  part: string | undefined,
  get: (code: string) => Promise<Result>,
  midnight: () => number,         /* seconds until 00:00 UTC, for Retry-After */
): Promise<Response> {
  const code = joinCode(part);
  if (code === null) return new Response(closedPage("This game is not open"), { status: 404, headers: HEADERS });
  const r = await get(code);
  if (r.ok) return new Response(gamePage(r.body.game as Game), { status: 200, headers: HEADERS });
  if (r.error === "unknown_game") {
    return new Response(closedPage("This game is not open"), { status: 404, headers: HEADERS });
  }
  if (r.error === "full_today") {
    const headers = { ...HEADERS, "Retry-After": String(midnight()) };
    return new Response(closedPage("Online games are full today, try again later"), { status: 503, headers });
  }
  return new Response(closedPage("Online games are not available now, try again later"), { status: 503, headers: HEADERS });
}
