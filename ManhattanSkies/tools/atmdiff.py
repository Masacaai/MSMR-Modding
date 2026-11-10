# atmdiff.py <mod.smpcmod> <hex asset id> [more ids]: diffs an atmosphere asset of a mod against the vanilla one (extracted from toc.BAK, the
# pristine table of contents Overstrike keeps) and prints every changed 4-byte word as hex and float, with its file offset.
# Needs tools/ALERT. Used to find which atmosphere values make rain: the rain mods change only atmospheres and textures.
import contextlib
import io
import os
import re
import struct
import sys
import zipfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ALERT"))
import dat1lib

A = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\asset_archive"
LIB = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\Mods Library"
with contextlib.redirect_stdout(io.StringIO()):
    toc = dat1lib.read(open(os.path.join(A, "toc.BAK"), "rb"))
    toc.set_archives_dir(A)
z = zipfile.ZipFile(os.path.join(LIB, sys.argv[1]))
for aid in sys.argv[2:]:
    name = [n for n in z.namelist() if n.endswith("_" + aid.upper())]
    if not name:
        print(aid, "not in the mod")
        continue
    mod = z.read(name[0])
    e = toc.get_asset_entries_by_assetid(int(aid, 16))
    if not e:
        print(aid, "not in toc.BAK")
        continue
    van = toc.extract_asset(e[0])
    print("== %s: vanilla %d bytes, mod %d bytes" % (aid, len(van), len(mod)))
    if len(van) != len(mod):
        print("   sizes differ; comparing the common prefix only")
    n = min(len(van), len(mod)) // 4
    changed = [i for i in range(n) if van[i * 4:i * 4 + 4] != mod[i * 4:i * 4 + 4]]
    print("   %d changed words" % len(changed))
    for i in changed[:150]:
        a = struct.unpack_from("<I", van, i * 4)[0]
        b = struct.unpack_from("<I", mod, i * 4)[0]
        fa = struct.unpack("<f", struct.pack("<I", a))[0]
        fb = struct.unpack("<f", struct.pack("<I", b))[0]
        print("   +0x%04x  %08x -> %08x   %.5g -> %.5g" % (i * 4, a, b, fa, fb))
