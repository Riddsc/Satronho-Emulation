import json, re, os, glob
keys = json.load(open('scripts/idiomas/claves.json', encoding='utf-8'))
ph = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?(?:hh|h|ll|l|z)?[sdufxXc%]')
skip = set(range(904, 912)) | {790}
extra = [l.rstrip('\n').split('|') for l in open('scripts/idiomas/traducciones/extra.txt', encoding='utf-8') if l.strip()]
os.makedirs('assets/idiomas', exist_ok=True)
for li, lang in enumerate(['en', 'fr', 'pt', 'it']):
    table = {}
    bad = []
    for f in sorted(glob.glob(f'scripts/idiomas/traducciones/{lang}_*.txt')):
        for line in open(f, encoding='utf-8'):
            line = line.rstrip('\n')
            if not line: continue
            n, _, t = line.partition('|')
            n = int(n)
            if n in skip: continue
            k = keys[n]
            t = t.replace('\\n', '\n')
            if ph.findall(k) != ph.findall(t):
                bad.append((n, k, t)); continue
            # espacios al principio y al final como en el español (fragmentos)
            lead = k[:len(k) - len(k.lstrip(' '))]; trail = k[len(k.rstrip(' ')):]
            t = lead + t.strip(' ') + trail if t.strip(' ') else t
            if t != k: table[k] = t
    for row in extra:
        k, t = row[0], row[1 + li]
        if ph.findall(k) == ph.findall(t): table[k] = t
        else: bad.append(('extra', k, t))
    json.dump(table, open(f'assets/idiomas/{lang}.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=1, sort_keys=True)
    print(lang, len(table), 'malos:', bad[:5])
