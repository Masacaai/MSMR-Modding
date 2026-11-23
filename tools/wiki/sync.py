# sync.py: builds the GitHub Wiki from the repository's docs and (optionally) pushes it. The wiki is a generated, read-only mirror: never edit it by hand,
# change docs/internals/ (or the guides listed below) and re-run. The repository stays the source of truth.
#
#   python tools/wiki/sync.py             build into wiki-out/ (git-ignored) and report; touches nothing else
#   python tools/wiki/sync.py --push      also clone <repo>.wiki.git, replace its contents with wiki-out/, commit and push
#
# Published: every docs/internals/*.md page (README.md becomes Home) plus the guides in GUIDES. Links between published pages become wiki links;
# links to anything else become absolute links into the repository on GitHub. Each page gets a banner pointing at its source file.
# One-time setup: enable the Wiki in the repository settings and save any first page by hand (GitHub creates the wiki repo on first save).
import os, re, shutil, subprocess, sys, tempfile

GUIDES = {  # source -> wiki page name
    'docs/GETTING_STARTED.md': 'Getting-started',
    'docs/REVERSE_ENGINEERING.md': 'Reverse-engineering-guide',
}
ROOT = subprocess.check_output(['git', 'rev-parse', '--show-toplevel'], text=True).strip()
os.chdir(ROOT)

def repo_slug():
    url = subprocess.check_output(['git', 'remote', 'get-url', 'origin'], text=True).strip()
    return re.sub(r'(\.git)?$', '', re.sub(r'^.*github\.com[:/]', '', url))
SLUG = repo_slug()
BLOB = f'https://github.com/{SLUG}/blob/main/'
TREE = f'https://github.com/{SLUG}/tree/main/'

def wiki_name(rel):
    if rel in GUIDES: return GUIDES[rel]
    base = os.path.basename(rel)[:-3]
    if rel == 'docs/internals/README.md': return 'Home'
    name = base[:1].upper() + base[1:]
    return re.sub(r'^Ui(?=$|-)', 'UI', name)

pages = {}  # source rel path -> wiki page name
for f in sorted(os.listdir('docs/internals')):
    if f.endswith('.md'): pages['docs/internals/' + f] = wiki_name('docs/internals/' + f)
for g in GUIDES: pages[g] = GUIDES[g]

LINK = re.compile(r'(\]\()([^)\s#]*)(#[^)\s]*)?(\))')
def convert(src, text):
    d = os.path.dirname(src)
    def fix(m):
        tgt, anc = m.group(2), m.group(3) or ''
        if tgt.startswith(('http', 'mailto')): return m.group(0)
        if tgt == '': return m.group(0)
        rel = os.path.normpath(os.path.join(d, tgt)).replace(os.sep, '/')
        if rel in pages: return m.group(1) + pages[rel] + anc + m.group(4)
        if not os.path.exists(rel): return m.group(0)
        return m.group(1) + (TREE if os.path.isdir(rel) else BLOB) + rel + (anc if rel.endswith('.md') else '') + m.group(4)
    return LINK.sub(fix, text)

out = 'wiki-out'
shutil.rmtree(out, ignore_errors=True); os.makedirs(out)
for src, name in pages.items():
    t = open(src, encoding='utf8').read()
    t = re.sub(r'(?s)<!-- BEGIN GENERATED INDEX -->.*?<!-- END GENERATED INDEX -->', '', t) if name == 'Home' else t
    banner = f'> Generated from [`{src}`]({BLOB}{src}) in the repository. Do not edit this page here; change the file and the wiki is rebuilt.\n\n'
    open(os.path.join(out, name + '.md'), 'w', encoding='utf8', newline='\n').write(banner + convert(src, t))

side = ['**[Home](Home)**', '', '**Guides**', '- [Getting started](Getting-started)', '- [Reverse-engineering guide](Reverse-engineering-guide)', '', '**Game internals**']
for src, name in pages.items():
    if src.startswith('docs/internals/') and name != 'Home':
        title = re.search(r'(?m)^# (.+)$', open(src, encoding='utf8').read()).group(1)
        side.append(f'- [{title}]({name})')
side += ['', f'[Mods and source on GitHub](https://github.com/{SLUG})']
open(os.path.join(out, '_Sidebar.md'), 'w', encoding='utf8', newline='\n').write('\n'.join(side) + '\n')
print(f'built {len(pages)} pages + sidebar in {out}/ (repository: {SLUG})')

if '--push' in sys.argv:
    token = os.environ.get('WIKI_TOKEN')
    url = f'https://github.com/{SLUG}.wiki.git'
    if token: url = f'https://x-access-token:{token}@github.com/{SLUG}.wiki.git'
    tmp = tempfile.mkdtemp()
    try:
        subprocess.check_call(['git', 'clone', '--quiet', url, tmp])
        for f in os.listdir(tmp):
            if f != '.git': (shutil.rmtree if os.path.isdir(os.path.join(tmp, f)) else os.remove)(os.path.join(tmp, f))
        for f in os.listdir(out): shutil.copy(os.path.join(out, f), tmp)
        subprocess.check_call(['git', '-C', tmp, 'add', '-A'])
        if subprocess.call(['git', '-C', tmp, 'diff', '--cached', '--quiet']) == 0: print('wiki already up to date')
        else:
            subprocess.check_call(['git', '-C', tmp, '-c', 'user.name=wiki-sync', '-c', 'user.email=wiki-sync@users.noreply.github.com', 'commit', '-q', '-m', 'Sync from docs/ (generated)'])
            subprocess.check_call(['git', '-C', tmp, 'push', '--quiet']); print('wiki pushed')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
