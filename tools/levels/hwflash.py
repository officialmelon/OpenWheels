#!/usr/bin/env python3
"""hwflash.py - fetch / decode / inspect browser Happy Wheels (totaljerkface.com) user levels.

The browser game (Flash client up to v1.87, and the current HTML5 "Flashless" client, which
speaks the same protocol) talks to totaljerkface.com with form-encoded POSTs. A level's body
("record") comes back as raw binary:

    record = Blowfish-CBC( key = UTF-8("eatshit" + str(author_user_id)),
                           iv  = b"abcd1234", PKCS#5 padding )
             over zlib( UTF-8 level XML <levelXML>...</levelXML> )

The author's user id is the `ui` attribute of the level's metadata (<lv ui=...>), which is why
`fetch` asks for metadata (action=get_level) before the record (action=get_record).
See docs/FLASH_LEVELS.md for the full write-up.

Usage
  python tools/levels/hwflash.py list   [--sortby newest|oldest|plays|rating]
                                        [--uploaded today|week|month|anytime] [--page N]
                                        [--name TERM | --user TERM]
  python tools/levels/hwflash.py fetch  <level_id> [--out DIR]          (2 requests, 3 s apart)
  python tools/levels/hwflash.py decode <record.bin> [--author UID] [-o out.xml]
  python tools/levels/hwflash.py info   <level.xml>
  python tools/levels/hwflash.py schema <level.xml|dir> ...             (tag/attr/type census)

Requires pycryptodome (`pip install pycryptodome`) for Blowfish.

Etiquette: these are the site's public endpoints that the game itself calls. Keep volume tiny,
never parallelise, keep the delay, and do not redistribute downloaded levels - they belong to
their authors. Downloads default to binary/flash/samples/ (gitignored).
"""
from __future__ import annotations

import argparse
import collections
import os
import random
import sys
import time
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

SITE = "https://totaljerkface.com/"
UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/128.0 Safari/537.36")
DELAY_S = 3.0
REPO = Path(__file__).resolve().parents[2]
DEFAULT_OUT = REPO / "binary" / "flash" / "samples"

KEY_PREFIX = "eatshit"     # LevelEncryptor key = KEY_PREFIX + author user id
IV = b"abcd1234"           # LevelEncryptor CBC IV (8 bytes = Blowfish block)

_last_request = 0.0


# --------------------------------------------------------------------------- crypto

def _blowfish():
    try:
        from Crypto.Cipher import Blowfish  # pycryptodome
    except ImportError:  # pragma: no cover
        sys.exit("hwflash: needs pycryptodome for Blowfish (pip install pycryptodome)")
    return Blowfish


def decrypt_record(data: bytes, author_id: int | str) -> bytes:
    """Blowfish-CBC + PKCS#5 decrypt, then zlib-inflate. Returns the UTF-8 XML bytes."""
    # A record is ciphertext, so it may begin with "<" by chance: only an HTML page or a
    # "failure:<reason>" body is a server error.
    head = data[:16].lstrip().lower()
    if data.startswith(b"failure") or head.startswith((b"<html", b"<!doctype", b"<?xml")):
        raise ValueError(f"not a level record (server said: {data[:80]!r})")
    if len(data) % 8:
        raise ValueError(f"record length {len(data)} is not a multiple of the 8-byte block")
    bf = _blowfish()
    key = (KEY_PREFIX + str(int(author_id))).encode("utf-8")
    plain = bf.new(key, bf.MODE_CBC, IV).decrypt(data)
    pad = plain[-1]
    if not 1 <= pad <= 8 or plain[-pad:] != bytes([pad]) * pad:
        raise ValueError("bad PKCS#5 padding - wrong author id?")
    return zlib.decompress(plain[:-pad])


def encrypt_record(xml: bytes, author_id: int | str) -> bytes:
    """Inverse of decrypt_record (what the editor's SaverLoader does before base64)."""
    bf = _blowfish()
    key = (KEY_PREFIX + str(int(author_id))).encode("utf-8")
    z = zlib.compress(xml)
    pad = 8 - len(z) % 8
    return bf.new(key, bf.MODE_CBC, IV).encrypt(z + bytes([pad]) * pad)


# --------------------------------------------------------------------------- network

def ip_tracking() -> str:
    """Mimic TextUtils.randomNumString(4, 8, incrementPlays=False).

    The server treats an odd last digit as "count this as a play"; we always send an even one
    so research fetches do not inflate authors' play counts.
    """
    n = random.randint(4, 8) - 1
    s = str(random.randint(1, 9)) + "".join(str(random.randint(0, 9)) for _ in range(1, n))
    return s + str(random.randint(0, 4) * 2)


def post(endpoint: str, fields: dict) -> bytes:
    global _last_request
    wait = DELAY_S - (time.monotonic() - _last_request)
    if wait > 0:
        time.sleep(wait)
    body = urllib.parse.urlencode(fields).encode()
    req = urllib.request.Request(
        SITE + endpoint, data=body, method="POST",
        headers={"User-Agent": UA,
                 "Content-Type": "application/x-www-form-urlencoded",
                 "Referer": SITE + "happy_wheels.tjf"})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return r.read()
    finally:
        _last_request = time.monotonic()


def check_failure(data: bytes) -> None:
    head = data[:8]
    if b"<html>" in head:
        raise RuntimeError("server returned an HTML page (system_error)")
    if b"failure" in head:
        raise RuntimeError("server: " + data.decode("utf-8", "replace").strip())


def get_meta(level_id: int) -> tuple[bytes, ET.Element | None]:
    raw = post("get_level.hw", {"level_id": level_id, "action": "get_level"})
    check_failure(raw)
    lv = ET.fromstring(raw).find("lv")
    return raw, lv


def get_record(level_id: int) -> bytes:
    raw = post("get_level.hw", {"level_id": level_id, "action": "get_record",
                                "ip_tracking": ip_tracking()})
    check_failure(raw)
    return raw


# --------------------------------------------------------------------------- commands

def cmd_list(a) -> None:
    fields = {"action": "get_all", "page": a.page, "sortby": a.sortby, "uploaded": a.uploaded}
    if a.name:
        fields.update(action="search_by_name", sterm=a.name)
    elif a.user:
        fields.update(action="search_by_user", sterm=a.user)
    raw = post("get_level.hw", fields)
    check_failure(raw)
    if a.save:
        Path(a.save).write_bytes(raw)
    root = ET.fromstring(raw)
    print(f"page {root.get('pg')}  per-page {root.get('pp')}  results {len(root.findall('lv'))}")
    for lv in root.findall("lv")[: a.limit]:
        print(f"{lv.get('id'):>9}  ui={lv.get('ui'):>9}  char={lv.get('pc'):>2}  "
              f"plays={lv.get('ps'):>7}  rating={float(lv.get('rg') or 0):.2f}  "
              f"{lv.get('dp')}  {lv.get('ln')!r} by {lv.get('un')!r}")


def cmd_fetch(a) -> None:
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    meta_raw, lv = get_meta(a.level_id)
    if lv is None:
        sys.exit(f"level {a.level_id}: not found")
    (out / f"{a.level_id}.meta.xml").write_bytes(meta_raw)
    author = lv.get("ui")
    rec = get_record(a.level_id)
    (out / f"{a.level_id}.record.bin").write_bytes(rec)
    xml = decrypt_record(rec, author)
    (out / f"{a.level_id}.xml").write_bytes(xml)
    print(f"{a.level_id}: {lv.get('ln')!r} by {lv.get('un')} (ui={author}) "
          f"record {len(rec)} B -> xml {len(xml)} B -> {out / (str(a.level_id) + '.xml')}")


def _author_from_sidecar(path: Path) -> str | None:
    stem = path.name.split(".")[0]
    meta = path.with_name(stem + ".meta.xml")
    if meta.exists():
        lv = ET.fromstring(meta.read_bytes()).find("lv")
        if lv is not None:
            return lv.get("ui")
    return None


def cmd_decode(a) -> None:
    p = Path(a.record)
    author = a.author or _author_from_sidecar(p)
    if author is None:
        sys.exit("decode: need --author <user id> (the <lv ui=...> of the level), "
                 "or a <id>.meta.xml next to the record")
    xml = decrypt_record(p.read_bytes(), author)
    if a.o:
        Path(a.o).write_bytes(xml)
    else:
        sys.stdout.write(xml.decode("utf-8"))


def _load(path: str) -> ET.Element:
    return ET.fromstring(Path(path).read_bytes())


def cmd_info(a) -> None:
    root = _load(a.xml)
    info = root.find("info")
    print("info:", dict(info.attrib) if info is not None else None)
    for sect, child in (("shapes", "sh"), ("specials", "sp"), ("groups", "g"),
                        ("joints", "j"), ("triggers", "t")):
        el = root.find(sect)
        items = el.findall(child) if el is not None else []
        types = collections.Counter(i.get("t") for i in items)
        print(f"{sect:9} {len(items):5}  types: "
              + ", ".join(f"{k}x{v}" for k, v in sorted(types.items(), key=lambda kv: int(kv[0] or -1))))
    xs, ys = [], []
    for sh in root.iter("sh"):
        try:
            xs.append(float(sh.get("p0"))); ys.append(float(sh.get("p1")))
        except (TypeError, ValueError):
            pass
    if xs:
        print(f"shape centres x [{min(xs):.1f}, {max(xs):.1f}]  y [{min(ys):.1f}, {max(ys):.1f}]")


def _iter_files(paths):
    for p in map(Path, paths):
        if p.is_dir():
            yield from sorted(q for q in p.rglob("*.xml") if not q.name.endswith(".meta.xml"))
        else:
            yield p


def cmd_schema(a) -> None:
    """Census of element/attribute names and per-element type ids across many level files."""
    attrs = collections.defaultdict(collections.Counter)
    types = collections.defaultdict(collections.Counter)
    tparams = collections.defaultdict(set)
    versions = collections.Counter()
    n = 0
    for f in _iter_files(a.paths):
        try:
            root = _load(f)
        except ET.ParseError as e:
            print(f"skip {f}: {e}", file=sys.stderr)
            continue
        n += 1
        info = root.find("info")
        if info is not None:
            versions[info.get("v")] += 1
        for el in root.iter():
            for k in el.attrib:
                attrs[el.tag][k] += 1
            if "t" in el.attrib:
                t = el.get("t")
                types[el.tag][t] += 1
                tparams[(el.tag, t)].update(k for k in el.attrib if k.startswith("p"))
    print(f"{n} files; info v: {dict(versions)}")
    for tag in sorted(attrs):
        print(f"<{tag}> attrs: " + " ".join(sorted(attrs[tag], key=_natkey)))
    for tag in sorted(types):
        print(f"<{tag}> t-ids:")
        for t, c in sorted(types[tag].items(), key=lambda kv: _natkey(kv[0])):
            ps = sorted(tparams[(tag, t)], key=_natkey)
            print(f"   t={t:>4} x{c:<6} params {ps[0] if ps else ''}..{ps[-1] if ps else ''} ({len(ps)})")


def _natkey(s):
    import re
    return [int(x) if x.isdigit() else x for x in re.split(r"(\d+)", str(s))]


def main(argv=None) -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("list", help="one level-browser page (1 request)")
    p.add_argument("--sortby", default="newest", choices=["newest", "oldest", "plays", "rating"])
    p.add_argument("--uploaded", default="week", choices=["today", "week", "month", "anytime"])
    p.add_argument("--page", type=int, default=1)
    p.add_argument("--name"); p.add_argument("--user")
    p.add_argument("--limit", type=int, default=25)
    p.add_argument("--save", help="write the raw <lvs> response here")
    p.set_defaults(fn=cmd_list)

    p = sub.add_parser("fetch", help="metadata + record for one level (2 requests)")
    p.add_argument("level_id", type=int)
    p.add_argument("--out", default=str(DEFAULT_OUT))
    p.set_defaults(fn=cmd_fetch)

    p = sub.add_parser("decode", help="record.bin -> level XML")
    p.add_argument("record"); p.add_argument("--author"); p.add_argument("-o")
    p.set_defaults(fn=cmd_decode)

    p = sub.add_parser("info", help="summary of a level XML")
    p.add_argument("xml"); p.set_defaults(fn=cmd_info)

    p = sub.add_parser("schema", help="tag/attribute/type census over files or dirs")
    p.add_argument("paths", nargs="+"); p.set_defaults(fn=cmd_schema)

    a = ap.parse_args(argv)
    a.fn(a)


if __name__ == "__main__":
    main()
