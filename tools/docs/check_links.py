# check_links.py: verifies every relative markdown link (file and #anchor) in the repository's .md files. Exit code 1 if any is broken.
# Usage: python tools/docs/check_links.py
import os, re, subprocess, sys
files = [f for f in subprocess.check_output(['git', 'ls-files', '-co', '--exclude-standard', '*.md']).decode().split('\n') if f and os.path.exists(f)]
def slug(h):
    h = re.sub(r'`', '', h.strip().lower()); h = re.sub(r'[^\w\- ]', '', h); return h.replace(' ', '-')
cache = {}
def anchors(p):
    if p not in cache: cache[p] = {slug(m) for m in re.findall(r'(?m)^#{1,6} (.+)$', open(p, encoding='utf8', errors='ignore').read())}
    return cache[p]
bad = []
for f in files:
    t = re.sub(r'(?s)```.*?```', '', open(f, encoding='utf8', errors='ignore').read())
    t = re.sub(r'`[^`\n]*`', '', t)
    for m in re.finditer(r'\]\(([^)#\s]*)(#[^)\s]*)?\)', t):
        l, a = m.group(1), (m.group(2) or '')[1:]
        if l.startswith(('http', 'mailto')): continue
        p = f if l == '' else os.path.normpath(os.path.join(os.path.dirname(f), l))
        if not os.path.exists(p): bad.append((f, l + ('#' + a if a else ''), 'missing file')); continue
        if a and p.endswith('.md') and a not in anchors(p): bad.append((f, l + '#' + a, 'missing anchor'))
for b in bad: print(*b, sep='  ')
print(len(bad), 'broken links in', len(files), 'files')
sys.exit(1 if bad else 0)
