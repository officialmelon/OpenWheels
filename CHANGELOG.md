# Changelog

## v0.5.3

### Fixed
- **iPhone still had no sound effects after v0.5.2** (#38). The script that makes the AAC copies of
  the sounds (iOS cannot play Ogg Vorbis) converted only the first file: `ffmpeg` read the list of
  files from the same input as the loop and swallowed it, so the iOS package held 1 `.m4a` for 402
  `.ogg`. Fixed (`ffmpeg -nostdin`); the script now fails unless every `.ogg` has an `.m4a`, the
  generated (restored-character and browser) sounds are converted too, the release workflow checks
  the finished iOS package has an `.m4a` for every `.ogg`, and a test (`tests/test_aac_script.sh`)
  covers the script.

## v0.5.2

### Fixed
- **"Unreadable level list" in the online browser** (#29, #31, #32). The site now and then sends a
  level list that is not well-formed XML (a stray Latin-1 byte in a level name, a bare `&`, control
  characters), and a phone may hand the game a reply still gzipped or with a BOM. The list is now
  cleaned up before it is read (gzip, BOM, invalid UTF-8, stray characters), repaired when it still
  does not parse, and as a last resort read level by level, so one bad level no longer blanks the
  whole list. If a reply is truly unreadable the message now says what arrived (HTTP status, size
  and its first characters) so it can be diagnosed. Downloaded level records are cleaned the same
  way.
- **No sound effects on iPhone** (#33). iOS cannot play Ogg Vorbis, which all sound effects are
  (only the `.mp3` music played). The iOS build now carries an AAC copy of every sound and uses it.

### New
- **Pogostick Man on touch screens: jump + pose buttons** (#30). The pose buttons are now
  **JUMP+LEGS** and **JUMP+SQUAT**: hold one to crouch with that pose, release it to jump. QoL ->
  *pogo touch buttons* switches back to pose-only buttons.
- **Tests and CI.** Unit tests for the site protocol (level lists, records, replies, search,
  versions), a version / changelog consistency check, and headless smoke tests that play synthetic
  levels in the real game (a level can be won, triggers hide text, click triggers, the Bad Apple!! 2
  tricks, the online browser, and grabbing a level from a mock site and playing it). They run on every
  push, and the release workflow runs them before it publishes. See `tests/README.md`.

## v0.5.1

### Fixed
- **Online levels, login and replays on phones.** Cloudflare challenged the Windows desktop
  User-Agent used by v0.5.0 with an HTTP 403 "Just a moment..." page on Android, even though the
  same UA works on PC; the Android HTTP stack and the desktop UA did not match. Android now sends a
  mobile Chrome User-Agent and the browser's `Accept` / `Accept-Language` / `Origin` /
  `Sec-Fetch-*` headers (and gzip/deflate instead of `identity`); iOS uses a mobile Safari
  User-Agent. Verified on an Android 14 emulator: the old desktop UA got
  `cf-mitigated: challenge`, the mobile UA got HTTP 200 XML.

## v0.5.0

### New
- **Update check.** On start-up the game asks GitHub (one request to `api.github.com`, nothing about
  you is sent) for the latest OpenWheels release; if it is newer than the version you run, the
  main menu offers **Download** (opens the release page) or **Later** (not asked again for that
  version). Turn it off with **QoL -> check for updates**.

### Fixed
- **Online levels, login, replays and account features work again.** The site's Cloudflare WAF now
  challenges the old `OpenWheels/1.0` User-Agent with an HTTP 403 "Just a moment..." page, which
  broke every request. Requests now use the official desktop client's browser/Electron
  User-Agent (including its HappyWheels version token), so level lists, search, featured levels,
  metadata, downloads, login and replays are served normally again.
- **Version/build fix:** restored `src/qol/AppVersion.h`, which had been left empty, so the running
  version is compiled into the game and the update check can compare releases.

## v0.4.7

### Improved
- **Online levels / login errors now say what actually happened.** The game reports the HTTP status
  and tells a blocked request (the site's Cloudflare "Just a moment..." page) apart from a plain
  network failure, in the level browser, the login screen and the account services, instead of
  "Server returned an error page" / "System error". If online levels still fail for you, the
  message now includes the cause (for example `HTTP 403` or the network error text): please report
  it in an issue.

## v0.4.6

Ports of the browser game's Shift / Ctrl behaviours for the original characters, checked against
its character classes (control bits 0x20 / 0x40; on touch screens each has a labelled button, and
the keys work through them):

### New
- **Wheelchair Guy:** riding, Shift / Ctrl **turn the jet** clockwise / counter-clockwise (0.1 rad
  per frame, the jet's thrust and sprite follow). Ejected, they are the two **wheel poses**.
- **Segway Guy:** riding, Shift = straight legs, Ctrl = squat; ejected, the two **fist poses**.
- **Pogostick Man:** ejected, Shift / Ctrl = the **fist poses** (riding poses came in v0.4.5).
- **Moped couple:** Shift lets the **girl** go, Ctrl **brakes** (both wheel motors on at speed 0).

### Fixed
- **Irresponsible Dad:** corrected to the browser's keys: **Ctrl** throws off the kid (was Shift)
  and **Shift** ejects Dad himself, like Z. The touch button is now KID on the Ctrl bit.

## v0.4.5

### New
- **Pogostick Man: Shift and Ctrl poses** (#22), ported from the browser game's `PogoStickMan`
  class: while riding, **Ctrl squats** (knees bent to 2.5 rad) and **Shift straightens the legs**
  (0.25 rad); released, the legs return to the normal pogo pose (1.2 rad). Ctrl wins if both are
  held. Touch screens get **LEGS** and **SQUAT** buttons above the lean buttons.

## v0.4.4

### Fixed
- **Online levels failing with "Server returned an error page" / "System error"** (#23, #25, #26,
  #27). The game sent a browser User-Agent from libcurl, which the site's bot protection (Cloudflare)
  answers with a 403 "Just a moment..." page. It now identifies itself honestly as OpenWheels, and
  when the site does block a request the message says so instead of "error page". If your network
  is still blocked, try another connection; this is the site's decision, not something the game can
  force.
- **Crawling / ejected controls mirrored on PC and controllers** (#24): the left key or pad
  direction now presses the left button of the ejected layout (arch) and the right one the right
  button (push-up), as in the vehicle layout.
- **Menu music kept playing over online, editor and nearby levels** (#22); it now stops whenever a
  level starts.
- **Accented text boxes** (Turkish ğ ş İ and other non-ASCII letters, #18): browser text boxes
  whose caption has such letters use Arial, which has them, instead of the embedded Helvetica
  subset.

### New
- **Irresponsible Dad: throw only the kid off** (#22): Shift on PC, a **KID** button on touch
  screens (shown while the kid is on the bike).

## v0.4.3

### Fixed (online levels)
- **Bad Apple!! 2 and other levels built from chained triggers now run.** The level is a video made
  of 4,000 pixel shapes driven by 30,000 triggers, and three things broke it:
  - Trigger-to-trigger links without an action list (`<t i="N"/>`, as written by level scripts)
    were dropped by the converter, so no chain ever ran; they now activate the target, like the
    live game.
  - A repeat interval saved as `NaN` (the video's frame clock) was turned into 0.1 s; Flash
    reads it as 0 frames, so the trigger fires every frame (30 fps).
  - A start position of `NaN` (also saved on purpose): Flash freezes the character and shows the
    stage's top-left corner, where the video is; the camera now does the same, and the frozen
    character counts as touching every sensor trigger (which is what starts the video).

## v0.4.2

### Fixed
- **Jets on user-built vehicles produced no thrust.** A vehicle's jets are held back until the jet
  key is pressed, and the first idle frame zeroed the thrust ramp of every jet without a ramp time
  (the default), so they never pushed again. Jets without a ramp now keep full thrust; ramped jets
  behave as before.

### New
- **Switch camera** (key **C**, remappable under Controls) between your character and the ones
  travelling with it: Irresponsible Dad's kid, the moped couple's girl, Santa's elves and
  Irresponsible Mom's kids. On touch screens a **CAM** button appears next to pause whenever
  there is more than one character to follow.
- **New launcher icon:** an OpenWheels wheel on the menu blue (adaptive and themed icon on
  Android 8+/13+; used when the original game's icon is not part of the build).

## v0.4.1

### Fixed
- **Android: the on-screen keyboard now opens in the level editor** (#16). Tapping a value field in
  the inspector or the level text box focused it but never raised the soft keyboard (seen on an
  Honor Pad X8a, Android 16); it is now requested on focus and closed again when the field is left.
- **Multi-word level searches work** (#15). The online site answers a search such as "Spinning
  Gears" with nothing; the game now falls back to the longest word and keeps the levels that
  contain every word, in any order and case.

### Changed
- **The restored characters' campaign chapters show "COMING LATER"** (Lawnmower Man, Explorer Guy,
  Santa Claus, Irresponsible Mom, Helicopter Man), like the motorcycle man and lady, instead of
  their levels. The level files stay in the repository for when they are opened again
  (`kCampaignComingSoon` in `src/restored/Restored.cpp`).

### Improved
- **"What's new" toast:** after updating, the main menu shows a one-time note listing these changes.
- **Online level timing and text:** trigger delays no longer fire one physics step late from float
  drift (0.5 s delays were summed as 29.99999 steps), text-box fade / slide frames are rounded
  with a safer margin, and large text (up to 160 pt) is rendered at full size instead of being
  scaled up from 72 pt (blurry titles).
- Chapters without levels no longer produce an invalid (NaN) completion percentage.
- Local (fixture) level searches also match every word of the term.

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
