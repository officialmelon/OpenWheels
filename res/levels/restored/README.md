# Restored characters campaign

Thirty original OpenWheels levels, six per restored browser character (`src/restored`,
`docs/RESTORED.md`), easiest first:

| folder | character | levels |
|---|---|---|
| `06_lawnmower_man/` | Lawnmower Man (6) | Lawn and Order, Suburban Sprawl, The Grass Is Greener, Hedge Maze, County Fair, Mow or Never |
| `07_explorer_guy/` | Explorer Guy (7) | Temple Run, Mine Shaft, Idol Hands, Rope Bridge, The Lost City, Tomb Raided |
| `08_santa_claus/` | Santa Claus (8) | Rooftop Run, Chimney Drop, Workshop Chaos, North Pole, Naughty List, Silent Night |
| `10_irresponsible_mom/` | Irresponsible Mom (10) | School Run, Soccer Practice, Mall Madness, Playground, Traffic Jam, Family Vacation |
| `11_helicopter_man/` | Helicopter Man (11) | Lift Off, Skyscraper, Crane Game, Air Traffic, Magnet Mayhem, Mayday |

They are browser-format level XML (`<info v="1.87" c=".." f="t">`, Flash pixels, see
`docs/FLASH_LEVELS.md` section 5). The game converts them on load with
`src/online/FlashLevelConverter.cpp`, and the editor opens them like any browser level
(`src/editor/flash/FlashLevelIO.cpp`). Every level forces its character and ends at a finish line.

## Regenerating

The XML files are generated, so don't edit them by hand:

```
python3 tools/levels/make_restored_levels.py            # writes res/levels/restored/**.xml
python3 tools/levels/preview_level.py <level.xml>       # PNG preview + layout checks
```

`make_restored_levels.py` (stdlib only, deterministic) holds one commented function per level;
`tools/levels/restored_lib.py` is the small XML builder it uses. `preview_level.py` (needs Pillow)
renders shapes, specials, joints and triggers, and checks the start, the finish support and
whether text boxes sit where the camera can see them. Terrain is built from rectangles (rotated
slabs for slopes), and rails sit flush with the ground. The finish strip is sunk into the ground
because a raised strip wrecks carts and bikes.

## License

Original OpenWheels content under the repository's MIT license (`LICENSE`). No Happy Wheels
level was copied; the designs and jokes are new.
