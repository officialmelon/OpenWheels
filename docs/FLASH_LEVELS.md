# Browser (Flash / HTML5) Happy Wheels levels

How OpenWheels loads user levels from the browser game at totaljerkface.com. Status:
**implemented** - `src/online/` (HWApi client + record decryption, FlashLevelConverter, the
in-game browser OnlineLevelBrowser); all 38 test levels convert and load. Section 6 keeps the
original converter plan; section 10 describes what was built on top of it.

In game: the blue globe button on the main menu opens the level browser (search by name or
author, sort, period, featured). `OpenWheels.exe --play-online <level id>` starts a level directly.
Records are downloaded one request at a time (at most one per second), cached under
`<writable path>/online/`, and a download counts as a play only when the player starts the level.
All browser-level behaviour is gated to converted levels, so the campaign is unaffected.

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
| | `get_cmb_by_user` | login | the player's levels: `<…><private><lvs/></private><published><lvs/></published></…>` |
| `user.hw` | `login`, `logout` | `login_user_email`, `login_user_pass` | `success:true` \| `failure:userpass` \| `failure:verify_email` \| `lockout:<min>` (§11.2) |
| | `get_favorites` / `set_favorite`, `delete_favorite` | login; `level_id` | `<lvs>` / `success` \| `failure:duplicate` |
| `set_level.hw` | `create` / `update` / `publish` | login; `level_name`, `user_comment`, `playable_character`, `level_record` (base64 of an encrypted record) / `level_id` | `success:<id>` (create) \| `failure:time_lockout`… (§11.4) |
| | `rate_level` | login; `level_id`, `rating` | `success` \| `failure:duplicate_rating` |
| `replay.hw` | `get_all_by_level`, `get_combined`, `get_cmb_records`, `create`, `rate_replay` | see §11.1 | replays (§11.1) |

Since 2026-10-04 OpenWheels uses all of these (§11); which were exercised live and which only
against the local mock is listed in §11.6.

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
the item code, nothing to convert. (Since 2026-10-08 browser levels step at 1/30 themselves with
the "browser physics" option, see 10.8.)

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

  **OpenWheels extension:** the converter preserves mobile `t="10000"` system triggers for the
  restored campaign. Their `i` attribute is an event ID: 0–4 highlight gameplay controls, 5 clears
  tutorial highlights, and 6–9 are camera events. The original Flash publisher accepts only trigger
  types 1–3, so do not use this extension in levels intended for upload to Totaljerkface.

## 6. Converter plan (original plan; implemented, see section 10)

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
  is committed beyond protocol facts and short identifiers.
* User levels are their authors' work (the ToS grants Fancy Force, not third parties, rights in
  them). Do not redistribute fetched levels; any importer should fetch on the player's behalf at
  play time, at human rates, with `ip_tracking` even unless the player is genuinely playing.
* Unofficial GitHub rehosts of the game ("Happy-Wheels-Source-Code" forks with `get_level.hw`
  files) exist; they were not used.

## 10. Making browser levels behave like the browser game (2026-10-04)

The first converter (sections 5/6) made browser levels *load*; most of them still did not *work*:
items the Android game lacks were dropped or replaced by grey blocks, text boxes and NPCs were
missing, click triggers did nothing, "hide vehicle" starts spawned a vehicle, and Box2D 2.3
differences broke contraptions. Ground truth for everything below is the decompiled Flash v1.87
code (`binary/flash/decomp/.../game/level`, `userspecials`, `editor/specials`, `character`).
A Flash reference player was not available: Ruffle runs the decrypted SWF, but the game loads its
characters and sounds as separate SWFs from swf.totaljerkface.com, which we don't have.

### 10.1 How the PC additions are wired (all marked `// ONLINE (PC addition):`)

* **Gate.** The converter writes `<info ... src="flash" fv="<browser version>">`. `LevelB2D::addInfo`
  calls `online::setFlashLevel()` for every level it loads, so every hook below is off for the
  campaign (`src/online/FlashRuntime.h`); the `LevelB2D` constructor switches it off first, so
  character select's preview level (no `<info>`) never runs the hooks of the level played before. `compare_play` stays 73/73. Editor levels are saved in
  the browser format and played through the converter too (`docs/EDITOR_PORT.md`), so the hooks
  are on for them; the editor marks them `ow="1"`, which makes the converter keep the mobile
  backgrounds (3, 4, 4001), mobile-only special params and the iOS 5001 item.
* **Item ports.** `src/online/items/`: each browser special registers itself
  (`FlashSpecialRegistration`, `FlashSpecials.h`); `LevelB2D::addSpecial` asks the registry for
  ids the Android game lacks (and for overrides: Chain/Token are stubs on Android, Van/Bottle
  inside groups). Ports derive from `online::FlashItem`, which tells `TargetActionSpecial` which
  actions run over time (Flash `_instant`). The converter passes the browser parameters of a
  ported id through unchanged (attributes p0..pN plus string properties such as a text box's
  `<p7>` caption) and only falls back to placeholder boxes for unported ids.
* **Art and fonts** come from the player's own SWF at build time:
  `tools/assets/extract_flash_items.py` renders the symbols listed in
  `tools/assets/flash_items/*.txt` (one manifest per item family; `clip:<id>` addresses nested
  clips) with `FlashPartRender.java`, and exports the text-box fonts, into
  `<exe dir>/generated/flash/` (`index.tsv` holds the registration points). Nothing is committed;
  without the SWF the items draw plain shapes. See 10.6 for the build steps.

### 10.2 Findings and fixes, by how many popular levels they affect

Sample: the 38 levels of section 4 plus 12 of the most played (50 in total, including the top 30
by plays).

| problem (Flash behaviour) | levels | fix |
|---|---|---|
| Text boxes (special 16) not shown | 32/38 | `TextBox` port: embedded Helvetica Neue / Clarendon fonts, colour, size, alignment, rotation, opacity, groups, trigger actions "change opacity" / "slide" (30 Hz frame interpolation as in Flash); levels before 1.69 hide trigger-targeted text until triggered |
| NPC characters (17) dropped (POKEMON TRAINING's Pokemon, 10 WAYS TO DIE...) | 21/38, 9/12 | `NPCharacter` port: 16 skins from the SWF, Flash ragdoll shapes and limits, hold/release pose, injuries, gore, blood, voices, trigger actions, joints, "any character" triggers, lawnmower grinding |
| Click triggers (b=6) did nothing (CLICK PARKOUR, Tic-Tac-Toe, MEME FACE...) | 9/38 (229 triggers) | `Trigger::onlineMouseClick` + `online::installClickTriggers`: Flash mouseUpHandler / roll-out semantics, only the topmost enabled button gets the click, unrotated hit box |
| "Hide vehicle" (info h="t") spawned the vehicle | 13/38 | `online::createBareCharacter`: the character's ragdoll without vehicle, with ejected controls (Flash PlayableCharacterB2D) |
| Trigger semantics (target order, delays, repeat, disable/enable, sensors, shape/group/joint actions, sounds) | most trigger levels | 30 differences fixed, see below |
| Furniture: table, chair, TV, boombox, toilet, trash can, food (1, 19, 21, 22, 24, 26, 32) were grey blocks | 6-7 each | ports with Flash break thresholds, pieces, damage frames, sounds, particles; food is stabbable (materials & 6) and grindable |
| User-built vehicles (`<g v="t">`) didn't drive | 9/38 | `src/online/vehicles/`: grab the handles with space; arrows drive the vehicle's joints (acceleration, lean); space/shift/ctrl actions (jets, arrow guns, brake, lock joints); Z ejects |
| User-vehicle arrow guns ("miniguns") never fired: the mobile ArrowGun only shoots at a target body (material & 2) in range and aligned | vehicle levels | `ArrowGun::onlineSetVehicleControlled` (from `UserVehicle::checkAddSpecial`): the gun keeps its aim relative to its body and fires straight along the barrel every `framesPerShot` frames while the arrow action is held, unlimited arrows, no targeting (so never at the rider). Free-standing guns are unchanged. Arrow guns have no trigger actions in Flash (only the harpoon gun has "trigger firing"), so there is no trigger-fired case |
| Grabbing a user-vehicle handle failed on some parts of a polygon handle | vehicle levels | every fixture of a handle shape is registered (a polygon can be split into several fixtures), not just the first |
| Box2D 2.3 wakes sleeping bodies as soon as their bounding boxes overlap (Box2D 2.0 only on real contact) | many (sleeping shapes are common) | `online::flashPostStep` puts bodies woken that way back to sleep (POKEMON TRAINING's arena bar fell on its mine row at the start and blew up the arena) |
| Box2D 2.3 polygon skin (0.6 px) makes items placed a pixel apart touch | many | polygon radius 0 in browser levels (`flashPreStep`) |
| Groups whose shapes sit far from the group origin: float inertia cancels to <= 0, NaN bodies (even the level body), physics hang | e.g. "string" (37M plays) | converter moves the group body origin onto its shapes (`recenterGroup`); NaN guard restores exploded bodies |
| Chain (30) and Token (31) were Android stubs; glass (18), meteor (11), buildings (13/14), rail (27), cannon (33), paddle (35) missing | 1-6 each | ports (token HUD and "all tokens" victory, glass shards and stabbing, cannon firing sequence...) |
| More than 1600 shapes per layer: decoration dropped | big levels (ROPE SWING 4 lost 644 shapes) | `FFDrawNode`'s delegate table grows (vector) |
| Draw order of levels < 1.8: Flash draws all immovable shapes first, then the others | old levels (POKEMON TRAINING is 1.31) | converter orders shapes that way |
| v <= 1.84: group shapes pick their collision filter by their own "immovable" flag | old levels | converter `fim` attribute, read by `LevelB2D::addShape` |
| v <= 1.84 joints: limits only when enabled, torque 50 / speed 0 without motor | old levels | converter (+ `LevelB2D::addJoint`) |
| Spikes, blades, harpoons and arrows stab materials & 6 (food too) | food levels | gated mask |
| Blade weapons in a group: the mobile port drops the handle's offset, so the handle box sits at the weapon's centre (7 of 12 types miss the drawn handle: the blade hits but the handle can't be grabbed) | sword fights | `BladeWeapon::init` places it like the blade in browser levels (and user / editor test levels) |
| Lawnmower Man (restored character 6) grinds NPCs and food | levels forcing c=6 | `online::Grindable` (Flash grindShape/removeBody), called from `src/restored/LawnMower.cpp` |

Trigger audit (Flash `Trigger`/`TargetAction*` against the mobile code and the converter), fixed in
flash-gated paths of `src/game/triggers/*`, `LevelB2D` and the converter: targets run in document
order; `prepareForTrigger` only for activating triggers; item targets without an action list (fan,
boost, mine, homing mine, wrecking ball) still activate; v >= 1.87 targets with != 1 action do
nothing; editor clamps (size, interval, delay, volume, `sd`); delays/intervals in whole 30 Hz
frames; sound volume; victory while dead; disabling clears pending delays; enabling with a body
inside fires; "each time"/"continuously" re-activation rules; zero-delay trigger chains recurse
(capped at depth 256 instead of Flash's stack overflow); safe iteration while triggers disable each
other; sensor category 24; NPCs count as characters; set fixed / non fixed with Flash's filters,
density, friction and joint removal; delete shape keeps the art; change collision per version (and
collision 7); group fixed/collision only on the group's own shapes; change limits enables limits
(v > 1.84); deleted joints notify their holders; density "NaN" shapes are static. The converter
no longer removes "unsafe" trigger actions: the runtime handles them. Flash's sound list and the
Android `soundlist.tsv` are identical (326 ids, same order), so sound ids need no mapping.

### 10.3 POKEMON TRAINING (562820, v1.31, 105M plays)

Was: no Pokemon (9 NPCs dropped) and no texts (15 text boxes: "I CHOOSE YOU!", "POKEMON GYM"...);
the sleeping red bar of the arena woke on the first step (Box2D 2.3 bounding-box wake), fell 1 px
onto the 16 mines below and the explosion wrecked the arena; the draw order of a 1.31 level was
wrong. Now: NPCs posed and asleep until hit, texts shown, mines intact (headless: 16 mines alive
after 600 frames), Flash draw order.

### 10.4 Characters

Browser characters 6, 7, 8, 10 and 11 map to `restored::hasCharacter(id) ? id : fallback`
(fallbacks: motor cart, motor cart, moped couple, irresponsible dad, segway guy). A "hide vehicle"
start with a restored character uses that character's own ragdoll, voice and gore without vehicle,
kids or elves (Flash `PlayableCharacterB2D`; `restored::createBareCharacter`, `docs/RESTORED.md`).

**Changing character and level** (PC additions; `online::LevelReturn` in `OnlinePlay.h`,
`OnlineLevelBrowser`, `qol/CharacterChoice.h`):

* Levels started from the browser are user levels (`LevelSession` chapter 5001) pushed over the
  browser scene. Leaving them (EXIT, back from character select) comes back to a fresh browser at
  the same list position (`MainMenu::createScene` -> `sceneForReturnFromLevel`, which pops the
  parked browser scene); restarts, VIEW REPLAY and CHANGE CHARACTER keep the way back. Levels
  played from Your Levels come back to Your Levels the same way (`UserLevelsScreen`).
* NEXT on the victory menu is shown only while the level started from the browser still plays and
  the result list has a level after it (`OnlineLevelBrowser::hasNextLevel`; the Android check of
  `Settings` chapter 5000/5001 never saw user levels, so NEXT used to show and just exit). It goes
  back to the browser with the next level selected and starts it like PLAY (download with play
  count, convert; a failed download stays in the browser with its alert). Replays, `--play-online`,
  races and levels received from nearby players have no NEXT.
* Browser levels force their character more often than not (`pc`), and Flash forbids changing it.
  The QoL option "any character" (on by default, user / online levels only; campaign levels and
  ghost races keep their forced character) enables CHANGE CHARACTER in the pause and victory menus
  of such levels and adds CHANGE next to the browser's CHARACTER (character select before
  playing). The level starts with its own character; the picked one replaces it until another level
  is selected (`qol::characterOverride`, also used to watch a replay recorded that way). Restored
  characters are in character select as usual.
* A level that forces a character no longer overwrites the player's own choice for good: it is
  remembered and selected again in character select and when a user level is left; levels that
  let the player pick open character select on the player's character instead of the level's
  default (`LevelSession::applyToSettings`).

### 10.5 Remaining gaps (by levels affected in the sample)

* Fixed since: the 69 sound-table entries without an Android file (Santa, elves, Helicopter Man,
  both kids, BoomboxHit...) are exported from the player's `happy_sounds_v1_72.swf` by
  `tools/assets/extract_flash_sounds.py` into `generated/flash/sounds/` (those the restored
  characters already export in `generated/restored/sounds/` are skipped); the folder is on the
  search path only while a browser level runs, so names resolve exactly as soundlist.tsv lists
  them. The city background (bg 2) is the browser one (`online/FlashCity.*`, art by
  `tools/assets/flash_city.py`: sky, CityBackDrop2 at 0.25 and CityBackDrop1 at 0.5 parallax,
  buildings tiled downwards and drawn twice as in Flash, Flash's blur). Wrecking balls targeted by
  a trigger wait frozen (limits 0, asleep, non-colliding) until triggered (Flash
  prepareForTrigger / triggerSingleActivation).
* Fixed since (2): restored characters in "hide vehicle" starts (10.4). NPC grind states: an NPC's
  joint breaks skip the bleeding (head, neck, stomach, shoulder and hip flows) of a part the mower
  blade is grinding (Flash GRIND_STATE), and the mower masks what it grinds as Flash does
  (`targetMaskHolder`: the art is clipped below the clearance sensor's top edge and disappears
  gradually into the deck; `LawnMower::maskTarget`, a ClippingNode around the part's art) instead
  of hiding the whole part once its centre passes the blade. Levels <= 1.8 keep a finished trigger
  action's counter at its end (Flash `levelVersion > 1.8` resets it), so a repeated fade / motor
  ramp / special action completes at once instead of running again (`TargetAction`,
  `TargetActionGroup`, `TargetActionRevJoint`, `TargetActionSpecial`, the text box's repeat
  handler; prismatic joints already behaved that way). The city backdrops scale with the QoL camera
  zoom (`BackgroundLayer::update`: the backdrops get the camera position at zoom 1 and are drawn at
  the zoom about the screen centre, so they line up with the zoomed level).
* CLICK PARKOUR 3 (10254164): its start sits in a non-fixed density-NaN box (collision 3 = none)
  that trigger 241 switches to collision 7 (character only). In Box2D 2.0 a NaN-density shape is
  static but its centre of mass is NaN, and the contact solver multiplies that into the impulses:
  the "hide vehicle" character goes NaN and is frozen, and Flash's camera, fed a NaN focus, snaps
  to the top-left limits of the stage, where the author built the whole game (title, PLAY and
  language buttons, character choice: everything inside x 0-900, y 0-500). Ours made the box a
  clean static body and the camera followed the character over empty space. Now
  `online::flashPostStep` emulates the NaN island (every awake dynamic body touching a NaN-mass
  body, and everything joined to it by joints or touching contacts, is stopped and deactivated;
  `online::flashNanBody`) and `StageCamera::center` reads such a focus as Flash's stage origin.
  Inferred from the Flash code, not compared with a Flash run. The camera also stops exactly at the
  stage's top and right edges in browser levels, as Flash's `cameraBounds` do: the mobile
  `StageCamera::setLimits` measures the window in pixels against a stage in points, so below the
  large asset tier (a 1600x900 window uses medium) the view could pass those edges by up to half a
  screen.
* HTML5-only special ids > 35: none found. The Flash table (`Settings.specialList`) ends at 35
  (PaddleRef). Ten of the newest levels (all saved by the HTML5 editor, v 2.01, 2026-10) use only
  ids 0-35, with the Flash parameter counts. The HTML5 client's bundle is obfuscated and
  domain-locked, so its table could not be read. Unknown ids degrade gracefully: the converter
  drops them (counted as "unknownSpecial" in its report, like the other dropped items) and
  `LevelB2D::addSpecial` returns nothing for an id no port registers. `tools/levels/hwflash.py`
  now parses listings whose names contain the server's stray Latin-1 bytes.
* Approximations: no Flash reference run was possible; Box2D 2.0 vs 2.3 solver differences remain
  (stacking, joint stiffness); props use the mobile particle systems.

### 10.6 Build and test notes

* Post-build steps (CMakeLists.txt; Gradle `owGenerateFlashItems`, `owGenerateFlashSounds`):
  `extract_flash_items.py` (item art, fonts, city background) and `extract_flash_sounds.py`
  (needs ffmpeg), both skipped quietly when the SWFs, FFDec, Java or ffmpeg are missing.
* Tests: `OpenWheels.exe --convert-flash in.xml out.xml`, then `--dump-world` / `--play-level`.
  Build trees outside the repo need an `assets` junction next to the exe (the exe finds the
  Android assets by walking up from its folder). Debug switches for the physics changes:
  `OW_FLASH_KEEP_POLYGON_RADIUS=1`, `OW_FLASH_KEEP_BOX2D_WAKES=1`.

### 10.7 Shape outlines, outline-only shapes and the fallback fill (2026-10-08)

The mobile game ignores `p9` (outline colour) and `FFDrawNode` never drew a border, so browser
levels lost every outline. Neon-style levels (black background, bright outlines, no fill or a
black fill) showed black shapes or nothing at all.

* **Converter.** A shape with fill `-1` and an outline is kept: `p8="-1"`, its real opacity, and
  it counts as visible, so `enforceDrawBudget` keeps it. A shape with neither fill nor outline
  still gets `p10="0"`. Circles keep `p12` (inner cutout, % of the radius). A physics polygon
  (`t="3"`, interactive) whose drawn outline differs from its physics vertices gets a second
  child `<av n v0…>`: the polygon's own outline, possibly concave and up to 100 vertices, with
  the same integer scale as `<v>`. `<v>` keeps at most 8 vertices for Box2D (their hull). The
  mobile loader reads only the first child, so it never sees `<av>`. An open art path (the thin
  ribbon) gets no outline of its own, and takes the outline colour when it has no fill. The
  editor keeps fill `-1` when it loads and saves a browser level (`RefShape::noFill`) until a
  colour is picked.
* **LevelB2D::addShape** (converted levels only) gives the shape layer a style before it creates
  the shape: `FFDrawNode::onlineSetShapeStyle(fill, outlineWidth, innerCutout)`. The style is
  cleared when `addShape` returns. Polygons are drawn from `<av>`, or from their `<v>` list, but
  never from the fixture's convex hull. That was the case for loose dynamic polygons before.
* **FFDrawNode** draws, in the same delegate record, the fill (skipped for `p8 -1`, a ring for a
  cutout circle), then a centred stroke along the outline in the `p9` colour:
  * Each edge is a quad between mitred corners, with the miter limited to 4 half-widths.
  * Circles get a ring, plus one on the cutout edge.
  * The width is one Flash px (`session ptm / 62.5` = 4 points). The gameplay view is as many
    Flash px tall as the browser stage, so this matches Flash's 1 px line. It is never thinner
    than about 1.25 framebuffer pixels when the window is small or the camera is zoomed out.
  * `updateVerts` sets the alpha of all of a record's triangles from `getArtOpacity()` (shape ×
    group opacity), so outlines fade with their shape, as in Flash.
  * Records with more than 98 triangles keep the rest of their local copy in
    `ArtDelegate::moreTriangles`.
  * Campaign levels never set a style and draw exactly as before.
* **Fallback fill (all levels; `src/game/render/PolyFill.cpp`).** When the original ear clipping
  finds no ear, `FFDrawNode` / `TerrainNode::drawPolyWithVerts` used to return 0 and the shape
  was invisible. This happens with duplicate or collinear vertices or a self-intersecting
  outline. It can also happen with near-collinear corners that classify differently on x86 than
  on the arm64 original: that build fuses multiply-adds (`-ffp-contract=on`), but nobody has
  checked whether it fuses `IsConvex`. Now:
  1. The polygon is repaired: duplicate and collinear points are removed and the winding is made
     counter-clockwise. If the outline is simple, it is ear-clipped again in double precision.
  2. Otherwise it gets an even-odd scanline fill: slabs at every vertex and crossing height, then
     trapezoids between pairs of edges. Flash fills self-intersecting paths even-odd.

  Shapes the original ear clipping handles get exactly the same triangles as before. Because the
  game can now fill any outline, `fitArtRing` in the converter keeps an outline that fits the
  100-vertex limit as it is, even when the ear clipping would reject it.

### 10.8 Browser physics profile: one world step per Flash frame (2026-10-08)

With the QOL option "browser physics (online levels)" (default on since 10.9, online levels only, `src/online/FlashPhysics.*`)
a browser level steps its world once per 30 Hz Flash frame: `Session` time step 1/30
(`LevelItem::s_timeStep` 1/30, `s_timeStepOverFlashTimeStep` 1, `s_timeStepInverse` 30),
10 + 10 iterations, no block solver. Off, or in any campaign level, everything steps at 1/60 as
before (`online::stepsPerFlashFrame()` is 2 there, 1 with the profile). Code that counted world
steps as 1/60 s was audited so both rates behave alike; at 1/60 every change below returns the
original values exactly (the helpers `online::stepsFor60HzFrames`, `stepsForFlashFrames` and
`perStep` return their argument when the step is 1/60):

* **Controls and timer** (`Gameplay::update`): `setState` (vehicle accelerations, lean impulses,
  restored extra controls, user vehicles) and the level timer ran once per scheduler tick, i.e.
  twice per 1/30 step at 60 fps (double impulses, timer at double speed). With the profile they run
  only on ticks that will step (`Session::onlineWillStep`).
* **Flash-frame emulations** that acted every other 1/60 step now act every
  `stepsPerFlashFrame()` steps: prop sprays (`PropItem`), the Jet's thrust (its ramp step is then
  the 1/60 one at both rates), the Sleigh's snow spray, the Helicopter's propeller / magnet art,
  the finish flag, the homing mine light; `PropItem` particles move by one step (was 1/60).
* **Per-60 Hz-step constants of the restored vehicles** go through `online::perStep`: Sleigh
  accelerations, boost fuel step and reins pump, Mine Cart wheel / rail accelerations, Lawn Mower
  acceleration, Helicopter spin acceleration (per step², twice), maximum spin step, rope speed and
  rope gravity (per step²). Their anti-gravity lifts use one step (was 1/60). Step counters
  (Mine Cart one-dongle timeout, Helicopter blade-sound delay) go through `stepsFor60HzFrames`.
* **Mobile items' frame counters** (frame actions run once per world step): the arrow gun's
  cooldown and string animation and the pogo stick's hop lock count two 60 Hz frames per 1/30
  step; mine blink, boost panel and fan blade animations keep their pace.
* **Joint limits** (|reaction force| at 60 Hz): `CharacterB2D::timeStepChanged` halves the
  player's at 1/30, but `LevelB2D::setTimeStep` only reaches the level's own characters; the
  passengers now follow through their owner (moped girl, Irresponsible Dad's kid, Irresponsible
  Mom's kids, Santa's elves), and NPC limits scale with the step at the check.

Left as they are (already rate-aware or correct at both rates): everything driven by
`misc::FlashClock` (Cannon, Chain, Paddle, Token, Glass), seconds-based timers (triggers, target
actions, text boxes, spring box, homing mine, character bleeding, ligaments), lean impulses
(`s_timeStepOverFlashTimeStep`), user vehicles, `restored::FlashParticles` and the mobile emitters
(own 1/60 s clocks), forces applied once per step (fans, boost panels), prop break thresholds
(contact impulses: at 1/30 they are the browser game's own). The mobile vehicles' motor
acceleration steps (`Vehicle::_accelStep`, RoadBike, Moped, Wheelchair...) and Irresponsible
Mom's bike (built as RoadBike) stay per step: the mobile port kept the Flash per-frame values
(RoadBike = Flash BicycleGuy's accelStep 1), so one per 1/30 step is the browser game's rate.
Bodies were drawn when the world stepped, so with the profile they moved at 30 Hz on screen;
10.9 draws every display frame in between.

### 10.9 Box2D 2.0 solver and smooth drawing (2026-10-10)

**Solver.** Box2D is now built from source (`thirdparty/box2d`, the exact source of the engine's
prebuilt) and, while a browser level's world steps with the profile, switched to Box2DFlash
2.0.2's rules (`g_flash20Solver`): velocity clamps, damping, sleep, contact order, the per-point
contact solver and its position correction, revolute and prismatic joints. The list, and what
stays 2.3, is in `thirdparty/box2d/README.md`. Off the profile nothing changes (bit-identical to
upstream source). Written from Box2DFlash 2.0.2's source and checked line by line
against the Box2D in the browser game's decompiled client, which also moved the contact points to
where 2.0 puts them (on the incident polygon / the circle's surface, not midway).

**Smooth drawing** (`online/RenderInterpolation.*`). The world still steps at 30 Hz, but every
display frame paints the level with each body at its pose interpolated between the last two steps
(alpha = time accumulated / step) and the camera (the Session node position) likewise; the
simulated state is put back bit for bit before anything else runs, so the drawing is one step
(1/30 s) behind and nothing the simulation sees changes. Bodies created, destroyed or moved further
than their velocity explains (a respawn, a teleport) are drawn where they are. `paint()` code with
side effects (homing mine flicker, token animation) skips them on these extra paints.

**Checks** (`--online-test dont-move` against `tools/online/mock_tjf.py` with the level from
`tools/online/make_dont_move_sample.py`: a ball rolls into dominoes, the last one falls into the
victory zone; the Wheelchair Guy between two mines must not move): the level finishes at step 326
(10.87 s), the character survives, and watching the run as a replay reaches the finish with every
one of its 327 world states equal bit for bit to the run's. Drawn frames that moved: 668 of 676
with smooth drawing, 327 of 683 without (one per step). The same level on the 1/60 profile:
finishes at step 656, replay exact.


### Replay check (2026-10-08)

The fastest replay of POKEMON TRAINING (562820, Chrepuhon, 34.13 s), watched to its end with
`OW_TJF_WATCH_SECONDS=48 OpenWheels --online-test live-replays`: on the mobile profile (1/60, 8 + 3,
block solver) the rider dies at 7.4 s in the first Thunderbolt battle; with browser physics the run
survives every battle to the end of its keys (still not counted as finished, so replays remain
approximate; 10.9 then moved the solver itself to the Box2D 2.0 rules, which this check has not
been re-run against: the live site is not reachable from the environment that made 10.9).

## 11. Replays, the player's account and publishing (2026-10-04)

All of this is a PC addition (`// ONLINE (PC addition):`), in `src/online/account/` and
`src/online/replays/`, with small hooks in `OnlineLevelBrowser`, `OnlinePlay`, `HWApi`,
`Gameplay::update`, `Session::update` (flash levels only), `Trigger::onlineMouseClick/Move` and
`main.cpp` (`--online-test`). Ground truth: the decompiled Flash v1.87 client (`ReplayData`,
`RecordLoader`, `ReplayLoader`, `SessionReplay`, `CharacterB2D.checkKeyStates/checkReplayData`,
`level/Trigger`, `menus/ReplayBrowser`, `SaveReplayMenu`, `SessionReplayMenu`, `SessionMenu`,
`LevelBrowser`, `editor/SaverLoader`, `editor/LoadMenu`, `utils/LevelEncryptor`, `PostEncryption`,
the preloader) and the site's own login page (`user_login.tjf`, `js/login-*.min.js`).

### 11.1 Replay format and replay.hw

**A browser replay stores inputs only** - no positions, no keyframes, no checkpoints:

* One byte per 30 Hz frame (`Session._iteration`), bits from the most significant:
  left (lean back), right (lean forward), up (accelerate), down, space, shift, ctrl, z (eject).
  User vehicles use the same 8 keys (`userVehicle.operateKeys`).
* Optionally a 0xFF separator and 4-byte big-endian mouse entries for click triggers:
  `uint16 iteration` (+32768 = roll-out instead of click) and `uint16 trigger index` (document
  order). Replay applies them before that frame's keys (`SessionReplay.run` → `mouseClickTrigger`).
  `parseByteArray` splits at the first 0xFF (but not at index 0) - a frame with all eight keys down
  would break a replay; OpenWheels writes 0xFE for it.
* `ct` = frames when the finish was reached, else 6000 (`Settings.maxReplayFrames`, 200 s, the
  upload limit). `ar` = an "architecture" fingerprint (x of a Box2D test body after 30 steps,
  `ArchitectureTest`); the browser marks replays with another `ar` "not 100% accurate" and only
  lets you vote on replays with your own `ar`: even Flash replays only played back exactly on the
  same floating-point setup. HTML5 replays all carry `ar="40922988"`.

| `action` | fields | answer |
|---|---|---|
| `get_all_by_level` | `level_id`, `page`, `sortby` = `newest`\|`oldest`\|`rating`\|`completion_time` | `<rps pg pp><rp id li ui un rg vs vw dc pc ct ar vr><uc/></rp>…</rps>`, 500 per page |
| `get_combined` | `replay_id` | `<combined_data><rp/><lv/></combined_data>` (the browser's `?replay_id=` start) |
| `get_cmb_records` | `replay_id`, `level_id` | int32 BE n, n raw replay bytes, then the level record (§3). Counts a view (`vw` +1, observed) |
| `create` | `rr` = base64(replay bytes), `em`, `ei` (below); login | `success:<replay id>` \| `failure:time_lockout` \| `hi_comp_time` \| `not_logged_in` |
| `rate_replay` | `replay_id`, `rating`; login | `success` \| `failure:duplicate_rating` \| `illegal_argument` |

`em`/`ei` (`SaveReplayMenu` + `PostEncryption`): the query
`id=<level>&pc=<character>&ar=<arch>&ct=<frames>&vr=<version>&uc=<AS3 escape(comment)>&ui=<user id>`,
AES-128-CBC with PKCS#5 under the client's fixed key `7ab7657e5595b5c3486988c90728c6ae` and a
random IV; `em` = base64 of the ciphertext, `ei` = the IV in lowercase hex. There is no "replays
by user" action (`get_all_by_user` returns an empty body); "My Replays" are the runs kept on this
PC. Mean rating = `ReplayDataObject.getAverageRating` (Bayesian prior of 10 votes at 2.5 undone).

**How OpenWheels plays them** (`ReplayRuntime`): OpenWheels' physics are Box2D 2.3, not the
browser's Box2D 2.0, so a replay is re-simulated from its keys and labelled "approximate" in game
(overlay with author, time and progress). With "browser physics" (10.8) the re-simulation runs at
the browser's 30 Hz, one world step and one input byte per Flash frame; with it off each Flash
frame drives two 1/60 s steps (`RunRecord::stepsPerFrame`). The byte is chosen per physics step (`Session::update` → `physicsStep`), not
per display frame as the game's own replay mode does, so a frame without a step doesn't shift the
input. Click/roll-out entries fire their triggers before the frame's steps. The replay's
character is used (browser 6/7/8/10/11 → restored character or the converter's fallback, shown in
the overlay). The player's own runs of online levels are recorded the same way (frame f = the
byte of step f, or 2f on the 1/60 profile; clicks at the next frame boundary), kept for the session (last 6), and can be saved
(`<writable>/online/replays/local/*.owreplay`), watched, uploaded or deleted. Watching one's own
run reproduces it exactly (test: same world position after 600 steps, distance 0).

Uploads say `ar="00000000"` and `vr="1.87"`: browser players see OpenWheels replays as "not 100%
accurate", which is true. The upload form says so, and the upload needs a second confirmation.

### 11.2 Login and session

* The Flash game never logs in. The player logs in on the site; the Java backend keeps the login
  in the servlet session (`JSESSIONID`, HttpOnly, `app0N~…`), and the game's requests carry that
  cookie (`failure:not_logged_in` without it). The page passes the user id/name to the game
  (Flash flashvars `userID`/`userName`; the HTML5 page's `HW_SETTINGS`). The preloader's
  `session` flashvar is **not** a login token: it is the AES key that unlocks the Blowfish key
  of the encrypted game SWF.
* Login (`js/login-*.min.js`): `POST /user.hw` with `login_user_email`, `login_user_pass`,
  `action=login` → `success:true` | `failure:userpass` | `failure:verify_email` |
  `lockout:<minutes>`. The site logs in by **email**, not by user name. Logout: `action=logout`.
* OpenWheels (`TjfAccount`): GET `user_login.tjf` (starts the servlet session), POST the login,
  then finds the user id: `happy-wheels-js/index.tjf` (`HW_SETTINGS` keys `userID`/`userName` and
  variants), else the site header's own profile link `profile.tjf?uid=N`, else the `ui` of the
  player's levels (`get_cmb_by_user`), else the login panel asks for the id. **Unverified on the
  live site** (no login was made): which of these the logged-in pages really contain.
* Stored: the site's cookies (`Set-Cookie`, both curl's one-line-per-cookie and Android's
  comma-joined form), user id, name and the email (form pre-fill) in
  `<writable>/online/tjf_session.txt` - only with "Remember me"; without it only the email is kept
  and the session lives in memory. The password exists only in the login request body (wiped after
  sending, never logged). A `not_logged_in` answer ends the local session. While `OW_TJF_BASE`
  points elsewhere everything goes to `<writable>/online/mock/` instead.

### 11.3 Favorites, ratings, the player's levels

`user.hw get_favorites` (no paging fields, `<lvs>`), `set_favorite`/`delete_favorite`
(`level_id`; `failure:duplicate`), `set_level.hw rate_level` (`level_id`, `rating` 1-5;
`failure:duplicate_rating`), `get_level.hw get_cmb_by_user` (the player's private and published
levels, root children `<private>`/`<published>`, each with `<lvs>`), `get_pub_by_user` (public).
In the browser: a heart and RATE on the detail panel's author line, "Favorites" and "My Levels"
in the sort menu (they ask for a login), the account button in the header (LOG IN / the player's
name → account panel: Favorites, My Levels, My Replays, Publish a Level, Log Out), and a REPLAYS
bar under the stats showing the level's record (fastest finished replay, fetched only after the
selection stays 1.2 s).

### 11.4 Publishing (set_level.hw)

`create`: `level_name`, `user_comment`, `playable_character` (= `c` when `f="t"`, else 0),
`level_record` = base64(Blowfish-CBC("eatshit" + **the logged-in player's id**, IV `abcd1234`,
PKCS#5) over zlib(level XML)) → `success:<new level id>` (a private level). `update`:
`level_id` + the same fields. `publish`: `level_id` (one per day: `failure:time_lockout`).
`del_level`/`del_priv_level` exist (not used). The Flash save menu's rules are kept: name 4-20
characters, comment up to 255, the same character set.

`PublishPanel` only publishes after the confirmation "Publish "X" to totaljerkface.com as
<user>? Everyone will be able to play it." ("Save privately" asks too), and only levels that pass
`checkBrowserLevel`: root `<levelXML>`, `<info v>` 1.0-2.5 with start position and character
1-11, backgrounds 0-2, shape types 0-4, special ids 0-35, joint types 0/1, trigger types 1-3,
only the browser's sections, at most 900 non-art shapes (`Canvas.maxShapes`); converted or
mobile-format levels (`ptm`, `src`, `fm`...) are refused; the editor's `ow="1"` marker is
removed. The client-side bad-words list of the Flash save menu is not reproduced.
**Editor hook:** `online::account::publishLevel(PublishRequest{xml, name, comment})`
(`PublishPanel.h`); the account panel's "Publish a Level" also lists the editor's browser-format
levels (LevelStore chapter 5000) and `*.xml` in `<writable>/online/publish/`.

### 11.5 Tools

* `python tools/levels/hwflash.py replays <level> [--sortby completion_time]` and
  `hwflash.py replay <replay id> <level id>` (read-only; the second counts one view).
* `python tools/online/mock_tjf.py [--port 8765] [--identify settings|header|levels|none]`: the
  protocol above as a local server (fixture account `tester@openwheels.test` /
  `mock-password-1`, user id 4242 - mock only). Serves the levels of `binary/flash/samples` and
  the replays fetched with hwflash.py, keeps logins, favorites, votes, uploads and created levels
  in memory, decrypts and checks every `level_record` and `em`/`ei`, never logs passwords;
  `GET /__state` dumps its state.
* `OW_TJF_BASE=http://127.0.0.1:8765/ OW_TJF_TEST_EMAIL=... OW_TJF_TEST_PASSWORD=...
  OW_TJF_TEST_OUT=<dir> OW_TJF_TEST_LEVEL=<browser level.xml> OpenWheels.exe --online-test tour`:
  drives the whole flow with simulated touches, keys and typing (login, favorite, rate, Favorites
  list, replay list, watch, record a scripted run, save, upload, watch it back and compare,
  publish, My Levels, log out), saves screenshots and quits. It refuses to run unless
  `OW_TJF_BASE` is a local address. `--online-test live-replays` is the read-only part (browse,
  replay list, watch the fastest replay) and may run against the live site.

### 11.6 What was verified where

| path | live (read-only) | mock only |
|---|---|---|
| replay list `get_all_by_level` (`completion_time` live; all four sorts in code) | yes (game + hwflash) | yes |
| `get_combined` | hwflash probe | yes |
| `get_cmb_records`: format; its level record is byte-identical to `get_record` | yes (the game watched the POKEMON TRAINING record) | yes |
| watching a browser replay | yes (approximate; desyncs are expected) | yes |
| recording / saving / watching the player's own runs (exact) | n/a | yes |
| login (`user.hw login`), cookie session, user id detection | **no** | yes (HW_SETTINGS and header detection) |
| logout, favorites, `rate_level`, `rate_replay` | **no** | yes |
| replay `create` (rr / em / ei) | **no** | yes (the mock decrypts and checks the query) |
| `set_level.hw create / update / publish`, `get_cmb_by_user` | **no** | yes (create + publish) |

So the first real login, favorite, vote, replay upload and publish should be done by the player
and checked on the site. Open questions: the exact keys the logged-in `happy-wheels-js/index.tjf`
uses for the user id and name; whether the HTML5 backend still accepts Flash-era replay `create`
fields and `vr="1.87"`; whether `set_level.hw create` is still open to logged-in players or
limited to the HTML5 editor. Live note: a multi-word `search_by_name` returns an empty body
(`HWApi::parseLevelList` now reads that as "no levels").
