// A small S3 client for the cloud saves (web/README.md, "Cloud saves"): Tigris, or any store that speaks the S3 API.
// No SDK: Signature Version 4 with node:crypto, requests with node:http(s).
//
// The settings are the variables that `fly storage create` sets on the app:
//   AWS_ENDPOINT_URL_S3   https://fly.storage.tigris.dev (or https://t3.storage.dev)
//   AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY, AWS_REGION (Tigris: auto), BUCKET_NAME
// The bucket goes in the host name (<bucket>.fly.storage.tigris.dev), as Tigris documents it; CM_S3_PATH_STYLE=1 puts it
// in the path instead (<endpoint>/<bucket>/<key>), for a local store.
//
// Keys are made only of A-Z a-z 0-9 - _ . / (cloud.mjs), so a path needs no encoding in the signature.
import { createHash, createHmac } from 'node:crypto';
import http from 'node:http';
import https from 'node:https';

const EMPTY_SHA256 = 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855';
const sha256 = (data) => createHash('sha256').update(data).digest('hex');
const hmac = (key, data) => createHmac('sha256', key).update(data).digest();

// RFC 3986 encoding, as Signature Version 4 wants it for query strings: everything but A-Z a-z 0-9 - _ . ~.
export const uriEncode = (s) => encodeURIComponent(s).replace(/[!'()*]/g, (c) => `%${c.charCodeAt(0).toString(16).toUpperCase()}`);

const lowerHeaders = (headers) => Object.fromEntries(Object.entries(headers).map(([k, v]) => [k.toLowerCase(), String(v).trim().replace(/\s+/g, ' ')]));
const canonicalQueryOf = (query) => Object.keys(query).sort().map((k) => `${uriEncode(k)}=${uriEncode(query[k] ?? '')}`).join('&');

// The signature of Signature Version 4 over a canonical request: lower holds the signed headers (lower case), signed
// their sorted names.
export function signatureV4({ method, path, query = {}, lower, signed, payloadHash, amzDate, secretAccessKey, region, service = 's3' }) {
  const day = amzDate.slice(0, 8);
  const canonical = [method, path, canonicalQueryOf(query), ...signed.map((k) => `${k}:${lower[k]}`), '', signed.join(';'), payloadHash].join('\n');
  const scope = `${day}/${region}/${service}/aws4_request`;
  const toSign = ['AWS4-HMAC-SHA256', amzDate, scope, sha256(canonical)].join('\n');
  const key = hmac(hmac(hmac(hmac(`AWS4${secretAccessKey}`, day), region), service), 'aws4_request');
  return createHmac('sha256', key).update(toSign).digest('hex');
}

// The Authorization header of a request (AWS Signature Version 4, "Authorization header", single chunk). headers must
// hold host and every header to sign; all x-amz-* and host are signed, plus the names in alsoSign.
export function signV4({ method, path, query = {}, headers, payloadHash, accessKeyId, secretAccessKey, region, service = 's3', alsoSign = [] }) {
  const lower = lowerHeaders(headers);
  const signed = Object.keys(lower).filter((k) => k === 'host' || k.startsWith('x-amz-') || alsoSign.includes(k)).sort();
  const amzDate = lower['x-amz-date'];
  const signature = signatureV4({ method, path, query, lower, signed, payloadHash, amzDate, secretAccessKey, region, service });
  return `AWS4-HMAC-SHA256 Credential=${accessKeyId}/${amzDate.slice(0, 8)}/${region}/${service}/aws4_request, SignedHeaders=${signed.join(';')}, Signature=${signature}`;
}

// A link for one request without the keys (Signature Version 4, "query string"): the query of the link. headers: the
// headers the request must carry with these values (signed, with host). The body is not signed (UNSIGNED-PAYLOAD): the
// link takes any body that matches the signed headers, content-length among them when it is given.
export function presignV4({ method, path, host, headers = {}, expires, amzDate, accessKeyId, secretAccessKey, region, service = 's3' }) {
  const lower = lowerHeaders({ host, ...headers });
  const signed = Object.keys(lower).sort();
  const query = { 'X-Amz-Algorithm': 'AWS4-HMAC-SHA256', 'X-Amz-Credential': `${accessKeyId}/${amzDate.slice(0, 8)}/${region}/${service}/aws4_request`,
    'X-Amz-Date': amzDate, 'X-Amz-Expires': String(expires), 'X-Amz-SignedHeaders': signed.join(';') };
  const signature = signatureV4({ method, path, query, lower, signed, payloadHash: 'UNSIGNED-PAYLOAD', amzDate, secretAccessKey, region, service });
  return `${canonicalQueryOf(query)}&X-Amz-Signature=${signature}`;
}

const amzNow = () => new Date().toISOString().replace(/[-:]/g, '').replace(/\.\d+/, '');
const unxml = (s) => s.replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&quot;/g, '"').replace(/&apos;/g, "'").replace(/&amp;/g, '&');
const tag = (xml, name) => { const m = new RegExp(`<${name}>([\\s\\S]*?)</${name}>`).exec(xml); return m ? unxml(m[1]) : null; };

export class S3Error extends Error {
  constructor(status, code, message) { super(`S3 ${status} ${code}: ${message}`); this.status = status; this.code = code; }
}

const RETRIES = [400, 1500];      // the pauses before a request that the store failed is sent again

export function createS3({ endpoint, bucket, accessKeyId, secretAccessKey, region = 'auto', pathStyle = false, timeoutMs = 30000 }) {
  const base = new URL(endpoint);
  const host = pathStyle ? base.host : `${bucket}.${base.host}`;
  const hostname = pathStyle ? base.hostname : `${bucket}.${base.hostname}`;
  const prefix = pathStyle ? `/${bucket}` : '';
  const lib = base.protocol === 'http:' ? http : https;

  // One request, and up to two more when the store answers 500 and up or the connection fails (each request of this
  // client can be sent again: a PUT writes the same object). body: a Buffer or undefined. Resolves to
  // { status, headers, body: Buffer } or, with stream, the response itself (to pipe on).
  async function request(method, key, opts = {}) {
    for (let i = 0; ; i++) {
      let r;
      try { r = await requestOnce(method, key, opts); } catch (err) {
        if (i >= RETRIES.length) throw err;
        await new Promise((ok) => setTimeout(ok, RETRIES[i]));
        continue;
      }
      const status = r.status ?? r.statusCode;
      if (status < 500 || i >= RETRIES.length) return r;
      if (r.resume) r.resume();                      // a stream answer: drop it before the next try
      await new Promise((ok) => setTimeout(ok, RETRIES[i]));
    }
  }

  function requestOnce(method, key, { query = {}, headers = {}, body, stream = false } = {}) {
    const path = `${prefix}/${key}`;
    const payloadHash = body ? sha256(body) : EMPTY_SHA256;
    const all = { host, 'x-amz-date': amzNow(), 'x-amz-content-sha256': payloadHash, ...headers };
    all.authorization = signV4({ method, path, query, headers: all, payloadHash, accessKeyId, secretAccessKey, region, alsoSign: ['content-type'] });
    if (body) all['content-length'] = body.length;
    const qs = canonicalQueryOf(query);
    return new Promise((resolveRequest, reject) => {
      const req = lib.request({ method, hostname, port: base.port || undefined,
        path: qs ? `${path}?${qs}` : path, headers: all, timeout: timeoutMs }, (res) => {
        if (stream && res.statusCode < 300) { resolveRequest(res); return; }
        const chunks = [];
        res.on('data', (c) => chunks.push(c));
        res.on('end', () => resolveRequest({ status: res.statusCode, headers: res.headers, body: Buffer.concat(chunks) }));
        res.on('error', reject);
      });
      req.on('timeout', () => req.destroy(new Error(`S3 ${method} ${key}: no answer in ${timeoutMs} ms`)));
      req.on('error', reject);
      req.end(body);
    });
  }

  const fail = (r, what) => {
    const xml = r.body?.toString() ?? '';
    return new S3Error(r.status ?? r.statusCode, tag(xml, 'Code') ?? 'Error', tag(xml, 'Message') ?? what);
  };
  const meta = (headers) => Object.fromEntries(Object.entries(headers).filter(([k]) => k.startsWith('x-amz-meta-')).map(([k, v]) => [k.slice(11), v]));

  return {
    host,
    // metadata: { name: ASCII value } as x-amz-meta-<name>. Resolves to the new ETag.
    async put(key, body, { type = 'application/octet-stream', metadata = {} } = {}) {
      const headers = { 'content-type': type, ...Object.fromEntries(Object.entries(metadata).map(([k, v]) => [`x-amz-meta-${k}`, String(v)])) };
      const r = await request('PUT', key, { headers, body });
      if (r.status !== 200) throw fail(r, `put ${key}`);
      return r.headers.etag;
    },
    // null when there is no such object, else { etag, size, metadata }.
    async head(key) {
      const r = await request('HEAD', key);
      if (r.status === 404) return null;
      if (r.status !== 200) throw fail(r, `head ${key}`);
      return { etag: r.headers.etag, size: Number(r.headers['content-length']), metadata: meta(r.headers) };
    },
    // null when there is no such object, else the response stream with statusCode and headers.
    async get(key) {
      const r = await request('GET', key, { stream: true });
      if (r.status === 404 || r.statusCode === 404) return null;
      if (r.statusCode === 200) return r;
      throw fail(r, `get ${key}`);
    },
    async getBuffer(key) {
      const r = await request('GET', key);
      if (r.status === 404) return null;
      if (r.status !== 200) throw fail(r, `get ${key}`);
      return r.body;
    },
    // A link that makes one request of method on key without the keys, for expires seconds: { url, headers }, the
    // headers that the request must carry as they are (signed).
    presign(method, key, { expires = 900, headers = {} } = {}) {
      const path = `${prefix}/${key}`;
      const q = presignV4({ method, path, host, headers, expires, amzDate: amzNow(), accessKeyId, secretAccessKey, region });
      return { url: `${base.protocol}//${host}${path}?${q}`, headers };
    },
    // A copy of the object from on to (on the store, the bytes do not come here), with its metadata, or with metadata and
    // type in place of it. The new ETag.
    async copy(from, to, { metadata = null, type = 'application/octet-stream' } = {}) {
      const headers = { 'x-amz-copy-source': `/${bucket}/${from}`, 'x-amz-metadata-directive': metadata ? 'REPLACE' : 'COPY',
        ...(metadata ? { 'content-type': type, ...Object.fromEntries(Object.entries(metadata).map(([k, v]) => [`x-amz-meta-${k}`, String(v)])) } : {}) };
      const r = await request('PUT', to, { headers });
      const xml = r.body?.toString() ?? '';
      if (r.status !== 200 || /<Error>/.test(xml)) throw fail(r, `copy ${from} to ${to}`);
      return tag(xml, 'ETag');
    },
    // The bucket's CORS rules: pages of origins may PUT and GET with the links of presign (cloud.mjs, "direct").
    async putCors(origins) {
      const xml = `<?xml version="1.0" encoding="UTF-8"?><CORSConfiguration><CORSRule>${origins.map((o) => `<AllowedOrigin>${o}</AllowedOrigin>`).join('')}`
        + '<AllowedMethod>GET</AllowedMethod><AllowedMethod>PUT</AllowedMethod><AllowedMethod>HEAD</AllowedMethod>'
        + '<AllowedHeader>*</AllowedHeader><ExposeHeader>ETag</ExposeHeader><MaxAgeSeconds>3600</MaxAgeSeconds></CORSRule></CORSConfiguration>';
      const body = Buffer.from(xml);
      const r = await request('PUT', '', { query: { cors: '' }, body,
        headers: { 'content-type': 'application/xml', 'content-md5': createHash('md5').update(body).digest('base64') } });
      if (r.status !== 200 && r.status !== 204) throw fail(r, 'put the bucket\'s CORS rules');
    },
    async delete(key) {
      const r = await request('DELETE', key);
      if (r.status !== 204 && r.status !== 200 && r.status !== 404) throw fail(r, `delete ${key}`);
    },
    // Every object under prefix: [{ key, size, etag, modified }].
    async list(listPrefix) {
      const out = [];
      let token = null;
      do {
        const query = { 'list-type': '2', prefix: listPrefix, ...(token ? { 'continuation-token': token } : {}) };
        const r = await request('GET', '', { query });
        if (r.status !== 200) throw fail(r, `list ${listPrefix}`);
        const xml = r.body.toString();
        for (const m of xml.matchAll(/<Contents>([\s\S]*?)<\/Contents>/g))
          out.push({ key: tag(m[1], 'Key'), size: Number(tag(m[1], 'Size')), etag: tag(m[1], 'ETag'), modified: tag(m[1], 'LastModified') });
        token = tag(xml, 'IsTruncated') === 'true' ? tag(xml, 'NextContinuationToken') : null;
      } while (token);
      return out;
    },
  };
}

// The store of the environment, or null when it is not set (cloud saves off).
export function s3FromEnv(env = process.env) {
  const need = ['AWS_ENDPOINT_URL_S3', 'AWS_ACCESS_KEY_ID', 'AWS_SECRET_ACCESS_KEY', 'BUCKET_NAME'];
  if (need.some((k) => !env[k])) return null;
  return createS3({ endpoint: env.AWS_ENDPOINT_URL_S3, bucket: env.BUCKET_NAME, accessKeyId: env.AWS_ACCESS_KEY_ID,
    secretAccessKey: env.AWS_SECRET_ACCESS_KEY, region: env.AWS_REGION || 'auto', pathStyle: env.CM_S3_PATH_STYLE === '1' });
}
