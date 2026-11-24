# Saca los textos de la interfaz (literales de C++, uniendo los que van seguidos) y los consejos.
import re, json, sys, glob
files = sys.argv[1:]
keys = []
seen = set()
def add(s):
    if s in seen: return
    seen.add(s); keys.append(s)
def unescape(s):
    return bytes(s, 'utf-8').decode('unicode_escape').encode('latin-1').decode('utf-8')
for p in files:
    t = open(p, encoding='utf-8').read()
    # quita comentarios
    t = re.sub(r'//[^\n]*', lambda m: ' ' * len(m.group(0)) if '"' not in m.group(0) or m.group(0).count('"') % 2 == 0 else m.group(0), t)
    i = 0; n = len(t)
    while i < n:
        c = t[i]
        if c == "'" :
            j = i + 1
            while j < n and t[j] != "'":
                j += 2 if t[j] == '\\' else 1
            i = j + 1; continue
        if c == '"':
            # literal (y los que siguen pegados)
            parts = []; j = i
            line_start = t.rfind('\n', 0, i) + 1
            line = t[line_start:t.find('\n', i)]
            while True:
                k = j + 1; buf = []
                while k < n and t[k] != '"':
                    if t[k] == '\\': buf.append(t[k:k+2]); k += 2
                    else: buf.append(t[k]); k += 1
                parts.append(''.join(buf)); j = k + 1
                m = re.match(r'\s*"', t[j:])
                if m: j += m.end() - 1; continue
                break
            i = j
            raw = ''.join(parts)
            if 'Log::' in line or '#include' in line or 'ini.' in line or 'SetOption(' in line: continue
            try: s = unescape(raw)
            except Exception: s = raw
            if not re.search(r'[A-Za-zÁÉÍÓÚáéíóúñÑ]{2}', s): continue
            if s.startswith('##') or re.fullmatch(r'[a-z0-9_.\-/]+', s): continue
            if re.search(r'\.(png|ttf|json|ini|log|so|dll|exe|zip|apk|srm|cue|chd|bin|txt)$', s): continue
            if re.fullmatch(r'[A-Z0-9_]+', s) and len(s) < 4: continue
            add(s)
            continue
        i += 1
json.dump(keys, open('scripts/idiomas/claves_nuevas.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
print(len(keys), sum(len(k) for k in keys))
