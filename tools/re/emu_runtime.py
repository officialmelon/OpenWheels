"""Bionic/Android runtime stubs for the libMyGame.so emulator (tools/re/emu.py).

Installs Python implementations of the imports the original library needs to boot cocos2d-x,
read the player's assets, load levels and step Box2D: libc, libm, stdio over the asset
directory, AAssetManager, GLES (inert fakes that hand out ids), pthreads (single thread),
time (deterministic). Unsupported imports still raise so gaps are visible.
"""
import math
import os
import re
import struct

from unicorn.arm64_const import (UC_ARM64_REG_SP, UC_ARM64_REG_LR, UC_ARM64_REG_PC, UC_ARM64_REG_X9,
                                 UC_ARM64_REG_D0, UC_ARM64_REG_D1, UC_ARM64_REG_D2, UC_ARM64_REG_D3,
                                 UC_ARM64_REG_D4, UC_ARM64_REG_D5, UC_ARM64_REG_D6, UC_ARM64_REG_D7,
                                 UC_ARM64_REG_S0, UC_ARM64_REG_S1, UC_ARM64_REG_S2)

from emu import Emu, EmuError, XREGS, STUB_BASE, align_up

DREGS = [UC_ARM64_REG_D0, UC_ARM64_REG_D1, UC_ARM64_REG_D2, UC_ARM64_REG_D3,
         UC_ARM64_REG_D4, UC_ARM64_REG_D5, UC_ARM64_REG_D6, UC_ARM64_REG_D7]


def f2u(f):
    return struct.unpack("<I", struct.pack("<f", f))[0]


def u2f(u):
    return struct.unpack("<f", struct.pack("<I", u & 0xFFFFFFFF))[0]


def d2u(d):
    return struct.unpack("<Q", struct.pack("<d", d))[0]


def u2d(u):
    return struct.unpack("<d", struct.pack("<Q", u & 0xFFFFFFFFFFFFFFFF))[0]


def to_f32(x):
    try:
        return struct.unpack("<f", struct.pack("<f", x))[0]
    except OverflowError:
        return math.copysign(math.inf, x)


class Runtime:
    def __init__(self, emu: Emu, assets_dir=None, verbose=False):
        self.e = emu
        self.assets_dir = assets_dir
        self.verbose = verbose
        self.files = {}       # handle -> [bytes, pos]
        self.next_handle = 0x1000
        self.gl_ids = 1
        self.tls_keys = {}
        self.errno_addr = emu.malloc(16)
        self.static_strs = {}
        self.free_bins = {}   # size -> [addr]
        self.sizes = {}       # addr -> size
        self.log = []
        self.rand_state = None
        self.fake_time = 1_700_000_000.0
        self._tramp = self._make_once_trampoline()
        self._install()

    # ----------------------------------------------------------- helpers
    def cstr_const(self, s):
        if s not in self.static_strs:
            b = s.encode() + b"\0"
            a = self.e.malloc(len(b))
            self.e.w(a, b)
            self.static_strs[s] = a
        return self.static_strs[s]

    def x(self, i):
        return self.e.x(i)

    def sreg(self, i):
        return u2f(self.e.uc.reg_read(DREGS[i]) & 0xFFFFFFFF)

    def dreg(self, i):
        return u2d(self.e.uc.reg_read(DREGS[i]))

    def ret_f32(self, v):
        self.e.uc.reg_write(UC_ARM64_REG_S0, f2u(to_f32(v)))

    def ret_f64(self, v):
        self.e.uc.reg_write(UC_ARM64_REG_D0, d2u(v))

    def alloc(self, n, align=16):
        n = max(align_up(n, 16), 16)
        b = self.free_bins.get(n)
        if b:
            a = b.pop()
            self.e.w(a, b"\0" * n)
            return a
        a = self.e.malloc(n, max(align, 16))
        self.sizes[a] = n
        return a

    def free(self, a):
        n = self.sizes.get(a)
        if a and n:
            self.free_bins.setdefault(n, []).append(a)

    def _make_once_trampoline(self):
        # stp x29,x30,[sp,#-16]! ; blr x9 ; ldp x29,x30,[sp],#16 ; mov x0,#0 ; ret
        code = bytes.fromhex("fd7bbfa9" "20013fd6" "fd7bc1a8" "000080d2" "c0035fd6")
        a = STUB_BASE + 0xF0000
        self.e.w(a, code)
        return a

    # ----------------------------------------------------------- printf
    class _RegArgs:
        def __init__(self, rt, first_x, first_v=0):
            self.rt, self.xi, self.vi = rt, first_x, first_v
            self.stack = rt.e.uc.reg_read(UC_ARM64_REG_SP)

        def int(self):
            if self.xi < 8:
                v = self.rt.x(self.xi)
                self.xi += 1
                return v
            v = self.rt.e.u64(self.stack)
            self.stack += 8
            return v

        def dbl(self):
            if self.vi < 8:
                v = self.rt.dreg(self.vi)
                self.vi += 1
                return v
            v = u2d(self.rt.e.u64(self.stack))
            self.stack += 8
            return v

    class _VaList:
        """AAPCS64 va_list: {__stack, __gr_top, __vr_top, int __gr_offs, int __vr_offs}"""

        def __init__(self, rt, va):
            self.rt, self.va = rt, va

        def _field(self):
            e = self.rt.e
            stack, gr_top, vr_top = e.u64(self.va), e.u64(self.va + 8), e.u64(self.va + 16)
            gr, vr = struct.unpack("<ii", e.r(self.va + 24, 8))
            return stack, gr_top, vr_top, gr, vr

        def int(self):
            e = self.rt.e
            stack, gr_top, vr_top, gr, vr = self._field()
            if gr < 0:
                v = e.u64(gr_top + gr)
                e.w(self.va + 24, struct.pack("<i", gr + 8))
                return v
            v = e.u64(stack)
            e.w(self.va, struct.pack("<Q", stack + 8))
            return v

        def dbl(self):
            e = self.rt.e
            stack, gr_top, vr_top, gr, vr = self._field()
            if vr < 0:
                v = u2d(e.u64(vr_top + vr))
                e.w(self.va + 28, struct.pack("<i", vr + 16))
                return v
            v = u2d(e.u64(stack))
            e.w(self.va, struct.pack("<Q", stack + 8))
            return v

    FMT = re.compile(r"%([-+ #0]*)(\*|\d+)?(?:\.(\*|\d+))?(hh|h|ll|l|L|z|j|t|q)?([diouxXeEfFgGaAcspn%])")

    def format(self, fmt, args):
        out = []
        pos = 0
        for m in self.FMT.finditer(fmt):
            out.append(fmt[pos:m.start()])
            pos = m.end()
            flags, width, prec, length, conv = m.groups()
            if conv == "%":
                out.append("%")
                continue
            if width == "*":
                width = str(struct.unpack("<i", struct.pack("<I", args.int() & 0xFFFFFFFF))[0])
            if prec == "*":
                prec = str(struct.unpack("<i", struct.pack("<I", args.int() & 0xFFFFFFFF))[0])
            spec = "%" + flags + (width or "") + (("." + prec) if prec is not None else "")
            if conv in "di":
                v = args.int()
                bits = 64 if length in ("l", "ll", "z", "j", "t", "q") else 32
                v &= (1 << bits) - 1
                if v >= 1 << (bits - 1):
                    v -= 1 << bits
                out.append((spec + "d") % v)
            elif conv in "ouxX":
                v = args.int()
                bits = 64 if length in ("l", "ll", "z", "j", "t", "q") else 32
                out.append((spec + conv) % (v & ((1 << bits) - 1)))
            elif conv in "eEfFgGaA":
                v = args.dbl()
                c = "f" if conv in "aA" else conv
                try:
                    out.append((spec + c) % v)
                except (OverflowError, ValueError):
                    out.append(str(v))
            elif conv == "c":
                out.append((spec + "c") % chr(args.int() & 0xFF))
            elif conv == "s":
                p = args.int()
                s = self.e.cstr(p) if p else "(null)"
                out.append((spec + "s") % s)
            elif conv == "p":
                out.append("0x%x" % args.int())
            elif conv == "n":
                args.int()
        out.append(fmt[pos:])
        return "".join(out)

    def _write_bounded(self, buf, size, text):
        data = text.encode("utf-8", "replace")
        if size:
            n = min(len(data), size - 1)
            self.e.w(buf, data[:n] + b"\0")
        return len(data)

    # ----------------------------------------------------------- files / assets
    def _asset_path(self, name):
        if self.assets_dir is None:
            return None
        name = name.lstrip("/")
        if name.startswith("assets/"):
            name = name[len("assets/"):]
        p = os.path.join(self.assets_dir, *name.split("/"))
        return p if os.path.isfile(p) else None

    def _open_handle(self, data):
        h = self.next_handle
        self.next_handle += 0x10
        self.files[h] = [data, 0]
        return h

    # ----------------------------------------------------------- install
    def _install(self):
        e = self.e
        H = e.handlers
        rt = self

        def reg(*names):
            def deco(fn):
                for n in names:
                    H[n] = fn
                return fn
            return deco

        # ---- memory
        @reg("malloc", "_Znwm", "_Znam")
        def _malloc(e):
            return rt.alloc(e.x(0))

        @reg("calloc")
        def _calloc(e):
            return rt.alloc(e.x(0) * e.x(1))

        @reg("memalign")
        def _memalign(e):
            return rt.alloc(e.x(1), e.x(0))

        @reg("posix_memalign")
        def _pmemalign(e):
            p = rt.alloc(e.x(2), e.x(1))
            e.w(e.x(0), struct.pack("<Q", p))
            return 0

        @reg("realloc")
        def _realloc(e):
            old, n = e.x(0), e.x(1)
            p = rt.alloc(n)
            if old:
                on = rt.sizes.get(old, n)
                e.w(p, e.r(old, min(on, n)))
                rt.free(old)
            return p

        @reg("free", "_ZdlPv", "_ZdaPv")
        def _free(e):
            rt.free(e.x(0))

        @reg("mmap")
        def _mmap(e):
            return rt.alloc(e.x(1), 4096)

        @reg("munmap")
        def _munmap(e):
            return 0

        # ---- strings
        @reg("memchr")
        def _memchr(e):
            p, c, n = e.x(0), e.x(1) & 0xFF, e.x(2)
            i = e.r(p, n).find(bytes([c])) if n else -1
            return p + i if i >= 0 else 0

        @reg("__memcpy_chk")
        def _memcpy_chk(e):
            if e.x(2):
                e.w(e.x(0), e.r(e.x(1), e.x(2)))
            return e.x(0)

        @reg("__memset_chk")
        def _memset_chk(e):
            if e.x(2):
                e.w(e.x(0), bytes([e.x(1) & 0xFF]) * e.x(2))
            return e.x(0)

        @reg("strlen", "__strlen_chk")
        def _strlen(e):
            return len(e.cstr(e.x(0)).encode("utf-8", "surrogateescape"))

        def rawstr(a):
            out = bytearray()
            while True:
                chunk = e.r(a + len(out), 64)
                z = chunk.find(b"\0")
                if z >= 0:
                    return bytes(out + chunk[:z])
                out += chunk

        rt.rawstr = rawstr

        @reg("strcmp")
        def _strcmp(e):
            a, b = rawstr(e.x(0)), rawstr(e.x(1))
            return (0 if a == b else (-1 if a < b else 1)) & 0xFFFFFFFFFFFFFFFF

        @reg("strncmp")
        def _strncmp(e):
            n = e.x(2)
            a, b = rawstr(e.x(0))[:n], rawstr(e.x(1))[:n]
            return (0 if a == b else (-1 if a < b else 1)) & 0xFFFFFFFFFFFFFFFF

        @reg("strcpy", "__strcpy_chk")
        def _strcpy(e):
            e.w(e.x(0), rawstr(e.x(1)) + b"\0")
            return e.x(0)

        @reg("strncpy")
        def _strncpy(e):
            s = rawstr(e.x(1))[:e.x(2)]
            e.w(e.x(0), s + b"\0" * (e.x(2) - len(s)))
            return e.x(0)

        @reg("strcat", "__strcat_chk")
        def _strcat(e):
            d = e.x(0)
            e.w(d + len(rawstr(d)), rawstr(e.x(1)) + b"\0")
            return d

        @reg("strchr", "__strchr_chk")
        def _strchr(e):
            s, c = rawstr(e.x(0)), e.x(1) & 0xFF
            if c == 0:
                return e.x(0) + len(s)
            i = s.find(bytes([c]))
            return e.x(0) + i if i >= 0 else 0

        @reg("strrchr", "__strrchr_chk")
        def _strrchr(e):
            s, c = rawstr(e.x(0)), e.x(1) & 0xFF
            if c == 0:
                return e.x(0) + len(s)
            i = s.rfind(bytes([c]))
            return e.x(0) + i if i >= 0 else 0

        @reg("strstr")
        def _strstr(e):
            i = rawstr(e.x(0)).find(rawstr(e.x(1)))
            return e.x(0) + i if i >= 0 else 0

        @reg("strdup")
        def _strdup(e):
            s = rawstr(e.x(0)) + b"\0"
            p = rt.alloc(len(s))
            e.w(p, s)
            return p

        @reg("toupper", "toupper_l")
        def _toupper(e):
            c = e.x(0) & 0xFFFFFFFF
            return c - 32 if 97 <= c <= 122 else c

        @reg("tolower_l")
        def _tolower(e):
            c = e.x(0) & 0xFFFFFFFF
            return c + 32 if 65 <= c <= 90 else c

        for nm, fn in (("isdigit_l", str.isdigit), ("isalnum", str.isalnum), ("islower_l", str.islower),
                       ("isupper_l", str.isupper), ("isxdigit_l", lambda ch: ch in "0123456789abcdefABCDEF")):
            H[nm] = (lambda f: (lambda e: 1 if (e.x(0) < 128 and f(chr(e.x(0) & 0x7F))) else 0))(fn)

        # ---- numbers
        num_re = re.compile(rb"\s*([+-]?(?:inf(?:inity)?|nan|(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?|0[xX][0-9a-fA-F]+))", re.I)

        def parse_float(p):
            s = rawstr(p)
            m = num_re.match(s)
            if not m:
                return 0.0, 0
            txt = m.group(1).decode()
            try:
                v = float.fromhex(txt) if "x" in txt.lower() else float(txt)
            except ValueError:
                v = 0.0
            return v, m.end()

        int_re = re.compile(rb"\s*([+-]?)(0[xX][0-9a-fA-F]+|0[0-7]*|[1-9]\d*)")

        def parse_int(p, base):
            s = rawstr(p)
            if base == 16:
                m = re.match(rb"\s*([+-]?)(?:0[xX])?([0-9a-fA-F]+)", s)
                if not m:
                    return 0, 0
                v = int(m.group(2), 16)
            elif base == 10:
                m = re.match(rb"\s*([+-]?)(\d+)", s)
                if not m:
                    return 0, 0
                v = int(m.group(2))
            else:
                m = int_re.match(s)
                if not m:
                    return 0, 0
                v = int(m.group(2), 0) if m.group(2) not in (b"0",) else 0
            if m.group(1) == b"-":
                v = -v
            return v, m.end()

        @reg("atof")
        def _atof(e):
            rt.ret_f64(parse_float(e.x(0))[0])

        @reg("strtod", "strtod_l")
        def _strtod(e):
            v, n = parse_float(e.x(0))
            if e.x(1):
                e.w(e.x(1), struct.pack("<Q", e.x(0) + n))
            rt.ret_f64(v)

        @reg("strtold", "strtold_l")
        def _strtold(e):
            raise EmuError("strtold (128-bit long double) not supported by the emulator runtime")

        @reg("strtof")
        def _strtof(e):
            v, n = parse_float(e.x(0))
            if e.x(1):
                e.w(e.x(1), struct.pack("<Q", e.x(0) + n))
            rt.ret_f32(v)

        @reg("atoi", "atol")
        def _atoi(e):
            v, n = parse_int(e.x(0), 10)
            return v & 0xFFFFFFFFFFFFFFFF

        @reg("strtol", "strtoll", "strtoul", "strtoull", "strtoll_l", "strtoull_l")
        def _strtol(e):
            v, n = parse_int(e.x(0), e.x(2) if e.x(2) else 0)
            if e.x(1):
                e.w(e.x(1), struct.pack("<Q", e.x(0) + n))
            return v & 0xFFFFFFFFFFFFFFFF

        # ---- scanf family (C semantics: returns number of assignments, EOF(-1) on empty input)
        scan_re = re.compile(r"%(\*)?(\d+)?(hh|h|ll|l|L|z|j|t)?([diouxXaAeEfFgGscpn%]|\[\^?\]?[^\]]*\])")

        def do_scanf(src, fmt, next_ptr):
            s = src
            i = 0
            n_assigned = 0
            fi = 0
            started = False
            while fi < len(fmt):
                ch = fmt[fi]
                if ch.isspace():
                    while i < len(s) and s[i:i + 1].isspace():
                        i += 1
                    fi += 1
                    continue
                if ch != "%":
                    if i < len(s) and s[i] == ord(ch):
                        i += 1
                        fi += 1
                        continue
                    break
                m = scan_re.match(fmt, fi)
                if not m:
                    break
                fi = m.end()
                suppress, width, length, conv = m.groups()
                width = int(width) if width else None
                if conv == "%":
                    while i < len(s) and s[i:i + 1].isspace():
                        i += 1
                    if i < len(s) and s[i] == 37:
                        i += 1
                        continue
                    break
                if conv == "n":
                    if not suppress:
                        e.w(next_ptr(), struct.pack("<i", i))
                    continue
                if conv not in ("c",) and not conv.startswith("["):
                    while i < len(s) and s[i:i + 1].isspace():
                        i += 1
                if i >= len(s):
                    break
                started = True
                end = len(s) if width is None else min(len(s), i + width)
                seg = s[i:end]
                if conv in "diouxXp":
                    base = {"d": 10, "u": 10, "o": 8, "x": 16, "X": 16, "p": 16, "i": 0}[conv]
                    pat = {10: rb"[+-]?\d+", 8: rb"[+-]?[0-7]+", 16: rb"[+-]?(?:0[xX])?[0-9a-fA-F]+",
                           0: rb"[+-]?(?:0[xX][0-9a-fA-F]+|0[0-7]*|[1-9]\d*)"}[base]
                    mm = re.match(pat, seg)
                    if not mm:
                        break
                    txt = mm.group(0)
                    v = int(txt, base) if base else int(txt, 0) if not re.fullmatch(rb"[+-]?0[0-7]+", txt) else int(txt, 8)
                    i += mm.end()
                    if not suppress:
                        size = 8 if length in ("l", "ll", "z", "j", "t") or conv == "p" else (
                            2 if length == "h" else 1 if length == "hh" else 4)
                        e.w(next_ptr(), (v & ((1 << (8 * size)) - 1)).to_bytes(size, "little"))
                        n_assigned += 1
                elif conv in "aAeEfFgG":
                    mm = re.match(rb"[+-]?(?:inf(?:inity)?|nan|(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)", seg, re.I)
                    if not mm:
                        break
                    v = float(mm.group(0))
                    i += mm.end()
                    if not suppress:
                        if length in ("l", "L"):
                            e.w(next_ptr(), struct.pack("<d", v))
                        else:
                            e.w(next_ptr(), struct.pack("<f", to_f32(v)))
                        n_assigned += 1
                elif conv == "s":
                    mm = re.match(rb"\S+", seg)
                    if not mm:
                        break
                    i += mm.end()
                    if not suppress:
                        e.w(next_ptr(), mm.group(0) + b"\0")
                        n_assigned += 1
                elif conv == "c":
                    w = width or 1
                    if len(s) - i < w:
                        break
                    if not suppress:
                        e.w(next_ptr(), s[i:i + w])
                        n_assigned += 1
                    i += w
                else:  # scanset
                    body = conv[1:-1]
                    neg = body.startswith("^")
                    chars = body[1:] if neg else body
                    k = i
                    while k < end and ((chr(s[k]) in chars) != neg):
                        k += 1
                    if k == i:
                        break
                    if not suppress:
                        e.w(next_ptr(), s[i:k] + b"\0")
                        n_assigned += 1
                    i = k
            if not started and n_assigned == 0 and i >= len(s):
                return 0xFFFFFFFF  # EOF
            return n_assigned

        @reg("sscanf")
        def _sscanf(e):
            args = Runtime._RegArgs(rt, 2)
            return do_scanf(rawstr(e.x(0)), e.cstr(e.x(1)), args.int)

        @reg("vsscanf")
        def _vsscanf(e):
            args = Runtime._VaList(rt, e.x(2))
            return do_scanf(rawstr(e.x(0)), e.cstr(e.x(1)), args.int)

        # ---- printf family
        @reg("snprintf")
        def _snprintf(e):
            text = rt.format(e.cstr(e.x(2)), Runtime._RegArgs(rt, 3))
            return rt._write_bounded(e.x(0), e.x(1), text)

        @reg("sprintf")
        def _sprintf(e):
            text = rt.format(e.cstr(e.x(1)), Runtime._RegArgs(rt, 2))
            return rt._write_bounded(e.x(0), 1 << 30, text)

        @reg("vsnprintf")
        def _vsnprintf(e):
            text = rt.format(e.cstr(e.x(2)), Runtime._VaList(rt, e.x(3)))
            return rt._write_bounded(e.x(0), e.x(1), text)

        @reg("__vsnprintf_chk")
        def _vsnprintf_chk(e):
            text = rt.format(e.cstr(e.x(4)), Runtime._VaList(rt, e.x(5)))
            return rt._write_bounded(e.x(0), e.x(1), text)

        @reg("vsprintf")
        def _vsprintf(e):
            text = rt.format(e.cstr(e.x(1)), Runtime._VaList(rt, e.x(2)))
            return rt._write_bounded(e.x(0), 1 << 30, text)

        @reg("__vsprintf_chk")
        def _vsprintf_chk(e):
            text = rt.format(e.cstr(e.x(3)), Runtime._VaList(rt, e.x(4)))
            return rt._write_bounded(e.x(0), 1 << 30, text)

        @reg("vasprintf")
        def _vasprintf(e):
            text = rt.format(e.cstr(e.x(1)), Runtime._VaList(rt, e.x(2))).encode() + b"\0"
            p = rt.alloc(len(text))
            e.w(p, text)
            e.w(e.x(0), struct.pack("<Q", p))
            return len(text) - 1

        def logline(text):
            rt.log.append(text)
            if rt.verbose:
                print("[orig] " + text.rstrip("\n"))

        @reg("printf")
        def _printf(e):
            logline(rt.format(e.cstr(e.x(0)), Runtime._RegArgs(rt, 1)))
            return 0

        @reg("vprintf")
        def _vprintf(e):
            logline(rt.format(e.cstr(e.x(0)), Runtime._VaList(rt, e.x(1))))
            return 0

        @reg("fprintf")
        def _fprintf(e):
            logline(rt.format(e.cstr(e.x(1)), Runtime._RegArgs(rt, 2)))
            return 0

        @reg("vfprintf")
        def _vfprintf(e):
            logline(rt.format(e.cstr(e.x(1)), Runtime._VaList(rt, e.x(2))))
            return 0

        @reg("__android_log_print")
        def _alog(e):
            logline(f"[{e.cstr(e.x(1))}] " + rt.format(e.cstr(e.x(2)), Runtime._RegArgs(rt, 3)))
            return 0

        @reg("__android_log_write")
        def _alogw(e):
            logline(f"[{e.cstr(e.x(1))}] {e.cstr(e.x(2))}")
            return 0

        @reg("puts", "fputs", "perror")
        def _puts(e):
            logline(e.cstr(e.x(0)))
            return 0

        @reg("fputc")
        def _fputc(e):
            return e.x(0) & 0xFF

        @reg("__android_log_assert", "__assert2", "abort", "android_set_abort_message")
        def _abort(e):
            raise EmuError("original code aborted/asserted: " + (e.cstr(e.x(2)) if e.x(2) else ""))

        # ---- libm (float32 results rounded from double)
        def f1(fn):
            return lambda e: rt.ret_f32(fn(rt.sreg(0)))

        def f2(fn):
            return lambda e: rt.ret_f32(fn(rt.sreg(0), rt.sreg(1)))

        def d1(fn):
            return lambda e: rt.ret_f64(fn(rt.dreg(0)))

        def d2(fn):
            return lambda e: rt.ret_f64(fn(rt.dreg(0), rt.dreg(1)))

        def safe(fn):
            def g(*a):
                try:
                    return fn(*a)
                except (ValueError, OverflowError):
                    return math.nan
            return g

        H.update({
            "sinf": f1(math.sin), "cosf": f1(math.cos), "tanf": f1(math.tan), "asinf": f1(safe(math.asin)),
            "acosf": f1(safe(math.acos)), "sqrtf": f1(safe(math.sqrt)), "expf": f1(safe(math.exp)),
            "exp2f": f1(safe(lambda v: 2.0 ** v)), "log10f": f1(safe(math.log10)),
            "atan2f": f2(math.atan2), "powf": f2(safe(math.pow)), "fmodf": f2(safe(math.fmod)),
            "sqrt": d1(safe(math.sqrt)), "exp": d1(safe(math.exp)), "log": d1(safe(math.log)),
            "tan": d1(math.tan), "acos": d1(safe(math.acos)), "atan2": d2(math.atan2), "pow": d2(safe(math.pow)),
        })

        @reg("ldexpf")
        def _ldexpf(e):
            rt.ret_f32(math.ldexp(rt.sreg(0), struct.unpack("<i", struct.pack("<I", e.x(0) & 0xFFFFFFFF))[0]))

        @reg("frexpf")
        def _frexpf(e):
            m, ex = math.frexp(rt.sreg(0))
            e.w(e.x(0), struct.pack("<i", ex))
            rt.ret_f32(m)

        @reg("frexp")
        def _frexp(e):
            m, ex = math.frexp(rt.dreg(0))
            e.w(e.x(0), struct.pack("<i", ex))
            rt.ret_f64(m)

        @reg("modf")
        def _modf(e):
            fr, ip = math.modf(rt.dreg(0))
            e.w(e.x(0), struct.pack("<d", ip))
            rt.ret_f64(fr)

        @reg("sincosf")
        def _sincosf(e):
            v = rt.sreg(0)
            e.w(e.x(0), struct.pack("<f", to_f32(math.sin(v))))
            e.w(e.x(1), struct.pack("<f", to_f32(math.cos(v))))

        @reg("sincos")
        def _sincos(e):
            v = rt.dreg(0)
            e.w(e.x(0), struct.pack("<d", math.sin(v)))
            e.w(e.x(1), struct.pack("<d", math.cos(v)))

        # ---- rand: bionic rand() == BSD random() (TYPE_3, x**31 + x**3 + 1), default seed 1
        def srandom(seed):
            r = [0] * 34
            r[0] = seed & 0xFFFFFFFF or 1
            for i in range(1, 31):
                hi, lo = divmod(r[i - 1] if r[i - 1] < 2**31 else r[i - 1] - 2**32, 127773)
                word = 16807 * lo - 2836 * hi
                if word < 0:
                    word += 2147483647
                r[i] = word
            state = r[:31]
            rt.rand_state = [state, 3, 0]  # fptr, rptr
            for _ in range(310):
                random_next()

        def random_next():
            state, f, r = rt.rand_state
            state[f] = (state[f] + state[r]) & 0xFFFFFFFF
            result = (state[f] >> 1) & 0x7FFFFFFF
            f = (f + 1) % 31
            r = (r + 1) % 31
            rt.rand_state = [state, f, r]
            return result

        @reg("srand")
        def _srand(e):
            srandom(e.x(0) & 0xFFFFFFFF)

        @reg("rand")
        def _rand(e):
            if rt.rand_state is None:
                srandom(1)
            return random_next()

        # ---- time (deterministic)
        @reg("gettimeofday")
        def _gtod(e):
            if e.x(0):
                t = rt.fake_time
                e.w(e.x(0), struct.pack("<qq", int(t), int((t % 1) * 1e6)))
            return 0

        @reg("clock_gettime")
        def _cgt(e):
            t = rt.fake_time
            e.w(e.x(1), struct.pack("<qq", int(t), int((t % 1) * 1e9)))
            return 0

        @reg("time")
        def _time(e):
            if e.x(0):
                e.w(e.x(0), struct.pack("<q", int(rt.fake_time)))
            return int(rt.fake_time)

        @reg("nanosleep", "usleep")
        def _sleep(e):
            return 0

        # ---- errno / env / misc
        @reg("__errno")
        def _errno(e):
            return rt.errno_addr

        @reg("getenv")
        def _getenv(e):
            return 0

        @reg("__system_property_get")
        def _prop(e):
            e.w(e.x(1), b"\0")
            return 0

        @reg("sysconf")
        def _sysconf(e):
            return 4096

        @reg("getpid")
        def _getpid(e):
            return 4242

        @reg("getauxval")
        def _getauxval(e):
            return 0

        # ---- "C" locale wide-char helpers (libc++ locale/ios init)
        @reg("mbtowc")
        def _mbtowc(e):
            if not e.x(1):
                return 0
            c = e.r(e.x(1), 1)[0]
            if e.x(0):
                e.w(e.x(0), struct.pack("<I", c))
            return 1 if c else 0

        @reg("mbrtowc")
        def _mbrtowc(e):
            if not e.x(1):
                return 0
            if e.x(2) == 0:
                return (-2) & 0xFFFFFFFFFFFFFFFF
            c = e.r(e.x(1), 1)[0]
            if e.x(0):
                e.w(e.x(0), struct.pack("<I", c))
            return 1 if c else 0

        @reg("mbrlen")
        def _mbrlen(e):
            if not e.x(0) or e.x(1) == 0:
                return 0
            return 1 if e.r(e.x(0), 1)[0] else 0

        @reg("wcrtomb")
        def _wcrtomb(e):
            if e.x(0):
                e.w(e.x(0), bytes([e.x(1) & 0xFF]))
            return 1

        @reg("btowc")
        def _btowc(e):
            c = e.x(0) & 0xFFFFFFFF
            return 0xFFFFFFFF if c == 0xFFFFFFFF else c & 0xFF

        @reg("wctob")
        def _wctob(e):
            c = e.x(0) & 0xFFFFFFFF
            return c if c < 256 else 0xFFFFFFFF

        @reg("mbsrtowcs", "mbsnrtowcs")
        def _mbsrtowcs(e):
            dst, srcp = e.x(0), e.x(1)
            src = e.u64(srcp)
            s = rt.rawstr(src)
            n = e.x(3) if e.x(0) else len(s)
            if dst:
                k = min(len(s), n)
                e.w(dst, b"".join(struct.pack("<I", c) for c in s[:k]) + (struct.pack("<I", 0) if k == len(s) else b""))
                e.w(srcp, struct.pack("<Q", 0 if k == len(s) else src + k))
                return k
            return len(s)

        @reg("wcsnrtombs")
        def _wcsnrtombs(e):
            dst, srcp, nwc, n = e.x(0), e.x(1), e.x(2), e.x(3)
            src = e.u64(srcp)
            out = bytearray()
            i = 0
            while i < nwc:
                c = e.u32(src + 4 * i)
                if c == 0:
                    break
                out.append(c & 0xFF)
                i += 1
            if dst:
                k = min(len(out), n)
                e.w(dst, bytes(out[:k]))
                e.w(srcp, struct.pack("<Q", src + 4 * k))
                return k
            return len(out)

        @reg("wcslen")
        def _wcslen(e):
            i = 0
            while e.u32(e.x(0) + 4 * i):
                i += 1
            return i

        for nm, test in (("iswalpha_l", str.isalpha), ("iswblank_l", lambda ch: ch in " \t"),
                         ("iswcntrl_l", lambda ch: ord(ch) < 32 or ord(ch) == 127), ("iswdigit_l", str.isdigit),
                         ("iswlower_l", str.islower), ("iswprint_l", lambda ch: 32 <= ord(ch) < 127),
                         ("iswpunct_l", lambda ch: 32 < ord(ch) < 127 and not ch.isalnum()),
                         ("iswspace_l", str.isspace), ("iswupper_l", str.isupper),
                         ("iswxdigit_l", lambda ch: ch in "0123456789abcdefABCDEF")):
            H[nm] = (lambda f: (lambda e: 1 if (e.x(0) < 128 and f(chr(e.x(0) & 0x7F))) else 0))(test)

        @reg("towlower_l")
        def _towlower(e):
            c = e.x(0) & 0xFFFFFFFF
            return c + 32 if 65 <= c <= 90 else c

        @reg("towupper_l")
        def _towupper(e):
            c = e.x(0) & 0xFFFFFFFF
            return c - 32 if 97 <= c <= 122 else c

        @reg("strcoll_l")
        def _strcoll(e):
            a, b = rt.rawstr(e.x(0)), rt.rawstr(e.x(1))
            return (0 if a == b else (-1 if a < b else 1)) & 0xFFFFFFFFFFFFFFFF

        @reg("strxfrm_l")
        def _strxfrm(e):
            s = rt.rawstr(e.x(1))
            if e.x(2) > len(s):
                e.w(e.x(0), s + b"\0")
            return len(s)

        @reg("syscall")
        def _syscall(e):
            nr = e.x(0)
            if nr == 278:  # getrandom(buf, len, flags): deterministic bytes
                buf, n = e.x(1), e.x(2)
                e.w(buf, bytes((i * 131 + 7) & 0xFF for i in range(n)))
                return n
            if nr == 98:   # futex: nothing ever blocks in a single-threaded world
                return 0
            if nr == 178:  # gettid
                return 4242
            raise EmuError(f"unhandled syscall {nr}")

        @reg("setjmp", "_setjmp", "sigsetjmp")
        def _setjmp(e):
            return 0

        @reg("longjmp", "siglongjmp", "_longjmp")
        def _longjmp(e):
            raise EmuError("longjmp (error path in original code, e.g. libpng error)")

        @reg("setlocale", "newlocale", "uselocale", "freelocale", "localeconv")
        def _locale(e):
            return rt.cstr_const("C")

        @reg("__register_atfork", "__cxa_atexit", "__cxa_finalize", "openlog", "syslog", "closelog",
             "fflush", "srand48")
        def _nop0(e):
            return 0

        # ---- stdio (read-only, served from the asset dir; absolute paths are refused)
        @reg("fopen")
        def _fopen(e):
            path = e.cstr(e.x(0))
            mode = e.cstr(e.x(1))
            if "w" in mode or "a" in mode:
                return rt._open_handle(bytearray())
            p = rt._asset_path(path) if not os.path.isabs(path) else (path if os.path.isfile(path) else None)
            if not p:
                return 0
            return rt._open_handle(open(p, "rb").read())

        @reg("fclose")
        def _fclose(e):
            rt.files.pop(e.x(0), None)
            return 0

        @reg("fread")
        def _fread(e):
            buf, sz, n, h = e.x(0), e.x(1), e.x(2), e.x(3)
            f = rt.files.get(h)
            if not f or not sz:
                return 0
            want = sz * n
            data = f[0][f[1]:f[1] + want]
            e.w(buf, bytes(data))
            f[1] += len(data)
            return len(data) // sz

        @reg("fwrite")
        def _fwrite(e):
            return e.x(2)

        @reg("fseek", "fseeko")
        def _fseek(e):
            f = rt.files.get(e.x(0))
            if not f:
                return -1 & 0xFFFFFFFFFFFFFFFF
            off = struct.unpack("<q", struct.pack("<Q", e.x(1)))[0]
            whence = e.x(2)
            f[1] = off if whence == 0 else (f[1] + off if whence == 1 else len(f[0]) + off)
            return 0

        @reg("ftell", "ftello")
        def _ftell(e):
            f = rt.files.get(e.x(0))
            return f[1] if f else -1 & 0xFFFFFFFFFFFFFFFF

        @reg("ferror")
        def _ferror(e):
            return 0

        @reg("getc")
        def _getc(e):
            f = rt.files.get(e.x(0))
            if not f or f[1] >= len(f[0]):
                return 0xFFFFFFFF
            c = f[0][f[1]]
            f[1] += 1
            return c

        @reg("stat", "fstat")
        def _stat(e):
            return -1 & 0xFFFFFFFFFFFFFFFF

        @reg("access")
        def _access(e):
            return -1 & 0xFFFFFFFFFFFFFFFF

        @reg("mkdir", "remove", "rename")
        def _fsmut(e):
            return -1 & 0xFFFFFFFFFFFFFFFF

        # ---- AAssetManager over the player's asset directory
        @reg("AAssetManager_fromJava")
        def _amfj(e):
            return 0xA55E7

        @reg("AAssetManager_open")
        def _amopen(e):
            name = e.cstr(e.x(1))
            p = rt._asset_path(name)
            if rt.verbose:
                print(f"[asset] open {name!r} -> {'OK' if p else 'MISSING'}")
            if not p:
                return 0
            return rt._open_handle(open(p, "rb").read())

        @reg("AAsset_getLength")
        def _alen(e):
            f = rt.files.get(e.x(0))
            return len(f[0]) if f else 0

        @reg("AAsset_read")
        def _aread(e):
            f = rt.files.get(e.x(0))
            if not f:
                return -1 & 0xFFFFFFFFFFFFFFFF
            data = f[0][f[1]:f[1] + e.x(2)]
            e.w(e.x(1), bytes(data))
            f[1] += len(data)
            return len(data)

        @reg("AAsset_close")
        def _aclose(e):
            rt.files.pop(e.x(0), None)

        @reg("AAsset_openFileDescriptor")
        def _afd(e):
            return -1 & 0xFFFFFFFFFFFFFFFF

        @reg("AAssetManager_openDir")
        def _aopendir(e):
            return 0

        # ---- pthreads (single-threaded world)
        for n in ("pthread_mutex_init", "pthread_mutex_destroy", "pthread_mutex_lock", "pthread_mutex_unlock",
                  "pthread_mutex_trylock", "pthread_mutexattr_init", "pthread_mutexattr_destroy",
                  "pthread_mutexattr_settype", "pthread_cond_init", "pthread_cond_destroy", "pthread_cond_signal",
                  "pthread_cond_broadcast", "pthread_rwlock_rdlock", "pthread_rwlock_wrlock", "pthread_rwlock_unlock",
                  "pthread_detach", "pthread_join"):
            H[n] = lambda e: 0

        @reg("pthread_self")
        def _self(e):
            return 0x7E1F

        @reg("pthread_create")
        def _pcreate(e):
            return 11  # EAGAIN: background threads are not emulated

        @reg("pthread_cond_wait", "pthread_cond_timedwait")
        def _cwait(e):
            raise EmuError("pthread_cond_wait in single-threaded emulation (would deadlock)")

        @reg("pthread_key_create")
        def _kc(e):
            k = len(rt.tls_keys) + 1
            rt.tls_keys[k] = 0
            e.w(e.x(0), struct.pack("<I", k))
            return 0

        @reg("pthread_key_delete")
        def _kd(e):
            return 0

        @reg("pthread_getspecific")
        def _gs(e):
            return rt.tls_keys.get(e.x(0) & 0xFFFFFFFF, 0)

        @reg("pthread_setspecific")
        def _ss(e):
            rt.tls_keys[e.x(0) & 0xFFFFFFFF] = e.x(1)
            return 0

        def _once(e):
            ctl, routine = e.x(0), e.x(1)
            if e.u32(ctl) != 0:
                return 0
            e.w(ctl, struct.pack("<I", 1))
            # run routine via trampoline: it returns 0 to the original caller
            e.uc.reg_write(UC_ARM64_REG_X9, routine)
            return ("jump", rt._tramp)

        H["pthread_once"] = _once

        # ---- GLES / EGL: inert, but hand out ids and plausible query results
        def gl_gen(e):
            n, out = e.x(0), e.x(1)
            for i in range(n):
                e.w(out + 4 * i, struct.pack("<I", rt.gl_ids))
                rt.gl_ids += 1

        for n in ("glGenTextures", "glGenBuffers", "glGenFramebuffers", "glGenRenderbuffers"):
            H[n] = gl_gen

        def gl_create(e):
            rt.gl_ids += 1
            return rt.gl_ids

        H["glCreateProgram"] = gl_create
        H["glCreateShader"] = gl_create

        gl_strings = {0x1F00: "OpenWheels", 0x1F01: "Emulated", 0x1F02: "OpenGL ES 2.0", 0x1F03: "",
                      0x8B8C: "OpenGL ES GLSL ES 1.00"}

        @reg("glGetString")
        def _gls(e):
            return rt.cstr_const(gl_strings.get(e.x(0) & 0xFFFFFFFF, ""))

        @reg("glGetIntegerv")
        def _gli(e):
            pname = e.x(0) & 0xFFFFFFFF
            val = {0x0D33: 4096, 0x8869: 16, 0x8872: 16, 0x8B4D: 16, 0x8DFB: 256, 0x8DFD: 64, 0x8DFC: 16,
                   0x84E8: 4096, 0x8CA6: 0, 0x8CA7: 0}.get(pname, 0)
            e.w(e.x(1), struct.pack("<i", val))

        @reg("glGetShaderiv", "glGetProgramiv")
        def _glgiv(e):
            pname = e.x(1) & 0xFFFFFFFF
            val = 1 if pname in (0x8B81, 0x8B82, 0x8B83) else 0  # COMPILE/LINK/VALIDATE status
            e.w(e.x(2), struct.pack("<i", val))

        @reg("glGetAttribLocation", "glGetUniformLocation")
        def _glloc(e):
            rt.gl_ids += 1
            return rt.gl_ids

        @reg("glCheckFramebufferStatus")
        def _glfb(e):
            return 0x8CD5

        @reg("glGetError")
        def _glerr(e):
            return 0

        @reg("glIsBuffer", "glIsRenderbuffer", "glIsEnabled")
        def _glis(e):
            return 0

        @reg("glMapBufferOES")
        def _glmap(e):
            return rt.alloc(1 << 20)

        @reg("eglGetProcAddress", "dlsym")
        def _proc(e):
            return 0

        @reg("glGetBooleanv", "glGetFloatv")
        def _glgetv(e):
            e.w(e.x(1), b"\0" * 16)

        @reg("glGetProgramInfoLog", "glGetShaderInfoLog", "glGetShaderSource")
        def _gllog(e):
            if e.x(3):
                e.w(e.x(3), b"\0")
            if e.x(2):
                e.w(e.x(2), b"\0\0\0\0")

        for s in self.e.elf.syms:
            if s.shndx == 0 and s.name.startswith(("gl", "egl")) and s.name not in H:
                H[s.name] = lambda e: 0

        # ---- OpenSL ES: refuse to create the engine (audio off)
        @reg("slCreateEngine")
        def _sl(e):
            return 1  # SL_RESULT_PRECONDITIONS_VIOLATED

        self.java_vm = self._install_jni()
        self._install_data_imports()

    # ----------------------------------------------------------- imported data objects
    def _install_data_imports(self):
        e = self.e
        d = e.data_imports
        if "_ctype_" in d:
            # bionic: `const char* _ctype_` -> 257-entry table, entry 0 = EOF; flags U L N S P C X B
            U, L, N, S, P, C, X, B = 1, 2, 4, 8, 16, 32, 64, 128
            tab = bytearray(257)
            for c in range(128):
                f = 0
                ch = chr(c)
                if c < 32 or c == 127:
                    f |= C
                if c in (9, 10, 11, 12, 13):
                    f |= S
                if c == 32:
                    f |= S | B
                if ch.isupper():
                    f |= U
                if ch.islower():
                    f |= L
                if ch.isdigit():
                    f |= N
                if ch in "0123456789abcdefABCDEF":
                    f |= X if not ch.isdigit() else 0
                if 33 <= c <= 126 and not ch.isalnum():
                    f |= P
                tab[1 + c] = f
            t = self.alloc(len(tab))
            e.w(t, bytes(tab))
            e.w(d["_ctype_"], struct.pack("<Q", t))
        if "__stack_chk_guard" in d:
            e.w(d["__stack_chk_guard"], struct.pack("<Q", 0x5CA1AB1E5CA1AB1E))
        if "__sF" in d:
            # FILE objects live inside __sF (bionic sizeof(FILE) == 152 on arm64)
            for i, nm in enumerate(("stdin", "stdout", "stderr")):
                if nm in d:
                    e.w(d[nm], struct.pack("<Q", d["__sF"] + 152 * i))
        for nm, a in d.items():
            if nm.startswith("SL_IID_"):
                e.w(a, struct.pack("<Q", a + 0x100))  # SLInterfaceID -> dummy GUID storage

    # ----------------------------------------------------------- fake JavaVM / JNIEnv
    def _install_jni(self):
        """A JavaVM whose JNIEnv answers every call with a neutral default (null/0/"")."""
        e = self.e
        rt = self
        handle = [0x1A000]

        def new_handle(_e):
            handle[0] += 0x10
            return handle[0]

        def zero(_e):
            _e.uc.reg_write(DREGS[0], 0)
            return 0

        empty = self.cstr_const("")
        special = {
            4: lambda _e: 0x10006,                       # GetVersion
            6: new_handle, 21: lambda _e: _e.x(1), 25: lambda _e: _e.x(1),  # FindClass/NewGlobalRef/NewLocalRef
            31: new_handle, 33: new_handle, 94: new_handle, 113: new_handle, 144: new_handle,
            167: new_handle,                             # NewStringUTF
            169: lambda _e: empty,                       # GetStringUTFChars
            219: lambda _e: (_e.w(_e.x(1), struct.pack("<Q", rt.java_vm)), 0)[1],  # GetJavaVM
        }
        env_tab = e.malloc(8 * 240)
        for i in range(240):
            name = f"JNIEnv[{i}]"
            a = e._stub_for(name)
            e.handlers[name] = special.get(i, zero)
            e.w(env_tab + 8 * i, struct.pack("<Q", a))
        env = e.malloc(16)
        e.w(env, struct.pack("<Q", env_tab))

        def get_env(_e):
            _e.w(_e.x(1), struct.pack("<Q", env))
            return 0  # JNI_OK

        vm_tab = e.malloc(8 * 8)
        vm_special = {4: get_env, 6: get_env, 7: get_env}
        for i in range(8):
            name = f"JavaVM[{i}]"
            a = e._stub_for(name)
            e.handlers[name] = vm_special.get(i, zero)
            e.w(vm_tab + 8 * i, struct.pack("<Q", a))
        vm = e.malloc(16)
        e.w(vm, struct.pack("<Q", vm_tab))
        self.jni_env = env
        return vm

