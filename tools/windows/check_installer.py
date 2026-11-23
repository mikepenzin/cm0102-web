"""Checks the Windows download (slice 6 of the release spec).

Usage:
    python3 -I tools/windows/check_installer.py <cm0102_recomp.exe> <Setup.exe> [--version X.Y.Z]
        [--manifest <manifest file> --game-dir <folder> --nsis-log <makensis log>]

Checks:
  1. The program is a PE32+ file for x86-64.
  2. The version resource holds the version, in the fixed block and in FileVersion and ProductVersion.
     Without --version the check reads it from project() in CMakeLists.txt.
  3. The program imports only Windows system DLLs. Static SDL2 and the C runtime are inside it.
  4. The program has an icon resource and an icon group.
  5. The file name of the setup program holds the version.
  6. The setup program exists and is not empty.
  7. With --manifest (0.6.0 slice 12): the game folder that makensis packed (--game-dir) holds exactly the
     files of the manifest, each with its size and SHA-256 (Seasons/rules.tsv, names.tsv and clubs.tsv
     and the season zips among them), and the makensis log (-V4, --nsis-log) adds exactly those files under game, each with its size.
     The installer packs the folder with File /r (packaging/windows/installer.nsi).

Exit code 0 when all checks pass, else 1."""
import argparse
import hashlib
import os
import re
import struct
import sys

failures = []


def check(cond, good, bad):
    print(("ok    " + good) if cond else ("FAIL  " + bad))
    if not cond:
        failures.append(bad)
    return cond


# DLLs that every Windows has. A name starting with api-ms-win- is an API set of the system.
SYSTEM_DLLS = {
    "kernel32.dll", "kernelbase.dll", "ntdll.dll", "user32.dll", "gdi32.dll", "advapi32.dll",
    "shell32.dll", "ole32.dll", "oleaut32.dll", "winmm.dll", "imm32.dll", "version.dll",
    "setupapi.dll", "msvcrt.dll", "ucrtbase.dll", "shlwapi.dll", "ws2_32.dll", "comdlg32.dll",
    "bcrypt.dll", "cfgmgr32.dll", "combase.dll", "rpcrt4.dll", "dwmapi.dll", "uxtheme.dll",
    "hid.dll", "opengl32.dll", "winhttp.dll", "wininet.dll", "crypt32.dll", "d3d9.dll",
    "dxgi.dll", "shcore.dll", "dinput8.dll", "xinput1_4.dll",
    "iphlpapi.dll",                       # 0.7.0: the computer's own addresses for network games (net_win32.c)
}


class PE:
    def __init__(self, data):
        self.d = data
        if data[:2] != b"MZ":
            raise ValueError("not a PE file")
        self.pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[self.pe:self.pe + 4] != b"PE\0\0":
            raise ValueError("no PE signature")
        self.machine, self.nsec = struct.unpack_from("<HH", data, self.pe + 4)
        opt_size = struct.unpack_from("<H", data, self.pe + 20)[0]
        self.opt = self.pe + 24
        self.magic = struct.unpack_from("<H", data, self.opt)[0]
        dd = self.opt + (112 if self.magic == 0x20B else 96)
        self.dirs = [struct.unpack_from("<II", data, dd + 8 * i) for i in range(16)]
        sec = self.opt + opt_size
        self.secs = []
        for i in range(self.nsec):
            vsize, va, rsize, raw = struct.unpack_from("<IIII", data, sec + 40 * i + 8)
            self.secs.append((va, max(vsize, rsize), raw))

    def off(self, rva):
        for va, size, raw in self.secs:
            if va <= rva < va + size:
                return raw + rva - va
        raise ValueError("bad RVA %#x" % rva)

    def cstr(self, off):
        end = self.d.index(b"\0", off)
        return self.d[off:end].decode("latin-1")

    def imports(self):
        rva, _ = self.dirs[1]
        names = []
        if not rva:
            return names
        o = self.off(rva)
        while True:
            name_rva = struct.unpack_from("<I", self.d, o + 12)[0]
            if not name_rva:
                break
            names.append(self.cstr(self.off(name_rva)))
            o += 20
        return names

    def resources(self):
        """Returns {type id: [(name id, data bytes)]}, with the first language of each resource."""
        rva, _ = self.dirs[2]
        out = {}
        if not rva:
            return out
        base = self.off(rva)

        def entries(o):
            named, ids = struct.unpack_from("<HH", self.d, o + 12)
            return [struct.unpack_from("<II", self.d, o + 16 + 8 * i) for i in range(named + ids)]

        for tid, toff in entries(base):
            if not toff & 0x80000000:
                continue
            for nid, noff in entries(base + (toff & 0x7FFFFFFF)):
                if not noff & 0x80000000:
                    continue
                lang = entries(base + (noff & 0x7FFFFFFF))
                if not lang:
                    continue
                drva, dsize = struct.unpack_from("<II", self.d, base + lang[0][1])
                o = self.off(drva)
                out.setdefault(tid, []).append((nid, self.d[o:o + dsize]))
        return out


def parse_manifest(path):
    """{path: (size, sha256)} of a manifest: SHA-256, size and path, separated by tabs."""
    out = {}
    for line in open(path, encoding="utf-8"):
        if not line.strip() or line.startswith("#"):
            continue
        digest, size, rel = line.rstrip("\n").split("\t", 2)
        out[rel] = (int(size), digest.lower())
    return out


def folder_files(root):
    out = {}
    for dirpath, _, files in os.walk(root):
        for f in files:
            if f == ".DS_Store":
                continue
            full = os.path.join(dirpath, f)
            h = hashlib.sha256()
            with open(full, "rb") as fh:
                for block in iter(lambda: fh.read(1 << 20), b""):
                    h.update(block)
            out[os.path.relpath(full, root).replace(os.sep, "/")] = (os.path.getsize(full), h.hexdigest())
    return out


def nsis_game_files(log, game_dir):
    """{path: size} of the files that makensis -V4 adds under $INSTDIR\\game. The log has
    'SetOutPath: "$INSTDIR\\game"', then 'File: "name" [compress] N bytes' lines (makensis 3.x writes
    'File: "name" N bytes', or 'File: "name" C/N bytes' for a compressed file: N is the size), with
    'File: Descending to:' and 'File: Returning to:' lines for the folders."""
    root = os.path.realpath(game_dir)
    out, in_game, cur = {}, False, ""
    for line in open(log, encoding="utf-8", errors="replace"):
        line = line.strip()
        m = re.match(r'^SetOutPath: "(.*)"$', line)
        if m:
            in_game = m.group(1).replace("/", "\\").lower().endswith("\\game")
            cur = ""
            continue
        if not in_game:
            continue
        m = re.match(r'^File: (?:Descending|Returning) to: "(.*)"$', line)
        if m:
            cur = os.path.relpath(os.path.realpath(m.group(1)), root).replace(os.sep, "/")
            cur = "" if cur == "." else cur
            continue
        m = re.match(r'^File: "([^"]+)"(?:->"[^"]*")?(?: \[[^\]]*\])? (?:\d+/)?(\d+) bytes', line)
        if m:
            out[(cur + "/" if cur else "") + m.group(1)] = int(m.group(2))
    return out


def cmake_version():
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    with open(os.path.join(root, "CMakeLists.txt")) as f:
        text = f.read()
    m = re.search(r"^project\([^)]*VERSION\s+([0-9][0-9.]*)", text, re.M)
    return m.group(1) if m else None


def utf16_strings(block):
    pattern = rb"((?:[A-Za-z]\x00)+)\x00\x00(?:\x00\x00)*((?:[^\x00]\x00)+)\x00\x00"
    return {m.group(1).decode("utf-16-le"): m.group(2).decode("utf-16-le")
            for m in re.finditer(pattern, block)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exe")
    ap.add_argument("setup")
    ap.add_argument("--version")
    ap.add_argument("--manifest")
    ap.add_argument("--game-dir")
    ap.add_argument("--nsis-log")
    args = ap.parse_args()
    version = args.version or cmake_version()
    if not check(version is not None, "version is " + str(version), "cannot find the version"):
        return 1
    try:
        with open(args.exe, "rb") as f:
            pe = PE(f.read())
    except (OSError, ValueError, struct.error) as e:
        check(False, "", "%s: %s" % (args.exe, e))
        return 1

    check(pe.machine == 0x8664 and pe.magic == 0x20B, "PE32+ for x86-64",
          "not PE32+ for x86-64 (machine %#x, magic %#x)" % (pe.machine, pe.magic))

    res = pe.resources()
    ver = res.get(16)
    if check(bool(ver), "version resource exists", "no version resource"):
        block = ver[0][1]
        sig = block.find(struct.pack("<I", 0xFEEF04BD))
        if check(sig >= 0, "fixed version block exists", "no fixed version block"):
            ms, ls = struct.unpack_from("<II", block, sig + 8)
            fixed = "%d.%d.%d" % (ms >> 16, ms & 0xFFFF, ls >> 16)
            check(fixed == version, "fixed file version is " + fixed,
                  "fixed file version is %s, expected %s" % (fixed, version))
        strings = utf16_strings(block)
        for key in ("FileVersion", "ProductVersion"):
            check(strings.get(key) == version, "%s is %s" % (key, version),
                  "%s is %r, expected %s" % (key, strings.get(key), version))

    check(bool(res.get(3)) and bool(res.get(14)), "icon resource and icon group exist",
          "no icon resource or no icon group")

    bad = [n for n in pe.imports()
           if n.lower() not in SYSTEM_DLLS and not n.lower().startswith("api-ms-win-")]
    check(not bad, "imports only Windows system DLLs",
          "imports a DLL that Windows does not have: " + ", ".join(bad))

    name = os.path.basename(args.setup)
    check(version in name and name.lower().endswith("-setup.exe"),
          "setup file name holds the version: " + name,
          "setup file name %r does not hold version %s" % (name, version))
    check(os.path.isfile(args.setup) and os.path.getsize(args.setup) > 0,
          "setup file exists and has content", "setup file is missing or empty: " + args.setup)

    if args.manifest:
        want = parse_manifest(args.manifest)
        tables = ("Seasons/rules.tsv", "Seasons/names.tsv", "Seasons/clubs.tsv")
        check(bool(want) and all(t in want for t in tables) and any(k.startswith("Seasons/") and k.endswith(".zip")
                                                                    for k in want),
              "manifest has %d files, the season tables and the season zips among them" % len(want),
              "manifest %s lacks files, a season table (rules, names, clubs) or the season zips" % args.manifest)
        have = folder_files(args.game_dir) if args.game_dir else {}
        bad = sorted(k for k in set(want) | set(have) if want.get(k) != have.get(k))
        check(bool(args.game_dir) and not bad, "the packed game folder matches the manifest",
              "the packed game folder differs from the manifest: %s" % bad[:10])
        packed = nsis_game_files(args.nsis_log, args.game_dir) if args.nsis_log and args.game_dir else {}
        bad = sorted(k for k in set(want) | set(packed) if packed.get(k) != (want[k][0] if k in want else None))
        check(bool(packed) and not bad, "the installer packs the %d files of the manifest" % len(packed),
              "the installer log differs from the manifest: %s" % bad[:10])

    print("%d check(s) failed" % len(failures) if failures else "all checks passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
