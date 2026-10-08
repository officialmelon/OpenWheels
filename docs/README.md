# OpenWheels documentation

## Overview and reconstruction

* [RECONSTRUCTION.md](RECONSTRUCTION.md) — the reconstruction handbook: ground rules for
  `src/game/`, the reverse-engineering tools and their inputs, reading Ghidra's arm64 output,
  source layout, shared facts, and the markers for code outside the 1:1 reconstruction.
* [MODULES.md](MODULES.md) — how the game classes were split into modules M1-M10, and the arm64
  layouts of the shared base classes.

## Verification

* [PARITY.md](PARITY.md) — campaign physics parity against the original running in the
  emulation oracle: results, fixes, and the remaining float drift on MSVC.

## Extras beyond the Android game

* [EDITOR_PORT.md](EDITOR_PORT.md) — the iOS level editor ported to cocos2d-x: rules, layout,
  where its art and text come from.
* [FLASH_LEVELS.md](FLASH_LEVELS.md) — playing browser-game levels from totaljerkface.com: the
  level API and record format, the Flash-to-mobile converter, and the browser-level runtime.
* [RESTORED.md](RESTORED.md) — the five browser-only player characters restored from the
  browser game.
* [QOL.md](QOL.md) — the Quality of Life options page (blood styles, particles, camera, ...)
  and the desktop keyboard controls.
* [NEARBY.md](NEARBY.md) — Send to Nearby: levels between players on the same Wi-Fi (LAN
  discovery, the transfer protocol, receive codes, firewall and emulator notes).
* [RACE.md](RACE.md) — Ghost Race: local races between nearby players, the others shown as
  translucent ghosts (lobby, snapshot format, protocol, bandwidth).
* [ANDROID.md](ANDROID.md) — building the Android APK, staged game files, release signing.
* [DESKTOP.md](DESKTOP.md) — the Linux and macOS builds, the PC window (resizable, maximized,
  fullscreen without black bars) and the notes on the engine's old prebuilts.
* [IOS.md](IOS.md) — the iOS build, bundled or Finder-copied game files.

## Work notes

Historical logs written while the code was reconstructed (see the note in MODULES.md). They
hold address-level findings, field layouts, kept quirks and parity explanations per class.

* [modules/M1.md](modules/M1.md)-[modules/M10.md](modules/M10.md) — the game modules
  (M1 CharacterB2D, M2 bodies and vehicle base, M3 vehicles, M4 level, M5 session core,
  M6 weapons, M7 items and gameplay, M8 menus, M9 windows and secondary menus,
  M10 infrastructure).
* [editor/E1.md](editor/E1.md)-[editor/E5.md](editor/E5.md) — the editor port (E1 model core,
  E2 per-item refs, E3 editor core, E4 editor UI, E5 persistence and user levels).
