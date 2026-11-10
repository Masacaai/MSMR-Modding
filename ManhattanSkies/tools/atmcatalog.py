# atmcatalog.py <paths.txt> <out.txt>: for every atmosphere path (one per line, last whitespace-separated token), extracts it from the game's
# toc and prints its header values (ALERT's atmosphere Header section) and the textures it names. Needs tools/ALERT (clone of Tkachov/ALERT).
import io, contextlib, os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ALERT"))
import dat1lib, dat1lib.crc64 as crc64
A = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\asset_archive"
with contextlib.redirect_stdout(io.StringIO()):
    toc = dat1lib.read(open(os.path.join(A, "toc"), "rb")); toc.set_archives_dir(A)
paths = []
for l in open(sys.argv[1], encoding="utf-8"):
    t = l.split()
    if t and t[-1].endswith(".atmosphere") and t[-1] not in paths: paths.append(t[-1])
out = open(sys.argv[2], "w", encoding="utf-8")
out.write("%-62s %7s %6s %6s  %-26s %-8s %-22s %s\n" % ("path", "size", "sunRot", "sunEl", "sunRGBA", "radius", "ambientRGB", "textures"))
for p in sorted(paths):
    e = toc.get_asset_entries_by_assetid(crc64.hash(p))
    if not e: out.write("%-62s MISSING\n" % p); continue
    try:
        d = toc.extract_asset(e[0])
        with contextlib.redirect_stdout(io.StringIO()):
            obj = dat1lib.read(io.BytesIO(d))
        h = obj.dat1.sections[0]
        tex = sorted(set(x.decode().split("/")[-1] for x in re.findall(rb"[A-Za-z0-9_/\.\-]{8,}\.texture", d)))
        out.write("%-80s %7d %6.1f %6.1f  %-26s %-8s %-22s %s\n" % (
            p.replace("atmosphere/", "", 1), len(d), h.sun_rot, h.sun_elev, ",".join("%.2f" % x for x in h.sun_rgba), h.sun_radius,
            ",".join("%.4f" % x for x in h.ambience_rgba[:3]), " ".join(tex)[:90]))
    except Exception as ex:
        out.write("%-62s ERROR %s\n" % (p, ex))
out.close()
print("done", len(paths))
