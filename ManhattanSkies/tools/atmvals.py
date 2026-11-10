# atmvals.py <file suffix>...: the look values we control, read straight from each atmosphere file (pristine toc.BAK): file offset = slot offset - 0x1b4.
import contextlib, io, os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ALERT"))
import dat1lib, dat1lib.crc64 as crc64
A = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\asset_archive"
with contextlib.redirect_stdout(io.StringIO()):
    toc = dat1lib.read(open(os.path.join(A, "toc.BAK"), "rb")); toc.set_archives_dir(A)
f = lambda d, o, n=1: struct.unpack_from("<%df" % n, d, o)
print("%-38s %-22s %6s %7s %5s %-27s %-5s %-5s %-15s %-15s" % ("file", "sun rgb x int", "elev", "expo", "haze", "fog dens (6)", "clds", "dist0", "filter", "fogcolor"))
for s in sys.argv[1:]:
    e = toc.get_asset_entries_by_assetid(crc64.hash("atmosphere/atm_ibl_lighting/" + s))
    if not e: print(s, "missing"); continue
    d = toc.extract_asset(e[0])
    sun = f(d, 0xf4, 4); el = f(d, 0x108)[0]; ex = f(d, 0x2d0)[0]; hz = f(d, 0x40c)[0]; fd = f(d, 0x3e4, 6); cl = f(d, 0x668)[0]; d0 = f(d, 0x3d4)[0]
    fl = f(d, 0x4b4, 3); fc = f(d, 0x400, 3)
    print("%-38s %-22s %6.1f %7.2f %5.2f %-27s %-5.1f %-5.0f %-15s %-15s" % (s[:38], "%.2f,%.2f,%.2f x%.1f" % sun, el, ex, hz, ",".join("%.3g" % x for x in fd), cl, d0, ",".join("%.2f" % x for x in fl), ",".join("%.2f" % x for x in fc)))
