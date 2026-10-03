"""AArch64 emulation harness for libMyGame.so (Unicorn).

Loads the original library at the Ghidra base (0x100000) so addresses match the
decompilation, applies its dynamic relocations, and runs individual functions.

* Calls to symbols the .so defines itself (cocos2d-x, Box2D, game code, and the template
  instantiations it exports) execute the original code.
* Calls to *imported* symbols (libc, libc++_shared, GLES, JNI, liblog...) land on stubs that
  are implemented in Python below. Unknown imports raise, so gaps are visible.

This is reference tooling: it reads the player's own copy of the game to extract data or to
diff behaviour against the reconstruction. Nothing it produces is committed.
"""
import struct
import sys
import os

from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_UNMAPPED, UcError
from unicorn.arm64_const import (UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_X3,
                                 UC_ARM64_REG_X4, UC_ARM64_REG_X5, UC_ARM64_REG_X6, UC_ARM64_REG_X7,
                                 UC_ARM64_REG_X8, UC_ARM64_REG_SP, UC_ARM64_REG_LR, UC_ARM64_REG_PC,
                                 UC_ARM64_REG_TPIDR_EL0, UC_ARM64_REG_CPACR_EL1, UC_ARM64_REG_S0,
                                 UC_ARM64_REG_S1, UC_ARM64_REG_S2, UC_ARM64_REG_S3)

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfsyms import Elf, GHIDRA_BASE  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_SO = os.path.join(ROOT, "binary", "HappyWheels_Android", "config.arm64_v8a", "lib", "arm64-v8a",
                          "libMyGame.so")

PAGE = 0x1000
STUB_BASE = 0x7000_0000
HEAP_BASE = 0x2000_0000
HEAP_SIZE = 0x1000_0000
STACK_BASE = 0x6000_0000
STACK_SIZE = 0x0010_0000
TLS_BASE = 0x6100_0000
RET_MAGIC = 0x7fff_0000

XREGS = [UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_X3,
         UC_ARM64_REG_X4, UC_ARM64_REG_X5, UC_ARM64_REG_X6, UC_ARM64_REG_X7]


def align_up(v, a):
    return (v + a - 1) & ~(a - 1)


class EmuError(Exception):
    pass


class Emu:
    def __init__(self, so_path=DEFAULT_SO, trace_stubs=False):
        self.elf = Elf(so_path).load()
        self.uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        self.trace_stubs = trace_stubs
        self.heap_ptr = HEAP_BASE
        self.stubs = {}        # stub addr -> symbol name
        self.stub_of = {}      # symbol name -> stub addr
        self.handlers = {}     # symbol name -> python fn(emu) (returns value for x0 or None)
        self.unknown_calls = {}
        self._map_image()
        self._map_runtime()
        self._relocate()
        self._install_default_handlers()
        self.uc.hook_add(UC_HOOK_CODE, self._on_stub, begin=STUB_BASE, end=STUB_BASE + 0x100000)

    # ----------------------------------------------------------------- setup
    def _map_image(self):
        d = self.elf.data
        e_phoff = struct.unpack_from("<Q", d, 0x20)[0]
        e_phentsize, e_phnum = struct.unpack_from("<HH", d, 0x36)
        hi = 0
        loads = []
        for i in range(e_phnum):
            p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(
                "<IIQQQQQQ", d, e_phoff + i * e_phentsize)
            if p_type == 1:  # PT_LOAD
                loads.append((p_offset, p_vaddr, p_filesz, p_memsz))
                hi = max(hi, p_vaddr + p_memsz)
        size = align_up(hi, PAGE) + PAGE
        self.uc.mem_map(GHIDRA_BASE, size)
        for (off, va, fsz, msz) in loads:
            self.uc.mem_write(GHIDRA_BASE + va, d[off:off + fsz])

    def _map_runtime(self):
        self.uc.mem_map(STUB_BASE, 0x100000)
        self.uc.mem_map(HEAP_BASE, HEAP_SIZE)
        self.uc.mem_map(STACK_BASE, STACK_SIZE)
        self.uc.mem_map(TLS_BASE, 0x10000)
        self.uc.mem_map(RET_MAGIC & ~0xfff, PAGE)
        self.uc.mem_write(RET_MAGIC, b"\x1f\x20\x03\xd5")  # nop; we stop on reaching it
        self.uc.reg_write(UC_ARM64_REG_TPIDR_EL0, TLS_BASE + 0x100)
        self.uc.mem_write(TLS_BASE + 0x100 + 0x28, struct.pack("<Q", 0x5ca1ab1e5ca1ab1e))  # stack guard
        # enable FP/SIMD
        self.uc.reg_write(UC_ARM64_REG_CPACR_EL1, 0x300000)

    def _stub_for(self, name):
        if name in self.stub_of:
            return self.stub_of[name]
        a = STUB_BASE + 8 * len(self.stub_of)
        self.stub_of[name] = a
        self.stubs[a] = name
        self.uc.mem_write(a, b"\xc0\x03\x5f\xd6")  # ret (never reached: hook redirects)
        return a

    def _relocate(self):
        # Imported *data* objects (bionic _ctype_, __sF, stderr, OpenSL IIDs...) need real storage,
        # not code stubs; the runtime layer fills their contents.
        self.data_imports = {}
        for s in self.elf.syms:
            if s.shndx == 0 and s.name and s.kind == 1 and s.name not in self.data_imports:
                self.data_imports[s.name] = self.malloc(0x400)
        for off, (rtype, sym, addend) in self.elf.relocs.items():
            where = GHIDRA_BASE + off
            if rtype == 1027:  # RELATIVE
                val = GHIDRA_BASE + addend
            elif rtype in (257, 1025, 1026):  # ABS64, GLOB_DAT, JUMP_SLOT
                if sym is not None and sym.shndx != 0:
                    val = GHIDRA_BASE + sym.value + addend
                elif sym is not None and sym.name in self.data_imports:
                    val = self.data_imports[sym.name] + addend
                else:
                    val = self._stub_for(sym.name if sym is not None else f"<anon@{off:x}>") + addend
            else:
                continue
            self.uc.mem_write(where, struct.pack("<Q", val & 0xFFFFFFFFFFFFFFFF))

    # ----------------------------------------------------------------- memory
    def malloc(self, n, align=16):
        self.heap_ptr = align_up(self.heap_ptr, align)
        p = self.heap_ptr
        self.heap_ptr += max(n, 1)
        if self.heap_ptr > HEAP_BASE + HEAP_SIZE:
            raise EmuError("emulated heap exhausted")
        self.uc.mem_write(p, b"\0" * max(n, 1))
        return p

    def r(self, a, n):
        return bytes(self.uc.mem_read(a, n))

    def w(self, a, b):
        self.uc.mem_write(a, b)

    def u64(self, a):
        return struct.unpack("<Q", self.r(a, 8))[0]

    def u32(self, a):
        return struct.unpack("<I", self.r(a, 4))[0]

    def f32(self, a):
        return struct.unpack("<f", self.r(a, 4))[0]

    def cstr(self, a, limit=1 << 16):
        out = bytearray()
        while len(out) < limit:
            chunk = self.r(a + len(out), 64)
            z = chunk.find(b"\0")
            if z >= 0:
                out += chunk[:z]
                break
            out += chunk
        return out.decode("utf-8", "replace")

    # libc++ (ndk1, arm64) std::string: short = [size<<1][23 chars]; long = [cap|1][size][ptr]
    def std_string(self, a):
        b0 = self.r(a, 1)[0]
        if b0 & 1:
            size = self.u64(a + 8)
            ptr = self.u64(a + 16)
            return self.r(ptr, size).decode("utf-8", "replace")
        size = b0 >> 1
        return self.r(a + 1, size).decode("utf-8", "replace")

    def make_std_string(self, a, s):
        data = s.encode("utf-8")
        if len(data) <= 22:
            self.w(a, bytes([len(data) << 1]) + data + b"\0" * (23 - len(data)))
        else:
            cap = align_up(len(data) + 1, 16)
            p = self.malloc(cap)
            self.w(p, data + b"\0")
            self.w(a, struct.pack("<QQQ", cap | 1, len(data), p))

    def std_vector(self, a, elem_size):
        b, e = self.u64(a), self.u64(a + 8)
        return [b + i * elem_size for i in range((e - b) // elem_size)]

    def std_map_nodes(self, a):
        """In-order traversal of a libc++ __tree (map/set): yields node addresses.

        Tree object: [begin_node][end_node.left (= root)][size]; node: [left][right][parent][is_black].
        """
        root = self.u64(a + 8)
        out = []

        def walk(n):
            while n:
                walk(self.u64(n))  # left
                out.append(n)
                n = self.u64(n + 8)  # right
        walk(root)
        return out

    # ----------------------------------------------------------------- calls
    def sym_addr(self, name_or_demangled):
        for s in self.elf.syms:
            if s.shndx and (s.name == name_or_demangled or s.demangled == name_or_demangled):
                return GHIDRA_BASE + s.value
        raise KeyError(name_or_demangled)

    def call(self, addr, *args, fargs=(), x8=None, max_insns=4_000_000_000):
        sp = STACK_BASE + STACK_SIZE - 0x1000
        self.uc.reg_write(UC_ARM64_REG_SP, sp)
        for i, a in enumerate(args):
            self.uc.reg_write(XREGS[i], a & 0xFFFFFFFFFFFFFFFF)
        for i, f in enumerate(fargs):
            self.uc.reg_write([UC_ARM64_REG_S0, UC_ARM64_REG_S1, UC_ARM64_REG_S2, UC_ARM64_REG_S3][i],
                              struct.unpack("<I", struct.pack("<f", f))[0])
        if x8 is not None:
            self.uc.reg_write(UC_ARM64_REG_X8, x8)
        self.uc.reg_write(UC_ARM64_REG_LR, RET_MAGIC)
        try:
            self.uc.emu_start(addr, RET_MAGIC, count=max_insns)
        except UcError as ex:
            pc = self.uc.reg_read(UC_ARM64_REG_PC)
            raise EmuError(f"emulation fault at pc={pc:#x}: {ex}") from ex
        pc = self.uc.reg_read(UC_ARM64_REG_PC)
        if pc != RET_MAGIC:
            # Unicorn stops silently when the instruction budget runs out; never treat that as a return
            raise EmuError(f"emulation stopped before returning (pc={pc:#x}); instruction budget "
                           f"{max_insns} exhausted or emu_stop called")
        return self.uc.reg_read(UC_ARM64_REG_X0)

    def x(self, i):
        return self.uc.reg_read(XREGS[i])

    # ----------------------------------------------------------------- stubs
    def _on_stub(self, uc, address, size, user):
        name = self.stubs.get(address)
        if name is None:
            return
        h = self.handlers.get(name)
        if h is None:
            self.unknown_calls[name] = self.unknown_calls.get(name, 0) + 1
            uc.emu_stop()
            raise EmuError(f"unimplemented import called: {name}")
        if self.trace_stubs:
            print(f"[stub] {name}", file=sys.stderr)
        ret = h(self)
        if isinstance(ret, tuple) and ret and ret[0] == "jump":
            # tail-transfer into emulated code (e.g. pthread_once trampoline); LR is preserved
            uc.reg_write(UC_ARM64_REG_PC, ret[1])
            return
        if ret is not None:
            uc.reg_write(UC_ARM64_REG_X0, ret & 0xFFFFFFFFFFFFFFFF)
        uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))

    def handle(self, *names):
        def deco(fn):
            for n in names:
                self.handlers[n] = fn
            return fn
        return deco

    def _install_default_handlers(self):
        h = self.handle
        nop = lambda e: None  # noqa: E731

        @h("_Znwm", "_Znam", "malloc")
        def _new(e):
            return e.malloc(e.x(0))

        @h("_ZnwmRKSt9nothrow_t", "_ZnamRKSt9nothrow_t")
        def _new_nt(e):
            return e.malloc(e.x(0))

        @h("calloc")
        def _calloc(e):
            return e.malloc(e.x(0) * e.x(1))

        @h("realloc")
        def _realloc(e):
            old, n = e.x(0), e.x(1)
            p = e.malloc(n)
            if old:
                e.w(p, e.r(old, n))  # over-read is harmless inside the bump heap
            return p

        for n in ("_ZdlPv", "_ZdaPv", "free", "_ZdlPvm", "__cxa_atexit", "__cxa_finalize", "__android_log_print",
                  "__cxa_guard_release", "pthread_mutex_lock", "pthread_mutex_unlock", "__stack_chk_guard"):
            self.handlers[n] = nop

        @h("__cxa_guard_acquire")
        def _guard(e):
            g = e.x(0)
            if e.r(g, 1)[0]:
                return 0
            e.w(g, b"\x01")
            return 1

        @h("__stack_chk_fail")
        def _scf(e):
            raise EmuError("__stack_chk_fail")

        @h("memcpy", "memmove", "__aeabi_memcpy")
        def _memcpy(e):
            d, s, n = e.x(0), e.x(1), e.x(2)
            if n:
                e.w(d, e.r(s, n))
            return d

        @h("memset")
        def _memset(e):
            d, c, n = e.x(0), e.x(1) & 0xFF, e.x(2)
            if n:
                e.w(d, bytes([c]) * n)
            return d

        @h("strlen")
        def _strlen(e):
            return len(e.cstr(e.x(0)).encode("utf-8"))

        @h("memcmp")
        def _memcmp(e):
            a, b, n = e.r(e.x(0), e.x(2)), e.r(e.x(1), e.x(2)), e.x(2)
            return 0 if a == b else (-1 if a < b else 1) & 0xFFFFFFFFFFFFFFFF

        # ---- libc++_shared std::string members (ndk1 mangling)
        S = "NSt6__ndk112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE"

        def assign_cstr(e):
            e.make_std_string(e.x(0), e.cstr(e.x(1)))
            return e.x(0)
        self.handlers[f"_ZN{S[1:-1]}E6assignEPKc"] = assign_cstr
        self.handlers[f"_ZN{S[1:-1]}EaSEPKc"] = assign_cstr

        def assign_ptr_len(e):
            e.make_std_string(e.x(0), e.r(e.x(1), e.x(2)).decode("utf-8", "replace"))
            return e.x(0)
        self.handlers[f"_ZN{S[1:-1]}E6assignEPKcm"] = assign_ptr_len
        self.handlers[f"_ZN{S[1:-1]}E6__initEPKcm"] = assign_ptr_len

        def copy_ctor(e):
            e.make_std_string(e.x(0), e.std_string(e.x(1)))
            return e.x(0)
        self.handlers[f"_ZN{S[1:-1]}EC1ERKS5_"] = copy_ctor
        self.handlers[f"_ZN{S[1:-1]}EC2ERKS5_"] = copy_ctor

        def assign_str(e):
            e.make_std_string(e.x(0), e.std_string(e.x(1)))
            return e.x(0)
        self.handlers[f"_ZN{S[1:-1]}EaSERKS5_"] = assign_str
        self.handlers[f"_ZN{S[1:-1]}E6assignERKS5_"] = assign_str

        def append_cstr(e):
            e.make_std_string(e.x(0), e.std_string(e.x(0)) + e.cstr(e.x(1)))
            return e.x(0)
        self.handlers[f"_ZN{S[1:-1]}E6appendEPKc"] = append_cstr

        def append_ptr_len(e):
            e.make_std_string(e.x(0), e.std_string(e.x(0)) + e.r(e.x(1), e.x(2)).decode("utf-8", "replace"))
            return e.x(0)
        self.handlers[f"_ZN{S[1:-1]}E6appendEPKcm"] = append_ptr_len

        def dtor(e):
            return None
        self.handlers[f"_ZN{S[1:-1]}ED1Ev"] = dtor
        self.handlers[f"_ZN{S[1:-1]}ED2Ev"] = dtor
