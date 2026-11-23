import { abortAllDurableObjects } from "cloudflare:test";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { DOWNLOAD_URL, escapeHtml, joinCode, joinPage } from "../src/join";
import { err } from "../src/http";
import { call, register, withLobby } from "./helpers";

const HOST_IP = "203.0.113.91";
const VISITOR_IP = "2001:db8::77";

const logged: string[] = [];
beforeEach(async () => {
  await abortAllDurableObjects();
  logged.length = 0;
  for (const level of ["log", "info", "warn", "error", "debug", "trace"] as const) {
    vi.spyOn(console, level).mockImplementation((...args: unknown[]) => {
      logged.push(args.map(String).join(" "));
    });
  }
});
afterEach(async () => {
  vi.restoreAllMocks();
  await abortAllDurableObjects();
});

async function page(path: string, ip = VISITOR_IP, method = "GET") {
  const res = await call(path, { ip, method });
  return { res, html: await res.text() };
}

describe("the invite link page", () => {
  it("shows the game, the button, the code and the download link", async () => {
    const g = await register({ name: "Sunday League", hostName: "Max", password: "pw" }, HOST_IP);
    const { res, html } = await page(`/join/${g.code}`);
    expect(res.status).toBe(200);
    expect(res.headers.get("Content-Type")).toBe("text/html; charset=utf-8");
    expect(html).toContain("<h1>Join Sunday League</h1>");
    expect(html).toContain(`href="cm0102://join/${g.code}">Open in Bosman Edition</a>`);
    expect(html).toContain(`<span class="code">${g.code}</span>`);
    expect(html).toContain(`href="${DOWNLOAD_URL}"`);
    expect(html).toContain("<dd>2001/02</dd>");
    expect(html).toContain("<dd>Arsenal, Scunthorpe United</dd>");
    expect(html).toContain("This game has a password");
    expect(html).toContain("writes them to no log");
    expect(html).not.toContain("pw<");
  });

  it("needs no game: no script, no outside file, no cookie", async () => {
    const g = await register({}, HOST_IP);
    const { res, html } = await page(`/join/${g.code}`);
    expect(html).not.toMatch(/<script|<img|<link|<iframe|<form|src=/i);
    expect(res.headers.get("Set-Cookie")).toBeNull();
    expect(res.headers.get("Content-Security-Policy")).toContain("default-src 'none'");
    expect(res.headers.get("Referrer-Policy")).toBe("no-referrer");
    expect(res.headers.get("Cache-Control")).toBe("no-store");
    // The only links are the game's own link and the download page.
    const links = [...html.matchAll(/href="([^"]*)"/g)].map((m) => m[1]);
    expect(links).toEqual([`cm0102://join/${g.code}`, DOWNLOAD_URL]);
    expect(html).not.toContain("This game has a password");
  });

  it("takes a code in small letters", async () => {
    const g = await register({}, HOST_IP);
    const { res, html } = await page(`/join/${g.code.toLowerCase()}`);
    expect(res.status).toBe(200);
    expect(html).toContain(`cm0102://join/${g.code}"`);
  });

  it("escapes every game text", async () => {
    // The word filter allows these characters, so the page must escape them.
    const g = await register({ name: `<b>"Bad" & 'co'</b>`, hostName: "<i>x</i>", humanClubs: ["A&B"] }, HOST_IP);
    const { html } = await page(`/join/${g.code}`);
    expect(html).not.toContain("<b>");
    expect(html).not.toContain("<i>x");
    expect(html).toContain("&#60;b&#62;&#34;Bad&#34; &#38; &#39;co&#39;&#60;/b&#62;");
    expect(html).toContain("<dd>A&#38;B</dd>");
  });

  it("says when the game is not open, with the download link", async () => {
    for (const path of ["/join/ABCDEFGH", "/join/22222222", "/join/not-a-code", "/join", "/join/"]) {
      const { res, html } = await page(path);
      expect(res.status, path).toBe(404);
      expect(res.headers.get("Content-Type")).toBe("text/html; charset=utf-8");
      expect(html).toContain("This game is not open");
      expect(html).toContain(DOWNLOAD_URL);
      expect(html).not.toContain("cm0102://");
    }
  });

  it("answers HEAD with no body, and refuses other methods", async () => {
    const g = await register({}, HOST_IP);
    const head = await page(`/join/${g.code}`, VISITOR_IP, "HEAD");
    expect(head.res.status).toBe(200);
    expect(head.html).toBe("");
    const post = await call(`/join/${g.code}`, { method: "POST", ip: VISITOR_IP });
    expect(post.status).toBe(405);
  });

  it("keeps the API answers for other paths", async () => {
    const res = await call("/join/ABCDEFGH/extra", { ip: VISITOR_IP });
    expect(res.status).toBe(404);
    expect(res.headers.get("X-Bosman-Api")).toBe("1");
  });

  it("logs nothing and keeps no address of a visitor (Q26)", async () => {
    const g = await register({}, HOST_IP);
    const storedRows = () =>
      withLobby(async (_inst, state) => JSON.stringify([...(await state.storage.list()).entries()]));
    // The lobby stores each game (it survives an eviction), so the check is that a visit adds nothing.
    const before = await storedRows();
    const answers: string[] = [];
    for (const path of [`/join/${g.code}`, "/join/ABCDEFGH", "/join/x"]) answers.push((await page(path)).html);
    const memory = await withLobby((inst) => inst.debugDump());
    const after = await storedRows();
    expect(logged).toEqual([]);
    expect(after).toBe(before);
    for (const ip of [HOST_IP, VISITOR_IP]) {
      expect(memory).not.toContain(ip);
      expect(after).not.toContain(ip);
      for (const a of answers) expect(a).not.toContain(ip);
    }
  });
});

describe("join page helpers", () => {
  it("reads only lobby codes", () => {
    expect(joinCode("abcdefgh")).toBe("ABCDEFGH");
    expect(joinCode("ABCDEFG")).toBeNull();
    expect(joinCode("ABCDEFGHJ")).toBeNull();
    expect(joinCode("ABCDEFG1")).toBeNull(); // 1 is not in the alphabet
    expect(joinCode(undefined)).toBeNull();
  });

  it("gives a page with Retry-After when the free plan is used up", async () => {
    const full = await joinPage("ABCDEFGH", async () => err("full_today"), () => 3600);
    expect(full.status).toBe(503);
    expect(full.headers.get("Retry-After")).toBe("3600");
    expect(await full.text()).toContain("Online games are full today, try again later");
    const down = await joinPage("ABCDEFGH", async () => err("unavailable"), () => 3600);
    expect(down.status).toBe(503);
    expect(down.headers.get("Retry-After")).toBeNull();
    expect(await down.text()).toContain("Online games are not available now");
  });

  it("escapes the five HTML characters", () => {
    expect(escapeHtml(`<>&"'`)).toBe("&#60;&#62;&#38;&#34;&#39;");
  });
});
