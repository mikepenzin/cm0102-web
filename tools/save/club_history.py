"""The club records history of a save: each club's list of season records (open item 192).

club records.dat (Club_Records::save 0x451ff0): int, int n1, n1 records of 0x38b bytes, int, int n2, n2 records of
0x38b bytes, int n3, n3 club entries of 0x6a bytes, int, int n4, n4 entries of 6 bytes. A club entry holds the file
offsets of the first and the last node of its list at +8 and +0xc (-1 for none) and its count of seasons at +0x68.

club records history.tmp (FILE_LLIST, add_node 0x58e340): a node is the season year (u16), the record (0x38b
bytes, Club_Records::end_season_for_nation 0x447880) and the offset of the next node (s32, -1 at the end).
get_nodes 0x58f120 reads from the first node to the last and stops there.

Usage: python3 tools/save/club_history.py <save> [<save> ...]
Prints for each save the size of the history file, the clubs with a list and the count of clubs by list length.
"""
import collections
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from savfile import Sav  # noqa: E402

RECORD = 0x38B
NODE_DATA = 2 + RECORD
CLUB = 0x6A


def clubs(records):
    """[(head, tail, seasons)] of the club entries of club records.dat."""
    at = 4
    n1 = struct.unpack_from("<i", records, at)[0]
    at += 4 + n1 * RECORD + 4
    n2 = struct.unpack_from("<i", records, at)[0]
    at += 4 + n2 * RECORD
    n3 = struct.unpack_from("<i", records, at)[0]
    at += 4
    out = []
    for i in range(n3):
        head, tail = struct.unpack_from("<ii", records, at + i * CLUB + 8)
        seasons = struct.unpack_from("<h", records, at + i * CLUB + 0x68)[0]
        out.append((head, tail, seasons))
    return out


def walk(history, head, tail, limit=100000):
    """The years of the list from head to tail, or None for a list that does not reach its tail."""
    if head < 0 or tail < 0:
        return []
    years, at = [], head
    while True:
        if at < 0 or at + NODE_DATA + 4 > len(history) or len(years) > limit:
            return None
        years.append(struct.unpack_from("<H", history, at)[0])
        if at == tail:
            return years
        at = struct.unpack_from("<i", history, at + NODE_DATA)[0]


def lists(path):
    """(size of the history file, [years of each club's list or None])."""
    s = Sav(path)
    records = s.read_member("club records.dat")
    history = s.read_member("club records history.tmp")
    return len(history), [walk(history, h, t) for h, t, _ in clubs(records)]


def main(argv):
    for p in argv[1:]:
        size, ls = lists(p)
        count = collections.Counter(len(y) if y is not None else -1 for y in ls)
        print(f"{p}: history {size} bytes, {sum(1 for y in ls if y)} clubs with a list, "
              f"broken {count.get(-1, 0)}, by length {sorted(count.items())}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
