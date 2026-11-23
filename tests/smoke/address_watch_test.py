#!/usr/bin/env python3
"""ctest address_watch (item 216): the network scripts' AddressWatch (tests/smoke/network_run.py) says nothing while the
run's address stays this computer's, and names the address and the time when it is not (as when the system removes
a LAN address at a network drop). No game runs."""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import network_run as nr  # noqa: E402


def main():
    bad = []
    here = nr.lan_address()
    if not nr.is_local(here) or not nr.is_local("127.0.0.1"):
        bad.append(f"is_local: {here} or 127.0.0.1 is not this computer's")
    if nr.is_local("192.0.2.55"):                       # RFC 5737: never a local address
        bad.append("is_local: 192.0.2.55 counts as this computer's")
    stays = nr.AddressWatch(here)
    gone = nr.AddressWatch("192.0.2.55")
    time.sleep(1.2)
    if stays.end():
        bad.append(f"a staying address gave {stays.end()!r}")
    text = gone.end()
    if "the test address 192.0.2.55 went away during the run" not in text:
        bad.append(f"a missing address gave {text!r}")
    lines = []
    nr.check_address(lambda ok, t: lines.append((ok, t)), nr.AddressWatch(here))
    if lines != [(True, f"the test address {here} stayed for the whole run")]:
        bad.append(f"check_address: {lines}")
    for b in bad:
        print("FAIL  " + b)
    print(("ok    " if not bad else "FAIL  ") + "address_watch: the run's address is watched")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
