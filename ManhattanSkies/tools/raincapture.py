# raincapture.py: records the exe's .data (mod command `dumpdata <name>`) twice in each lighting mode, for raindiff.py.
# Run with the game in the open world, standing still outdoors. Takes about 4 minutes. Writes <game>\scripts\dump_<tag><n>.bin.
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fieldprobe as fp

# (tag, lighting mode): Day first (dry), then the rain candidates and two more references
PLAN = [("rday", 0), ("rovc", 6), ("rnight", 5), ("rsunset", 2), ("rmorn", 4), ("rday", 0)]
SETTLE = 14   # seconds after the mode change: the lighting fade and the rain ramp
count = {}
for tag, mode in PLAN:
    fp.send("sky %d" % mode, wait=SETTLE)
    n = count.get(tag, 0)
    for k in (1, 2):
        name = "%s%d" % (tag, n * 2 + k)
        fp.send("dumpdata %s" % name, wait=4)
        print("dumped", name, "mode", mode, flush=True)
        if k == 1:
            time.sleep(6)
    count[tag] = n + 1
fp.send("sky 0", wait=1)
print("done")
