"""Read and write the container of a CM 01/02 saved game (.sav).

Format (zip.cpp of the original, cm0102.exe 3.9.68):
- Header of 0x416ec bytes at offset 0: int version, int unknown, int file count, then 1000 entries of
  0x10c bytes: int offset, int size (uncompressed), char name[260]. Read at 0x9326e0, written at 0x932b00.
  Version 1 and 3 mean "stored", version 2 and 4 mean "compressed" (0x9326e0 maps 1 to 3 and 2 to 4).
- Each member file follows the header. Its offset is in its entry. A compressed member is a byte run
  code: a byte below 0x81 is one literal byte, a byte b of 0x81 or more is followed by one byte that
  repeats b - 0x80 times. Decoder 0x932c30, encoder 0x932e10. The file has no checksum.

Merged parts (0.6.2 slice 13, src/recomp_rt/win/save_parts.c): the temporary files of one type (the name with each
run of digits as '#', "fixtures_#.tmp") go into one member named after the type with ".parts" when the type has two
files or more. The member holds b"BEPARTS1", the count of files (u32), and for each file the length of its name
(u32), the name, its size (u32) and its bytes. Sav lists each of these files as a member of its own (virtual: it can
be read, not written), after the members of the file, so a tool reads "fixtures_2001.tmp" from any save.

The tool keeps the header bytes and every member that it does not change, so only the changed members
and the offsets differ from the input file. add_member appends a member.
"""
import struct

MAGIC = b"BEPARTS1"
MERGED = ".parts"
HEADER_SIZE = 0x416EC
ENTRY_SIZE = 0x10C
MAX_ENTRIES = 1000


class Member:
    def __init__(self, index, offset, size, name):
        self.index, self.offset, self.size, self.name = index, offset, size, name
        self.raw = b""        # bytes as stored in the .sav
        self.data = None      # decoded bytes, set by Sav.read_member
        self.changed = False
        self.merged_in = None # the merged member that holds this file (virtual member), else None


def decode(raw, size):
    out = bytearray()
    i, n = 0, len(raw)
    while len(out) < size and i < n:
        b = raw[i]
        i += 1
        if b < 0x81:
            out.append(b)
        else:
            out += bytes([raw[i]]) * (b - 0x80)
            i += 1
    if len(out) < size:
        raise ValueError(f"member ends early: {len(out)} of {size} bytes")
    return bytes(out[:size]), i


def encode(data):
    """Same output as the encoder at 0x932e10: runs of up to 0x7f, a single byte below 0x81 stays literal."""
    out = bytearray()
    i, n = 0, len(data)
    while i < n:
        b = data[i]
        j = i + 1
        while j < n and data[j] == b and j - i < 0x7F:
            j += 1
        count = j - i
        if count < 2 and b < 0x81:
            out.append(b)
        else:
            out += bytes([count + 0x80, b])
        i = j
    return bytes(out)


def split_merged(data):
    """[(name, bytes)] of a merged member."""
    if data[:8] != MAGIC:
        raise ValueError("a merged member without its mark")
    count, at, out = struct.unpack_from("<I", data, 8)[0], 12, []
    for _ in range(count):
        n = struct.unpack_from("<I", data, at)[0]
        name = data[at + 4:at + 4 + n].decode("latin-1")
        size = struct.unpack_from("<I", data, at + 4 + n)[0]
        at += 8 + n
        out.append((name, data[at:at + size]))
        at += size
    if at != len(data):
        raise ValueError("a merged member with bytes after its files")
    return out


def merge(files):
    """The bytes of a merged member of [(name, bytes)] (in that order)."""
    out = bytearray(MAGIC + struct.pack("<I", len(files)))
    for name, data in files:
        b = name.encode("latin-1")
        out += struct.pack("<I", len(b)) + b + struct.pack("<I", len(data)) + data
    return bytes(out)


class Sav:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.blob = f.read()
        self.version, self.unknown, count = struct.unpack_from("<3i", self.blob, 0)
        if self.version not in (1, 2, 3, 4) or not 0 <= count <= MAX_ENTRIES:
            raise ValueError(f"not a saved game: version {self.version}, {count} files")
        self.compressed = self.version in (2, 4)
        self.members = []
        for k in range(count):
            o = 12 + k * ENTRY_SIZE
            off, size = struct.unpack_from("<2I", self.blob, o)   # unsigned: a save may pass 2 GB
            name = self.blob[o + 8:o + ENTRY_SIZE].split(b"\0")[0].decode("latin-1")
            self.members.append(Member(k, off, size, name))
        order = sorted(self.members, key=lambda m: m.offset)
        for a, b in zip(order, order[1:] + [None]):
            end = b.offset if b else len(self.blob)
            a.raw = self.blob[a.offset:end]
        for m in list(self.members):
            if m.name.lower().endswith(MERGED):
                for name, data in split_merged(self.read_member(m.name)):
                    v = Member(None, None, len(data), name)
                    v.data, v.merged_in = data, m.name
                    self.members.append(v)

    def real_members(self):
        """The members of the file, without the files of merged members."""
        return [m for m in self.members if m.merged_in is None]

    def member(self, name):
        for m in self.members:
            if m.name.lower() == name.lower():
                return m
        raise KeyError(name)

    def read_member(self, name):
        m = self.member(name)
        if m.data is None:
            if self.compressed:
                m.data, used = decode(m.raw, m.size)
                m.raw = m.raw[:used]
            else:
                m.data = m.raw[:m.size]
        return m.data

    def write_member(self, name, data):
        m = self.member(name)
        if m.merged_in:
            raise ValueError(f"{name} is in the merged member {m.merged_in}: the tool cannot change it")
        self.read_member(name)
        if len(data) != m.size:
            raise ValueError("the tool keeps member sizes unchanged")
        m.data, m.changed = bytes(data), True

    def add_member(self, name, data):
        """Appends a member (a name of at most 259 bytes). The container holds at most 1000 rows."""
        real = self.real_members()
        if len(real) >= MAX_ENTRIES:
            raise ValueError("the container is full")
        if len(name.encode("latin-1")) > 259:
            raise ValueError("a member name has at most 259 bytes")
        m = Member(len(real), len(self.blob) + 1 << 40, len(data), name)   # after every member of the file
        m.data, m.changed = bytes(data), True
        self.members.insert(len(real), m)
        return m

    def save(self, path):
        header = bytearray(self.blob[:HEADER_SIZE])
        real = self.real_members()
        struct.pack_into("<i", header, 8, len(real))
        for m in real:
            o = 12 + m.index * ENTRY_SIZE
            if m.offset >> 40:                             # a new row
                header[o:o + ENTRY_SIZE] = bytes(ENTRY_SIZE)
                struct.pack_into("<I", header, o + 4, m.size)
                header[o + 8:o + 8 + len(m.name.encode("latin-1"))] = m.name.encode("latin-1")
        body = bytearray()
        for m in sorted(real, key=lambda m: m.offset):
            if m.changed:
                raw = encode(m.data) if self.compressed else m.data
            else:
                raw = m.raw
            offset = HEADER_SIZE + len(body)
            struct.pack_into("<I", header, 12 + m.index * ENTRY_SIZE, offset)
            body += raw
        with open(path, "xb") as f:   # never overwrite a file
            f.write(header + body)
