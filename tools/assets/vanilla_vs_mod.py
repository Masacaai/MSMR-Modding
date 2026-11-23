# vanilla_vs_mod.py <mod.smpcmod> <asset id hex> [...]: compares the file a mod ships for an asset id with the game's own copy (read through the ORIGINAL toc, toc.BAK,
# so other mods' replacements do not show). Prints sizes and whether the bytes are identical, and where they first differ. Needs tools/ALERT. Nothing is written.
import contextlib
import hashlib
import io
import os
import sys
import zipfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ALERT"))
import dat1lib

A = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\asset_archive"
mod = zipfile.ZipFile(sys.argv[1])
with contextlib.redirect_stdout(io.StringIO()):
    toc = dat1lib.read(open(os.path.join(A, "toc.BAK"), "rb"))
    toc.set_archives_dir(A)
for h in sys.argv[2:]:
    aid = int(h, 16)
    name = next((n for n in mod.namelist() if n.upper().endswith("_%016X" % aid)), None)
    if not name:
        print("%016X: not in the mod" % aid)
        continue
    ours = mod.read(name)
    entries = toc.get_asset_entries_by_assetid(aid)
    if not entries:
        print("%016X: mod %d bytes; not in the original toc" % (aid, len(ours)))
        continue
    with contextlib.redirect_stdout(io.StringIO()):
        theirs = toc.extract_asset(entries[0])
    same = ours == theirs
    first = next((i for i in range(min(len(ours), len(theirs))) if ours[i] != theirs[i]), None)
    print("%016X: mod %d bytes (%s), game %d bytes (%s): %s%s" % (aid, len(ours), hashlib.sha1(ours).hexdigest()[:10], len(theirs), hashlib.sha1(theirs).hexdigest()[:10],
          "IDENTICAL" if same else "DIFFERENT", "" if same or first is None else ", first difference at byte %d" % first))
