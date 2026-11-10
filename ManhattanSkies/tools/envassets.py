import sys, zipfile, os, re, struct
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ALERT"))
import dat1lib.crc64 as crc64
lib = sys.argv[1]
mods = [l.strip() for l in open(sys.argv[2], encoding="utf-8") if l.strip()]
TYPES = {0x39F27E27:"atmosphere",0x5C4580B9:"texture",0x1C04EF8C:"material",0x07DC03E3:"materialgraph",0x21A56F68:"config",0xF05EF819:"visualeffect",0x8A0B1487:"zone",0x98906B9F:"model",0x567CC2F0:"levellight"}
assets = {}   # (span, id) -> {"type", "mods":[]}
strings = set()
for m in mods:
    z = zipfile.ZipFile(os.path.join(lib, m))
    for n in z.namelist():
        mm = re.match(r"ModFiles/(\d+)_([0-9A-F]{16})$", n)
        if not mm: continue
        data = z.read(n)
        t = "?"
        i = data.find(b"1TAD")
        if i >= 0 and i + 8 <= len(data):
            unk = struct.unpack_from("<I", data, i + 4)[0]; t = TYPES.get(unk, "%08x" % unk)
        key = (int(mm.group(1)), mm.group(2))
        a = assets.setdefault(key, {"type": t, "mods": [], "size": set()})
        a["mods"].append(m.rsplit(".", 1)[0]); a["size"].add(len(data))
        for s in re.findall(rb"[a-z0-9_/\.\-]{6,200}\.(?:texture|atmosphere|material|materialgraph|config|visualeffect|zone|model)", data[:2000000], re.I):
            strings.add(s.decode().lower())
names = {}
for s in strings:
    names["%016X" % crc64.hash(s)] = s
for (span, aid), a in sorted(assets.items(), key=lambda kv: (kv[1]["type"], kv[0])):
    print("%-3d %s %-12s %-70s %d mods: %s" % (span, aid, a["type"], names.get(aid, "?"), len(a["mods"]), ", ".join(sorted(a["mods"]))[:300]))
print("\n# strings found:", len(strings))
for s in sorted(strings):
    if "atm" in s or "sky" in s or "atmos" in s: print("  ", s)
