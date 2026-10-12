# Building from source

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

## Windows

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

## Linux and macOS

```sh
tools/fetch_engine.sh     # cocos2d-x 3.17.2 + dependencies into thirdparty/
tools/build.sh            # RelWithDebInfo by default; --config Debug|Release
build-linux/bin/OpenWheels/OpenWheels   # or build-macos/bin/OpenWheels/OpenWheels.app
```

Linux needs CMake 3.13+, GCC or Clang, Python 3 and the engine's system libraries (X11, GTK 3,
GLEW, OpenAL, libvorbis, libmpg123, ...; the apt line is in [DESKTOP.md](DESKTOP.md)).
The generators need the same Python packages and tools as on Windows. Game files are looked up in
`--assets <dir>`, `assets/` next to the executable, `~/.local/share/OpenWheels/assets` (macOS:
`~/Library/Application Support/OpenWheels/assets`) or `binary/` in the repo.

## iOS

On a Mac with Xcode: `tools/fetch_engine.sh`, then `OW_IOS_TEAM=<team id> tools/build.sh --ios`
(or open the generated `build-ios/OpenWheels.xcodeproj`). Your game files are bundled when
`binary/` has them, or can be copied onto the device with Finder. See [IOS.md](IOS.md).

## Android

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
[ANDROID.md](ANDROID.md).

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
src/input/       game controllers, menu focus navigation, rumble and tilt steering
src/platform/    Win32, Linux, macOS, iOS and Android entry points (desktop/: keyboard bridge and
                 window shared by the PC builds, unix/: POSIX main), crash handlers, shared helpers,
                 no-op stand-ins for the mobile SDKs (ads, analytics), Box2D world dumper
android/         the Android Gradle project
tools/           build scripts, window capture and input helpers
tools/re/        reverse-engineering and verification tools (emulator oracle, parity, world diff)
tools/assets/    build-time generators for the browser-game art and sounds (from your own files)
tools/levels/    browser-level decoding, the restored characters' level generator and previewer
res/levels/      OpenWheels' own levels (the restored characters' campaign chapters)
tools/parity/    compiler parity experiments
docs/            documentation (index: docs/README.md)
```

Code outside the 1:1 reconstruction is marked in the source (`EDITOR (iOS port)`,
`ONLINE (PC addition)`, `RESTORED (PC addition)`, `QOL (PC addition)`). All of it is gated, so the
campaign keeps behaving exactly like the original.
