# switchtest.py <addr_expr> <value> <restore>: poke a float, take screenshots before/after, restore. One-off gate helper.
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fieldprobe as fp

addr, val, restore = sys.argv[1], sys.argv[2], sys.argv[3]
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "recon", "probe")
def pk():
    fp.send("peek %s 4" % addr, wait=0.7)
    return fp.log_text().strip().splitlines()[-1]
base = fp.shot()
base.save(os.path.join(out, "S0_base.png"))
fp.send("pokef %s %s" % (addr, val), wait=0.8)
print("readback:", pk())
a = fp.shot()
a.save(os.path.join(out, "S1_poked.png"))
time.sleep(1.5)
print("readback later:", pk())
b = fp.shot()
b.save(os.path.join(out, "S2_later.png"))
print("diff base->poked", fp.compare(fp.small(base), fp.small(a)), "base->later", fp.compare(fp.small(base), fp.small(b)))
fp.send("pokef %s %s" % (addr, restore), wait=0.8)
print("restored:", pk())
