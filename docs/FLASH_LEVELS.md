# Browser (Flash / HTML5) Happy Wheels levels

How OpenWheels loads user levels from the browser game at totaljerkface.com. Status:
**implemented** - `src/online/` (HWApi client + record decryption, FlashLevelConverter, the
in-game browser OnlineLevelBrowser); all 38 test levels convert and load. Sections 6 and 9 keep the
original plan and the ToS notes.

Tool: `tools/levels/hwflash.py` (`list`, `fetch`, `decode`, `info`, `schema`; needs
`pip install pycryptodome`). Everything downloaded (clients, decompiler output, sample levels)
lives under `binary/flash/`, which is gitignored with the rest of `binary/`. User levels belong
to their authors: never commit them, never commit decompiled game code.

## 1. Clients (provenance)

| client | where | notes |
|---|---|---|
| **HTML5 "Flashless" client** (current) | `https://totaljerkface.com/happy_wheels.tjf` embeds an iframe `happy-wheels-js/index.tjf`, which reads `happy-wheels-js/version.json` and loads `js/dependencies.<hash>.js` + `js/index.<hash>.js`; art from `happy-wheels-js/assets-<id>/` (Animate JSON/PNG atlases, fonts) | Checked 2026-10-03 (`index` 6f07d140…, `dependencies` da534584…). `dependencies` is a webpack vendor bundle (crypto-js incl. Blowfish, zlib inflate, an AS3 `ByteArray` shim, PIXI). `index` (the game) is wrapped in a self-decoding obfuscation layer with a domain lock (busy-loops unless `location` is `*.totaljerkface.com`); we did **not** deobfuscate or bypass it. Instead the protocol was confirmed by watching the live client's own requests in a normal browser session (§2). The iframe page also accepts `?level_id=` / `?replay_id=` on the parent URL. |
| **Flash client** v1.87 (legacy, secondary source) | Wayback Machine: `swf/game_e_v1_87_g.swf` (capture 2020-08-31) loaded by `swf/hw_preloader_bf.swf` with flashvars `file=…game_e_v1_87_i.swf` (+ a `session` key) | The game SWF is shipped encrypted (Blowfish, key derived from a page flashvar, decrypted by the preloader before `Loader.loadBytes`). Decompiled locally with JPEXS FFDec 26.3.0 (`ffdec-cli.jar -export script`). Relevant classes: `UserLevelLoader`, `LevelLoader`, `menus.LevelBrowser`, `menus.FeaturedMenu`, `editor.SaverLoader`, `RecordLoader`, `utils.LevelEncryptor` (+ `com.hurlant.crypto.symmetric.ButtHoleKey`, which is byte-for-byte hurlant's `BlowFishKey` renamed). `Settings.CURRENT_VERSION = 1.87`. |

No levels are embedded in either client (the SWF has no `DefineBinaryData`; every level, featured
ones included, comes from the server). The Flash build has no campaign.

## 2. API (alive, 2026-10-03)

All calls are `POST https://totaljerkface.com/<endpoint>` with an `application/x-www-form-urlencoded`
body. The HTML5 client sends exactly the same bodies as the Flash client (observed:
`level_id=11101498&action=get_record&ip_tracking=5277765`). Errors come back as plain text
`failure:<reason>` (`invalid_action`, `bad_param`, `app_error`, …) or an HTML page.

| endpoint | `action` | parameters | response |
|---|---|---|---|
| `get_level.hw` | `get_all` | `page` (1-based), `sortby` = `newest`\|`oldest`\|`plays`\|`rating`, `uploaded` = `today`\|`week`\|`month`\|`anytime` | `<lvs pg pp><lv …/>…</lvs>` (`pp` = 500 per page now) |
| | `search_by_name` / `search_by_user` | `sterm` + the `get_all` fields | same |
| | `get_pub_by_user` | `user_id` (+ `get_all` fields) | same |
| | `get_featured` | — | same |
| | `get_level` | `level_id` | `<lvs><lv …/></lvs>` (metadata of one level) |
| | `get_record` | `level_id`, `ip_tracking` | **binary level record** (§3) |
| `user.hw` | `get_favorites`, `set_favorite`, `delete_favorite` | login session | — (not used) |
| `set_level.hw` | `create`, `update`, `publish` | login; `level_record` = base64 of an encrypted record | — (not used) |
| `replay.hw` | `get_all_by_level`, `get_combined`, `get_cmb_records`, `get_cmb_by_user`, `rate_replay` | — | replays; `get_cmb_records` returns a level record encrypted the same way (not used) |

`<lv>` attributes: `id` level id, `ln` name, `ui` **author user id**, `un` author name,
`rg` weighted rating, `vs` votes, `ps` plays, `dp` date published, `pc` forced character (0 = any),
`<uc>` author comment (CDATA).

`ip_tracking` is a random 4–8 digit string (`TextUtils.randomNumString`); its **last digit's
parity is a flag**: odd = count this load as a play, even = don't. `hwflash.py` always sends an
even digit so research fetches don't touch play counts.

## 3. Encoding of a level record

```
record = Blowfish-CBC-encrypt( key = UTF-8("eatshit" + <author user id>),
                               IV  = ASCII "abcd1234",
                               padding = PKCS#5 (8-byte block) )
         of zlib-compress( UTF-8 level XML "<levelXML>…</levelXML>" )
```

Decode: decrypt with the key built from the `<lv ui>` of that level, strip PKCS#5 padding,
`zlib.decompress`, parse XML. The author id is the only per-level secret, and it is public in the
level metadata, which is why `fetch` does `get_level` then `get_record`. (The editor upload path is
the inverse plus base64.) "Encoded in the code" is right in the sense that the key prefix and IV
are constants in the client.

Verified on 8 live levels (2012–2026, editor versions 1.67–2.01): all decrypt with valid padding and
inflate to well-formed XML; `encrypt_record(decrypt_record(x)) == x` round-trips; a wrong author id
fails the padding check.

## 4. Tool usage

```
python tools/levels/hwflash.py list --sortby rating --uploaded anytime --limit 20   # 1 request
python tools/levels/hwflash.py fetch 4464510          # 2 requests, 3 s apart -> binary/flash/samples/
python tools/levels/hwflash.py decode binary/flash/samples/4464510.record.bin [--author UID] [-o x.xml]
python tools/levels/hwflash.py info binary/flash/samples/4464510.xml
python tools/levels/hwflash.py schema binary/flash/samples/*[0-9].xml
python tools/levels/hwflash.py schema binary/HappyWheels_Android/HW_Android/assets/shared/levels
```

`fetch` writes `<id>.meta.xml` (the `<lvs>` metadata), `<id>.record.bin` (raw) and `<id>.xml`
(decoded); `decode` picks the author id from the `.meta.xml` sidecar when `--author` is omitted.
Requests are serial with ≥ 3 s spacing and a normal browser User-Agent; keep volumes tiny.

## 5. Flash/HTML5 schema vs mobile schema

The mobile format **is** the Flash format (same element names, same `p0…pN` positional params,
same type-id numbering), plus a few mobile-only additions. Units match: Flash levels are in
Flash editor pixels, y down, inside a 20000×10000 canvas, 62.5 px/m (`ArchitectureTest`
`m_physScale = 62.5`), rotations in degrees clockwise. Mobile levels store the very same numbers
and declare that with `<info ptm="62.5" sw="20000" sh="10000" r="1" cw="1">`; `LevelB2D` then divides
by `ptm`, flips y against `sh` and negates rotations/joint limits (`cw`). **Without those
attributes `LevelB2D` assumes ptm = 1 and a 320×160 stage**, so a converter must add them.
Flash steps at 1/30 s; mobile at 1/60 (`s_timeStepOverFlashTimeStep` = 0.5) — already handled by
the item code, nothing to convert.

### `<info>`

| attr | Flash/HTML5 | mobile | conversion |
|---|---|---|---|
| `v` | 1.0–1.87 (Flash), 1.9x–2.01 (HTML5) | 1.82–1.86 | keep; see version notes below |
| `rv` | HTML5 runtime version (`1.99.x`) | — | drop |
| `x`,`y` | start position (px) | same | 1:1 |
| `c` | character 1–11 | 1,2,3,4,5,9 | 1:1 if supported, else fallback (§6) |
| `f`,`h` | force character / hide vehicle (`t`/`f`) | same | 1:1 |
| `bg` | 0 blank, 1 green hills, 2 city | 0–4 (+`b0`,`b1`) | 1:1 for 0–2 |
| `bgc` | RGB int | same | 1:1 |
| `e` | present on newer levels | same | 1:1 |
| `ptm`,`sw`,`sh`,`r`,`cw` | — | `62.5`,`20000`,`10000`,`1`,`1` | **add** |

### Shapes `<sh t>` (attrs `i h a p0…p12`, polygons carry a `<v id f n v0…>` vertex list)

| t | Flash (`Settings.shapeList`) | mobile | notes |
|---|---|---|---|
| 0 | RectangleShape | rectangle | 1:1 |
| 1 | CircleShape (`p12` cutout) | circle | 1:1 |
| 2 | TriangleShape | triangle | 1:1 |
| 3 | PolygonShape (`i="f"` ⇒ treated as 4) | polygon | 1:1; vertices `x_y` (v ≥ 1.84) vs `x.y` (older) — normalise to `_` |
| 4 | ArtShape (no physics) | art polygon | 1:1 |
| 5 | — | terrain (mobile-only) | n/a |

Params p0 x, p1 y, p2/p3 size, p4 rotation, p5 immovable, p6 sleeping, p7 density, p8 fill,
p9 outline, p10 opacity, p11 collision, p12 cutout — identical meaning in both.

### Specials `<sp t>` — ids are indices into Flash `Settings.specialList`; mobile `LevelB2D::addSpecial` uses the same ids

| t | Flash item | mobile | | t | Flash item | mobile |
|---|---|---|---|---|---|---|
| 0 | Van | Van | | 18 | Glass | — (ignored) |
| 1 | Table | — (ignored) | | 19 | Chair | — |
| 2 | Mine | Mine | | 20 | Bottle | Bottle |
| 3 | IBeam | IBeam | | 21 | TV | — |
| 4 | Log | Log | | 22 | Boombox | — |
| 5 | SpringBox | SpringBox | | 23 | SignPost | Sign |
| 6 | Spikes | Spikes | | 24 | Toilet | — |
| 7 | WreckingBall | WreckingBall | | 25 | HomingMine | HomingMine |
| 8 | Fan | Fan | | 26 | TrashCan | — |
| 9 | FinishLine | FinishLine | | 27 | Rail | — |
| 10 | SoccerBall | SoccerBall | | 28 | Jet | Jet |
| 11 | Meteor | — | | 29 | ArrowGun | ArrowGun |
| 12 | Boost | BoostPanel | | 30 | Chain | Chain |
| 13 | Building1 | — | | 31 | Token | Token |
| 14 | Building2 | — | | 32 | FoodItem | — |
| 15 | HarpoonGun | HarpoonGun | | 33 | Cannon | — |
| 16 | TextBox | — (present in mobile data, not instantiated) | | 34 | BladeWeapon | BladeWeapon |
| 17 | NPCharacter (p0…p17) | — | | 35 | Paddle | — (falls to `default`) |
| | | | | 5001 | — | SlowMotionPanel (mobile-only) |

Param counts per supported id match between the two corpora (e.g. 15: p0–p7, 34: p0–p6,
23: p0–p4, 29: p0–p5, 28: p0–p7). HTML5-era levels may use ids > 35 — none seen in the sample;
unknown.

### Groups `<g>`, joints `<j>`, triggers `<t>`

* **Groups**: plain groups use the same attributes in both (`x y r o ox oy f fr im s`, nested
  `sh`/`sp`) → 1:1. **Flash-only: user-built vehicles** — a group with `v="t"` is a `RefVehicle`
  with `sb`/`sh`/`ct` (space/shift/ctrl key actions), `a` acceleration, `l` leaning strength,
  `cp` character pose, `lo` lock joints. Mobile `LevelB2D::addGroup` reads none of these (no custom
  vehicles on mobile); such groups would load as inert groups.
* **Joints**: `t` 0 = pin/revolute, 1 = prismatic; `x y b1 b2 l ua la ul ll m tq sp c fo a` are
  shared → 1:1 (rotation/limit sign handled by `cw="1"`). Flash adds `v="t"` for joints driven by
  a user vehicle's controls (ignored by mobile).
* **Triggers**: `t` 1 activate targets, 2 sound (`s` sound id, `v` volume, `l` location, `p` pan),
  3 victory — same in mobile; mobile adds 5000/5001 slow-motion and 10000 system triggers.
  Target children `<sh|sp|g|j|t i=…>` reference items by index in both. **Difference:** Flash
  levels with `v` ≥ 1.87 (all HTML5 levels too) can give one target several actions as child
  elements `<sh i="2"><a i="3" p0="100" p1="2"/></sh>`; older Flash and all mobile levels put a
  single action inline: `<sh i="2" a="3" p0="100" p1="2"/>`.

## 6. Converter plan (not implemented)

1. **Fetch + decode** with `hwflash.py` (or let the player paste exported level XML, which the
   Flash/HTML5 editor's import box also produces — same schema, no crypto involved).
2. **`<info>`**: keep `v x y c f h bg bgc e`; add `ptm="62.5" sw="20000" sh="10000" r="1" cw="1"`;
   drop `rv`. Keep `v` unchanged: `LevelB2D`'s version branches (`> 1.84`, `>= 1.85`, collision
   filters) were inherited from the Flash loader, so old levels keep their old behaviour and
   v 1.9x/2.01 simply take the newest path (check the HTML5 client for behaviour added after 1.87).
3. **Shapes, groups, joints**: copy verbatim; rewrite pre-1.84 polygon vertices `x.y` → `x_y`.
   User vehicles (`<g v="t">` + `<j v="t">`) have no mobile equivalent: either flag the level as
   unsupported, or (bigger job) port Flash `RefVehicle`/user-vehicle control code into OpenWheels
   as an extension outside the 1:1 reconstruction.
4. **Specials**: copy supported ids verbatim. For unsupported ids (1, 11, 13, 14, 16–19, 21, 22,
   24, 26, 27, 32, 33, 35) choose per item: drop (decor: 16 text, 13/14 buildings, 21 TV, 22
   boombox), or substitute a placeholder rectangle with the item's footprint (tables, chairs,
   glass, toilets, trash cans, rails, cannons, meteors, NPCs, food) so level geometry still
   works. **Indices shift when items are dropped** — triggers reference specials by index, so
   either keep a stand-in element in place (e.g. an inert type the mobile loader skips, as it
   already skips 16) or remap every trigger target index.
5. **Triggers**: flatten v ≥ 1.87 multi-action targets into repeated inline target elements
   (one per `<a>`), then verify `LevelB2D`'s trigger loader accepts several entries for the same
   target index. Map sound ids (`s`) to the mobile sound list — not yet checked.
6. **Characters**: 1–5 and 9 exist on mobile; 6 lawnmower man, 7 explorer guy, 8 Santa,
   10 irresponsible mom, 11 helicopter man do not → fall back to the closest vehicle or the
   player's choice and clear `f`.
7. **Test** by loading converted levels through `LevelB2D` (the same path editor/custom levels
   will use), starting with levels that only use supported items.

## 7. Prior art

* BobTrollge/Happy-Wheels-Level-Utility (C#, GitHub): browses/downloads levels via the same
  `get_level.hw` actions and ports `LevelEncryptor.as` (Blowfish-CBC, IV `abcd1234`) — matches our
  findings independently.
* kittenswolf/hwxml (Python): level XML parser. ultrako/LevelXML (C#), mimimishkin/happy-svg
  (Kotlin), YellowDiamond420/Midi2HappyWheels: level XML generators — useful for attribute
  semantics.
* JustProgrammingStuff/happy-wheels-mobile-levelxml: iOS `.happywheels` → levelXML conversions
  (confirms mobile ↔ browser XML kinship); the same author's Steam `record.bin`/`meta.json` →
  levelXML converter is referenced in search results but its repo now 404s.

## 8. Open questions

* Special/shape ids introduced by the HTML5 editor (v 1.9x–2.01) beyond Flash's 0–35, and any new
  attributes — needs the HTML5 client's tables (obfuscated) or a larger sample of new levels.
* Trigger sound ids vs the mobile sound table; `e` attribute semantics.
* Whether the server enforces rate limits (none observed at our volume).

## 9. Legal / ToS

* The site's Terms of Use say the user "may not modify, translate, reverse-engineer,
  reverse-compile or decompile" Fancy Force software. The Flash analysis above decompiled (and
  first decrypted) the archived SWF; it was done for interoperability research and nothing from it
  is committed beyond protocol facts and short identifiers. The coordinator should decide whether
  that is acceptable for the project before building on it.
* User levels are their authors' work (the ToS grants Fancy Force, not third parties, rights in
  them). Do not redistribute fetched levels; any importer should fetch on the player's behalf at
  play time, at human rates, with `ip_tracking` even unless the player is genuinely playing.
* Unofficial GitHub rehosts of the game ("Happy-Wheels-Source-Code" forks with `get_level.hw`
  files) exist; they were not used.
