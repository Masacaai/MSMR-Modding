# rewrite_links.py: one-shot helper used for the 2026-11-22 restructure; kept as the template for future moves.
# Usage: python tools/docs/rewrite_links.py mv.json   (mv.json = {"old/path": "new/path", ...}, files or directory prefixes ending in "/")
# Rewrites relative markdown links in every tracked/new .md file so they follow the moved targets, and re-points #anchors that moved with a split.
import json, os, re, subprocess, sys
mv = json.load(open(sys.argv[1]))
files = subprocess.check_output(['git', 'ls-files', '-co', '--exclude-standard', '*.md']).decode().split('\n')
files = [f for f in files if f and os.path.exists(f)]
inv = {v: k for k, v in mv.items() if not k.endswith('/')}
dirmaps = {k: v for k, v in mv.items() if k.endswith('/')}
def newpath(old):
    if old in mv: return mv[old]
    for k, v in dirmaps.items():
        if old.startswith(k): return v + old[len(k):]
    return old
def oldloc(new):
    if new in inv: return inv[new]
    for k, v in dirmaps.items():
        if new.startswith(v): return k + new[len(v):]
    return new
def slug(h):
    h = re.sub(r'`', '', h.strip().lower()); h = re.sub(r'[^\w\- ]', '', h); return h.replace(' ', '-')
heads = {}
for f in files:
    if f.startswith('docs/internals/'):
        for m in re.finditer(r'(?m)^#{1,6} (.+)$', open(f, encoding='utf8').read()): heads.setdefault(slug(m.group(1)), f)
LINK = re.compile(r'(\]\()([^)\s#]*)(#[^)\s]*)?(\))')
changed = 0
for f in files:
    t = open(f, encoding='utf8', newline='').read()
    od = os.path.dirname(oldloc(f)); nd = os.path.dirname(f)
    def fix(m):
        tgt, anc = m.group(2), m.group(3) or ''
        if tgt.startswith(('http', 'mailto')): return m.group(0)
        if tgt == '':
            if anc and f.startswith('docs/internals/'):
                own = {slug(x) for x in re.findall(r'(?m)^#{1,6} (.+)$', t)}
                if anc[1:] not in own and anc[1:] in heads:
                    return m.group(1) + os.path.relpath(heads[anc[1:]], nd).replace(os.sep, '/') + anc + m.group(4)
            return m.group(0)
        old = os.path.normpath(os.path.join(od, tgt)).replace(os.sep, '/')
        new = newpath(old)
        if old == 'docs/GAME_INTERNALS.md' and anc and anc[1:] in heads: new = heads[anc[1:]]
        rel = os.path.relpath(new, nd).replace(os.sep, '/')
        if tgt.endswith('/') and not rel.endswith('/'): rel += '/'
        return m.group(1) + rel + anc + m.group(4)
    n = LINK.sub(fix, t)
    if n != t: open(f, 'w', encoding='utf8', newline='').write(n); changed += 1
print('rewrote', changed, 'files')
