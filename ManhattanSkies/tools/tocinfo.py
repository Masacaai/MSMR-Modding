# tocinfo.py <hex asset id> [...]: reads the game's CURRENT toc (the one Overstrike patched; it matches what the running game holds in memory) and prints
# the DAT1 section layout, the archive list, and for each asset id its index, archive, offset, size and the byte position of its entries inside each
# section (so the same entries can be located and patched in the running game's memory). Also prints a 4-id pattern (the id and its 3 successors in
# the sorted id table) to search memory for. Needs tools/ALERT.
import contextlib
import io
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ALERT"))
import dat1lib

A = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\asset_archive"
with contextlib.redirect_stdout(io.StringIO()):
    toc = dat1lib.read(open(os.path.join(A, "toc"), "rb"))
    toc.set_archives_dir(A)

d = toc.dat1
print("toc version:", toc.version if hasattr(toc, "version") else "?")
hdr = d.header if hasattr(d, "header") else None
if hdr is not None:
    print("DAT1 header size %d, sections:" % hdr.size)
    for s in hdr.sections:
        print("  tag %08x  offset 0x%x  size 0x%x" % (s.tag, s.offset, s.size))
ids = toc.get_assets_section().ids
print("assets:", len(ids))
arch = toc.get_archives_section()
names = [getattr(a, "filename", None) for a in getattr(arch, "entries", [])]
print("archives:", len(names))
for i, n in enumerate(names):
    if i < 3 or i >= len(names) - 6:
        print("  [%d] %r" % (i, n))
spans = toc.get_spans_section()
print("spans:", len(spans.entries) if hasattr(spans, "entries") else "?")
for a in sys.argv[1:]:
    aid = int(a, 16)
    es = toc.get_asset_entries_by_assetid(aid)
    for e in es:
        print("asset %016X: index %d archive %d offset 0x%x size %d" % (aid, e.index, e.archive, e.offset, e.size))
        pat = [ids[e.index + k] if e.index + k < len(ids) else 0 for k in range(4)]
        print("   id pattern: " + " ".join("%016X" % p for p in pat))
    if not es:
        print("asset %016X: not in the toc" % aid)
