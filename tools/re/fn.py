#!/usr/bin/env python3
# usage: fn.py <sub_XXXXXXXX | file:line>  -> prints function body (asm comments only with -a)
import sys,re,glob,os
G=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..','generated','default')
arg=sys.argv[1]; asm='-a' in sys.argv
if ':' in arg:
    f,l=arg.split(':'); l=int(l); path=os.path.join(G,f)
    lines=open(path).read().split('\n'); s=l-1
    while not lines[s].startswith('DEFINE_REX_FUNC'): s-=1
    name=re.search(r'\((\w+)\)',lines[s]).group(1)
else:
    name=arg
    for p in glob.glob(G+'/asura_wrath_recomp.*.cpp'):
        txt=open(p).read()
        if f'DEFINE_REX_FUNC({name})' in txt:
            path=p; lines=txt.split('\n'); break
    s=next(i for i,x in enumerate(lines) if x.startswith(f'DEFINE_REX_FUNC({name})'))
e=s+1
while e<len(lines) and not lines[e].startswith('DEFINE_REX_FUNC'): e+=1
print(f'// {name} in {os.path.basename(path)}:{s+1}-{e}')
for x in lines[s:e]:
    if asm:
        t=x.strip()
        if t.startswith('// ') or t.startswith('loc_') or 'REX_CALL' in t or re.match(r'(sub_|__imp__)\w+\(ctx',t): print(t)
    else: print(x)
