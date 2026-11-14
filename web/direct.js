// Cloud saves and data sets straight between the page and the store (cloud.mjs, "direct"; web/README.md, "Cloud saves").
// An upload: the server checks it may go and signs a link (?direct=put), the page sends the file to the store with that
// link, and the server then checks the file and puts it in its place (?direct=done). A download: a link from the
// server (?direct=get), then the store. The files do not pass the server's machine.
//
// When the server has it off it says so ({ direct: false }), and the callers send the file through the server as before.
// When the store does not answer the page (no CORS rules for this site, a network that blocks the store) or refuses a
// link, the page goes through the server for the rest of the visit.

let off = false;
const JSON_CHANGE = { 'content-type': 'application/json', 'x-cm': '1' };

function turnOff(log, why) {
  if (!off) log(`page: the cloud store did not answer this page directly (${why}): saves and data sets go through the server`);
  off = true;
}

// url: the API address of the file (/api/saves/<name>); body: the Blob or bytes to send; fields: what the server checks before
// (rawSize, saved); base: the ETag the change is made on, or 'none'. Resolves to null (send it through the server
// instead), or to the answer the caller reads as the server's own: the last step's, or the first's when the server
// refused the upload (409 changed, 507 full, 413, 401...). A store that failed (500 and up) gives its answer: try again.
export async function upDirect({ url, body, fields = {}, base, signal, log = () => {}, send = fetch }) {
  if (off) return null;
  const ask = await send(`${url}?direct=put`, { method: 'POST', signal, headers: JSON_CHANGE,
    body: JSON.stringify({ ...fields, size: body.size ?? body.byteLength, base }) });
  if (!ask.ok) return ask;
  const link = await ask.clone().json();
  if (!link.direct) return null;
  let put;
  try { put = await send(link.url, { method: 'PUT', body, headers: link.headers, signal }); } catch (err) {
    if (err.name === 'AbortError' || err.name === 'TimeoutError') throw err;
    turnOff(log, err.message);
    return null;
  }
  if (put.status >= 500) return put;
  if (!put.ok) { turnOff(log, `HTTP ${put.status}`); return null; }
  return send(`${url}?direct=done`, { method: 'POST', signal, headers: JSON_CHANGE, body: JSON.stringify({ key: link.key, base }) });
}

// Resolves to null (get it through the server instead), or { response, etag, saved }: response is the store's answer
// with the file, or the server's when it refused (404, 401...) or the store failed (500 and up: try again).
export async function downDirect({ url, signal, log = () => {}, send = fetch }) {
  if (off) return null;
  const ask = await send(`${url}?direct=get`, { cache: 'no-store', signal });
  if (!ask.ok) return { response: ask };
  const link = await ask.json();
  if (!link.direct) return null;
  let r;
  try { r = await send(link.url, { cache: 'no-store', signal }); } catch (err) {
    if (err.name === 'AbortError' || err.name === 'TimeoutError') throw err;
    turnOff(log, err.message);
    return null;
  }
  if (r.status >= 500) return { response: r };
  if (!r.ok) { turnOff(log, `HTTP ${r.status}`); return null; }
  return { response: r, etag: link.etag, saved: link.saved };
}

export const directOff = () => off;
