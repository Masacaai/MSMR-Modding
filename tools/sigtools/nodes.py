# nodes.py [out.txt]: catalogue of the game's script-graph node types ("...Action" classes), built offline from Spider-Man.exe 4.0630.0.0.
# For each class with a ScriptAction-style vtable it lists: the execute method (vtable slot 10), the pin-name hashes that method compares its
# input against or passes to the pin API (resolved to names through fx.hashdict), the node's data fields (the "<Class>Prius" DDL type) and
# the component types and statics it touches. Pin API: inputs exe+1923410/1923490/19234e0/1923780/19238a0/1923cf0(ctx, &out, pinHash),
# outputs exe+1923a90(ctx, pinHash); ctx = [node+0x58]; the triggered input pin hash arrives in edx.
import re
import sys

import fx

PIN_IN = {0x1923410, 0x1923490, 0x19234e0, 0x1923780, 0x19238a0, 0x1923cf0, 0x1923520}
PIN_OUT = {0x1923a90}


def main():
    img = fx.Img()
    hd = fx.hashdict(img)
    ddl = fx.ddl_types(img)
    from capstone import x86 as X
    rows = []
    for vt, (name, ofs, n) in sorted(img.rt["vts"].items(), key=lambda kv: kv[1][0]):
        base = name.split("::")[-1]
        if not base.endswith("Action") or n < 11 or ofs:
            continue
        ex = img.u64(vt + 10 * 8) - fx.BASE
        f = img.func(ex)
        if not f or f[1] - f[0] > 6000:
            rows.append((base, vt, ex, [], [], [], []))
            continue
        ins_cmp, outs, ins_val, uses = [], [], [], []
        last_imm = {}
        for a, m, o, note, ins in fx.disasm(img, f[0], f[1]):
            ops = o.split(", ")
            if m == "cmp" and len(ops) == 2 and re.fullmatch(r"0x[0-9a-f]{6,8}", ops[1]) and ops[0] in ("edx", "esi", "edi", "ebx", "ebp", "eax", "r12d", "r13d", "r14d", "r15d"):
                h = int(ops[1], 16)
                if h in hd and h not in ins_cmp:
                    ins_cmp.append(h)
            elif m == "mov" and len(ops) == 2 and ops[0] in ("edx", "r8d") and re.fullmatch(r"0x[0-9a-f]{6,8}", ops[1]):
                last_imm[ops[0]] = int(ops[1], 16)
            elif m in ("call", "jmp") and ins.operands and ins.operands[0].type == X.X86_OP_IMM:
                t = ins.operands[0].imm
                if t in PIN_OUT and "edx" in last_imm and last_imm["edx"] not in outs:
                    outs.append(last_imm["edx"])
                elif t in PIN_IN:
                    h = last_imm.get("r8d") if t != 0x1923520 else last_imm.get("edx")
                    if h is not None and h not in ins_val:
                        ins_val.append(h)
            if "type:" in note:
                t = note.split("type:")[1].split()[0]
                if t not in uses and t != base:
                    uses.append(t)
        rows.append((base, vt, ex, ins_cmp, ins_val, outs, uses))
    out = open(sys.argv[1], "w", encoding="utf-8") if len(sys.argv) > 1 else sys.stdout
    out.write("# node class | vtable | execute | input pins (compared) | value pins (read) | output pins (fired) | data fields (<Class>Prius) | component types used\n")
    nm = lambda hs: ",".join(hd.get(h, "%08x" % h) for h in hs) or "-"
    for base, vt, ex, a, b, c, uses in rows:
        t = ddl.get(base + "Prius")
        flds = ",".join("%s@%x" % (fn, o) for fn, k, o, th, cnt in t[4] if fn != "ActionType") if t else ""
        out.write("%s | vt exe+%x | exec exe+%x | in: %s | values: %s | out: %s | data: %s | uses: %s\n" % (base, vt, ex, nm(a), nm(b), nm(c), flds or "-", ",".join(uses[:8]) or "-"))
    sys.stderr.write("%d node classes\n" % len(rows))


if __name__ == "__main__":
    main()
