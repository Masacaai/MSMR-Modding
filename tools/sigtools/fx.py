# fx.py: one fast toolbox for reading Spider-Man.exe (4.0630.0.0) offline. Needs: pip install capstone numpy.
#
#   fx.py d <rva> [len|f]      disassemble (default: the whole function per .pdata), annotated: call targets by name,
#                              rip-relative operands resolved to strings, vtables (class names) and symbols
#   fx.py sk <rva>...          "skeleton" of a function: size, calls (in order, deduplicated), strings, vtables, globals
#   fx.py f <rva>...           function bounds (.pdata), vtable slots that point at it
#   fx.py x <rva>...           cross references: rel32 code refs (call/jmp/lea/mov) with the containing function, and
#                              8-byte absolute pointers in .rdata/.data (vtable slot or data)
#   fx.py s <text>             strings containing <text> (ascii) with their RVA
#   fx.py sx <text>            exact NUL-terminated string -> RVA -> code that references it
#   fx.py cls <regex>          RTTI classes matching -> vtable RVAs and slot counts
#   fx.py vt <rva> [n]         a vtable: class name and its slots
#   fx.py q <rva> [n]          qwords at an RVA (pointers shown as code / string / vtable)
#   fx.py imm <hex>            uses of a 32-bit constant in code (function start, instruction)
#
# RVAs are hex without 0x. The RTTI and .pdata indexes are cached in .fxcache/ next to this file (not committed).
# Names come from symbols.txt next to this file ("<rva hex> <name>" per line; add to it as functions are identified).
import bisect
import os
import pickle
import re
import struct
import sys

import numpy as np

EXE = os.environ.get("MSMR_EXE", r"C:\Program Files (x86)\Steam\steamapps\common\Marvel's Spider-Man Remastered\Spider-Man.exe")
HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, ".fxcache")
BASE = 0x140000000


class Img:
    def __init__(self, path=EXE):
        self.d = open(path, "rb").read()
        d = self.d
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        n = struct.unpack_from("<H", d, pe + 6)[0]
        opt = struct.unpack_from("<H", d, pe + 20)[0]
        self.secs = []
        for i in range(n):
            name, vs, v, rs, r = struct.unpack_from("<8sIIII", d, pe + 24 + opt + i * 40)
            self.secs.append((name.rstrip(b"\0").decode(), v, vs, r, rs))
        self.sec = {s[0]: s for s in self.secs}
        self._pd = None
        self._rt = None
        self._sym = None
        self._tgt = None

    def off(self, rva):
        for _, v, vs, r, rs in self.secs:
            if v <= rva < v + rs:
                return rva - v + r
        return None

    def rva(self, off):
        for _, v, vs, r, rs in self.secs:
            if r <= off < r + rs:
                return off - r + v
        return None

    def secname(self, rva):
        for name, v, vs, r, rs in self.secs:
            if v <= rva < v + max(vs, rs):
                return name
        return None

    def u32(self, rva):
        o = self.off(rva)
        return struct.unpack_from("<I", self.d, o)[0] if o is not None else None

    def u64(self, rva):
        o = self.off(rva)
        return struct.unpack_from("<Q", self.d, o)[0] if o is not None else None

    def bytes(self, rva, n):
        o = self.off(rva)
        return self.d[o:o + n]

    def cstr(self, rva, maxlen=200):
        o = self.off(rva)
        if o is None:
            return None
        e = self.d.find(b"\0", o, o + maxlen)
        if e < 0 or e - o < 3:
            return None
        s = self.d[o:e]
        if all(32 <= c < 127 for c in s):
            return s.decode()
        return None

    # ---- .pdata
    @property
    def pd(self):
        if self._pd is None:
            _, v, vs, r, rs = self.sec[".pdata"]
            a = np.frombuffer(self.d, dtype="<u4", count=vs // 12 * 3, offset=r).reshape(-1, 3)
            a = a[a[:, 0] != 0]
            # a chunk whose unwind info has UNW_FLAG_CHAININFO (4) continues the previous function
            raw = np.frombuffer(self.d, dtype=np.uint8)
            rd = self.sec[".rdata"]
            uoff = a[:, 2].astype(np.int64) - rd[1] + rd[3]
            ok = (uoff >= 0) & (uoff < len(raw))
            flags = np.zeros(len(a), dtype=np.uint8)
            flags[ok] = raw[uoff[ok]] >> 3
            chained = (flags & 4) != 0
            self._pd = (a[:, 0].copy(), a[:, 1].copy(), chained)
        return self._pd

    def func(self, rva):
        st, en, ch = self.pd
        i = bisect.bisect_right(st, rva) - 1
        if i < 0 or not (st[i] <= rva < en[i]):
            return None
        lo = i
        while lo > 0 and ch[lo] and en[lo - 1] == st[lo]:
            lo -= 1
        hi = i
        while hi + 1 < len(st) and ch[hi + 1] and st[hi + 1] == en[hi]:
            hi += 1
        return int(st[lo]), int(en[hi])

    def func_start(self, rva):
        # .pdata splits some functions into chunks; walk back while the previous chunk ends exactly where this one
        # starts and there is no padding. Good enough for naming.
        f = self.func(rva)
        return f[0] if f else None

    # ---- RTTI
    @property
    def rt(self):
        if self._rt is None:
            p = os.path.join(CACHE, "rtti.pkl")
            if os.path.exists(p):
                self._rt = pickle.load(open(p, "rb"))
            else:
                self._rt = self._build_rtti()
                os.makedirs(CACHE, exist_ok=True)
                pickle.dump(self._rt, open(p, "wb"))
        return self._rt

    def _build_rtti(self):
        d = self.d
        td = {}  # rva of type descriptor -> name
        for secn in (".data", ".rdata"):
            _, v, vs, r, rs = self.sec[secn]
            for m in re.finditer(rb"\.\?A[VU][^\0]{1,400}\0", d[r:r + rs]):
                td[m.start() + v - 0x10] = m.group()[:-1].decode(errors="replace")
        _, v, vs, r, rs = self.sec[".rdata"]
        rd = np.frombuffer(d, dtype="<u4", count=rs // 4, offset=r)
        # COL: sig(1) off cdoff td chd self
        cols = {}
        idx = np.nonzero(rd[:-6] == 1)[0]
        for i in idx:
            col_rva = v + int(i) * 4
            if int(rd[i + 5]) == col_rva and int(rd[i + 3]) in td:
                cols[col_rva] = (td[int(rd[i + 3])], int(rd[i + 1]))
        q = np.frombuffer(d, dtype="<u8", count=rs // 8, offset=r)
        tlo, thi = BASE + 0x1000, BASE + 0x1000 + self.sec[".text"][2]
        vts = {}  # vtable rva -> (class, offset_in_object, nslots)
        f2v = {}  # function rva -> [(vtable rva, slot)]
        colset = np.array(sorted(cols), dtype=np.uint64) + np.uint64(BASE)
        hits = np.nonzero(np.isin(q, colset))[0]
        hitset = set(int(h) for h in hits)
        for h in hits:
            h = int(h)
            col = int(q[h]) - BASE
            name, ofs = cols[col]
            n = 0
            j = h + 1
            while j < len(q) and tlo <= int(q[j]) < thi and (j == h + 1 or j - 1 not in hitset):
                f2v.setdefault(int(q[j]) - BASE, []).append((v + (h + 1) * 8, n))
                n += 1
                j += 1
            vts[v + (h + 1) * 8] = (demangle(name), ofs, n)
        return {"vts": vts, "f2v": f2v}

    def vt_name(self, rva):
        e = self.rt["vts"].get(rva)
        return e[0] if e else None

    def slots_of(self, frva):
        out = []
        for vt, slot in self.rt["f2v"].get(frva, []):
            out.append("%s[%d]" % (self.rt["vts"][vt][0], slot))
        return out

    # ---- symbols
    @property
    def sym(self):
        if self._sym is None:
            self._sym = {}
            p = os.path.join(HERE, "symbols.txt")
            if os.path.exists(p):
                for line in open(p, encoding="utf-8"):
                    line = line.split("#")[0].split()
                    if len(line) >= 2:
                        try:
                            self._sym[int(line[0], 16)] = line[1]
                        except ValueError:
                            pass
            # component / script-action type objects (recon/component_types_4.0630.txt, see `fx.py types`)
            p = os.path.join(HERE, "..", "..", "recon", "component_types_4.0630.txt")
            if os.path.exists(p):
                for line in open(p, encoding="utf-8"):
                    line = line.split("#")[0].split()
                    if len(line) >= 2:
                        self._sym.setdefault(int(line[0], 16), "type:" + line[1])
        return self._sym

    def name(self, rva, short=False):
        if rva in self.sym:
            return self.sym[rva]
        s = self.slots_of(rva)
        if s:
            return s[0] + ("+%d more" % (len(s) - 1) if len(s) > 1 else "")
        return None

    def describe(self, rva):
        """What is at this RVA, for an operand annotation."""
        sec = self.secname(rva)
        if sec is None:
            return None
        if rva in self.sym:
            return self.sym[rva]
        if sec == ".text":
            n = self.name(rva)
            return "fn " + n if n else None
        vn = self.vt_name(rva)
        if vn:
            return "vt " + vn
        if sec in (".rdata", "_RDATA"):
            s = self.cstr(rva)
            if s:
                return '"%s"' % s[:80]
            q = self.u64(rva)
            if q and BASE <= q < BASE + 0x8400000:
                s2 = self.cstr(q - BASE)
                if s2:
                    return '-> "%s"' % s2[:60]
        return None

    # ---- xrefs
    @property
    def tgt(self):
        if self._tgt is None:
            _, v, vs, r, rs = self.sec[".text"]
            b = np.frombuffer(self.d, dtype=np.uint8, count=vs, offset=r)
            val = (b[:-3].astype(np.uint32) | (b[1:-2].astype(np.uint32) << 8) | (b[2:-1].astype(np.uint32) << 16)
                   | (b[3:].astype(np.uint32) << 24))
            pos = np.arange(len(val), dtype=np.uint32) + np.uint32(v + 4)
            self._tgt = (val + pos, b, v)
        return self._tgt

    def xrefs(self, target, slack=(0, 1, 4)):
        tgt, b, v = self.tgt
        out = []
        for k in slack:
            for i in np.nonzero(tgt == np.uint32(target - k))[0]:
                i = int(i)
                # classify by the bytes before the displacement
                p1 = int(b[i - 1]) if i >= 1 else 0
                p2 = int(b[i - 2]) if i >= 2 else 0
                kind = None
                if k == 0 and p1 == 0xE8:
                    kind = "call"
                elif k == 0 and p1 == 0xE9:
                    kind = "jmp"
                elif k == 0 and p2 == 0x0F and 0x80 <= p1 <= 0x8F:
                    kind = "jcc"
                elif (p1 & 0xC7) == 0x05:  # modrm with rip-relative
                    op = p2
                    kind = {0x8D: "lea", 0x8B: "mov r,[m]", 0x89: "mov [m],r", 0x3B: "cmp", 0x39: "cmp", 0x03: "add",
                            0x63: "movsxd", 0xFF: "call/jmp [m]", 0xC7: "mov [m],imm", 0x83: "op [m],imm8",
                            0x81: "op [m],imm32", 0x80: "op8 [m],imm8", 0x88: "mov8 [m],r", 0x8A: "mov8 r,[m]",
                            0x38: "cmp8", 0x3A: "cmp8", 0x10: "movss/ups r,[m]", 0x11: "movss/ups [m],r", 0x28: "movaps r,[m]",
                            0x29: "movaps [m],r", 0xB6: "movzx8", 0xB7: "movzx16", 0x2F: "comiss", 0x2E: "ucomiss", 0x59: "mulss",
                            0x58: "addss", 0x5C: "subss", 0x5E: "divss", 0x6F: "movdq", 0x7F: "movdq [m]", 0x85: "test",
                            0x84: "test8", 0xC6: "mov8 [m],imm", 0x87: "xchg", 0x33: "xor", 0x2B: "sub", 0x23: "and",
                            0x0B: "or", 0x09: "or [m]", 0x01: "add [m]", 0x29 + 0x100: "", 0xAF: "imul", 0x54: "andps",
                            0x57: "xorps", 0x5F: "maxss", 0x5D: "minss", 0x51: "sqrtss", 0x5A: "cvt", 0x2A: "cvtsi2ss",
                            0x2C: "cvttss2si", 0x14: "unpcklps", 0xC1: "xadd", 0xB1: "cmpxchg", 0x0D: "prefetch"}.get(op, "op%02x" % op)
                if kind is None:
                    continue
                out.append((v + i, kind, k))
        out.sort()
        return out

    def ptr_refs(self, target):
        """8-byte absolute pointers to target in .rdata/.data."""
        out = []
        for secn in (".rdata", ".data", "_RDATA"):
            _, v, vs, r, rs = self.sec[secn]
            q = np.frombuffer(self.d, dtype="<u8", count=rs // 8, offset=r)
            for i in np.nonzero(q == np.uint64(BASE + target))[0]:
                out.append(v + int(i) * 8)
        return out


def demangle(n):
    # .?AVName@Namespace@@ -> Namespace::Name ; templates left as is
    m = re.match(r"\.\?A[VU](.*)@@$", n)
    if not m:
        return n
    body = m.group(1)
    if "?" in body or "<" in body:
        return body
    return "::".join(reversed(body.split("@")))


def cs():
    import capstone
    c = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    c.detail = True
    return c


def disasm(img, start, end):
    import capstone
    from capstone import x86 as X
    c = cs()
    data = img.bytes(start, end - start)
    out = []
    for ins in c.disasm(data, start):
        note = ""
        if ins.mnemonic in ("call", "jmp") or ins.mnemonic.startswith("j"):
            op = ins.operands[0] if ins.operands else None
            if op is not None and op.type == X.X86_OP_IMM:
                t = op.imm
                f = img.func(t)
                if ins.mnemonic == "call" or not (start <= t < end):
                    n = img.name(t)
                    note = (n or "")
        for op in ins.operands:
            if op.type == X.X86_OP_MEM and op.mem.base == X.X86_REG_RIP:
                t = ins.address + ins.size + op.mem.disp
                dsc = img.describe(t)
                note = "[exe+%x]%s" % (t, " " + dsc if dsc else "")
        out.append((ins.address, ins.mnemonic, ins.op_str, note, ins))
    return out


def cmd_d(img, args):
    rva = int(args[0], 16)
    if len(args) > 1 and args[1] != "f":
        start, end = rva, rva + int(args[1], 16)
    else:
        f = img.func(rva)
        if not f:
            sys.exit("no .pdata entry for %x" % rva)
        start, end = f
        print("; function exe+%x..%x (%d bytes) %s" % (start, end, end - start, " ".join(img.slots_of(start)[:4])))
    for a, m, o, note, _ in disasm(img, start, end):
        o = re.sub(r"\b(qword|dword|word|byte|xmmword) ptr ", lambda mm: {"qword": "q", "dword": "d", "word": "w", "byte": "b", "xmmword": "x"}[mm.group(1)], o)
        print("%x %s %s%s" % (a, m, o, ("  ; " + note) if note else ""))


def cmd_sk(img, args):
    from capstone import x86 as X
    for a in args:
        rva = int(a, 16)
        f = img.func(rva)
        if not f:
            print("%x: no function" % rva)
            continue
        start, end = f
        calls, strs, vts, globs, icalls = [], [], [], [], []
        for adr, m, o, note, ins in disasm(img, start, end):
            if m == "call" or (m == "jmp" and ins.operands and ins.operands[0].type == X.X86_OP_IMM and not (start <= ins.operands[0].imm < end)):
                op = ins.operands[0]
                if op.type == X.X86_OP_IMM:
                    s = "%x%s" % (op.imm, ("=" + note) if note else "")
                    if m == "jmp":
                        s = "tail:" + s
                    if s not in calls:
                        calls.append(s)
                elif op.type == X.X86_OP_MEM and op.mem.base != X.X86_REG_RIP:
                    s = "[%s+%x]" % (ins.reg_name(op.mem.base), op.mem.disp)
                    if s not in icalls:
                        icalls.append(s)
            if note.startswith("[exe+"):
                t = int(note[5:].split("]")[0], 16)
                rest = note.split("]", 1)[1].strip()
                if rest.startswith('"') or rest.startswith("->"):
                    if rest not in strs:
                        strs.append(rest)
                elif rest.startswith("vt "):
                    if rest not in vts:
                        vts.append(rest[3:] + "@%x" % t)
                elif img.secname(t) in (".data",):
                    s = "%x%s" % (t, ("=" + rest) if rest else "")
                    if s not in globs:
                        globs.append(s)
        print("fn exe+%x..%x size %d %s" % (start, end, end - start, " ".join(img.slots_of(start)[:6])))
        if calls:
            print("  calls: " + " ".join(calls))
        if icalls:
            print("  vcalls: " + " ".join(icalls))
        if strs:
            print("  strings: " + " | ".join(strs))
        if vts:
            print("  vtables: " + " ".join(vts))
        if globs:
            print("  globals: " + " ".join(globs))


def cmd_f(img, args):
    for a in args:
        rva = int(a, 16)
        f = img.func(rva)
        if f:
            print("exe+%x in function exe+%x..%x %s %s" % (rva, f[0], f[1], img.sym.get(f[0], ""), " ".join(img.slots_of(f[0]))))
        else:
            print("exe+%x: no .pdata entry" % rva)


def cmd_x(img, args):
    for a in args:
        rva = int(a, 16)
        refs = img.xrefs(rva)
        print("== exe+%x %s: %d code refs" % (rva, img.describe(rva) or "", len(refs)))
        for at, kind, k in refs:
            f = img.func(at)
            fs = f[0] if f else 0
            n = img.name(fs) if f else None
            print("  %x %-12s in fn %x%s%s" % (at - (1 if kind in ("call", "jmp") else 0), kind, fs, (" " + n) if n else "", (" (imm slack %d)" % k) if k else ""))
        for p in img.ptr_refs(rva):
            desc = ""
            # which vtable?
            for vt, slot in img.rt["f2v"].get(rva, []):
                if vt + slot * 8 == p:
                    desc = "%s[%d]" % (img.rt["vts"][vt][0], slot)
            print("  ptr at exe+%x %s %s" % (p, img.secname(p), desc))


def cmd_s(img, args):
    needle = args[0].encode()
    d = img.d
    n = 0
    for m in re.finditer(re.escape(needle), d):
        o = m.start()
        s = o
        while s > 0 and 32 <= d[s - 1] < 127 and o - s < 120:
            s -= 1
        e = d.find(b"\0", o, o + 200)
        r = img.rva(s)
        if r is None:
            continue
        print("%x %s" % (r, d[s:e].decode(errors="replace")))
        n += 1
        if n >= int(args[1]) if len(args) > 1 else n >= 200:
            print("... (limit)")
            break


def cmd_sx(img, args):
    needle = args[0].encode() + b"\0"
    d = img.d
    for m in re.finditer(re.escape(needle), d):
        o = m.start()
        if o > 0 and d[o - 1] != 0:
            continue
        r = img.rva(o)
        if r is None:
            continue
        cmd_x(img, ["%x" % r])


def cmd_cls(img, args):
    rx = re.compile(args[0])
    rows = []
    for vt, (name, ofs, n) in img.rt["vts"].items():
        if rx.search(name):
            rows.append((name, ofs, vt, n))
    for name, ofs, vt, n in sorted(rows):
        print("%s vt exe+%x slots %d%s" % (name, vt, n, (" (subobject +0x%x)" % ofs) if ofs else ""))


def cmd_vt(img, args):
    rva = int(args[0], 16)
    e = img.rt["vts"].get(rva)
    n = int(args[1]) if len(args) > 1 else (e[2] if e else 16)
    print("vtable exe+%x %s" % (rva, e[0] if e else "?"))
    for i in range(n):
        t = img.u64(rva + i * 8) - BASE
        f = img.func(t)
        print("  [%d] +0x%x exe+%x%s%s" % (i, i * 8, t, (" size %d" % (f[1] - f[0])) if f else "", (" " + img.sym[t]) if t in img.sym else ""))


def cmd_q(img, args):
    rva = int(args[0], 16)
    n = int(args[1]) if len(args) > 1 else 8
    for i in range(n):
        q = img.u64(rva + i * 8)
        if q is None:
            print("%x (not in file)" % (rva + i * 8))
            continue
        note = ""
        if BASE <= q < BASE + 0x8400000:
            note = "exe+%x %s" % (q - BASE, img.describe(q - BASE) or img.secname(q - BASE) or "")
        else:
            lo, hi = q & 0xFFFFFFFF, q >> 32
            note = "u32 %x %x  f %g %g" % (lo, hi, struct.unpack("<f", struct.pack("<I", lo))[0], struct.unpack("<f", struct.pack("<I", hi))[0])
        print("%x %016x %s" % (rva + i * 8, q, note))


def cmd_imm(img, args):
    val = int(args[0], 16)
    _, v, vs, r, rs = img.sec[".text"]
    needle = struct.pack("<I", val)
    d = img.d
    seen = {}
    o = r
    while True:
        o = d.find(needle, o, r + vs)
        if o < 0:
            break
        rva = o - r + v
        f = img.func(rva)
        if f:
            seen.setdefault(f[0], []).append(rva)
        o += 1
    c = cs()
    for fs, hits in sorted(seen.items()):
        # find the instruction containing the first hit
        line = ""
        for ins in c.disasm(img.bytes(fs, hits[0] - fs + 16), fs):
            if ins.address <= hits[0] < ins.address + ins.size:
                line = "%x %s %s" % (ins.address, ins.mnemonic, ins.op_str)
                break
        print("fn %x %s (%d hits) %s" % (fs, img.name(fs) or "", len(hits), line))


# ---- Insomniac CRC-32 (init 0xEDB88320, no final xor): event type ids, enum value hashes, DDL type hashes, mission ids
_T = []
for _i in range(256):
    _c = _i
    for _ in range(8):
        _c = (_c >> 1) ^ 0xEDB88320 if _c & 1 else _c >> 1
    _T.append(_c)


def crc(name):
    v = 0xEDB88320
    for ch in name.encode():
        v = (v >> 8) ^ _T[(v ^ ch) & 0xFF]
    return v


def crc_lower(name):
    return crc(name.lower())


def hashdict(img):
    """crc -> name for every identifier-like string in the exe (and RTTI class names), cached."""
    p = os.path.join(CACHE, "hashdict.pkl")
    if os.path.exists(p):
        return pickle.load(open(p, "rb"))
    out = {}
    for m in re.finditer(rb"[A-Za-z_][A-Za-z0-9_:. /-]{2,90}(?=\x00)", img.d):
        s = m.group().decode()
        out.setdefault(crc(s), s)
        out.setdefault(crc(s.lower()), s)
    for name, _, _ in img.rt["vts"].values():
        base = name.split("::")[-1]
        out[crc(base)] = base
        out.setdefault(crc(name), name)
    extra = os.path.join(HERE, "names_extra.txt")
    if os.path.exists(extra):
        for line in open(extra, encoding="utf-8"):
            s = line.strip()
            if s:
                out[crc(s)] = s
    os.makedirs(CACHE, exist_ok=True)
    pickle.dump(out, open(p, "wb"))
    return out


def cmd_h(img, args):
    for a in args:
        print("%08x %s   (lowercased: %08x)" % (crc(a), a, crc(a.lower())))


def cmd_hn(img, args):
    hd = hashdict(img)
    for a in args:
        print("%s %s" % (a, hd.get(int(a, 16), "?")))


# ---- DDL enums: static descriptors in .rdata {u32 type hash, u32 count, ..., +0x18 u32* value hashes, +0x20 char** names,
# +0x40 values}; a table of pointers to them sits in .data. `fx.py enum <regex>` prints the enums with a matching value or
# type name; `fx.py enums <out file>` writes all of them.
def enums(img):
    _, v, vs, r, rs = img.sec[".rdata"]
    dlo, dhi = BASE + img.sec[".data"][1], BASE + img.sec[".data"][1] + img.sec[".data"][4]
    hd = hashdict(img)
    q = np.frombuffer(img.d, dtype="<u8", count=rs // 8, offset=r)
    out = []
    cand = np.nonzero((q[3:-6] >= dlo) & (q[3:-6] < dhi) & (q[4:-5] >= dlo) & (q[4:-5] < dhi) & (q[1:-8] == 0) & (q[2:-7] == 0))[0]
    for i in cand:
        i = int(i)
        rva = v + i * 8
        h, cnt = int(q[i]) & 0xFFFFFFFF, int(q[i]) >> 32
        if not (0 < cnt <= 4096):
            continue
        hashes, names = int(q[i + 3]) - BASE, int(q[i + 4]) - BASE
        rows = []
        ok = True
        for k in range(cnt):
            np_ = img.u64(names + k * 8)
            s = img.cstr(np_ - BASE, 300) if np_ and BASE <= np_ < BASE + 0x8400000 else None
            if s is None:
                s = ""
                if k == 0:
                    ok = False
                    break
            hv = img.u32(hashes + k * 4)
            rows.append((k, s, hv))
        if not ok:
            continue
        if rows and rows[0][2] != crc(rows[0][1]) and (len(rows) < 2 or rows[1][2] != crc(rows[1][1])):
            continue
        out.append((rva, h, hd.get(h, "?"), rows))
    return out


def fmt_enum(e):
    rva, h, name, rows = e
    lines = ["enum %s (type hash %08x, descriptor exe+%x, %d values)" % (name, h, rva, len(rows))]
    for k, s, hv in rows:
        lines.append("  %3d %-40s %08x" % (k, s, hv))
    return "\n".join(lines)


def cmd_enum(img, args):
    rx = re.compile(args[0])
    for e in enums(img):
        if rx.search(e[2]) or any(rx.search(r[1]) for r in e[3]):
            print(fmt_enum(e))


def enum_users(img):
    users = {}
    for name, (rva, h, base_h, size, fields) in ddl_types(img).items():
        for fname, kind, ofs, th, cnt in fields:
            if kind in (0xb, 0xc):
                users.setdefault(th, []).append("%s.%s" % (name, fname))
    return users


def cmd_enums(img, args):
    es = enums(img)
    users = enum_users(img)
    with open(args[0], "w", encoding="utf-8") as f:
        for e in es:
            u = sorted(users.get(e[1], []))
            f.write(fmt_enum(e) + (("\n  used by: " + ", ".join(u[:8]) + (" ... (%d)" % len(u) if len(u) > 8 else "")) if u else "") + "\n\n")
    print("%d enums written to %s" % (len(es), args[0]))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__ or "usage: fx.py <cmd> ...")
    img = Img()
    cmd = sys.argv[1]
    fn = globals().get("cmd_" + cmd)
    if not fn:
        sys.exit("unknown command " + cmd)
    fn(img, sys.argv[2:])




# ---- DDL type descriptors (static, in .data): every reflected type of the game (events, configs, component data, node pins).
#   +0x00 char* name, +0x08 u32 hash (crc of name), +0x10 u32 base type hash, +0x14 u32 size, +0x18 u32 field count,
#   +0x48 u32* field name hashes, +0x50 char** field names, +0x58 u8* field kinds, +0x70 u32* array counts,
#   +0x78 u32* field offsets, +0x80 u32* field type hashes, +0x88/+0x90 char** notes, +0x98 char** display names
KIND = {0: "u8", 1: "u16", 2: "u32", 3: "u64", 4: "i8", 5: "i16", 6: "i32", 7: "i64", 8: "float", 9: "double", 0xa: "string", 0xb: "enum", 0xc: "flags", 0xd: "struct", 0xf: "bool", 0x10: "file", 0x11: "tuid", 0x12: "json", 0x14: "assetid"}


def ddl_types(img):
    p = os.path.join(CACHE, "ddl.pkl")
    if os.path.exists(p):
        return pickle.load(open(p, "rb"))
    _, v, vs, r, rs = img.sec[".data"]
    q = np.frombuffer(img.d, dtype="<u8", count=rs // 8, offset=r)
    rlo, rhi = BASE + img.sec[".rdata"][1], BASE + img.sec[".rdata"][1] + img.sec[".rdata"][4]
    out = {}
    cand = np.nonzero((q[:-20] >= rlo) & (q[:-20] < rhi) & (q[10:-10] >= rlo) & (q[10:-10] < rhi) & (q[9:-11] >= rlo) & (q[9:-11] < rhi))[0]
    for i in cand:
        i = int(i)
        name = img.cstr(int(q[i]) - BASE, 200)
        if not name:
            continue
        h = int(q[i + 1]) & 0xFFFFFFFF
        if h != crc(name):
            continue
        base_h, size = int(q[i + 2]) & 0xFFFFFFFF, int(q[i + 2]) >> 32
        n = int(q[i + 3]) & 0xFFFFFFFF
        if n > 2000:
            continue
        a = [int(q[i + k]) - BASE for k in range(9, 20)]
        fields = []
        for k in range(n):
            fn = img.u64(a[1] + k * 8)
            fname = img.cstr(fn - BASE, 200) if fn and BASE <= fn < BASE + 0x8400000 else None
            kind = img.bytes(a[2] + k, 1)[0] if img.off(a[2]) is not None else 0
            cnt = img.u32(a[5] + k * 4) if img.off(a[5]) is not None else 1
            ofs = img.u32(a[6] + k * 4) if img.off(a[6]) is not None else 0
            th = img.u32(a[7] + k * 4) if img.off(a[7]) is not None else 0
            fields.append((fname or "?", kind, ofs, th, cnt))
        out[name] = (v + i * 8, h, base_h, size, fields)
    os.makedirs(CACHE, exist_ok=True)
    pickle.dump(out, open(p, "wb"))
    return out


_EN = None


def enum_map(img):
    global _EN
    if _EN is None:
        _EN = {e[1]: e for e in enums(img)}
    return _EN


def fmt_ddl(img, name, t, hd):
    rva, h, base_h, size, fields = t
    em = enum_map(img)
    lines = ["type %s (hash %08x, base %s, size 0x%x, descriptor exe+%x)" % (name, h, hd.get(base_h, "%08x" % base_h) if base_h else "-", size, rva)]
    for fname, kind, ofs, th, cnt in fields:
        tn = hd.get(th, "%08x" % th) if th and kind in (0xb, 0xc, 0xd) else ""
        if kind in (0xb, 0xc) and th in em:
            vals = [r[1] for r in em[th][3]]
            tn += " {" + ", ".join(vals[:6]) + (", ... %d values" % len(vals) if len(vals) > 6 else "") + "}"
        lines.append("  +0x%-4x %-36s %s%s%s" % (ofs, fname, KIND.get(kind, "kind%x" % kind), (" " + tn) if tn else "", ("[%d]" % cnt) if cnt != 1 else ""))
    return "\n".join(lines)


def cmd_ddl(img, args):
    rx = re.compile(args[0])
    hd = hashdict(img)
    for name, t in sorted(ddl_types(img).items()):
        if rx.search(name):
            print(fmt_ddl(img, name, t, hd))


def cmd_ddls(img, args):
    hd = hashdict(img)
    ts = ddl_types(img)
    with open(args[0], "w", encoding="utf-8") as f:
        for name, t in sorted(ts.items()):
            f.write(fmt_ddl(img, name, t, hd) + "\n\n")
    print("%d types written to %s" % (len(ts), args[0]))




def ddl_flat(img, name, base=0, prefix="", depth=0, out=None, hd=None, by_hash=None):
    """Flatten a DDL type into (offset, dotted name, kind, type name), expanding nested structs."""
    ts = ddl_types(img)
    if hd is None:
        hd = hashdict(img)
    if by_hash is None:
        by_hash = {t[1]: n for n, t in ts.items()}
    if out is None:
        out = []
    t = ts.get(name)
    if not t:
        return out
    for fname, kind, ofs, th, cnt in t[4]:
        tn = by_hash.get(th) or hd.get(th, "%08x" % th)
        if kind == 0xd and th in by_hash and cnt == 1 and depth < 6:
            ddl_flat(img, by_hash[th], base + ofs, prefix + fname + ".", depth + 1, out, hd, by_hash)
        else:
            out.append((base + ofs, prefix + fname, kind, tn if kind in (0xb, 0xc, 0xd) else "", cnt))
    return out


def cmd_ddlflat(img, args):
    em = enum_map(img)
    ts = ddl_types(img)
    by_hash = {t[1]: n for n, t in ts.items()}
    shift = int(args[1], 0) if len(args) > 1 else 0
    for ofs, name, kind, tn, cnt in sorted(ddl_flat(img, args[0])):
        extra = ""
        if kind in (0xb, 0xc):
            th = [h for h, n in by_hash.items() if n == tn]
            e = None
            for h, ev in em.items():
                if ("%08x" % h) == tn or ev[2] == tn:
                    e = ev
                    break
            if e:
                vals = [r[1] for r in e[3]]
                extra = " {" + ", ".join(vals[:8]) + (", ..." if len(vals) > 8 else "") + "}"
        print("+0x%-4x %-60s %s%s%s%s" % (ofs + shift, name, KIND.get(kind, "kind%x" % kind), (" " + tn) if tn else "", ("[%d]" % cnt) if cnt != 1 else "", extra))


# ---- flag sets (bitfields): descriptors in .data {u32 count, ..., +0x18 u32* values, +0x20 char** names}. They carry no type hash;
# `fx.py flags <regex>` prints the sets with a matching value name.
def flagsets(img):
    _, v, vs, r, rs = img.sec[".data"]
    q = np.frombuffer(img.d, dtype="<u8", count=rs // 8, offset=r)
    lo, hi = BASE + v, BASE + v + rs
    out = []
    cand = np.nonzero((q[:-6] > 0) & (q[:-6] <= 64) & (q[1:-5] == 0) & (q[2:-4] == 0) & (q[3:-3] >= lo) & (q[3:-3] < hi) & (q[4:-2] >= lo) & (q[4:-2] < hi))[0]
    for i in cand:
        i = int(i)
        cnt = int(q[i])
        vals, names = int(q[i + 3]) - BASE, int(q[i + 4]) - BASE
        rows = []
        for k in range(cnt):
            np_ = img.u64(names + k * 8)
            sname = img.cstr(np_ - BASE, 200) if np_ and BASE <= np_ < BASE + 0x8400000 else None
            if sname is None:
                rows = None
                break
            rows.append((sname, img.u32(vals + k * 4)))
        if rows:
            out.append((v + i * 8, rows))
    return out


def cmd_flags(img, args):
    rx = re.compile(args[0])
    for rva, rows in flagsets(img):
        if any(rx.search(n) for n, _ in rows):
            print("flags at exe+%x: %s" % (rva, ", ".join("%s=0x%x" % (n, val) for n, val in rows)))



if __name__ == "__main__":
    main()
