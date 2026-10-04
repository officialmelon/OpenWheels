# Restored browser characters

The Android 1.1.3 port ships six of the browser game's eleven player characters. OpenWheels
restores the other five in `src/restored/` (marked `// RESTORED (PC addition):` where they hook
into `src/game/`). They are ports of the browser game's Flash v1.87 classes, built as
mobile-style `CharacterB2D` riders and `Vehicle`s.

| id | Character | Vehicle class | Extra controls (keyboard; buttons on touch) |
|---|---|---|---|
| 6 | Lawnmower Man | `LawnMower` | space: deck lift; the blade grinds what it catches |
| 7 | Explorer Guy | `MineCart` | space: clamp onto rails; shift / ctrl: stand up / crouch |
| 8 | Santa Claus | `Sleigh` (with two elves) | space: flight while the boost meter lasts; shift: let go of elves |
| 10 | Irresponsible Mom | `MomBike` (daughter's trailer bike, son's basket) | space: brake; shift / ctrl: eject son / daughter |
| 11 | Helicopter Man | `Helicopter` | space: magnet; shift / ctrl: reel the rope in / out |

The ids are the browser game's character ids. The extra buttons use control bits 0x20 and 0x40
(shift and ctrl on the keyboard). Arrow keys, space and Z work as for the original characters.
`FlashParticles` draws the browser game's debris and spark particles these characters use.
The kids' gore follows the QoL "child gore" option (`docs/QOL.md`).

## Assets

Nothing of the browser game is in the repo. At build time `tools/assets/extract_character.py`
rebuilds each character from the player's own browser-game SWFs (`characters/character<N>.swf`
and the sound SWF `happy_sounds_v1_72.swf`, by default under `binary/flash/swf/`) into the
mobile game's formats:

```
generated/restored/shared/   Characters_restored.plist, character and vehicle body plists
generated/restored/<tier>/   sprite sheets, character-select art, control buttons
generated/restored/sounds/   sounds the mobile assets lack
```

It runs as a post-build step (CMake on Windows, the Gradle task `owGenerateRestoredCharacters`
on Android) and is skipped quietly when the SWFs or its tools are missing (Java 11+, FFDec,
numpy, Pillow; ffmpeg for the sounds). The conversion conventions are validated with
`--calibrate`, which converts Irresponsible Dad's SWF and compares the result with his Android
files.

Without the generated files nothing changes: the character list stays the original six. With
them the five appear in character select, play in any level that lets the player choose, and
stay forced in converted browser levels that require them (`docs/FLASH_LEVELS.md` 10.4).

### "Hide vehicle" starts

A browser level that forces a restored character with "hide vehicle" (`<info h="t">`) gets that
character's own ragdoll without the vehicle, as Flash's `PlayableCharacterB2D`:
`restored::createBareCharacter` (called by `online::createBareCharacter`) builds a plain
`CharacterB2D` from the character's body description, sprites, gore and voice (Flash tags: Char11,
Char2, Santa, Char4, Heli), ejected from the start, controlled with the ejected d-pad and grab.
`PlayableCharacterB2D` creates nothing else: no vehicle, no Irresponsible Mom kids, no Santa
elves, and shift / ctrl do nothing (the ejected controls leave out the kid / elf buttons). The
Explorer is not among Flash's helmeted characters there, so his hat stays on and his head smashes
at the normal limit.
