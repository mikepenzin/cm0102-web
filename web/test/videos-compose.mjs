// The how-to videos as HyperFrames projects (web/README.md, "How-to videos"; the skill in .claude/skills/hyperframes):
// a title card, the screen capture of the page in a frame, a caption under it for each step (and a gentle zoom on what
// the step points at), and an end card. make-videos.mjs records the capture and the steps' times; this writes the
// project, and render() makes the MP4 with the HyperFrames CLI (telemetry off).
//
// Music (optional): a track under the whole film from its start, faded in over MUSIC_FADE_IN s and out over the end
// card's last MUSIC_FADE_OUT s, at MUSIC_VOLUME (a Suno track mastered at -13.5 LUFS comes to about -18 LUFS: under a
// tutorial with no voice, background, not a soundtrack).
// 1920x1080 at 30 fps. The capture (1280x800, the page's viewport) sits 1:1 in a frame at the top; the captions sit
// under it, inside the title-safe box (the frame inside the action-safe box).
import { execFileSync } from 'node:child_process';
import { copyFileSync, mkdirSync, writeFileSync } from 'node:fs';
import { extname, join, resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const LAUNCHER = resolve(HERE, '../../.claude/skills/hyperframes/scripts/plugin-cli.mjs');
export const CLI_VERSION = '0.9.70';

const W = 1920, H = 1080, VW = 1280, VH = 800;
const FRAME = { left: (W - VW) / 2, top: 60 };
const INTRO = 3.8, OUTRO = 3.2, ENTER = 0.45;
export const MUSIC_VOLUME = 0.6, MUSIC_FADE_IN = 0.5, MUSIC_FADE_OUT = 4;
const COLORS = { ground: '#0b0d10', panel: '#151a20', line: '#29313b', text: '#e8ebef', muted: '#97a2af', accent: '#35b673' };

const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const r2 = (n) => Math.round(n * 1000) / 1000;

// The zoom onto a step's target: a small one only (a button, a field); a wide one (a list) stays whole.
export function focusOf(box) {
  if (!box) return null;
  const s = Math.min(1.45, (VW * 0.55) / box.w, (VH * 0.55) / box.h);
  if (s < 1.15) return null;
  const cx = box.x + box.w / 2, cy = box.y + box.h / 2;
  const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
  return { scale: r2(s), x: r2(clamp(VW / 2 - cx * s, VW - VW * s, 0)), y: r2(clamp(VH / 2 - cy * s, VH - VH * s, 0)) };
}

// The steps between the title (the log's first caption) and the end (its last), on the capture's clock from the cut:
// each held to the next one's start when the gap is under 5 s (typing, a click), so the words do not blink out.
export function plan({ captions }) {
  const [title, ...rest] = captions;
  const end = rest.pop();
  const from = title.end, to = end.start;
  const steps = rest.map((c) => ({ ...c, start: r2(c.start - from), end: r2(c.end - from) }));
  steps.forEach((c, i) => {
    const next = steps[i + 1]?.start ?? r2(to - from);
    c.end = r2(next - c.end < 5 ? next - 0.05 : c.end + 1.2);
  });
  return { title, end, mediaStart: r2(from), length: r2(to - from), steps };
}

const page = (body, script = '') => `<!doctype html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=${W}, height=${H}">
<script src="https://cdn.jsdelivr.net/npm/gsap@3.15.0/dist/gsap.min.js"></script>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  html, body { width: ${W}px; height: ${H}px; overflow: hidden; background: ${COLORS.ground}; }
  #bg { position: absolute; inset: 0; background: radial-gradient(ellipse 70% 60% at 50% 38%, #142029 0%, ${COLORS.ground} 72%); }
</style>
</head>
<body>
${body}
<script>
${script}
</script>
</body>
</html>
`;

function intro({ title, sub }) {
  return `<template>
  <style>
    #root { position: absolute; inset: 0; display: grid; place-items: center; font-family: Inter, sans-serif; text-align: center; }
    #intro-box { display: flex; flex-direction: column; align-items: center; gap: 22px; }
    #intro-label { color: ${COLORS.accent}; font-size: 28px; font-weight: 700; letter-spacing: .14em; text-transform: uppercase; }
    #intro-title { color: ${COLORS.text}; font-size: 84px; font-weight: 900; letter-spacing: -.025em; line-height: 1.05; max-width: 1500px; }
    #intro-sub { color: ${COLORS.muted}; font-size: 34px; font-weight: 400; }
    #intro-rule { width: 96px; height: 4px; border-radius: 2px; background: ${COLORS.accent}; }
  </style>
  <div id="root" data-composition-id="intro" data-width="${W}" data-height="${H}">
    <div id="intro-box">
      <div id="intro-label">CM 01/02 in the Browser</div>
      <h1 id="intro-title" data-var-text="title">${esc(title)}</h1>
      <div id="intro-rule"></div>
      <div id="intro-sub" data-var-text="sub">${esc(sub)}</div>
    </div>
  </div>
  <script>
    const tl = gsap.timeline({ paused: true });
    // The title is on the first frame (the thumbnail a player sees); the rule and the line under it come in.
    tl.fromTo('#intro-title', { scale: 1.02 }, { scale: 1, duration: 1.2, ease: 'power2.out' }, 0);
    tl.fromTo('#intro-rule', { scaleX: 0 }, { scaleX: 1, duration: 0.5, ease: 'power2.out' }, 0.25);
    tl.fromTo('#intro-sub', { opacity: 0, y: 12 }, { opacity: 1, y: 0, duration: 0.5, ease: 'power3.out' }, 0.4);
    tl.to('#intro-box', { opacity: 0, y: -10, duration: 0.3, ease: 'power2.in' }, ${INTRO - 0.35});
    window.__timelines['intro'] = tl;
  </script>
</template>
`;
}

function screen({ mediaStart, length, steps }) {
  const moves = [];
  let zoomed = false;
  steps.forEach((c, i) => {
    const f = focusOf(c.focus);
    if (f) {
      moves.push(`tl.to('#cam', { scale: ${f.scale}, x: ${f.x}, y: ${f.y}, duration: 0.7, ease: 'power2.inOut' }, ${r2(Math.max(0, c.start - 0.1))});`);
      zoomed = true;
    }
    const nextZooms = Boolean(focusOf(steps[i + 1]?.focus));
    if (zoomed && !nextZooms) {
      moves.push(`tl.to('#cam', { scale: 1, x: 0, y: 0, duration: 0.6, ease: 'power2.inOut' }, ${r2(c.end - 0.1)});`);
      zoomed = false;
    }
  });
  return `<template>
  <style>
    #root { position: absolute; inset: 0; }
    #frame { position: absolute; left: ${FRAME.left}px; top: ${FRAME.top}px; width: ${VW}px; height: ${VH}px; overflow: hidden;
      border-radius: 14px; border: 1px solid ${COLORS.line}; box-shadow: 0 30px 80px rgba(0, 0, 0, .55), 0 0 0 6px rgba(255, 255, 255, .02); background: #000; }
    #cam { position: absolute; left: 0; top: 0; width: ${VW}px; height: ${VH}px; transform-origin: 0 0; }
    #capture { position: absolute; left: 0; top: 0; width: ${VW}px; height: ${VH}px; object-fit: contain; }
  </style>
  <div id="root" data-composition-id="screen" data-width="${W}" data-height="${H}">
    <div id="frame">
      <div id="cam" data-layout-allow-overflow>
        <video id="capture" class="clip" src="assets/capture.mp4" data-start="0" data-duration="${length}" data-media-start="${mediaStart}" data-track-index="0" muted playsinline></video>
      </div>
    </div>
  </div>
  <script>
    const tl = gsap.timeline({ paused: true });
    tl.fromTo('#frame', { opacity: 0, scale: 0.97 }, { opacity: 1, scale: 1, duration: ${ENTER}, ease: 'power3.out' }, 0);
    ${moves.join('\n    ')}
    tl.to('#frame', { opacity: 0, duration: 0.3, ease: 'power2.in' }, ${r2(length - 0.3)});
    window.__timelines['screen'] = tl;
  </script>
</template>
`;
}

function captions({ steps }) {
  const groups = steps.map((c, i) => `    <div id="cap-${i}" class="clip cap" data-start="${c.start}" data-duration="${r2(c.end - c.start)}" data-track-index="0">
      <div class="cap-in" id="cap-in-${i}"><b>${esc(c.text)}</b>${c.sub ? `<span>${esc(c.sub)}</span>` : ''}</div>
    </div>`).join('\n');
  const tweens = steps.map((c, i) => `tl.fromTo('#cap-in-${i}', { opacity: 0, y: 10 }, { opacity: 1, y: 0, duration: 0.25, ease: 'power2.out' }, ${c.start});
    tl.to('#cap-in-${i}', { opacity: 0, duration: 0.15, ease: 'power2.in' }, ${r2(c.end - 0.16)});`).join('\n    ');
  return `<template>
  <style>
    #root { position: absolute; inset: 0; font-family: Inter, sans-serif; }
    .cap { position: absolute; left: 0; right: 0; top: ${FRAME.top + VH + 26}px; text-align: center; overflow: visible; }
    .cap-in { display: flex; flex-direction: column; align-items: center; gap: 6px; }
    .cap b { color: ${COLORS.text}; font-size: 38px; font-weight: 700; line-height: 1.2; letter-spacing: -.01em; max-width: 1500px; }
    .cap span { color: ${COLORS.muted}; font-size: 28px; font-weight: 400; line-height: 1.25; max-width: 1500px; }
  </style>
  <div id="root" data-composition-id="captions" data-width="${W}" data-height="${H}">
${groups}
  </div>
  <script>
    const tl = gsap.timeline({ paused: true });
    ${tweens}
    window.__timelines['captions'] = tl;
  </script>
</template>
`;
}

function outro({ text, sub }) {
  return `<template>
  <style>
    #root { position: absolute; inset: 0; display: grid; place-items: center; font-family: Inter, sans-serif; text-align: center; }
    #outro-box { display: flex; flex-direction: column; align-items: center; gap: 20px; }
    #outro-title { color: ${COLORS.text}; font-size: 104px; font-weight: 900; letter-spacing: -.03em; }
    #outro-site { color: ${COLORS.accent}; font-size: 38px; font-weight: 700; }
  </style>
  <div id="root" data-composition-id="outro" data-width="${W}" data-height="${H}">
    <div id="outro-box">
      <div id="outro-title" data-var-text="end">${esc(text)}</div>
      <div id="outro-site">${esc(sub)}</div>
    </div>
  </div>
  <script>
    const tl = gsap.timeline({ paused: true });
    tl.fromTo('#outro-title', { opacity: 0, scale: 0.94 }, { opacity: 1, scale: 1, duration: 0.55, ease: 'back.out(1.6)' }, 0.15);
    tl.fromTo('#outro-site', { opacity: 0, y: 12 }, { opacity: 1, y: 0, duration: 0.45, ease: 'power3.out' }, 0.45);
    window.__timelines['outro'] = tl;
  </script>
</template>
`;
}

// Writes the project into dir (index.html, compositions/, assets/capture.mp4). Returns its length in seconds.
export function composeVideo({ dir, capture, log, music = null }) {
  const p = plan(log);
  mkdirSync(join(dir, 'compositions'), { recursive: true });
  mkdirSync(join(dir, 'assets'), { recursive: true });
  copyFileSync(capture, join(dir, 'assets', 'capture.mp4'));
  const musicFile = music ? `music${extname(music).toLowerCase() || '.mp3'}` : null;
  if (music) copyFileSync(music, join(dir, 'assets', musicFile));
  const total = r2(INTRO + p.length + OUTRO);
  const host = (id, start, duration, kind = 'graphics', lane = 0) => `  <div id="${id}" data-composition-id="${id}" data-composition-src="compositions/${id}.html" data-start="${start}" data-duration="${duration}" data-track-index="${lane}" data-track-kind="${kind}" data-width="${W}" data-height="${H}"></div>`;
  const vars = JSON.stringify([
    { id: 'title', type: 'string', label: 'Title', default: p.title.text },
    { id: 'sub', type: 'string', label: 'Line under the title', default: p.title.sub ?? '' },
    { id: 'end', type: 'string', label: 'End card', default: p.end.text },
  ]);
  const index = page(`<div id="root" data-composition-id="main" data-start="0" data-duration="${total}" data-width="${W}" data-height="${H}" data-fps="30">
  <div id="bg"></div>
${host('intro', 0, INTRO)}
${host('screen', INTRO, p.length)}
${host('captions', INTRO, p.length, 'captions', 1)}
${host('outro', r2(INTRO + p.length), OUTRO)}
${musicFile ? `  <audio id="music" class="clip" src="assets/${musicFile}" data-start="0" data-duration="${total}" data-track-index="2" data-volume="${MUSIC_VOLUME}" data-fade-in="${MUSIC_FADE_IN}" data-fade-out="${MUSIC_FADE_OUT}"></audio>` : ''}
</div>`, `window.__timelines['main'] = gsap.timeline({ paused: true });`).replace('<html lang="en">', `<html lang="en" data-composition-variables='${vars.replace(/'/g, '&#39;')}'>`);
  writeFileSync(join(dir, 'index.html'), index);
  writeFileSync(join(dir, 'compositions', 'intro.html'), intro({ title: p.title.text, sub: p.title.sub }));
  writeFileSync(join(dir, 'compositions', 'screen.html'), screen(p));
  writeFileSync(join(dir, 'compositions', 'captions.html'), captions(p));
  writeFileSync(join(dir, 'compositions', 'outro.html'), outro({ text: p.end.text, sub: p.end.sub }));
  writeFileSync(join(dir, 'hyperframes.json'), JSON.stringify({ paths: { blocks: 'compositions', assets: 'assets' }, media: { autoProxy: true } }, null, 2));
  return total;
}

// The HyperFrames CLI through the skill's launcher, never sending telemetry (.claude/skills/THIRD_PARTY.md).
export function hyperframes(args, { cwd } = {}) {
  return execFileSync('node', [LAUNCHER, '--cli-version', CLI_VERSION, ...args], {
    cwd, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'], maxBuffer: 64 << 20,
    env: { ...process.env, HYPERFRAMES_NO_TELEMETRY: '1', DO_NOT_TRACK: '1' },
  });
}

// The music of a film (editor, app): CM_VIDEO_MUSIC_<NAME>, else CM_VIDEO_MUSIC for both, else its take of
// "Matchday Manager" in test/video-music/. "none": no music.
export function musicOf(name) {
  const named = process.env[`CM_VIDEO_MUSIC_${name.toUpperCase()}`] || process.env.CM_VIDEO_MUSIC;
  if (named) return named === 'none' ? null : resolve(named);
  return resolve(HERE, 'video-music', `matchday-manager-${name}.mp3`);
}

export function render(dir, output) {
  return hyperframes(['render', '--output', resolve(output), '--quality', 'delivery'], { cwd: dir });
}
