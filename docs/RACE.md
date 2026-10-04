# Ghost Race (local multiplayer)

Two to four players on the same Wi-Fi race the same level at the same time. Everybody rides in
their own world; the other riders show up as **ghosts**: the same character art, see-through
(55 %), tinted in the player's colour, with a name tag, drawn behind your own rider. Ghosts copy
everything the other player sees happen to their rider: ragdolls, broken joints, lost limbs, the
swapped damage frames, brains, hearts, gore chunks, intestines, broken and separated vehicle parts,
kids and elves, the mower deck and the helicopter's rotor and rope. Campaign levels, your own
levels and online levels all work. PC <-> PC, Android <-> Android and PC <-> Android use the same
protocol. There are no accounts or servers.

Code: `src/net/race/`. Hooks elsewhere are marked `// NET (PC addition):`. It reuses the LAN
discovery and the framed TCP channel of Send to Nearby ([NEARBY.md](NEARBY.md)).

## Playing

* **Hosting.** Main menu → the pink button with the checkered flag (**Race**). The lobby opens
  with a campaign level picked (chapter and level dropdowns). To race one of your levels or an
  online level, open it in **Your Levels** (**Race** next to Send to Nearby) or the **online
  browser** (**RACE** next to Send to Nearby; the level is downloaded and converted first).
* **Inviting.** The lobby lists the OpenWheels players found on the network; press **Invite**.
  If someone does not show up, type their receive code (Your Levels → Receive) into
  "Not listed? Their code". A race has at most four riders.
* **Joining.** The invited player gets "*Name* invites you to a ghost race on “*Level*”" with
  Join / Decline (during gameplay it waits until the game is paused). Joining opens the lobby.
* **Lobby.** Every rider is listed with their colour, rider and ready state. Pick your rider with
  `<` `>` (or the arrow keys) unless the level picks it (campaign chapters always do; user and
  online levels may). Guests press **Ready!**; the host presses **Start Race** once everyone is
  ready. The X closes the lobby: for a guest that leaves the race, for the host it ends it.
* **Start.** The host sends the level: campaign levels by chapter and level, your levels as their
  XML, online levels as the converted XML (guests need no internet and no download). Each game
  loads it and holds it frozen until all are loaded, then everyone sees the same 3-2-1-GO.
* **Racing.** The HUD (top left) shows the race clock and every rider's colour, progress towards
  the finish line and state (Ejected, Dead, the finish time, Gave up, Left). Restarting (reset
  button, R, or changing the rider from the pause menu) is allowed; the race clock keeps running
  and your ghost restarts on everyone's screen. **Give Up** ends your race; the host also has
  **End Race** (everyone still riding gets DNF). Leaving the level through the pause menu counts as
  giving up. Reaching the finish shows "Finished 2nd! 0:31.42" instead of the victory menu; you
  can keep riding and watching the ghosts.
* **Results.** When every rider has finished, given up or left: places, times and the gap to the
  winner. The host picks **Rematch** (same level, straight to the countdown), **Lobby** (everyone
  back to the main menu and the lobby) or **Close Race**; guests can **Leave Race**.
* **Disconnects.** A guest who quits or loses the connection fades out ("*Name* left the race")
  and counts as Left. If the host goes away the guests get "Race Over" and keep playing alone.

## How ghosts work

**Capture (GhostCapture).** Every playable character (the six originals, the five restored
browser characters, the bare characters of "hide vehicle" browser levels) and every vehicle draw
only into the Session's five rig layers: character background / midground / foreground (z 6, 8,
11) and vehicle background / foreground (z 7, 12). Gore pieces, wounds, kids, elves, broken parts,
the mower's pieces and the helicopter's rope all land there too; nothing else does except the
browser levels' NPCs, whose nodes are named `ow_npc` (set in `NPCharacter`). So no per-character
code is needed: a snapshot is every node below those layers. Each node gets a stable id while it
exists (the capture retains it, so a freed address is never confused with a new node), and is
recorded with its parent (a layer or another node: the helmet on the head, wounds on the chest),
its draw order among its siblings, sprite frame, position, rotation (and skew), scale, anchor,
flips, visibility, opacity and colour. DrawNodes (the helicopter's rope, the sleigh's reins) send
their content at most ~12 times a second and only when it changed: as segments when it was drawn
with `drawSegment` (recognised from the 18-vertex pattern it writes, redrawn with `drawSegment`),
as plain triangles otherwise.

Sprite frames are sent by **name**, once, then by index. The capture finds the name by looking
the sprite's texture and texture rectangle up in the SpriteFrameCache (a reverse index, rebuilt
when the cache changed), with the plist it came from relative to the search paths. Names are
tier-independent (tiny/small/medium/large textures differ per device), so a phone and a PC
exchange the same names; positions are in design points, also the same everywhere. Sprites cut
from part of a frame (`createWithTexture(rect)`) are sent as frame + sub-rectangle; whole-image
sprites as their file path.

**Wire format** (one `s` message of the race channel, field `d`; little-endian, `var` = LEB128,
`svar` = zig-zag):

```
u8 format (1)  var run  var seq  f64 time (sender clock, s)  f32 focusX, focusY (points)
u8 flags (1 = keyframe)
var images { var id, u8 kind (1 frame, 2 frame+rect, 3 file), str name, str plist, [f32 x,y,w,h] }
var nodes  { var id, var mask, [new: u8 kind], [parent var], [rank svar], [image var],
             [pos: svar dx,dy, 1/8 pt], [rot: svar d, 1/32 deg], [skew svar], [scale svar x,y,
             1/1024], [anchor svar x,y], [flags u8 visible|flipX|flipY], [colour u8 a,r,g,b],
             [draw: var n<<1|segments, u8 rgba, then per vertex svar dx,dy (1/4 pt) or per
              segment svar a, b, var radius, u8 new colour (+ rgba)] }
var removed { var id }
```

Only what changed since the previous snapshot is sent (the channel is TCP, so deltas are safe).
A restart is a new `run`: its first snapshot is a keyframe. Snapshots go out 20 times a second
(every third frame at 60 FPS).

**Ghost (GhostView).** `GhostTrack` decodes a player's snapshots into the full node state and
keeps ~1.5 s of them with the sender's timestamps; the clock offset is the smallest
"received − sent" seen. `GhostLayer` is a child of the Session at z 5: in front of the level's
shapes and items, behind every layer of the local rider. Each frame it draws every track 120 ms in
the past, interpolating positions, rotations and scales between the two snapshots around that
time; frames, draw order, parents and visibility follow the older one. Sprites are multiplied by
the player colour (red, blue, green, yellow by join order) and drawn at 55 % opacity; a Clarendon
name tag follows the rider's camera focus. Ghost art that is not loaded yet is loaded from the
plist named in the snapshot (racing as Business Guy against a Helicopter Man loads Helicopter
Man's sheet). Ghosts have no bodies and never touch your world.

**Bandwidth** (measured on PC with the race log line `race: ghost upload`, 20 snapshots/s,
including about 32 bytes of framing per snapshot): Irresponsible Dad 1.5-3.6 KB/s (32 nodes),
Lawnmower Man 1.3-2.8 KB/s (18), Helicopter Man 3-4 KB/s (22, rotor and rope),
Irresponsible Mom 2-5 KB/s (18-48: more after a crash with gore), Santa Claus 10-12 KB/s (83:
sleigh, reindeer, elves, reins). The host also relays every guest's stream to the other guests,
so with four riders it sends about three streams per guest. Well below what Wi-Fi carries.
## Protocol

The race reuses Send to Nearby's discovery (the beacon's services become `levels,race`) and its
TCP listener: a `hello` whose `purpose` is `race` is handed from `LevelTransfer` to
`race::RaceSession`. The host keeps one channel per guest (a star) and relays.

| from | message | fields |
|---|---|---|
| host | `hello` | app, proto (1), purpose=race, name, dev, plat, id |
| guest | `hello` | the same |
| host | `invite` | level, kind (campaign / mobile / online), host, players |
| guest | `answer` | accept, reason (declined / busy / timeout), plat |
| host | `welcome` | you (player id) |
| host | `lobby` | phase, kind, name, chapter, level, forced, fchar, host, players (one line per player: id, name, platform, colour slot, character, ready, status, time, place) |
| guest | `pick` / `ready` | char / ready |
| host | `load` | the level fields + xml (user and online levels), race |
| guest | `loaded` | |
| host | `go` | delay (ms; the countdown) |
| both | `s` | d (snapshot), f (sender id, added by the host when relaying) |
| guest | `st` | status (racing / ejected / dead / finished / gaveup), time (ms) |
| host | `results`, `left` {id, name}, `end` {reason} | |
| guest | `bye` | |

Statuses come from the game's own events (`characterEjected`, `characterDead`,
`levelCompleted`) and from the scene (a new Gameplay = a restart, the main menu = gave up).
Finish times are the race clock (from GO, wall time) at `levelCompleted`. Progress is the rider's
camera focus projected on the line from the start to the level's finish line (no bar for levels
that finish through triggers or tokens).

## Game hooks

* `MainMenu`: the Race button (tag 6) → `race::openRaceMenu()`.
* `Gameplay::handleLevelComplete`: no VictoryMenu while racing (`race::suppressVictoryMenu()`);
  the sound, the controls' victory state and the hidden timer are unchanged.
* `LevelTransfer`: race hellos are handed over; `race` is announced.
* `NPCharacter`: NPC nodes are named `ow_npc`.
* `UserLevelsScreen`, `OnlineLevelBrowser`: "Race on this level" buttons.
* `OnlineUi`: the checkered-flag icon.
* Everything else (freezing the level until GO, the HUD, the ghost layer) is done from outside:
  `Node::pause()` on the Gameplay, children added to the Gameplay and the Session. Single-player
  is unchanged (campaign parity 73/73).

Test hook (PC): `--race-test host:campaign:<chapter>:<level> | host:level:<xml> |
host:online:<browser xml> | join`, with `--race-players N`, `--race-char <id>` and
`--player-name <name>`: the host invites whoever it finds and starts once N riders are ready;
`join` accepts invites and gets ready. Two or three copies of the exe under different names (each
gets its own profile and its own listen port) race each other on one PC.

## Limitations

* Blood particles are not sent (wounds, blood-stained damage frames and gore pieces are).
* The mower's grinding mask (a ClippingNode around a limb in the deck) is not copied: ghost limbs
  in a ghost mower are drawn whole.
* Ghosts are translucent per sprite, so overlapping parts of one ghost show through each other.
* A player name changed after joining is not updated in the race.
* A rider the other side does not have (a restored character not generated there) is invisible
  as a ghost; the lobby still lists it.
* Finish times use each device's wall clock from its own GO (the GO message arrives within a few
  milliseconds on a LAN).
