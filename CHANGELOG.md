# Changelog

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
