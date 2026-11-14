# skipped.py: for every .smpcmod in the Atmospheres folder, lists the files the mod ships that atmpacks.cpp does NOT use: not one of the five base
# atmospheres and not a texture its atmosphere references. Those stay the game's own while the mod's atmosphere is on (they may matter: RECON Part 17f).
import collections
import glob
import os
import struct
import sys
import zipfile

BASE = {0x882245A8C2B5BD1A: "DAY", 0x8636707D50EACAB9: "EARLY MORNING", 0x98246BE74781784F: "SUNSET", 0x9F4F5899144AF4FA: "NIGHT", 0x8BEDB52FCD0301DF: "OVERCAST"}
folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.expanduser("~"), "Documents", "Marvel's Spider-Man Remastered", "ManhattanSkies", "Atmospheres")
seen = collections.defaultdict(list)
for path in sorted(glob.glob(os.path.join(folder, "*.smpcmod"))):
    z = zipfile.ZipFile(path)
    files = {}
    for i in z.infolist():
        n = i.filename
        if n.startswith("ModFiles/") and not n.endswith("/") and "_" in n:
            files[int(n.rsplit("_", 1)[1], 16)] = (n.split("/")[1].split("_")[0], i.file_size)
    used = set()
    for b in BASE:
        if b in files:
            data = z.read("ModFiles/%s_%016X" % (files[b][0], b))
            for off in range(0, len(data) - 7, 4):
                v = struct.unpack_from("<Q", data, off)[0]
                if v in files and v not in BASE:
                    used.add(v)
    skipped = [(h, files[h]) for h in files if h not in BASE and h not in used]
    name = os.path.basename(path)
    print("%-52s base %d, textures %d, skipped %d: %s" % (name, sum(1 for b in BASE if b in files), len(used), len(skipped), ", ".join("span %s %016X %d B" % (s[0], h, s[1]) for h, s in skipped)))
    for h, s in skipped:
        seen[(h, s[1])].append(name)
print()
for (h, size), names in sorted(seen.items(), key=lambda kv: -len(kv[1])):
    print("%016X %8d B in %d mod(s)" % (h, size, len(names)))
