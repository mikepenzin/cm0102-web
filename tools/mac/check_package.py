"""Checks the Mac download (slice 5 of the release spec).

Usage:
    python3 -I tools/mac/check_package.py <file.dmg | App.app> [--version X.Y.Z] [--archs arm64,x86_64]
                                          [--manifest <manifest file>]

Checks:
  1. The bundle has Info.plist, the program, Frameworks and Resources/game with cm0102.exe, Data and
     Sounds (the game opens its sounds there, for example intro1.wav).
  2. Info.plist and the window title in the program hold the version. Without --version the check
     reads it from project() in CMakeLists.txt. Info.plist names the cm0102:// link of invite links (0.7.0).
  3. The program and every library hold exactly the expected architectures (default arm64 and x86_64).
  4. otool -L, for each architecture, lists only /usr/lib, /System, or @rpath, @loader_path and
     @executable_path names that resolve to a file in Contents/Frameworks. No Homebrew path.
  5. A library that loads libSDL3.dylib with dlopen (sdl2-compat) has it in Contents/Frameworks.
  6. Every file needs a macOS version no newer than LSMinimumSystemVersion.
  7. The ad hoc signature is valid.
  8. A .dmg has an Applications link.
  9. With --manifest: every file in Resources/game matches the manifest (path, size, SHA-256) and no file
     is missing or extra. A manifest line holds a 64-character SHA-256, a size and a path, separated by
     tabs, commas or spaces, in any order. Lines that start with # are ignored.

Exit code 0 when all checks pass, else 1."""
import argparse
import hashlib
import os
import plistlib
import re
import subprocess
import sys
import tempfile

failures = []


def ok(msg):
    print("ok    " + msg)


def fail(msg):
    print("FAIL  " + msg)
    failures.append(msg)


def check(cond, good, bad):
    ok(good) if cond else fail(bad)
    return cond


def run(*cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, r.stdout, r.stderr


def is_macho(path):
    try:
        with open(path, "rb") as f:
            magic = f.read(4)
    except OSError:
        return False
    return magic in (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca")


def archs_of(path):
    code, out, _ = run("lipo", "-archs", path)
    return sorted(out.split()) if code == 0 else []


def deps_of(path, arch):
    code, out, err = run("otool", "-arch", arch, "-L", path)
    if code != 0:
        return None
    lines = out.splitlines()[1:]
    deps = [ln.strip().split(" (")[0] for ln in lines if ln.strip()]
    # In a library the first line is its own install name.
    if path.endswith(".dylib") and deps:
        deps = deps[1:]
    return deps


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def parse_manifest(path):
    entries = {}
    for ln in open(path, encoding="utf-8"):
        ln = ln.strip()
        if not ln or ln.startswith("#"):
            continue
        parts = [p for p in re.split(r"[\t,]|\s{2,}", ln) if p.strip()]
        if len(parts) < 3:
            parts = ln.split(None, 2)
        digest = next((p for p in parts if re.fullmatch(r"[0-9a-fA-F]{64}", p.strip())), None)
        size = next((p for p in parts if re.fullmatch(r"\d+", p.strip())), None)
        rest = [p.strip().strip('"') for p in parts if p.strip() not in (digest, size)]
        if digest is None or size is None or not rest:
            continue  # a header line
        entries[rest[0].replace("\\", "/").removeprefix("./").lstrip("/")] = (int(size), digest.lower())
    return entries


def check_app(app, version, want_archs, manifest):
    contents = os.path.join(app, "Contents")
    plist_path = os.path.join(contents, "Info.plist")
    if not check(os.path.isfile(plist_path), "Info.plist exists", "Info.plist is missing"):
        return
    with open(plist_path, "rb") as f:
        info = plistlib.load(f)
    exe_name = info.get("CFBundleExecutable", "")
    exe = os.path.join(contents, "MacOS", exe_name)
    if not check(os.path.isfile(exe), "program %s exists" % exe_name, "program %r is missing" % exe_name):
        return

    # 2. Version.
    for key in ("CFBundleShortVersionString", "CFBundleVersion"):
        check(info.get(key) == version, "%s is %s" % (key, version), "%s is %r, expected %s" % (key, info.get(key), version))
    title = ("CM 01/02: Bosman Edition " + version).encode()
    with open(exe, "rb") as f:
        data = f.read()
    check(title in data, "window title with the version is in the program",
          "window title %r is not in the program" % title.decode())
    schemes = [s for t in info.get("CFBundleURLTypes", []) for s in t.get("CFBundleURLSchemes", [])]
    check(schemes == ["cm0102"], "Info.plist names the cm0102:// link", "Info.plist link schemes are %r" % schemes)

    # 3. Architectures of every Mach-O file.
    fw = os.path.join(contents, "Frameworks")
    machos = [exe]
    if os.path.isdir(fw):
        machos += sorted(os.path.join(fw, n) for n in os.listdir(fw) if is_macho(os.path.join(fw, n)))
    check(os.path.isdir(fw) and len(machos) > 1, "Frameworks holds libraries", "Frameworks holds no library")
    for m in machos:
        got = archs_of(m)
        check(got == want_archs, "%s: %s" % (os.path.relpath(m, contents), " ".join(got)),
              "%s: architectures %s, expected %s" % (os.path.relpath(m, contents), got, want_archs))

    # 4 and 5. Linked libraries.
    for m in machos:
        for arch in want_archs:
            deps = deps_of(m, arch)
            if deps is None:
                fail("%s: otool -L failed for %s" % (os.path.relpath(m, contents), arch))
                continue
            bad = []
            for d in deps:
                if d.startswith(("/usr/lib/", "/System/Library/")):
                    continue
                if d.startswith(("@rpath/", "@loader_path/", "@executable_path/")):
                    if os.path.isfile(os.path.join(fw, os.path.basename(d))):
                        continue
                bad.append(d)
            check(not bad, "%s (%s): libraries are system or bundled" % (os.path.relpath(m, contents), arch),
                  "%s (%s): bad library paths %s" % (os.path.relpath(m, contents), arch, bad))
        if m != exe and os.path.basename(m) != "libSDL3.dylib":
            with open(m, "rb") as f:
                if b"libSDL3.dylib" in f.read():
                    check(os.path.isfile(os.path.join(fw, "libSDL3.dylib")),
                          "%s loads libSDL3.dylib and the bundle has it" % os.path.basename(m),
                          "%s loads libSDL3.dylib and the bundle lacks it" % os.path.basename(m))

    # Minimum macOS version of every file, per architecture, against LSMinimumSystemVersion.
    least = tuple(int(x) for x in info.get("LSMinimumSystemVersion", "0").split("."))
    for m in machos:
        for arch in want_archs:
            code, out, _ = run("otool", "-arch", arch, "-l", m)
            found = re.findall(r"cmd LC_BUILD_VERSION.*?minos (\d+(?:\.\d+)*)", out, re.S)
            minos = tuple(int(x) for x in found[0].split(".")) if found else None
            name = "%s (%s)" % (os.path.relpath(m, contents), arch)
            check(minos is not None and minos <= least, "%s: needs macOS %s" % (name, ".".join(map(str, minos or ()))),
                  "%s: needs macOS %s, but LSMinimumSystemVersion is %s" % (
                      name, ".".join(map(str, minos)) if minos else "unknown", info.get("LSMinimumSystemVersion")))

    # 7. Signature.
    code, _, err = run("codesign", "--verify", "--strict", "--deep", app)
    check(code == 0, "ad hoc signature is valid", "signature check failed: " + err.strip())

    # 1 and 8. The game files.
    game = os.path.join(contents, "Resources", "game")
    check(os.path.isfile(os.path.join(game, "cm0102.exe")) and os.path.isdir(os.path.join(game, "Data")),
          "Resources/game holds cm0102.exe and Data", "Resources/game lacks cm0102.exe or Data")
    sounds = [n for n in os.listdir(game) if n.lower() == "sounds"] if os.path.isdir(game) else []
    check(bool(sounds) and any(n.lower() == "intro1.wav" for n in os.listdir(os.path.join(game, sounds[0]))),
          "Resources/game holds Sounds", "Resources/game lacks Sounds/intro1.wav, so the game is silent")
    if manifest:
        want = parse_manifest(manifest)
        check(bool(want), "manifest has %d entries" % len(want), "manifest %s has no entries" % manifest)
        have = {}
        for root, _, names in os.walk(game):
            for n in names:
                p = os.path.join(root, n)
                have[os.path.relpath(p, game).replace(os.sep, "/")] = p
        want_ci = {k.lower(): (k, v) for k, v in want.items()}
        have_ci = {k.lower(): (k, v) for k, v in have.items()}
        missing = sorted(want_ci[k][0] for k in want_ci if k not in have_ci)
        extra = sorted(have_ci[k][0] for k in have_ci if k not in want_ci)
        check(not missing, "no file of the manifest is missing", "missing from the bundle: %s" % missing[:10])
        check(not extra, "no file outside the manifest", "not in the manifest: %s" % extra[:10])
        wrong = []
        for k in sorted(set(want_ci) & set(have_ci)):
            size, digest = want_ci[k][1]
            p = have_ci[k][1]
            if os.path.getsize(p) != size or sha256(p) != digest:
                wrong.append(have_ci[k][0])
        check(not wrong, "all files match the manifest (size and SHA-256)", "files that differ: %s" % wrong[:10])
    else:
        print("note  no --manifest, so the game files are not compared with the clean files")


def default_version():
    here = os.path.dirname(os.path.abspath(__file__))
    cm = os.path.join(here, "..", "..", "CMakeLists.txt")
    m = re.search(r"project\([^)]*VERSION\s+([0-9][0-9.]*)", open(cm).read())
    return m.group(1) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("target")
    ap.add_argument("--version")
    ap.add_argument("--archs", default="arm64,x86_64")
    ap.add_argument("--manifest")
    a = ap.parse_args()
    version = a.version or default_version()
    if not version:
        print("cannot find the version; give --version")
        return 2
    want_archs = sorted(a.archs.split(","))

    if a.target.endswith(".dmg"):
        if not check(os.path.isfile(a.target), "dmg exists", "dmg %s not found" % a.target):
            return 1
        check(("-" + version + "-") in os.path.basename(a.target), "dmg file name holds the version",
              "dmg file name lacks the version %s" % version)
        mount = tempfile.mkdtemp(prefix="cm0102-check-")
        code, _, err = run("hdiutil", "attach", "-nobrowse", "-readonly", "-noverify", "-mountpoint", mount, a.target)
        if code != 0:
            fail("cannot mount the dmg: " + err.strip())
            os.rmdir(mount)
            return 1
        try:
            link = os.path.join(mount, "Applications")
            check(os.path.islink(link) and os.readlink(link) == "/Applications",
                  "dmg has an Applications link", "dmg has no Applications link")
            apps = [n for n in os.listdir(mount) if n.endswith(".app")]
            if check(len(apps) == 1, "dmg holds one app: %s" % (apps[0] if apps else ""),
                     "dmg holds %d apps, expected 1" % len(apps)):
                check_app(os.path.join(mount, apps[0]), version, want_archs, a.manifest)
        finally:
            run("hdiutil", "detach", mount)
            try:
                os.rmdir(mount)
            except OSError:
                pass
    else:
        check_app(a.target.rstrip("/"), version, want_archs, a.manifest)

    print()
    print("PASS" if not failures else "FAILED: %d check(s)" % len(failures))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
