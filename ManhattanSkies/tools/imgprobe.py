# imgprobe.py <lo hex> <hi hex> [mode]: scans the active atmosphere slot's floats between two offsets for post-process controls the first probe
# (fieldprobe.py) could not see: vignette (corners vs center), saturation, contrast, highlight area, edge sharpness (bloom/blur).
# For each finite nonzero float (and each zero float, tried at 1.0) it writes a test value, recomputes, screenshots the game window, restores,
# and prints the features that moved more than the baseline noise. Output: recon/imgprobe.csv. Stand still outdoors; takes ~4 s per float.
import csv
import os
import struct
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fieldprobe as fp

SKIP = [(0x2a8, 0x2c0), (0x300, 0x310), (0x47c, 0x484), (0x484, 0x488), (0x494, 0x498), (0x588, 0x5b0), (0x5b4, 0x5c4), (0x668, 0x674), (0x81c, 0x820)]
FLOOR = {"vignette": 0.03, "sat": 0.012, "contrast": 1.2, "bright": 0.006, "sharp": 0.35}


def features(a):
    """a: small RGB float array (H, W, 3), 0..255."""
    lum = a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114
    h, w = lum.shape
    ph, pw = int(h * 0.15), int(w * 0.15)
    corners = np.mean([lum[:ph, :pw].mean(), lum[:ph, -pw:].mean(), lum[-ph:, :pw].mean(), lum[-ph:, -pw:].mean()])
    center = lum[int(h * 0.3):int(h * 0.7), int(w * 0.3):int(w * 0.7)].mean()
    mx, mn = a.max(axis=2), a.min(axis=2)
    return {
        "vignette": float(corners / (center + 1e-3)),
        "sat": float(((mx - mn) / (mx + 1.0)).mean()),
        "contrast": float(lum.std()),
        "bright": float((lum > 190).mean()),
        "sharp": float(np.abs(np.diff(lum, axis=1)).mean()),
    }


def baseline():
    fp.send("recompute", wait=1.5)
    fs = []
    for _ in range(4):
        fs.append(features(fp.small(fp.shot())))
        time.sleep(0.5)
    base = {k: float(np.mean([f[k] for f in fs])) for k in fs[0]}
    noise = {k: max(abs(f[k] - base[k]) for f in fs) for k in fs[0]}
    return base, noise


def main():
    lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
    mode = sys.argv[3] if len(sys.argv) > 3 else "0"
    fp.send("skyreset", wait=1.0)
    fp.send("sky 2", wait=6.0)
    fp.send("sky " + mode, wait=7.0)
    slot = fp.active_slot()
    addr = fp.slot_address(slot)
    print("slot", slot, "at %x" % addr, flush=True)
    base, noise = baseline()
    print("baseline", {k: round(v, 3) for k, v in base.items()}, "noise", {k: round(v, 3) for k, v in noise.items()}, flush=True)
    out = os.path.join(fp.ROOT, "recon", "imgprobe.csv")
    new = not os.path.exists(out)
    with open(out, "a", newline="") as f:
        w = csv.writer(f)
        if new:
            w.writerow(["offset", "orig", "test", "dvignette", "dsat", "dcontrast", "dbright", "dsharp", "flags"])
        count = 0
        for off in range(lo, hi, 4):
            if any(a <= off < b for a, b in SKIP):
                continue
            raw = fp.peek(addr + off, 4)
            bits = struct.unpack("<I", raw)[0]
            v = fp.f32(bits)
            if not np.isfinite(v) or abs(v) > 1e6:
                continue   # not a float (an id, a pointer half or a flag)
            if 0 < abs(v) < 1e-4:
                continue
            test = 1.0 if v == 0 else v * 8.0
            fp.send("pokef %x %r" % (addr + off, test), "recompute", wait=1.4)
            feats = features(fp.small(fp.shot()))
            fp.send("poke32 %x %08x" % (addr + off, bits), "recompute", wait=1.0)
            d = {k: feats[k] - base[k] for k in feats}
            flags = [k for k in d if abs(d[k]) > max(noise[k] * 3.0, FLOOR[k])]
            w.writerow(["0x%x" % off, "%g" % v, "%g" % test] + ["%.4f" % d[k] for k in ("vignette", "sat", "contrast", "bright", "sharp")] + [" ".join(flags)])
            f.flush()
            count += 1
            if flags:
                print("+0x%x (%g -> %g): %s  %s" % (off, v, test, " ".join(flags), " ".join("%s%+.3f" % (k[:4], d[k]) for k in flags)), flush=True)
            if count % 20 == 0:
                base, noise = baseline()   # drift (clouds, the sun's flicker) moves the baseline
    print("done", count, "tested", flush=True)


main()
