# atmpaths_all.py <grepall output> <out.txt>: every "<something>.atmosphere" path mentioned by the assets grepall flagged, with how many assets mention it
# and whether the toc has it (CRC64 of the path as written, lowercased, forward slashes).
import io, contextlib, os, re, sys, collections
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ALERT"))
import dat1lib, dat1lib.crc64 as crc64
A = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\asset_archive"
with contextlib.redirect_stdout(io.StringIO()):
    toc = dat1lib.read(open(os.path.join(A, "toc"), "rb")); toc.set_archives_dir(A)
ids = sorted({l.split()[1] for l in open(sys.argv[1]) if l.startswith("HIT")})
refs = collections.defaultdict(set)
for n, h in enumerate(ids):
    e = toc.get_asset_entries_by_assetid(int(h, 16))
    if not e: continue
    try: d = toc.extract_asset(e[0])
    except Exception: continue
    for m in re.findall(rb"[A-Za-z0-9_/\.\-]{4,200}\.atmosphere", d):
        p = m.decode().replace("\\", "/").lower()
        refs[p].add(h)
with open(sys.argv[2], "w", encoding="utf-8") as out:
    for p in sorted(refs):
        present = bool(toc.get_asset_entries_by_assetid(crc64.hash(p)))
        out.write("%s %3d refs %s\n" % ("IN " if present else "-- ", len(refs[p]), p))
print("paths", len(refs))
