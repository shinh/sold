# How to use
# readelf --dyn-syms --wide hoge.so | python3 listup_duplicated_syms.py

import sys

alllines = sys.stdin.readlines()
head = alllines[:3]
lines = alllines[4:]
symcount = dict()

for l in lines:
    ws = l.split()
    name = ws[7].split('@')[0]
    if name in symcount:
        symcount[name] += 1
    else:
        symcount[name] = 1

for l in head:
    print(l, end="")

for l in lines:
    ws = l.split()
    name = ws[7].split('@')[0]
    if symcount[name] >= 2:
        print(l,end="")
