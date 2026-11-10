import sys, zipfile, os, re, json
lib = sys.argv[1]
pat = re.compile(r"atmo|night|day|sunset|rain|sky|morning|aurora|lavender|manhattanhenge|contrail|vibe|red night", re.I)
for f in sorted(os.listdir(lib)):
    if not pat.search(f) or not f.lower().endswith(('.smpcmod','.modular','.stage')): continue
    p = os.path.join(lib, f)
    try: z = zipfile.ZipFile(p)
    except Exception as e: print("##", f, "NOT ZIP", e); continue
    names = z.namelist()
    print("##", f, len(names), "entries")
    for n in names[:40]:
        i = z.getinfo(n); print("   %9d  %s" % (i.file_size, n))
    if len(names) > 40: print("   ...")
