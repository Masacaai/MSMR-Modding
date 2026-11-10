# atmpair.py <file suffix A> <file suffix B> ...: how different are these atmospheres? Extracts each from the pristine toc.BAK and prints, against the first,
# the number of differing 4-byte words, the differing words inside the first 0x8c0 bytes (the values we edit: sun, fog, exposure, filter, haze...) and
# whether the baked lighting (everything after the header) is identical. Needs tools/ALERT.
import contextlib
import io
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ALERT"))
import dat1lib
import dat1lib.crc64 as crc64

A = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\asset_archive"
with contextlib.redirect_stdout(io.StringIO()):
    toc = dat1lib.read(open(os.path.join(A, "toc.BAK"), "rb"))
    toc.set_archives_dir(A)
data = []
for suffix in sys.argv[1:]:
    path = "atmosphere/atm_ibl_lighting/" + suffix
    e = toc.get_asset_entries_by_assetid(crc64.hash(path))
    if not e:
        print("missing", suffix)
        data.append(None)
        continue
    data.append(toc.extract_asset(e[0]))
base = data[0]
for suffix, d in zip(sys.argv[1:], data):
    if d is None or d is base:
        continue
    n = min(len(base), len(d)) // 4
    diff = [i for i in range(n) if base[i * 4:i * 4 + 4] != d[i * 4:i * 4 + 4]]
    head = [i for i in diff if i * 4 < 0x8c0]
    print("%s vs %s: sizes %d/%d, %d differing words, %d in the first 0x8c0 bytes, baked part (after 0x8c0) %s" % (
        sys.argv[1], suffix, len(base), len(d), len(diff), len(head), "identical" if all(i * 4 < 0x8c0 for i in diff) and len(base) == len(d) else "differs"))
    for i in head[:12]:
        a = struct.unpack_from("<f", base, i * 4)[0]
        b = struct.unpack_from("<f", d, i * 4)[0]
        print("    +0x%04x  %.5g -> %.5g" % (i * 4, a, b))
