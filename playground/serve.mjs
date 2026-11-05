// A small static server for the playground: a module worker needs http, not file://. Run: node playground/serve.mjs
import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { dirname, extname, join, normalize } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = dirname(fileURLToPath(import.meta.url));
const TYPES = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.json': 'application/json' };
const PORT = Number(process.env.PORT ?? 8090), HOST = process.env.HOST ?? '127.0.0.1';

http.createServer(async (req, res) => {
  const path = normalize(decodeURIComponent(new URL(req.url, 'http://x').pathname)).replace(/^([/\\])+/, '');
  const file = join(ROOT, path || 'index.html');
  if (!file.startsWith(ROOT) || file.includes(`${ROOT}/test`) || file.includes(`${ROOT}/node_modules`)) {
    res.writeHead(404); res.end(); return;
  }
  try {
    const body = await readFile(file);
    res.writeHead(200, { 'Content-Type': TYPES[extname(file)] ?? 'application/octet-stream', 'Cache-Control': 'no-store' });
    res.end(body);
  } catch {
    res.writeHead(404); res.end();
  }
}).listen(PORT, HOST, () => console.log(`playground on http://${HOST}:${PORT}`));
