# slotcompare.py <log> <mark> <mod.smpcmod> <base atmosphere id hex>: compares a live slot dump (mod command `peek` x3 between `mark <mark>` and `mark <mark>END`, 0x8c0 bytes)
# with what the game's parser should have copied into it from the mod's atmosphere (RECON Part 19 delta table), and prints every word that differs: leftovers of the slot's
# previous occupant (slots are recycled without being cleared) and values the parser does not copy.
import re
import struct
import sys
import zipfile

sys.argv_saved = sys.argv
log, mark, mod, base = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4], 16)
sys.argv = ["x"]
import atmslot

text = open(log, encoding="utf-8", errors="replace").read()
a = text.rindex("mark: " + mark + " ")
b = text.index(mark + "END", a)
mem = {}
for m in re.finditer(r"peek: ([0-9a-f]+)  ((?:[0-9a-f]{2} )+)", text[a:b]):
    addr = int(m[1], 16)
    for i, byte in enumerate(bytes.fromhex(m[2].replace(" ", ""))):
        mem[addr + i] = byte
lo = min(mem)
live = bytes(mem.get(lo + i, 0) for i in range(0x8c0))
z = zipfile.ZipFile(mod)
n = next(n for n in z.namelist() if n.upper().endswith("_%016X" % base))
data = z.read(n)
i = data.find(b"\x31\x54\x41\x44")
nsec = struct.unpack_from("<H", data, i + 12)[0]
sec = None
for k in range(nsec):
    tag, off, size = struct.unpack_from("<III", data, i + 16 + 12 * k)
    if tag == atmslot.SECTION:
        sec = data[i + off:i + off + size]
print("slot at %x; mod block %d bytes; live id %016x" % (lo, len(sec), struct.unpack_from("<Q", live, 8)[0]))
diffs = 0
for off in range(0x70, len(sec) - 3, 4):
    s = atmslot.slot_of(off)
    if s is None:
        continue
    want = struct.unpack_from("<I", sec, off)[0]
    got = struct.unpack_from("<I", live, s)[0]
    if want != got:
        diffs += 1
        print("slot+%03x (sec+%03x): parser should give %08x %-12.6g live %08x %-12.6g" % (s, off, want, struct.unpack_from("<f", sec, off)[0], got, struct.unpack_from("<f", live, s)[0]))
print(diffs, "differing words")
