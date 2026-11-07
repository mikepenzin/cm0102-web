// The engine host in the browser: the mock engine in a Web Worker, as the real engine will run (concept design,
// "Where the engine runs"). Messages in: {id, method, params}. Out: {id, ok, step, date, result} or
// {id, ok: false, error: {code, message}}, and events {event, data} that the engine sends on its own.
import { MockEngine } from './mock-engine.js';

const engine = new MockEngine({ dayMs: 80, tickMs: 220, onEvent: (event, data) => postMessage({ event, data }) });

onmessage = async ({ data: { id, method, params } }) => {
  try {
    postMessage({ id, ok: true, ...(await engine.call(method, params)) });
  } catch (err) {
    postMessage({ id, ok: false, error: { code: err.code ?? 'internal', message: err.message } });
  }
};
