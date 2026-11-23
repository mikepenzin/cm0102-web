#!/usr/bin/env python3
"""Intro logo test (Q205 to Q208): the intro shows the Bosman logo, and a data folder supplies no picture.

Usage:
    python3 tests/smoke/intro_logo_run.py <cm0102_recomp> <game folder> <Update Team folder>
        [--logo <logo.png>] [--shots <folder>] [--same-save]

Each run uses a new user folder (RECOMP_USER_DIR) and a game folder of links to <game folder>, with
Data/bosman_logo.rgn made by tools/release/make_intro_logo.py from --logo (default: a test pattern).
The runs dump the screen once a second through the intro, with no click.
    classic, widescreen       the intro shows the logo, 520x520 in the centre of the screen
    folder    database = folder:<name>, a copy of the Update Team folder (it has its own logo.RGN and
              eidos.RGN): the intro shows the logo, and each picture of the intro is one of the classic run,
              so the game reads no .rgn file of the folder
    no-logo   a game folder without bosman_logo.rgn: the intro does not show the logo (the game's own
              logo.rgn, the Sports Interactive logo)
--shots copies the first dump with the logo of each run to <folder>/intro-<run>.bmp.
--same-save   also plays 14 days in one game folder, twice without bosman_logo.rgn and once with it (the
              steps of smoke_run.py --same-save): the saves must be the same, so the picture changes
              nothing in the game.
"""
import concurrent.futures
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "release"))
import make_intro_logo  # noqa: E402
import smoke_run  # noqa: E402

SIZE = make_intro_logo.SIZE
DUMPS = 24                      # one a second; the intro without sound ends after about 20 s
MODES = {"classic": (800, 600), "widescreen": (1280, 800)}


def test_png(path):
    """A test pattern PNG: colour bands on a transparent background."""
    w, h = 400, 200
    raw = b""
    for y in range(h):
        raw += b"\0" + b"".join(bytes(((x * 7) & 255, (y * 5) & 255, 200, 255 if 20 < y < 180 else 0))
                                for x in range(w))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def linked_game(game, dest, logo):
    """A game folder of links to <game>; Data is a real folder of links, with the logo file when given."""
    os.makedirs(dest)
    for name in os.listdir(game):
        if name.lower() != "data":
            os.symlink(os.path.join(game, name), os.path.join(dest, name))
            continue
        os.mkdir(os.path.join(dest, name))
        for f in os.listdir(os.path.join(game, name)):
            os.symlink(os.path.join(game, name, f), os.path.join(dest, name, f))
        if logo:
            shutil.copyfile(logo, os.path.join(dest, name, "bosman_logo.rgn"))


def shows_logo(bmp, want):
    """1 when the dump has the 520x520 picture WANT (RGB565 values) in the centre of the screen."""
    w, h, bpp, rows = smoke_run.read_bmp(bmp)
    x0, y0 = (w - SIZE) // 2, (h - SIZE) // 2
    if x0 < 0 or y0 < 0:
        return 0
    for y in range(SIZE):
        row = rows[y0 + y]
        line = want[y * SIZE:(y + 1) * SIZE]
        for x in range(0, SIZE, 1):
            i = (x0 + x) * bpp
            b, g, r = row[i], row[i + 1], row[i + 2]
            if ((r >> 3) << 11 | (g >> 2) << 5 | b >> 3) != line[x]:
                return 0
    return 1


def in_intro(bmp):
    """1 when the dump belongs to the intro: its pictures lie on black, the screens after it do not."""
    _, _, bpp, rows = smoke_run.read_bmp(bmp)
    return not any(rows[y][:8 * bpp].strip(b"\0") for y in range(8))


def intro(program, game, mode, user, shots):
    """Plays the intro and returns the paths of the dumps."""
    steps = [f"{(i + 1) * 1000} dump {shots}/i{i:02d}.bmp" for i in range(DUMPS)] + [f"{DUMPS * 1000 + 500} quit"]
    env = dict(os.environ, HOME=os.path.join(user, "..", "home"), SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
               RECOMP_NO_BOX="1", RECOMP_USER_DIR=user, RECOMP_SCRIPT="; ".join(steps))
    r = subprocess.run([program, "--game", game], env=env, capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        raise RuntimeError(f"the game ended with code {r.returncode}: {r.stderr[-500:]}")
    return [os.path.join(shots, f"i{i:02d}.bmp") for i in range(DUMPS) if os.path.exists(os.path.join(shots, f"i{i:02d}.bmp"))]


def run(program, games, name, mode, folder, update_team, work):
    """Plays one run. Returns the dumps and the user folder."""
    base = os.path.join(work, name)
    user, shots = os.path.join(base, "user"), os.path.join(base, "shots")
    for d in (user, shots, os.path.join(base, "home")):
        os.makedirs(d)
    lines = [f"display = {mode}"]
    if folder:
        shutil.copytree(update_team, os.path.join(user, "Databases", os.path.basename(update_team)))
        lines.append(f"database = folder:{os.path.basename(update_team)}")
    else:
        lines.append("database = 3.9.68")
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    return intro(program, games["no-logo" if name == "no-logo" else "logo"], mode, user, shots)


def play_save(program, game, work, name):
    """Plays 14 game days with the steps of smoke_run.py --same-save and returns the saved game."""
    base = os.path.join(work, name)
    user, home, shots = (os.path.join(base, d) for d in ("user", "home", "shots"))
    for d in (user, home, shots):
        os.makedirs(d)
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write("display = classic\ndatabase = 3.9.68\n")
    steps = [s for s in smoke_run.STEPS if "5-holiday" not in s and "quit" not in s]
    steps = smoke_run.without_intro_click(steps) + ["400000 quit"]
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SAVE_AFTER_DAYS="14",
               RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("classic", shots, steps)))
    subprocess.run([program, "--game", game, "-seed", "1"], env=env, capture_output=True, timeout=600)
    return os.path.join(user, smoke_run.SAVE_NAME)


def same_save(program, game, rgn, work):
    """The saves with and without the logo file. The game folder path is the same in all runs: the save
    of a game in another folder differs in human_manager.dat. First two runs without the file at the same
    time (they must be the same), then one with it."""
    folder = os.path.join(work, "game-save")
    linked_game(game, folder, None)
    with concurrent.futures.ThreadPoolExecutor(2) as pool:
        a, b = pool.map(lambda n: play_save(program, folder, work, n), ("save-no-logo", "save-again"))
    shutil.copyfile(rgn, os.path.join(folder, "Data", "bosman_logo.rgn"))
    c = play_save(program, folder, work, "save-logo")
    out = []
    for name, path in (("again", b), ("logo", c)):
        if not os.path.exists(a) or not os.path.exists(path):
            print(f"FAIL  same-save {name}: no saved game")
            out.append(f"same-save {name}: no save")
            continue
        pa, pb = smoke_run.save_parts(a), smoke_run.save_parts(path)
        differ = sorted(k for k in set(pa) | set(pb) if pa.get(k) != pb.get(k))
        print(f"{'ok   ' if not differ else 'FAIL '} same-save {name}: "
              f"{'the save is the same as without the logo' if not differ else 'differs in ' + ', '.join(differ)}")
        if differ:
            out.append(f"same-save {name}: the saves differ")
    return out


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    program, game, update_team = (os.path.abspath(a.rstrip("/")) for a in argv[1:4])
    logo_png = argv[argv.index("--logo") + 1] if "--logo" in argv else None
    keep = argv[argv.index("--shots") + 1] if "--shots" in argv else None
    failed = []
    with tempfile.TemporaryDirectory(prefix="cm0102-intro-logo-") as work:
        if not logo_png:
            logo_png = os.path.join(work, "test.png")
            test_png(logo_png)
        rgn = os.path.join(work, "bosman_logo.rgn")
        data = make_intro_logo.make_rgn(logo_png)
        open(rgn, "wb").write(data)
        want = struct.unpack(f"<{SIZE * SIZE}H", data[48:])
        games = {"logo": os.path.join(work, "game-1"), "no-logo": os.path.join(work, "game-0")}
        linked_game(game, games["logo"], rgn)
        linked_game(game, games["no-logo"], None)
        runs = [("classic", "classic", False), ("widescreen", "widescreen", False),
                ("folder", "classic", True), ("no-logo", "classic", False)]
        with concurrent.futures.ThreadPoolExecutor(len(runs) + 1) as pool:
            jobs = {name: pool.submit(run, program, games, name, mode, folder, update_team, work)
                    for name, mode, folder in runs}
            dumps = {}
            for name, job in jobs.items():
                try:
                    dumps[name] = job.result()
                except RuntimeError as e:
                    failed.append(f"{name}: {e}")
                    print(f"FAIL  {name}: {e}")
            for name, mode, folder in runs:
                if name not in dumps:
                    continue
                with_logo = [d for d in dumps[name] if shows_logo(d, want)]
                expect = name != "no-logo"
                ok = bool(with_logo) == expect
                print(f"{'ok   ' if ok else 'FAIL '} {name}: {len(with_logo)} of {len(dumps[name])} dumps show the logo")
                if not ok:
                    failed.append(f"{name}: logo {'missing' if expect else 'shown'}")
                if keep and with_logo:
                    os.makedirs(keep, exist_ok=True)
                    shutil.copyfile(with_logo[0], os.path.join(keep, f"intro-{name}.bmp"))
            if "folder" in dumps and "classic" in dumps:
                # Only the dumps of the intro: the screen after it has a random background picture of
                # Pictures, which differs between a folder and the game's own Data.
                seen = {open(d, "rb").read() for d in dumps["classic"]}
                other = [os.path.basename(d) for d in dumps["folder"]
                         if in_intro(d) and open(d, "rb").read() not in seen]
                print(f"{'ok   ' if not other else 'FAIL '} folder: {len(other)} dumps show a picture the classic "
                      f"run does not show {' '.join(other)}")
                if other:
                    failed.append("folder: a picture of the folder")
        if "--same-save" in argv:
            failed += same_save(program, game, rgn, work)
    print("PASS" if not failed else "FAIL")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
