# Changelog

## v0.4.0

### New
- **Rebuilt restored-campaign levels: Lawn and Order, Idol Hands and Lift Off.** Each is now a
  full-length level in the style of the original campaign (layered scenery, invisible collision
  under detailed art, no floating text) with set pieces driven by triggers:
  - *Lawn and Order:* the neighbours' sunbathers and picnic, Lemonade Hill, a kiddie pool whose
    rims catch the deck unless you lift it, a seesaw over the koi pond, and Mrs. Henderson, who
    swoons when you knock her prize pumpkin off its stand. Finish at the HOA office.
  - *Idol Hands:* a jungle camp and a carved temple face; strike the idol hanging from a giant
    stone hand and the door grinds open; a boulder drops behind you on the long descent and
    chases you to a low arch; aiming dart guns, a crystal hall with glass and a spike pit crossed
    on a rail, two swinging axes, a gallery that caves in behind you, and a sunset escape.
  - *Lift Off:* carry a crate with the magnet to Bay 1 to open the hangar door, take the engine
    crate to Bay 2 to open the main door, then fly out under the power lines to the tower helipad.
  Screenshots and the test report are in `artifacts/level-review/`.
- **Tutorial arrows in the restored campaigns.** Lawn and Order, Idol Hands and Lift Off show the
  game's own control arrows to teach each restored character's controls; browser-format levels
  keep their system triggers when converted.
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
- **Browser physics, closer to the browser game and smooth.** Online levels now use the browser
  game's Box2D 2.0 rules: its solver (contacts, joints, damping, speed limits, sleep), its
  sweep-and-prune broad-phase, continuous collision and contact points, on top of its 30 Hz step.
  Characters and vehicles use the browser's exact shapes, level shapes are built in its order, and
  fans, boost panels, harpoon guns, explosions and NPC break limits behave as in Flash. Checked step
  by step against the HTML5 client on real replays. Levels are drawn smoothly at your display's
  frame rate instead of moving at 30 fps, and replays of your own runs play back exactly. On by
  default for online levels (QOL "browser physics"); campaign, restored and editor levels keep the
  mobile physics. Box2D is now built from source (`thirdparty/box2d`).
- **Main menu fits small screens.** The button row shrinks, scales or wraps to two lines on 4:3,
  16:10 and narrow windows instead of running off the edge, and the logo shrinks on very narrow
  windows. A credit line under the logo and an OpenWheels section in the credits.
- **macOS and iOS packages with every release.** `OpenWheels-macos.zip` (Intel, runs on Apple
  Silicon through Rosetta) and `OpenWheels-ios-unsigned.ipa` for sideloading, built by the release
  workflow alongside Windows, Android and Linux.

### Fixed
- **Lawnmower Man died on his own mower:** the mower's hull was in a different collision group
  from its rider, so a bump smashed his chest against the hood. The hull now shares his group while
  he rides.
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
- **Browser physics only on online levels:** restored campaign and editor levels no longer run at
  the browser step.
- The "select a character" chapter is now the last chapter in level select.
- **Online browser:** the search field no longer covers the account button on 4:3 screens.
- **Controls page:** the table and hint fit narrow windows.
- A rare crash when closing the level editor's inspector.
- **Android:** the APK is signed with the OpenWheels release key again, so it installs over
  v0.3.0. Test builds from the workflow install alongside it as "OpenWheels beta"
  (`org.openwheels.game.beta`).
- Builds: macOS (Xcode 16 SDK), iOS (CMake 4, Xcode 15+) and Windows (MSVC) release builds compile
  again; engine patches stay byte-exact on Windows checkouts.

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
