# Changelog

## Unreleased

### New
- **Game controllers on every platform.** Windows and Linux (Xbox and PlayStation pads, any pad
  remappable), macOS, Android (Bluetooth / USB pads and handhelds' built-in controls such as the
  AYN Odin / Thor) and iOS. The controller drives through the same virtual fingers as the
  keyboard, and every driving action is remappable with three inputs each (QOL "controller").
  See [docs/CONTROLLERS.md](docs/CONTROLLERS.md).
- **Full menu control with a controller.** A glowing highlight moves between the buttons of every
  screen (menus, level and character select, pause / victory, popups, the online browser, Your
  Levels, Nearby, account panels); A presses, B goes back or closes, LB / RB turn the chapters,
  the right stick scrolls, R3 gives a free pointer (level editor).
- **Rumble** on crashes, hits, broken bones, lost limbs and explosions, by strength and distance
  (off / low / medium / high). Android: the controller's vibrator or, for built-in controls, the
  device's; "phone vibration" (off by default) for touch play.
- **Tilt steering** on Android and iOS (off / low / medium / high): turn the device to lean.
- **Touch controls** can be hidden on phones and tablets too, and hide by themselves while a
  controller is connected.
- **User vehicles on touch screens:** riding a browser user vehicle shows SHIFT / CTRL buttons for
  its assigned actions and an eject button, which only existed as PC keys.

### Fixed
- **Jets on user vehicles (jetpack levels) didn't fire** when the vehicle had come to rest: a
  sleeping jet ignored its key. The jet key now wakes them.
- **Restored characters' extra buttons** (Helicopter Man's rope up / down, Explorer Guy's stand /
  crouch, Irresponsible Mom's kids) were covered by the special button when the special button was
  set to the left side, so they couldn't be pressed (touch or keys). They now sit above it.
- Helicopter Man's rope always has at least 2 m to let out.
- **Swords in grouped objects couldn't be grabbed** (some sword fights): the original put the
  handle's collision box at the weapon's centre instead of under the drawn handle, so the blade
  hit but the handle had nothing to hold. On browser levels, user levels and the editor's test
  play it now sits where it is drawn (the campaign keeps the original's box).

## v0.3.0

### New
- **Linux, macOS and iOS.** Native Linux (x86_64) and macOS builds (`tools/fetch_engine.sh`,
  `tools/build.sh`) and an iOS app (Xcode project from `tools/build.sh --ios`; game files bundled
  or copied onto the device with Finder). See [docs/DESKTOP.md](docs/DESKTOP.md) and
  [docs/IOS.md](docs/IOS.md).
- **Restored character campaigns.** Lawnmower Man, Explorer Guy, Santa Claus, Irresponsible Mom and
  Helicopter Man each get a chapter of six new levels made for OpenWheels, with their own
  main-menu portraits (rendered from your own copy of the browser game, or from the already
  generated sprites).
- **Re-grab your vehicle.** After ejecting, grab your own vehicle and you climb back on and keep
  riding, as often as you like, with all 11 characters (QOL "re-grab vehicle", on by default).
- **A real PC port.** The window is resizable, starts maximized and goes fullscreen (F11) at the
  monitor's resolution with no black bars at any aspect ratio. The mobile touch buttons are hidden
  on PC (QOL "touch controls"); key hints replace the tutorial arrows and label the restart
  button; held keys carry over when the controls change (hold Space through an eject to grab);
  Esc and R work right after a death.
- **Browser physics for online levels.** Browser levels step like the Flash game: one 1/30 s step
  per frame, its iterations and contact solver (no block solver). Balances, stacks, "don't move"
  levels and joint stiffness behave as in the browser, and browser replays are re-simulated one
  input per step (QOL "browser physics", off by default, online levels only).
- **Online levels:** change character on levels that force one (QOL "any character"), NEXT plays
  the next level of the browser list, and leaving a level returns to the browser or Your Levels.

### Fixed
- **Browser physics only for online levels.** Restored campaign and editor levels no longer run at the
  browser step, and the option is off by default. The "select a character" chapter is now last.
- **Missing scenery everywhere:** every static polygon and art shape (trees, roots, cliffs,
  windows, walls...) was drawn far off-screen. The swamp level alone lost 262 shapes.
- **Neon / outline levels showed black:** shape outlines are drawn, outline-only shapes are kept,
  concave polygons draw with their real outline, circle cutouts work, and polygons whose
  triangulation fails get a fallback fill instead of vanishing.
- **Vehicle miniguns didn't fire:** arrow guns on user-built vehicles fire straight on Space; every
  fixture of a vehicle's handle can be grabbed; Space reaches user vehicles in every layout.
- **Random "death" with no controls** on deep browser and user levels: the fall-off check now
  follows the level's geometry instead of a fixed height.
- Your chosen character is no longer replaced by a level's forced character; stale browser-level
  state no longer leaks into character select.
- Timers and per-step values of the restored vehicles, browser items, particles and passengers
  follow the physics step.

## v0.2.1

### Fixed
- **Ghost racing:** a player who had opened the Race screen (and so was hosting an empty lobby)
  answered every invite "busy", so two players who both tapped Race could never connect. An
  empty lobby now accepts invites, and joining closes it.
- A race invite popup that disappeared because of a screen change no longer leaves the device
  stuck as "busy".

## v0.2.0

### New
- **Ghost racing (local multiplayer).** Race friends on the same Wi-Fi on any campaign level,
  your own levels or online levels. Other players appear as tinted, see-through ghosts behind you,
  showing their ragdolls, lost limbs, gore and broken vehicles for all 11 characters. Race HUD,
  finish banner, results with rematch. See [docs/RACE.md](docs/RACE.md).
- **Send levels to nearby players.** "Send to Nearby" in Your Levels, the editor and the online
  browser; the receiver gets an Accept / Decline prompt and can play right away. A short receive
  code works when automatic discovery is blocked. See [docs/NEARBY.md](docs/NEARBY.md).
- **Full level editor.** Every browser-game feature: NPCs, text boxes, signs, food, furniture,
  cannons, chains, paddles, tokens, buildings, the restored characters, the city background,
  polygon and art shapes, triggers with per-target action lists, pin and sliding joints, groups and
  user-built vehicles. New inspector and item palette, full mouse and keyboard editing on PC
  (box select, copy/paste, undo/redo, nudge, zoom, pan), and **Edit** on online levels to remix
  them. Levels save in the browser format; old editor levels still open.
- **Totaljerkface replays and account.** Browse a level's replays and record times, watch replays,
  record / save / upload your own runs and rate replays. Log in with your totaljerkface.com
  account for favorites, level ratings, My Levels / My Replays and publishing your editor levels
  (always confirmed first). Browser replays store only key presses, so they are re-simulated and
  marked approximate; OpenWheels' own replays play back exactly.
- **Quality of Life:** 30 / 60 FPS, forced texture quality, separate sound-effect and music
  volume sliders, and a remappable controls page (two keys per action).
- **Your Levels** redesigned in the modern online-browser style.

### Fixed
- Restored characters in browser levels with "hide vehicle" now spawn as themselves on foot.
- Online levels: NPC grinding clips into the mower deck, old (1.8 and earlier) trigger counters,
  the city background with camera zoom, CLICK PARKOUR 3's intro screen.
- Android 64-bit ARM build (the replay upload's encryption no longer uses OpenSSL's AES).
- The editor's "+ Add action" button was unreadable.

### Notes
- Account features (login, favorites, rating, uploading, publishing) were developed against a
  local mock of the site's protocol; please report any problem with the real site.
- Windows Firewall asks for permission the first time Send to Nearby or racing is used.

## v0.1.0
First public release: the 1:1 reconstruction of Happy Wheels mobile, the iOS level editor port,
online browser levels, the five restored browser characters, the Quality of Life page,
Child Gore, no ads, Windows and Android builds.
