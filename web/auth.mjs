// Sign in with Google, and the signed cookies of the server (web/README.md, "Cloud saves").
//
// The sign-in is the OAuth 2.0 authorization code flow with PKCE, run by the server: the page goes to Google, Google
// comes back to /auth/google/callback with a code, and the server trades the code for an ID token at Google's token
// endpoint, with the client secret. The ID token then comes straight from Google over TLS, so its claims are checked
// (issuer, audience, expiry, a verified email) and its signature need not be: Google's OpenID Connect guide, "Obtain
// user information from the ID token". No script of Google's runs on the page.
//
// The server keeps no sessions: a cookie holds the account, sealed with an HMAC of CM_SESSION_SECRET.
import { createHash, createHmac, randomBytes, timingSafeEqual } from 'node:crypto';

export const GOOGLE = {
  authUrl: 'https://accounts.google.com/o/oauth2/v2/auth',
  tokenUrl: 'https://oauth2.googleapis.com/token',
  issuers: ['https://accounts.google.com', 'accounts.google.com'],
};

const b64u = (buf) => Buffer.from(buf).toString('base64url');
const nowSec = () => Math.floor(Date.now() / 1000);

// seal(kind, data, seconds) gives a token that open(kind, token) turns back into data until it expires. The kind keeps
// one sort of token from being taken for another (a sign-in state for a session).
export function sealer(secret) {
  const key = createHash('sha256').update(`cm0102-web seal\n${secret}`).digest();
  const mac = (body) => createHmac('sha256', key).update(body).digest();
  return {
    seal(kind, data, seconds) {
      const body = b64u(JSON.stringify({ k: kind, d: data, exp: nowSec() + seconds }));
      return `${body}.${b64u(mac(body))}`;
    },
    open(kind, token) {
      if (typeof token !== 'string' || token.length > 4096) return null;
      const parts = token.split('.');
      if (parts.length !== 2) return null;
      const want = mac(parts[0]), got = Buffer.from(parts[1], 'base64url');
      if (got.length !== want.length || !timingSafeEqual(got, want)) return null;
      let v;
      try { v = JSON.parse(Buffer.from(parts[0], 'base64url').toString()); } catch { return null; }
      if (!v || v.k !== kind || typeof v.exp !== 'number' || v.exp < nowSec()) return null;
      return v.d;
    },
  };
}

export function readCookies(req) {
  const out = {};
  for (const part of (req.headers.cookie ?? '').split(';')) {
    const i = part.indexOf('=');
    if (i > 0) out[part.slice(0, i).trim()] = decodeURIComponent(part.slice(i + 1).trim());
  }
  return out;
}

// value null clears the cookie.
export function setCookie(res, name, value, { maxAge, secure, path = '/' }) {
  const parts = [`${name}=${value === null ? '' : encodeURIComponent(value)}`, `Path=${path}`, 'HttpOnly', 'SameSite=Lax',
    `Max-Age=${value === null ? 0 : maxAge}`];
  if (secure) parts.push('Secure');
  const had = res.getHeader('Set-Cookie');
  res.setHeader('Set-Cookie', [...(Array.isArray(had) ? had : had ? [had] : []), parts.join('; ')]);
}

// Where Google sends the browser, and what the server keeps to finish the sign-in (in a sealed cookie).
export function googleStart({ clientId, redirectUri, google = GOOGLE }) {
  const state = b64u(randomBytes(24)), verifier = b64u(randomBytes(48));
  const challenge = b64u(createHash('sha256').update(verifier).digest());
  const u = new URL(google.authUrl);
  for (const [k, v] of Object.entries({ client_id: clientId, redirect_uri: redirectUri, response_type: 'code',
    scope: 'openid email profile', state, code_challenge: challenge, code_challenge_method: 'S256', prompt: 'select_account' }))
    u.searchParams.set(k, v);
  return { url: u.href, state, verifier };
}

export class SignInError extends Error {}

// The code back from Google, traded for the account: { sub, email, name, picture }.
export async function googleFinish({ code, verifier, clientId, clientSecret, redirectUri, google = GOOGLE }) {
  const r = await fetch(google.tokenUrl, {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams({ code, code_verifier: verifier, client_id: clientId, client_secret: clientSecret,
      redirect_uri: redirectUri, grant_type: 'authorization_code' }),
    signal: AbortSignal.timeout(15000),
  });
  const answer = await r.json().catch(() => ({}));
  if (!r.ok || typeof answer.id_token !== 'string') throw new SignInError(`Google did not accept the sign-in (${answer.error ?? r.status})`);
  let claims;
  try { claims = JSON.parse(Buffer.from(answer.id_token.split('.')[1], 'base64url').toString()); }
  catch { throw new SignInError('Google gave an ID token that is not readable'); }
  const aud = Array.isArray(claims.aud) ? claims.aud : [claims.aud];
  if (!google.issuers.includes(claims.iss)) throw new SignInError('the ID token is not from Google');
  if (!aud.includes(clientId)) throw new SignInError('the ID token is for another app');
  if (typeof claims.exp !== 'number' || claims.exp < nowSec() - 60) throw new SignInError('the ID token has expired');
  if (typeof claims.sub !== 'string' || !/^[\w-]{1,64}$/.test(claims.sub)) throw new SignInError('the ID token has no account id');
  if (!claims.email || claims.email_verified !== true) throw new SignInError('the Google account has no verified email');
  return { sub: claims.sub, email: String(claims.email).toLowerCase(), name: String(claims.name ?? claims.email).slice(0, 80),
    picture: typeof claims.picture === 'string' && claims.picture.startsWith('https://') ? claims.picture : null };
}
