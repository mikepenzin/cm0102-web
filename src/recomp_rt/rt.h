/* Runtime of the recompiled original: registers, flags, x87, the flat memory and the helpers that the
 * generated C (tools/recomp) calls. Every generated function is void f_XXXXXXXX(void) and works on cpu,
 * fpu and the memory block g_mem, where the PE image sits at its original addresses. */
#ifndef RECOMP_RT_H
#define RECOMP_RT_H

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t df;
} Cpu;

typedef struct {
    double st[8];
    uint32_t top;
    uint16_t sw, cw;
} Fpu;

extern Cpu cpu;
extern Fpu fpu;
extern uint8_t *g_mem;

#define RT_FS_BASE 0u                 /* fs:[0] holds the SEH chain; the Unicorn run uses base 0 too */

typedef void (*RtFn)(void);

/* Memory. M is the local copy of g_mem in each generated function. */
static inline uint32_t rd32(const uint8_t *m, uint32_t a) { uint32_t v; memcpy(&v, m + a, 4); return v; }
static inline uint32_t rd16(const uint8_t *m, uint32_t a) { uint16_t v; memcpy(&v, m + a, 2); return v; }
static inline uint32_t rd8(const uint8_t *m, uint32_t a) { return m[a]; }
static inline uint64_t rd64(const uint8_t *m, uint32_t a) { uint64_t v; memcpy(&v, m + a, 8); return v; }
static inline void wr32(uint8_t *m, uint32_t a, uint32_t v) { memcpy(m + a, &v, 4); }
static inline void wr16(uint8_t *m, uint32_t a, uint32_t v) { uint16_t x = (uint16_t)v; memcpy(m + a, &x, 2); }
static inline void wr8(uint8_t *m, uint32_t a, uint32_t v) { m[a] = (uint8_t)v; }
static inline void wr64(uint8_t *m, uint32_t a, uint64_t v) { memcpy(m + a, &v, 8); }
static inline double rdf32(const uint8_t *m, uint32_t a) { float v; memcpy(&v, m + a, 4); return v; }
static inline double rdf64(const uint8_t *m, uint32_t a) { double v; memcpy(&v, m + a, 8); return v; }
static inline void wrf32(uint8_t *m, uint32_t a, double v) { float x = (float)v; memcpy(m + a, &x, 4); }
static inline void wrf64(uint8_t *m, uint32_t a, double v) { memcpy(m + a, &v, 8); }
double rdf80(const uint8_t *m, uint32_t a);
void wrf80(uint8_t *m, uint32_t a, double v);

#define R8(a) rd8(M, (a))
#define R16(a) rd16(M, (a))
#define R32(a) rd32(M, (a))
#define R64(a) rd64(M, (a))
#define W8(a, v) wr8(M, (a), (v))
#define W16(a, v) wr16(M, (a), (v))
#define W32(a, v) wr32(M, (a), (v))
#define W64(a, v) wr64(M, (a), (v))

#define SYNC() (cpu.eax = eax, cpu.ecx = ecx, cpu.edx = edx, cpu.ebx = ebx, \
                cpu.esp = esp, cpu.ebp = ebp, cpu.esi = esi, cpu.edi = edi)
#define LOAD() (eax = cpu.eax, ecx = cpu.ecx, edx = cpu.edx, ebx = cpu.ebx, \
                esp = cpu.esp, ebp = cpu.ebp, esi = cpu.esi, edi = cpu.edi)

/* Flags: the last instruction that set them, kept as its operands and result.
 * K_ADD: r = a + b + c.  K_SUB: r = a - b - c.  K_INC, K_DEC: as add and sub of 1, c is the kept CF.
 * K_RES: ZF, SF and PF from r, c = CF | OF << 1.  K_RAW: r holds EFLAGS. */
typedef struct { uint32_t op, a, b, r, c; } Flags;
enum { K_ADD = 0, K_SUB = 1, K_INC = 2, K_DEC = 3, K_RES = 4, K_RAW = 5 };
#define SZ1 0x10u
#define SZ2 0x20u
#define SZ4 0x40u
#define SETF(o, A, B, R, C) (F.op = (o), F.a = (A), F.b = (B), F.r = (R), F.c = (C))

static inline uint32_t f_mask(uint32_t op) { return op & SZ1 ? 0xffu : op & SZ2 ? 0xffffu : 0xffffffffu; }
static inline uint32_t f_msb(uint32_t op) { return op & SZ1 ? 0x80u : op & SZ2 ? 0x8000u : 0x80000000u; }

static inline uint32_t f_cf(Flags f)
{
    uint32_t m = f_mask(f.op);
    switch (f.op & 15) {
    case K_ADD: return f.c ? (f.r & m) <= (f.a & m) : (f.r & m) < (f.a & m);
    case K_SUB: return f.c ? (f.a & m) <= (f.b & m) : (f.a & m) < (f.b & m);
    case K_INC: case K_DEC: return f.c;
    case K_RES: return f.c & 1;
    default: return f.r & 1;
    }
}
static inline uint32_t f_zf(Flags f) { return (f.op & 15) == K_RAW ? (f.r >> 6) & 1 : (f.r & f_mask(f.op)) == 0; }
static inline uint32_t f_sf(Flags f) { return (f.op & 15) == K_RAW ? (f.r >> 7) & 1 : (f.r & f_msb(f.op)) != 0; }
static inline uint32_t f_pf(Flags f) { return (f.op & 15) == K_RAW ? (f.r >> 2) & 1 : !__builtin_parity(f.r & 0xffu); }
static inline uint32_t f_of(Flags f)
{
    uint32_t s = f_msb(f.op), m = f_mask(f.op);
    switch (f.op & 15) {
    case K_ADD: return ((f.a ^ f.r) & (f.b ^ f.r) & s) != 0;
    case K_SUB: return ((f.a ^ f.b) & (f.a ^ f.r) & s) != 0;
    case K_INC: return (f.r & m) == s;
    case K_DEC: return (f.r & m) == s - 1;
    case K_RES: return (f.c >> 1) & 1;
    default: return (f.r >> 11) & 1;
    }
}
static inline uint32_t f_af(Flags f)
{
    switch (f.op & 15) {
    case K_ADD: case K_SUB: case K_INC: case K_DEC: return ((f.a ^ f.b ^ f.r) >> 4) & 1;
    case K_RAW: return (f.r >> 4) & 1;
    default: return 0;
    }
}
static inline uint32_t f_flags(Flags f)
{
    return f_cf(f) | 2u | f_pf(f) << 2 | f_af(f) << 4 | f_zf(f) << 6 | f_sf(f) << 7 | f_of(f) << 11;
}

/* Conditions. The common cases fold to one comparison when the flag kind is known. */
static inline int cc_o(Flags f) { return f_of(f); }
static inline int cc_b(Flags f)
{
    if ((f.op & 15) == K_SUB && f.c == 0) return (f.a & f_mask(f.op)) < (f.b & f_mask(f.op));
    return f_cf(f);
}
static inline int cc_e(Flags f) { return f_zf(f); }
static inline int cc_be(Flags f)
{
    if ((f.op & 15) == K_SUB && f.c == 0) return (f.a & f_mask(f.op)) <= (f.b & f_mask(f.op));
    return f_cf(f) | f_zf(f);
}
static inline int cc_s(Flags f) { return f_sf(f); }
static inline int cc_p(Flags f) { return f_pf(f); }
static inline int32_t f_sext(Flags f, uint32_t v)
{
    return f.op & SZ1 ? (int8_t)v : f.op & SZ2 ? (int16_t)v : (int32_t)v;
}
static inline int cc_l(Flags f)
{
    if ((f.op & 15) == K_SUB && f.c == 0) return f_sext(f, f.a) < f_sext(f, f.b);
    if ((f.op & 15) == K_RES) return f_sf(f) != ((f.c >> 1) & 1);
    return f_sf(f) != f_of(f);
}
static inline int cc_le(Flags f)
{
    if ((f.op & 15) == K_SUB && f.c == 0) return f_sext(f, f.a) <= f_sext(f, f.b);
    return f_zf(f) | (f_sf(f) != f_of(f));
}

/* x87. The registers are doubles: the original runs with the precision control at 53 bits (CW 0x27f). */
#define ST(i) fpu.st[(fpu.top + (i)) & 7]
static inline void fpush(double v) { fpu.top = (fpu.top - 1) & 7; fpu.st[fpu.top] = v; }
static inline void fpop(void) { fpu.top = (fpu.top + 1) & 7; }
static inline void fcompare(double a, double b)
{
    uint16_t c = 0;
    if (a != a || b != b) c = 0x4500;        /* C3 C2 C0: unordered */
    else if (a < b) c = 0x0100;               /* C0 */
    else if (a == b) c = 0x4000;              /* C3 */
    fpu.sw = (uint16_t)((fpu.sw & ~0x4700) | c);
}
static inline uint32_t fstatus(void) { return (uint32_t)((fpu.sw & ~0x3800) | ((fpu.top & 7) << 11)); }
static inline double fround(double v)
{
    switch ((fpu.cw >> 10) & 3) {
    case 0: return nearbyint(v);              /* host rounding is to nearest even */
    case 1: return floor(v);
    case 2: return ceil(v);
    default: return trunc(v);
    }
}
static inline int32_t fist32(double v)
{
    double r = fround(v);
    return (r != r || r >= 2147483648.0 || r < -2147483648.0) ? INT32_MIN : (int32_t)r;
}
static inline int16_t fist16(double v)
{
    double r = fround(v);
    return (r != r || r >= 32768.0 || r < -32768.0) ? INT16_MIN : (int16_t)r;
}
static inline int64_t fist64(double v)
{
    double r = fround(v);
    return (r != r || r >= 9223372036854775808.0 || r < -9223372036854775808.0) ? INT64_MIN : (int64_t)r;
}
void fpu_init(void);

/* Division: the CPU raises #DE on a zero divisor or a quotient that does not fit. */
void rt_divide_error(uint32_t at);
static inline void rt_div32(uint32_t at, uint32_t *eax, uint32_t *edx, uint32_t d)
{
    uint64_t n = (uint64_t)*edx << 32 | *eax;
    if (!d || n / d > 0xffffffffu) rt_divide_error(at);
    *eax = (uint32_t)(n / d); *edx = (uint32_t)(n % d);
}
static inline void rt_idiv32(uint32_t at, uint32_t *eax, uint32_t *edx, uint32_t d)
{
    int64_t n = (int64_t)((uint64_t)*edx << 32 | *eax);
    if (!d || (n == INT64_MIN && (int32_t)d == -1)) rt_divide_error(at);
    int64_t q = n / (int32_t)d;
    if (q != (int32_t)q) rt_divide_error(at);
    *eax = (uint32_t)q; *edx = (uint32_t)(n % (int32_t)d);
}
static inline void rt_div16(uint32_t at, uint32_t *eax, uint32_t *edx, uint32_t d)
{
    uint32_t n = (*edx & 0xffffu) << 16 | (*eax & 0xffffu);
    d &= 0xffffu;
    if (!d || n / d > 0xffffu) rt_divide_error(at);
    *eax = (*eax & 0xffff0000u) | (n / d); *edx = (*edx & 0xffff0000u) | (n % d);
}
static inline void rt_idiv16(uint32_t at, uint32_t *eax, uint32_t *edx, uint32_t d)
{
    int32_t n = (int32_t)((*edx & 0xffffu) << 16 | (*eax & 0xffffu));
    int32_t s = (int16_t)d;
    if (!s) rt_divide_error(at);
    int32_t q = n / s;
    if (q != (int16_t)q) rt_divide_error(at);
    *eax = (*eax & 0xffff0000u) | ((uint32_t)q & 0xffffu); *edx = (*edx & 0xffff0000u) | ((uint32_t)(n % s) & 0xffffu);
}
static inline void rt_div8(uint32_t at, uint32_t *eax, uint32_t d)
{
    uint32_t n = *eax & 0xffffu;
    d &= 0xffu;
    if (!d || n / d > 0xffu) rt_divide_error(at);
    *eax = (*eax & 0xffff0000u) | (n % d) << 8 | (n / d);
}
static inline void rt_idiv8(uint32_t at, uint32_t *eax, uint32_t d)
{
    int32_t n = (int16_t)*eax, s = (int8_t)d;
    if (!s) rt_divide_error(at);
    int32_t q = n / s;
    if (q != (int8_t)q) rt_divide_error(at);
    *eax = (*eax & 0xffff0000u) | ((uint32_t)(n % s) & 0xffu) << 8 | ((uint32_t)q & 0xffu);
}

/* rep movs and rep stos. With the direction flag clear and no forward overlap, a block copy. */
static inline void rt_rep_movs(uint8_t *m, uint32_t size, uint32_t *ecx, uint32_t *esi, uint32_t *edi)
{
    uint32_t n = *ecx, bytes = n * size;
    if (!n) return;
    if (!cpu.df && !(*edi > *esi && *edi < *esi + bytes)) {
        memmove(m + *edi, m + *esi, bytes);
        *esi += bytes; *edi += bytes;
    } else {
        uint32_t step = cpu.df ? -size : size;
        for (; n; n--, *esi += step, *edi += step) memmove(m + *edi, m + *esi, size);
    }
    *ecx = 0;
}
static inline void rt_rep_stos(uint8_t *m, uint32_t size, uint32_t *ecx, uint32_t *edi, uint32_t eax)
{
    uint32_t n = *ecx;
    uint32_t step = cpu.df ? -size : size;
    if (size == 1 && !cpu.df) { memset(m + *edi, (int)(eax & 0xff), n); *edi += n; }
    else for (; n; n--, *edi += step) memcpy(m + *edi, &eax, size);
    *ecx = 0;
}

/* Calls through an address: generated functions, replacements, import stubs and host functions. */
void rt_call(uint32_t target);
void rt_jump(uint32_t target);
RtFn rt_lookup(uint32_t target);

/* Unwinding of host frames. longjmp and a jump to an exception continuation run the rest of an older
 * frame of the original as a new host call (a resume entry). When it returns, every host frame of a
 * generated function whose entry esp lies below the new esp is gone and returns at once. */
extern int rt_unwinding;
static inline int rt_unwound(uint32_t esp0)
{
    if (esp0 < cpu.esp) return 1;
    rt_unwinding = 0;
    return 0;
}
#define RT_AFTER_CALL() do { if (__builtin_expect(rt_unwinding, 0) && rt_unwound(esp0)) return; } while (0)

/* fnsave and frstor: the 108-byte x87 state. */
void rt_fnsave(uint8_t *m, uint32_t a);
void rt_frstor(uint8_t *m, uint32_t a);

/* Stops the program. */
void rt_unimpl(uint32_t at, const char *text) __attribute__((noreturn));
void rt_fell_off(uint32_t at) __attribute__((noreturn));
void rt_bad_jump(uint32_t at, uint32_t target) __attribute__((noreturn));
void rt_missing_import(const char *name) __attribute__((noreturn));
void rt_fatal(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

/* Replacement functions. RT_REPLACE marks a function of the original that the runtime replaces; the
 * recompiler reads these lines to leave it out. RT_IMPORT is the same for an import, by name. */
#define RT_REPLACE(addr, name) void f_##addr(void)
#define RT_IMPORT(name) void imp_##name(void)
/* RT_WRAP marks a function that the runtime wraps: the recompiler writes the translation as o_<addr>,
 * and every call goes to the runtime's f_<addr>, which can change the arguments and call o_<addr>. */
#define RT_WRAP(addr, name) void f_##addr(void)

/* The size of the screen that the original draws: 800x600 in classic mode, 1280x800 in widescreen
 * mode. The constants of tools/recomp/widescreen.py read these. */
extern uint32_t rt_screen_w, rt_screen_h;

/* The start year of the game (0.6.0 slice 11, src/recomp_rt/win/season.c): 2001 in the base game. The year
 * sites of tools/recomp/seasons.py read it in place of the constants 2001, 2002 and 2003. */
extern uint32_t rt_season_year;

/* The league load's row arrays (win/league_load.c, tools/recomp/league_load.py at 0x68c898): 0 when the read
 * of a row's result array failed only because the array has 0 bytes (a league of one club), else 1. */
uint32_t rt_league_row_read_failed(uint32_t league);
/* The ban release's nation index (win/discipline_ban.c, tools/recomp/discipline_ban.py at 0x556dcf): the entry
 * of the staff id under any nation, 0 for none, or 1 when the fix is off (the original's box). */
uint32_t rt_discipline_ban_entry(uint32_t discipline, uint32_t staff);
/* The rule-set load's Oceania body (win/season_intl.c, tools/recomp/discipline_ban.py at 0x55deb6): 1 when a
 * season game moved a selected Australia to Asia, so its saves hold the body. */
uint32_t rt_discipline_oceania_body(void);
/* The ruling body's held ban (win/discipline_ban.c, tools/recomp/discipline_ban.py at 0x7e833c): 1 goes on without
 * the box ruling_body..cpp 661, 0 when the fix is off. */
uint32_t rt_ruling_body_held(uint32_t body, uint32_t staff);
/* The loan's ban hold and restore (win/season_new_cups.c, tools/recomp/discipline_ban.py at 0x55647d, 0x55671e): 1
 * when a domestic body of one nation applies at a club of the other through the All-Ireland cup. */
uint32_t rt_ban_nation_applies(uint32_t club, uint32_t body);
/* The club count of a league in a season game (0.6.0 slice 18, src/recomp_rt/win/season_sizes.c): the
 * patched stores of tools/recomp/league_sizes.py in create_table read it in place of the class's constant. */
uint32_t rt_league_clubs(uint32_t obj, uint32_t original);
/* The club count of the German Regional object, the 3. Liga from 2008/09 (0.6.0 slice 20,
 * win/season_divisions.c): the patched stores of tools/recomp/new_divisions.py at 0x5dd801 and 0x5dd809. */
uint32_t rt_ger_regional_clubs(uint32_t obj, uint32_t original);
/* Liga 3 (slice 20): the series size of the Portuguese Segunda B object, 0x7d2412, 0x7d2965, 0x7d29ff, 0x7d2ab2. */
uint32_t rt_por_group_clubs(uint32_t obj, uint32_t original);
uint32_t rt_por_stage_clubs(uint32_t obj, uint32_t original);
/* The Turkish Category B in three groups with Summer 2002 (win/season_tur_groups.c): the table of TurSecondB
 * (0x9002cd, 0x9002d2) and its group stages (0x900812, 0x90089d, 0x900932, 0x9009e4), tools/recomp/tur_groups.py. */
uint32_t rt_tur_table_clubs(uint32_t obj, uint32_t original);
uint32_t rt_tur_table_bytes(uint32_t obj, uint32_t original);
uint32_t rt_tur_stage_clubs(uint32_t obj, uint32_t original);
uint32_t rt_tur_stage_bytes(uint32_t obj, uint32_t original);
/* 1 when the Portuguese Cup takes a club with no division (0x7ce37d): a Portuguese club in a game with Liga 3. */
uint32_t rt_por_cup_none(uint32_t club);
/* The date count of a French league's template for the clubs that its division holds (slice 18). */
uint32_t rt_league_dates(uint32_t obj, uint32_t original);
/* The room for lower clubs of a cup with a fixed entrant count (6 bytes a place): the entrants less the clubs
 * of the top divisions of keys a, b and c (key ids, 0 for none). The original in the base game (slice 18).
 * rt_cup_add: the value to add to base so that the sum is that count (FraCup). */
uint32_t rt_cup_room(uint32_t original, uint32_t entrants, uint32_t key_a, uint32_t key_b, uint32_t key_c);
uint32_t rt_cup_add(uint32_t original, uint32_t base, uint32_t entrants, uint32_t key_a, uint32_t key_b);
uint32_t rt_fra_lge_first(uint32_t original, uint32_t edx);   /* FraLgeCup first loop limit (0x5bd244) */
uint32_t rt_fa_cup_takes(uint32_t esp);   /* EngFaCup places by pool (0x56ffe4) */
uint32_t rt_fra_cfa_lower_cap(uint32_t original);              /* FraCfa lower club cap (0x5b92e8) */
uint32_t rt_ita_cup_first_position(uint32_t original);   /* ItaCup second loop start (0x63fb16) */
uint32_t rt_cup_places_list(uint32_t list, uint32_t bytes);  /* provincial and Polish cup lists, cleared */
uint32_t rt_promote_expected(uint32_t league, uint32_t up, uint32_t original);  /* ItaSerA chain counts (0x642476) */
/* Italy's Serie C of three groups (0.6.0 slice 20b, win/season_divisions_ita.c, tools/recomp/new_divisions.py). */
uint32_t rt_ita_table_bytes(uint32_t obj, uint32_t original);
uint32_t rt_ita_c2a_clubs(uint32_t obj, uint32_t original);
uint32_t rt_ita_c2_end_clubs(uint32_t obj, uint32_t original);
uint32_t rt_ita_c2_none(uint32_t group);
uint32_t rt_ita_ccup_group(uint32_t cup, uint32_t original);
uint32_t rt_ita_ccup_days(uint32_t cup, uint32_t original);
uint32_t rt_ita_ccup_seconds(uint32_t cup, uint32_t original);
uint32_t rt_ita_ccup_first_seed(uint32_t cup, uint32_t original);
uint32_t rt_ita_ccup_seed_slot(uint32_t cup, uint32_t list, uint32_t first);
uint32_t rt_ita_tier_c2(uint32_t c2a);
uint32_t rt_ita_tier_last(uint32_t obj);
uint32_t rt_ita_cup_c1b(uint32_t id, uint32_t c1b);
uint32_t rt_ita_cup_fill(uint32_t esp, uint32_t count, uint32_t position);
uint32_t rt_ita_start_c2(uint32_t c2a);
/* The offset of the knockout stage in the stage array of a Champions League object (0.6.0 slice 22,
 * win/season_uefa.c): 0x30 (stage 12) in the original's format, 0x20 (stage 8) in the format with one group
 * stage. The sites of tools/recomp/uefa.py read it. */
uint32_t rt_uefa_cl_ko_offset(uint32_t cl);
/* The Intertoto Cup winners in the UEFA Cup (0.6.0 slice 22, win/season_uefa_cup.c): the first slot counted
 * back from the UEFA Cup's slot count (0x1b in the original) and the number of winners (3). */
uint32_t rt_uefa_uic_back(uint32_t uc, uint32_t uic);
uint32_t rt_uefa_uic_winners(uint32_t uc, uint32_t uic);

/* The season text "2009/10" in place of the original's "2009/0" (0.6.0, Q358; win/season.c). The patches of
 * tools/recomp/seasons.py at the two season formatters of key_nation.cpp read these: the divisor of the second
 * year (10 in the original) and the format string (0x9eb024 "%d/%d"). With RECOMP_SEASON_CODE=off they give
 * the original's values. */
uint32_t rt_season_text_mod(uint32_t original);
uint32_t rt_season_text_format(uint32_t original);

/* The club competitions outside UEFA (0.6.0 slice 26, src/recomp_rt/win/season_kind_club_world.c): in a season
 * game the Club World Championship plays every year and draws on 1 July, the Oceania Champions Cup plays every
 * year, and the Copa Mercosur does not start itself while it waits. The sites of tools/recomp/seasons.py read
 * these. In the base game each gives the original's value. */
uint32_t rt_club_world_annual(void);
uint32_t rt_mls_round_flags(uint32_t flags);   /* season_mls.c */
uint32_t rt_club_world_step(uint32_t original);
uint32_t rt_oceania_step(uint32_t original);
uint32_t rt_club_world_month(uint32_t original);
uint32_t rt_club_world_day(uint32_t original);
uint32_t rt_club_world_waits(uint32_t obj);
/* The CONCACAF Champions Cup's club count of the edition of `year` (slice 26c, win/season_club_cups.c): 8 in the
 * base game and with no row. */
uint32_t rt_concacaf_clubs(uint32_t year, uint32_t original);
/* International competitions (0.6.0 slice 27, win/season_intl.h): the edition year of a finals class at its
 * constructor and its start_new_season, the golden goal bit of a knockout round, and the World Cup holder of the
 * qualifying sections and of slot 0. The sites of tools/recomp/seasons.py call them. Each gives the original's
 * value in the base game. */
uint32_t rt_intl_first_year(uint32_t cls, uint32_t year);
uint32_t rt_intl_next_year(uint32_t cls, uint32_t year, uint32_t step);
uint32_t rt_intl_round_flags(uint32_t cls, uint32_t year, uint32_t flags);
uint32_t rt_intl_wc_holder(uint32_t holder);
uint32_t rt_intl_wc_holder_slot(uint32_t year);
uint32_t rt_intl_oceania_left_out(uint32_t id);
uint32_t rt_intl_euro_no_host(void);
/* The European Championship of 24 teams (win/season_intl_euro.c): the team count (16), the knockout stage index
 * (3) and a new stage array after the step of start_new_season. Each gives the original's value for an edition
 * of 16 teams and in the base game. */
uint32_t rt_intl_euro_teams(uint32_t self, uint32_t original);
uint32_t rt_intl_euro_ko(uint32_t self, uint32_t original);
void rt_intl_euro_stages(uint32_t self);
/* The World Cup of 48 teams (win/season_wc48.c, 0.6.0 slice 29): the team count (32), the knockout stage index
 * (7) and new stage and slot arrays after the step of start_new_season. Each gives the original's value for an
 * edition of 32 teams and in the base game. */
uint32_t rt_intl_wc_teams(uint32_t self, uint32_t original);
uint32_t rt_intl_wc_ko(uint32_t self, uint32_t original);
void rt_intl_wc_stages(uint32_t self);
/* World Cup qualifying from the 2026 cycle (win/season_wcq26.c, 0.6.0 slice 29). Each gives the original's
 * value outside its gate (a section of a campaign for a World Cup of 48 teams in a season game). */
uint32_t rt_wcq26_south_america_direct(uint32_t self, uint32_t original);
uint32_t rt_wcq26_concacaf_exempt(uint32_t self, uint32_t holder);
uint32_t rt_wcq26_concacaf_hosts(uint32_t self, uint32_t exempt);
uint32_t rt_wcq26_africa_first(uint32_t self, uint32_t original);
uint32_t rt_wcq26_africa_groups(uint32_t self, uint32_t original);
void rt_wcq26_africa_format(uint32_t self);
uint32_t rt_wcq26_asia_direct(uint32_t self, uint32_t original);
uint32_t rt_wcq26_asia_playoff_row(uint32_t self, uint32_t original);
uint32_t rt_wcq26_asia_stages(uint32_t self, uint32_t original);
void rt_wcq26_asia_format(uint32_t self);
uint32_t rt_wcq26_oceania_stages(uint32_t self, uint32_t original);
void rt_wcq26_europe_format(uint32_t self);
uint32_t rt_wcq26_europe_index(uint32_t self, uint32_t original);
/* The African Cup of Nations and the Asian Cup of 24 teams (win/season_intl_cups.c): the team count (16, 12),
 * the group count of the draw (4, 3), the knockout stage index (3, 2) and a new stage array after the step of
 * start_new_season. Each gives the original's value for another edition, another class and the base game. */
uint32_t rt_intl_cup_teams(uint32_t self, uint32_t original);
uint32_t rt_intl_cup_groups(uint32_t self, uint32_t original);
uint32_t rt_intl_cup_ko(uint32_t self, uint32_t original);
void rt_intl_cup_stages(uint32_t self);
/* The Copa America of 16 teams (win/season_intl_copa.c): the knockout stage index (2), the group count (3) and a
 * new stage array after the step of start_new_season. Each gives the original's value for an edition of 12
 * teams and in the base game. */
uint32_t rt_intl_copa_ko(uint32_t self, uint32_t original);
uint32_t rt_intl_copa_groups(uint32_t self, uint32_t original);
void rt_intl_copa_stages(uint32_t self);
/* Euro qualifying places (win/season_intl_euroq.c): the places and stage indices of the format of the Euro year
 * (Euro 2016: 9 groups, top two, best third, play-off of 8 thirds; Euro 2020 on: 10 groups, top two). Each
 * gives the original's value in the base game and for a Euro of 16 teams. */
void rt_euroq_rules(uint32_t self);
uint32_t rt_euroq_hosts(uint32_t self, uint32_t original);
uint32_t rt_euroq_ranked_place(uint32_t self, uint32_t original);
uint32_t rt_euroq_direct(uint32_t self, uint32_t original);
uint32_t rt_euroq_no_ranking(uint32_t self);
uint32_t rt_euroq_group_size(uint32_t self, uint32_t group, uint32_t original);
uint32_t rt_euroq_ten_groups(uint32_t self);
uint32_t rt_euroq_skip_2002(uint32_t self);
void rt_euroq_log(uint32_t euro, uint32_t club, uint32_t stage, uint32_t playoff);
/* World Cup qualifying 2006 to 2022 (win/season_intl_qual.c): the places of each section and the
 * inter-confederation play-offs. Each gives the original's value outside a season game and the cycles 2006 to
 * 2022. */
uint32_t rt_wcq_europe_playoff(uint32_t self, uint32_t exempt);
uint32_t rt_wcq_europe_no_asia(uint32_t self);
void rt_wcq_asia_format(uint32_t self);
uint32_t rt_wcq_asia_partner(uint32_t self, uint32_t asian_at, uint32_t original);
uint32_t rt_wcq_oceania_pair(uint32_t self, uint32_t teams, uint32_t found);
uint32_t rt_wcq_concacaf_playoff(uint32_t self);
uint32_t rt_wcq_south_america_hosts(uint32_t self, uint32_t hosts);
void rt_wcq_africa_format(uint32_t self, uint32_t pool);
/* The UEFA Nations League (0.6.0 slice 28, win/season_nl.c): the group stages that the draw of
 * EuroChampQual::update_for_date makes (8 at 0x57ff34). The original's value for every other object. */
uint32_t rt_nl_group_stages(uint32_t self, uint32_t original);
/* 0.6.0 slice 28 part b: the year sites of copa_america (0x5e06fb, 0x5e1590) for the Copa America and the Gold
 * Cup (win/season_gold_cup.c). */
uint32_t rt_copa_first_year(uint32_t self, uint32_t year);
uint32_t rt_copa_next_year(uint32_t self, uint32_t year, uint32_t step);

/* The reserve of blank staff rows that a database load makes (0.6.0, src/recomp_rt/win/staff_reserve.c):
 * 23,000 in the original. The sites of tools/recomp/staff_reserve.py read it. rt_staff_reserve_size() sets
 * it for the load that runs (the first sizing site) and returns it. */
extern uint32_t rt_staff_reserve;
uint32_t rt_staff_reserve_size(void);

/* Full player search (#57, src/recomp_rt/win/playersearch.c): 1 when the option is on. The replaced jumps
 * at 0x7afc8d, 0x7afc9d and 0x7afcdf in tools/recomp/widescreen.py ask it. */
uint32_t rt_full_player_search(uint32_t screen);
/* The limit of the list with the option on (playersearch.c): the sites 0x7afbb3, 0x7afdda, 0x7afe5e, 0x7afe6b
 * and 0x7af467 in tools/recomp/widescreen.py. */
void rt_full_player_search_reset(void);
void rt_full_player_search_skip(void);
uint32_t rt_full_player_search_slot(uint32_t screen);
uint32_t rt_full_player_search_replaced(void);
uint32_t rt_full_player_search_shown(uint32_t passed);

/* Attribute groups (#43, src/recomp_rt/win/groups.c): the cells of the player profile's attribute table
 * and their place. The constants of tools/recomp/attribute_groups.py read these. */
uint32_t rt_groups_cells(uint32_t esp);
uint32_t rt_groups_table(void);
uint32_t rt_groups_count(void);

/* Arguments and return for replacements: esp points at the return address. */
static inline uint32_t rt_arg(int i) { return rd32(g_mem, cpu.esp + 4 + 4 * (uint32_t)i); }
static inline void rt_ret_cdecl(uint32_t v) { cpu.eax = v; cpu.esp += 4; }
static inline void rt_ret_stdcall(uint32_t v, int nargs) { cpu.eax = v; cpu.esp += 4 + 4 * (uint32_t)nargs; }

#define RT_RESUME 1u      /* entered by a jump into the middle of a function; see rt_unwinding */
typedef struct { uint32_t addr; RtFn fn; const char *name; uint32_t flags; } RtEntry;
extern const RtEntry rt_table[];
extern const uint32_t rt_table_size;

#endif
