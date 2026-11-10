# datadiff.py <day1.bin> <day2.bin> <night1.bin> <night2.bin> [out.csv]: candidates for "values the game keeps per lighting mode" in the exe's .data.
# A 4-byte word is a candidate when it is the same in both Day dumps, the same in both Night dumps, and differs between Day and Night (so it is neither noise
# nor a timer). Words that are halves of a pointer are dropped. Dumps come from the mod's `dumpdata <name>` command (u32 rva, u32 size, then the bytes).
import struct
import sys

import numpy as np


def load(path):
    with open(path, "rb") as f:
        rva, size = struct.unpack("<II", f.read(8))
        return rva, np.frombuffer(f.read(size), dtype="<u4")[: size // 4]


rva, d1 = load(sys.argv[1])
_, d2 = load(sys.argv[2])
_, n1 = load(sys.argv[3])
_, n2 = load(sys.argv[4])
n = min(len(d1), len(d2), len(n1), len(n2))
d1, d2, n1, n2 = d1[:n], d2[:n], n1[:n], n2[:n]
stable = (d1 == d2) & (n1 == n2)
cand = stable & (d1 != n1)
idx = np.nonzero(cand)[0]
print("words: %d, stable within both modes: %d, differ Day vs Night: %d" % (n, stable.sum(), len(idx)))

# drop pointer halves: an 8-byte pair whose high word looks like a user-space address high half (0x000001xx .. 0x00007fxx)
def pointer_like(i):
    hi = d1[i | 1] if i % 2 == 0 else d1[i]
    return 0x0 < hi < 0x8000 and (d1[i | 1] if i % 2 == 0 else d1[i]) in range(1, 0x8000)

out = []
for i in idx:
    day, night = int(d1[i]), int(n1[i])
    pair_hi = int(d1[i + 1]) if i % 2 == 0 and i + 1 < n else int(d1[i - 1]) if i > 0 else 0
    if 0x10 <= pair_hi < 0x8000 and (day > 0xffff or night > 0xffff):
        continue   # looks like half of a pointer
    fd = struct.unpack("<f", struct.pack("<I", day))[0]
    fn = struct.unpack("<f", struct.pack("<I", night))[0]
    isf = lambda v: v == v and (v == 0 or 1e-4 < abs(v) < 1e7)
    out.append((rva + 4 * int(i), day, night, fd, fn, isf(fd) and isf(fn)))
print("after dropping pointer halves: %d (float-like in both: %d)" % (len(out), sum(1 for o in out if o[5])))
path = sys.argv[5] if len(sys.argv) > 5 else None
if path:
    import csv
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["rva", "day_bits", "night_bits", "day_float", "night_float", "floatlike"])
        for o in out:
            w.writerow(["0x%x" % o[0], "0x%08x" % o[1], "0x%08x" % o[2], "%g" % o[3], "%g" % o[4], int(o[5])])
# a compact cluster summary: consecutive candidates within 64 bytes
clusters = []
for o in out:
    if clusters and o[0] - clusters[-1][-1][0] <= 64:
        clusters[-1].append(o)
    else:
        clusters.append([o])
print("clusters (candidates within 64 bytes of each other): %d" % len(clusters))
for c in clusters[:60]:
    fl = [o for o in c if o[5]]
    print("  rva 0x%x..0x%x  %d words (%d float-like)  e.g. %s" % (c[0][0], c[-1][0], len(c), len(fl),
          ", ".join("0x%x:%g->%g" % (o[0], o[3], o[4]) for o in fl[:4])))
