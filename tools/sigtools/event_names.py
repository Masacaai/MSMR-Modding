# event_names.py <log> [exe]: names for the "events: new <hash>" lines an EndlessArena recording writes ([debug] events=1 in EndlessMode.ini).
# An event type is the Insomniac CRC-32 (ALERT's hash(name, False)) of its class name; the class names come from the exe's RTTI.
import re
import sys

DEFAULT_EXE = r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\Spider-Man.exe"
T = []
for i in range(256):
    c = i
    for _ in range(8):
        c = (c >> 1) ^ 0xEDB88320 if c & 1 else c >> 1
    T.append(c)


def h(name):
    v = 0xEDB88320
    for ch in name.encode():
        v = (v >> 8) ^ T[(v ^ ch) & 0xFF]
    return v


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    data = open(sys.argv[2] if len(sys.argv) > 2 else DEFAULT_EXE, "rb").read()
    names = {}
    for m in re.finditer(rb"\.\?AV([A-Za-z0-9_]+)@@", data):
        n = m.group(1).decode()
        names[h(n)] = n
    for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
        m = re.search(r"events: new ([0-9a-f]{8})", line)
        if m:
            print(line[1:13], m.group(1), names.get(int(m.group(1), 16), "?"))


if __name__ == "__main__":
    main()
