# buildslot2.py <mod.smpcmod> <mod atmosphere id hex> <first new id hex> <out dir>
# Splits one atmosphere .smpcmod into loose files for the "new assets" method (docs/RECON.md Part 17): the mod's atmosphere and every texture it ships and
# references get brand-new asset ids (ascending from <first new id>), and the references inside the atmosphere bytes are rewritten to the new texture
# ids. Nothing of the game is replaced. Files the atmosphere does not reference (shared helper assets other mods also ship) are ignored.
# Output: <out>/<id>.bin per asset and <out>/commands.txt (`addasset` lines in ascending id order; ARCH = an open raw mod archive index).
import os
import struct
import sys
import zipfile

LIB = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\Mods Library"
mod_name, mod_atm_hex, first_hex, out = sys.argv[1:5]
mod_atm = int(mod_atm_hex, 16)
first = int(first_hex, 16)
os.makedirs(out, exist_ok=True)

z = zipfile.ZipFile(mod_name if os.path.isabs(mod_name) else os.path.join(LIB, mod_name))
files = {}
for n in z.namelist():
    if n.startswith("ModFiles/") and not n.endswith("/"):
        _, aid = n.split("/")[-1].split("_")
        files[int(aid, 16)] = z.read(n)
atm = bytearray(files[mod_atm])

# which shipped assets does the atmosphere reference (8-byte ids at 4-byte aligned offsets)?
refs = {}
for off in range(0, len(atm) - 7, 4):
    v = struct.unpack_from("<Q", atm, off)[0]
    if v != mod_atm and v in files:
        refs.setdefault(v, []).append(off)
ignored = [m for m in files if m != mod_atm and m not in refs]
print("mod atmosphere %016X: %d bytes; it references %d shipped asset(s): %s" % (mod_atm, len(atm), len(refs), ["%016X x%d (%d bytes)" % (m, len(o), len(files[m])) for m, o in refs.items()]))
print("ignored (shipped but not referenced by the atmosphere): %s" % ["%016X" % m for m in ignored])

new_id = {}
nxt = first
for m in sorted(refs, key=lambda m: -len(files[m])):
    new_id[m] = nxt
    nxt += 1
atm_new = nxt
for m, offs in refs.items():
    for off in offs:
        struct.pack_into("<Q", atm, off, new_id[m])
lines = []
for m, nid in sorted(new_id.items(), key=lambda kv: kv[1]):
    fn = os.path.join(out, "%016X.bin" % nid)
    open(fn, "wb").write(files[m])
    lines.append((nid, fn))
    print("  texture %016X (%d bytes) -> new id %016X" % (m, len(files[m]), nid))
fn = os.path.join(out, "%016X.bin" % atm_new)
open(fn, "wb").write(bytes(atm))
lines.append((atm_new, fn))
print("  atmosphere %016X -> new id %016X (%d bytes, %d reference(s) rewritten)" % (mod_atm, atm_new, len(atm), sum(len(o) for o in refs.values())))
with open(os.path.join(out, "commands.txt"), "w") as f:
    for nid, fn in lines:
        f.write("addasset %016X ARCH %s\n" % (nid, fn))
print("atmosphere to load: %016X" % atm_new)
