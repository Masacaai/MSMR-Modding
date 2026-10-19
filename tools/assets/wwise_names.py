# wwise_names.py <audio log> [Events.wwiselookup]: turns a recording made with the `audio on` command (EndlessMode_audio.txt) into names.
# Wwise ids are the FNV-1 (32 bit) hash of the lower-cased name; the names come from the printable strings of the game's own lookup asset,
# sound/banks/Events.wwiselookup, extracted with tools/assets/extract.py (nothing from the game is stored in the repository).
# Usage: python tools/assets/extract.py out "sound/banks/Events.wwiselookup"   (then pass the extracted file as the second argument)
import re
import sys


def fnv(name):
    h = 2166136261
    for c in name.lower().encode("latin-1", "replace"):
        h = ((h * 16777619) & 0xFFFFFFFF) ^ c
    return h


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    log = open(sys.argv[1], encoding="utf-8", errors="replace").read().splitlines()
    names = {}
    if len(sys.argv) > 2:
        data = open(sys.argv[2], "rb").read()
        for m in re.finditer(rb"[ -~]{4,100}", data):
            t = m.group(0).decode()
            names.setdefault(fnv(t), t)
    out = []
    for line in log:
        def sub(m):
            v = int(m.group(2), 16)
            return "%s=%s(%s)" % (m.group(1), m.group(2), names.get(v, "?"))
        out.append(re.sub(r"\b(id|group|state|switch|event)=([0-9a-f]{8})\b", sub, line))
    print("\n".join(out))


if __name__ == "__main__":
    main()
