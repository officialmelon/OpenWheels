# OpenWheels

An open-source, source-level reconstruction of the **Happy Wheels mobile** game engine
(Android 1.1.3: cocos2d-x 3.17.2 + Box2D), rebuilt as readable C++ that compiles against the
same engine version and runs natively on Windows.

The goal is behavioural 1:1 parity with the original: same physics steps, same level loading,
same item, character and vehicle behaviour, same menus. Parity is *measured*, not assumed:
the original game runs in an arm64 emulator (the "oracle") and the reconstruction's Box2D world
is diffed against it body by body, frame by frame (see `docs/RECONSTRUCTION.md` §5).

**No game assets are included.** OpenWheels needs your own legally obtained copy of the
Android game; it loads the art, sounds and levels from it at runtime, and generates the few
data tables it needs (sound list, long UI text) from your copy at build time.

## Building (Windows)

Requirements: Visual Studio 2022 (MSVC v143, Win32), Python 3 with `unicorn` and `capstone`
(`pip install unicorn capstone`).

```powershell
powershell -ExecutionPolicy Bypass -File tools\fetch_engine.ps1   # cocos2d-x 3.17.2 + deps
powershell -ExecutionPolicy Bypass -File tools\build.ps1           # -> build\bin\OpenWheels\RelWithDebInfo\
```

Put your game files where the build and the game can find them:

* the extracted Android assets folder (contains `shared/`, `sounds/`, `large/`, ...) — pass
  `--assets <dir>` or place it at `binary/HappyWheels_Android/HW_Android/assets` in the repo;
* `libMyGame.so` (arm64) at `binary/HappyWheels_Android/config.arm64_v8a/lib/arm64-v8a/`
  or set the CMake cache variable `OW_GAME_LIB`.

## Playing

Mouse = touch. Keyboard: Up/W forward, Down/S back, Right/D lean forward, Left/A lean back,
Space special (jump/jet/brake; grab when ejected), Z eject, Esc/P pause, R reset after death.
Options: `--width/--height` (window size picks the original's art tier), `--console`.

## Layout

```
src/game/<subsystem>/   one .h/.cpp per original class, original names and signatures
  app audio services session render level items triggers
  characters vehicles gameplay menus debug
src/platform/win32/     PC entry point, keyboard bridge, --dump-world verification runner
src/platform/compat/    bionic-compatible rand() (same random sequences as Android)
src/platform/stubs/     no-op stand-ins for mobile SDKs (ads, analytics, store review)
src/platform/debug/     Box2D world dumper (oracle-compatible JSON)
tools/re/               reverse-engineering + verification tooling (emulator oracle, parity)
docs/                   reconstruction rules, module notes, roadmap
```

## Verification

```powershell
python tools\re\oracle.py play --frames 120 --script "0:01" --dump-at 120 --out reports\o.json
build\bin\OpenWheels\RelWithDebInfo\OpenWheels.exe --dump-world reports\ours.json --frames 120 --script 0:01
python tools\re\worlddiff.py reports\o_f120.json reports\ours.json
python tools\re\compare_play.py          # every level, original vs reconstruction
```

## Roadmap

See `docs/ROADMAP.md`: translations, custom levels, and the iOS-only level editor.

## Legal

Happy Wheels, its art, sounds, text and levels are © Fancy Force. This repository contains
no game assets or game data; it is an independent reimplementation intended for
interoperability with a legally obtained copy of the game. `binary/` (reference copies of the
original) is never committed.
