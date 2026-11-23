// The levels of an account (web/README.md, "Levels and early access"), lowest first; each includes everything below
// it. free: what everyone gets; supporter: the same, as a thank-you; squad: Membership (Cloud Saves, the full Scout);
// legend: Membership and early access (early.mjs).
//
// A Buy Me a Coffee membership's level name (or id) maps onto them through CM_LEVELS, its level names from lowest to
// highest (default Supporter,Squad Player,Club Legend): the first is supporter, the second squad, any later one legend.
// A server with only the older BMC_CLOUD_LEVELS (the levels that gave Cloud Saves) keeps working: those are squad,
// any other paid level supporter.
export const LEVELS = ['free', 'supporter', 'squad', 'legend'];
const DEFAULT_NAMES = ['supporter', 'squad player', 'club legend'];

export const rank = (level) => Math.max(0, LEVELS.indexOf(level));
export const atLeast = (level, min) => rank(level) >= rank(min);
export const higher = (a, b) => (rank(b) > rank(a) ? b : a);

const list = (v) => String(v ?? '').split(',').map((x) => x.trim().toLowerCase()).filter(Boolean);

// { names, legacyCloud }: CM_LEVELS (or the default), or, when only BMC_CLOUD_LEVELS is set, its list.
export function levelsFromEnv(env = process.env) {
  if (env.CM_LEVELS != null && list(env.CM_LEVELS).length) return { names: list(env.CM_LEVELS), legacyCloud: null };
  if (env.BMC_CLOUD_LEVELS != null) return { names: null, legacyCloud: list(env.BMC_CLOUD_LEVELS) };
  return { names: DEFAULT_NAMES, legacyCloud: null };
}

// The site level of a paid membership ({ level, levelId }), and whether its name is one the settings know.
export function paidLevel(member, cfg) {
  const name = String(member?.level ?? '').trim().toLowerCase(), id = member?.levelId == null ? '' : String(member.levelId);
  const has = (xs) => xs.includes(name) || (id !== '' && xs.includes(id));
  if (!cfg?.names) {
    const cloud = cfg?.legacyCloud ?? [];
    return { level: !cloud.length || has(cloud) ? 'squad' : 'supporter', known: true };
  }
  const at = cfg.names.findIndex((n) => n === name || (id !== '' && n === id));
  if (at < 0) return { level: 'supporter', known: false };
  return { level: at === 0 ? 'supporter' : at === 1 ? 'squad' : 'legend', known: true };
}
