#!/usr/bin/env python3
# insn.py ADDR [ctx]: show the generated C for the instruction at ADDR
import sys,re,bisect,glob,os
G=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..','generated','default')
fs=sorted((int(a,16),n) for a,n in re.findall(r'SetFunction\(0x([0-9A-Fa-f]+),\s*(\w+)\)',open(G+'/asura_wrath_register.cpp').read()))
addrs=[a for a,_ in fs]
cache={}
def find(name):
    for p in glob.glob(G+'/asura_wrath_recomp.*.cpp'):
        if p not in cache: cache[p]=open(p).read()
        if f'DEFINE_REX_FUNC({name})' in cache[p]: return p
ctxn=int(sys.argv[2]) if len(sys.argv)>2 else 2
for x in sys.argv[1].split(','):
    v=int(x,16); i=bisect.bisect_right(addrs,v)-1; start,name=fs[i]
    p=find(name); L=cache[p].split('\n')
    s=next(k for k,l in enumerate(L) if l.startswith(f'DEFINE_REX_FUNC({name})'))
    idx=[]; k=s+1
    while k<len(L) and not L[k].startswith('DEFINE_REX_FUNC'):
        t=L[k].strip()
        if re.match(r'// [a-z]',t): idx.append(k)
        k+=1
    n=(v-start)//4
    j=idx[n]; nxt=idx[n+1] if n+1<len(idx) else j+3
    print(f'== {x} {name}+{hex(v-start)} {os.path.basename(p)}:{j+1}')
    a=idx[max(0,n-ctxn)]; b=idx[min(len(idx)-1,n+ctxn+1)]
    for q in range(a,b): print(('>> ' if j<=q<nxt else '   ')+L[q])
