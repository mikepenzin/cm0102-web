"""Presses the Windows keys with RECOMP_KEYS_WINDOWS set and checks the log.
Usage: windows_keys_run.py <recomp program> <game folder>
Ctrl+[ must press Back. F11 must switch full screen. Cmd+[, Cmd+F, Alt+Left and Alt+Enter must do nothing."""
import os
import subprocess
import sys
import tempfile


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keys = ["ctrl+[", "f11", "gui+[", "gui+f", "alt+left", "alt+return"]
    expect = ["key back", "key full screen", None, None, None, None]
    failed = 0
    with tempfile.TemporaryDirectory(prefix="cm0102-winkeys-") as work:
        for key, want in zip(keys, expect):
            user = os.path.join(work, "u" + str(len(os.listdir(work))))
            os.makedirs(user)
            with open(os.path.join(user, "settings.txt"), "w") as f:
                f.write("display = classic\ndatabase = 3.9.68\n")
            env = dict(os.environ, SDL_VIDEODRIVER="dummy", RECOMP_NO_BOX="1", RECOMP_KEYS_WINDOWS="1",
                       RECOMP_USER_DIR=user, RECOMP_SCRIPT=f"3000 press {key}; 5000 quit")
            out = subprocess.run([program, "--game", game], env=env, capture_output=True, text=True, timeout=120).stderr
            got = [m for m in ("key back", "key full screen") if "recomp: " + m in out]
            ok = got == ([want] if want else [])
            print(("ok    " if ok else "FAIL  ") + f"{key}: {got or 'nothing'}")
            failed += not ok
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
