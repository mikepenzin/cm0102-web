// The page's features in early access (web/README.md, "Levels and early access"): a new feature adds its line here
// when it is built, { key, name } (key: a-z, 0-9 and -, the name in English for the admin page), and gates its entry
// point with early.js (earlyState, lockedHtml). The server (early.mjs) gives a key it sees for the first time 14 days
// of Club Legends only; the admin page's Early access tab changes the day. Features already out are not listed.
export const FEATURES = [
  { key: 'shortlist', name: 'Scout to shortlist' },          // Squad Hub: a Scout player onto a manager's Short List (shortlist.js)
  { key: 'scout-request', name: 'Scout a player' },          // Squad Hub: the manager's club asks its scouts to watch him (shortlist.js)
  { key: 'nationality', name: 'Nationality filter' },        // Squad Hub: the Players list and the Scout by a player's nation
  { key: 'manager-attributes', name: 'Manager attributes' }, // Squad Hub, Managers tab: any manager's or coach's attributes, changed
  { key: 'club-finances', name: 'Club finances' },           // Squad Hub, Clubs tab: any club's record and finances; balance, reputation, training changed
];
