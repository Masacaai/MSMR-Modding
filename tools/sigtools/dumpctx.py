# dumpctx.py <crash.mdmp> [max frames]: the REAL faulting context of a game crash dump.
#
# dumpstack.py prints the context stored in the dump's thread list, which for this game is the crash handler's own state (the same
# ntdll frame every time). The faulting state is elsewhere, and this tool looks for it in three places:
#   1. the exception stream's own ThreadContext (the CONTEXT the dump writer was given),
#   2. every CONTEXT record lying in any thread's stack memory (the OS pushes one when it dispatches an exception; signature:
#      ContextFlags 0x0010xxxx at +0x30, SegCs 0x33 at +0x38, SegSs 0x2b at +0x42),
#   3. every EXCEPTION_RECORD in stack memory (code 0xCxxxxxxx/0x8xxxxxxx, address inside a module).
# For each context found it prints Rip, the registers and a stack walk from that context's Rsp (return-address candidates inside
# Spider-Man.exe or a mod DLL, as exe+RVA). Needs: pip install minidump.   See docs/REVERSE_ENGINEERING.md section 4.
import logging
import struct
import sys

from minidump.minidumpfile import MinidumpFile

REGS = [("Rax", 0x78), ("Rcx", 0x80), ("Rdx", 0x88), ("Rbx", 0x90), ("Rsp", 0x98), ("Rbp", 0xA0), ("Rsi", 0xA8), ("Rdi", 0xB0),
        ("R8", 0xB8), ("R9", 0xC0), ("R10", 0xC8), ("R11", 0xD0), ("R12", 0xD8), ("R13", 0xE0), ("R14", 0xE8), ("R15", 0xF0), ("Rip", 0xF8)]
CODES = {0xC0000005: "access violation", 0xC00000FD: "stack overflow", 0xC0000409: "fail fast / stack buffer overrun", 0x80000003: "breakpoint",
         0xC000001D: "illegal instruction", 0xC0000094: "integer divide by zero", 0xE06D7363: "C++ exception", 0xC0000374: "heap corruption"}


def main(path, max_frames=40):
    logging.disable(logging.CRITICAL)   # the library logs a harmless PEB parsing error for these dumps
    md = MinidumpFile.parse(path)
    raw = open(path, "rb").read()
    reader = md.get_reader()
    mods = sorted((m.baseaddress, m.baseaddress + m.size, m.name.split("\\")[-1]) for m in md.modules.modules)

    def where(addr):
        for lo, hi, name in mods:
            if lo <= addr < hi:
                return "%s+%x" % (name.replace("Spider-Man.exe", "exe"), addr - lo)
        return None

    def interesting(w):
        return w and not any(w.lower().startswith(p) for p in ("ntdll", "kernel", "ucrtbase", "vcruntime", "msvcp", "user32", "win32u", "combase", "rpcrt4"))

    def show_ctx(ctx, title):
        r = {n: struct.unpack_from("<Q", ctx, o)[0] for n, o in REGS}
        print("  %s: Rip=%#x (%s)  Rsp=%#x" % (title, r["Rip"], where(r["Rip"]) or "not in a module", r["Rsp"]))
        line = []
        for n, _ in REGS:
            if n in ("Rip", "Rsp"):
                continue
            w = where(r[n])
            line.append("%s=%#x%s" % (n, r[n], (" (" + w + ")") if w else ""))
        print("    " + "  ".join(line))
        # stack walk: return-address candidates
        n = 0
        for off in range(0, 0x3000, 8):
            try:
                v = int.from_bytes(reader.read(r["Rsp"] + off, 8), "little")
            except Exception:
                break
            w = where(v)
            if interesting(w):
                print("    [rsp+%#06x] %s" % (off, w))
                n += 1
                if n >= max_frames:
                    break
        return r

    exc = md.exception.exception_records[0]
    rec = exc.ExceptionRecord
    code = rec.ExceptionCode if isinstance(rec.ExceptionCode, int) else getattr(rec.ExceptionCode, "value", 0)
    print("exception stream: code=%s addr=%#x (%s) thread=%d" % (CODES.get(code, hex(code) if code else str(rec.ExceptionCode)), rec.ExceptionAddress,
                                                                 where(rec.ExceptionAddress), exc.ThreadId))
    if rec.NumberParameters >= 2:
        kind = {0: "read", 1: "write", 8: "execute"}.get(rec.ExceptionInformation[0], str(rec.ExceptionInformation[0]))
        print("  access: %s of %#x" % (kind, rec.ExceptionInformation[1]))
    seen = set()
    # 1. the exception stream's ThreadContext
    loc = getattr(exc, "ThreadContext", None)
    if loc is not None and getattr(loc, "DataSize", 0) >= 0x100:
        ctx = raw[loc.Rva:loc.Rva + loc.DataSize]
        print("\n[1] context stored with the exception record:")
        r = show_ctx(ctx, "exception context")
        seen.add((r["Rip"], r["Rsp"]))
    # 2 and 3: scan every thread's stack
    print("\n[2] CONTEXT records found in thread stacks (the faulting state the OS saved):")
    found = 0
    recs = []
    for t in md.threads.threads:
        st = t.Stack
        base, size = st.StartOfMemoryRange, st.MemoryLocation.DataSize
        data = raw[st.MemoryLocation.Rva:st.MemoryLocation.Rva + size]
        for o in range(0, size - 0x4D0, 8):
            flags = struct.unpack_from("<I", data, o + 0x30)[0]
            if (flags & 0xFFFF0000) != 0x00100000 or not (flags & 0x3):
                continue
            if struct.unpack_from("<H", data, o + 0x38)[0] != 0x33 or struct.unpack_from("<H", data, o + 0x42)[0] != 0x2B:
                continue
            rip, rsp = struct.unpack_from("<Q", data, o + 0xF8)[0], struct.unpack_from("<Q", data, o + 0x98)[0]
            if (rip, rsp) in seen:
                continue
            seen.add((rip, rsp))
            found += 1
            mark = "  <-- matches the exception address" if rip == rec.ExceptionAddress else ""
            print(" thread %d, stack+%#x (ContextFlags %#x)%s" % (t.ThreadId, o, flags, mark))
            show_ctx(data[o:o + 0x4D0], "context")
        for o in range(0, size - 0x98, 8):
            c, fl, nxt, addr, npar = struct.unpack_from("<IIQQI", data, o)
            if (c >> 28) in (0xC, 0x8, 0xE) and fl in (0, 1) and npar <= 15 and where(addr) and (c in CODES or (c & 0x0FFF0000) == 0):
                recs.append((t.ThreadId, base + o, c, addr, struct.unpack_from("<QQ", data, o + 0x20) if npar >= 2 else None))
    if not found:
        print("  none")
    print("\n[3] EXCEPTION_RECORDs found in thread stacks:")
    shown = set()
    for tid, at, c, addr, info in recs:
        if (c, addr) in shown:
            continue
        shown.add((c, addr))
        extra = ""
        if info and c == 0xC0000005:
            extra = "  %s of %#x" % ({0: "read", 1: "write", 8: "execute"}.get(info[0], str(info[0])), info[1])
        print("  thread %d at %#x: %s at %s%s" % (tid, at, CODES.get(c, hex(c)), where(addr), extra))
    if not recs:
        print("  none")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__ or "usage: dumpctx.py <dump>")
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 40)
