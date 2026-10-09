# Reference notes: what the original campaign levels do

Sources: the 73 original mobile campaign levels in the v0.3.0 release's game data
(`assets/shared/levels/`, listed by `levelData.plist`), played in the OpenWheels build made for
this review (Linux, Xvfb, 1600 x 900), and their XML read with a small script. Screenshots in this
folder are from those runs (original game data, used here only as review references).

| Screenshot | Level (chapter) | What it shows |
|---|---|---|
| `orig_city_rooftop.png` | City (Effective Shopper 8) | rooftop route over a detailed building; HVAC units, railings and a parapet give the roof a function. The green-hills backdrop (bg 1) supplies depth. |
| `orig_barn.png` | Barn (Wheelchair Guy 15) | one large, simple, readable landmark (barn front, X-braced loft doors, arched door). A wagon prop doubles as the route. |
| `orig_blue_industrial.png` | Blue Industrial (Wheelchair Guy 8) | a single-hue palette (5 blues), big diagonal forms, hazard-striped platforms and a hand-drawn arrow as signage. |
| `orig_factory.png` | Factory (Effective Shopper 15) | interior rooms framed by walls, with hazard stripes marking mechanisms; drop shafts lead between rooms. |
| `orig_swamp.png` | Kreft Swamp (Wheelchair Guy 6) | organic foliage polygons, root-covered cliff faces and layered tree trunks (no outlines anywhere). |
| `orig_boulder.png` | Boulder (Effective Shopper 11) | a boulder chase: the threat shares the screen with the rider at the start. |
| `orig_egyptian_tomb.png` | Egyptian Tomb (Effective Shopper 13) | carved wall panels and statues (mummies, Anubis). A spike row kills a careless start immediately. |

## Measured facts that shaped the three new levels

* **Camera.** At 1600 x 900 the gameplay view shows about 885 x 500 Flash px, with about 330 px above
  the rider's ground and 170 px below (measured from the old Lawn and Order house windows). Set
  pieces must read inside that frame. 12-18 camera widths is about 11 000-16 000 px.
* **Density.** Originals carry 500-1600 shapes, 70-90 % of them art, 80-300 groups, 12-108
  joints and 23-215 triggers (Egyptian Tomb: 640 shapes, 124 triggers; City: 1619 shapes; Barn:
  1309 shapes, 137 triggers). The old restored levels had 50-150 shapes and 1-4 triggers.
* **Style.** No outlines (`p9 = -1` on more than 99 % of shapes), flat fills with 50-230 distinct
  colours per level (a light and a dark tone per material), and organic art polygons of up to 100
  vertices. Many collision shapes are invisible (opacity 0) under the art.
* **Depth.** The bg 1 backdrop (green hills, clouds, parallax) does most of the far-depth work
  outdoors. Indoors, depth comes from a darker back wall behind lighter structural elements.

## Qualities reproduced on purpose

1. **One landmark per screen.** Each camera-width has one large readable structure (house,
   carved face, rack, plane) rather than many small scattered props.
2. **Material tone pairs.** Every material gets a lit top edge and a darker side or underside:
   grass lip over soil strata, block highlights on masonry, ribs on corrugated steel.
3. **Function before decoration.** Hazards and mechanisms carry their own colour language:
   hazard stripes on bays and doors, gold on the idol, red and green status lamps.
4. **Invisible, simple collision under detailed art.** Collision comes from the same profile
   the art follows (convex quads per segment), so nothing snags that isn't drawn.
5. **Fair warnings.** Hazards are announced in the scene: a seesaw sign, a falling-boulder sign
   at the low arch, a skeleton pinned by darts. This avoids Egyptian Tomb's instant spike death.
6. **Story through the scene, not paragraphs.** No titles, hints or captions in the level; one
   or two triggered physical reactions (Mrs. Henderson swoons, the frieze sheds chunks) and
   signs painted on objects (LAWN OF THE MONTH, BAY 1, COUNTY FAIR WINNER).
7. **Endings that conclude.** Each level ends in a distinct place: the HOA office, a sunset
   clearing with the getaway jeep, the tower helipad.
