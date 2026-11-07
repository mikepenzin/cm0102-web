// The app's side of the Game API: calls with ids, answers as promises, events to listeners, and a log of every
// message so the playground can show the traffic. It holds no game rules.
//
// Stale answers: each answer and event carries the engine's step, a counter that only grows. The client keeps the
// highest step it has seen; an answer with a lower one is marked stale, and a stale event is not passed on.
export class EngineClient {
  constructor(worker) {
    this.worker = worker;
    this.nextId = 1;
    this.pending = new Map();
    this.listeners = new Map();
    this.step = 0;
    this.onLog = () => {};
    worker.onmessage = (e) => this.#receive(e.data);
    worker.onerror = (e) => {
      const err = Object.assign(new Error(`The engine stopped: ${e.message ?? 'unknown error'}`), { code: 'engine_down' });
      this.onLog({ kind: 'error', text: err.message });
      for (const p of this.pending.values()) p.reject(err);
      this.pending.clear();
    };
  }

  // Resolves to {result, step, date, stale}; rejects with an Error whose code is the engine's.
  call(method, params = {}) {
    const id = this.nextId++;
    this.onLog({ kind: 'call', id, method, params });
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject, method });
      this.worker.postMessage({ id, method, params });
    });
  }

  on(event, fn) {
    if (!this.listeners.has(event)) this.listeners.set(event, []);
    this.listeners.get(event).push(fn);
  }

  #receive(m) {
    if (m.event) {
      const stale = m.data.step < this.step;
      this.onLog({ kind: 'event', event: m.event, data: m.data, stale });
      if (!stale) for (const fn of this.listeners.get(m.event) ?? []) fn(m.data);
      return;
    }
    const p = this.pending.get(m.id);
    if (!p) return;
    this.pending.delete(m.id);
    if (!m.ok) {
      this.onLog({ kind: 'refused', id: m.id, method: p.method, error: m.error });
      p.reject(Object.assign(new Error(m.error.message), { code: m.error.code }));
      return;
    }
    const stale = m.step < this.step;
    this.step = Math.max(this.step, m.step);
    this.onLog({ kind: 'answer', id: m.id, method: p.method, step: m.step, date: m.date, result: m.result, stale });
    p.resolve({ result: m.result, step: m.step, date: m.date, stale });
  }
}
