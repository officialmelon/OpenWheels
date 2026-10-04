"""Recompile TUs of build_parity with NDK clang-cl (same flags as MSVC + FMA contraction).
usage: clangify.py [regex-on-source-path] [extra clang flags...]
Writes build_parity/clangobj/build.bat; objects go to build_parity/clangobj/<name>.obj
Environment: OW_PARITY_BUILD (build tree, default <repo>/build_parity), OW_CLANG_CL (clang-cl.exe;
default: the one in $ANDROID_NDK, NDK r27), OW_VCVARS32 (VS 2022 vcvars32.bat, default Community)."""
import os
import re
import sys

R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
B = os.environ.get('OW_PARITY_BUILD') or os.path.join(R, 'build_parity')
VCV = os.environ.get('OW_VCVARS32') or \
    r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat'
CLANG = os.environ.get('OW_CLANG_CL')
if not CLANG:
    NDK = (os.environ.get('ANDROID_NDK') or os.environ.get('ANDROID_NDK_HOME')
           or os.environ.get('ANDROID_NDK_ROOT'))
    if not NDK:
        sys.exit('clangify.py: set OW_CLANG_CL to clang-cl.exe, or ANDROID_NDK to an NDK r27 folder')
    CLANG = os.path.join(NDK, 'toolchains', 'llvm', 'prebuilt', 'windows-x86_64', 'bin', 'clang-cl.exe')
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
