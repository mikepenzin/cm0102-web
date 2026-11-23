import { describe, expect, it } from "vitest";
import { nameAllowed } from "../src/filter";

describe("word filter", () => {
  it.each([
    "Scunthorpe United",
    "Arsenal",
    "Middlesbrough",
    "Essex Sunday League",
    "Cockfosters FC",
    "Dick Advocaat",
    "Penistone Church",
    "Shitterton Rovers",
    "Club 1874",
    "Real Sociedad",
    "Max",
    "Zoë",
    "Cumming",
    "Spicy Wings",
    "Fukuoka",
    "Pakistan XI",
    "Kike Garcia",
    "Twatt",
    "Fuks",
    "Scunthorpe Utd",
  ])("allows %s", (name) => {
    expect(nameAllowed(name)).toBe(true);
  });

  it.each([
    "fuck",
    "FUCK league",
    "F.U.C.K",
    "f u c k",
    "fvck",
    "fuuuuck",
    "motherfuckers",
    "sh1t",
    "$hit FC",
    "cunts united",
    "wanker",
    "Hitler Youth",
    "n1gger",
    "f*ck",
    "c*nt",
    "sh*t FC",
    "cuntface",
    "shitbag",
    "phuck",
  ])("rejects %s", (name) => {
    expect(nameAllowed(name)).toBe(false);
  });
});
