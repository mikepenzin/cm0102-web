#!/usr/bin/env python3
"""Release test 3 of the online play spec (Q29, slice 9), the Mac's side: GitHub's Windows and Linux machines join a
game that this Mac hosts over the internet, with scripted clicks.

Usage:
    python3 tests/online/github_join_run.py <cm0102_recomp> [--game <folder>] [--server real|<address>]
        [--push --appimage <0.7.0 AppImage> [--win-build build-win]] [--run-id <id>] [--name <game name>]
        [--host-wait-minutes 45]
        [--out <folder>]

The workflow is .github/workflows/online-release-join.yml, and its joiner is tests/online/remote_join.py.
`gh workflow run` works only for a workflow on the default branch of GitHub, so the workflow must be pushed
first. A push waits for Max.

The steps:
    1  Wait for room (online_common.wait_for_room). The host makes a game named "00S9-<time>" (or --name): one
       word, because the type step reads one word, and 00 makes it the first row of the joiners' list
       (remote_join.py).
    2  --push: put the packages on the draft release ci-online-test (visible only with write access): the
       AppImage of --appimage (tools/linux/make_appimage.sh or its container step) and a zip of --win-build's
       cm0102_recomp.exe and seasons folder with the game folder. Then push this checkout's commit to the branch online-ci-test (never main). The workflow's push
       trigger starts it with the game name 00S9-CI and the real lobby; the run's id comes from `gh run list`.
       (GitHub starts workflow_dispatch only for a workflow on the default branch, which the 0.6.0 work owns.)
       Without --push the script prints the push command and finds the run in `gh run list` while the host
       waits. Max allowed pushes of online branches (Q43). Before the push the script checks brief rule 10 (no
       commit from before the history rewrite). After the run it deletes the draft release.
    3  The host waits at its Enter Name for --host-wait-minutes (default 45: the build takes about 20 minutes and
       the two joiners join one at a time or together), then types its own names, as in network_run.py.
    4  With a run id (--push, a run found while the host waits, or --run-id): `gh run watch`, then `gh run download`
       into <out>/runner/, and the joiners' dumps are compared with the host's.

Checks:
    1 the host ends with code 0, after the lobby listed its game
    2 the host's traffic log has 2 joiner connections from the relay (0.66.255.<channel>) with packets both ways
    3 each joiner's result.txt ends with "remote join: PASS" (Linux and Windows)
    4 each joiner's three dumps equal the host's: at most 1% of pixels differ by more than 2 levels (TOLERANCE)
Result: PASS (exit code 0) when all checks pass; FAIL (1) else; WAIT (3) when the build has no relay (slice 5) or
no workflow run took part (the host's checks only).
"""
import calendar
import glob
import json
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import online_common as oc  # noqa: E402

nr = oc.nr
import smoke_run  # noqa: E402

WORKFLOW = "online-release-join.yml"
JOINERS = ("join-Linux", "join-Windows")
TEST_BRANCH = "online-ci-test"          # the workflow's push trigger; never main
CI_NAME = "00S9-CI"                     # the workflow's default game name for a push run


def gh(*args, timeout=120):
    return subprocess.run(["gh", *args], capture_output=True, text=True, timeout=timeout)


RELEASE_TAG = "ci-online-test"           # the draft release that the workflow reads; deleted after the run
OLD_MAIN = "0e9a510c"                    # a commit of main from before the rewrite of 29 October (brief rule 10)


def git(*args):
    return subprocess.run(["git", "-C", oc.REPO, *args], capture_output=True, text=True)


def push_allowed():
    """Brief rule 10: fetch, then refuse a push of a branch that holds a commit from before the history rewrite."""
    git("fetch", "-q", "origin")
    if git("cat-file", "-e", OLD_MAIN).returncode == 0 and \
            git("merge-base", "--is-ancestor", OLD_MAIN, "HEAD").returncode == 0:
        return f"HEAD holds {OLD_MAIN}, a commit from before the rewrite"
    if not git("merge-base", "HEAD", "origin/main").stdout.strip():
        return "HEAD has no merge base with origin/main"
    return None


def make_draft(appimage, win_build, game, folder):
    """The draft release with the two packages: the AppImage, and a zip of the Windows program, its seasons folder
    and the game folder. A draft is visible only to people with write access. Returns None or the reason."""
    if not os.path.exists(appimage) or not os.path.exists(os.path.join(win_build, "cm0102_recomp.exe")):
        return f"no package: {appimage} or {win_build}/cm0102_recomp.exe"
    win_zip = os.path.join(folder, "windows-joiner.zip")
    linux = os.path.join(folder, "linux-joiner.AppImage")
    if os.path.exists(win_zip):
        os.remove(win_zip)
    subprocess.run(["zip", "-q", "-j", win_zip, os.path.join(win_build, "cm0102_recomp.exe")], check=True)
    subprocess.run(["zip", "-q", "-r", win_zip, "seasons"], cwd=win_build, check=True)
    subprocess.run(["zip", "-q", "-r", win_zip, "game"], cwd=_game_link(game, folder), check=True)
    if os.path.exists(linux):
        os.remove(linux)
    os.link(appimage, linux)
    gh("release", "delete", RELEASE_TAG, "--yes")
    r = gh("release", "create", RELEASE_TAG, "--draft",          # no --target: the commit is not on GitHub yet
           "--title", "CI online test (draft, deleted after the run)",
           "--notes", "Packages for .github/workflows/online-release-join.yml. Not a release.", win_zip, linux,
           timeout=3600)
    return None if r.returncode == 0 else f"gh release create failed: {r.stderr.strip()[:200]}"


def _game_link(game, folder):
    """A folder whose "game" entry is the game folder, for the zip's game/ prefix."""
    d = os.path.join(folder, "zip-root")
    os.makedirs(d, exist_ok=True)
    link = os.path.join(d, "game")
    if not os.path.islink(link):
        os.symlink(game, link)
    return d


def push_test_branch():
    """Pushes this checkout's commit to the test branch, which starts the workflow with its default inputs
    (game name CI_NAME, the real lobby). Returns the run id, or None. Never main."""
    why = push_allowed()
    if why:
        print(f"no push: {why}")
        return None
    t0 = time.time()
    r = subprocess.run(["git", "-C", oc.REPO, "push", "--force", "origin", f"HEAD:refs/heads/{TEST_BRANCH}"],
                       capture_output=True, text=True)
    if r.returncode:
        print(f"git push to {TEST_BRANCH} failed: {r.stderr.strip()}")
        return None
    for _ in range(30):
        time.sleep(5)
        run = new_run(t0, "push")
        if run:
            return run
    return None


def new_run(t0, event="push"):
    """The id of a run of the workflow (started by event) that started at t0 or later, or None."""
    r = gh("run", "list", "--workflow", WORKFLOW, "--event", event, "--limit", "5",
           "--json", "databaseId,createdAt")
    try:
        runs = json.loads(r.stdout)
    except ValueError:
        return None
    for run in runs:
        if calendar.timegm(time.strptime(run["createdAt"], "%Y-%m-%dT%H:%M:%SZ")) >= t0 - 30:
            return str(run["databaseId"])
    return None


def relay_connections(log):
    """[(bytes out, bytes in)] of each closed or ended connection from the relay (0.66.255.x) in a traffic log."""
    out = []
    for line in open(log) if os.path.exists(log) else []:
        p = line.split()
        if len(p) > 4 and p[1] in ("close", "end") and p[4].startswith("peer=0.66.255."):
            f = dict(x.split("=", 1) for x in p if "=" in x)
            out.append({"bytes_out": int(f["out"]), "bytes_in": int(f["in"])})
    return out


# Each platform decodes the background picture with its own rounding: the Mac's and the runners' screens differ by
# at most 2 levels in 38% of pixels (run release3-ci3), while Windows and Linux are equal. So the screens compare
# with a tolerance of 2 levels.
TOLERANCE = 2


def main(argv):
    args = argv[1:]
    if not args or args[0].startswith("--"):
        print(__doc__)
        return 2
    program = os.path.abspath(args.pop(0))
    o = {"game": oc.GAME, "server": "real", "run": None, "wait": "45", "out": None, "name": None, "appimage": None, "win": os.path.join(oc.REPO, "build-win")}
    do_push = False
    while args:
        k = args.pop(0)
        if k == "--push":
            do_push = True
            continue
        if not args:
            print(__doc__)
            return 2
        key = {"--game": "game", "--server": "server", "--run-id": "run", "--host-wait-minutes": "wait",
               "--out": "out", "--name": "name", "--appimage": "appimage", "--win-build": "win"}.get(k)
        if not key:
            print(__doc__)
            return 2
        o[key] = args.pop(0)
    url = oc.REAL_LOBBY if o["server"] == "real" else o["server"].rstrip("/")
    if not oc.relay_in_build(program):
        oc.put_result("release3", "all", "WAIT", "the build has no relay (slice 5)", "")
        return 2
    if not oc.lobby_ok(url):
        print(f"FAIL the lobby {url} does not answer /v1/health")
        return 1
    out = os.path.abspath(o["out"] or os.path.join(oc.RUN_LOGS, time.strftime("release3-%Y%m%d-%H%M")))
    os.makedirs(out, exist_ok=True)
    # One word (the type step reads one word, user32.c); 00 sorts it first in the joiners' list.
    name = CI_NAME if do_push else o["name"] or "00S9-" + time.strftime("%H%M%S")
    wait_ms = int(o["wait"]) * 60000
    failed = []

    def check(ok, text):
        print(("ok   " if ok else "FAIL ") + text, flush=True)
        if not ok:
            failed.append(text)

    if do_push:
        why = push_allowed() or make_draft(o["appimage"] or "", o["win"], o["game"], out)
        if why:
            check(False, f"the draft release with the packages: {why}")
            oc.put_result("release3", "all", "FAIL", why, out)
            return 1
        print(f"the draft release {RELEASE_TAG} holds the packages", flush=True)
    oc.wait_for_room()
    host, err = nr.start(program, o["game"], os.path.join(out, "work"), "host", oc.host_steps(name, wait_ms), out,
                         1, {"RECOMP_LOBBY_URL": url})
    run_id, listed = o["run"], False
    try:
        ready = nr.wait_for(os.path.join(out, "host-stderr.txt"), f"screen {nr.NAME_BUILD}", host, 240)
        check(ready, "the host reaches its Enter Name screen")
        for _ in range(40):
            listed = any(g.get("name") == name for g in nr.lobby_games(url) or [])
            if listed or not ready:
                break
            time.sleep(1)
        check(listed, f"1 the lobby lists the game {name}")
        if listed and do_push:
            run_id = push_test_branch()
            check(run_id is not None, f"the workflow run started: {run_id}")
        elif listed and not run_id:
            print(f"start the workflow within {o['wait']} minutes (the game name must be {CI_NAME}: give "
                  f"--name {CI_NAME}):\n  git push --force origin HEAD:refs/heads/{TEST_BRANCH}", flush=True)
        t_host, end = time.time(), time.time() + wait_ms / 1000 + 300
        while host.poll() is None and time.time() < end:
            time.sleep(30)
            if listed and not run_id:       # a run started by hand while the host waits
                run_id = new_run(t_host, "push") or new_run(t_host, "workflow_dispatch")
        if host.poll() is None:
            raise subprocess.TimeoutExpired("host", wait_ms / 1000 + 300)
    except subprocess.TimeoutExpired:
        pass
    finally:
        if host.poll() is None:
            host.kill()
            host.wait()
        err.close()
    check(host.returncode == 0, f"1 the host ended with code {host.returncode}")
    # Relay joiners can come from the same 0.66.255.<channel>:<port> one after the other, so count by handle
    # (nr.connections keys by peer). Run release3-ci3: two joiners, both 0.66.255.0:50000.
    joined = relay_connections(os.path.join(out, "host-net.log"))
    if run_id:              # without a workflow run no joiner came, and the result is WAIT
        check(len(joined) == len(JOINERS) and all(c["bytes_in"] and c["bytes_out"] for c in joined),
              f"2 host log: {len(joined)} joiner connections from the relay with bytes both ways, "
              f"{len(JOINERS)} expected")
        try:
            gh("run", "watch", run_id, "--exit-status", timeout=7200)
        except subprocess.TimeoutExpired:
            check(False, f"the workflow run {run_id} did not end within 2 hours")
        runner = os.path.join(out, "runner")
        r = gh("run", "download", run_id, "--dir", runner, "--pattern", "shots-*", timeout=600)
        check(r.returncode == 0, f"the joiners' results downloaded from run {run_id}")
        for j in JOINERS:
            res = glob.glob(os.path.join(runner, "shots-*", "result.txt"))
            text = next((open(p).read() for p in res if os.path.exists(os.path.join(os.path.dirname(p),
                                                                                    f"{j}-stderr.txt"))), "")
            check(text.rstrip().endswith("remote join: PASS"), f"3 {j}: {text.strip().splitlines()[-1:] or 'none'}")
            for host_shot, suffix in (("host-1-name.bmp", "1-name"), ("host-2-typed.bmp", "2-typed"),
                                      ("host-3-nation.bmp", "3-nation")):
                found = glob.glob(os.path.join(runner, "shots-*", f"{j}-{suffix}.bmp"))
                a = os.path.join(out, host_shot)
                if found and os.path.exists(a):
                    share = smoke_run.differing_share(a, found[0], tol=TOLERANCE)
                    check(share <= 0.01, f"4 {j} {suffix} equals the host's: {share:.2%} of pixels differ")
                else:
                    check(False, f"4 {j} {suffix}: dump missing")
    if do_push:
        r = gh("release", "delete", RELEASE_TAG, "--yes")
        check(r.returncode == 0, f"the draft release {RELEASE_TAG} deleted")
    verdict = "FAIL" if failed else "PASS" if run_id else "WAIT"
    oc.put_result("release3", "all", verdict, "; ".join(failed[:2]) or (f"run {run_id}" if run_id else
                                                                        "host only: no workflow run"), out)
    return {"PASS": 0, "WAIT": 3, "FAIL": 1}[verdict]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
