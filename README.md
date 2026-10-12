# OpenWheels

**An open-source remake of the *Happy Wheels* mobile game, rebuilt in C++ and running natively on
Windows, Linux, macOS, Android and iOS.**

OpenWheels is a function-for-function reconstruction of the Android game (cocos2d-x + Box2D), so
the campaign plays exactly like the original. On top of that it adds what mobile never had: the
level editor, online levels from the browser game, the five browser-only characters, controller
support and a page of Quality of Life options. No ads, no account needed.

![Main menu](docs/screenshots/main-menu.png)

> Unofficial fan project, not affiliated with Fancy Force or Jim Bonacci. The repository contains
> **no game assets**: you need your own copy of the game. See [Legal](#legal).

## Highlights

- **All 73 campaign levels**, physics checked body by body against the original
  ([parity](docs/PARITY.md))
- **11 characters**: the six mobile ones plus Lawnmower Man, Explorer Guy, Santa Claus,
  Irresponsible Mom and Helicopter Man, each with a new campaign chapter
  ([restored characters](docs/RESTORED.md))
- **Level editor** with everything the browser editor can do ([editor](docs/EDITOR_PORT.md))
- **Online levels** from totaljerkface.com, with replays, ratings and login
  ([online levels](docs/FLASH_LEVELS.md))
- **Ghost racing** and **level sharing** with friends on the same Wi-Fi
  ([race](docs/RACE.md), [nearby](docs/NEARBY.md))
- **Controllers, keyboard remapping, blood styles, camera zoom** and more ([QoL](docs/QOL.md),
  [controllers](docs/CONTROLLERS.md))

The full tour with screenshots and the controls is in [docs/FEATURES.md](docs/FEATURES.md).

## Get it

Download the latest build from [Releases](https://github.com/officialmelon/OpenWheels/releases):

- **Windows:** unzip `OpenWheels-windows.zip` and run `OpenWheels.exe`
- **Android:** install `OpenWheels-release.apk` (allow unknown sources)
- **Linux:** unpack `OpenWheels-linux-x86_64.tar.gz` and run `./OpenWheels`
- **macOS, iOS:** unsigned packages when available, otherwise build from source

To build it yourself, see [docs/BUILDING.md](docs/BUILDING.md).

**Controls on PC:** arrows / WASD to drive and lean, Space for the special action, Z to eject,
Esc to pause, R to restart, F11 for fullscreen. Controllers work too.

## Documentation

- [Features and controls](docs/FEATURES.md)
- [Building from source](docs/BUILDING.md) and project layout
- [Documentation index](docs/README.md): how the reconstruction was done, parity, per-module notes
- [Changelog](CHANGELOG.md)

## Credits

Made by [@officialmelon](https://github.com/officialmelon).

*Happy Wheels* was created by **Jim Bonacci** and published by **Fancy Force**
([totaljerkface.com](https://totaljerkface.com)). Online levels belong to their authors.
Built on [cocos2d-x](https://github.com/cocos2d/cocos2d-x) (MIT) and [Box2D](https://box2d.org)
(zlib).

## Legal

OpenWheels is an unofficial, non-commercial fan project, not affiliated with, endorsed by or
sponsored by Fancy Force or Jim Bonacci. *Happy Wheels* is their trademark, and its art, sounds,
text and levels are their property. This repository contains no game assets. If you enjoy
OpenWheels, please support the official game. Full notice: [NOTICE.md](NOTICE.md).

## License

The OpenWheels source code is [MIT](LICENSE), © 2026 officialmelon. It covers only the OpenWheels
code, not Happy Wheels or its assets.
