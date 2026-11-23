// Early access on the page (web/README.md, "Levels and early access"): a feature of features.js, or a library
// database, that is for Club Legends until its day. The server says what is early (/api/me's early, /api/library's
// early and locked); this turns that into one of three states and draws the two shared pieces: the badge a Club Legend
// sees, and the line everyone else sees with the day it opens to them.
import { language, t } from './i18n.js';

const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const today = () => new Date().toISOString().slice(0, 10);

// 'open' (not early), 'early' (still early, and this player is a Club Legend) or 'locked' (still early, and not).
export function earlyState(me, key) {
  const day = me?.early?.features?.[key];
  if (!day || day <= today()) return 'open';
  return me.early.legend ? 'early' : 'locked';
}
// The same for a library database (its entry from /api/library).
export const entryState = (e) => (e?.locked ? 'locked' : e?.early ? 'early' : 'open');

// The day in the player's language (a day is UTC: read as such, so it is the same day everywhere).
export const earlyDate = (day) => new Date(`${day}T00:00:00Z`).toLocaleDateString(language() === 'en' ? undefined : language(),
  { dateStyle: 'medium', timeZone: 'UTC' });

export const badgeHtml = () => `<span class="early-badge">${esc(t('Early access'))}</span>`;

// The locked line: who has it and from when, then Sign in (signed out: a Club Legend signs in to get it) and the link
// to the membership page (offer.url, as Membership's). A button [data-early-signin] is the caller's to wire.
export function lockedHtml(day, { signedIn = true, offer = null } = {}) {
  const links = [signedIn ? '' : `<button class="small" data-early-signin>${esc(t('Sign in'))}</button>`,
    offer?.url ? `<a class="early-join" href="${esc(offer.url)}" target="_blank" rel="noopener">${esc(t('Become a Club Legend'))}</a>` : ''].filter(Boolean).join(' ');
  return `<div class="early-locked small">${esc(t('Club Legends have it now · everyone from {date}', { date: earlyDate(day) }))}${links ? ` <span class="early-links">${links}</span>` : ''}</div>`;
}
