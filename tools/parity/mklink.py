"""Write build_parity/clangobj/link.rsp: the MSVC link of OpenWheels.exe with every game object
replaced by its clang-cl counterpart from build_parity/clangobj, output to bin/OpenWheels/Clang.
Build tree: OW_PARITY_BUILD, default <repo>/build_parity."""
import os
import re

B = os.environ.get('OW_PARITY_BUILD') or os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'build_parity'))
CO = os.path.join(B, 'clangobj')
tl = os.path.join(B, 'OpenWheels.dir', 'RelWithDebInfo', 'OpenWheels.tlog', 'link.command.1.tlog')
cmd = open(tl, 'rb').read().decode('utf-16-le').splitlines()[1]
toks = re.findall(r'"[^"]*"|\S+', cmd)
clang = set(f.upper() for f in os.listdir(CO) if f.lower().endswith('.obj'))
out, n = [], 0
skip = ('/INCREMENTAL', '/ILK:', '/PDB', '/DEBUG', '/MANIFEST', 'uiAccess', '/ManifestFile')
for t in toks:
    u = t.strip('"').upper()
    base = u.replace('/', os.sep).split(os.sep)[-1]
    if u.endswith('.OBJ') and base in clang:
        out.append('"' + os.path.join(CO, base) + '"')
        n += 1
        continue
    if t.startswith('/OUT:'):
        out.append('/OUT:"' + os.path.join(B, 'bin', 'OpenWheels', 'Clang', 'OpenWheels.exe') + '"')
        continue
    if t.startswith(skip):
        continue
    out.append(t)
objdir = os.path.join(B, 'OpenWheels.dir', 'RelWithDebInfo')
out = [t for t in out if not t.strip('"').upper().endswith('.OBJ')]
for f in sorted(os.listdir(objdir)):
    if f.lower().endswith(('.obj', '.res')):
        if f.upper() in clang:
            out.append('"' + os.path.join(CO, f.upper()) + '"'); n += 1
        else:
            out.append('"' + os.path.join(objdir, f) + '"')
out.append('/INCREMENTAL:NO')
open(os.path.join(CO, 'link.rsp'), 'w').write('\n'.join(out))
print('replaced', n)
