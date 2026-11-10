# fieldprobe.py: automated A/B probe of the live atmosphere slot (dev tool; the game must be running with ManhattanSkies loaded, the camera still, the game
# window visible and on top). For each float in the active slot it writes a test value, re-blends, screenshots the game window, compares against a noise
# baseline (rain and flickering lights move on their own), restores the value, and records how big and where the change was.
#
#   python fieldprobe.py validate            known fields (sun direction, sun color, fog, ambient): checks the method
#   python fieldprobe.py scan [from to]      every candidate float in +from..+to of the slot (hex offsets; default 0x2a0 0x700)
#   python fieldprobe.py one <hexoffset> <value> [<value> ...]   probe single values
#
# Results go to recon/fieldprobe_<mode>.csv; screenshots of notable changes to recon/probe/.
import ctypes
import csv
import math
import os
import re
import struct
import subprocess
import sys
import time

import numpy as np
from PIL import ImageGrab

ctypes.windll.user32.SetProcessDPIAware()

GAME = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\scripts"
CMD = GAME + r"\ManhattanSkies.cmd"
LOG = GAME + r"\ManhattanSkies.log"
ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT_DIR = ROOT + r"\recon\probe"
os.makedirs(OUT_DIR, exist_ok=True)


# ---------- talking to the mod ----------
def send(*lines, wait=0.6):
    with open(CMD, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    time.sleep(wait)


def log_text():
    with open(LOG, encoding="utf-8", errors="ignore") as f:
        return f.read()


def mark(name):
    send("mark " + name, wait=0.5)
    return name


def after(marker):
    t = log_text()
    i = t.rfind("==== mark: " + marker + " ====")
    return t[i:] if i >= 0 else ""


def peek(addr, length):
    m = mark("pk%d" % int(time.time() * 1000))
    send("peek %x %d" % (addr, length), wait=0.7)
    mem = {}
    for line in after(m).splitlines():
        r = re.search(r"peek: ([0-9a-f]+)\s+(.*)", line)
        if r:
            a = int(r.group(1), 16)
            for i, b in enumerate(r.group(2).split()):
                mem[a + i] = int(b, 16)
    return bytes(mem.get(addr + i, 0) for i in range(length))


def slot_address(idx):
    m = mark("sa%d" % int(time.time() * 1000))
    send("slotaddr %d" % idx, wait=0.7)
    r = re.search(r"slotaddr: slot \d+ at ([0-9a-f]+)", after(m))
    return int(r.group(1), 16)


def active_slot():
    """The slot that is the visible atmosphere: the sky override's slot when one is on, else the base slot."""
    m = mark("ss%d" % int(time.time() * 1000))
    send("skystate", wait=0.7)
    t = after(m)
    r = re.search(r"look ([0-9A-F]{16}) slot (-?\d+) \| base slot (-?\d+)", t)
    look, lslot, bslot = r.group(1), int(r.group(2)), int(r.group(3))
    return lslot if look != "0" * 16 and lslot >= 0 else bslot


# ---------- screenshots ----------
def game_rect():
    import ctypes.wintypes as wt
    user32 = ctypes.windll.user32
    hwnd = None
    out = subprocess.run(["powershell", "-NoProfile", "-Command", "(Get-Process Spider-Man).MainWindowHandle"], capture_output=True, text=True).stdout.strip()
    hwnd = int(out)
    rc = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rc))
    pt = wt.POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    return pt.x, pt.y, rc.right, rc.bottom


RECT = None


def shot():
    global RECT
    if RECT is None:
        RECT = game_rect()
    x, y, w, h = RECT
    return ImageGrab.grab(bbox=(x, y, x + w, y + h)).convert("RGB")


def small(img):
    return np.asarray(img.resize((img.width // 6, img.height // 6)), dtype=np.float32)


def compare(a, b):
    """(overall mean abs diff, sky diff, city diff, brightness change) between two small arrays."""
    d = np.abs(a - b)
    h = d.shape[0]
    top, bottom = d[int(h * 0.1):int(h * 0.45)], d[int(h * 0.45):int(h * 0.9)]
    lum = lambda x: float(x.mean())
    return float(d.mean()), float(top.mean()), float(bottom.mean()), lum(b) - lum(a)


def noise_level(n=4):
    send("recompute", wait=1.5)   # a re-blend nudges the image a little: take the baseline after one, like every probe
    frames = []
    for _ in range(n):
        frames.append(small(shot()))
        time.sleep(0.5)
    ds = [compare(frames[i], frames[i + 1])[0] for i in range(n - 1)]
    return max(ds), frames[0]


# ---------- probing ----------
def verdict(r, thr):
    o, sky, dl = r[2], r[3], r[5]
    if o > 8 or abs(dl) > 1.0 or sky > 8:
        return "EFFECT"
    return "weak" if o > thr else ""


def f32(bits):
    return struct.unpack("<f", struct.pack("<I", bits))[0]


def bits_of(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def probe(addr, original_bits, values, base_img, noise, tag):
    """Write each test value, recompute, screenshot, compare with base_img, restore. Returns the rows."""
    rows = []
    for v in values:
        send("pokef %x %r" % (addr, v), "recompute", wait=1.4)
        full = shot()
        img = small(full)
        o, sky, city, dl = compare(base_img, img)
        if o > 8 or abs(dl) > 1.0 or os.environ.get("SAVE") == "1":
            full.save(os.path.join(OUT_DIR, "%s_%g.png" % (tag.replace("+", "").replace(" ", "_"), v)))
        rows.append((tag, v, o, sky, city, dl))
        send("poke32 %x %08x" % (addr, original_bits), "recompute", wait=1.0)
    return rows


KNOWN_SKIP = [(0x2a8, 0x2c0), (0x300, 0x310), (0x598, 0x5b0)]   # sun block, ambient, fog densities: already identified


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "validate"
    # a clean starting state: no sky override, Day lighting
    send("skyreset", wait=1.0)
    send("sky 2", wait=6.0)   # Sunset then Day: reloads the atmosphere, so no value a stopped run left behind survives
    send("sky " + os.environ.get("MODE", "0"), wait=7.0)   # MODE: the lighting mode to probe in (0 Day, 5 Night, ...)
    s = active_slot()
    base = slot_address(s)
    if os.environ.get("BASE") == "world":   # probe the world singleton's lighting block instead of the atmosphere slot
        m = mark("w%d" % int(time.time() * 1000))
        send("snap probe_world", wait=1.5)
        r = re.search(r"snap: 'probe_world' taken \(world ([0-9a-f]+)", after(m))
        base = int(r.group(1), 16)
        print("world singleton at %x" % base)
    print("active slot", s, "at %x" % base)
    noise, base_img = noise_level()
    thr = max(noise * 3.0, 2.0)
    print("noise %.3f, threshold %.3f" % (noise, thr))

    if cmd == "validate":
        tests = [("sun rotation +0x2b8", 0x2b8, [280.0]), ("sun elevation +0x2bc", 0x2bc, [80.0]), ("fog density +0x598", 0x598, [0.0]),
                 ("ambient +0x300 (no effect expected)", 0x300, [0.0]), ("header c +0x2c8 (no effect expected)", 0x2c8, [10.0])]
        for name, off, vals in tests:
            orig = struct.unpack("<I", peek(base + off, 4))[0]
            for r in probe(base + off, orig, vals, base_img, noise, name):
                print("  %-40s -> %-8g overall %.2f  sky %.2f  city %.2f  brightness %+.2f  %s" % (r[0], r[1], r[2], r[3], r[4], r[5], verdict(r, thr) or "-"))
        return

    if cmd == "one":
        off = int(sys.argv[2], 16)
        vals = [float(x) for x in sys.argv[3:]]
        orig = struct.unpack("<I", peek(base + off, 4))[0]
        for r in probe(base + off, orig, vals, base_img, noise, "+0x%x" % off):
            print("  %s -> %g overall %.2f sky %.2f city %.2f brightness %+.2f %s" % (r[0], r[1], r[2], r[3], r[4], r[5], verdict(r, thr) or "-"))
        return

    if cmd == "world":
        lo = int(sys.argv[2], 16)
        hi = int(sys.argv[3], 16)
        blob = b""
        for o in range(lo, hi, 1024):
            blob += peek(base + o, min(1024, hi - o))
        outpath = os.path.join(ROOT, "recon", "fieldprobe_world.csv")
        with open(outpath, "a", newline="") as f:
            w = csv.writer(f)
            for i in range(0, len(blob) - 3, 4):
                off = lo + i
                bits = struct.unpack_from("<I", blob, i)[0]
                v = f32(bits)
                if not math.isfinite(v) or v == 0 or not (1e-3 < abs(v) < 1e5):
                    continue
                test = v * 8.0
                rows = probe(base + off, bits, [test], base_img, noise, "+0x%x" % off)
                for r in rows:
                    w.writerow(["0x%x" % off, "%g" % v, "%g" % r[1], "%.2f" % r[2], "%.2f" % r[3], "%.2f" % r[4], "%+.2f" % r[5], verdict(r, thr)])
                    f.flush()
                    print("+0x%x (%g) -> %g : %s o%.1f s%.1f c%.1f b%+.1f" % (off, v, r[1], verdict(r, thr) or "-", r[2], r[3], r[4], r[5]), flush=True)
                if (i // 4) % 25 == 24:
                    noise, base_img = noise_level()
                    thr = max(noise * 3.0, 2.0)
        print("world done")
        return

    if cmd == "list":
        offsets = [int(x, 16) for x in sys.argv[2:]]
        blobs = {off: peek(base + off, 4) for off in offsets}
        outpath = os.path.join(ROOT, "recon", "fieldprobe_scan.csv")
        new = not os.path.exists(outpath)
        with open(outpath, "a", newline="") as f:
            w = csv.writer(f)
            if new:
                w.writerow(["offset", "original", "test", "overall", "sky", "city", "brightness", "effect"])
            for n, off in enumerate(offsets):
                bits = struct.unpack("<I", blobs[off])[0]
                v = f32(bits)
                if not math.isfinite(v) or (v != 0 and not (1e-5 < abs(v) < 1e7)):
                    print("+0x%x skipped (not a float: %08x)" % (off, bits), flush=True)
                    continue
                tests = [v * 8.0, v * 0.05] if v != 0 else [4.0, -4.0]
                label = []
                for r in probe(base + off, bits, tests, base_img, noise, "+0x%x" % off):
                    w.writerow(["0x%x" % off, "%g" % v, "%g" % r[1], "%.2f" % r[2], "%.2f" % r[3], "%.2f" % r[4], "%+.2f" % r[5], verdict(r, thr)])
                    f.flush()
                    label.append("%g:%s o%.1f s%.1f c%.1f b%+.1f" % (r[1], verdict(r, thr) or "-", r[2], r[3], r[4], r[5]))
                print("+0x%x (%g)  %s" % (off, v, " | ".join(label)), flush=True)
                if n % 25 == 24:
                    noise, base_img = noise_level()
                    thr = max(noise * 3.0, 2.0)
        print("list done")
        return

    # scan
    lo = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x2a0
    hi = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x700
    blob = b""
    for o in range(lo, hi, 1024):
        blob += peek(base + o, min(1024, hi - o))
        outpath = os.path.join(ROOT, "recon", "fieldprobe_scan.csv")
    new = not os.path.exists(outpath)
    with open(outpath, "a", newline="") as f:
        w = csv.writer(f)
        if new:
            w.writerow(["offset", "original", "test", "overall", "sky", "city", "brightness", "effect"])
        for i in range(0, len(blob) - 3, 4):
            off = lo + i
            if any(a <= off < b for a, b in KNOWN_SKIP):
                continue
            bits = struct.unpack_from("<I", blob, i)[0]
            v = f32(bits)
            if not math.isfinite(v) or (v != 0 and not (1e-5 < abs(v) < 1e7)):
                continue   # a pointer, a flag, a count: not a float we should poke
            tests = [v * 8.0, v * 0.05] if v != 0 else [4.0, -4.0]
            if os.environ.get("QUICK") == "1":
                tests = tests[:1]   # one test value per field: find what does anything, characterize afterwards
            hit = False
            for r in probe(base + off, bits, tests, base_img, noise, "+0x%x" % off):
                effect = verdict(r, thr) == "EFFECT"
                hit |= effect
                w.writerow(["0x%x" % off, "%g" % v, "%g" % r[1], "%.2f" % r[2], "%.2f" % r[3], "%.2f" % r[4], "%+.2f" % r[5], verdict(r, thr)])
                f.flush()
                if effect:
                    print("+0x%x  %g -> %g : overall %.2f sky %.2f city %.2f brightness %+.2f" % (off, v, r[1], r[2], r[3], r[4], r[5]), flush=True)
            if not hit:
                print("+0x%x  %g  no effect" % (off, v), flush=True)
            # re-measure the noise now and then (the camera or the weather may have moved)
            if (i // 4) % 40 == 39:
                noise, base_img = noise_level()
                thr = max(noise * 3.0, 2.0)
    print("scan done")


if __name__ == "__main__":
    main()
