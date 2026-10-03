# Phase I — implementation

Phase H produced verified headers for every class (`src/game/*.h`). Phase I implements every
function in `src/game/<Class>.cpp`. Rules in `docs/RECONSTRUCTION.md` still apply in full.

## Shared facts established in Phase H

* **Control bits** (`GameplayControls::getState()` → `CharacterB2D::setState(unsigned char)`):
  0x01 forward/accelerate, 0x02 back/brake, 0x04 lean forward, 0x08 lean back,
  0x10 special/grab/jump, 0x80 eject.
* **HWWindowDelegate** (M9): 8-byte interface, no virtual dtor, non-pure empty defaults:
  slot 0 `virtual void hwWindowButtonPressed(int buttonTag, HWWindow* window) {}`,
  slot 1 `virtual void hwWindowWasDismissed(HWWindow* window) {}`; tags 1 = confirm,
  0 = cancel, -1 = close button.
* **SecondaryMenu** (M9): `createScene(bool)` (arg ignored), new virtuals `addContent()` then
  `backBtnPressed()`; subclasses set `_title` before calling `SecondaryMenu::init`.
* **Level start path** (M8): `Settings::setSelectedLevel(chapter, level)` reads
  `"levels/" + dataFile` via `FileUtils::getStringFromFile` into Settings; menus then call
  `Gameplay::createScene("", nullptr)`; an empty string makes `Gameplay::init` use the Settings
  copy; `LevelB2D::init(std::string)` receives the XML *contents*.
* **Allocation**: `operator_new(N, nothrow)` followed by `memset(p,0,N)` is value-initialisation
  `new (std::nothrow) X()` — those classes deliberately have no user-declared constructor.
  No memset ⇒ `new (std::nothrow) X` / user constructor (members may stay uninitialised, as in
  the original — keep it).
* **Original member names**: `python tools/re/ios_ivars.py <ObjCClass>` dumps ivar names,
  types and offsets from the iOS build (the original Objective-C codebase).
* **Shared helpers**: `src/game/Patch.h` (`patch::to_string<T>`), `src/game/GameText.h`
  (long UI text by key + original address — see RECONSTRUCTION.md rule 7).
* `LevelItem::debugFunction(int)` is `void` (a lone `ret` in the binary).
* Ghidra mislabels some calls as `switchD_xxx::caseD_yy()` — check the assembly
  (`owre.py fn <addr> --asm`) for the real `bl` target; e.g. `caseD_4d` =
  `DestructionListener::addJointListener`, `caseD_56` = `ContactListener::addPostSolveListener`.
* Ghidra often drops or misreads float/bool arguments and Vec2 halves; when a call looks odd,
  read the assembly — AAPCS64 register assignment is the ground truth.

## What to write

1. `src/game/<Class>.cpp` for every class you own: **every** named function (one definition
   per function; inline-duplicate copies once) and every unnamed helper/lambda of your TU.
   Each definition gets its `// @address` tag.
2. Header fixes you discover (yours only). If you need a trivial accessor that another
   module's header lacks (inlined everywhere, so no symbol), you MAY add a one-line inline
   getter/setter to that header with a minimal Edit; change nothing else in others' files and
   list it in your report.
3. Keep `docs/modules/Mx.md` updated as you go (what is done, what is left) — if you are
   interrupted, a successor must be able to continue from it.

## Mandatory verification for every .cpp

```bash
bash tools/check_tu.sh src/game/Foo.cpp                     # must pass (real engine headers)
OW_CHECK_EMIT_OBJ=1 bash tools/check_tu.sh src/game/Foo.cpp # arm64 object for parity
python tools/re/parity.py build/arm64/Foo.o                 # call/constant diff vs original
```

`parity.py` lists, per function, calls and float constants the original has but your build
does not (and extras). Inlining differences cause some noise; every **missing call** or
**missing constant** must be either fixed or explained in your report. Target: every function
"identical" or explained. Use `python tools/re/oracle.py ...` (RECONSTRUCTION.md §5) when
behaviour is ambiguous.

## Report when done (under ~500 words)

Functions implemented / total for each class, parity summary (identical vs explained),
remaining RE-TODOs, header changes (incl. any accessor added to others' headers), and
anything the integrator must know.
