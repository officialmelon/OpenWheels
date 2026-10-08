# Linux and macOS builds

OpenWheels builds natively on Linux (x86_64) and macOS on the same unmodified cocos2d-x 3.17.2 and
v3-deps-158 prebuilts as Windows and Android. The game code is shared; only the entry point differs:

| Folder | Built on | Contents |
|---|---|---|
| `src/platform/win32/` | Windows | `WinMain`, minidump crash handler |
| `src/platform/linux/`, `src/platform/mac/` | Linux, macOS | `main()` (+ the macOS `Info.plist`) |
| `src/platform/unix/` | Linux, macOS | `DesktopMain.cpp` (the POSIX twin of the Windows `main.cpp`: same command line, asset lookup, logging), signal crash handler |
| `src/platform/desktop/` | Windows, Linux, macOS | keyboard bridge (`PCInput`), `--dump-world` runner, window management (`DesktopWindow`) |

## Building

```sh
tools/fetch_engine.sh     # cocos2d-x 3.17.2 + v3-deps-158 into thirdparty/cocos2d-x, applies thirdparty/patches
tools/build.sh            # RelWithDebInfo; --config Debug|Release, --build-dir <dir>, --no-build
```

Linux needs CMake 3.13+, a C++14 compiler (GCC or Clang), Python 3 and the engine's system
libraries. On Debian / Ubuntu:

```sh
sudo apt install build-essential cmake ninja-build python3 libx11-dev libxi-dev libxrandr-dev \
  libxxf86vm-dev libxinerama-dev libxcursor-dev libfontconfig1-dev libgtk-3-dev zlib1g-dev \
  libpng-dev libglew-dev libgl1-mesa-dev libcurl4-openssl-dev libsqlite3-dev
```

macOS needs Xcode's command line tools and CMake. The v3-deps-158 macOS prebuilts are x86_64 only,
so `tools/build.sh` builds an Intel app (`CMAKE_OSX_ARCHITECTURES=x86_64`) that runs under
Rosetta 2 on Apple Silicon.

The optional generators (sound table and UI text from `libMyGame.so`, restored characters,
browser-level items and sounds, kid gore) run after the build exactly as on Windows and need the
same inputs under `binary/` (see the README). Python needs `unicorn` and `capstone`; the browser
extras need Java 11+, FFDec, `numpy`, `Pillow` and `ffmpeg`.

Output: `build-linux/bin/OpenWheels/OpenWheels`, or `build-macos/bin/OpenWheels/OpenWheels.app`.

### Notes on the old prebuilts

* The Linux prebuilts (freetype, ...) are not position independent, so the executable is linked
  with `-no-pie`.
* They were built with `-ffast-math` against glibc < 2.31; `src/platform/linux/GlibcCompat.c`
  forwards the `__powf_finite`-style entry points newer glibc dropped.
* `thirdparty/patches/cocos2d-x-3.17.2-gcc-cstdint.patch` adds a missing `<cstdint>` include that
  current GCC needs (harmless on MSVC).
* The game's translation units get bionic's `rand()` (`src/platform/compat/BionicRand.h`) on every
  platform but Android, and are compiled without FMA contraction (`-ffp-contract=off`), like the
  original's arm64 clang build.

## Running

The game looks for the player's own Android `assets/` folder, in this order: `--assets <dir>`,
`assets/` next to the executable (macOS: also `OpenWheels.app/Contents/Resources/assets/` and
`assets/` next to the `.app`), `~/.local/share/OpenWheels/assets/` (Linux, or
`$XDG_DATA_HOME/OpenWheels/assets/`) / `~/Library/Application Support/OpenWheels/assets/` (macOS),
then `binary/HappyWheels_Android/HW_Android/assets/` found by walking up from the executable.

Every command-line option of the Windows build works (`--play-level`, `--play-online`, `--open`,
`--dump-world`, `--convert-flash`, `--console`, ...). Without `--console` the log is written to
`openwheels.log` next to the executable (or in the per-user data folder when that isn't writable);
a crash writes `openwheels_crash.txt` with a stack trace.

Saves and settings live in cocos2d-x's writable path: `~/.config/OpenWheels/` on Linux,
`~/Library/Application Support/OpenWheels/` on macOS.

## Window

On every PC build the window is resizable and starts maximized (unless `--width` / `--height`
are given). Fullscreen (QOL page or F11) uses the monitor's current resolution. The design
resolution keeps the original's fixed height, so any aspect ratio fills the screen without black
bars: the size is settled before the first scene is laid out, and when it changes later the
running scene stays centred with its backgrounds stretched (the next screen is laid out for the
new size; the title screen is rebuilt at once). See `src/platform/desktop/DesktopWindow.cpp`.
