# dumpindex.py [folder]: one line per crash dump in the game's dump folder: date, faulting address (exe+RVA), access kind and
# the first return addresses on the faulting stack, read from the exception stream's own context (see dumpctx.py).
import glob
import logging
import os
import re
import struct
import sys

from minidump.minidumpfile import MinidumpFile

logging.disable(logging.CRITICAL)
folder = sys.argv[1] if len(sys.argv) > 1 else os.path.expandvars(r"%USERPROFILE%\Documents\Marvel's Spider-Man Remastered")
for path in sorted(glob.glob(os.path.join(folder, "*.mdmp"))):
    try:
        md = MinidumpFile.parse(path)
        raw = open(path, "rb").read()
        reader = md.get_reader()
        mods = [(m.baseaddress, m.baseaddress + m.size, m.name.split("\\")[-1]) for m in md.modules.modules]

        def where(a):
            for lo, hi, n in mods:
                if lo <= a < hi:
                    return "%s+%x" % (n.replace("Spider-Man.exe", "exe"), a - lo)
            return None
        exc = md.exception.exception_records[0]
        rec = exc.ExceptionRecord
        acc = ""
        if rec.NumberParameters >= 2:
            acc = "%s %#x" % ({0: "read", 1: "write", 8: "exec"}.get(rec.ExceptionInformation[0], "?"), rec.ExceptionInformation[1])
        loc = exc.ThreadContext
        ctx = raw[loc.Rva:loc.Rva + loc.DataSize]
        rsp = struct.unpack_from("<Q", ctx, 0x98)[0]
        frames = []
        for off in range(0, 0x1000, 8):
            try:
                v = int.from_bytes(reader.read(rsp + off, 8), "little")
            except Exception:
                break
            w = where(v)
            if w and (w.startswith("exe+") and int(w[4:], 16) < 0x37ed000 or (".dll" in w.lower() and not re.match(r"(ntdll|kernel|ucrt|vcrun|msvcp|user32|win32u)", w.lower()))):
                frames.append(w)
                if len(frames) >= 6:
                    break
        date = re.search(r"(\d{4}-\d\d-\d\d-\d\d-\d\d-\d\d)", path).group(1)
        print("%s  %-18s %-22s %s" % (date, where(rec.ExceptionAddress) or hex(rec.ExceptionAddress), acc, " ".join(frames)))
    except Exception as e:
        print(os.path.basename(path), "unreadable:", e)
