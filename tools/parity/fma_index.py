import csv, sys, collections
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
so = open('binary/HappyWheels_Android/config.arm64_v8a/lib/arm64-v8a/libMyGame.so','rb').read()
# map vaddr->offset via program headers
import struct
phoff = struct.unpack_from('<Q', so, 0x20)[0]; phnum = struct.unpack_from('<H', so, 0x38)[0]
segs=[]
for i in range(phnum):
    p_type,p_flags,p_off,p_vaddr,p_paddr,p_filesz = struct.unpack_from('<IIQQQQ', so, phoff+56*i)
    if p_type==1: segs.append((p_vaddr,p_off,p_filesz))
def off(v):
    for va,o,sz in segs:
        if va<=v<va+sz: return v-va+o
md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
out=[]
for r in csv.DictReader(open('reports/decomp/functions.csv')):
    if r['owner'] in ('_preamble',) : continue
    n=int(r['insns'] or 0)
    if n==0: continue
    v=int(r['vaddr'],16); o=off(v)
    if o is None: continue
    c=collections.Counter()
    for ins in md.disasm(so[o:o+4*n], v):
        if ins.mnemonic in ('fmadd','fmsub','fnmadd','fnmsub','fmla','fmls'): c[ins.mnemonic]+=1
    if c: out.append((sum(c.values()), r['ghidra_addr'], r['owner'], r['class'], r['method'], dict(c)))
out.sort(key=lambda x:(x[2],x[3],x[1]))
with open(sys.argv[1],'w') as f:
    for t in out: f.write('%3d %s %-6s %-24s %-40s %s\n'%t)
print(len(out), sum(t[0] for t in out))
