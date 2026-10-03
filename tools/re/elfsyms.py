"""Minimal ELF64 (AArch64) reader for libMyGame.so.

Gives dynamic symbols (with demangled names), dynamic relocations and raw
section bytes, which is everything needed to rebuild vtables and the RTTI
class hierarchy without a Ghidra round-trip.
"""
import struct
import subprocess
from dataclasses import dataclass, field

GHIDRA_BASE = 0x100000  # Ghidra loads the .so at 0x100000; file vaddrs start at 0


@dataclass
class Sym:
    name: str
    value: int
    size: int
    kind: int  # STT_*
    bind: int  # STB_*
    shndx: int
    demangled: str = ""


@dataclass
class Section:
    name: str
    type: int
    addr: int
    offset: int
    size: int
    entsize: int
    link: int


@dataclass
class Elf:
    path: str
    data: bytes = b""
    sections: dict = field(default_factory=dict)
    syms: list = field(default_factory=list)
    by_addr: dict = field(default_factory=dict)  # vaddr -> [Sym]
    by_name: dict = field(default_factory=dict)  # mangled -> Sym
    # r_offset -> (type, sym or None, addend)
    relocs: dict = field(default_factory=dict)

    # ---------------------------------------------------------------- loading
    def load(self):
        with open(self.path, "rb") as f:
            self.data = f.read()
        d = self.data
        e_shoff = struct.unpack_from("<Q", d, 0x28)[0]
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", d, 0x3A)
        raw = []
        for i in range(e_shnum):
            off = e_shoff + i * e_shentsize
            (sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size,
             sh_link, sh_info, sh_addralign, sh_entsize) = struct.unpack_from(
                "<IIQQQQIIQQ", d, off)
            raw.append((sh_name, sh_type, sh_addr, sh_offset, sh_size, sh_entsize, sh_link))
        shstr = raw[e_shstrndx]
        for (nm, ty, addr, off, size, ent, link) in raw:
            name = self._cstr(shstr[3] + nm)
            self.sections[name] = Section(name, ty, addr, off, size, ent, link)
        self._load_dynsym()
        self._load_relocs(".rela.dyn")
        self._load_relocs(".rela.plt")
        return self

    def _cstr(self, off):
        end = self.data.index(b"\0", off)
        return self.data[off:end].decode("utf-8", "replace")

    def _load_dynsym(self):
        ds = self.sections[".dynsym"]
        dstr = self.sections[".dynstr"]
        n = ds.size // 24
        for i in range(n):
            st_name, st_info, st_other, st_shndx, st_value, st_size = struct.unpack_from(
                "<IBBHQQ", self.data, ds.offset + i * 24)
            name = self._cstr(dstr.offset + st_name) if st_name else ""
            self.syms.append(Sym(name, st_value, st_size, st_info & 0xF, st_info >> 4, st_shndx))
        names = [s.name for s in self.syms]
        dem = demangle(names)
        for s, dn in zip(self.syms, dem):
            s.demangled = dn
            if s.name:
                self.by_name.setdefault(s.name, s)
            if s.shndx != 0 and s.name:
                self.by_addr.setdefault(s.value, []).append(s)

    def _load_relocs(self, secname):
        sec = self.sections.get(secname)
        if not sec:
            return
        n = sec.size // 24
        for i in range(n):
            r_offset, r_info, r_addend = struct.unpack_from("<QQq", self.data, sec.offset + i * 24)
            rtype = r_info & 0xFFFFFFFF
            symi = r_info >> 32
            sym = self.syms[symi] if symi else None
            self.relocs[r_offset] = (rtype, sym, r_addend)

    # ---------------------------------------------------------------- access
    def vaddr_to_off(self, vaddr):
        for s in self.sections.values():
            if s.type != 8 and s.addr and s.addr <= vaddr < s.addr + s.size:  # skip NOBITS
                return s.offset + (vaddr - s.addr)
        return None

    def read(self, vaddr, n):
        off = self.vaddr_to_off(vaddr)
        if off is None:
            return None
        return self.data[off:off + n]

    def pointer_at(self, vaddr):
        """Resolve the 8-byte pointer stored at vaddr (applying dynamic relocations).

        Returns (target_vaddr or None, symbol or None).
        """
        rel = self.relocs.get(vaddr)
        if rel:
            rtype, sym, addend = rel
            if rtype == 1027:  # R_AARCH64_RELATIVE
                return addend, None
            if rtype in (257, 1025, 1026):  # ABS64, GLOB_DAT, JUMP_SLOT
                if sym is not None and sym.shndx != 0:
                    return sym.value + addend, sym
                return None, sym
            return None, sym
        raw = self.read(vaddr, 8)
        if raw is None:
            return None, None
        v = struct.unpack("<Q", raw)[0]
        return (v if v else None), None

    def name_of(self, vaddr):
        syms = self.by_addr.get(vaddr)
        if not syms:
            return None
        # prefer a function symbol with the most specific (longest) name
        syms = sorted(syms, key=lambda s: (s.kind != 2, -len(s.demangled)))
        return syms[0]


def demangle(names):
    """Demangle many names in one llvm-cxxfilt call (order preserving)."""
    payload = "\n".join(n if n else "_" for n in names) + "\n"
    out = subprocess.run(["llvm-cxxfilt"], input=payload, capture_output=True, text=True,
                         encoding="utf-8").stdout.split("\n")
    out = out[:len(names)]
    return [o if n else "" for n, o in zip(names, out)]
