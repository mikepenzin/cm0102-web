// The service worker (web/README.md, "As an app"): lets the page start without a connection once it has run.
//
// With a connection it changes nothing: every request goes to the server first, so a new version of the page or the
// engine is used at once. Only when the server cannot be reached (or does not answer in time) the answer comes from
// the cache that the page filled after its last good start (loader.js, primeOffline). The game files are not its
// business: the page keeps those in its own cache and asks for them there first.
const SHELL = 'cm0102-shell';
const WAIT_MS = 6000;          // a server that does not answer this long, with a kept copy at hand: the copy

self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (e) => e.waitUntil(self.clients.claim()));

async function kept(request) {
  const cache = await caches.open(SHELL);
  const url = new URL(request.url);
  return (await cache.match(request)) ?? (await cache.match(url.pathname + url.search)) ?? (url.pathname === '/' ? cache.match('/') : undefined);
}

// The engine under its version never changes: the kept copy first.
async function keptFirst(request) {
  return (await kept(request)) ?? fetch(request);
}

async function serverFirst(request) {
  const copy = await kept(request);
  try {
    const answer = fetch(request);
    if (!copy) return await answer;
    const timeout = new Promise((resolve) => setTimeout(() => resolve(null), WAIT_MS));
    return (await Promise.race([answer, timeout])) ?? copy;
  } catch (err) {
    if (copy) return copy;
    throw err;
  }
}

self.addEventListener('fetch', (e) => {
  const url = new URL(e.request.url);
  if (e.request.method !== 'GET' || url.origin !== self.location.origin || url.pathname.startsWith('/game/')) return;
  e.respondWith(url.pathname.startsWith('/engine/') && url.searchParams.has('v') ? keptFirst(e.request) : serverFirst(e.request));
});
