# events.py [out.txt]: the event catalogue of Spider-Man.exe 4.0630.0.0, built offline.
# An event type id is the Insomniac CRC-32 of the event class name (fx.crc). This lists, per event: its id, the code that sends it
# (calls of SendEvent exe+16d5e80 with `mov edx, <id>`), the payload size when the call site shows it, and the handlers registered for it
# (calls of AddEventHandler exe+198f160(listener, id, handler)). Needs fx.py next to it.
import re
import sys

import fx

SEND, ADD = 0x16d5e80, 0x198f160


def main():
    img = fx.Img()
    hd = fx.hashdict(img)
    ev = {}

    def rec(h):
        return ev.setdefault(h, {"send": [], "listen": []})

    for target, key in ((SEND, "send"), (ADD, "listen")):
        for at, kind, _ in img.xrefs(target, slack=(0,)):
            if kind not in ("call", "jmp"):
                continue
            call = at - 1
            f = img.func(call)
            if not f:
                continue
            start = max(f[0], call - 0x90)
            # decode a window ending at the call; resync by trying a few start offsets
            best = None
            for s in range(start, start + 16):
                ins = fx.disasm(img, s, call + 5)
                if ins and ins[-1][0] == call:
                    best = ins
                    break
            if not best:
                continue
            h = handler = size = None
            for a, m, o, note, i in best:
                if m == "mov" and o.startswith("edx, 0x"):
                    h = int(o.split(", ")[1], 16)
                elif m == "lea" and o.startswith("r8, ") and note.startswith("[exe+"):
                    handler = int(note[5:].split("]")[0], 16)
                elif m == "mov" and re.match(r"dword ptr \[rsp \+ 0x60\], ", o):
                    try:
                        size = int(o.split(", ")[1], 0)
                    except ValueError:
                        size = None
            if h is None or h < 0x10000:
                continue
            name = img.name(f[0]) or ""
            if key == "send":
                rec(h)["send"].append((f[0], name, size))
            else:
                rec(h)["listen"].append((handler, f[0], name))
    # every RTTI class that looks like an event, even if no static sender/listener was found
    for cname, _, _ in img.rt["vts"].values():
        base = cname.split("::")[-1]
        if base.endswith("Event") or base.endswith("Msg") or base.endswith("Message"):
            rec(fx.crc(base))
            hd.setdefault(fx.crc(base), base)
    ddl = fx.ddl_types(img)
    for tname, t in ddl.items():
        if hd.get(t[2]) == "EventBase" or tname == "EventBase":
            rec(t[1])
            hd[t[1]] = tname
    out = open(sys.argv[1], "w", encoding="utf-8") if len(sys.argv) > 1 else sys.stdout
    out.write("# event id (CRC-32 of the class name) | name | size and payload fields beyond EventBase (+offset name:type) | senders: fn | handlers: handler<-registering fn\n"
              "# EventBase: +0x28 Frame u32, +0x2c SenderHandle u32, +0x30 TrackedNameHash, +0x34 EventLocatorHash, +0x38 SourceAnimDriverNameHash,\n"
              "#            +0x3c SourceAnimDriverHandle, +0x40 Orientation (DDLTransform), +0x120 RequireActivated u8; own fields start at +0x138.\n")
    for h in sorted(ev, key=lambda x: hd.get(x, "~%08x" % x)):
        e = ev[h]
        snd = " ".join(sorted({"%x%s%s" % (fs, ("(" + n + ")") if n else "", ("[%d]" % sz) if sz else "") for fs, n, sz in e["send"]}))
        lst = " ".join(sorted({"%s<-%x%s" % (("%x" % hh) if hh else "?", fs, ("(" + n + ")") if n else "") for hh, fs, n in e["listen"]}))
        t = ddl.get(hd.get(h, ""))
        flds = ""
        if t:
            flds = " ".join("+%x %s:%s" % (ofs, fn, fx.KIND.get(kind, "?") if kind not in (0xb, 0xc, 0xd) else hd.get(th, "%08x" % th))
                            for fn, kind, ofs, th, cnt in t[4] if ofs >= 0x128)
            flds = "size 0x%x %s" % (t[3], flds)
        out.write("%08x %s | %s | send: %s | handlers: %s\n" % (h, hd.get(h, "?"), flds or "-", snd or "-", lst or "-"))
    sys.stderr.write("%d events (%d with a static sender, %d with a handler)\n" % (len(ev), sum(1 for e in ev.values() if e["send"]), sum(1 for e in ev.values() if e["listen"])))


if __name__ == "__main__":
    main()
