#!/usr/bin/env python3
"""mock_tjf.py - a local stand-in for totaljerkface.com's game endpoints, for testing
OpenWheels' account / replay / publish code without touching the live site.

The protocol is the one derived from the decompiled Flash v1.87 client and the site's login page
(see docs/FLASH_LEVELS.md section 11). Run it, then start the game with
    OW_TJF_BASE=http://127.0.0.1:8765/
so every request goes here instead of https://totaljerkface.com/.

    python tools/online/mock_tjf.py [--port 8765] [--samples binary/flash/samples]
                                    [--identify settings|header|levels|none]

Mock account (fixture values, valid ONLY on this mock - never a real account):
    email  tester@openwheels.test   password  mock-password-1   user id 4242   name MockTester
    (override with --email/--password/--uid/--name)

What it serves:
  GET  user_login.tjf, happy_wheels.tjf, happy-wheels-js/index.tjf (the logged-in user's id/name,
       placed where --identify says: HW_SETTINGS, the header's profile link, or nowhere)
  POST user.hw        login / logout / get_favorites / set_favorite / delete_favorite
  POST get_level.hw   get_all / search_by_name / search_by_user / get_featured / get_level /
                      get_record / get_pub_by_user / get_cmb_by_user   (levels from --samples)
  POST set_level.hw   create / update / publish / rate_level (records are decrypted and checked)
  POST replay.hw      get_all_by_level / get_combined / get_cmb_records / create / rate_replay
                      (create decrypts the AES em/ei fields and checks them)
Replays: <samples>/replays/<id>.cmb.bin + <level>.list.xml (from hwflash.py replays/replay) are
served as the site's replays; uploads are kept in memory.

Every request is printed (passwords redacted). GET /__state returns the mock's state as JSON.
Requires pycryptodome.
"""
from __future__ import annotations

import argparse
import base64
import datetime as dt
import html
import json
import secrets
import struct
import sys
import threading
import urllib.parse
import xml.etree.ElementTree as ET
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

try:
    from Crypto.Cipher import AES, Blowfish
except ImportError:  # pragma: no cover
    sys.exit("mock_tjf: needs pycryptodome (pip install pycryptodome)")

REPO = Path(__file__).resolve().parents[2]
POST_KEY = bytes.fromhex("7ab7657e5595b5c3486988c90728c6ae")  # SaveReplayMenu.APPLESAUCE
IV_LEVEL = b"abcd1234"


# --------------------------------------------------------------------------- crypto

def bf_decrypt_record(data: bytes, author: int) -> bytes:
    key = ("eatshit" + str(author)).encode()
    plain = Blowfish.new(key, Blowfish.MODE_CBC, IV_LEVEL).decrypt(data)
    pad = plain[-1]
    if not 1 <= pad <= 8 or plain[-pad:] != bytes([pad]) * pad:
        raise ValueError("bad padding (wrong author id?)")
    return zlib.decompress(plain[:-pad])


def bf_encrypt_record(xml: bytes, author: int) -> bytes:
    key = ("eatshit" + str(author)).encode()
    z = zlib.compress(xml)
    pad = 8 - len(z) % 8
    return Blowfish.new(key, Blowfish.MODE_CBC, IV_LEVEL).encrypt(z + bytes([pad]) * pad)


def aes_decrypt_post(em_b64: str, ei_hex: str) -> str:
    data = base64.b64decode(em_b64)
    plain = AES.new(POST_KEY, AES.MODE_CBC, bytes.fromhex(ei_hex)).decrypt(data)
    pad = plain[-1]
    if not 1 <= pad <= 16 or plain[-pad:] != bytes([pad]) * pad:
        raise ValueError("bad padding")
    return plain[:-pad].decode("utf-8")


# --------------------------------------------------------------------------- state

class State:
    def __init__(self, args):
        self.args = args
        self.lock = threading.Lock()
        self.sessions: dict[str, dict] = {}          # JSESSIONID -> {"uid": int|None}
        self.failed_logins = 0
        self.favorites: set[int] = set()
        self.level_votes: dict[int, int] = {}
        self.replay_votes: dict[int, int] = {}
        self.created: dict[int, dict] = {}           # level id -> {name, comment, pc, xml, public}
        self.next_level = 90000001
        self.next_replay = 80000001
        self.uploads: dict[int, dict] = {}           # replay id -> {"rp": attrs, "bytes": b}
        self.last_publish: dt.date | None = None
        self.samples = Path(args.samples)
        self.metas: dict[int, ET.Element] = {}
        for f in sorted(self.samples.glob("*.meta.xml")):
            try:
                lv = ET.parse(f).getroot().find("lv")
            except ET.ParseError:
                continue
            if lv is not None:
                self.metas[int(lv.get("id"))] = lv

    def to_json(self):
        return {
            "sessions": {k[:6] + "...": v for k, v in self.sessions.items()},
            "favorites": sorted(self.favorites),
            "level_votes": self.level_votes,
            "replay_votes": self.replay_votes,
            "created": {k: {kk: vv for kk, vv in v.items() if kk != "xml"} | {"xml_len": len(v["xml"])}
                        for k, v in self.created.items()},
            "uploads": {k: v["rp"] | {"bytes": len(v["bytes"])} for k, v in self.uploads.items()},
        }


def lv_xml(lvs: list[ET.Element]) -> bytes:
    root = ET.Element("lvs", pg="1", pp="500")
    for lv in lvs:
        root.append(lv)
    return b'<?xml version="1.0" encoding="UTF-8"?>' + ET.tostring(root)


def created_lv(state: State, level_id: int, info: dict) -> ET.Element:
    lv = ET.Element("lv", id=str(level_id), ln=info["name"], ui=str(state.args.uid), un=state.args.name,
                    rg="0", vs="0", ps="0", dp=info["date"], dc=info["date"], pc=str(info["pc"]))
    uc = ET.SubElement(lv, "uc")
    uc.text = info["comment"]
    return lv


# --------------------------------------------------------------------------- handler

class Handler(BaseHTTPRequestHandler):
    server_version = "mock_tjf/1"
    state: State = None  # set in main

    # -- plumbing
    def log_message(self, fmt, *a):  # quieter default log
        pass

    def session(self) -> tuple[str | None, dict | None]:
        cookie = self.headers.get("Cookie", "")
        for part in cookie.split(";"):
            k, _, v = part.strip().partition("=")
            if k == "JSESSIONID" and v in self.state.sessions:
                return v, self.state.sessions[v]
        return None, None

    def uid(self) -> int | None:
        _, s = self.session()
        return s.get("uid") if s else None

    def reply(self, body: bytes | str, ctype="text/plain; charset=utf-8", cookie: str | None = None):
        if isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        if cookie:
            self.send_header("Set-Cookie", cookie)
        self.end_headers()
        self.wfile.write(body)

    def new_session(self) -> str:
        sid = "mock~" + secrets.token_hex(12).upper()
        self.state.sessions[sid] = {"uid": None}
        return sid

    def show(self, what: str, fields: dict):
        safe = {k: ("<redacted>" if "pass" in k else (v if len(v) < 60 else v[:57] + "...")) for k, v in fields.items()}
        print(f"[mock] {what} {safe}", flush=True)

    # -- GET pages
    def do_GET(self):
        path = urllib.parse.urlparse(self.path).path.lstrip("/")
        st = self.state
        with st.lock:
            if path == "__state":
                return self.reply(json.dumps(st.to_json(), indent=1), "application/json")
            sid, s = self.session()
            cookie = None
            if s is None:
                sid = self.new_session()
                s = st.sessions[sid]
                cookie = f"JSESSIONID={sid}; Path=/; HttpOnly"
            uid = s.get("uid")
            self.show("GET " + path, {"logged_in": str(bool(uid))})
            mode = st.args.identify
            if path == "happy-wheels-js/index.tjf":
                extra = ""
                if uid and mode == "settings":
                    extra = f'"userID": {uid},\n                "userName": "{st.args.name}",'
                page = ('<html><body><script>window["HW_SETTINGS"] = {\n "siteURL": "x",\n "tesselation": "tess2",\n'
                        f'                {extra}\n}};</script></body></html>')
                return self.reply(page, "text/html;charset=utf-8", cookie)
            if path in ("happy_wheels.tjf", "user_login.tjf", ""):
                if uid and mode in ("settings", "header"):
                    login = (f'<div id="login"><a href="profile.tjf?uid={uid}">{html.escape(st.args.name)}</a>'
                             ' <a href="#" onclick="logOutUser()">logout</a></div>')
                elif uid:
                    login = '<div id="login"><a href="#" onclick="logOutUser()">logout</a></div>'
                else:
                    login = '<div id="login"><a href="user_login.tjf"><img src="/img/login_w.png"></a></div>'
                return self.reply(f"<html><body>{login}<div id='body'>mock</div></body></html>",
                                  "text/html;charset=utf-8", cookie)
            self.send_response(404)
            self.end_headers()

    # -- POST endpoints
    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(n).decode("utf-8", "replace")
        fields = {k: v[0] for k, v in urllib.parse.parse_qs(raw, keep_blank_values=True).items()}
        path = urllib.parse.urlparse(self.path).path.lstrip("/")
        action = fields.get("action", "")
        with self.state.lock:
            self.show(f"POST {path} {action}", fields)
            handler = {
                "user.hw": self.user_hw,
                "get_level.hw": self.get_level_hw,
                "set_level.hw": self.set_level_hw,
                "replay.hw": self.replay_hw,
            }.get(path)
            if not handler:
                self.send_response(404)
                self.end_headers()
                return
            handler(action, fields)

    def user_hw(self, action, f):
        st = self.state
        if action == "login":
            sid, s = self.session()
            cookie = None
            if s is None:
                sid = self.new_session()
                s = st.sessions[sid]
                cookie = f"JSESSIONID={sid}; Path=/; HttpOnly"
            if st.failed_logins >= 5:
                return self.reply("lockout:2", cookie=cookie)
            email = f.get("login_user_email", "")
            if email == "unverified@openwheels.test":
                return self.reply("failure:verify_email", cookie=cookie)
            if email == st.args.email and f.get("login_user_pass") == st.args.password:
                st.failed_logins = 0
                # a fresh session id on login, like a servlet container's session fixation guard
                del st.sessions[sid]
                sid = self.new_session()
                st.sessions[sid]["uid"] = st.args.uid
                return self.reply("success:true", cookie=f"JSESSIONID={sid}; Path=/; HttpOnly")
            st.failed_logins += 1
            return self.reply("failure:userpass", cookie=cookie)
        if action == "logout":
            sid, s = self.session()
            if s:
                del st.sessions[sid]
            return self.reply("success:true", cookie="JSESSIONID=; Path=/; Max-Age=0")
        if not self.uid():
            return self.reply("failure:not_logged_in")
        if action == "get_favorites":
            lvs = [st.metas[i] for i in sorted(st.favorites) if i in st.metas]
            lvs += [created_lv(st, i, st.created[i]) for i in sorted(st.favorites) if i in st.created]
            return self.reply(lv_xml(lvs), "text/xml")
        level = int(f.get("level_id", "0") or 0)
        if not level:
            return self.reply("failure:bad_param")
        if action == "set_favorite":
            if level in st.favorites:
                return self.reply("failure:duplicate")
            st.favorites.add(level)
            return self.reply("success")
        if action == "delete_favorite":
            st.favorites.discard(level)
            return self.reply("success")
        return self.reply("failure:invalid_action")

    def get_level_hw(self, action, f):
        st = self.state
        metas = list(st.metas.values())
        if action in ("get_all", "get_featured"):
            return self.reply(lv_xml(metas), "text/xml")
        if action in ("search_by_name", "search_by_user"):
            term = f.get("sterm", "").lower()
            key = "ln" if action == "search_by_name" else "un"
            return self.reply(lv_xml([lv for lv in metas if term in lv.get(key, "").lower()]), "text/xml")
        if action == "get_pub_by_user":
            uid = f.get("user_id", "")
            lvs = [lv for lv in metas if lv.get("ui") == uid]
            if uid == str(st.args.uid):
                lvs += [created_lv(st, i, c) for i, c in st.created.items() if c["public"]]
            return self.reply(lv_xml(lvs), "text/xml")
        if action == "get_level":
            lid = int(f.get("level_id", "0") or 0)
            if lid in st.created:
                return self.reply(lv_xml([created_lv(st, lid, st.created[lid])]), "text/xml")
            return self.reply(lv_xml([st.metas[lid]] if lid in st.metas else []), "text/xml")
        if action == "get_record":
            lid = int(f.get("level_id", "0") or 0)
            if lid in st.created:
                return self.reply(bf_encrypt_record(st.created[lid]["xml"].encode(), st.args.uid),
                                  "application/octet-stream")
            p = st.samples / f"{lid}.record.bin"
            if not p.exists():
                return self.reply("failure:bad_param")
            return self.reply(p.read_bytes(), "application/octet-stream")
        if action == "get_cmb_by_user":
            if not self.uid():
                return self.reply("failure:not_logged_in")
            root = ET.Element("user_levels")
            priv = ET.SubElement(ET.SubElement(root, "private"), "lvs")
            pub = ET.SubElement(ET.SubElement(root, "published"), "lvs")
            for i, c in st.created.items():
                (pub if c["public"] else priv).append(created_lv(st, i, c))
            return self.reply(ET.tostring(root), "text/xml")
        return self.reply("failure:invalid_action")

    def set_level_hw(self, action, f):
        st = self.state
        uid = self.uid()
        if not uid:
            return self.reply("failure:not_logged_in")
        if action == "rate_level":
            r = int(f.get("rating", "-1") or -1)
            lid = int(f.get("level_id", "0") or 0)
            if not 0 <= r <= 5:
                return self.reply("failure:illegal_argument")
            if lid in st.level_votes:
                return self.reply("failure:duplicate_rating")
            st.level_votes[lid] = r
            return self.reply("success")
        if action in ("create", "update"):
            try:
                xml = bf_decrypt_record(base64.b64decode(f.get("level_record", "")), uid)
                root = ET.fromstring(xml)
                assert root.tag == "levelXML" and root.find("info") is not None
            except Exception as e:  # noqa: BLE001
                print(f"[mock]   level_record rejected: {e}", flush=True)
                return self.reply("failure:bad_param")
            name = f.get("level_name", "")
            if not 1 <= len(name) <= 20:
                return self.reply("failure:bad_param")
            info = {"name": name, "comment": f.get("user_comment", ""), "pc": int(f.get("playable_character", "0") or 0),
                    "xml": xml.decode("utf-8"), "public": False, "date": dt.date.today().isoformat()}
            if action == "create":
                lid = st.next_level
                st.next_level += 1
            else:
                lid = int(f.get("level_id", "0") or 0)
                if lid not in st.created:
                    return self.reply("failure:bad_param")
                info["public"] = st.created[lid]["public"]
            st.created[lid] = info
            print(f"[mock]   level {lid} stored: {len(xml)} bytes of XML, v={root.find('info').get('v')}", flush=True)
            return self.reply(f"success:{lid}" if action == "create" else "success")
        if action == "publish":
            lid = int(f.get("level_id", "0") or 0)
            if lid not in st.created:
                return self.reply("failure:bad_param")
            if st.last_publish == dt.date.today() and not st.args.no_publish_limit:
                return self.reply("failure:time_lockout")
            st.created[lid]["public"] = True
            st.last_publish = dt.date.today()
            return self.reply("success")
        return self.reply("failure:invalid_action")

    # replays: site samples + uploads
    def sample_replays(self, level: int) -> list[ET.Element]:
        p = self.state.samples / "replays" / f"{level}.list.xml"
        if not p.exists():
            return []
        try:
            return list(ET.parse(p).getroot().iter("rp"))
        except ET.ParseError:
            return []

    def all_replays(self, level: int) -> list[ET.Element]:
        out = self.sample_replays(level)
        for rid, u in self.state.uploads.items():
            if u["rp"]["li"] == str(level):
                rp = ET.Element("rp", {k: v for k, v in u["rp"].items() if k != "uc"})
                ET.SubElement(rp, "uc").text = u["rp"].get("uc", "")
                out.append(rp)
        return out

    def replay_hw(self, action, f):
        st = self.state
        if action == "get_all_by_level":
            level = int(f.get("level_id", "0") or 0)
            reps = self.all_replays(level)
            sort = f.get("sortby", "newest")
            if sort == "completion_time":
                reps.sort(key=lambda r: int(r.get("ct", "6000")))
            elif sort == "rating":
                reps.sort(key=lambda r: -float(r.get("rg", "0")))
            elif sort == "oldest":
                reps.sort(key=lambda r: r.get("dc", ""))
            else:
                reps.sort(key=lambda r: r.get("dc", ""), reverse=True)
            root = ET.Element("rps", pg="1", pp="500")
            root.extend(reps)
            return self.reply(b'<?xml version="1.0" encoding="UTF-8"?>' + ET.tostring(root), "text/xml")
        if action in ("get_combined", "get_cmb_records"):
            rid = int(f.get("replay_id", "0") or 0)
            if rid in st.uploads:
                u = st.uploads[rid]
                level = int(u["rp"]["li"])
                body = u["bytes"]
            else:
                p = st.samples / "replays" / f"{rid}.cmb.bin"
                if not p.exists():
                    return self.reply("failure:bad_param")
                raw = p.read_bytes()
                n = struct.unpack(">i", raw[:4])[0]
                body = raw[4:4 + n]
                level = int(f.get("level_id", "0") or 0)
                for lvl in st.metas:
                    for rp in self.sample_replays(lvl):
                        if rp.get("id") == str(rid):
                            level = lvl
            if action == "get_combined":
                rp = next((r for r in self.all_replays(level) if r.get("id") == str(rid)), None)
                if rp is None or level not in st.metas:
                    return self.reply("failure:bad_param")
                root = ET.Element("combined_data")
                root.append(rp)
                root.append(st.metas[level])
                return self.reply(ET.tostring(root), "text/xml")
            rec = st.samples / f"{level}.record.bin"
            if not rec.exists():
                return self.reply("failure:bad_param")
            return self.reply(struct.pack(">i", len(body)) + body + rec.read_bytes(), "application/octet-stream")
        uid = self.uid()
        if not uid:
            return self.reply("failure:not_logged_in")
        if action == "rate_replay":
            r = int(f.get("rating", "-1") or -1)
            rid = int(f.get("replay_id", "0") or 0)
            if not 0 <= r <= 5:
                return self.reply("failure:illegal_argument")
            if rid in st.replay_votes:
                return self.reply("failure:duplicate_rating")
            st.replay_votes[rid] = r
            return self.reply("success")
        if action == "create":
            try:
                query = aes_decrypt_post(f["em"], f["ei"])
                q = {k: v[0] for k, v in urllib.parse.parse_qs(query, keep_blank_values=True).items()}
                data = base64.b64decode(f["rr"])
            except Exception as e:  # noqa: BLE001
                print(f"[mock]   replay rejected: {e}", flush=True)
                return self.reply("failure:bad_param")
            print(f"[mock]   replay query: {query}", flush=True)
            if q.get("ui") != str(uid):
                return self.reply("failure:bad_param")
            ct = int(q.get("ct", "0") or 0)
            keys = data.split(b"\xff", 1)[0] if b"\xff" in data[1:] else data
            if ct > 6000:
                return self.reply("failure:hi_comp_time")
            if ct != 6000 and ct != len(keys):
                print(f"[mock]   ct {ct} != {len(keys)} key frames", flush=True)
                return self.reply("failure:bad_param")
            rid = st.next_replay
            st.next_replay += 1
            st.uploads[rid] = {"bytes": data, "rp": {
                "id": str(rid), "li": q.get("id", "0"), "ui": str(uid), "un": st.args.name, "rg": "0", "vs": "0",
                "vw": "0", "dc": dt.date.today().isoformat(), "pc": q.get("pc", "1"), "ct": str(ct),
                "ar": q.get("ar", ""), "vr": q.get("vr", ""), "uc": q.get("uc", "")}}
            return self.reply(f"success:{rid}")
        return self.reply("failure:invalid_action")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--samples", default=str(REPO / "binary" / "flash" / "samples"))
    ap.add_argument("--identify", choices=["settings", "header", "levels", "none"], default="settings",
                    help="where the logged-in user's id shows up (tests TjfAccount's fallbacks)")
    ap.add_argument("--email", default="tester@openwheels.test")
    ap.add_argument("--password", default="mock-password-1")
    ap.add_argument("--uid", type=int, default=4242)
    ap.add_argument("--name", default="MockTester")
    ap.add_argument("--no-publish-limit", action="store_true", help="allow more than one publish per day")
    args = ap.parse_args()
    Handler.state = State(args)
    srv = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"mock_tjf on http://127.0.0.1:{args.port}/ ({len(Handler.state.metas)} sample levels)", flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
