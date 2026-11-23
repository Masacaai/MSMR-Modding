# buildslot.py <mod.smpcmod> <mod atmosphere id hex> <carrier atmosphere file suffix> <out dir>
# Builds the loose files for one "slot": a mod's atmosphere plus the textures it ships, mapped onto an unused game atmosphere (the carrier) and onto
# texture assets that ONLY that carrier atmosphere references (docs/internals/atmosphere-and-menu-recon-log.md Part 17). In the mod's atmosphere bytes each texture id the mod ships is
# replaced by one of those carrier texture ids, so two mods never fight over the same texture id.
# Output: <out>/atmosphere.bin (the carrier atmosphere id gets these bytes), <out>/tex_<carrier texture id>.bin, <out>/commands.txt (the `carrier` lines
# to send, with ARCH to replace by an open raw mod archive index) and a report. Needs tools/ALERT and the game's current toc.
import contextlib
import io
import os
import re
import struct
import sys
import zipfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ALERT"))
import dat1lib
import dat1lib.crc64 as crc64

HERE = os.path.dirname(os.path.abspath(__file__))
A = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\asset_archive"
LIB = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\Mods Library"
mod_name, mod_atm_hex, carrier_suffix, out = sys.argv[1:5]
mod_atm = int(mod_atm_hex, 16)
os.makedirs(out, exist_ok=True)

with contextlib.redirect_stdout(io.StringIO()):
    toc = dat1lib.read(open(os.path.join(A, "toc"), "rb"))
    toc.set_archives_dir(A)
toc_ids = set(toc.get_assets_section().ids)


def refs(data, own):
    """(offset, id) of every 8-byte value in the data that is an asset id in the toc (a texture, material, ...)."""
    found = []
    for off in range(0, len(data) - 7, 4):
        v = struct.unpack_from("<Q", data, off)[0]
        if v != own and v > 0xFFFFFFFF and v in toc_ids:
            found.append((off, v))
    return found


def extract(aid):
    e = toc.get_asset_entries_by_assetid(aid)
    return toc.extract_asset(e[0]) if e else None


# every atmosphere the game has, and which textures each uses
src = open(os.path.join(HERE, "..", "mod", "src", "atm_ids.inc"), encoding="utf-8").read()
all_atm = [int(m, 16) for m in re.findall(r"\{ 0x([0-9A-F]+)ULL", src)]
usage = {}
atm_refs = {}
for aid in all_atm:
    data = extract(aid)
    if not data:
        continue
    r = refs(data, aid)
    atm_refs[aid] = r
    for _, t in r:
        usage.setdefault(t, set()).add(aid)
print("read %d game atmospheres, %d distinct referenced assets" % (len(atm_refs), len(usage)))

carrier_id = crc64.hash("atmosphere/atm_ibl_lighting/" + carrier_suffix)
if carrier_id not in atm_refs:
    sys.exit("carrier %s is not a known game atmosphere" % carrier_suffix)
# textures only this carrier uses (and that are textures: DAT1 type 5c4580b9)
exclusive = []
for off, t in atm_refs[carrier_id]:
    if usage[t] == {carrier_id} and t not in [x for x, _ in exclusive]:
        d = extract(t)
        if d and d[d.find(b"1TAD") + 4:][:4] == struct.pack("<I", 0x5C4580B9):
            exclusive.append((t, len(d)))
print("carrier %s (%016X): textures only it uses: %s" % (carrier_suffix, carrier_id, ["%016X (%d bytes)" % x for x in exclusive]))

z = zipfile.ZipFile(mod_name if os.path.isabs(mod_name) else os.path.join(LIB, mod_name))
files = {}
for n in z.namelist():
    if n.startswith("ModFiles/") and not n.endswith("/"):
        span, aid = n.split("/")[-1].split("_")
        files[int(aid, 16)] = z.read(n)
mod_bytes = files[mod_atm]
shipped = []
seen = set()
for off, mid in refs(mod_bytes, mod_atm):
    if mid in files and mid not in seen:
        shipped.append(mid)
        seen.add(mid)
shipped.sort(key=lambda m: -len(files[m]))
print("mod atmosphere %016X (%d bytes) references textures the mod ships: %s" % (mod_atm, len(mod_bytes), ["%016X (%d bytes)" % (m, len(files[m])) for m in shipped]))
if len(shipped) > len(exclusive):
    sys.exit("the carrier has only %d exclusive textures but the mod ships %d" % (len(exclusive), len(shipped)))

mapping = {}
carriers = [t for t, _ in sorted(exclusive, key=lambda x: -x[1])]
for mid, cid in zip(shipped, carriers):
    mapping[mid] = cid
new = bytearray(mod_bytes)
n_rewritten = 0
for off, mid in refs(mod_bytes, mod_atm):
    if mid in mapping:
        struct.pack_into("<Q", new, off, mapping[mid])
        n_rewritten += 1
open(os.path.join(out, "atmosphere.bin"), "wb").write(bytes(new))
with open(os.path.join(out, "commands.txt"), "w") as f:
    f.write("carrier %016X ARCH %s\n" % (carrier_id, os.path.join(out, "atmosphere.bin")))
    for mid, cid in mapping.items():
        fn = "tex_%016X.bin" % cid
        open(os.path.join(out, fn), "wb").write(files[mid])
        f.write("carrier %016X ARCH %s\n" % (cid, os.path.join(out, fn)))
        print("  texture %016X (%d bytes) -> carrier texture %016X" % (mid, len(files[mid]), cid))
print("rewrote %d reference(s) in the atmosphere; slot built in %s" % (n_rewritten, out))
