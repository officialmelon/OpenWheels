# Send to Nearby (levels over Wi-Fi)

Players on the same network can send each other levels: your brother builds a level, taps
**Send to Nearby**, picks you, you accept, and it is in your levels. PC ↔ PC, Android ↔ Android
and PC ↔ Android all work. There are no accounts and no servers; everything stays on the local
network. Code: `src/net/`; hooks elsewhere are marked `// NET (PC addition):`. The restyled
user-level screen is `src/editor/persistence/UserLevelsScreen.*` (`// UI (PC addition): restyled`).

## Using it

* **Sending.** Main menu → grey play button (Your Levels) → pick a level → **Send to Nearby**.
  Also in the editor menu (**Send Nearby**, the level as it is in the editor, like Share Level)
  and in the online browser for downloaded levels (**Send to Nearby** next to the author's
  comment). The panel lists the OpenWheels players found on the network with their name and
  device; pick one and press **Send**. The progress ("Waiting for Little Bro to accept…") and the
  answer show above the button.
* **Receiving.** Nothing to do: while the game runs, an offer pops up anywhere ("Alex wants
  to send you “Dungeon”" — Accept / Decline). During gameplay the popup waits until the game is
  paused or the level is left. Accepted levels are saved under **Your Levels → Received** (the
  imported-levels chapter, also listed in the editor's Load Level), then "Play Now?" offers to
  start it.
* **Your name** is shown to others ("You appear as …" in both panels; UserDefault
  `net_player_name`, default: the computer name or phone model).
* **Receive codes.** If the other player does not show up (a router that blocks broadcasts, a
  guest network, an emulator), open **Your Levels → Receive**: it shows a code such as
  `R2M0-3Q0B`. On the sending side tap **Not listed? Use a code** and type it. A plain
  `192.168.1.20` or `192.168.1.20:47812` works too.

## Protocol

**Discovery** (`LanDiscovery`, generic): every instance sends a beacon every 1.5 s to UDP port
**47810**: the multicast group 239.255.77.81 (once per interface), every interface's directed
broadcast, 255.255.255.255 and 127.0.0.1. The beacon is text, one `key=value` per line after the
line `OWLAN`: `t` (beacon / query), `v` (1), `app` (OpenWheels), `id` (random per process),
`name`, `dev`, `plat` (pc / phone), `port` (TCP), `svc` (services, `levels`). An instance that
opens a player list sends a `query`, which peers answer at once with a unicast beacon. Peers
disappear 6 s after their last beacon; interfaces are re-read every 10 s.

**Connection** (`Channel`, generic): TCP, one framed message per frame: magic `OWNM`, version 1,
payload length, CRC-32 of the payload, then a type and string fields (binary-safe). Bad magic,
version, length or checksum closes the connection.

**Level transfer** (`LevelTransfer`) on TCP **47811** (the next free port up to 47826 when
several instances share a machine):

| sender | receiver |
|---|---|
| `hello` app, proto, purpose=level, name, dev, id | `hello` (app / protocol check) |
| `offer` name, comments, kind, size, character, force | popup Accept / Decline |
| | `answer` accept=0/1, reason (declined, busy, rate, timeout, too_large) |
| `level` buildVersion, name, comments, data, force_character, playable_character, kind | size ≤ 4 MB, flash levels converted, XML must parse, saved |
| | `result` ok, error, saved name |

The `level` fields are those of the editor's Share (`.happywheels`) plus `kind`: `mobile` (level
XML) or `flash` (a downloaded browser level, converted by the receiver with
`FlashLevelConverter` like the online browser does).

**Receive code:** IPv4 address + port offset + 4-bit check, 40 bits as 8 Crockford base-32
characters (`XXXX-XXXX`).

**Safety:** offers are never accepted automatically; one offer on screen at a time (others get
"busy"); one offer per address per 3 s, 10 s after a decline, at most 10 a minute; offers time
out after 60 s; payloads are capped at 4 MB and the XML is validated before it is saved.

The ghost race ([RACE.md](RACE.md)) reuses `LanDiscovery` (service `race`), the listener and `Channel`
(`purpose` = race).

## Platform notes

* **Windows:** the first start asks Windows Defender Firewall whether OpenWheels may use the
  network (it listens on TCP 47811 and UDP 47810). Allow it on private networks, or other
  devices cannot reach you. Two instances on one PC find each other without that.
* **Android:** the manifest adds `CHANGE_WIFI_MULTICAST_STATE` (a Wi-Fi `MulticastLock` is held
  only while a player list is open, `AppActivity.setMulticastLock`) and `ACCESS_NETWORK_STATE`.
* **Android emulator:** the emulator sits behind its own NAT (10.0.2.15), so discovery between
  the emulator and the PC does not work. Emulator → PC: type the PC's receive code (or
  `10.0.2.2:47811`) on the emulator. PC → emulator: `adb forward tcp:47900 tcp:47811`, then type
  `127.0.0.1:47900` on the PC.
* Networks that isolate clients (many guest / public Wi-Fi networks) block both discovery and
  connections; a home network works.
