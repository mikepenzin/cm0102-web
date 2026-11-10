// Stand-ins for the services of the cloud saves, for the tests: an S3 store that checks every signature (as Tigris
// does), and Google's sign-in (its authorization endpoint answers at once, as if the person had picked an account).
import { createHash, createHmac, randomBytes } from 'node:crypto';
import http from 'node:http';
import { signV4 } from '../s3.mjs';

const sha256 = (b) => createHash('sha256').update(b).digest('hex');
const xmlEsc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

function body(req) {
  return new Promise((resolve) => { const c = []; req.on('data', (d) => c.push(d)); req.on('end', () => resolve(Buffer.concat(c))); });
}

// An S3 store, path style (http://127.0.0.1:<port>/<bucket>/<key>). objects: key -> { body, etag, type, meta, modified }.
export async function fakeS3({ bucket = 'cm-test', accessKeyId = 'tid_test', secretAccessKey = 'tsec_test', region = 'auto', pageSize = 1000 } = {}) {
  const objects = new Map(), requests = [];
  const server = http.createServer(async (req, res) => {
    const data = await body(req);
    const url = new URL(req.url, 'http://x');
    requests.push(`${req.method} ${url.pathname}${url.search}`);
    const reply = (status, xml = '', headers = {}) => { res.writeHead(status, { 'Content-Type': 'application/xml', ...headers }); res.end(xml); };
    const error = (status, code) => reply(status, `<?xml version="1.0"?><Error><Code>${code}</Code><Message>${code}</Message></Error>`);
    // The signature, as S3 checks it.
    const auth = req.headers.authorization ?? '';
    const m = /^AWS4-HMAC-SHA256 Credential=([^/]+)\/(\d{8})\/([^/]+)\/s3\/aws4_request, SignedHeaders=([\w;-]+), Signature=(\w+)$/.exec(auth);
    if (!m || m[1] !== accessKeyId || m[3] !== region) return error(403, 'InvalidAccessKeyId');
    if (req.headers['x-amz-content-sha256'] !== sha256(data)) return error(400, 'XAmzContentSHA256Mismatch');
    const signedHeaders = Object.fromEntries(m[4].split(';').map((h) => [h, req.headers[h]]));
    const query = Object.fromEntries(url.searchParams);
    const want = signV4({ method: req.method, path: url.pathname, query, headers: signedHeaders, payloadHash: req.headers['x-amz-content-sha256'],
      accessKeyId, secretAccessKey, region, alsoSign: m[4].split(';') });
    if (want !== auth) return error(403, 'SignatureDoesNotMatch');
    const parts = url.pathname.split('/');
    if (parts[1] !== bucket) return error(404, 'NoSuchBucket');
    const key = decodeURIComponent(parts.slice(2).join('/'));
    if (!key && req.method === 'GET' && query['list-type'] === '2') {
      const keys = [...objects.keys()].filter((k) => k.startsWith(query.prefix ?? '')).sort();
      const start = query['continuation-token'] ? Number(Buffer.from(query['continuation-token'], 'base64').toString()) : 0;
      const page = keys.slice(start, start + pageSize), more = start + pageSize < keys.length;
      return reply(200, `<?xml version="1.0"?><ListBucketResult><IsTruncated>${more}</IsTruncated>${page.map((k) => {
        const o = objects.get(k);
        return `<Contents><Key>${xmlEsc(k)}</Key><LastModified>${o.modified}</LastModified><ETag>${xmlEsc(o.etag)}</ETag><Size>${o.body.length}</Size></Contents>`;
      }).join('')}${more ? `<NextContinuationToken>${Buffer.from(String(start + pageSize)).toString('base64')}</NextContinuationToken>` : ''}</ListBucketResult>`);
    }
    const o = objects.get(key);
    if (req.method === 'PUT') {
      const meta = Object.fromEntries(Object.entries(req.headers).filter(([k]) => k.startsWith('x-amz-meta-')));
      const etag = `"${createHash('md5').update(data).digest('hex')}"`;
      objects.set(key, { body: data, etag, type: req.headers['content-type'], meta, modified: new Date().toISOString() });
      return reply(200, '', { ETag: etag });
    }
    if (req.method === 'DELETE') { objects.delete(key); return reply(204); }
    if (!o) return error(404, 'NoSuchKey');
    res.writeHead(200, { 'Content-Type': o.type ?? 'application/octet-stream', 'Content-Length': o.body.length, ETag: o.etag, ...o.meta });
    res.end(req.method === 'HEAD' ? undefined : o.body);
  });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  const endpoint = `http://127.0.0.1:${server.address().port}`;
  return { server, objects, requests, endpoint, bucket, close: () => server.close(),
    env: { AWS_ENDPOINT_URL_S3: endpoint, AWS_ACCESS_KEY_ID: accessKeyId, AWS_SECRET_ACCESS_KEY: secretAccessKey, AWS_REGION: region,
      BUCKET_NAME: bucket, CM_S3_PATH_STYLE: '1' } };
}

// Google's sign-in. The next sign-in is as fake.user (claims of the ID token); claims may be changed for a test.
export async function fakeGoogle({ clientId = 'test-client.apps.googleusercontent.com', clientSecret = 'test-secret' } = {}) {
  const codes = new Map();
  const fake = { user: { sub: '1000001', email: 'player@example.com', email_verified: true, name: 'A Player' }, extra: {}, tokenCalls: 0 };
  const server = http.createServer(async (req, res) => {
    const url = new URL(req.url, 'http://x');
    if (url.pathname === '/auth') {
      const q = Object.fromEntries(url.searchParams);
      if (q.client_id !== clientId || q.response_type !== 'code' || q.code_challenge_method !== 'S256' || !/openid/.test(q.scope)) {
        res.writeHead(400); res.end('bad request'); return;
      }
      const code = randomBytes(12).toString('hex');
      codes.set(code, { challenge: q.code_challenge, redirectUri: q.redirect_uri, user: { ...fake.user } });
      const back = new URL(q.redirect_uri);
      back.searchParams.set('code', code);
      back.searchParams.set('state', q.state);
      res.writeHead(302, { Location: back.href });
      res.end();
      return;
    }
    if (url.pathname === '/token' && req.method === 'POST') {
      fake.tokenCalls++;
      const p = Object.fromEntries(new URLSearchParams((await body(req)).toString()));
      const c = codes.get(p.code);
      codes.delete(p.code);
      const verifierHash = createHash('sha256').update(p.code_verifier ?? '').digest('base64url');
      if (!c || p.client_id !== clientId || p.client_secret !== clientSecret || p.redirect_uri !== c.redirectUri
        || p.grant_type !== 'authorization_code' || verifierHash !== c.challenge) {
        res.writeHead(400, { 'Content-Type': 'application/json' }); res.end(JSON.stringify({ error: 'invalid_grant' })); return;
      }
      const now = Math.floor(Date.now() / 1000);
      const claims = { iss: 'https://accounts.google.com', aud: clientId, iat: now, exp: now + 3600, ...c.user, ...fake.extra };
      const seg = (o) => Buffer.from(JSON.stringify(o)).toString('base64url');
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ id_token: `${seg({ alg: 'RS256', kid: 'test' })}.${seg(claims)}.c2ln`, access_token: 'x', token_type: 'Bearer' }));
      return;
    }
    res.writeHead(404); res.end();
  });
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  const base = `http://127.0.0.1:${server.address().port}`;
  fake.server = server;
  fake.close = () => server.close();
  fake.google = { authUrl: `${base}/auth`, tokenUrl: `${base}/token`, issuers: ['https://accounts.google.com'] };
  fake.clientId = clientId;
  fake.clientSecret = clientSecret;
  return fake;
}

// The bytes of a small saved game as saves.js checks it: the container header, one part, and some data.
export function fakeSaveBytes(fill = 7, extra = 4000) {
  const HEADER = 0x416ec;
  const b = Buffer.alloc(HEADER + extra, fill);
  b.writeInt32LE(1, 0);
  b.writeInt32LE(1, 8);
  b.writeUInt32LE(HEADER, 12);
  return b;
}

// A Paddle notification as Paddle sends it, and its Paddle-Signature.
export function signPaddle(raw, secret, ts = Math.floor(Date.now() / 1000)) {
  return `ts=${ts};h1=${createHmac('sha256', secret).update(`${ts}:${raw}`).digest('hex')}`;
}
export function paddleEvent(type, { ref, at = new Date().toISOString(), status = 'active', price = 'pri_cloud_month', cancelAt = null } = {}) {
  const start = new Date(), end = new Date(Date.now() + 30 * 86400e3);
  return {
    event_id: `evt_${randomBytes(6).toString('hex')}`, event_type: type, occurred_at: at, notification_id: `ntf_${randomBytes(6).toString('hex')}`,
    data: {
      id: 'sub_test_1', status, customer_id: 'ctm_test_1', custom_data: { account: ref },
      current_billing_period: { starts_at: start.toISOString(), ends_at: end.toISOString() },
      scheduled_change: cancelAt ? { action: 'cancel', effective_at: cancelAt, resume_at: null } : null,
      items: [{ status: 'active', quantity: 1, price: { id: price, product_id: 'pro_cloud' } }],
    },
  };
}

// A small saved game in the format of the game's own (savedit.js has the facts): the container (version 4, members
// run-coded), index.dat with the 22 entries, and the tables the editor reads. Three people: two players and one
// coach. players: [{ first, second, common, club, ca, pa, gk, born: [y, m, d], attrs: byte of each attribute }].
export async function fakeSaveGame(players = [
  { first: 'Alessandro', second: 'Del Piero', ca: 181, pa: 190, gk: 1, born: [1974, 11, 9], club: 0, fill: 15 },
  { first: 'Gianluigi', second: 'Buffon', ca: 187, pa: 194, gk: 20, born: [1978, 1, 28], club: 0, fill: 12 },
]) {
  const { encodeRuns, writeDate } = await import('../savedit.js');
  const names = (list) => { const b = Buffer.alloc(list.length * 60); list.forEach((n, i) => { b.write(n, i * 60, 'latin1'); b.writeInt32LE(i, i * 60 + 0x33); b.writeInt32LE(-1, i * 60 + 0x37); b[i * 60 + 0x3b] = 1; }); return b; };
  const first = ['', ...players.map((p) => p.first), 'Marcello'], second = ['', ...players.map((p) => p.second), 'Lippi'];
  const staff = Buffer.alloc((players.length + 1) * 110), player = Buffer.alloc(players.length * 70);
  players.forEach((p, i) => {
    const o = i * 110;
    staff.writeInt32LE(i, o); staff.writeInt32LE(i + 1, o + 4); staff.writeInt32LE(i + 1, o + 8); staff.writeInt32LE(0, o + 0x0c);
    writeDate(staff, o + 0x10, { year: p.born[0], month: p.born[1], day: p.born[2] });
    staff.writeUInt16LE(p.born[0], o + 0x18); staff.writeInt32LE(0, o + 0x1a); staff.writeInt32LE(p.club, o + 0x39); staff[o + 0x3d] = 11;
    staff[o + 0x58] = 16; staff[o + 0x5f] = 2; staff.writeInt32LE(i, o + 0x61); staff.writeInt32LE(-1, o + 0x65); staff.writeInt32LE(-1, o + 0x69);
    const q = i * 70;
    player.writeInt32LE(i, q); player.writeInt16LE(p.ca, q + 5); player.writeInt16LE(p.pa, q + 7);
    for (let k = 0; k < 12; k++) player[q + 0x0f + k] = 1;
    player[q + 0x0f] = p.gk; if (p.gk !== 20) { player[q + 0x0f + 6] = 20; player[q + 0x0f + 10] = 20; }
    for (let k = 0x1b; k <= 0x44; k++) player[q + k] = p.fill;
  });
  const c = players.length * 110;                         // the coach: no player data
  staff.writeInt32LE(players.length, c); staff.writeInt32LE(first.length - 1, c + 4); staff.writeInt32LE(second.length - 1, c + 8);
  staff.writeInt32LE(-1, c + 0x61); staff.writeInt32LE(-1, c + 0x65); staff.writeInt32LE(0, c + 0x69); staff.writeInt32LE(0, c + 0x39);
  const club = Buffer.alloc(581); club.writeInt32LE(0, 0); club.write('Juventus', 4, 'latin1');
  const natClub = Buffer.alloc(581); natClub.writeInt32LE(1, 0); natClub.write('Italy', 4, 'latin1');
  const nation = Buffer.alloc(290); nation.writeInt32LE(0, 0); nation.write('Italy', 4, 'latin1');
  const tables = [['first_names.dat', 13, names(first)], ['second_names.dat', 14, names(second)], ['common_names.dat', 15, names([''])],
    ['club.dat', 0, club], ['nat_club.dat', 1, natClub], ['nation.dat', 4, nation], ['staff.dat', 6, staff], ['player.dat', 10, player]];
  const sizes = { 13: 60, 14: 60, 15: 60, 0: 581, 1: 581, 4: 290, 6: 110, 10: 70 };
  const index = Buffer.alloc(8 + 22 * 0x43 + 0x1cc, 0xcc);           // the save writes stack bytes after the entries
  index.writeInt32LE(127, 0); index.writeInt32LE(16, 4);
  for (let k = 0; k < 22; k++) {
    const o = 8 + k * 0x43, t = tables[k];
    index.fill(0, o, o + 0x43);
    if (t) { index.write(t[0], o, 'latin1'); index.writeInt32LE(t[1], o + 0x33); index.writeInt32LE(t[2].length / sizes[t[1]], o + 0x37); index.writeInt32LE(2, o + 0x3f); }
    else index.writeInt32LE(100 + k, o + 0x33);
  }
  const members = [['continent.dat', Buffer.from('stays as it is, byte for byte')], ...tables.map((t) => [t[0], t[2]]),
    ['index.dat', index], ['general.dat', Buffer.alloc(3000, 7)]];
  const HEADER = 0x416ec, header = Buffer.alloc(HEADER);
  header.writeInt32LE(4, 0); header.writeInt32LE(0, 4); header.writeInt32LE(members.length, 8);
  const body = [];
  let at = HEADER;
  members.forEach(([name, data], k) => {
    const raw = Buffer.from(encodeRuns(data));
    header.writeUInt32LE(at, 12 + k * 0x10c); header.writeUInt32LE(data.length, 16 + k * 0x10c); header.write(name, 20 + k * 0x10c, 'latin1');
    body.push(raw);
    at += raw.length;
  });
  return new Uint8Array(Buffer.concat([header, ...body]));
}
