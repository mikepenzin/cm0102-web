// Payments for Cloud Saves through Paddle Billing (web/README.md, "Cloud saves", "Payments"). Paddle is the merchant
// of record: it sells the subscription to the player, takes the payment and the sales tax, and tells this server
// about the subscription with signed webhooks. The server keeps no card or payment data; it keeps the subscription's
// id and status in the account record, and the plan they give (cloud.mjs).
//
// The checkout runs in Paddle's overlay on the page (Paddle.js, account.js). It carries the account in custom_data:
// the Google id and an HMAC of it with the server's secret, so a webhook can only name an account this server named.
import { createHmac, timingSafeEqual } from 'node:crypto';

const DAY = 86400000;
export const PADDLE_API = { production: 'https://api.paddle.com', sandbox: 'https://sandbox-api.paddle.com' };

// The account's mark for the checkout's custom_data, and the account it names (or null).
export function accountRef(sub, secret) {
  return `${sub}.${createHmac('sha256', secret).update(`paddle account\n${sub}`).digest('base64url').slice(0, 32)}`;
}
export function openAccountRef(ref, secret) {
  if (typeof ref !== 'string') return null;
  const i = ref.lastIndexOf('.');
  if (i <= 0) return null;
  const sub = ref.slice(0, i);
  const want = Buffer.from(accountRef(sub, secret)), got = Buffer.from(ref);
  return /^[\w-]{1,64}$/.test(sub) && want.length === got.length && timingSafeEqual(want, got) ? sub : null;
}

// The Paddle-Signature header ("ts=<seconds>;h1=<hex>", more than one h1 while a secret is rotated): an HMAC-SHA256
// of "<ts>:<raw body>" with the notification destination's secret. null when it holds, else why not.
export function checkPaddleSignature(header, rawBody, secret, { toleranceSec = 300, now = Date.now() } = {}) {
  if (typeof header !== 'string' || !header) return 'no Paddle-Signature';
  let ts = null;
  const h1 = [];
  for (const part of header.split(';')) {
    const [k, v] = part.split('=');
    if (k === 'ts') ts = v;
    else if (k === 'h1' && v) h1.push(v);
  }
  if (!ts || !/^\d+$/.test(ts) || !h1.length) return 'a Paddle-Signature without ts or h1';
  if (Math.abs(now / 1000 - Number(ts)) > toleranceSec) return 'a Paddle-Signature too old or from the future';
  const want = createHmac('sha256', secret).update(`${ts}:`).update(rawBody).digest();
  for (const h of h1) {
    const got = Buffer.from(h, 'hex');
    if (got.length === want.length && timingSafeEqual(got, want)) return null;
  }
  return 'a Paddle-Signature that does not match';
}

// What a subscription's state gives: { plan, planUntil, cancelAt }. An active subscription runs to the end of its
// billing period, plus three days for the renewal's webhook to come; one that is due (Paddle retries the payment)
// keeps Cloud Saves a week; a paused or canceled one ends them.
export function planOf(sub, now = Date.now()) {
  const ends = Date.parse(sub.current_billing_period?.ends_at ?? '') || null;
  const cancelAt = sub.scheduled_change?.action === 'cancel' ? sub.scheduled_change.effective_at ?? null : null;
  const iso = (t) => new Date(t).toISOString();
  if (sub.status === 'active' || sub.status === 'trialing') {
    if (cancelAt) return { plan: 'cloud', planUntil: iso(Date.parse(cancelAt)), cancelAt };
    return { plan: 'cloud', planUntil: iso((ends ?? now) + 3 * DAY), cancelAt: null };
  }
  if (sub.status === 'past_due') return { plan: 'cloud', planUntil: iso(Math.max(ends ?? 0, now) + 7 * DAY), cancelAt: null };
  return { plan: 'free', planUntil: null, cancelAt: null };
}

// The account record after a subscription event, or null when the event changes nothing (an older event than the
// last one of the same subscription).
export function applySubscription(record, event, now = Date.now()) {
  const sub = event.data;
  const last = record?.paddle;
  if (last && last.subscriptionId === sub.id && last.eventAt && Date.parse(last.eventAt) > Date.parse(event.occurred_at)) return null;
  const p = planOf(sub, now);
  return {
    ...(record ?? {}),
    plan: p.plan,
    planUntil: p.planUntil,
    paddle: { subscriptionId: sub.id, customerId: sub.customer_id, status: sub.status, cancelAt: p.cancelAt,
      priceIds: (sub.items ?? []).map((i) => i.price?.id).filter(Boolean), eventAt: event.occurred_at },
  };
}

// A link into Paddle's customer portal for the account's subscription (cancel, card, invoices), or null.
export async function portalUrl({ apiKey, apiBase, customerId, subscriptionId }) {
  const r = await fetch(`${apiBase}/customers/${encodeURIComponent(customerId)}/portal-sessions`, {
    method: 'POST',
    headers: { Authorization: `Bearer ${apiKey}`, 'Content-Type': 'application/json' },
    body: JSON.stringify(subscriptionId ? { subscription_ids: [subscriptionId] } : {}),
    signal: AbortSignal.timeout(15000),
  });
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(`Paddle portal session: HTTP ${r.status} ${j.error?.code ?? ''}`);
  return j.data?.urls?.general?.overview ?? null;
}
