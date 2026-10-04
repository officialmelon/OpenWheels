# OpenWheels

**OpenWheels — an open-source reimplementation of Happy Wheels.**

OpenWheels rebuilds *Happy Wheels* mobile (Android 1.1.3, cocos2d-x 3.17.2 + Box2D) as readable
C++, class for class and function for function, and runs it natively on Windows and Android. On
top of that faithful core it adds the things the mobile game never had: the iOS level editor,
online browser levels from totaljerkface.com, the five browser-only characters, and a Quality of
Life options page. There are no ads, and the game doesn't need to be online.

![Main menu](docs/screenshots/main-menu.png)

> OpenWheels is an unofficial fan project. It is not affiliated with or endorsed by Fancy Force or
> Jim Bonacci. The source tree contains **no game assets**. See [Legal](#legal).

## Features

### A 1:1 reconstruction of the mobile game

Every class of the original `libMyGame.so` was rebuilt with its original names, signatures and
behaviour. That covers the characters and ragdolls, vehicles, level loading, items and triggers,
gameplay, menus and windows. Parity is measured, not assumed: the original game runs in an arm64
emulator (the "oracle"), and the reconstruction's Box2D world is compared with it body by body.
**All 73 campaign levels load into a world identical to the original's.** Over long runs the
physics drifts slightly, because MSVC rounds floats differently from the original's clang build;
see [docs/PARITY.md](docs/PARITY.md).

![A campaign level](docs/screenshots/campaign.png)

### Eleven playable characters

The six mobile characters plus the five that only existed in the browser game, all in character
select. Each restored character has his or her own vehicle, controls, gore and sounds:
**Lawnmower Man**, **Explorer Guy**, **Santa Claus**, **Irresponsible Mom** and **Helicopter
Man**. They are rebuilt at build time from your own copy of the browser game's character files;
see [docs/RESTORED.md](docs/RESTORED.md).

![Character select with all 11 characters](docs/screenshots/character-select.png)

![The restored characters](docs/screenshots/restored-characters.png)

![Helicopter Man](docs/screenshots/helicopter-man.png)

### Level editor (ported from iOS)

The iOS version had a level editor that Android never got. OpenWheels ports it: build levels with
shapes, building blocks, hazards and the other items, test-play them, save them, and play and share
your own levels. It uses the editor art and text from your own copy of the iOS app; see
[docs/EDITOR_PORT.md](docs/EDITOR_PORT.md).

![Level editor](docs/screenshots/level-editor.png)

### Online levels from the browser game

Search and browse the browser game's user levels on totaljerkface.com (top rated, most played,
newest and more), then download and play them. Levels are decrypted and converted to the mobile format on the
fly. The browser-only features are implemented too: NPC characters, text boxes, triggers,
furniture and other items, user-built vehicles, the city background and the browser game's sounds.
See [docs/FLASH_LEVELS.md](docs/FLASH_LEVELS.md).

![Online level browser](docs/screenshots/online-levels.png)

![An online level with NPCs and text](docs/screenshots/online-level-npcs.png)

### Quality of Life options

*Options → quality of life* collects the extras. Every option defaults to the original game's
behaviour:

- **Blood:** classic, streaks, liquid or realistic (the four blood settings of the browser game)
- **Max particles:** 2000 (original), 4000 or 8000
- **Camera:** normal, or three levels of zoom-out
- **Textures:** auto (the original's choice) or a fixed large / medium / small / tiny asset tier
- **Frame rate:** 60 (original) or 30 FPS, like the browser game; the physics run the same steps
- **Sound effects** and **music** volume sliders
- **FPS counter**, **unlock all levels**, **fullscreen** (or F11)
- **Child gore:** gives the Irresponsible Dad's son the gore the mobile version left out
- **Controls** (PC): remap every keyboard action

See [docs/QOL.md](docs/QOL.md).

![Quality of Life page](docs/screenshots/quality-of-life.png)

![Classic and realistic blood](docs/screenshots/blood-styles.png)

### Windows and Android

OpenWheels runs on Windows (Win32) with mouse and keyboard, and on Android phones and tablets with
any aspect ratio.

![OpenWheels on an Android phone](docs/screenshots/android-phone.png)

## Download and install

Builds are published on the [Releases](https://github.com/officialmelon/OpenWheels/releases) page.

- **Windows:** download `OpenWheels-windows.zip`, unzip it anywhere and run `OpenWheels.exe`.
- **Android:** download `OpenWheels-release.apk` to your device and open it. Allow installs from
  unknown sources when Android asks.

## Controls

On touch screens the controls are the same as the mobile game's. On PC, the mouse acts as touch
and these keys work by default (remap them in *Options → quality of life → controls*):

| Key | Action |
|---|---|
| Up / W | Forward (accelerate) |
| Down / S | Back (brake / reverse) |
| Left / A | Lean back |
| Right / D | Lean forward |
| Space | Special action (jump, jet, brake; grab when ejected) |
| Z | Eject |
| Shift / Ctrl | Character actions (restored characters, e.g. Helicopter Man's magnet) |
| Esc / P | Pause |
| R | Restart |
| F11 | Toggle fullscreen |

## Building from source

The source tree contains no game assets. To build and run OpenWheels you need **your own copy** of
the game files, placed under `binary/` in the repo. `binary/` is git-ignored and never committed.

```
binary/
  HappyWheels_Android/
    HW_Android/assets/                         required: the Android game's assets folder
                                               (shared/, sounds/, large/, medium/, small/, tiny/)
    config.arm64_v8a/lib/arm64-v8a/libMyGame.so  required: the original arm64 game library, used
                                               at build time to generate the sound table and the
                                               UI text (gametext.tsv, soundlist.tsv)
  HappyWheels_iOS/Payload/happywheels.app/     optional: the iOS app, for the level editor's art
                                               and text
  flash/                                       optional: browser-game files for the extras
    swf/game_e_v1_87_g.dec.swf                 decrypted browser game: online-level item art,
                                               text-box fonts, child gore
    swf/characters/character<N>.swf            the restored characters
    swf/happy_sounds_v1_72.swf                 browser-game sounds
    tools/ffdec/                               JPEXS FFDec, used to read the SWFs
```

The optional parts are skipped quietly when they are missing. Without the iOS app there is no
editor art. Without the SWFs the restored characters aren't available and online-level items
draw as plain shapes.

### Windows

Requirements:

- Visual Studio 2022 with the C++ desktop workload (MSVC v143, Win32). The build uses Visual
  Studio's bundled CMake.
- Python 3 with `unicorn` and `capstone` (`pip install unicorn capstone`), to generate the tables
  from `libMyGame.so`.
- For the browser-game extras: Java 11+, FFDec, `numpy` and `Pillow`, and `ffmpeg` for the
  sounds.

```powershell
git clone https://github.com/officialmelon/OpenWheels.git
cd OpenWheels
powershell -ExecutionPolicy Bypass -File tools\fetch_engine.ps1   # cocos2d-x 3.17.2 + dependencies
powershell -ExecutionPolicy Bypass -File tools\build.ps1          # RelWithDebInfo by default
build\bin\OpenWheels\RelWithDebInfo\OpenWheels.exe
```

`tools\build.ps1 -Config Debug|Release` picks the configuration. If you only have Visual Studio
2026 installed, use `-Generator "Visual Studio 18 2026" -Toolset v145` with a separate
`-BuildDir`. `tools\package_windows.ps1` makes the release folder and zip (`dist\OpenWheels-windows\`
and `dist\OpenWheels-windows.zip`).

The game finds its assets in `assets\` next to the exe, or in `binary\HappyWheels_Android\...` in
the repo, or wherever `--assets <dir>` points.

Useful command-line options:

| Option | Effect |
|---|---|
| `--width <px> --height <px>` | Window size (picks the original's art tier) |
| `--assets <dir>`, `--ios-app <dir>` | Use game files from another location |
| `--play-level <file.xml>` | Play a level file directly (`--select-character <id>` opens character select) |
| `--play-online <id>` | Play a browser level by its id |
| `--open <file>` | Import a `.happywheels` or level file (dragging it onto the exe does the same) |
| `--console` | Log to a console window instead of `openwheels.log` |
| `--dump-world <out.json>` | Verification: dump the Box2D world for comparison with the oracle |
| `--convert-flash <in> <out>` | Convert a browser level to the mobile format |

If the game crashes or hangs, it writes `openwheels_crash.txt` (with a minidump) or
`openwheels_hang.txt` next to the exe. Please attach these to bug reports.

### Android

Requirements: the Android SDK (`tools\build_android.ps1` installs the platform, build tools, CMake
and NDK r27 it needs), JDK 17+ (Android Studio's bundled one works), and Python 3 with `unicorn`.

```powershell
powershell -ExecutionPolicy Bypass -File tools\build_android.ps1                      # debug APK
powershell -ExecutionPolicy Bypass -File tools\build_android.ps1 -Config Release
powershell -ExecutionPolicy Bypass -File tools\build_android.ps1 -Abis arm64-v8a -Run # build, install, launch
```

Gradle copies your game files from `binary/` into the APK. The APK is written to
`build\android\OpenWheels-<config>.apk`. Release builds are signed with the keystore that
`OW_KEYSTORE_PROPERTIES` describes, or with the debug key if it isn't set. See
[docs/ANDROID.md](docs/ANDROID.md).

## Project layout

```
src/game/        the 1:1 reconstruction of the Android game, one .h/.cpp per original class
                 (app, audio, services, session, render, level, items, triggers, characters,
                 vehicles, gameplay, menus, debug)
src/editor/      the iOS level editor port (model, core, ui, persistence)
src/online/      online browser levels: level API client, browser, Flash-to-mobile converter,
                 browser-level runtime, NPCs and items
src/restored/    the five restored browser characters and their vehicles
src/qol/         the Quality of Life options and the blood compositor
src/platform/    Win32 and Android entry points, keyboard bridge, crash handler, shared helpers,
                 no-op stand-ins for the mobile SDKs (ads, analytics), Box2D world dumper
android/         the Android Gradle project
tools/           build scripts, window capture and input helpers
tools/re/        reverse-engineering and verification tools (emulator oracle, parity, world diff)
tools/assets/    build-time generators for the browser-game art and sounds (from your own files)
tools/levels/    browser-level decoding
tools/parity/    compiler parity experiments
docs/            documentation (see below)
```

Code outside the 1:1 reconstruction is marked in the source (`EDITOR (iOS port)`,
`ONLINE (PC addition)`, `RESTORED (PC addition)`, `QOL (PC addition)`). All of it is gated, so the
campaign keeps behaving exactly like the original.

## Documentation

[docs/README.md](docs/README.md) is the index. The main documents are:

- [RECONSTRUCTION.md](docs/RECONSTRUCTION.md): how the reconstruction was done, its rules and
  tools
- [PARITY.md](docs/PARITY.md): physics parity against the original
- [EDITOR_PORT.md](docs/EDITOR_PORT.md): the level editor port
- [FLASH_LEVELS.md](docs/FLASH_LEVELS.md): online browser levels
- [RESTORED.md](docs/RESTORED.md), [QOL.md](docs/QOL.md), [ANDROID.md](docs/ANDROID.md)
- [MODULES.md](docs/MODULES.md), `docs/modules/` and `docs/editor/`: per-class work notes

## Credits

- *Happy Wheels* was created by **Jim Bonacci** and published by **Fancy Force**
  ([totaljerkface.com](https://totaljerkface.com)).
- The user levels in the online browser belong to their authors.
- OpenWheels is built on [cocos2d-x](https://github.com/cocos2d/cocos2d-x) (MIT) and
  [Box2D](https://box2d.org) (zlib).

## Legal

OpenWheels is an unofficial, non-commercial fan project. It is not affiliated with, endorsed by or
sponsored by Fancy Force or Jim Bonacci. *Happy Wheels* is their trademark, and its art, sounds,
text and levels are their property. This repository contains no game assets; building it requires
your own copy of the game. If you enjoy OpenWheels, please support the official game.

The full notice is in [NOTICE.md](NOTICE.md).

## License

The OpenWheels source code is released under the [MIT License](LICENSE), © 2026 officialmelon.
The license covers only the OpenWheels code. It doesn't cover Happy Wheels or any of its assets.
