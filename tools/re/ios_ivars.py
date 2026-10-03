#!/usr/bin/env python3
"""Dump Objective-C instance variables (original member names/types) from the iOS build.

The iOS app (1.2.7) is the original Objective-C codebase the Android C++ port was made from;
its ObjC metadata keeps every class's ivar names, types and offsets, which map almost 1:1 onto
the Android classes' members. Use it to name fields.

  python tools/re/ios_ivars.py CharacterB2D [MoreClasses...]
  python tools/re/ios_ivars.py --binary <macho> ClassName
(original version written by the M1 module agent)"""
import struct, sys

import os
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
args = sys.argv[1:]
path = os.path.join(ROOT, "binary", "HappyWheels_iOS", "Payload", "happywheels.app", "happywheels")
if args and args[0] == "--binary":
    path, args = args[1], args[2:]
want = set(args)
data = open(path, 'rb').read()

magic, cputype, cpusub, filetype, ncmds, sizeofcmds, flags, _ = struct.unpack_from('<IiiIIIII', data, 0)
assert magic == 0xfeedfacf, hex(magic)
off = 32
segs = []
sects = {}
for _ in range(ncmds):
    cmd, cmdsize = struct.unpack_from('<II', data, off)
    if cmd == 0x19:  # LC_SEGMENT_64
        segname = data[off+8:off+24].rstrip(b'\0').decode()
        vmaddr, vmsize, fileoff, filesize = struct.unpack_from('<QQQQ', data, off+24)
        nsects = struct.unpack_from('<I', data, off+64)[0]
        segs.append((segname, vmaddr, vmsize, fileoff, filesize))
        so = off + 72
        for i in range(nsects):
            sectname = data[so:so+16].rstrip(b'\0').decode()
            sseg = data[so+16:so+32].rstrip(b'\0').decode()
            addr, size, foff = struct.unpack_from('<QQI', data, so+32)
            sects[(sseg, sectname)] = (addr, size, foff)
            so += 80
    off += cmdsize

base = min(s[1] for s in segs if s[0] != '__PAGEZERO')

def v2o(va):
    for name, vmaddr, vmsize, fileoff, filesize in segs:
        if vmaddr <= va < vmaddr + filesize:
            return fileoff + (va - vmaddr)
    return None

def ptr(va_or_raw):
    v = va_or_raw
    if v >> 36:  # chained fixup: DYLD_CHAINED_PTR_64(_OFFSET) rebase -> low 36 bits target
        bind = v >> 63
        if bind:
            return None
        tgt = v & ((1 << 36) - 1)
        high8 = (v >> 36) & 0xff
        if tgt < base:
            tgt += base
        return tgt | (high8 << 56)
    return v

def rd64(va):
    o = v2o(va)
    return struct.unpack_from('<Q', data, o)[0]

def rd32(va):
    o = v2o(va)
    return struct.unpack_from('<I', data, o)[0]

def cstr(va):
    o = v2o(va)
    e = data.index(b'\0', o)
    return data[o:e].decode('utf-8', 'replace')

cl = sects.get(('__DATA', '__objc_classlist')) or sects.get(('__DATA_CONST', '__objc_classlist'))
addr, size, foff = cl
for i in range(size // 8):
    cls = ptr(struct.unpack_from('<Q', data, foff + i*8)[0])
    ro = ptr(rd64(cls + 32)) & ~7
    flags_, istart, isize, _r = struct.unpack_from('<IIII', data, v2o(ro))
    name = cstr(ptr(rd64(ro + 24)))
    if want and name not in want:
        continue
    sup = ptr(rd64(cls + 8))
    supname = '?'
    try:
        sro = ptr(rd64(sup + 32)) & ~7
        supname = cstr(ptr(rd64(sro + 24)))
    except Exception:
        pass
    print(f'class {name} : {supname}  instanceStart=0x{istart:x} instanceSize=0x{isize:x}')
    iv = ptr(rd64(ro + 48))
    if not iv:
        continue
    entsize, count = struct.unpack_from('<II', data, v2o(iv))
    for k in range(count):
        e = iv + 8 + k * (entsize & 0xffff)
        offp = ptr(rd64(e))
        nm = cstr(ptr(rd64(e + 8)))
        ty = cstr(ptr(rd64(e + 16)))
        align, sz = struct.unpack_from('<II', data, v2o(e + 24))
        o = rd32(offp) if offp else -1
        print(f'  +0x{o:04x} size={sz:<3} {nm:32s} {ty}')
