# Features

A tour of everything OpenWheels does, with screenshots. The short version is in the
[README](../README.md).

## A 1:1 reconstruction of the mobile game

Every class of the original `libMyGame.so` was rebuilt with its original names, signatures and
behaviour. That covers the characters and ragdolls, vehicles, level loading, items and triggers,
gameplay, menus and windows. Parity is measured, not assumed: the original game runs in an arm64
emulator (the "oracle"), and the reconstruction's Box2D world is compared with it body by body.
**All 73 campaign levels load into a world identical to the original's.** Over long runs the
physics drifts slightly, because MSVC rounds floats differently from the original's clang build;
see [PARITY.md](PARITY.md).

![A campaign level](screenshots/campaign.png)

## Eleven playable characters

The six mobile characters plus the five that only existed in the browser game, all in character
select. Each restored character has his or her own vehicle, controls, gore and sounds:
**Lawnmower Man**, **Explorer Guy**, **Santa Claus**, **Irresponsible Mom** and **Helicopter
Man**. They are rebuilt at build time from your own copy of the browser game's character files;
see [RESTORED.md](RESTORED.md).

Each of them also gets **a campaign chapter of their own**: six new levels per character, made
for OpenWheels, after the original chapters in the level select, with their own main-menu
portraits. See [RESTORED.md](RESTORED.md#restored-character-campaigns).

![Character select with all 11 characters](screenshots/character-select.png)

![The restored characters](screenshots/restored-characters.png)

![Helicopter Man](screenshots/helicopter-man.png)

## Level editor

The iOS version had a level editor that Android never got. OpenWheels ports it and extends it with
**everything the browser game's editor can do**: NPCs, text boxes, signs, food, furniture, cannons,
chains, paddles, tokens and buildings; polygon and art shapes; **triggers** with per-target action
lists; pin and sliding **joints**; **groups** and user-built **vehicles**; all 11 characters and the
city background. On PC it has full mouse and keyboard editing (box select, copy/paste, undo/redo,
nudge, zoom, pan), and any online level can be opened with **Edit** to remix it. See
[EDITOR_PORT.md](EDITOR_PORT.md).

![Level editor](screenshots/level-editor.png)

![Editing a trigger](screenshots/editor-triggers.png)

## Your levels, sent over Wi-Fi

Your Levels keeps everything you've built or received. **Send to Nearby** sends a level to anyone
running OpenWheels on the same Wi-Fi (PC or phone); they get an Accept / Decline prompt and can
play it straight away. See [NEARBY.md](NEARBY.md).

![Your Levels](screenshots/your-levels.png)

## Ghost racing

Race friends on the same Wi-Fi on any campaign, user or online level. Everyone rides their own
world; the other players appear as tinted, see-through **ghosts** behind you, complete with
ragdolls, lost limbs, gore and broken vehicles. Race HUD, results and rematches. See
[RACE.md](RACE.md).

![Ghost racing](screenshots/ghost-race.png)

## Online levels from the browser game

Search and browse the browser game's user levels on totaljerkface.com (top rated, most played,
newest and more), then download and play them. Levels are decrypted and converted to the mobile format on the
fly. The browser-only features are implemented too: NPC characters, text boxes, triggers,
furniture and other items, user-built vehicles, the city background and the browser game's sounds.
See [FLASH_LEVELS.md](FLASH_LEVELS.md).

Each level also has its **replays and record times**: watch them, record and upload your own runs,
and rate them. **Log in** with your totaljerkface.com account for favorites, ratings, your levels
and replays, and publishing levels made in the editor.

![Online level browser](screenshots/online-levels.png)

![An online level with NPCs and text](screenshots/online-level-npcs.png)

## Quality of Life options

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
- **Keyboard controls** (PC): remap every keyboard action
- **Controller**: remap every controller action, rumble strength, tilt steering and phone
  vibration
- **Touch controls**: hidden by default on PC, where the keyboard drives and key hints replace
  the tutorial arrows; shown on phones and tablets unless a controller is connected
- **Re-grab vehicle:** after ejecting, grab your own vehicle and you climb back on and ride again
  (all 11 characters)
- **Any character** on online and user levels that force one
- **Browser physics:** online levels step like the browser game (30 Hz, its solver settings), so
  balances, "don't move" levels and browser replays behave as in Flash

See [QOL.md](QOL.md).

![Quality of Life page](screenshots/quality-of-life.png)

![Classic and realistic blood](screenshots/blood-styles.png)

## Game controllers

Play with a controller on every platform: Xbox and PlayStation pads on Windows and Linux, any
Android controller (including the built-in controls of handhelds like the AYN Odin and Thor), and
MFi / Xbox / PlayStation pads on iOS. The controller drives, and it moves through **every menu**
with a glowing highlight on the selected button (A presses it, B goes back). R3 switches to a free
pointer for the level editor. Every action is **remappable** (three inputs each). It also has
**rumble** that follows every crash, broken bone and explosion (off / low / medium / high), and
**tilt steering** on phones, tablets and handhelds. See [CONTROLLERS.md](CONTROLLERS.md).

## Windows, Linux, macOS, Android and iOS

OpenWheels runs on Windows (Win32), Linux (x86_64) and macOS with mouse and keyboard, and on
Android and iOS phones and tablets with any aspect ratio. On PC the window is resizable, starts
maximized and goes fullscreen (F11) at any aspect ratio without black bars. See
[DESKTOP.md](DESKTOP.md) and [IOS.md](IOS.md).

![OpenWheels on an Android phone](screenshots/android-phone.png)

## Controls

On touch screens the controls are the same as the mobile game's. On PC, the mouse acts as touch
and these keys work by default (remap them in *Options → quality of life → controls*):

| Key | Action |
|---|---|
| Up / W | Forward (accelerate) |
| Down / S | Back (brake / reverse) |
| Left / A | Lean back |
| Right / D | Lean forward |
| Space | Special action (jump, jet, brake, fire a vehicle's guns; grab when ejected, grab your vehicle to get back on) |
| Z | Eject |
| Shift / Ctrl | Character actions (restored characters, e.g. Helicopter Man's magnet) |
| Esc / P | Pause |
| R | Restart |
| F11 | Toggle fullscreen |

With a game controller: sticks / d-pad drive and lean, RT / LT accelerate and brake, A is the
special action, Y ejects, X / B and LB / RB are the extra actions, Start pauses and Back restarts.
In menus, the d-pad / left stick moves the highlight, A selects and B goes back. Remap them in
*Options → quality of life → controller*. See [CONTROLLERS.md](CONTROLLERS.md).
