# pausediff.py <dump dir>: finds a game clock in the exe's .data: words that change between the two unpaused dumps (pu1, pu2), stay identical
# between the two paused dumps (pp1, pp2) and change again after resuming (pu3), increasing over time.
import os
import struct
import sys

import numpy as np


def load(path):
    with open(path, "rb") as f:
        rva, size = struct.unpack("<II", f.read(8))
        return rva, np.frombuffer(f.read(size), dtype="<u4")[: size // 4]


d = sys.argv[1]
rva, u1 = load(os.path.join(d, "dump_pu1.bin"))
_, u2 = load(os.path.join(d, "dump_pu2.bin"))
_, p1 = load(os.path.join(d, "dump_pp1.bin"))
_, p2 = load(os.path.join(d, "dump_pp2.bin"))
_, u3 = load(os.path.join(d, "dump_pu3.bin"))
n = min(map(len, (u1, u2, p1, p2, u3)))
u1, u2, p1, p2, u3 = (a[:n] for a in (u1, u2, p1, p2, u3))
cand = (u1 != u2) & (p1 == p2) & (p2 != u2) & (u3 != p2)
idx = np.nonzero(cand)[0]
print("changing unpaused, frozen paused, moving again after: %d words" % len(idx))
rows = []
for i in idx:
    a, b, c, e = int(u1[i]), int(u2[i]), int(p1[i]), int(u3[i])
    fa, fb, fc, fe = [struct.unpack("<f", struct.pack("<I", x))[0] for x in (a, b, c, e)]
    inc_u32 = a < b < e or (a < b and b < e)
    inc_f = fa < fb and fb < fe and fa == fa
    rows.append((rva + 4 * int(i), a, b, c, e, fa, fb, fc, fe, inc_u32, inc_f))
shown = 0
for r in rows:
    if (r[9] or r[10]) and shown < 40:
        print("  0x%x  u32 %d -> %d (paused %d) -> %d | f32 %g -> %g (paused %g) -> %g  %s%s" % (r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], "U" if r[9] else "", "F" if r[10] else ""))
        shown += 1
print("monotonic candidates shown:", shown)
