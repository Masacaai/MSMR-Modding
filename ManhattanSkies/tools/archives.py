# archives.py [toc file name]: lists the game's asset archives (default: the toc Overstrike patched; pass toc.BAK for the original): the archive entry's
# install bucket and chunk map, the file's size on disk and first four bytes ("DSAR" = a compressed archive, anything else raw), how many assets point
# into it and the highest offset used. Used to work out which archive a runtime-added asset may borrow (RECON Part 17e). Needs tools/ALERT.
import contextlib
import io
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ALERT"))
import dat1lib

A = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\asset_archive"
name = sys.argv[1] if len(sys.argv) > 1 else "toc"
with contextlib.redirect_stdout(io.StringIO()):
    toc = dat1lib.read(open(os.path.join(A, name), "rb"))
arch = toc.get_archives_section().archives
count = [0] * len(arch)
top = [0] * len(arch)
for e in toc.get_offsets_section().entries:
    if e.archive_index < len(arch):
        count[e.archive_index] += 1
        top[e.archive_index] = max(top[e.archive_index], e.offset)
for i, a in enumerate(arch):
    fn = a.filename.split(b"\0")[0].decode("ascii")
    p = os.path.join(A, fn.replace("\\", "/"))
    disk, magic = -1, "-"
    if os.path.exists(p):
        disk = os.path.getsize(p)
        with open(p, "rb") as f:
            magic = f.read(4).hex()
    print("%4d %-22s bucket %08x chunkmap %08x  disk %12d magic %s  assets %7d  max-offset %10d" % (i, fn, a.install_bucket, a.chunkmap, disk, magic, count[i], top[i]))
