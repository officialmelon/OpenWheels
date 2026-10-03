"""Recompile TUs of build_parity with NDK clang-cl (same flags as MSVC + FMA contraction).
usage: clangify.py [regex-on-source-path] [extra clang flags...]
Writes build_parity/clangobj/build.bat; objects go to build_parity/clangobj/<name>.obj"""
import os
import re
import sys

R = r'<repo>'
B = R + r'\build_parity'
VCV = r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat'
CLANG = r'<home>\ndk-install\android-ndk-r27\toolchains\llvm\prebuilt\windows-x86_64\bin\clang-cl.exe'
only = sys.argv[1] if len(sys.argv) > 1 else r'SRC.GAME.'
extra = '-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH ' + (' '.join(sys.argv[2:]) if len(sys.argv) > 2 else '-mfma -mavx2 -ffp-contract=on')

tlog = open(B + r'\OpenWheels.dir\RelWithDebInfo\OpenWheels.tlog\CL.command.1.tlog', 'rb').read().decode('utf-16-le')
lines = tlog.splitlines()
out = []
for i, l in enumerate(lines):
    if not (l.startswith('^') and re.search(only, l, re.I) and '|' not in l):
        continue
    src, c = l[1:], lines[i + 1]
    for drop in ['/analyze-', '/diagnostics:column', '/Zi', '/WX-', '/external:W0']:
        c = c.replace(drop, '')
    c = re.sub(r'/Fo"[^"]*"', '', c)
    c = re.sub(r'/Fd"[^"]*"', '', c)
    c = c.replace('/external:I ', '/imsvc ')
    name = os.path.splitext(os.path.basename(src))[0]
    obj = B + '\\clangobj\\' + name + '.obj'
    c = re.sub(r'/D "CMAKE_INTDIR=[^ ]*', '', c)
    out.append('"%s" --target=i686-pc-windows-msvc %s -w %s /Fo"%s" || echo FAILED %s'
               % (CLANG, extra, c, obj, name))
with open(B + r'\clangobj\build.bat', 'w') as f:
    f.write('@echo off\ncall "' + VCV + '" >nul 2>nul\n' + '\n'.join(out) + '\necho DONE\n')
print(len(out), 'TUs')
