# gen_internals_index.py: rewrites the generated block of docs/internals/README.md (between the BEGIN/END markers): every page, with its `##` sections.
# Run after adding, renaming or splitting a page or heading:  python tools/docs/gen_internals_index.py
import glob, os, re
D = 'docs/internals'
def slug(h):
    h = re.sub(r'`', '', h.strip().lower()); h = re.sub(r'[^\w\- ]', '', h); return h.replace(' ', '-')
out = []
for p in sorted(glob.glob(D + '/*.md')):
    n = os.path.basename(p)
    if n == 'README.md': continue
    t = open(p, encoding='utf8').read()
    title = re.search(r'(?m)^# (.+)$', t).group(1)
    out.append(f'### [{title}]({n})')
    for h in re.findall(r'(?m)^## (.+)$', t):
        out.append(f'- [{h}]({n}#{slug(h)})')
    out.append('')
p = D + '/README.md'
t = open(p, encoding='utf8').read()
b, e = '<!-- BEGIN GENERATED INDEX -->', '<!-- END GENERATED INDEX -->'
t = re.sub(re.escape(b) + '.*?' + re.escape(e), lambda m: b + '\n\n' + '\n'.join(out) + '\n' + e, t, flags=re.S)
open(p, 'w', encoding='utf8', newline='\n').write(t)
print('index rewritten:', len(out), 'lines')
