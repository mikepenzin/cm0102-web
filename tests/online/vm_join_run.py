#!/usr/bin/env python3
"""Release test 2 of the online play spec (Q29, slice 9): the Windows 11 virtual machine joins a game that this Mac
hosts.

Usage:
    python3 tests/online/vm_join_run.py <cm0102_recomp> <Windows build folder> --gates-done
        [--game <folder>] [--via lan|relay|both] [--server real|<address>] [--vm "Windows 11 (1)"]
        [--mac-address 10.211.55.2] [--out <folder>] [--keep-vm]

<Windows build folder> holds cm0102_recomp.exe and its seasons folder (a mingw-w64 build of the same commit,
packaging/windows/mingw-w64-x86_64.cmake). Max allowed the build sessions to start, use and suspend the virtual
machine after the 0.6.0 gates finish (Q30). --gates-done says that they did, and the script also refuses while a
gate_run.py, full_play_run.py or soak_run.py process runs.

The steps:
    1  Wait for room (online_common.wait_for_room). Start or resume the virtual machine (prlctl) and wait until
       `prlctl exec` runs a command in it.
    2  Serve a folder over HTTP on the Mac's address on the virtual machine's network (--mac-address, default
       the Mac's 10.211.55.x address): game.zip (the game folder, packed once into run-logs/s9-tests/vm-cache),
       win.zip (the Windows build) and a PowerShell script. The virtual machine fetches them into C:\\cm0102-s9
       (the game folder only once) and sends its logs and dumps back with HTTP PUT.
    3  For each join (--via): the Mac starts the host (tests/smoke/network_run.py's host steps, game name
       "00VM-<time>"). When the host shows its Enter Name, the virtual machine runs the joiner's timed clicks of
       network_run.py:
           lan    it types the Mac's address in the Join field (no lobby, Online play on)
           relay  it sees lobby rows only (RECOMP_TEST_NO_LAN_ROWS), clicks the first row and joins through the
                  relay of --server (default: the real server). The game name "00VM-<time>" sorts first in the
                  list (remote_join.py).
       The host waits 150 s at its Enter Name, then types its own names as in network_run.py.
    4  Suspend the virtual machine (unless --keep-vm).
--via both (the default when the Mac build has the relay) runs lan, then relay; otherwise the default is lan.

Checks of each join:
    1 the host and the joiner end with code 0
    2 the host's traffic log has one joiner connection with packets both ways
    3 the joiner's traffic log has the connection to the host (lan: <mac>:6667; relay: a 0.66 address) with
      packets both ways, and 4 click packets (opcode 6)
    4 the joiner's three dumps equal the host's (Enter Name, Enter Name with the typed names, Select
      Nationality after Next): at most 1% of pixels differ
    5 relay: the joiner's log says it joined through the relay
Result: PASS (exit code 0) when every join passes and the relay join ran; LAN (exit code 3) when only the lan
join ran and passed (the release test needs the relay join too); FAIL (exit code 1) else. The relay join needs
slice 5 in the Mac build and in the Windows build. The script suspends the virtual machine at the end only when
it started or resumed it.
"""
import base64
import functools
import http.server
import os
import re
import shutil
import subprocess
import sys
import threading
import time
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import online_common as oc  # noqa: E402
import remote_join  # noqa: E402

nr = oc.nr
import smoke_run  # noqa: E402  (tests/smoke is on the path through online_common)

VM = "Windows 11 (1)"
WIN_DIR = r"C:\cm0102-s9"
CACHE = os.path.join(oc.RUN_LOGS, "vm-cache")
HOST_WAIT_MS = 150000


def gate_processes():
    return [c for _, _, c in oc.processes()
            if re.search(r"(gate_run|full_play_run|soak_run)\.py", c) and not c.startswith("/bin/")]


def prl(*args, timeout=600):
    return subprocess.run(["prlctl", *args], capture_output=True, text=True, timeout=timeout)


def vm_state(vm):
    m = re.search(r"State: (\w+)", prl("list", "-i", vm).stdout)
    return m.group(1) if m else None


def vm_up(vm):
    """Starts or resumes the virtual machine and waits until exec works. Returns the state it had."""
    state = vm_state(vm)
    if state is None:
        raise SystemExit(f"vm_join_run.py: no virtual machine named {vm}")
    r = prl("resume", vm) if state == "suspended" else prl("start", vm) if state == "stopped" else None
    if r is not None and "only in Parallels Desktop for Mac Pro" in r.stdout + r.stderr:
        # Checked 2026-10-29: this Mac has the standard edition, which refuses resume, start and exec from prlctl.
        raise SystemExit("vm_join_run.py: prlctl needs Parallels Desktop Pro or Business Edition for resume, "
                         "start and exec; this Mac's edition refuses them")
    end = time.time() + 600
    while time.time() < end:
        r = prl("exec", vm, "cmd", "/c", "echo", "ready", timeout=60)
        if r.returncode == 0 and "ready" in r.stdout:
            return state
        time.sleep(10)
    raise SystemExit(f"vm_join_run.py: {vm} does not run commands after 10 minutes (state was {state})")


def mac_address_for_vm():
    """The Mac's address on the 10.211.55.x network of Parallels (shared network)."""
    text = subprocess.run(["ifconfig"], capture_output=True, text=True).stdout
    m = re.search(r"inet (10\.211\.55\.\d+) ", text)
    return m.group(1) if m else None


class Handler(http.server.SimpleHTTPRequestHandler):
    """GET serves the folder; PUT /up/<name> writes <upload folder>/<name>."""

    def __init__(self, *a, upload=None, **k):
        self.upload = upload
        super().__init__(*a, **k)

    def do_PUT(self):
        name = os.path.basename(self.path)
        if not self.path.startswith("/up/") or not re.fullmatch(r"[\w.-]+", name):
            self.send_error(400)
            return
        n = int(self.headers.get("Content-Length", 0))
        with open(os.path.join(self.upload, name), "wb") as f:
            while n > 0:
                chunk = self.rfile.read(min(n, 1 << 20))
                if not chunk:
                    break
                f.write(chunk)
                n -= len(chunk)
        self.send_response(201)
        self.end_headers()

    def log_message(self, *a):
        pass


def serve(folder, upload, address):
    h = functools.partial(Handler, directory=folder, upload=upload)
    s = http.server.ThreadingHTTPServer((address, 0), h)
    threading.Thread(target=s.serve_forever, daemon=True).start()
    return s, f"http://{address}:{s.server_address[1]}"


def pack_game(game):
    """game.zip of the game folder, made once."""
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, "game.zip")
    if not os.path.exists(path):
        tmp = path + ".part"
        with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as z:
            for root, _, files in os.walk(game):
                for f in files:
                    full = os.path.join(root, f)
                    z.write(full, os.path.relpath(full, game))
        os.replace(tmp, path)
    return path


def pack_build(win_build, folder):
    path = os.path.join(folder, "win.zip")
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(os.path.join(win_build, "cm0102_recomp.exe"), "cm0102_recomp.exe")
        for root, _, files in os.walk(os.path.join(win_build, "seasons")):
            for f in files:
                full = os.path.join(root, f)
                z.write(full, os.path.relpath(full, win_build))
    return path


def ps_quote(s):
    return "'" + s.replace("'", "''") + "'"


PREPARE = r"""$ErrorActionPreference = 'Stop'
$d = '{dir}'
New-Item -ItemType Directory -Force -Path $d | Out-Null
if (-not (Test-Path "$d\game\cm0102.exe")) {{
  Invoke-WebRequest -UseBasicParsing -Uri '{url}/game.zip' -OutFile "$d\game.zip"
  Expand-Archive -Force -Path "$d\game.zip" -DestinationPath "$d\game"
  Remove-Item "$d\game.zip"
}}
if (Test-Path "$d\build") {{ Remove-Item -Recurse -Force "$d\build" }}
Invoke-WebRequest -UseBasicParsing -Uri '{url}/win.zip' -OutFile "$d\win.zip"
Expand-Archive -Force -Path "$d\win.zip" -DestinationPath "$d\build"
Write-Output 'prepared'
"""

JOIN = r"""$ErrorActionPreference = 'Continue'
$d = '{dir}'
$w = "$d\run-{name}"
if (Test-Path $w) {{ Remove-Item -Recurse -Force $w }}
New-Item -ItemType Directory -Force -Path "$w\user", "$w\shots" | Out-Null
Set-Content -Path "$w\user\settings.txt" -Value "display = classic"
$env:SDL_VIDEODRIVER = 'dummy'; $env:SDL_AUDIODRIVER = 'dummy'; $env:RECOMP_NO_BOX = '1'
$env:RECOMP_USER_DIR = "$w\user"; $env:HOME = "$w\user"; $env:RECOMP_SCREEN_LOG = '1'
$env:RECOMP_NET_LOG = "$w\shots\{name}-net.log"
{env}
$env:RECOMP_SCRIPT = {script}
$p = Start-Process -FilePath "$d\build\cm0102_recomp.exe" -ArgumentList '--game', "$d\game" -PassThru -Wait `
  -RedirectStandardError "$w\shots\{name}-stderr.txt" -RedirectStandardOutput "$w\shots\{name}-stdout.txt"
Set-Content -Path "$w\shots\{name}-exit.txt" -Value $p.ExitCode
Get-ChildItem "$w\shots" -File | ForEach-Object {{
  Invoke-WebRequest -UseBasicParsing -Method Put -InFile $_.FullName -Uri ('{url}/up/' + $_.Name) | Out-Null
}}
Write-Output ('joined, exit ' + $p.ExitCode)
"""


def vm_ps(vm, url, script_name, timeout):
    """Fetches a script from the Mac and runs it in PowerShell. The command goes as -EncodedCommand (base64 of
    UTF-16LE), so no shell between prlctl and PowerShell can split its quotes or "&"."""
    cmd = (f"$ProgressPreference='SilentlyContinue'; New-Item -ItemType Directory -Force -Path '{WIN_DIR}' | "
           f"Out-Null; Invoke-WebRequest -UseBasicParsing -Uri '{url}/{script_name}' "
           f"-OutFile '{WIN_DIR}\\{script_name}'; & '{WIN_DIR}\\{script_name}'")
    encoded = base64.b64encode(cmd.encode("utf-16-le")).decode()
    return prl("exec", vm, "powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-EncodedCommand", encoded,
               timeout=timeout)


def one_join(o, via, serve_dir, url):
    out = os.path.join(o["out"], via)
    os.makedirs(out, exist_ok=True)
    name = "00VM-" + time.strftime("%H%M%S")   # one word; 00 makes it the first row of a list of lobby rows
    lobby = o["server"] if via == "relay" else None
    failed = []

    def check(ok, text):
        print(("ok   " if ok else "FAIL ") + f"{via}: {text}", flush=True)
        if not ok:
            failed.append(text)

    jname = "join1"
    steps = remote_join.row_steps(1, 0) if via == "relay" else nr.joiner_steps(o["mac"], 1)
    script = "; ".join(smoke_run.scaled_steps("classic", WIN_DIR + f"\\run-{jname}\\shots", steps))
    env = ""
    if lobby:
        env = f"$env:RECOMP_LOBBY_URL = {ps_quote(lobby)}; $env:RECOMP_TEST_NO_LAN_ROWS = '1'"
    with open(os.path.join(serve_dir, "join.ps1"), "w", newline="\r\n") as f:
        f.write(JOIN.format(dir=WIN_DIR, name=jname, env=env, script=ps_quote(script), url=url))

    oc.wait_for_room()
    host_env = {"RECOMP_LOBBY_URL": lobby} if lobby else {}
    host, err = nr.start(o["program"], o["game"], os.path.join(out, "work"), "host",
                         oc.host_steps(name, HOST_WAIT_MS), out, 1, host_env)
    try:
        ready = nr.wait_for(os.path.join(out, "host-stderr.txt"), f"screen {nr.NAME_BUILD}", host, 240)
        check(ready, "the host reaches its Enter Name screen")
        r = vm_ps(o["vm"], url, "join.ps1", 600) if ready else None
        if r is not None:
            open(os.path.join(out, "vm-join.txt"), "w").write(r.stdout + r.stderr)
        host.wait(timeout=HOST_WAIT_MS / 1000 + 240)
    except subprocess.TimeoutExpired:
        pass
    finally:
        if host.poll() is None:
            host.kill()
            host.wait()
        err.close()

    up = os.path.join(serve_dir, "up")
    for f in os.listdir(up):
        shutil.move(os.path.join(up, f), os.path.join(out, f))
    code = open(os.path.join(out, f"{jname}-exit.txt")).read().strip() if os.path.exists(
        os.path.join(out, f"{jname}-exit.txt")) else "none"
    check(host.returncode == 0, f"1 the host ended with code {host.returncode}")
    check(code == "0", f"1 the joiner ended with code {code}")
    hl = nr.connections(os.path.join(out, "host-net.log"))
    joined = [c for c in hl.values() if not c["peer"].endswith(":6667") and
              (c["peer"].startswith("0.66.255.") if via == "relay" else not c["peer"].startswith("127."))]
    check(len(joined) == 1 and joined[0]["packets_in"] and joined[0]["packets_out"],
          f"2 host log: {len(joined)} joiner connections with packets both ways, 1 expected")
    jlog = os.path.join(out, f"{jname}-net.log")
    jl = nr.connections(jlog)
    peer = next((p for p in jl if p.startswith("0.66.") and p.endswith(":6667")), None) if via == "relay" \
        else f"{o['mac']}:6667"
    c = jl.get(peer) if peer else None
    check(bool(c and c["packets_in"] and c["packets_out"]), f"3 joiner log: the connection to {peer}")
    clicks = nr.click_packets(jlog, peer) if peer else None
    check(clicks == nr.JOINER_CLIENT_CLICKS, f"3 the joiner sent {clicks} click packets, {nr.JOINER_CLIENT_CLICKS} "
                                             "expected")
    for host_shot, suffix, what in (("host-1-name.bmp", "1-name", "Enter Name"),
                                    ("host-2-typed.bmp", "2-typed", "Enter Name with the typed names"),
                                    ("host-3-nation.bmp", "3-nation", "Select Nationality after Next")):
        a, b = os.path.join(out, host_shot), os.path.join(out, f"{jname}-{suffix}.bmp")
        if os.path.exists(a) and os.path.exists(b):
            share = smoke_run.differing_share(a, b)
            check(share <= 0.01, f"4 the joiner's {what} equals the host's: {share:.2%} of pixels differ")
        else:
            check(False, f"4 the joiner's {what}: dump missing")
    if via == "relay":
        jerr = open(os.path.join(out, f"{jname}-stderr.txt"), errors="replace").read() if os.path.exists(
            os.path.join(out, f"{jname}-stderr.txt")) else ""
        check("recomp: relay: joined" in jerr, "5 the joiner joined through the relay")
    oc.put_result("release2", via, "PASS" if not failed else "FAIL",
                  "; ".join(failed[:2]) or f"game {name}", out)
    return not failed


def main(argv):
    args = argv[1:]
    if len(args) < 2 or args[0].startswith("--"):
        print(__doc__)
        return 2
    o = {"program": os.path.abspath(args.pop(0)), "win": os.path.abspath(args.pop(0)), "game": oc.GAME,
         "via": None, "server": oc.REAL_LOBBY, "vm": VM, "mac": mac_address_for_vm(), "out": None}
    gates_done, keep_vm = False, False
    while args:
        k = args.pop(0)
        if k == "--gates-done":
            gates_done = True
            continue
        if k == "--keep-vm":
            keep_vm = True
            continue
        if not args:
            print(__doc__)
            return 2
        v = args.pop(0)
        key = {"--game": "game", "--via": "via", "--server": "server", "--vm": "vm", "--mac-address": "mac",
               "--out": "out"}.get(k)
        if not key:
            print(__doc__)
            return 2
        o[key] = v
    if o["server"] == "real":
        o["server"] = oc.REAL_LOBBY
    if not gates_done:
        print("vm_join_run.py: the virtual machine starts only after the 0.6.0 gates finish (Q30). Give "
              "--gates-done when they did.")
        return 2
    running = gate_processes()
    if running:
        print(f"vm_join_run.py: a gate run still runs ({running[0][:120]}); the virtual machine waits")
        return 2
    if not os.path.exists(os.path.join(o["win"], "cm0102_recomp.exe")):
        print(f"vm_join_run.py: no cm0102_recomp.exe in {o['win']}")
        return 2
    if not o["mac"]:
        print("vm_join_run.py: no 10.211.55.x address on this Mac; give --mac-address")
        return 2
    relay = oc.relay_in_build(o["program"]) and oc.relay_in_build(os.path.join(o["win"], "cm0102_recomp.exe"))
    vias = {"lan": ["lan"], "relay": ["relay"], "both": ["lan", "relay"], None: ["lan", "relay"] if relay
            else ["lan"]}[o["via"]]
    if "relay" in vias and not relay:
        print("vm_join_run.py: the Mac build has no relay (slice 5)")
        return 2
    if "relay" in vias and not oc.lobby_ok(o["server"]):
        print(f"vm_join_run.py: the lobby {o['server']} does not answer")
        return 2
    o["out"] = os.path.abspath(o["out"] or os.path.join(oc.RUN_LOGS, time.strftime("release2-%Y%m%d-%H%M")))
    serve_dir = os.path.join(o["out"], "serve")
    os.makedirs(os.path.join(serve_dir, "up"), exist_ok=True)
    print("packing the game folder (once) and the Windows build", flush=True)
    if not os.path.exists(os.path.join(serve_dir, "game.zip")):
        os.link(pack_game(o["game"]), os.path.join(serve_dir, "game.zip"))
    pack_build(o["win"], serve_dir)
    server, url = serve(serve_dir, os.path.join(serve_dir, "up"), o["mac"])
    with open(os.path.join(serve_dir, "prepare.ps1"), "w", newline="\r\n") as f:
        f.write(PREPARE.format(dir=WIN_DIR, url=url))
    oc.wait_for_room()
    results, was = [], vm_state(o["vm"])
    try:
        vm_up(o["vm"])
        print(f"{o['vm']} runs (it was {was}); files at {url}", flush=True)
        r = vm_ps(o["vm"], url, "prepare.ps1", 1800)
        open(os.path.join(o["out"], "vm-prepare.txt"), "w").write(r.stdout + r.stderr)
        if "prepared" not in r.stdout:
            oc.put_result("release2", "prepare", "FAIL", "the virtual machine did not fetch the files "
                          "(vm-prepare.txt)", o["out"])
            return 1
        for via in vias:
            oc.wait_for_room()
            with oc.NetLock(f"vm_join_run.py {via}"):     # one host on port 6667 at a time (lead's net-lock.sh)
                results.append((via, one_join(o, via, serve_dir, url)))
    finally:
        server.shutdown()
        # Suspend the virtual machine only when this run started or resumed it.
        if not keep_vm and was in ("suspended", "stopped"):
            prl("suspend", o["vm"])
            print(f"{o['vm']} suspended", flush=True)
    ok = all(r for _, r in results)
    verdict = "FAIL" if not ok else "PASS" if "relay" in vias else "LAN"
    oc.put_result("release2", "all", verdict, {"PASS": "release test 2 passes", "LAN": "the LAN join passes; the "
                  "relay join waits for slice 5", "FAIL": "a join failed"}[verdict], o["out"])
    return {"PASS": 0, "LAN": 3, "FAIL": 1}[verdict]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
