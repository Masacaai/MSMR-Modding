import io, contextlib, os, sys, re, collections
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ALERT"))
import dat1lib, dat1lib.crc64 as crc64
A = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\asset_archive"
with contextlib.redirect_stdout(io.StringIO()):
    toc = dat1lib.read(open(os.path.join(A, "toc"), "rb")); toc.set_archives_dir(A)
ids = sorted({l.split()[1] for l in open(sys.argv[1]) if l.startswith("HIT")})
refs = collections.defaultdict(set)   # atmosphere path -> referencing asset ids
owner_strings = {}
for h in ids:
    e = toc.get_asset_entries_by_assetid(int(h, 16))
    if not e: continue
    d = toc.extract_asset(e[0])
    strs = [x.decode() for x in re.findall(rb'[\x20-\x7e]{5,}', d)]
    zone = next((s for s in strs if s.endswith((".zone", ".level", ".config", ".actor", ".conduit"))), "")
    for s in strs:
        for m in re.findall(r"[A-Za-z0-9_/\.\-]*atmosphere/[A-Za-z0-9_/\.\-]+\.atmosphere", s):
            refs[m.lower()].add(h)
for p in sorted(refs):
    present = bool(toc.get_asset_entries_by_assetid(crc64.hash(p)))
    print("%s %3d refs %s" % ("IN " if present else "-- ", len(refs[p]), p))
