// Which errors of a game file's download are a dropped connection, so the page asks for the file again
// (loader.js, download): fetch's TypeError, an error marked again (a short body, a server error), and the browser's
// cache refusing a body that broke on the way ("Cache.put() encountered a network error", a DOMException named
// NetworkError: an Android phone's start stopped on it, 26 November 2026). A stop the player asked for never is.
export function retryable(e) {
  if (!e || e.name === 'AbortError') return false;
  return e instanceof TypeError || e.again === true || e.name === 'NetworkError';
}
