"""Build the OpenWheels reversal index from the re-agent Ghidra export.

Inputs
  * libMyGame.so (Android 1.1.3, arm64)            -> symbols, RTTI, vtables
  * re-agent ghidra-bridge per-function JSON export -> decompiled C, call graph

Outputs (all under reports/decomp/, which is gitignored: decompiler output is
derived from the original binary and must never be committed)
  * functions.csv       every function in the game-code range
  * classes.json        RTTI hierarchy, vtables, sizes, method lists per class
  * classes/<Class>.c   decompiled bodies grouped by owning class, address order
"""
import csv
import json
import os
import re
import struct
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(__file__))
from elfsyms import Elf, GHIDRA_BASE  # noqa: E402
from names import class_and_method, qualified_name, _split_depth0  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SO = os.path.join(ROOT, "binary", "HappyWheels_Android", "config.arm64_v8a", "lib", "arm64-v8a",
                  "libMyGame.so")
# Folder of per-function Ghidra JSON exports of libMyGame.so (<gaddr>.json).
EXPORTS = os.environ.get("OW_EXPORTS", "")
OUT = os.path.join(ROOT, "reports", "decomp")

# Game code is linked first: [start of .text, cocos_android_app_init]. Everything after is
# cocos2d-x / third-party / JNI glue.
GAME_END_SYMBOL = "cocos_android_app_init"

LIB_TOPS = {"std", "cocos2d", "b2", "rapidxml", "tinyxml2", "sdkbox", "firebase", "_JNIEnv",
            "patch", "__cxxabiv1", "CocosDenshion", "cocostudio", "flatbuffers", "spine", "ClipperLib"}


def is_library_top(top):
    return (top in LIB_TOPS or top.startswith("b2") or top.startswith("std")
            or top.startswith("Java_") or top.startswith("__"))


def main():
    if not EXPORTS or not os.path.isdir(EXPORTS):
        sys.exit("index_exports.py: set OW_EXPORTS to the folder of Ghidra JSON exports of libMyGame.so")
    os.makedirs(os.path.join(OUT, "classes"), exist_ok=True)
    elf = Elf(SO).load()
    text = elf.sections[".text"]
    game_end = next(s.value for s in elf.syms
                    if s.kind == 2 and s.demangled.startswith(GAME_END_SYMBOL))
    game_lo, game_hi = text.addr, game_end
    print(f"game range {game_lo:#x}-{game_hi:#x}")

    # ---------------------------------------------------------------- functions
    funcs = {}  # vaddr -> record
    for s in elf.syms:
        if s.kind == 2 and s.shndx and s.value:
            r = funcs.setdefault(s.value, {"vaddr": s.value, "size": s.size, "names": []})
            r["names"].append((s.name, s.demangled))
            r["size"] = max(r["size"], s.size)

    # Pull decompilation for everything in the game range from the export.
    print("loading export ...")
    exported = {}
    for fn in os.listdir(EXPORTS):
        if not fn.endswith(".json") or not re.fullmatch(r"[0-9a-fA-F]+", fn[:-5]):
            continue
        gaddr = int(fn[:-5], 16)
        va = gaddr - GHIDRA_BASE
        if not (game_lo <= va < game_hi):
            continue
        with open(os.path.join(EXPORTS, fn), encoding="utf-8") as f:
            d = json.load(f)
        exported[va] = {
            "ghidra_name": d.get("name"),
            "signature": d.get("signature"),
            "decompiled": d.get("decompiled") or "",
            "callers": d.get("callers") or [],
            "callees": d.get("callees") or [],
            "data_refs": d.get("data_refs") or [],
            "insns": len(d.get("assembly") or []),
        }
    print(f"exported functions in game range: {len(exported)}")

    # Merge: every exported function in range becomes a record, named or not.
    for va, ex in exported.items():
        r = funcs.setdefault(va, {"vaddr": va, "size": 0, "names": []})
        r["export"] = ex

    game = {va: r for va, r in funcs.items() if game_lo <= va < game_hi}

    # Owner assignment. Named: by demangled qualifier. Unnamed: nearest preceding named
    # game function in address order (same translation unit -> same class file).
    order = sorted(game)
    last_owner = None
    for va in order:
        r = game[va]
        owner = None
        kind = "unnamed"
        if r["names"]:
            # prefer the non-thunk demangled name
            mangled, dem = sorted(r["names"], key=lambda n: ("thunk" in n[1], len(n[1])))[0]
            prefix, cls, meth, params = class_and_method(dem)
            top = _split_depth0(qualified_name(dem)[1], "::")[0]
            r["mangled"], r["demangled"] = mangled, dem
            r["class"], r["method"], r["prefix"] = cls, meth, prefix
            if is_library_top(top):
                kind = "library"
                owner = last_owner  # inline-template instance emitted in this TU
            else:
                kind = "game"
                owner = cls if cls else "_free"
                last_owner = owner
        else:
            owner = last_owner
            r["mangled"] = r["demangled"] = ""
            r["class"], r["method"], r["prefix"] = "", (r.get("export") or {}).get("ghidra_name", ""), ""
        r["kind"] = kind
        r["owner"] = owner or "_preamble"

    # ---------------------------------------------------------------- RTTI
    classes = defaultdict(lambda: {"bases": [], "vtable": [], "size": None, "methods": [],
                                   "statics": [], "typeinfo_kind": None})
    ti_kind = {}
    for s in elf.syms:
        if s.name.startswith("_ZTVN10__cxxabiv1"):
            ti_kind[s.value + 16] = s.name  # typeinfo vptr points at vtable+16
    for s in elf.syms:
        if not (s.name.startswith("_ZTI") and s.shndx):
            continue
        cname = s.demangled.replace("typeinfo for ", "")
        vptr, vsym = elf.pointer_at(s.value)
        kindname = vsym.name if vsym is not None else ti_kind.get(vptr, "?")
        bases = []
        if "__si_class_type_info" in kindname:
            b, bsym = elf.pointer_at(s.value + 16)
            bname = bsym.demangled.replace("typeinfo for ", "") if bsym is not None else (
                elf.name_of(b).demangled.replace("typeinfo for ", "") if b and elf.name_of(b) else "?")
            bases.append({"name": bname, "offset": 0, "flags": 2})
        elif "__vmi_class_type_info" in kindname:
            raw = elf.read(s.value + 16, 8)
            flags, nbase = struct.unpack("<II", raw)
            for i in range(nbase):
                b, bsym = elf.pointer_at(s.value + 24 + i * 16)
                oflags = struct.unpack("<q", elf.read(s.value + 32 + i * 16, 8))[0]
                bname = bsym.demangled.replace("typeinfo for ", "") if bsym is not None else (
                    elf.name_of(b).demangled.replace("typeinfo for ", "") if b and elf.name_of(b) else "?")
                bases.append({"name": bname, "offset": oflags >> 8, "flags": oflags & 0xFF})
        if cname.startswith("cocos2d::") or cname.startswith("std::"):
            continue
        c = classes[cname]
        c["bases"] = bases
        c["typeinfo_kind"] = kindname.replace("_ZTVN10__cxxabiv1", "")

    # vtables
    for s in elf.syms:
        if not (s.name.startswith("_ZTV") and s.shndx) or s.name.startswith("_ZTVN10__cxxabiv1"):
            continue
        cname = s.demangled.replace("vtable for ", "")
        if cname.startswith("cocos2d::") or cname.startswith("std::"):
            continue
        n = s.size // 8
        slots = []
        for i in range(n):
            tgt, tsym = elf.pointer_at(s.value + i * 8)
            if tsym is not None and tsym.shndx == 0:
                nm = tsym.demangled or tsym.name
            elif tgt:
                ns = elf.name_of(tgt)
                nm = ns.demangled if ns else f"sub_{tgt + GHIDRA_BASE:08x}"
            else:
                raw = elf.read(s.value + i * 8, 8)
                v = struct.unpack("<q", raw)[0] if raw else 0
                nm = f"<offset {v}>" if i % 1 == 0 else "0"
            slots.append(nm)
        classes[cname]["vtable"] = slots

    # sizes: find 'operator new(0x...)' in create()/factory bodies of each class
    # cocos2d-style `new (std::nothrow) X()` decompiles to operator_new(N, nothrow) + memset(p,0,N);
    # the first one inside X::create*/X::scene is the X instance itself.
    new_re = re.compile(r"operator_new\((0x[0-9a-fA-F]+|\d+),\s*\(nothrow_t")
    plain_new_re = re.compile(r"(\w+)\s*=\s*\(?\w*\s*\*?\)?\s*operator_new\((0x[0-9a-fA-F]+|\d+)\);\s*\n\s*(?:memset\(\1,0,(?:0x[0-9a-fA-F]+|\d+)\);\s*\n\s*)?(\w+)::\3\(")
    for va in order:
        r = game[va]
        ex = r.get("export")
        if not ex or r["kind"] != "game" or not r["class"]:
            continue
        meth = r["method"]
        if not (meth.startswith("create") or meth in ("node", "scene", "getInstance", "sharedInstance")
                or meth.startswith("shared")):
            continue
        m = new_re.findall(ex["decompiled"])
        if m and classes[r["class"]]["size"] is None:
            classes[r["class"]]["size"] = int(m[0], 0)
            classes[r["class"]]["size_from"] = r["demangled"]
    # plain `new X` (singletons etc.): operator_new(N) immediately followed by X::X(p)
    for va in order:
        ex = game[va].get("export")
        if not ex:
            continue
        for var, sz, cls in plain_new_re.findall(ex["decompiled"]):
            if cls in classes and classes[cls]["size"] is None:
                classes[cls]["size"] = int(sz, 0)
                classes[cls]["size_from"] = game[va]["demangled"] or f"@{va + GHIDRA_BASE:08x}"

    # static data members / globals owned by game classes
    for s in elf.syms:
        if s.kind == 1 and s.shndx and s.name.startswith("_Z") and not s.name.startswith(("_ZTV", "_ZTI", "_ZTS", "_ZGV")):
            prefix, cls, meth, params = class_and_method(s.demangled)
            top = cls.split("::")[0] if cls else meth
            if cls and not is_library_top(top):
                classes[cls]["statics"].append({"name": meth, "vaddr": s.value, "size": s.size})

    for va in order:
        r = game[va]
        if r["kind"] == "game" and r["class"]:
            classes[r["class"]]["methods"].append(r["demangled"])

    # ---------------------------------------------------------------- write
    with open(os.path.join(OUT, "functions.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["ghidra_addr", "vaddr", "size", "insns", "kind", "owner", "class", "method",
                    "demangled", "mangled"])
        for va in order:
            r = game[va]
            ex = r.get("export") or {}
            w.writerow([f"{va + GHIDRA_BASE:08x}", f"{va:08x}", r["size"], ex.get("insns", ""), r["kind"],
                        r["owner"], r["class"], r["method"], r["demangled"], r["mangled"]])

    with open(os.path.join(OUT, "classes.json"), "w", encoding="utf-8") as f:
        json.dump(classes, f, indent=1, sort_keys=True)

    by_owner = defaultdict(list)
    for va in order:
        by_owner[game[va]["owner"]].append(game[va])
    for owner, recs in by_owner.items():
        safe = re.sub(r"[^A-Za-z0-9_]+", "_", owner)
        with open(os.path.join(OUT, "classes", safe + ".c"), "w", encoding="utf-8") as f:
            f.write(f"// Decompilation of translation unit owning class '{owner}' (libMyGame.so 1.1.3 arm64)\n")
            f.write("// GENERATED for reference only - derived from the original binary; never commit.\n\n")
            for r in recs:
                ex = r.get("export") or {}
                f.write("// " + "=" * 96 + "\n")
                f.write(f"// @{r['vaddr'] + GHIDRA_BASE:08x} (vaddr {r['vaddr']:08x}) size={r['size']} "
                        f"insns={ex.get('insns', '?')} kind={r['kind']}\n")
                if r["demangled"]:
                    f.write(f"// {r['demangled']}\n// {r['mangled']}\n")
                callees = ex.get("callees") or []
                if callees:
                    names = []
                    for c in callees[:60]:
                        names.append(c if isinstance(c, str) else (c.get("name") or str(c)))
                    f.write("// callees: " + ", ".join(names) + "\n")
                body = (ex.get("decompiled") or "// <no decompilation in export>\n").replace("\r\n", "\n").strip("\n")
                f.write(body + "\n\n")

    # quick stats
    kinds = defaultdict(int)
    for r in game.values():
        kinds[r["kind"]] += 1
    print("function kinds:", dict(kinds))
    print("owners:", len(by_owner), "classes with RTTI/vtables:", len(classes))


if __name__ == "__main__":
    main()
