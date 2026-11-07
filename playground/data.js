// The hardcoded world of the mock engine: one club, a squad of 16, three transfer targets, a friendly and three league
// rounds. Every name is made up. Nothing here comes from the original's database.

export const CLUB = 'hc';

// Attribute names by position, 1 to 20, as the original shows them.
const KEYS = {
  GK: ['handling', 'reflexes', 'positioning', 'pace', 'stamina'],
  D: ['tackling', 'heading', 'passing', 'pace', 'stamina'],
  M: ['passing', 'tackling', 'shooting', 'pace', 'stamina'],
  F: ['shooting', 'pace', 'heading', 'passing', 'stamina'],
};

// ability: a hidden value, as current ability in the original. The engine keeps it and never sends it.
function player(id, name, position, age, club, values, extra = {}) {
  const attributes = Object.fromEntries(KEYS[position].map((k, i) => [k, values[i]]));
  const mean = values.reduce((a, b) => a + b, 0) / values.length;
  return {
    id, name, position, age, club, attributes,
    ability: Math.round(mean * 10),
    condition: 100, morale: 7, injuredDays: 0,
    wage: Math.round(mean * 900 / 50) * 50, contractEnds: '2004-06-30',
    value: Math.round(mean * mean * 6000 / 10000) * 10000,
    apps: 0, goals: 0,
    ...extra,
  };
}

export function newWorld() {
  const players = [
    player('p1', 'Tom Hale', 'GK', 29, CLUB, [14, 15, 13, 8, 12]),
    player('p2', 'Ben Ostrowski', 'GK', 21, CLUB, [10, 12, 9, 9, 13]),
    player('p3', 'Sam Kerridge', 'D', 27, CLUB, [15, 14, 10, 12, 14]),
    player('p4', 'Luis Varga', 'D', 31, CLUB, [14, 15, 11, 9, 12]),
    player('p5', 'Joe Penhallow', 'D', 24, CLUB, [13, 12, 12, 14, 15]),
    player('p6', 'Ade Okafor', 'D', 22, CLUB, [12, 13, 10, 15, 14]),
    player('p7', 'Rhys Morgan', 'D', 33, CLUB, [13, 14, 9, 7, 10]),
    player('p8', 'Danny Wren', 'M', 26, CLUB, [15, 11, 12, 13, 16]),
    player('p9', 'Kofi Mensah', 'M', 24, CLUB, [14, 13, 11, 14, 15]),
    player('p10', 'Elliot Shaw', 'M', 29, CLUB, [16, 10, 13, 11, 13]),
    player('p11', 'Marco Bellini', 'M', 20, CLUB, [12, 9, 12, 15, 14]),
    player('p12', 'Pete Lyle', 'M', 30, CLUB, [13, 12, 10, 10, 12]),
    player('p13', 'Jamie Calder', 'F', 25, CLUB, [16, 15, 12, 11, 13]),
    player('p14', 'Nico Ferraz', 'F', 28, CLUB, [15, 12, 14, 12, 12]),
    player('p15', 'Owen Pritchard', 'F', 19, CLUB, [12, 14, 10, 10, 14]),
    player('p16', 'Craig Doyle', 'F', 32, CLUB, [13, 9, 15, 10, 10]),
    player('t1', 'Viktor Lund', 'F', 26, 'ku', [17, 14, 13, 12, 14], { target: true, asking: 1200000 }),
    player('t2', 'Callum Reid', 'D', 23, 'wa', [15, 14, 11, 13, 15], { target: true, asking: 600000 }),
    player('t3', 'Yusuf Demir', 'M', 28, 'ar', [16, 12, 13, 12, 14], { target: true, asking: 900000 }),
  ];
  return structuredClone({
    day: 0,                      // days since 14.7.2001
    money: 1500000,
    rng: 42,                     // the state of the seeded roll: a save keeps it
    aiBidDone: false,            // the scripted bid of Kingsmead for Elliot Shaw, on day 5
    injuryDone: false,           // the scripted injury in the first match
    nextId: 1,
    clubs: {
      hc: { name: 'Harbour City' },
      ar: { name: 'Ashford Rovers', strength: 11.5 },
      ku: { name: 'Kingsmead United', strength: 12.5 },
      wa: { name: 'Westgate Athletic', strength: 10.5 },
      ra: { name: 'Riverside Amateurs', strength: 7 },
    },
    league: ['hc', 'ar', 'ku', 'wa'],
    players: Object.fromEntries(players.map((p) => [p.id, p])),
    selection: ['p1', 'p3', 'p4', 'p5', 'p6', 'p8', 'p9', 'p10', 'p11', 'p13', 'p14'],
    fixtures: [
      { id: 'f1', day: 6, home: 'hc', away: 'ra', competition: 'Friendly' },
      { id: 'f2', day: 21, home: 'hc', away: 'ar', competition: 'League' },
      { id: 'f3', day: 21, home: 'ku', away: 'wa', competition: 'League' },
      { id: 'f4', day: 28, home: 'ku', away: 'hc', competition: 'League' },
      { id: 'f5', day: 28, home: 'wa', away: 'ar', competition: 'League' },
      { id: 'f6', day: 35, home: 'hc', away: 'wa', competition: 'League' },
      { id: 'f7', day: 35, home: 'ar', away: 'ku', competition: 'League' },
    ].map((f) => ({ ...f, played: false, score: null, commentary: [], scorers: [] })),
    bids: [],
    inbox: [{
      id: 'n0', day: 0, kind: 'welcome', title: 'Welcome to Harbour City',
      text: 'The board expects a top-two finish. A friendly against Riverside Amateurs comes on 20 July.',
      choices: null, answered: null,
    }],
  });
}
