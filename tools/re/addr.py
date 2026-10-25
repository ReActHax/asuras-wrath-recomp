#!/usr/bin/env python3
import sys,re,bisect,os
G=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..','generated','default')
fs=sorted((int(a,16),n) for a,n in re.findall(r'SetFunction\(0x([0-9A-Fa-f]+),\s*(\w+)\)',open(G+'/asura_wrath_register.cpp').read()))
addrs=[a for a,_ in fs]
for x in sys.argv[1:]:
    v=int(x,16); i=bisect.bisect_right(addrs,v)-1
    print(x, '->', fs[i][1], hex(fs[i][0]), '+',hex(v-fs[i][0]))
