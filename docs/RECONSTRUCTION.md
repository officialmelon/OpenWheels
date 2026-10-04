# OpenWheels reconstruction handbook

OpenWheels rebuilds the Happy Wheels mobile game code (Android 1.1.3, `libMyGame.so`, arm64,
cocos2d-x 3.17.2 + Box2D) as readable C++ that compiles against the *same* engine version on
Windows and Android. The target is behavioural 1:1 parity: same constants, same control flow,
same order of operations, same float math. Not "inspired by" — *the same program*, re-expressed
as source.

Status: every game class of `libMyGame.so` is reconstructed in `src/game/` (the module split
and per-module notes are in `docs/MODULES.md`). All 73 campaign levels load into the same Box2D
world as the original (`docs/PARITY.md`). These rules apply to all changes in `src/game/`; code that is
not part of the 1:1 reconstruction lives elsewhere and is marked (section 8).

## 1. Ground rules

1. **Fidelity over taste.** Translate what the binary does, not what you think it should do.
   Keep weird code weird (odd constants, redundant checks, dead branches, bugs). Never
   "simplify" physics, ordering, or math. If the original creates fixtures in order A,B,C you
   create them A,B,C (Box2D contact/solver order depends on it).
2. **Every function.** Each named function in your classes, and every unnamed helper/lambda
   (`FUN_xxxxxxxx`) that lives in your translation unit, must be reconstructed. Inline copies
   of the same function (duplicates at other addresses) are written once.
3. **Exact numbers.** Float constants must have identical bits. Ghidra usually prints a
   round-trippable decimal; when a value is from `DAT_xxxxxxxx`, read it with
   `owre.py data <addr> --as f32` and write it with enough digits (9 significant) + `f`.
   Preserve float vs double: if the decompilation converts to `double` (`(double)x`, `fcvt`),
   the original did double math there (a literal without `f`, or `cos()/sqrt()` on double) —
   reproduce that so rounding matches. Plain float ops stay float.
4. **Original names.** Class names, method names, parameter types and `const`-ness are exactly
   the demangled symbol (`owre.py methods <Class>`). Enums/types that appear in signatures
   (e.g. `SessionMode`) are declared with those names.
5. **No decompiler text, no game data in `src/`.** Write clean, idiomatic C++ that does the same
   thing. Never paste Ghidra output, never commit anything from `reports/`, never embed the
   game's art, sounds, level files, or big data tables. Referencing asset *file names* (the
   strings the game passes to `FileUtils`/`SpriteFrameCache`) is fine — they are how we load
   the player's own copy of the game at runtime.
6. **Mark doubt.** Anything you could not pin down: `// RE-TODO(@0060f534): <what is unclear>`.
7. **Long text stays out of the repo.** Short labels and identifiers (button captions, titles,
   UserDefault keys, asset names, format strings) are ordinary literals. Any user-facing text
   longer than a short sentence (~60 characters: option descriptions, warnings, help paragraphs,
   messages) is NOT written into source: `#include "GameText.h"` and write
   `OW_GAMETEXT(uniqueKey, 0xADDR)` where the text is used (ADDR = the string's Ghidra address;
   the expression yields `const std::string&`). The build extracts those strings from the
   player's own binary (`tools/re/extract_gametext.py`). Keys must be unique project-wide.

## 2. Tools and inputs

Inputs (none of them are in the repo; `binary/` and `reports/` are gitignored):

* the player's own Android 1.1.3 game: `binary/HappyWheels_Android/config.arm64_v8a/lib/arm64-v8a/libMyGame.so`
  and its assets in `binary/HappyWheels_Android/HW_Android/assets/`;
* a Ghidra export of `libMyGame.so`, one `<address>.json` per function (decompilation,
  assembly, callers), in the folder named by the `OW_EXPORTS` environment variable.
  `python tools/re/index_exports.py` turns it into `reports/decomp/` (per-class dumps,
  `functions.csv`, `classes.json`);
* optionally the iOS 1.2.7 app (`binary/HappyWheels_iOS/Payload/happywheels.app`) for
  `ios_ivars.py`, and per-class Ghidra exports of its binary (`ns_<Class>.c`) in
  `OW_IOS_EXPORTS` for `iosre.py`;
* the Android NDK r27 for `check_tu.sh` and `parity.py`: `ANDROID_NDK`, or the copy that
  `tools/build_android.ps1` installs into the Android SDK.

Python 3 with `capstone` (parity) and `unicorn` (oracle). Run from the repo root:

| Command | Use |
|---|---|
| `python tools/re/owre.py methods <Class>` | all methods: address, size, this/static, Ghidra return type, virtual slot |
| `python tools/re/owre.py fields <Class>` | every `this+off` access in the class's own code, with access types and users |
| `python tools/re/owre.py fn <regex\|addr> [--asm]` | decompiled body (and arm64 disassembly) of a function |
| `python tools/re/owre.py callers <regex\|addr>` | call sites |
| `python tools/re/owre.py vtable <Class>` / `vcall <Class> <off>` | resolve `(*(vptr+off))(...)` virtual calls |
| `python tools/re/owre.py layout <EngineType>` | arm64 layout of `cocos2d::Node`, `b2Body`, ... (to name engine field reads) |
| `python tools/re/owre.py data <addr> --as f32\|u32\|ptr\|str [--count N]` | constants, tables, strings, pointer tables (relocations applied) |
| `python tools/re/owre.py sym <addr>` | what symbol/global lives at an address |
| `python tools/re/owre.py imm 0x70637768 0x70` | decode short-string immediates → `'hwcpp'` |
| `python tools/re/owre.py class <Class>` | bases (RTTI), size, statics |
| `python tools/re/ios_ivars.py <ObjCClass>` | original member names, types and offsets from the iOS build (the original Objective-C codebase) |
| `python tools/re/iosre.py classes\|methods\|fn\|ivars\|senders ...` | iOS per-class decompilation (used for the editor port and for naming) |

Per-class decompilation dumps (address order, includes unnamed helpers of that TU):
`reports/decomp/classes/<Class>.c`. Index of all game functions: `reports/decomp/functions.csv`.
Engine layouts: `reports/decomp/engine_layouts.txt` (exact; verified against the binary:
`Node` = 0x2f8, `Layer` = 0x320, `Sprite` = 0x52d nvsize, `b2World` gravity at 0x19338).

Engine source (MIT/zlib): `thirdparty/cocos2d-x/cocos/...`, Box2D headers:
`thirdparty/cocos2d-x/external/Box2D/include/Box2D/...`. Read them to recognise inlined code.

Addresses everywhere are **Ghidra addresses** (file vaddr + 0x100000).

## 3. Reading Ghidra's arm64 output

* **Fields**: `*(float *)(this + 0x324)` = member at arm64 offset 0x324. Offsets below the
  base-class size belong to the base (engine bases: use `owre.py layout`). Prefer the public
  engine API the original inlined: a read of `Node+0x...` that is `_position` is
  `getPosition()`, `b2Body` `m_xf.p` is `GetPosition()`, etc.
* **Argument order is scrambled when floats are involved.** AAPCS64 passes integer/pointer
  args in x0..x7 (x0 = `this`) and float/HFA args in s0..s7 *independently*. Ghidra prints
  them in its own order, e.g. `(**(code **)(*(long *)view + 0xc0))(w, h, view, 3)` is
  `view->setDesignResolutionSize(w, h, (ResolutionPolicy)3)`, and
  `init(param_1._0_4_, this, param_2, param_3)` is `this->init(float, SoundController*, SessionMode)`.
  Always rebuild the call from the demangled signature + register classes. Ghidra's own
  *parameter lists* for functions taking floats are frequently wrong for the same reason
  (`Session::create(Session *param_1, ...)` really is `create(float, SoundController*, SessionMode)`).
* `cocos2d::Vec2`/`Size`/`b2Vec2` passed or returned by value travel in s-registers
  (`extraout_s1`, `uStack_xx` pairs). `Color3B/4B` pack into integers.
* **Virtual calls**: `(**(code **)(*(long *)p + 0x40))(p, ...)` → `owre.py vcall <StaticTypeOfP> 0x40`.
  `+0x0`/`+0x8` on game classes are the complete/deleting destructors; on `Ref`-derived
  objects a `+8` call after a null-check is `delete p`.
* **Boilerplate to drop**: `tpidr_el0`/`__stack_chk_fail` (stack protector), libc++
  short-string flag tests `if ((byte)local_70 & 1) operator_delete(...)` (that is just a
  `std::string` going out of scope), exception landing pads.
* **Strings**: inline literals usually appear as `"..."`. Short strings built from immediates
  (`local_70 = 0xa; local_6f = 0x70637768; local_6b = 0x70;`) — first byte is `len*2`, the
  rest is the text: `owre.py imm 0x70637768 0x70` → `hwcpp`.
* **Creation idiom**: `operator_new(N, nothrow)` + `memset(p,0,N)` + ctor + `init(...)` +
  `autorelease` is cocos2d's `new (std::nothrow) X(); if (x && x->init(..)) { x->autorelease(); return x; } delete x; return nullptr;`
  (check the exact failure path in the decompilation). `N` is the arm64 `sizeof(X)`.
* `FUN_xxxxxxxx` callees inside your TU are lambdas (`std::function` targets: `CallFunc`,
  touch/menu callbacks, `scheduleOnce`) or `static` helpers. Rebuild lambdas inline at the
  place they are created; their captures are the fields read off the closure object.
* `DAT_00ac....` (.bss, zero-init) and `DAT_00ab....` (.data) are globals/statics:
  `owre.py sym` names them; `_INIT_n` functions are static initialisers for the TU's globals.
* `std::vector`, `std::map`, `cocos2d::Vector` operations are inlined as raw pointer code —
  recognise the operation (push_back, erase, find, iteration) and write it normally.
* Ghidra mislabels some calls as `switchD_xxx::caseD_yy()` — check the assembly
  (`owre.py fn <addr> --asm`) for the real `bl` target; e.g. `caseD_4d` =
  `DestructionListener::addJointListener`, `caseD_56` = `ContactListener::addPostSolveListener`.
* Ghidra often drops or misreads float/bool arguments and Vec2 halves; when a call looks odd,
  read the assembly — AAPCS64 register assignment is the ground truth.

## 4. Source layout and style

* One header + one source per original class, named after the class:
  `src/game/<subsystem>/<Class>.h/.cpp`, subsystems `app, audio, services, session, render,
  level, items, triggers, characters, vehicles, gameplay, menus, debug`. Every folder is on the
  include path, so includes stay flat (`#include "LevelB2D.h"`). Pure interfaces (`*Delegate`,
  protocols) may be header-only. Shared enums go in the header of the class that owns them.
  Shared helpers: `src/game/app/Patch.h` (`patch::to_string<T>`), `GameText.h` (rule 7).
* C++14, cocos2d-x 3.17.2 API. Headers use fully qualified `cocos2d::` names; `.cpp` files may
  use `USING_NS_CC;`. `#pragma once` in headers. Include what you use.
* Members: `_camelCase` (cocos2d style), declared **in original offset order**, each with its
  arm64 offset: `b2World* _world; // +0x2f8`. Use real types (pointers to game/engine classes,
  `float`, `int`, `bool`, `cocos2d::Vec2`, `std::string`, `std::vector<...>`, `b2Vec2`...).
  Unknown purpose → name by role if you can (`_unk0x344` only as last resort). Padding/unused
  holes don't need placeholders. Name fields after their getters/setters when they exist
  (`getWorld()` → `_world`).
* Virtuals: declare exactly the virtuals the class's vtable shows (`owre.py vtable`), with
  `override` where they override a base. Keep the original virtual *order* within the class
  for new virtuals (declaration order = vtable order).
* Every function definition gets a one-line address tag: `// @0060f3cc` above it.
* Platform services that only exist on mobile (ads `AdController`, purchases
  `IAPController`, `Tracker` analytics, `sdkbox::*`, Firebase, JNI): keep the game-facing class
  and method signatures, but their *implementation* is a stub on every platform (ads never
  shown, analytics dropped, store unavailable). Game-side logic that merely *calls* them stays faithful.
* Debug-only/QA classes (`DebugLayer`, `DebugScene`, `Test`, `UITest`, `B2DebugDrawLayer`,
  `GLESDebugDraw`) are reconstructed too — they are reachable in the original.
* `cocos2d::log(...)` calls present in the binary are kept.

## 5. The behaviour oracle (original game running in an emulator)

`tools/re/oracle.py` boots the **original** `libMyGame.so` (engine + Box2D + game code) in an
arm64 emulator against the player's own assets, so you can *observe* what the real game does:

```bash
# load a level exactly like Gameplay does and dump the Box2D world (bodies/fixtures/joints)
python tools/re/oracle.py level --level levels/01_business_guy/02_business_guy_missing_fish.xml --out reports/x.json
# run the real Gameplay scene in replay mode: per-frame control bytes (GameplayBtn state bits,
# 0x01 = accelerate, 0x10 = jump/special ...), dumps at chosen frames
python tools/re/oracle.py play --frames 120 --script "0:01,60:00" --dump-at 30,60 --out reports/run.json
python tools/re/worlddiff.py reports/a.json reports/b.json      # compare two dumps
```

Use it to check constants, creation order and behaviour when the decompilation is ambiguous.
Our build gets the same dump format via `src/platform/debug/WorldDump.*`, so reconstructed
levels/physics are diffed body-by-body against the original:
`OpenWheels.exe --dump-world out.json --level <levels/...xml> --frames N --script "0:01,90:05" --dump-at a,b`
(Windows build; writes the dump and exits). Outputs go to `reports/` (ignored).

## 6. Verification of a translation unit

`bash tools/check_tu.sh src/game/<dir>/Foo.cpp` runs a syntax/semantic check of one TU against
the real cocos2d-x 3.17.2 headers (NDK clang, Android/arm64 config — the same configuration the
game was built with). Every `.cpp` in `src/game/` must pass it. With `OW_CHECK_EMIT_OBJ=1` it
also writes an arm64 object for the static parity diff:

```bash
bash tools/check_tu.sh src/game/level/Foo.cpp                     # must pass
OW_CHECK_EMIT_OBJ=1 bash tools/check_tu.sh src/game/level/Foo.cpp # -> build/arm64/Foo.o
python tools/re/parity.py build/arm64/Foo.o                       # call/constant diff vs original
```

`parity.py` lists, per function, calls and float constants the original has but our build does
not (and extras). Inlining differences cause some noise; every **missing call** or **missing
constant** must be fixed or explained (the module notes record the explanations). Use the
oracle (section 5) when behaviour is ambiguous, and `tools/re/compare_play.py` for the
whole-campaign physics comparison (`docs/PARITY.md`).

The full builds are `tools/build.ps1` (Windows, MSVC, Win32) and `tools/build_android.ps1`
(`docs/ANDROID.md`); both need the engine from `tools/fetch_engine.ps1`.

## 7. Shared facts

* **Control bits** (`GameplayControls::getState()` → `CharacterB2D::setState(unsigned char)`):
  0x01 forward/accelerate, 0x02 back/brake, 0x04 lean forward, 0x08 lean back,
  0x10 special/grab/jump, 0x80 eject. The restored characters add 0x20/0x40 for their extra
  buttons (`docs/RESTORED.md`).
* **HWWindowDelegate**: 8-byte interface, no virtual dtor, non-pure empty defaults:
  slot 0 `virtual void hwWindowButtonPressed(int buttonTag, HWWindow* window) {}`,
  slot 1 `virtual void hwWindowWasDismissed(HWWindow* window) {}`; tags 1 = confirm,
  0 = cancel, -1 = close button.
* **SecondaryMenu**: `createScene(bool)` (arg ignored), new virtuals `addContent()` then
  `backBtnPressed()`; subclasses set `_title` before calling `SecondaryMenu::init`.
* **Level start path**: `Settings::setSelectedLevel(chapter, level)` reads
  `"levels/" + dataFile` via `FileUtils::getStringFromFile` into Settings; menus then call
  `Gameplay::createScene("", nullptr)`; an empty string makes `Gameplay::init` use the Settings
  copy; `LevelB2D::init(std::string)` receives the XML *contents*.
* **Allocation**: `operator_new(N, nothrow)` followed by `memset(p,0,N)` is value-initialisation
  `new (std::nothrow) X()` — those classes deliberately have no user-declared constructor.
  No memset ⇒ `new (std::nothrow) X` / user constructor (members may stay uninitialised, as in
  the original — keep it).
* `LevelItem::debugFunction(int)` is `void` (a lone `ret` in the binary).
* `rand()`: the game relies on bionic's `RAND_MAX` (0x7fffffff); `src/platform/compat/BionicRand.h`
  supplies a bionic-compatible `rand()` for every `src/game` TU on Windows.
* Float parity on Windows: see `docs/PARITY.md` (x87 joint getters → `owb2::jointAngle/jointSpeed`
  in `src/platform/compat/Box2DFloat.h`; fused multiply-adds spelled out with `std::fma`).

## 8. Code outside the 1:1 reconstruction

Everything that is not a translation of `libMyGame.so` is kept out of `src/game/` where
possible, and every hook inside `src/game/` carries a marker comment so it can be found and
audited. None of these change the campaign's behaviour unless the feature is in use.

| Marker | Code | Doc |
|---|---|---|
| `// EDITOR (iOS port):` | `src/editor/`, `src/platform/common/` | `docs/EDITOR_PORT.md` |
| `// ONLINE (PC addition):` | `src/online/` (browser levels) | `docs/FLASH_LEVELS.md` |
| `// RESTORED (PC addition):` | `src/restored/` (browser-only characters) | `docs/RESTORED.md` |
| `// QOL (PC addition):` | `src/qol/` (options page) | `docs/QOL.md` |
| `NO-ADS (port)`, PC platform policy comments | `src/game/services/` (ads, store, analytics) | `docs/modules/M10.md` |
| `ANDROID (port)` | `src/platform/android/`, a few UI spots | `docs/ANDROID.md` |

Mobile-only services keep their game-facing API but do nothing: no ads on any platform (the
mascot's rewarded-video revive is granted at once), no store, no analytics.
