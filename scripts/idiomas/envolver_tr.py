# Envuelve con Tr() el texto de las llamadas que escriben en pantalla.
import re, sys
FUNCS = {  # nombre -> función que elige el índice del argumento de texto
 'ImGui::TextUnformatted': lambda a: 0,
 'ImGui::TextWrapped': lambda a: 1 if a[0].strip()=='"%s"' else 0,
 'ImGui::Text': lambda a: 1 if a[0].strip()=='"%s"' else 0,
 'ImGui::MenuItem': lambda a: 0,
 'ImGui::BeginMenu': lambda a: 0,
 'ImGui::Selectable': lambda a: 0,
 'ImGui::SmallButton': lambda a: 0,
 'ImGui::Button': lambda a: 0,
 'ImGui::Checkbox': lambda a: 0,
 'ImGui::CalcTextSize': lambda a: 0,
 'ImGui::SetTooltip': lambda a: 1 if a[0].strip()=='"%s"' else 0,
 '->CalcTextSizeA': lambda a: 3 if len(a)>=4 else None,
 '->AddText': lambda a: 2 if len(a)==3 else (4 if len(a)>=5 else None),
}
def split_args(s, i):
    # s[i] == '(' ; devuelve (lista de (ini,fin) de cada argumento, índice del ')')
    depth=0; args=[]; start=i+1; j=i; instr=None
    while j < len(s):
        c=s[j]
        if instr:
            if c=='\\': j+=2; continue
            if c==instr: instr=None
        elif c in '"\'': instr=c
        elif c in '([{': depth+=1
        elif c in ')]}':
            depth-=1
            if depth==0:
                args.append((start,j)); return args,j
        elif c==',' and depth==1:
            args.append((start,j)); start=j+1
        j+=1
    return None,None
def process(text):
    out=[]; pos=0; changes=0
    pat=re.compile('|'.join(re.escape(k)+r'\s*\(' for k in sorted(FUNCS,key=len,reverse=True)))
    for m in pat.finditer(text):
        if m.start() < pos: continue
        name=re.sub(r'\s*\($','',m.group(0))
        # evita que «ImGui::Text» atrape «ImGui::TextUnformatted»
        par=m.end()-1
        args,end=split_args(text,par)
        if not args: continue
        argtxt=[text[a:b] for a,b in args]
        idx=FUNCS[name](argtxt)
        if idx is None or idx>=len(args): continue
        a,b=args[idx]
        raw=text[a:b]; st=raw.strip()
        if not st or st.startswith('Tr(') or st=='nullptr' or st.startswith('I18n::Tr('): continue
        lead=raw[:len(raw)-len(raw.lstrip())]; trail=raw[len(raw.rstrip()):]
        out.append(text[pos:a]); out.append(lead+'Tr('+st+')'+trail); pos=b; changes+=1
    out.append(text[pos:])
    return ''.join(out),changes
for p in sys.argv[1:]:
    t=open(p,encoding='utf-8').read()
    n,c=process(t)
    if c: open(p,'w',encoding='utf-8').write(n)
    print(p,c)
