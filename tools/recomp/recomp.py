"""Static recompiler for cm0102.exe: x86 to C.

Usage:
    python tools/recomp/recomp.py --exe <cm0102.exe> --out <build folder> [--entries 0x528540,...]
        [--functions <file of entry addresses>] [--all] [--no-follow] [--compile <driver.c>]

--entries     the functions to start from. Their direct callees and tail jumps follow unless --no-follow.
--functions   the known function entries: one hex address per line, or the JSON that REA list_procedures
              writes (its normalized_result). A jump to one of them is a tail call. The indirect call
              table holds every translated function.
--all         translate every function of --functions, and every function that a pointer in the data
              sections names.
--runtime     a C file of replacements (RT_REPLACE, RT_WRAP, RT_IMPORT) that goes with the core runtime
              (src/recomp_rt/*.c). Give it once per file.
--shards      write the functions into this many files gen/fn_000.c ... so that a build system knows
              their names before the run.
--compile     compile the generated C with the runtime and this driver into <out>/program.

The generated C is a translation of the original. It goes into the build folder and is not committed.
"""
import argparse
import collections
import concurrent.futures
import io
import json
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.setrecursionlimit(50000)
from analysis import Analyzer  # noqa: E402
from pe import Image  # noqa: E402
from callsites import CALL_SITES, SITES  # noqa: E402
import callbacks  # noqa: E402
from translate import GUARDS_SEEN, PATCHED, SITES_SEEN, FunctionWriter  # noqa: E402
from crashguards import GUARDS  # noqa: E402
from widescreen import INSNS, PATCHES, TRUE_WIDTH  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
RUNTIME = os.path.join(ROOT, "src", "recomp_rt")
GROUP = 64          # functions per C file


# Calls after which the code can be entered again at the return address: _setjmp3 returns a second
# time through longjmp (0x94791c), which jumps to the saved return address.
RESUME_AFTER_CALLS = {0x947998}


def runtime_sources(extra):
    core = [os.path.join(RUNTIME, n) for n in sorted(os.listdir(RUNTIME)) if n.endswith(".c")]
    core.append(os.path.join(RUNTIME, "host", "posix.c"))     # --compile builds on the Mac
    return core + [os.path.abspath(p) for p in extra]


def runtime_symbols(sources):
    """The functions and imports that the runtime replaces, from its RT_REPLACE and RT_IMPORT lines."""
    replaced, imports, wrapped = {}, set(), {}
    for path in sources:
        text = open(path).read()
        for m in re.finditer(r"^RT_REPLACE\(([0-9a-f]{8}),\s*(\w+)\)", text, re.M):
            replaced[int(m.group(1), 16)] = m.group(2)
        for m in re.finditer(r"^RT_WRAP\(([0-9a-f]{8}),\s*(\w+)\)", text, re.M):
            wrapped[int(m.group(1), 16)] = m.group(2)
        for m in re.finditer(r"^RT_IMPORT\((\w+)\)", text, re.M):
            imports.add(m.group(1))
    return replaced, imports, wrapped


def data_pointers(image, known):
    """Addresses of code that the data sections hold (vtables, initializer tables, callbacks) and
    that are not in the known list. Only values where a function can start count: aligned to 16 or
    after padding or a ret. Other values are text that happens to look like an address."""
    found = set()
    for name, va, vsize, rptr, rsize, flags in image.sections:
        if image.is_code(va) or not rsize:
            continue
        data = image.read(va, min(vsize or rsize, rsize))
        for off in range(0, len(data) - 3, 4):
            v = int.from_bytes(data[off:off + 4], "little")
            if not image.is_code(v) or v in known:
                continue
            if function_start(image, v, ()):
                found.add(v)
    return found


def function_start(image, v, known):
    """Whether a function can start at v: a known entry, or aligned to 16, or after padding or a ret."""
    return v in known or v % 16 == 0 or image.u8(v - 1) in (0x90, 0xCC, 0xC3) \
        or (image.u8(v - 3) == 0xC2 and image.u8(v - 1) == 0)


def read_functions(path):
    text = open(path).read()
    if text.lstrip().startswith("{"):
        rows = json.loads(text)["normalized_result"]
        return sorted({int(r["address"], 16) for r in rows if r["address"].startswith("0x")})
    return [int(x, 16) for x in text.split()]


class ChangedFile(io.StringIO):
    """A file written only when its text changes, so that a build does not compile it again."""
    def __init__(self, path):
        super().__init__()
        self.path = path

    def __exit__(self, *exc):
        text = self.getvalue()
        if not os.path.exists(self.path) or open(self.path).read() != text:
            with open(self.path, "w") as out:
                out.write(text)
        return super().__exit__(*exc)


def fname(addr):
    return "f_%08x" % addr


def oname(addr):
    return "o_%08x" % addr


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--entries", default="")
    ap.add_argument("--functions")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--no-follow", action="store_true")
    ap.add_argument("--compile")
    ap.add_argument("--runtime", action="append", default=[])
    ap.add_argument("--shards", type=int, default=0)
    ap.add_argument("--stamp", help="a file to touch when the run ends (for a build system)")
    ap.add_argument("--opt", default="-O1")
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    args = ap.parse_args(argv)

    t_start = time.time()
    image = Image(args.exe)
    known = read_functions(args.functions) if args.functions else []
    entries = [int(x, 16) for x in args.entries.split(",") if x]
    if args.all:
        entries += known
    sources = runtime_sources(args.runtime)
    replaced, imported, wrapped = runtime_symbols(sources)
    # Function boundaries: the known list and the given entries. Entries found below (data pointers,
    # callbacks, resume points) are translated too, but do not cut the function they lie in.
    an = Analyzer(image, set(known) | set(entries))
    known_set = set(known)
    if args.all:
        entries += sorted(data_pointers(image, known_set))

    gen = os.path.join(args.out, "gen")
    os.makedirs(gen, exist_ok=True)
    stats = collections.Counter()
    unhandled = collections.Counter()
    unhandled_at = []
    features = collections.Counter()
    mnemonics = collections.Counter()
    referenced = set()

    def names(addr):
        referenced.add(addr)
        return fname(addr)

    # Each function is translated as soon as it is analysed, and its decoded instructions are then dropped (from the
    # Function and from the analyser's decode cache): the capstone instructions of the whole game held at once took
    # 8.3 to 8.9 GB, more than the 8 GiB of the release's Linux container (gate (d), the dry run of a2330b34 was
    # "Killed" at recomp.stamp). A Function keeps its calls, tails, returns and bad, which the analysis of other
    # functions reads (Analyzer.noreturn); the translation of a function reads only its own instructions, so the C
    # text does not change. The shards are written from the bodies in address order, as before.
    bodies = {}
    per = {}
    t_translate = 0.0

    def translate(e, f):
        nonlocal t_translate
        t = time.time()
        # A wrapped function: the translation is o_<address>, and the runtime's f_<address> calls it.
        w = FunctionWriter(f, image, names, own_name=oname(e) if e in wrapped else None)
        bodies[e] = w.write()
        # The figures of the report, added up in address order after the loop (the order of the old run).
        per[e] = (len(f.insns), w.handled, len(f.bad), len(f.tables),
                  collections.Counter(insn.mnemonic for insn in f.insns.values()), w.unhandled, w.features)
        for a in f.insns:
            an.forget(a)
        f.insns = {}
        t_translate += time.time() - t

    funcs = {}
    resume = set()
    work = list(dict.fromkeys(entries))
    while work:
        e = work.pop()
        if e in funcs or e in replaced:
            continue
        if not image.is_code(e):
            print(f"skipping {e:#x}: not in .text", file=sys.stderr)
            continue
        f = an.function(e)
        funcs[e] = f
        for a, insn in f.insns.items():
            if args.all and insn.mnemonic in ("push", "mov"):
                # An address of code as a value: a callback, for example the constructor that the
                # vector constructor iterator calls.
                for op in insn.operands:
                    if op.type == 2 and image.is_code(op.imm & 0xFFFFFFFF) and op.imm & 0xFFFFFFFF not in funcs \
                            and function_start(image, op.imm & 0xFFFFFFFF, known_set):
                        work.append(op.imm & 0xFFFFFFFF)
            if insn.mnemonic == "call" and insn.operands[0].type == 2 \
                    and insn.operands[0].imm & 0xFFFFFFFF in RESUME_AFTER_CALLS:
                resume.add(a + insn.size)
                work.append(a + insn.size)
        if not args.no_follow:
            work.extend(t for t in (f.calls | f.tails) if t not in funcs)
        translate(e, f)
    for e in sorted(per):
        n, handled, bad, tables, mn, unh, feats = per[e]
        stats["functions"] += 1
        stats["instructions"] += n
        stats["handled"] += handled
        stats["decode_stops"] += bad
        stats["jump_tables"] += tables
        mnemonics.update(mn)
        for a, text, why in unh:
            unhandled[text.split()[0]] += 1
            unhandled_at.append((a, text, why))
        for x in feats:
            features[x] += 1
    t_analysis = time.time() - t_start - t_translate
    t0 = time.time()

    # Functions that are called but neither translated nor replaced (with --no-follow).
    missing = sorted(a for a in referenced if a not in funcs and a not in replaced)
    files = []
    order = sorted(bodies)
    if args.shards:
        per = -(-len(order) // args.shards)
        chunks = [order[i * per:(i + 1) * per] for i in range(args.shards)]
        paths = [os.path.join(gen, "fn_%03d.c" % i) for i in range(args.shards)]
    else:
        chunks = [order[g:g + GROUP] for g in range(0, len(order), GROUP)]
        paths = [os.path.join(gen, "fn_%06x.c" % c[0]) for c in chunks]
    for chunk, path in zip(chunks, paths):
        with ChangedFile(path) as out:
            out.write('#include "rt.h"\n#include "functions.h"\n\n')
            for e in chunk:
                out.write(bodies[e])
                out.write("\n")
        files.append(path)

    with ChangedFile(os.path.join(gen, "functions.h")) as out:
        out.write("/* Generated by tools/recomp. */\n")
        for a in sorted(set(funcs) | set(referenced) | set(replaced)):
            out.write(f"void {fname(a)}(void);\n")
        for a in sorted(set(wrapped) & set(funcs)):
            out.write(f"void {oname(a)}(void);\n")
        stubs = sorted(set(n for _, n, _ in image.imports.values()))
        for n in stubs:
            out.write(f"void imp_{n}(void);\n")

    # The table of addresses, the import address table and stubs for missing pieces.
    with ChangedFile(os.path.join(gen, "table.c")) as out:
        out.write('#include "rt.h"\n#include "rt_host.h"\n#include "functions.h"\n\n')
        for a in missing:
            out.write(f"void {fname(a)}(void) {{ rt_fatal(\"function %#x was not recompiled\", 0x{a:x}u); }}\n")
        for n in stubs:
            if n not in imported:
                out.write(f"void imp_{n}(void) {{ rt_missing_import(\"{n}\"); }}\n")
        rows = {}
        for a in set(funcs) | set(replaced) | set(missing):
            rows[a] = (fname(a), "")
        for iat, (dll, n, stub) in image.imports.items():
            rows[stub] = (f"imp_{n}", n)
        out.write("\nconst RtEntry rt_table[] = {\n")
        for a in sorted(rows):
            fn, n = rows[a]
            flags = "RT_RESUME" if a in resume else "0"
            out.write(f"    {{0x{a:x}u, {fn}, \"{n or fn}\", {flags}}},\n")
        out.write("};\n")
        out.write(f"const uint32_t rt_table_size = {len(rows)};\n\n")
        out.write("const RtIat rt_iat[] = {\n")
        for iat, (dll, n, stub) in sorted(image.imports.items()):
            out.write(f"    {{0x{iat:x}u, 0x{stub:x}u}},\n")
        out.write("};\n")
        out.write(f"const uint32_t rt_iat_size = {len(image.imports)};\n")
        # The return addresses of the text width calls that get screen pixels (widescreen.py TRUE_WIDTH).
        sites = sorted(a + 5 for a in TRUE_WIDTH)
        out.write("\nconst uint32_t rt_true_width_returns[] = {" + ", ".join("0x%xu" % a for a in sites) + "};\n")
        out.write(f"const uint32_t rt_true_width_count = {len(sites)};\n")
        # The return addresses of the call sites that the runtime tests (callsites.py).
        out.write("\n")
        for name, (a, _) in sorted(CALL_SITES.items()):
            out.write(f"const uint32_t rt_site_{name} = 0x{a + 5:x}u;\n")
    files.append(os.path.join(gen, "table.c"))
    t_translate += time.time() - t0
    if args.all:
        lost = sorted((set(PATCHES) | set(INSNS) | TRUE_WIDTH) - PATCHED)
        if lost:
            raise SystemExit("widescreen.py: no translated instruction at " + ", ".join("%#x" % a for a in lost))
        lost = sorted(set(GUARDS) - GUARDS_SEEN)
        if lost:
            raise SystemExit("crashguards.py: no translated instruction at " + ", ".join("%#x" % a for a in lost))
        lost = sorted(set(SITES) - SITES_SEEN)
        if lost:
            raise SystemExit("callsites.py: no translated instruction at " + ", ".join("%#x" % a for a in lost))
        lost = sorted(set(wrapped) - set(funcs))
        if lost:
            raise SystemExit("RT_WRAP of a function that was not translated: " + ", ".join("%#x" % a for a in lost))
        bad = callbacks.check(image, set(funcs), os.path.join(RUNTIME, "win"))
        if bad:
            raise SystemExit("callbacks.py: " + "; ".join(bad))

    report = {
        "functions": stats["functions"],
        "instructions": stats["instructions"],
        "handled": stats["handled"],
        "handled_share": stats["handled"] / max(1, stats["instructions"]),
        "mnemonics": len(mnemonics),
        "unhandled": dict(unhandled.most_common()),
        "unhandled_at": [("%#x" % a, t, why) for a, t, why in unhandled_at[:200]],
        "decode_stops": stats["decode_stops"],
        "jump_tables": stats["jump_tables"],
        "resume_entries": len(resume),
        "features": dict(features.most_common()),
        "missing_functions": ["%#x" % a for a in missing],
        "c_bytes": sum(os.path.getsize(p) for p in files),
        "seconds_analysis": round(t_analysis, 2),
        "seconds_translate": round(t_translate, 2),
    }
    unhandled_mn = {m for m in mnemonics if any(t.split()[0] == m.split()[0] for t in unhandled)}
    report["mnemonics_handled"] = len(mnemonics) - len(unhandled_mn)
    report["mnemonics_unhandled"] = sorted(unhandled_mn)

    if args.compile:
        report.update(compile_all(args, files, sources))
    json.dump(report, open(os.path.join(args.out, "report.json"), "w"), indent=1)
    short = {k: v for k, v in report.items() if k not in ("unhandled_at", "missing_functions")}
    print(json.dumps(short, indent=1))
    if args.stamp:
        with open(args.stamp, "w") as out:
            out.write("%d functions\n" % stats["functions"])
    return 0


def compile_all(args, files, runtime):
    obj = os.path.join(args.out, "obj")
    os.makedirs(obj, exist_ok=True)
    gen = os.path.join(args.out, "gen")
    flags = [args.opt, "-std=c11", "-fno-strict-aliasing", "-fwrapv", "-w", "-I", RUNTIME, "-I", gen]
    sources = files + runtime + [os.path.abspath(args.compile)]

    def one(src):
        o = os.path.join(obj, os.path.basename(src)[:-2] + ".o")
        t = time.time()
        r = subprocess.run(["clang", "-c", src, "-o", o] + flags, capture_output=True, text=True)
        if r.returncode:
            raise SystemExit(f"clang failed on {src}:\n{r.stderr[:4000]}")
        return o, time.time() - t

    t0 = time.time()
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        results = list(ex.map(one, sources))
    t_compile = time.time() - t0
    program = os.path.join(args.out, "program")
    t1 = time.time()
    r = subprocess.run(["clang", "-o", program] + [o for o, _ in results] + ["-lm"], capture_output=True, text=True)
    if r.returncode:
        raise SystemExit("link failed:\n" + r.stderr[:4000])
    return {"seconds_compile_wall": round(t_compile, 2), "seconds_compile_cpu": round(sum(s for _, s in results), 2),
            "seconds_link": round(time.time() - t1, 2), "jobs": args.jobs, "opt": args.opt}


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
