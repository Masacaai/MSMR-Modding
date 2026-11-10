# raindiff.py <dump dir> : candidates for "the rain level" in the exe's .data, from raincapture.py's dumps.
# A word qualifies when it is zero in every Day dump, stable inside the Night pair and inside the Overcast pair, and nonzero (float-like) in both.
# Sunset and Sunrise are reported as extra columns (they may or may not rain).
import glob
import os
import struct
import sys

import numpy as np


def load(path):
    with open(path, "rb") as f:
        rva, size = struct.unpack("<II", f.read(8))
        return rva, np.frombuffer(f.read(size), dtype="<u4")[: size // 4]


d = sys.argv[1]
rva = None
sets = {}
for tag in ("rday", "rovc", "rnight", "rsunset", "rmorn"):
    arr = []
    for p in sorted(glob.glob(os.path.join(d, "dump_%s*.bin" % tag))):
        rva, a = load(p)
        arr.append(a)
    sets[tag] = arr
    print(tag, len(arr), "dumps")
n = min(len(a) for arrs in sets.values() for a in arrs)
for k in sets:
    sets[k] = [a[:n] for a in sets[k]]


def stable(arrs):
    m = np.ones(n, dtype=bool)
    for a in arrs[1:]:
        m &= a == arrs[0]
    return m


def isfloat(w):
    f = w.view("<f4")
    return np.isfinite(f) & (np.abs(f) > 1e-6) & (np.abs(f) < 1e6)


day_zero = np.ones(n, dtype=bool)
for a in sets["rday"]:
    day_zero &= a == 0
cand = day_zero & stable(sets["rovc"]) & stable(sets["rnight"]) & (sets["rovc"][0] != 0) & (sets["rnight"][0] != 0)
cand &= isfloat(sets["rovc"][0]) & isfloat(sets["rnight"][0])
idx = np.nonzero(cand)[0]
print("candidates (0 in Day, nonzero float in Night and Overcast): %d" % len(idx))
rows = []
for i in idx:
    ov = sets["rovc"][0][i:i + 1].view("<f4")[0]
    ni = sets["rnight"][0][i:i + 1].view("<f4")[0]
    su = sets["rsunset"][0][i:i + 1].view("<f4")[0] if sets["rsunset"] else float("nan")
    mo = sets["rmorn"][0][i:i + 1].view("<f4")[0] if sets["rmorn"] else float("nan")
    rows.append((rva + 4 * int(i), ov, ni, su, mo))
out = os.path.join(d, "raindiff_candidates.csv")
with open(out, "w") as f:
    f.write("rva,overcast,night,sunset,morning\n")
    for r in rows:
        f.write("0x%x,%g,%g,%g,%g\n" % r)
print("wrote", out)
for r in rows[:60]:
    print("  0x%x  overcast %g  night %g  sunset %g  morning %g" % r)
