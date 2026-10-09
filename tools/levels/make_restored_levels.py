#!/usr/bin/env python3
"""Generate the OpenWheels "restored characters" campaign: 5 chapters x 6 browser-format levels.

    python3 tools/levels/make_restored_levels.py [--out res/levels/restored] [--check]

Original OpenWheels content (MIT, see LICENSE): one chapter per restored browser character
(src/restored, docs/RESTORED.md), written in the browser game's level XML (v 1.87), which the game
converts on load (src/online/FlashLevelConverter.cpp). Deterministic: no randomness, so the output
only changes when this file does. Preview / sanity-check any level with
tools/levels/preview_level.py.

Units: Flash px, y down, stage 20000 x 10000, 62.5 px per metre. Levels sit around y = 5000.
Character controls the designs lean on (docs/RESTORED.md):
  6  Lawnmower Man   space: deck lift; the blade grinds NPCs and food it catches
  7  Explorer Guy    space: clamp onto rails (special 27); shift / ctrl: stand up / crouch
  8  Santa Claus     space: fly while the boost meter lasts (~3 s, refills); shift: let elves go
  10 Irresponsible Mom  space: brake; shift / ctrl: eject son / daughter
  11 Helicopter Man  space: magnet on / off (holds any loose body under it); shift / ctrl: rope
"""

import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from restored_lib import LEVEL, Level, rgb  # noqa: E402

G = 5000          # default ground surface (y)
RIDE = 120        # start point height above the ground for ground vehicles

# Palette ------------------------------------------------------------------------------------------
GRASS = rgb('5f9e35')
GRASS_DARK = rgb('3f7a22')
GRASS_LIGHT = rgb('8cc152')
DIRT = rgb('7a5230')
DIRT_DARK = rgb('5a3a20')
SAND = rgb('e3cf8c')
STONE = rgb('8f8a80')
STONE_DARK = rgb('5e5a52')
STONE_LIGHT = rgb('b8b2a4')
TEMPLE = rgb('a39060')
TEMPLE_DARK = rgb('6e6040')
MOSS = rgb('4e7a3a')
WOOD = rgb('8b5a2b')
WOOD_DARK = rgb('5e3b1a')
WOOD_LIGHT = rgb('b07a45')
ASPHALT = rgb('4a4a4f')
CONCRETE = rgb('a7a7a2')
CURB = rgb('cfcfc8')
WHITE = rgb('ffffff')
BLACK = rgb('000000')
RED = rgb('c0392b')
RED_DARK = rgb('8e2318')
YELLOW = rgb('f1c40f')
ORANGE = rgb('e67e22')
BLUE = rgb('2e86de')
WATER = rgb('3d8fd6')
SKY = rgb('bfe3ff')
NIGHT = rgb('0d1b3a')
SNOW = rgb('f4f8ff')
ICE = rgb('bfe6ff')
PINK = rgb('f39cc0')
PURPLE = rgb('8e44ad')
GREEN = rgb('27ae60')
HEDGE = rgb('2f6b2a')
HEDGE_DARK = rgb('1f4f1c')
METAL = rgb('9aa3ab')
METAL_DARK = rgb('5f6870')
LAVA = rgb('ff5a1f')
GOLD = rgb('e5b80b')
BRICK = rgb('a0522d')
ROOF = rgb('7b2e2e')


# ---------------------------------------------------------------------------------------------
# Decoration helpers (art only: no physics)

def tree(L, x, ground, h=320, crown=GRASS_DARK, trunk=WOOD_DARK):
    L.rect(x, ground - h * 0.3, h * 0.12, h * 0.6, color=trunk, inter=False)
    L.circle(x, ground - h * 0.7, h * 0.55, color=crown, inter=False)
    L.circle(x - h * 0.2, ground - h * 0.6, h * 0.4, color=crown, inter=False)
    L.circle(x + h * 0.2, ground - h * 0.62, h * 0.42, color=crown, inter=False)


def pine(L, x, ground, h=400, color=rgb('1e5a32'), snow=False):
    L.rect(x, ground - h * 0.1, h * 0.08, h * 0.2, color=WOOD_DARK, inter=False)
    for k in range(3):
        w = h * (0.62 - k * 0.14)
        top = ground - h * (0.15 + k * 0.26)
        L.roof(x, top, w, h * 0.42, color=color)
        if snow:
            L.roof(x, top - h * 0.28, w * 0.4, h * 0.14, color=SNOW)


def bush(L, x, ground, w=160, color=GRASS_DARK):
    L.circle(x - w * 0.25, ground - w * 0.2, w * 0.55, color=color, inter=False)
    L.circle(x + w * 0.25, ground - w * 0.2, w * 0.5, color=color, inter=False)
    L.circle(x, ground - w * 0.35, w * 0.55, color=color, inter=False)


def cloud(L, x, y, w=300, color=WHITE, opacity=90):
    L.circle(x, y, w * 0.45, color=color, inter=False, opacity=opacity)
    L.circle(x - w * 0.3, y + w * 0.08, w * 0.32, color=color, inter=False, opacity=opacity)
    L.circle(x + w * 0.3, y + w * 0.06, w * 0.36, color=color, inter=False, opacity=opacity)


def house(L, x, ground, w=520, h=300, wall=rgb('e8d8b0'), roof=ROOF, door=WOOD, windows=True,
          snow=False):
    """A house facade (art) with its left edge at x."""
    L.box(x, ground - h, x + w, ground, color=wall, outline=rgb('6b5b40'), inter=False)
    L.roof(x + w / 2, ground - h, w + 60, h * 0.6, color=roof)
    if snow:
        L.roof(x + w / 2, ground - h - h * 0.3, (w + 60) / 2, h * 0.3, color=SNOW)
        L.box(x - 30, ground - h - 6, x + w + 30, ground - h + 8, color=SNOW, inter=False)
    L.box(x + w * 0.42, ground - h * 0.55, x + w * 0.58, ground, color=door, inter=False)
    if windows:
        for wx in (x + w * 0.14, x + w * 0.72):
            L.box(wx, ground - h * 0.75, wx + w * 0.14, ground - h * 0.45, color=rgb('9fd3f0'),
                  outline=WHITE, inter=False)


def picket_fence(L, x1, x2, ground, h=80, color=WHITE):
    L.box(x1, ground - h * 0.7, x2, ground - h * 0.58, color=color, inter=False)
    L.box(x1, ground - h * 0.35, x2, ground - h * 0.23, color=color, inter=False)
    x = x1
    while x <= x2 - 14:
        L.box(x, ground - h + 10, x + 14, ground, color=color, inter=False)
        L.roof(x + 7, ground - h + 10, 14, 12, color=color)
        x += 30


def lamp_post(L, x, ground, h=320, color=METAL_DARK, light=YELLOW):
    L.rect(x, ground - h / 2, 10, h, color=color, inter=False)
    L.rect(x + 25, ground - h, 60, 8, color=color, inter=False)
    L.circle(x + 50, ground - h + 14, 22, color=light, inter=False)


def stars(L, x1, x2, y1, y2, n=40):
    # deterministic scatter
    for k in range(n):
        fx = (k * 0.6180339887) % 1.0
        fy = (k * 0.4142135623 + 0.13) % 1.0
        d = 6 + (k % 3) * 3
        L.circle(x1 + fx * (x2 - x1), y1 + fy * (y2 - y1), d, color=rgb('fff7c2'), inter=False,
                 opacity=85)


def hill_profile(x1, x2, base, amp, waves=1.0, step=250, phase=0.0):
    """Smooth rolling-hill surface points from x1 to x2."""
    pts = []
    n = max(1, int((x2 - x1) / step))
    for i in range(n + 1):
        x = x1 + (x2 - x1) * i / n
        t = (x - x1) / float(x2 - x1)
        y = base - amp * math.sin(math.pi * waves * t + phase) ** 2
        pts.append((round(x), round(y)))
    return pts


def arc_points(cx, cy, r, a1, a2, n):
    """Points on a circle (degrees, Flash orientation: 0 = +x, 90 = +y/down)."""
    out = []
    for i in range(n + 1):
        a = math.radians(a1 + (a2 - a1) * i / n)
        out.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return out


def ring_group(L, cx, cy, r, thick, segments, gaps=(), color=METAL, opacity=100, collision=1,
               density=2.0):
    """A ring of rectangles in a new group (use a motorised pin at its centre to spin it)."""
    g = L.group()
    seg_len = 2 * math.pi * r / segments * 1.04
    for k in range(segments):
        if k in gaps:
            continue
        a = 360.0 * k / segments
        x = cx + r * math.cos(math.radians(a))
        y = cy + r * math.sin(math.radians(a))
        g.rect(x, y, thick, seg_len, color=color, rot=a, density=density, opacity=opacity,
               collision=collision, fixed=False)
    return g


def gate(L, x, top, bottom, w=60, color=HEDGE, outline=-1):
    """A static door block from top to bottom (removed by a trigger)."""
    return L.box(x - w / 2, top, x + w / 2, bottom, color=color, outline=outline)


def open_gate(ref, fade=0.4):
    """Trigger target: fade the gate's art out and remove its physics at once."""
    return (ref, [(3, 0, fade), (5,)])


def show_text(ref, fade=0.5):
    return (ref, [(0, 100, fade)])


# =============================================================================================
# 06 LAWNMOWER MAN (character 6): riding mower; space lifts the deck; the blade grinds NPCs/food.
# =============================================================================================

def title(L, x, ground, name, lines, color=BLACK, size=16):
    """Level intro: the name and a few lines, low enough to be on screen at the start (the
    in-game camera shows roughly 1000 x 560 px of the level around the rider)."""
    n = lines.count('\n') + 1
    top = ground - 120 - n * (size + 4) - 50
    x = max(20, L.start[0] - 240)          # the camera keeps the rider about a third in from the left
    L.text(x, top, name, size=36, color=color, font=5)
    L.text(x, top + 46, lines, size=size, color=color, font=2)


lawn_title = title


def lm_01():
    """Lawn and Order (easy): a sunny suburban lawn. Teaches driving, the blade (sunbathers and a
    picnic to grind), a gentle hill, a kiddie-pool dip, a see-saw, and a text that pops in."""
    L = Level('06_lawnmower_man/01_lawn_and_order.xml', 'Lawn and Order', 6, (500, G - RIDE),
              bg=1, bgc=SKY)
    cloud(L, 1500, 3900, 360)
    cloud(L, 4200, 3800, 420)
    cloud(L, 6900, 3950, 300)
    house(L, 80, G, 560, 320)
    tree(L, 1100, G, 360)
    picket_fence(L, 1250, 2500, G)
    house(L, 2700, G, 600, 340, wall=rgb('c9e0f2'), roof=rgb('3b4f7a'))
    tree(L, 4150, G - 150, 330)
    house(L, 5300, G, 640, 360, wall=rgb('f2d0c9'), roof=rgb('6b3b2b'))
    picket_fence(L, 6100, 7300, G, color=rgb('f5f5f5'))
    lawn_title(L, 180, G, 'LAWN AND ORDER',
               'The homeowners association says your grass is "a cry for help".\n'
               'Mow everything. They did say EVERYTHING.\n'
               'Arrows drive and lean. Space lifts the deck.')
    # ground: driveway, lawn, a gentle hill, the kiddie-pool dip, lawn to the end
    L.box(0, G, 900, G + 600, color=CONCRETE)                       # driveway
    L.ground([(900, G), (2600, G)], bottom=G + 600, color=GRASS)
    L.ground(hill_profile(2600, 4400, G, 150, 1.0, 200), bottom=G + 600, color=GRASS)
    # kiddie pool: a shallow dip with sloped sides and water art
    L.ground([(4300, G), (4460, G + 40), (4840, G + 40), (5000, G)], bottom=G + 600, color=GRASS)
    L.box(4400, G + 12, 4900, G + 40, color=WATER, opacity=60, inter=False)
    L.ground([(5000, G), (7600, G)], bottom=G + 600, color=GRASS)
    # Native gameplay arrows: forward, lean before the hill, and deck lift before the pool.
    # Every highlight is explicitly cleared so it never stacks or follows the player forever.
    L.system_trigger(700, G - 130, 90, 300, 0)
    L.system_trigger(1100, G - 130, 90, 300, 5)
    L.system_trigger(2450, G - 130, 90, 300, 2)
    L.system_trigger(2850, G - 130, 90, 300, 5)
    L.system_trigger(4100, G - 130, 90, 300, 4)
    L.system_trigger(4260, G - 130, 90, 300, 5)
    L.box(7600, G - 400, 7700, G + 600, color=HEDGE)                # end wall (hedge)
    # Grass tufts along the lawn (the "tall grass" you are here to mow)
    for k in range(18):
        x = 1000 + k * 360
        if 4280 < x < 5020:
            continue
        L.roof(x + 10, G, 20, 28, color=GRASS_LIGHT)
        L.roof(x + 28, G, 20, 20, color=GRASS_LIGHT)
    # sunbathers standing on their towels (sleeping NPCs stay put until hit) -> blade fodder
    for x, c in [(1500, 3), (1800, 7), (2150, 5)]:
        L.box(x - 80, G - 4, x + 80, G, color=rgb('e74c3c') if c != 7 else rgb('3498db'), inter=False)
        L.npc(x, G, char=c, sleep=True, reverse=(c == 7))
    # picnic on the hill top: table with food, a chair
    L.table(3500, G - 150)
    L.food(3470, G - 150 - 75, kind=1)
    L.food(3540, G - 150 - 75, kind=2)
    L.chair(3640, G - 148, reverse=True)
    L.token(3500, G - 420, 1)
    # see-saw after the pool: a plank pinned to a static wedge
    L.tri(5600, G - 15, 70, 46, color=WOOD_DARK)
    g = L.group()        # starts tipped, near end on the grass (the ground limits it both ways)
    g.rect(5600, G - 55, 520, 20, rot=-10, color=WOOD_LIGHT, outline=WOOD_DARK, fixed=False, density=2)
    L.pin(5600, G - 55, g, LEVEL)
    # Mrs. Henderson guards her prize begonias; her complaint pops in when you arrive
    L.sign(6450, G, kind=3)
    L.npc(6700, G, char=12, sleep=True, hold=True, pose=(0, -150, -40, -60, 0, 0, 0, 0, 0))
    L.table(6880, G)                          # her prize-winning produce, at deck height
    for x in (6840, 6920):
        L.food(x, G - 78, kind=3)
    yell = L.text(6250, G - 330, 'HEY! THOSE BEGONIAS ARE\nIN THE HOA BYLAWS!', size=26, color=RED_DARK,
                  font=3, opacity=0)
    L.trigger(6200, G - 150, 200, 300, [show_text(yell)])
    L.finish(7300, G)
    L.text(6950, 4600, 'Mrs. Henderson has\nwithdrawn her complaint.', size=16, color=BLACK)
    return L


def lm_02():
    """Suburban Sprawl (easy+): a street of houses. Curb ramps, a garage sale to plough through,
    a backyard barbecue crowd, a pool to jump (boost panel on the diving deck), rose-bush spikes,
    and a hedge you clear from a driveway ramp."""
    L = Level('06_lawnmower_man/02_suburban_sprawl.xml', 'Suburban Sprawl', 6, (450, G - RIDE),
              bg=1, bgc=SKY)
    for x in (900, 3600, 6400, 9300):
        cloud(L, x, 3800 + (x % 300), 380)
    house(L, 150, G, 520, 320)
    house(L, 1900, G, 560, 300, wall=rgb('d7e8c4'), roof=rgb('4a6b3a'))
    house(L, 4300, G, 600, 360, wall=rgb('f0e0c0'), roof=rgb('8a4b2b'))
    house(L, 7600, G, 560, 330, wall=rgb('dcd0f0'), roof=rgb('5a3b7a'))
    house(L, 10100, G, 620, 340, wall=rgb('f2c9c9'), roof=rgb('7a2b2b'))
    lawn_title(L, 180, G, 'SUBURBAN SPRAWL',
               'Six lawns. One mower. Zero permits.\n'
               'Garage sale ahead: everything must go (under the blade).')
    # street level with curbs: concrete driveways alternate with lawns; little curb ramps
    prof = [(0, G), (1300, G), (1360, G - 40), (1600, G - 40), (1660, G)]
    L.ground(prof, bottom=G + 600, color=GRASS)
    # garage sale on the driveway of house 2
    L.box(1660, G, 3400, G + 600, color=CONCRETE)
    L.table(2200, G)
    L.tv(2170, G - 78)
    L.boombox(2240, G - 72)
    L.chair(2750, G)
    L.sign(1900, G, kind=5)
    L.text(2050, 4600, 'GARAGE SALE\nNO REFUNDS', size=22, color=RED_DARK, font=3)
    # backyard barbecue: a crowd standing around a grill, food everywhere
    L.ground([(3400, G), (5800, G)], bottom=G + 600, color=GRASS)
    L.box(4050, G - 70, 4130, G, color=METAL_DARK, inter=False)        # grill
    L.box(4030, G - 80, 4150, G - 66, color=BLACK, inter=False)
    for k, (x, c) in enumerate([(3700, 2), (3950, 9), (4380, 11)]):
        L.npc(x, G, char=c, sleep=True, reverse=(k % 2 == 1))
    L.table(4100, G)
    for x in (4070, 4130):
        L.food(x, G - 78, kind=1 + (x // 40) % 3)
    # pool: a boost panel on the deck to jump it; fall in and the shallow end has a gentle ramp
    # out (the mower climbs about 15 degrees at most)
    L.box(5800, G, 6150, G + 600, color=CONCRETE)                       # pool deck
    L.boost(5975, G, panels=1, power=40)
    L.box(6150, G + 120, 6450, G + 600, color=rgb('7fb8d8'))             # deep end floor
    L.box(6150, G + 20, 7300, G + 120, color=WATER, opacity=65, inter=False)
    L.ground([(6450, G + 120), (7300, G)], bottom=G + 600, color=rgb('7fb8d8'))   # shallow end (8 deg)
    L.box(7300, G, 7500, G + 600, color=CONCRETE)
    # front lawn with a rose bed (spikes) in a dip: a ramp and a boost panel clear it
    L.ground([(7500, G), (7600, G), (8050, G - 110)], bottom=G + 600, color=GRASS)
    L.box(8050, G - 110, 8070, G + 600, color=GRASS)
    L.box(8070, G + 60, 8430, G + 600, color=DIRT)
    L.spikes_on(8250, G + 60, count=22)
    for k in range(7):
        L.circle(8100 + k * 50, G + 30, 34, color=rgb('c2185b'), inter=False, opacity=80)
    L.boost(7830, G - 55, panels=1, power=40, rot=-14)
    L.text(8100, 4700, 'Prize roses', size=14, color=BLACK)
    L.ground([(8430, G), (8900, G)], bottom=G + 600, color=GRASS)
    # driveway with a hedge: drive up the ramp, over the hedge top and down the other side
    L.box(8900, G, 10000, G + 600, color=CONCRETE)
    L.ground([(9000, G), (9450, G - 75)], bottom=G, color=CONCRETE)
    L.box(9450, G - 75, 9700, G, color=HEDGE, outline=HEDGE_DARK)
    L.ground([(9700, G - 75), (9990, G)], bottom=G, color=CONCRETE)
    L.ground([(10000, G), (11800, G)], bottom=G + 600, color=GRASS)
    for x in (10300, 10450, 10600):
        L.token(x, G - 60, 2)
    L.box(11800, G - 400, 11900, G + 600, color=HEDGE)
    L.finish(11500, G)
    L.text(11100, G - 260, 'Cul-de-sac reached.\nProperty values: down.', size=16)
    return L


def lm_03():
    """The Grass Is Greener (medium): a golf course. Rolling fairways, golfers in mid-swing, a sand
    bunker, a water hazard crossed on a pinned plank bridge, a fan "crosswind", and a spinning
    mini-golf windmill (motorised group) you slip past at the 18th."""
    L = Level('06_lawnmower_man/03_grass_is_greener.xml', 'The Grass Is Greener', 6, (500, G - RIDE),
              bg=1, bgc=rgb('c6ecff'))
    for x in (1200, 4000, 7200, 10200):
        cloud(L, x, 3700, 420)
    for x in (1500, 3300, 5200, 8600, 11000):
        tree(L, x, G - 60, 380, crown=rgb('2e6b2e'))
    lawn_title(L, 180, G, 'THE GRASS IS GREENER',
               'Members only. Your membership: one riding mower.\n'
               'Keep off the green. Ignore that.')
    # tee box
    L.box(0, G, 1000, G + 700, color=GRASS_LIGHT)
    L.sign(850, G, kind=7)
    # fairway 1: rolling hills with golfers
    L.ground(hill_profile(1000, 3500, G, 70, 2.0, 200), bottom=G + 700, color=GRASS)
    for x, c in [(1650, 8), (2900, 10)]:
        L.npc(x, G - 70 * math.sin(math.pi * 2.0 * (x - 1000) / 2500.0) ** 2, char=c, sleep=True, hold=True,
              pose=(0, -120, -100, -40, -40, 0, 0, 0, 0))
    # sand bunker: a dip filled with sand (art) and a little lip
    L.ground([(3500, G), (3800, G + 30), (4200, G + 30), (4500, G)], bottom=G + 700, color=SAND)
    L.ground([(4500, G), (4700, G), (5300, G - 120)], bottom=G + 700, color=GRASS)
    L.npc(4700, G, char=13, sleep=True, hold=True, pose=(10, -170, -20, -30, -20, 0, 0, 0, 0))
    L.text(4450, G - 300, 'FORE!', size=40, color=RED_DARK, font=5)
    # water hazard: a pond spanned by a hanging plank bridge (planks pinned end to end)
    pond_l, pond_r, top = 5300, 6500, G - 120
    L.box(pond_l, G + 200, pond_r, G + 700, color=DIRT_DARK)
    L.box(pond_l, G - 40, pond_r, G + 200, color=WATER, opacity=75, inter=False)
    L.box(pond_l - 10, top, pond_l + 10, G + 200, color=GRASS_DARK)      # bank posts
    L.box(pond_r - 10, top, pond_r + 10, G + 200, color=GRASS_DARK)
    planks = 8
    span = (pond_r - pond_l) / planks
    prev = LEVEL
    for k in range(planks):
        x1 = pond_l + k * span
        g = L.group()
        g.rect(x1 + span / 2, top + 10, span - 4, 20, color=WOOD_LIGHT, fixed=False, density=3)
        L.pin(x1 + 2, top + 10, g, prev)
        prev = g
    L.pin(pond_r - 2, top + 10, prev, LEVEL)
    L.ground([(6500, top), (7100, top), (7700, G)], bottom=G + 700, color=GRASS)
    # crosswind on the open fairway: fans tilted to blow along the path
    L.ground([(7700, G), (9300, G)], bottom=G + 700, color=GRASS)
    L.sign(8100, G, kind=8)
    L.text(7950, G - 330, 'Wind warning\n(the sign said "light breeze")', size=14)
    for x in (8500, 8650, 8800):
        L.token(x, G - 80, 3)
    # green with the windmill
    L.ground([(9300, G), (12100, G)], bottom=G + 700, color=GRASS_LIGHT)
    wx, wy = 10200, G - 330
    L.box(wx - 120, G - 260, wx + 120, G - 60, color=rgb('d35400'), inter=False)       # mill body
    L.roof(wx, G - 260, 300, 160, color=RED_DARK)
    L.box(wx - 120, G - 60, wx - 50, G, color=rgb('d35400'), inter=False)               # legs (art: the
    L.box(wx + 50, G - 60, wx + 120, G, color=rgb('d35400'), inter=False)               # blades sweep here)
    g = L.group()
    for a in (0, 90, 180, 270):
        bx = wx + 150 * math.cos(math.radians(a))
        by = wy + 150 * math.sin(math.radians(a))
        g.rect(bx, by, 280, 36, rot=a, color=WHITE, outline=BLACK, fixed=False, density=1)
    g.circle(wx, wy, 50, color=WOOD_DARK, fixed=False)
    L.pin(wx, wy, g, LEVEL, motor=True, torque=200000, speed=1.2)
    L.text(9400, G - 340, 'Hole 18: through the windmill.\nPar 3. Your par: whatever.', size=18)
    L.box(11500, G - 6, 11700, G, color=WHITE, inter=False)
    L.rect(11600, G - 110, 6, 220, color=WHITE, inter=False)
    L.art([(11603, G - 220), (11680, G - 195), (11603, G - 170)], color=RED)   # pennant (art polygon)
    L.tri(11640, G - 195, 50, 70, rot=90, color=RED, inter=False)
    L.finish(11650, G)
    L.box(12100, G - 400, 12200, G + 700, color=HEDGE)
    return L


def lm_04():
    """Hedge Maze (medium+): three stacked hedge lanes, built so you only ever drive right. Each
    fork is a gap with a boost panel and a kicker: you sail over it into a dead-end branch that
    holds a lever, reverse out and drop through the gap to go on. Lever A opens gate A on the
    middle lane, lever B opens gate B on the bottom lane, which leads out to the finish. A
    gardener and a crowd of lost tourists wait to be mown."""
    TOP, MID, LOW = 4000, 4400, 4800          # lane floors (surface y)
    TH = 70                                    # slab thickness
    L = Level('06_lawnmower_man/04_hedge_maze.xml', 'Hedge Maze', 6, (450, TOP - RIDE),
              bg=1, bgc=rgb('cfe8c8'))
    lawn_title(L, 160, TOP, 'HEDGE MAZE',
               "Lord Pemberton's famous maze. Nobody has ever come out.\n"
               "Nobody has gone in, either. It is very exclusive.\n"
               "Red levers open the gates. The boost panels get you over the gaps;\n"
               "reverse (down arrow) out of the dead ends and drop through the gaps.")

    def hedge(x1, y1, x2, y2):
        return L.box(x1, y1, x2, y2, color=HEDGE, outline=HEDGE_DARK)

    def kicker(x2, y):
        # a small ramp ending at x2 on a floor at y
        L.slab(x2 - 160, y, x2, y - 35, thick=35, color=HEDGE, outline=HEDGE_DARK)

    def lever(x, y, gate_ref, msg, msg_x, msg_y):
        L.box(x - 20, y - 50, x + 20, y, color=RED, outline=RED_DARK, inter=False)
        L.circle(x, y - 55, 26, color=RED_DARK, inter=False)
        note = L.text(msg_x, msg_y, msg, size=15, color=RED_DARK, opacity=0)
        L.trigger(x, y - 70, 90, 140, [open_gate(gate_ref), show_text(note)])

    hedge(0, LOW, 9600, LOW + 400)            # base
    hedge(0, 3300, 120, LOW)                  # left wall
    # --- fork 1: top lane; gap G1 at 1700..1950 (kicker before it) ---------------------------
    hedge(120, TOP, 1700, LOW)                # solid under the start
    kicker(1700, TOP)
    L.boost(1400, TOP, panels=1, power=45)
    hedge(1950, TOP, 3600, TOP + TH)          # top dead-end branch (lever A)
    hedge(3600, 3300, 3680, TOP + TH)         # its end wall (the middle lane runs on below)
    # --- middle lane: 1700..3600 under the top branch, floor block below it --------------------
    hedge(1700, MID, 4600, LOW)               # middle lane floor up to G2
    gate_a = gate(L, 2900, TOP + TH, MID, w=60, color=rgb('245a20'), outline=HEDGE_DARK)
    lever(3450, TOP, gate_a, 'Click. Somewhere,\na gate opens.', 2950, 3700)
    L.text(2050, 3760, 'Overshot? Reverse (down arrow)\nand drop through the gap.', size=14)
    L.text(3700, TOP - 300, 'Dead end.\nLike the hedge\nfund industry.', size=14)
    # --- fork 2 on the middle lane: gap G2 at 4600..4850 ---------------------------------------
    hedge(3680, TOP, 6400, TOP + TH)          # ceiling over the middle lane
    kicker(4600, MID)
    L.boost(4300, MID, panels=1, power=45)
    hedge(4850, MID, 6300, MID + TH)          # middle dead-end branch (lever B)
    hedge(6300, TOP + TH, 6400, MID + TH)     # its end wall
    L.npc(5500, MID, char=8, sleep=True, hold=True, pose=(0, -60, -120, -30, -30, 0, 0, 0, 0))
    L.text(5000, 4120, 'This branch has a gardener.\nHe was here first.', size=14)
    # --- bottom lane: from under G2 to the exit ------------------------------------------------
    hedge(6400, TOP + TH, 8200, MID + TH)     # ceiling over the bottom lane (solid block)
    for x, c in [(5300, 1), (5650, 15), (6000, 16), (7000, 3), (7300, 5)]:
        L.npc(x, LOW, char=c, sleep=True, reverse=True)
    for x in (5850, 6700):
        L.trash(x, LOW)
    L.text(5000, 4520, 'The tourists went this way.\nWe found most of them.', size=14)
    gate_b = gate(L, 7800, MID + TH, LOW, w=60, color=rgb('245a20'), outline=HEDGE_DARK)
    lever(6150, MID, gate_b, 'Clunk. The last gate\nshould be open now.', 5700, 4120)
    # exit garden
    L.text(8300, 4380, 'You escaped the maze!\nThe minotaur was just\na guy named Gary.', size=16)
    L.npc(8650, LOW, char=9, sleep=True, hold=True, pose=(0, -160, -20, 0, 0, 0, 0, 0, 0))
    L.finish(9000, LOW)
    hedge(9500, LOW - 500, 9600, LOW)
    L.token(2200, MID - 60, 4)                # under the top branch, on the way
    for x in (800, 2800, 5000, 7000):
        bush(L, x, 3300 if x < 3600 else TOP, 160, color=rgb('3c8a35'))
    return L


def lm_05():
    """County Fair (medium-hard): a carnival midway. A ticket-line crowd, the pie stand, the Great
    Zambini's cannon act on a stage over the path, the rollers of enthusiasm (motorised), the high
    striker, a decorative spinning ferris wheel, bumper-ball alley, balloons and the prize booth."""
    L = Level('06_lawnmower_man/05_county_fair.xml', 'County Fair', 6, (450, G - RIDE),
              bg=1, bgc=rgb('ffe7b3'))
    lawn_title(L, 180, G, 'COUNTY FAIR',
               'Blue ribbon for "Best In Show". Your entry: chaos.\n'
               'Ride the barrel of fun. It is a legal requirement.')
    L.ground([(0, G), (5800, G)], bottom=G + 600, color=rgb('9b7b4a'))
    L.ground([(7000, G), (13000, G)], bottom=G + 600, color=rgb('9b7b4a'))
    # decorative ferris wheel spinning in the background (no collision: collision 3)
    fx, fy, fr = 2600, G - 700, 520
    L.box(fx - 12, fy, fx + 12, G, color=METAL_DARK, inter=False, rot=0)
    L.slab(fx - 300, G, fx, fy, thick=20, color=METAL_DARK, inter=False)
    L.slab(fx, fy, fx + 300, G, thick=20, color=METAL_DARK, inter=False)
    wheel = L.group()
    for k in range(8):
        a = 45 * k
        wheel.rect(fx + fr / 2 * math.cos(math.radians(a)), fy + fr / 2 * math.sin(math.radians(a)),
                   fr, 10, rot=a, color=METAL, fixed=False, collision=3)
        wheel.rect(fx + fr * math.cos(math.radians(a)), fy + fr * math.sin(math.radians(a)), 70, 50,
                   color=[RED, YELLOW, BLUE, GREEN][k % 4], fixed=False, collision=3)
    wheel.circle(fx, fy, 60, color=METAL_DARK, fixed=False, collision=3)
    L.pin(fx, fy, wheel, LEVEL, motor=True, torque=1e6, speed=0.3)
    # entrance arch and the ticket crowd
    L.box(1150, G - 380, 1190, G, color=RED, inter=False)
    L.box(1810, G - 380, 1850, G, color=RED, inter=False)
    L.box(1150, G - 420, 1850, G - 360, color=YELLOW, outline=RED, inter=False)
    L.text(1260, G - 412, 'COUNTY FAIR', size=36, color=RED_DARK, font=5)
    for k, x in enumerate(range(1300, 1800, 90)):
        L.npc(x, G, char=1 + (k * 5) % 16, sleep=True, reverse=(k % 2 == 0))
    # pie-eating contest: two tables of pies (food) and contestants
    for tx in (3600, 3900):
        L.table(tx, G)
        L.food(tx - 30, G - 78, kind=2)
        L.food(tx + 30, G - 78, kind=2)
    L.npc(3750, G, char=10, sleep=True)
    L.text(3500, G - 330, 'PIE-EATING CONTEST\n(the pies are eating back)', size=16, color=RED_DARK)
    # human cannonball: a clown waits on a ledge over the cannon's mouth until you ride past
    cx = 4700
    L.box(cx - 220, G - 330, cx + 220, G - 300, color=WOOD, outline=WOOD_DARK)       # stage over the path
    L.box(cx - 220, G - 300, cx - 200, G, color=WOOD_DARK, inter=False)
    L.box(cx + 200, G - 300, cx + 220, G, color=WOOD_DARK, inter=False)
    L.cannon(cx, G - 330, rot=0, start=-30, fire=45, delay=1, muzzle=3, power=6)
    L.box(cx - 140, G - 660, cx - 40, G - 630, color=WOOD)
    clown = L.npc(cx - 90, G - 630, char=16, sleep=True)
    L.text(cx + 240, G - 330, 'THE GREAT ZAMBINI\nhuman cannonball', size=16, color=PURPLE)
    L.trigger(cx + 150, G - 100, 150, 200, [(clown, [(0,), (1, 3, 0, 0)])])
    # the rollers of enthusiasm: a trench of motorised rollers that carry you along
    rx1, rx2 = 5800, 7000
    L.box(rx1, G + 70, rx2, G + 600, color=rgb('5a4630'))
    for k in range(10):
        x = rx1 + 60 + k * 120
        roller = L.circle(x, G + 58, 116, color=[RED, YELLOW][k % 2], outline=BLACK, density=2)
        # (in the converted level a negative motor speed turns the rollers' tops forward)
        L.pin(x, G + 58, roller, LEVEL, motor=True, torque=30000, speed=-4)
    L.text(rx1, G - 330, 'THE ROLLERS OF ENTHUSIASM\nthey believe in you more than you do.', size=16,
           color=RED_DARK)
    # strongman: the high striker, and a prize platform with tokens
    L.box(7560, G - 600, 7640, G, color=RED, outline=RED_DARK, inter=False)          # the high striker (scenery)
    L.circle(7600, G - 620, 70, color=YELLOW, outline=RED_DARK, inter=False)
    L.box(8200, G - 450, 8700, G - 400, color=WOOD)
    L.box(8640, G - 750, 8700, G - 450, color=RED, inter=False)
    for x in (8300, 8420, 8540):
        L.token(x, G - 500, 5)
    L.text(7300, G - 330, 'TEST YOUR STRENGTH', size=18, color=RED_DARK, font=3)
    # bumper alley: soccer balls and sleeping balloons (circles) to bounce through
    for k in range(10):
        L.soccer(9100 + k * 70, G - 20)
    for k in range(5):
        L.circle(9900 + k * 160, G - 300 - (k % 2) * 80, 70, color=[RED, BLUE, YELLOW, PINK, GREEN][k],
                 sleep=True, density=0.1)
    # prize booth finish
    L.box(11300, G - 300, 12100, G - 280, color=RED, inter=False)
    L.text(11420, G - 340, 'PRIZE BOOTH', size=28, color=RED_DARK, font=5)
    L.tv(11450, G - 20)
    L.boombox(11550, G - 14)
    L.npc(11950, G, char=4, sleep=True)
    L.finish(11700, G)
    L.box(12900, G - 400, 13000, G, color=WOOD_DARK)
    return L


def lm_06():
    """Mow or Never (hard): the garden fights back. A mined flower bed crossed on loose I-beams,
    a shed with harpoon turrets, a gully whose boards drop behind you, a greenhouse of glass, a
    log avalanche, swinging wrecking-ball bird feeders, a gnome army, and a boosted final jump."""
    L = Level('06_lawnmower_man/06_mow_or_never.xml', 'Mow or Never', 6, (450, G - RIDE),
              bg=1, bgc=rgb('ffd9a8'))
    lawn_title(L, 180, G, 'MOW OR NEVER',
               'The lawn has unionised. It has demands.\n'
               'Its first demand is your spleen.', color=RED_DARK)
    # 1) the minefield: a sunken flower bed full of mines, crossed on a boardwalk of loose
    #    I-beams resting on posts (they shift under you)
    L.ground([(0, G), (1000, G)], bottom=G + 700, color=GRASS)
    L.box(1000, G + 160, 2400, G + 700, color=DIRT)
    L.box(1000, G + 20, 1030, G + 700, color=GRASS)                             # banks: the beam
    L.box(2370, G + 20, 2400, G + 700, color=GRASS)                             # ends rest on them
    for x in (1150, 1400, 1650, 1900, 2150):
        L.mine(x, G + 160)
    for x in (1350, 1700, 2050):
        L.box(x - 15, G + 20, x + 15, G + 160, color=WOOD_DARK)               # posts
    for k in range(4):
        L.ibeam(1000 + k * 350 + 175, G + 10, 352, 20, sleep=True)
    L.ground([(2400, G), (2600, G)], bottom=G + 700, color=GRASS)
    L.text(1000, G - 300, 'The flower bed is mined. The boardwalk\nis "mostly" nailed down.', size=14)
    # 2) garden shed with harpoon turrets in its windows
    L.ground([(2600, G), (4200, G)], bottom=G + 700, color=GRASS)
    L.box(3000, G - 420, 3500, G - 40, color=WOOD, outline=WOOD_DARK, inter=False)
    L.roof(3250, G - 420, 580, 140, color=WOOD_DARK)
    L.harpoon(3100, G - 300, rot=0)
    L.harpoon(3400, G - 200, rot=0)
    L.text(3020, G - 380, 'SHED OF DOOM', size=22, color=RED_DARK, font=5)
    # 3) gully with a bridge that drops once you are on it (trigger deletes its pins)
    gl, gr_, top = 4200, 5200, G
    L.box(gl, G + 500, gr_, G + 700, color=DIRT_DARK)
    L.spikes_on((gl + gr_) / 2, G + 500, count=60)
    L.box(gl - 40, G, gl, G + 700, color=GRASS)
    planks = 5
    span = (gr_ - gl) / planks
    boards = [L.rect(gl + k * span + span / 2, top + 12, span - 6, 24, color=WOOD_LIGHT, outline=WOOD_DARK)
              for k in range(planks)]
    # each board drops (set to non-fixed) once you are two boards further on; the last two go
    # together a moment after you reach the far bank: don't stop, don't reverse
    for k in range(planks - 2):
        L.trigger(gl + (k + 2) * span + 40, top - 120, 40, 240, [(boards[k], [(2,), (4, 0, 2, 0)])])
    L.trigger(gr_ + 60, top - 120, 40, 240, [(boards[3], [(2,)]), (boards[4], [(2,)])], delay=0.3)
    L.text(gl - 300, G - 330, 'This bridge has trust issues.\nDo not stop. Do not reverse.', size=14)
    # 4) greenhouse: glass panes to smash through, with potted cacti (spikes) on shelves
    L.ground([(gr_, G), (7200, G)], bottom=G + 700, color=GRASS)
    L.box(5600, G - 380, 6800, G, color=rgb('d8f3ff'), opacity=35, inter=False)
    L.roof(6200, G - 380, 1200, 140, color=rgb('d8f3ff'), opacity=35)
    for x in (5650, 6000, 6350, 6700):
        L.glass(x, G - 110, w=12, h=220, strength=3)
    L.box(6050, G - 270, 6300, G - 255, color=WOOD, fixed=True)
    L.spikes(6175, G - 290, 20, fixed=True)
    # 5) log avalanche: logs asleep on a slope above, woken as you pass under
    L.ground([(7200, G), (9400, G)], bottom=G + 700, color=GRASS)
    L.slab(7500, G - 700, 8600, G - 400, thick=60, color=DIRT)
    L.box(7440, G - 760, 7500, G - 640, color=DIRT)
    logs = [L.log(7600 + k * 130, G - 760 + k * 36, 120, 60, rot=15, sleep=True) for k in range(5)]
    L.trigger(8300, G - 100, 200, 200, [(lg, [(0,)]) for lg in logs])
    L.text(7600, 4300, 'Timber.', size=24, color=WOOD_DARK, font=5)
    # 6) bird feeders of doom: wrecking balls released by a trigger
    L.box(9400, G - 760, 11000, G - 720, color=WOOD_DARK)
    L.box(9400, G - 720, 9440, G, color=WOOD_DARK, inter=False)
    L.box(10960, G - 720, 11000, G, color=WOOD_DARK, inter=False)
    wb = [L.wrecking_ball(x, G - 700, rope=520) for x in (9800, 10300, 10750)]
    L.ground([(9400, G), (11800, G)], bottom=G + 700, color=GRASS)
    L.trigger(9500, G - 100, 150, 200, [(w, []) for w in wb])
    # 7) gnome army: a crowd to grind, then a ramp over the compost spike pit
    for k, x in enumerate(range(11000, 11800, 100)):
        L.npc(x, G, char=1 + (k * 3) % 16, sleep=True)
    L.slab(11800, G, 12200, G - 140, thick=60, color=GRASS)
    L.box(12200, G - 140, 12240, G + 700, color=GRASS)
    L.box(12240, G + 300, 12740, G + 700, color=DIRT_DARK)
    L.spikes_on(12490, G + 300, count=32)
    L.boost(12000, G - 70, panels=1, power=40, rot=-19)
    L.ground([(12740, G), (14200, G)], bottom=G + 700, color=GRASS)
    L.finish(13600, G)
    L.text(13250, G - 330, 'The lawn surrenders.\nIt is 2 mm long now.', size=18)
    L.box(14200, G - 400, 14300, G + 700, color=HEDGE)
    return L


# =============================================================================================
# 07 EXPLORER GUY (character 7): mine cart; space clamps the wheels onto rails (special 27, 18 px
# thick, so rails sit flush with the ground: p1 = surface + 9); shift / ctrl stand up / crouch.
# The cart is quick and its frame breaks on hard edges: keep the running surface smooth.
# =============================================================================================

def rail_span(L, x1, x2, top):
    """Rails whose top surface runs flat at `top` from x1 to x2 (pieces of at most 2000 px)."""
    refs = []
    n = max(1, int(math.ceil((x2 - x1) / 2000.0)))
    w = (x2 - x1) / float(n)
    for k in range(n):
        refs.append(L.rail(x1 + w * (k + 0.5), top + 9, w))
    return refs


def rail_slope(L, x1, y1, x2, y2):
    """One rail whose top edge runs from (x1, y1) to (x2, y2)."""
    dx, dy = x2 - x1, y2 - y1
    ln = math.hypot(dx, dy)
    ang = math.degrees(math.atan2(dy, dx))
    nx, ny = -dy / ln, dx / ln
    return L.rail((x1 + x2) / 2.0 + nx * 9, (y1 + y2) / 2.0 + ny * 9, min(2000, ln), rot=round(ang, 3))


def pit(L, x1, x2, ground, depth=500, color=DIRT_DARK, spikes=True, walls=STONE_DARK):
    """A deadly pit between two ground edges: walls, a floor and spikes."""
    L.box(x1, ground, x1 + 30, ground + depth + 100, color=walls)
    L.box(x2 - 30, ground, x2, ground + depth + 100, color=walls)
    L.box(x1, ground + depth, x2, ground + depth + 100, color=color)
    if spikes:
        n = max(20, min(150, int((x2 - x1 - 60) / 15)))
        L.spikes_on((x1 + x2) / 2.0, ground + depth, count=n)


def jungle_backdrop(L, x1, x2, ground):
    for k, x in enumerate(range(int(x1), int(x2), 700)):
        tree(L, x + (k * 137) % 300, ground, 420 + (k % 3) * 60, crown=[rgb('2d6a2d'), rgb('3a7d32'), rgb('25592a')][k % 3])


def temple_wall(L, x1, x2, top, bottom, color=TEMPLE_DARK):
    """Background masonry (art): a dark wall with block lines."""
    L.box(x1, top, x2, bottom, color=color, inter=False)
    y = top + 60
    while y < bottom:
        L.box(x1, y, x2, y + 4, color=rgb('4a3f2a'), inter=False, opacity=60)
        y += 60


def ex_01():
    """Temple Run (easy): jungle trail to a temple. A rail over a creek (clamp with space or just
    roll across), a gentle climb to the temple terrace, a boulder released behind you inside, and a
    downhill sprint out the back door to the finish."""
    L = Level('07_explorer_guy/01_temple_run.xml', 'Temple Run', 7, (450, G - RIDE), bg=1,
              bgc=rgb('cdeac0'))
    jungle_backdrop(L, 900, 4200, G)
    title(L, 160, G, 'TEMPLE RUN',
          'Somewhere in this jungle is a temple full of priceless artefacts.\n'
          'Somewhere in your contract it says "no refunds".\n'
          'Space clamps the cart onto rails. Shift / ctrl: stand / crouch.')
    L.ground([(0, G), (1900, G)], bottom=G + 600, color=GRASS)
    # creek crossed on a rail
    L.box(1900, G + 300, 2400, G + 600, color=DIRT_DARK)
    L.box(1900, G + 200, 2400, G + 300, color=WATER, opacity=70, inter=False)
    rail_span(L, 1880, 2420, G)
    L.text(1850, G - 230, 'Rails: hold space to clamp on.', size=15)
    L.ground([(2400, G), (3300, G), (3900, G - 160), (4600, G - 160)], bottom=G + 600, color=GRASS)
    for x in (2700, 2850, 3000):
        L.token(x, G - 60, 1)
    # the temple: terrace, facade, interior hall
    T = G - 160
    temple_wall(L, 4600, 7400, T - 520, T)
    L.box(4550, T - 600, 7450, T - 520, color=TEMPLE, outline=TEMPLE_DARK)       # roof slab (solid)
    L.roof(6000, T - 600, 3100, 260, color=TEMPLE)
    L.box(4550, T - 520, 4620, T - 220, color=TEMPLE)                             # door lintel post
    L.text(4700, T - 480, 'TEMPLE OF THE\nUNPAID INTERN', size=22, color=GOLD, font=5)
    L.ground([(4600, T), (6200, T)], bottom=G + 600, color=TEMPLE)
    # the boulder: asleep on a ledge behind you, woken when you pass the idol
    L.box(4620, T - 260, 5000, T - 230, color=TEMPLE)
    boulder = L.circle(4800, T - 380, 220, color=STONE, outline=STONE_DARK, sleep=True, density=4)
    L.box(5560, T - 120, 5640, T, color=TEMPLE_DARK, inter=False)
    L.box(5575, T - 150, 5625, T - 120, color=GOLD, inter=False)
    L.trigger(5600, T - 100, 120, 200, [(boulder, [(0,), (4, 6, 0, 2)])])
    note = L.text(5300, T - 330, 'You hear a rumble behind you.\nIt is not your stomach. RUN.', size=16,
                  color=RED_DARK, opacity=0)
    L.trigger(5640, T - 100, 40, 200, [show_text(note, 0.2)])
    # downhill out of the back door
    L.ground([(6200, T), (7400, G + 60), (8200, G + 60)], bottom=G + 700, color=TEMPLE)
    L.box(7380, T - 520, 7450, G - 250, color=TEMPLE)                              # back lintel
    L.ground([(8200, G + 60), (9800, G + 60)], bottom=G + 700, color=GRASS)
    jungle_backdrop(L, 8300, 9800, G + 60)
    L.finish(9200, G + 60)
    L.text(8700, G - 260, 'You escaped with your life.\nThe artefacts escaped with your dignity.', size=16)
    L.box(9800, G - 500, 9900, G + 700, color=STONE_DARK)
    return L


def ex_02():
    """Mine Shaft (easy+): an old mine. Timber-framed tunnels, a rail across a flooded shaft, a
    dynamite shelf, the Plunge (a smooth dive to the lower level), a door on a prismatic joint
    that slides up as you come down, loose rocks dropping behind you, a rail over a chasm, and
    daylight at the exit."""
    L = Level('07_explorer_guy/02_mine_shaft.xml', 'Mine Shaft', 7, (450, G - RIDE), bg=0,
              bgc=rgb('2b2118'))
    title(L, 160, G, 'MINE SHAFT', 'Closed since 1897 "for safety reasons".\n'
          'Those reasons are still down here.', color=rgb('f0e0c0'))
    ROCK = rgb('4a3a2a')
    # tunnel 1 (y = G), ceiling 380 above
    L.box(0, G - 800, 4200, G - 380, color=ROCK)
    L.ground([(0, G), (1800, G)], bottom=G + 700, color=ROCK)
    for x in range(400, 4200, 450):
        L.box(x, G - 380, x + 24, G, color=WOOD_DARK, inter=False)
        L.box(x - 30, G - 395, x + 54, G - 370, color=WOOD, inter=False)
        L.circle(x + 70, G - 330, 26, color=YELLOW, inter=False, opacity=80)
    # flooded shaft crossed on a rail
    L.box(1800, G + 500, 2500, G + 700, color=ROCK)
    L.box(1800, G + 150, 2500, G + 500, color=rgb('2d5a7a'), opacity=80, inter=False)
    rail_span(L, 1780, 2520, G)
    L.ground([(2500, G), (3780, G)], bottom=G + 700, color=ROCK)
    L.box(2850, G - 250, 3250, G - 230, color=WOOD)                              # dynamite shelf
    for x in (2920, 3050, 3180):
        L.mine(x, G - 250)
    L.text(2750, G - 340, 'Dynamite: do not touch.\nAlso do not breathe on it.', size=14, color=rgb('f0e0c0'))
    # the plunge: the tunnel dives down a smooth curve to the lower level
    B = G + 830
    curve = []
    for k in range(13):
        t = k / 12.0
        curve.append((round(3780 + 1500 * t), round(G + (B - G) * (1 - math.cos(math.pi * t)) / 2)))
    L.ground(curve, bottom=B + 600, color=ROCK)
    L.box(4200, G - 800, 5400, G - 380, color=ROCK)                             # ceiling continues
    L.ground([(5280, B), (5800, B)], bottom=B + 600, color=ROCK)
    L.box(5280, G - 380, 5510, B - 380, color=ROCK)                             # lower tunnel roof
    L.box(5590, G - 380, 7500, B - 380, color=ROCK)                             # (slot for the door)
    L.box(7460, B - 640, 7540, B - 380, color=WOOD_DARK, inter=False)          # mine mouth timbers
    L.text(3500, G - 330, 'THE PLUNGE. Brakes were\nnot invented until 1903.', size=15, color=YELLOW)
    # a sliding door (prismatic joint, motor) that lifts as you come down the plunge
    door = L.group()
    door.rect(5550, B - 190, 60, 376, color=WOOD, outline=WOOD_DARK, fixed=False, density=2)
    dj = L.slider(5550, B - 190, door, LEVEL, axis=-90, lower=0, upper=420, motor=True, force=1e6, speed=0)
    L.trigger(4300, G + 80, 80, 300, [(dj, [(1, 6, 0.2)])])
    L.text(5000, B - 330, 'The door opens itself.\nThe mine is haunted. Probably.', size=14, color=rgb('f0e0c0'))
    # loose rocks in the roof: they drop as you pass
    rocks = [L.circle(x, B - 300, 70, color=STONE, sleep=True, density=3) for x in (5000, 5100, 5180)]
    L.trigger(5250, B - 100, 80, 200, [(r, [(0,)]) for r in rocks])
    for x in range(5400, 7400, 450):
        L.box(x, B - 330, x + 24, B, color=WOOD_DARK, inter=False)
        L.circle(x + 70, B - 290, 26, color=YELLOW, inter=False, opacity=80)
    # chasm on a long rail
    pit(L, 5800, 6700, B, depth=600, color=ROCK)
    rail_span(L, 5780, 6720, B)
    L.ground([(6700, B), (7600, B), (8600, B - 330)], bottom=B + 600, color=ROCK)
    L.ground([(8600, B - 330), (10000, B - 330)], bottom=B + 600, color=GRASS)
    L.finish(9400, B - 330)
    L.text(8900, B - 600, 'Daylight! The canary would\nhave been so proud.', size=16, color=WHITE)
    L.box(10000, B - 900, 10100, B + 600, color=ROCK)
    return L


def ex_03():
    """Idol Hands (medium): the idol chamber. Grab the golden idol from its pedestal and the
    temple turns on you: dart guns in the walls, a boulder, spikes popping through the floor
    (glass covers that shatter), swinging axes (wrecking balls) and a rail over the final pit."""
    L = Level('07_explorer_guy/03_idol_hands.xml', 'Idol Hands', 7, (450, G - RIDE), bg=0,
              bgc=rgb('3a2e1c'))
    title(L, 160, G, 'IDOL HANDS', 'The Golden Idol of Ka-Ching.\n'
          'Legend says whoever takes it will be cursed.\nLegend also says it is worth $40 million.',
          color=rgb('f6e7b0'))
    temple_wall(L, 0, 9000, G - 700, G)
    L.box(0, G - 760, 9000, G - 640, color=TEMPLE, outline=TEMPLE_DARK)          # ceiling
    # Native control arrows: drive into the chamber, then use SPACE before each rail crossing.
    L.system_trigger(700, G - 130, 90, 300, 0)
    L.system_trigger(1050, G - 130, 90, 300, 5)
    L.ground([(0, G), (2600, G)], bottom=G + 600, color=TEMPLE)
    for x in (900, 1500):
        L.box(x, G - 640, x + 60, G, color=TEMPLE, inter=False)
    # the idol on its pedestal: a loose gold block over a trigger plate
    L.box(2370, G - 120, 2430, G, color=TEMPLE_DARK, inter=False)               # pedestal (art)
    idol = L.rect(2400, G - 150, 40, 60, color=GOLD, outline=rgb('8a6d00'), fixed=False, sleep=True, density=2)
    L.text(2200, G - 330, 'Drive into the idol.\nWhat could go wrong?', size=16, color=GOLD)
    # traps, armed by the idol plate (the player OR the idol leaving it)
    darts = [L.arrow_gun(x, G - 540, rot=90, fixed=True, rate=4) for x in (3000, 3500, 4000)]
    boulder = L.circle(1200, G - 560, 200, color=STONE, outline=STONE_DARK, sleep=True, density=5)
    L.box(1000, G - 450, 1400, G - 420, color=TEMPLE)
    note = L.text(2700, G - 330, 'The idol was the load-bearing\npart of the temple. Classic.', size=16,
                  color=RED, opacity=0)
    L.trigger(2400, G - 60, 160, 120, [(boulder, [(0,), (4, 5, 0, 1)]), show_text(note, 0.2)], by=1)
    # dart corridor (guns in the ceiling shoot down)
    L.ground([(2600, G), (4600, G)], bottom=G + 600, color=TEMPLE)
    # pop-up spikes under glass covers: the glass shatters when you roll over it
    for x in (3300, 4300):
        L.glass(x, G - 150, w=14, h=300, strength=3)
    pit(L, 4600, 5400, G, depth=400, color=TEMPLE_DARK, walls=TEMPLE)
    rail_span(L, 4580, 5420, G)
    L.text(4300, G - 330, 'Crystal doors and a spike pit.\nAncient engineering was mostly vibes.', size=15,
           color=rgb('f6e7b0'))
    L.system_trigger(4350, G - 130, 90, 300, 4)
    L.system_trigger(4700, G - 130, 90, 300, 5)
    # axes: wrecking balls swinging from the ceiling, released by a trigger
    L.ground([(5400, G), (7000, G)], bottom=G + 600, color=TEMPLE)
    axes = [L.wrecking_ball(x, G - 640, rope=500) for x in (5800, 6250, 6700)]
    L.trigger(5500, G - 100, 100, 200, [(a, []) for a in axes])
    # final pit with a rail and the exit ramp into daylight
    pit(L, 7000, 7900, G, depth=500, color=TEMPLE_DARK, walls=TEMPLE)
    rail_span(L, 6980, 7920, G)
    L.ground([(7900, G), (9000, G), (9600, G - 200), (11000, G - 200)], bottom=G + 600, color=TEMPLE)
    L.box(9000, G - 760, 9060, G - 300, color=TEMPLE)
    L.finish(10300, G - 200)
    L.text(9800, G - 520, 'Idol acquired. Curse acquired.\nNet worth: complicated.', size=16, color=BLACK)
    L.system_trigger(6850, G - 130, 90, 300, 4)
    L.system_trigger(7150, G - 130, 90, 300, 5)
    L.box(11000, G - 800, 11100, G + 600, color=STONE_DARK)
    for x in (9900, 10050):
        L.token(x, G - 260, 6)
    return L


def ex_04():
    """Rope Bridge (medium+): a canyon crossing in three acts. A long sagging plank bridge
    (planks pinned end to end) under swinging logs; a high rail you must clamp onto because the
    wind (fans) shoves you; and a bridge whose boards fall behind you."""
    L = Level('07_explorer_guy/04_rope_bridge.xml', 'Rope Bridge', 7, (450, G - RIDE), bg=1,
              bgc=rgb('f6d7a7'))
    title(L, 160, G, 'ROPE BRIDGE', 'Built by the ancients. Maintained by nobody.\n'
          'Weight limit: one explorer, no snacks.')
    CLIFF = rgb('b5733a')
    L.ground([(0, G), (1400, G)], bottom=G + 1400, color=CLIFF)
    # act 1: plank bridge 1400..3400 over the river
    L.box(1400, G + 1200, 3400, G + 1400, color=rgb('7a4a22'))
    L.box(1400, G + 1050, 3400, G + 1200, color=WATER, opacity=80, inter=False)
    L.spikes_on(2400, G + 1200, count=120)
    planks, span = 10, 200
    prev = LEVEL
    for k in range(planks):
        g = L.group()
        g.rect(1400 + k * span + span / 2, G + 10, span - 6, 20, color=WOOD_LIGHT, outline=WOOD_DARK,
               fixed=False, density=4)
        L.pin(1400 + k * span + 3, G + 10, g, prev)
        prev = g
    L.pin(3400 - 3, G + 10, prev, LEVEL)
    L.box(1330, G - 200, 1350, G, color=WOOD_DARK, inter=False)
    L.box(3450, G - 200, 3470, G, color=WOOD_DARK, inter=False)
    logs = [L.wrecking_ball(x, G - 820, rope=640) for x in (2100, 2800)]
    L.trigger(1500, G - 100, 100, 200, [(lg, []) for lg in logs])
    L.text(1500, G - 330, 'Swinging logs. The ancients had\na very specific sense of humour.', size=15)
    # act 2: the high rail across the gorge, with crosswind fans below
    L.ground([(3400, G), (4200, G), (4600, G - 200)], bottom=G + 1400, color=CLIFF)
    L.box(4600, G + 1200, 6600, G + 1400, color=rgb('7a4a22'))
    L.spikes_on(5600, G + 1200, count=130)
    rail_span(L, 4580, 6620, G - 200)
    for x in (5100, 5900):
        L.box(x - 160, G + 200, x + 160, G + 1200, color=CLIFF, outline=rgb('8a5528'))
        L.fan(x, G + 200)
    L.text(3900, G - 330, 'Hold SPACE on the rail.\nThe wind is not your friend.', size=16, color=RED_DARK)
    # act 3: boards that fall behind you
    L.ground([(6600, G - 200), (7000, G - 200), (7300, G)], bottom=G + 1400, color=CLIFF)
    L.box(7300, G + 1200, 8800, G + 1400, color=rgb('7a4a22'))
    L.spikes_on(8050, G + 1200, count=100)
    boards = [L.rect(7300 + k * 150 + 75, G + 12, 144, 24, color=WOOD_LIGHT, outline=WOOD_DARK)
              for k in range(10)]
    for k in range(8):
        L.trigger(7300 + (k + 2) * 150 + 30, G - 120, 30, 240, [(boards[k], [(2,)])])
    L.trigger(8830, G - 120, 30, 240, [(boards[8], [(2,)]), (boards[9], [(2,)])], delay=0.2)
    L.text(7000, G - 300, 'These boards are load-bearing\nfor exactly one crossing.', size=15)
    L.ground([(8800, G), (10400, G)], bottom=G + 1400, color=CLIFF)
    L.finish(9800, G)
    L.box(10400, G - 600, 10500, G + 1400, color=CLIFF)
    for x in (5200, 5600, 6000):
        L.token(x, G - 260, 2)
    return L


def ex_05():
    """The Lost City (hard): ruins on stilts. Rails between crumbling pillars (one pillar is loose
    and topples when touched), a motorised stone cross turning over the path, stone guardians
    (NPC statues), wandering spirit lights (homing mines), jets on a falling statue head, and a
    switch that raises the drawbridge to the golden throne."""
    L = Level('07_explorer_guy/05_lost_city.xml', 'The Lost City', 7, (450, G - RIDE), bg=1,
              bgc=rgb('ffe2b0'))
    title(L, 160, G, 'THE LOST CITY', 'Found it. Unfortunately, it also found you.\n'
          'Mind the pillars. Mind the cross. Mind everything.')
    L.ground([(0, G), (1500, G)], bottom=G + 1200, color=TEMPLE)
    for x in range(200, 1400, 400):
        L.box(x, G - 400, x + 60, G, color=TEMPLE_DARK, inter=False)
    # pillars with rails between them (rails flush with the pillar tops)
    tops = [(1500, 1800), (2400, 2600), (3200, 3400), (4000, 4300)]
    L.box(1500, G + 1100, 4300, G + 1200, color=TEMPLE_DARK)
    L.spikes_on(2900, G + 1100, count=150)
    for x1, x2 in tops:
        L.box(x1, G, x2, G + 1100, color=TEMPLE, outline=TEMPLE_DARK)
    for (a1, a2), (b1, b2) in zip(tops, tops[1:]):
        rail_span(L, a2 - 20, b1 + 20, G)
    # the loose pillar segment: a stack that topples if bumped (decor + chaos above the path)
    L.box(2560, G - 200, 2600, G, color=TEMPLE_DARK, inter=False)              # broken column (art)
    L.text(1700, G - 330, 'Clamp on (space) and keep\nyour speed up between pillars.', size=15)
    # turning stone cross over a flat plaza
    L.ground([(4300, G), (6400, G)], bottom=G + 1200, color=TEMPLE)
    cx, cy = 5300, G - 330
    cross = L.group()
    cross.rect(cx, cy, 560, 50, color=STONE, outline=STONE_DARK, fixed=False, density=2)
    cross.rect(cx, cy, 50, 560, color=STONE, outline=STONE_DARK, fixed=False, density=2)
    L.pin(cx, cy, cross, LEVEL, motor=True, torque=3e6, speed=0.8)
    L.box(cx - 20, cy, cx + 20, G - 300 + 290, color=TEMPLE_DARK, inter=False)
    L.text(4500, G - 400, 'Pass under the cross\nwhen its arms are up.', size=15)
    # guardians and spirits
    for x in (5900, 6250):                      # guardian statues on ledges over the path
        L.box(x - 80, G - 330, x + 80, G - 300, color=TEMPLE, outline=TEMPLE_DARK)
        L.npc(x, G - 330, char=11, sleep=True, hold=True, pose=(0, -170, -170, 0, 0, 0, 0, 0, 0))
    L.homing_mine(6600, G - 300, speed=1, delay=2)
    L.homing_mine(7400, G - 250, speed=2, delay=2)
    # the giant head on a ledge above the stairs: woken (with its jet) once you are past, it
    # tumbles down after you
    L.ground([(6400, G), (7600, G + 200), (8600, G + 200)], bottom=G + 1200, color=TEMPLE)
    L.box(6250, G - 420, 6650, G - 380, color=TEMPLE, outline=TEMPLE_DARK)
    head = L.group(sleep=True)
    head.rect(6450, G - 500, 200, 220, color=GOLD, outline=rgb('8a6d00'), fixed=False, density=2)
    head.rect(6400, G - 540, 36, 28, color=BLACK, fixed=False)
    head.rect(6500, G - 540, 36, 28, color=BLACK, fixed=False)
    jet = L.jet(6340, G - 500, rot=-90, sleep=True, power=2, fire_time=10)
    L.pin(6345, G - 500, jet, head)
    L.trigger(6900, G + 50, 60, 300, [(head, [(0,), (2, 6, 0, 2)]), (jet, [(0,)])])
    L.text(6600, G - 330, 'Is that statue... following you?', size=15)
    # drawbridge: a hinged plank over the moat, raised until you hit the switch
    L.box(8600, G + 1100, 9400, G + 1200, color=TEMPLE_DARK)
    L.spikes_on(9000, G + 1100, count=50)
    L.box(8600, G + 200, 8640, G + 1200, color=TEMPLE)
    bridge = L.group()
    bridge.rect(9000, G + 212, 780, 24, color=WOOD, outline=WOOD_DARK, fixed=False, density=2)
    # pinned at its far end; the motor holds it raised (clockwise = left end up) until the switch
    hinge = L.pin(9390, G + 212, bridge, LEVEL, limit=True, upper=80, lower=0, motor=True, torque=5e5, speed=1)
    L.box(9400, G + 200, 9440, G + 1200, color=TEMPLE)
    L.box(8300, G + 140, 8360, G + 200, color=RED, outline=RED_DARK)
    L.trigger(8330, G + 120, 80, 120, [(hinge, [(0,)])])
    L.text(8100, G - 140, 'Hit the red switch to\nlower the drawbridge.', size=15)
    L.ground([(9400, G + 200), (11200, G + 200)], bottom=G + 1200, color=TEMPLE)
    L.box(10300, G - 100, 10500, G + 200, color=GOLD, inter=False)
    L.box(10280, G - 300, 10520, G - 100, color=GOLD, outline=rgb('8a6d00'), inter=False)
    L.finish(10000, G + 200)
    L.text(9600, G - 80, 'The Golden Throne!\nSlightly sticky.', size=16)
    L.box(11200, G - 600, 11300, G + 1200, color=TEMPLE_DARK)
    for x in (2000, 2900, 3700):
        L.token(x, G - 120, 3)
    return L


def ex_06():
    """Tomb Raided (hard): the tomb collapses around you. Ceiling blocks fall in sequence,
    rails cross lava pits, a dart-gun gauntlet, harpoon-wielding mummies, a ceiling of spikes you
    pass crouched, and a boost-panel leap out of the pyramid."""
    L = Level('07_explorer_guy/06_tomb_raided.xml', 'Tomb Raided', 7, (450, G - RIDE), bg=0,
              bgc=rgb('1e140c'))
    title(L, 160, G, 'TOMB RAIDED', 'You opened the sarcophagus. The pharaoh has filed a complaint.\n'
          'The complaint is the entire building falling on you.', color=rgb('f6e7b0'))
    temple_wall(L, 0, 12800, G - 700, G, color=rgb('3e3220'))
    L.box(0, G - 760, 12800, G - 640, color=TEMPLE_DARK)
    L.ground([(0, G), (1500, G)], bottom=G + 700, color=TEMPLE)
    L.box(700, G - 100, 900, G, color=GOLD, inter=False)
    L.box(680, G - 120, 920, G - 100, color=rgb('8a6d00'), inter=False)
    # collapsing ceiling: blocks woken one after another as you go
    blocks = [L.rect(x, G - 580, 180, 100, color=TEMPLE, outline=TEMPLE_DARK, fixed=False, sleep=True,
                     density=4) for x in range(1300, 3300, 250)]
    for k, b in enumerate(blocks):
        L.trigger(1450 + k * 250, G - 100, 40, 200, [(b, [(0,)])])        # just behind the rider
    L.ground([(1500, G), (3300, G)], bottom=G + 700, color=TEMPLE)
    L.text(1500, G - 330, 'Do not stop.\nThe ceiling has opinions.', size=16, color=RED)
    # lava pits on rails
    for x1, x2 in ((3300, 4100), (4500, 5400)):
        L.box(x1, G + 200, x2, G + 700, color=rgb('5a1a0a'))
        L.box(x1, G + 80, x2, G + 200, color=LAVA, inter=False, opacity=95)
        L.spikes_on((x1 + x2) / 2, G + 200, count=min(150, int((x2 - x1) / 15) - 2))
        rail_span(L, x1 - 20, x2 + 20, G)
    L.ground([(4100, G), (4500, G)], bottom=G + 700, color=TEMPLE)
    L.ground([(5400, G), (7600, G)], bottom=G + 700, color=TEMPLE)
    # dart-gun gauntlet: guns low in the walls, firing across the corridor
    for x in (5800, 6300, 6800, 7300):
        L.arrow_gun(x, G - 520, rot=90, fixed=True, rate=6)
    L.text(5600, G - 330, 'The darts are poisoned.\nThe poison is also darts.', size=15, color=rgb('f6e7b0'))
    # mummies with harpoons
    L.ground([(7600, G), (9200, G)], bottom=G + 700, color=TEMPLE)
    for x in (8000, 8600):
        L.npc(x, G, char=5, sleep=True, hold=True, pose=(0, -90, -90, 0, 0, 0, 0, 0, 0))
    L.harpoon(8300, G - 560, rot=180)
    L.harpoon(9000, G - 560, rot=180)
    # spiked ceiling section: lower roof, spikes pointing down (crouch!)
    L.box(9200, G - 640, 10400, G - 260, color=TEMPLE_DARK)
    L.spikes(9800, G - 245, 70, rot=180, fixed=True)
    L.ground([(9200, G), (10400, G)], bottom=G + 700, color=TEMPLE)
    L.text(8800, G - 330, 'CTRL to crouch.\nTrust us.', size=16, color=RED)
    # the exit: up the ramp with a boost over the final shaft, out into the desert
    L.ground([(10400, G), (11200, G), (11800, G - 200)], bottom=G + 700, color=TEMPLE)
    L.boost(11500, G - 100, panels=2, power=40, rot=-18)
    pit(L, 11800, 12500, G - 200, depth=900, color=rgb('5a1a0a'), walls=TEMPLE)
    L.box(12500, G - 200, 12800, G + 700, color=TEMPLE)
    L.ground([(12500, G - 200), (14400, G - 200)], bottom=G + 700, color=SAND)
    L.finish(13700, G - 200)
    L.text(13100, G - 500, 'Out! The tomb is now a\nslightly flatter tomb.', size=16, color=BLACK)
    L.box(14400, G - 800, 14500, G + 700, color=SAND)
    return L


# =============================================================================================
# 08 SANTA CLAUS (character 8): sleigh pulled by two elves; space flies while the boost meter
# lasts (about three seconds, it refills on the ground); shift lets the elves go.
# =============================================================================================

def night_sky(L, x1, x2, y1, y2, moon=None):
    stars(L, x1, x2, y1, y2, n=int((x2 - x1) / 120))
    if moon:
        L.circle(moon[0], moon[1], 260, color=rgb('fdf6c8'), inter=False)
        L.circle(moon[0] + 70, moon[1] - 40, 220, color=NIGHT, inter=False)


def flat_house(L, x1, x2, top, bottom, wall=BRICK, lights=True, chimney=None, solid_walls=True):
    """A house seen from the side whose flat snowy roof is part of the course. With
    solid_walls=False only the roof slab is solid (a street can run past in front of it)."""
    if solid_walls:
        L.box(x1, top, x2, bottom, color=wall, outline=rgb('5a2a14'))
    else:
        L.box(x1, top + 40, x2, bottom, color=wall, outline=rgb('5a2a14'), inter=False)
        L.box(x1, top, x2, top + 40, color=wall, outline=rgb('5a2a14'))
    L.box(x1 - 10, top - 14, x2 + 10, top + 6, color=SNOW, inter=False)
    w = x2 - x1
    for k in range(int(w // 220)):
        wx = x1 + 60 + k * 220
        if wx + 90 > x2 - 30:
            break
        L.box(wx, top + 80, wx + 90, top + 190, color=rgb('ffd36b'), outline=rgb('5a2a14'), inter=False)
    if lights:
        for k in range(int(w // 60) + 1):
            L.circle(x1 + k * 60, top + 18, 14, color=[RED, GREEN, YELLOW, BLUE][k % 4], inter=False)
    if chimney is not None:
        L.box(chimney - 45, top - 150, chimney + 45, top - 6, color=BRICK, outline=rgb('5a2a14'), inter=False)


def billboard_gun(L, x, roof, rot=0):
    """A harpoon turret on a billboard 250 px above a roof: it shoots, it does not block."""
    L.box(x - 8, roof - 250, x + 8, roof, color=METAL_DARK, inter=False)
    L.box(x - 80, roof - 270, x + 80, roof - 250, color=METAL_DARK)
    L.box(x - 80, roof - 380, x + 80, roof - 270, color=rgb('c0392b'), outline=WHITE, inter=False)
    return L.harpoon(x, roof - 290, rot=rot)


def snowman(L, x, ground, inter=False):
    # decoration by default: snowballs piled in front of a sleigh stop it dead
    L.circle(x, ground - 50, 100, color=SNOW, outline=ICE, sleep=True, density=0.5, inter=inter)
    L.circle(x, ground - 135, 74, color=SNOW, outline=ICE, sleep=True, density=0.5, inter=inter)
    L.circle(x, ground - 197, 52, color=SNOW, outline=ICE, sleep=True, density=0.5, inter=inter)
    L.tri(x + 30, ground - 197, 12, 40, rot=90, color=ORANGE, inter=False)


def present(L, x, ground, size=70, color=RED, ribbon=YELLOW, sleep=True):
    r = L.rect(x, ground - size / 2, size, size, color=color, outline=ribbon, sleep=sleep, fixed=False,
               density=0.6)
    return r


def sn_01():
    """Rooftop Run (easy): a row of joined snowy roofs, one wide gap that teaches flight, presents
    (tokens), a chimney delivery that pops a thank-you. Miss the jump and a snow chute slides you
    down to the street, which runs under the houses to its own finish line."""
    R, S = G, G + 520
    L = Level('08_santa_claus/01_rooftop_run.xml', 'Rooftop Run', 8, (450, R - RIDE), bg=0, bgc=NIGHT)
    night_sky(L, 0, 9600, 3600, 4700, moon=(1400, 3900))
    title(L, 160, R, 'ROOFTOP RUN', "It's Christmas Eve and you are 400 years old.\n"
          'Ride the roofs. Hold SPACE to fly while the meter lasts.\n'
          'Shift lets the elves go. They will not come back.', color=WHITE)
    L.box(0, R, 1300, S + 400, color=SNOW, outline=ICE)                       # snowy hill top
    L.box(1300, S, 10600, S + 400, color=SNOW, outline=ICE)                    # the street below
    roofs = [(1300, 2400), (2400, 3600), (3600, 4600), (5200, 6300), (6300, 7300)]   # joined except the big gap
    walls = [BRICK, rgb('6b8fb0'), rgb('8a5a9a'), BRICK, rgb('4f7a55')]
    for (x1, x2), w in zip(roofs, walls):
        flat_house(L, x1, x2, R, S, wall=w, chimney=(x1 + x2) / 2 + 150, solid_walls=False)
    L.text(4250, R - 330, 'Big gap ahead! Hold SPACE before\nthe edge and keep holding.', size=16, color=YELLOW)
    for x in (2400, 3620, 4750, 4900, 5050, 6300):
        L.token(x, R - 90, 2)
    # last roof slopes down to the street, finish at the tree
    # a long slide down from the last roof, high enough above the street that the street route
    # passes underneath to its own finish line; the slide lands at a second finish
    L.slab(7300, R, 9400, S, thick=40, color=SNOW, outline=ICE)
    pine(L, 10200, S, 520, snow=True)
    L.finish(8100, S)
    L.finish(9800, S)
    thanks = L.text(5300, R - 300, 'Ho ho ho! Chimney #4: delivered.\n(It was socks again.)', size=16,
                    color=WHITE, opacity=0)
    L.trigger(5850, R - 200, 160, 260, [show_text(thanks)])
    for x in (2400, 4900, 7600):
        snowman(L, x, S)
    L.text(1600, S - 250, 'Down here? The street\nleads to the finish too.', size=14, color=WHITE)
    # the big gap has a snow chute: fall in and you slide gently down to the street
    L.slab(4650, R + 120, 5150, S, thick=40, color=SNOW, outline=ICE)
    L.box(4600, R + 120, 4650, R + 160, color=SNOW, inter=False)
    L.box(10600, S - 600, 10700, S + 400, color=ICE)
    return L


def sn_02():
    """Chimney Drop (easy+): Santa does it the traditional way. Float down a wide chimney (hold
    space to slow the fall) onto a slide into a living room (sleeping dad, cookies, milk, the
    tree), out the front door, fly up onto the next roof and drop down a narrower chimney to the
    finish beside the second tree."""
    R, F = G, G + 700
    L = Level('08_santa_claus/02_chimney_drop.xml', 'Chimney Drop', 8, (450, R - RIDE), bg=0, bgc=NIGHT)
    night_sky(L, 0, 9800, 3700, 4700, moon=(7600, 3950))
    title(L, 160, R, 'CHIMNEY DROP', 'Chimneys: the only door Santa respects.\n'
          'Hold SPACE while falling. Santa\'s knees are 400 years old.', color=WHITE)

    def chimney_house(x1, x2, sx1, sx2, wall):
        """House from x1 to x2 with a chimney shaft from sx1 to sx2 down to a slide into the room."""
        L.box(x1, R + 60, x2, F, color=rgb('f3e2c2'), inter=False)              # wallpaper (behind)
        L.box(x1, R, sx1, R + 60, color=wall, outline=rgb('5a2a14'))           # roof left of the shaft
        L.box(x1 - 10, R - 14, sx1, R + 6, color=SNOW, inter=False)
        L.box(sx1 - 40, R, sx1, F - 420, color=BRICK, outline=rgb('5a2a14'))       # shaft left wall (flush)
        L.box(sx1 - 40, R - 180, sx1, R, color=BRICK, inter=False)              # chimney lip (art)
        L.box(sx2, R - 180, sx2 + 40, F - 300, color=BRICK, outline=rgb('5a2a14'))   # shaft right wall
        L.ground([(sx1, F - 420), (sx2 + 300, F)], bottom=F + 300, color=BRICK)    # the slide
        L.box(sx2 + 40, R, x2, R + 60, color=wall, outline=rgb('5a2a14'))       # roof right of the shaft
        L.box(sx2 + 40, R - 14, x2 + 10, R + 6, color=SNOW, inter=False)
        L.box(sx2 + 300, F, x2, F + 300, color=WOOD)                            # floor
        L.box(x2 - 40, R + 60, x2, F - 280, color=wall)                          # front wall above the door
        return x2

    # house 1: start roof leads to a wide chimney
    L.box(0, R, 600, F + 300, color=SNOW, outline=ICE)
    chimney_house(600, 4300, 1500, 2100, BRICK)
    L.text(1150, R - 330, 'Drive into the chimney.\nYes, really.', size=16, color=YELLOW)
    # living room 1
    L.box(2500, F - 200, 2700, F, color=rgb('6b3b2b'), inter=False)        # fireplace
    L.box(2540, F - 120, 2660, F, color=ORANGE, inter=False, opacity=80)
    # the furniture is decoration (non-interactive): a sleigh shoving a sofa goes nowhere
    L.chair(3000, F, inter=False)
    L.npc(3000, F, char=6, sleep=True, hold=True, inter=False, pose=(10, -40, -40, -60, -60, -90, -90, 90, 90),
          y=F - 80)
    L.text(2800, F - 330, 'Dad is "waiting up for Santa".\nDad is asleep.', size=15)
    L.table(3500, F, inter=False)
    L.food(3480, F - 78, kind=3, inter=False)
    L.bottle(3540, F - 72, 4, inter=False)
    pine(L, 3950, F, 360)
    for x, y in ((3900, F - 160), (3990, F - 230), (3950, F - 110)):
        L.circle(x, y, 22, color=RED, inter=False)
    # front yard, snowdrift ramp up to the roof of house 2
    L.box(4300, F, 4450, F + 300, color=SNOW, outline=ICE)
    L.ground([(4450, F), (7000, R)], bottom=F + 300, color=SNOW, outline=ICE)
    L.text(4500, F - 330, 'Up the big snowdrift to the next roof.\nSPACE makes climbing easier.', size=16,
           color=YELLOW)
    snowman(L, 4380, F)
    # house 2 with a narrower chimney
    chimney_house(7000, 10600, 8000, 8450, rgb('6b8fb0'))
    pine(L, 10000, F, 380)
    L.tv(9300, F - 20, inter=False)
    L.finish(9700, F)
    L.text(9100, F - 330, 'Presents delivered.\nCookies confiscated.', size=16)
    L.box(10600, R - 400, 10700, F + 300, color=BRICK)
    return L


def sn_03():
    """Workshop Chaos (medium): the toy factory on December 24th. Conveyor rollers, elves at
    their benches, a toy rocket (a jet pinned to a crate), a stamping press that slams as you
    approach and lifts again (two chained triggers), spring boxes to the gift shelf, a paddle
    gift-launcher and a boost runway out of the doors."""
    F = G
    L = Level('08_santa_claus/03_workshop_chaos.xml', 'Workshop Chaos', 8, (450, F - RIDE), bg=0,
              bgc=rgb('3b2416'))
    title(L, 160, F, 'WORKSHOP CHAOS', 'Toy production is 600% behind schedule.\n'
          'The elves blame "supply chains". The elves ARE the supply chain.', color=rgb('ffe9b0'))
    L.box(0, F - 800, 11000, F - 700, color=WOOD_DARK)                          # ceiling
    L.box(0, F - 700, 11000, F, color=rgb('5c3a22'), inter=False)              # back wall
    for x in range(300, 11000, 900):
        L.box(x, F - 700, x + 30, F, color=WOOD_DARK, inter=False)
        L.circle(x + 120, F - 600, 40, color=YELLOW, inter=False, opacity=70)
    L.box(0, F, 11000, F + 400, color=WOOD)
    # conveyor: rollers in a trench carrying you (and the crates) along
    L.box(1200, F, 2700, F + 400, color=rgb('3a2a1a'))
    for k in range(12):
        x = 1260 + k * 120
        r = L.circle(x, F + 58, 116, color=METAL, outline=METAL_DARK, density=2)
        L.pin(x, F + 58, r, LEVEL, motor=True, torque=40000, speed=-4)          # tops turn forward
    for x in (1500, 1900, 2300):
        present(L, x, F, 60, color=[RED, GREEN, BLUE][x % 3], sleep=False)
    L.text(1250, F - 330, 'Conveyor of Joy. Mind your fingers.\nElves count theirs every morning.', size=15,
           color=rgb('ffe9b0'))
    # elves at their benches (the benches are scenery; one elf is in the way)
    for k, x in enumerate((3000, 3400, 3800)):
        L.table(x, F, inter=False)
        L.npc(x + 90, F, char=[2, 8, 14][k], sleep=True, inter=(k == 1))
    L.tv(3000, F - 76, inter=False)
    L.boombox(3400, F - 70, inter=False)
    # toy rocket: a crate with a jet pinned under it on a shelf, lit as you pass below
    L.box(4350, F - 330, 4650, F - 300, color=WOOD_DARK)
    crate = L.rect(4500, F - 370, 120, 80, color=RED, outline=YELLOW, fixed=False, density=1)
    jet = L.jet(4500, F - 425, rot=-90, power=6, fire_time=30, sleep=True)
    L.pin(4500, F - 412, jet, crate)
    L.trigger(4450, F - 100, 60, 200, [(jet, [(0,)]), (crate, [(0,)])])
    L.text(3900, F - 330, 'Quality control: the rocket.\nIt passed. Upwards.', size=15, color=rgb('ffe9b0'))
    # stamping press: a heavy block on a vertical slider; slams then lifts
    px = 5600
    press = L.group()
    press.rect(px, F - 520, 360, 200, color=METAL_DARK, outline=BLACK, fixed=False, density=10)
    press.rect(px, F - 400, 300, 40, color=RED, fixed=False, density=10)
    L.box(px - 30, F - 700, px + 30, F - 620, color=METAL_DARK, inter=False)
    pj = L.slider(px, F - 520, press, LEVEL, axis=90, lower=0, upper=330, motor=True, force=5e6, speed=-2)
    lift = L.trigger(px, F - 300, 40, 40, [(pj, [(1, -3, 0.2)])], by=5, delay=1.2)
    L.trigger(px - 700, F - 100, 60, 200, [(pj, [(1, 6, 0.1)]), (lift, [(0,)])], repeat=2)
    L.text(px - 500, F - 330, 'THE GIFT FLATTENER\nwait for it to rise.', size=16, color=RED)
    # spring boxes to the gift shelf (optional tokens)
    L.box(6600, F - 420, 7600, F - 390, color=WOOD_DARK)
    L.spring_box(6500, F)
    for x in (6800, 7000, 7200, 7400):
        L.token(x, F - 460, 5)
    # paddle launcher and the doors
    L.paddle(8200, F, delay=0.2, angle=40, speed=7)
    L.text(8000, F - 330, 'Gift launcher. Gifts or Santas.\nIt does not discriminate.', size=15,
           color=rgb('ffe9b0'))
    L.box(9300, F - 700, 9340, F - 300, color=WOOD_DARK)                       # door frame
    L.boost(9700, F, panels=3, power=30)
    L.finish(10500, F)
    L.text(9900, F - 330, 'Loading dock. Sleigh clearance\ngranted. Reindeer on strike.', size=15,
           color=rgb('ffe9b0'))
    L.box(10900, F - 700, 11000, F, color=WOOD_DARK)
    return L


def sn_04():
    """North Pole (medium+): the great outdoors. Icy hills, polar fans blowing you back up a
    slope, a cave of icicles that drop behind you, a frozen lake of glass that cracks under the
    sleigh (fly!), snowmen to bowl over, and the actual pole at the finish."""
    L = Level('08_santa_claus/04_north_pole.xml', 'North Pole', 8, (450, G - RIDE), bg=0, bgc=rgb('16294f'))
    night_sky(L, 0, 12000, 3500, 4500)
    for k in range(6):                                                         # aurora
        L.box(1000 + k * 1800, 3600 + (k % 2) * 80, 2400 + k * 1800, 3680 + (k % 2) * 80,
              color=[rgb('4cd38a'), rgb('6ad1c9')][k % 2], inter=False, opacity=35)
    title(L, 160, G, 'NORTH POLE', 'Home sweet home. Average temperature: "no".\n'
          'The lake is frozen. Mostly.', color=WHITE)
    L.ground([(0, G), (1500, G)] + hill_profile(1500, 3500, G, 260, 1.0, 200)[1:], bottom=G + 800,
             color=SNOW, outline=ICE)
    for x in (1900, 2500, 3200):
        snowman(L, x, G - 260 * math.sin(math.pi * (x - 1500) / 2000.0) ** 2)
    # polar updraft: a crevasse with fans blowing up out of it; hold SPACE and float across
    L.ground([(3500, G), (4300, G)], bottom=G + 800, color=SNOW, outline=ICE)
    L.box(4300, G + 700, 5100, G + 800, color=rgb('9cc9e6'))
    L.box(4300, G, 4330, G + 800, color=ICE)
    L.box(5070, G, 5100, G + 800, color=ICE)
    for x in (4500, 4900):
        L.box(x - 160, G + 450, x + 160, G + 700, color=ICE)
        L.fan(x, G + 450)
    L.spikes_on(4700, G + 450, count=20)
    L.text(3700, G - 330, 'Polar updraft. Hold SPACE over\nthe crevasse and let it carry you.', size=15,
           color=WHITE)
    L.ground([(5100, G), (5800, G - 120), (6300, G - 120)], bottom=G + 800, color=SNOW, outline=ICE)
    # icicle cave
    L.ground([(6300, G - 120), (6600, G)], bottom=G + 800, color=SNOW, outline=ICE)
    L.box(6400, G - 900, 8400, G - 480, color=rgb('9cc9e6'), outline=WHITE)
    L.ground([(6600, G), (8400, G)], bottom=G + 800, color=ICE, outline=WHITE)
    icicles = [L.tri(x, G - 430, 50, 150, rot=180, color=rgb('dff4ff'), outline=WHITE, fixed=False, sleep=True,
                     density=2) for x in range(6700, 8300, 160)]
    for k, ic in enumerate(icicles):
        L.trigger(6700 + k * 160 + 220, G - 100, 40, 200, [(ic, [(0,)])])
    L.text(6700, G - 330, 'Icicles. Pointy end down.\nStop for nothing.', size=15, color=NIGHT)
    # frozen lake: glass panes over freezing water and spikes; fly if they crack
    L.ground([(8400, G), (8800, G)], bottom=G + 800, color=SNOW, outline=ICE)
    lake1, lake2 = 8800, 10000
    L.box(lake1, G + 600, lake2, G + 800, color=rgb('0f2a44'))
    L.box(lake1, G + 20, lake2, G + 600, color=rgb('2f6fa0'), inter=False, opacity=85)
    L.spikes_on((lake1 + lake2) / 2, G + 600, count=78)
    for k in range(6):
        L.glass(lake1 + 100 + k * 200, G + 6, w=12, h=200, rot=90, strength=2, stab=False)
    L.box(lake1 - 20, G, lake1, G + 800, color=ICE)
    L.box(lake2, G, lake2 + 20, G + 800, color=ICE)
    L.text(8500, G - 330, 'Thin ice. Fly across it,\nor skate and pray.', size=16, color=YELLOW)
    L.ground([(lake2, G), (12000, G)], bottom=G + 800, color=SNOW, outline=ICE)
    # the pole
    for k in range(8):
        L.box(11000, G - 420 + k * 50, 11040, G - 370 + k * 50, color=[RED, WHITE][k % 2], inter=False)
    L.circle(11020, G - 440, 50, color=GOLD, inter=False)
    L.finish(10800, G)
    L.text(10500, G - 360, 'The actual North Pole.\nSmaller than advertised.', size=16, color=WHITE)
    L.box(12000, G - 600, 12100, G + 800, color=ICE)
    return L


def sn_05():
    """Naughty List (hard-ish): coal deliveries to three naughty houses. Sleigh up the long snow
    ramps onto each roof and fly through the glowing zone over the chimney: the third delivery
    opens the gate to the finish. The naughty kids have prepared: mines on a roof, harpoon guns
    on another, a homing drone, and a wrecking-ball swing set by the gate."""
    S = G + 520
    R = S - 260                    # low houses: the sleigh climbs about 15 degrees, not more
    L = Level('08_santa_claus/05_naughty_list.xml', 'Naughty List', 8, (450, S - RIDE), bg=0, bgc=NIGHT)
    night_sky(L, 0, 12000, 3800, 4800, moon=(2200, 4100))
    title(L, 160, S, 'NAUGHTY LIST', 'Three houses. Three lumps of coal.\n'
          'Fly through the glow over each chimney.\nThe kids saw you coming.', color=WHITE)
    L.ground([(0, S), (12000, S)], bottom=S + 400, color=SNOW, outline=ICE)
    deliveries = []
    houses = [(1500, 2600), (4600, 5700), (7700, 8800)]
    for n, (x1, x2) in enumerate(houses):
        L.ground([(x1 - 900, S), (x1, R)], bottom=S, color=SNOW, outline=ICE)       # ramp up
        flat_house(L, x1, x2, R, S, wall=[rgb('5a4a6b'), rgb('4a5a4a'), rgb('6b3b3b')][n], chimney=(x1 + x2) / 2)
        L.ground([(x2, R), (x2 + 900, S)], bottom=S, color=SNOW, outline=ICE)       # ramp down
        cx = (x1 + x2) / 2
        glow = L.box(cx - 90, R - 430, cx + 90, R - 170, color=YELLOW, inter=False, opacity=25)
        done = L.text(cx - 120, R - 330, 'COAL #%d DELIVERED' % (n + 1), size=18, color=YELLOW, font=3,
                      opacity=0)
        deliveries.append((glow, done, (cx, R - 300)))
    # traps
    L.mine(1850, R)
    L.mine(2350, R)
    billboard_gun(L, 4900, R)
    billboard_gun(L, 5450, R)
    L.homing_mine(6700, S - 400, speed=2, delay=1)
    L.text(6300, S - 330, 'Little Timmy built a drone.\nIt has a mine. Little Timmy is 8.', size=15, color=WHITE)
    L.text(3550, S - 330, 'Mines on the roof. Timmy got\na chemistry set for Christmas.', size=15, color=WHITE)
    L.box(9900, S - 640, 10900, S - 610, color=METAL_DARK)
    L.box(9900, S - 610, 9930, S, color=METAL_DARK, inter=False)
    L.box(10870, S - 610, 10900, S, color=METAL_DARK, inter=False)
    swings = [L.wrecking_ball(x, S - 610, rope=420) for x in (10200, 10600)]
    L.trigger(9800, S - 100, 60, 200, [(w, []) for w in swings])
    L.boombox(8200, R - 14)
    # the gate before the finish opens after the third delivery
    gate_ref = L.box(11100, S - 400, 11160, S, color=METAL_DARK, outline=BLACK)
    for n, (glow, done, (gx, gy)) in enumerate(deliveries):
        targets = [show_text(done, 0.3), (glow, [(3, 70, 0.3)])]
        if n == 2:
            targets.append(open_gate(gate_ref))
        L.trigger(gx, gy, 180, 260, targets)
    L.text(10950, S - 330, 'Gate opens after\nthe third delivery.', size=14, color=WHITE)
    L.finish(11600, S)
    L.box(12000, S - 600, 12100, S + 400, color=ICE)
    return L


def sn_06():
    """Silent Night (hard): over the city at midnight. Eight skyscraper roofs (building specials),
    each a little lower than the last, with gaps from 300 to 500 px that only flight crosses:
    tap SPACE as the elves reach the edge and let go once you land, so the meter refills on the
    roof. Billboard harpoon turrets, a water tower, "fireworks" (meteors) dropping through the
    gaps, a homing drone, and the church roof finish."""
    TOPS = [3800, 3850, 3950, 4000, 4100, 4150, 4250, 4300]
    GAPS = [300, 350, 400, 400, 450, 450, 500, 500]          # gap after roof k
    W = 1200                                                  # 4 floors of 300 px
    L = Level('08_santa_claus/06_silent_night.xml', 'Silent Night', 8, (450, TOPS[0] - RIDE), bg=2, bgc=NIGHT)
    title(L, 160, TOPS[0], 'SILENT NIGHT', 'All is calm. All is bright. All is 200 metres down.\n'
          'Tap SPACE at each edge; the meter only refills on a roof.', color=WHITE)
    roofs = []
    x = 0
    for k, top in enumerate(TOPS):
        L.building(x, top, floors=int((6400 - top) / 165), width=4)
        roofs.append((x, x + W, top))
        L.box(x + 40, top - 70, x + 90, top, color=METAL_DARK, inter=False)          # vents
        x += W + GAPS[k]
    church_x = x
    L.box(-100, 6400, church_x + 1500, 6800, color=ASPHALT)                          # the street far below
    L.text(900, TOPS[0] - 330, 'Gaps get wider. Jump, then hold SPACE.', size=15, color=YELLOW)
    # hazards
    billboard_gun(L, roofs[3][0] + 450, roofs[3][2])
    billboard_gun(L, roofs[6][0] + 450, roofs[6][2])
    wx, wt = roofs[5][0] + 500, roofs[5][2]                                           # water tower
    L.box(wx, wt - 260, wx + 40, wt, color=WOOD_DARK, inter=False)
    L.box(wx + 260, wt - 260, wx + 300, wt, color=WOOD_DARK, inter=False)
    L.box(wx - 40, wt - 480, wx + 340, wt - 260, color=WOOD, outline=WOOD_DARK)
    # "fireworks": meteors dropping through gaps 4, 5 and 6, across your flight path
    for k in (3, 4, 5):
        gx = roofs[k][1] + GAPS[k] / 2.0
        m = L.meteor(gx, 2700, d=220)
        L.trigger(roofs[k][0] + 150, roofs[k][2] - 120, 60, 240, [(m, [(0,)])])
    L.text(roofs[3][1] - 700, roofs[3][2] - 330, 'Fireworks! Very large ones.\nVery rocky ones.', size=15,
           color=YELLOW)
    L.homing_mine(roofs[7][0] + 300, roofs[7][2] - 300, speed=2, delay=1)
    for k in range(7):
        L.token(roofs[k][1] + GAPS[k] / 2.0, roofs[k][2] - 150, 6)
    # church: steeple and the finish on its roof
    ct = TOPS[-1] + 50
    L.box(church_x, ct, church_x + 1400, 6400, color=STONE, outline=STONE_DARK)
    L.box(church_x + 950, ct - 600, church_x + 1150, ct, color=STONE, outline=STONE_DARK, inter=False)
    L.roof(church_x + 1050, ct - 600, 260, 300, color=STONE_DARK)
    L.circle(church_x + 1050, ct - 420, 90, color=rgb('fff3b0'), inter=False)
    L.finish(church_x + 500, ct)
    L.text(church_x + 100, ct - 330, 'Silent night. Holy... that\nwas close. Merry Christmas!', size=16,
           color=WHITE)
    return L

# =============================================================================================
# 10 IRRESPONSIBLE MOM (character 10): bicycle with the son in the basket and the daughter on a
# trailer bike behind; space brakes; shift / ctrl eject the son / daughter.
# =============================================================================================

def bump(L, x, ground, w=160, h=22, color=ASPHALT):
    """A speed bump (static, smooth enough for a bike)."""
    L.ground([(x - w / 2, ground), (x, ground - h), (x + w / 2, ground)], bottom=ground + 5, color=color)


def car(L, x, ground, color=BLUE, w=300, motor_speed=0, sleep=False):
    """A simple car: a body group with two wheels (shapes) on motorised pins. Returns
    (body group, [wheel pins]). Drive it with a trigger: change motor speed (negative = left)."""
    body = L.group(sleep=sleep)
    body.rect(x, ground - 95, w, 70, color=color, outline=BLACK, fixed=False, density=1.5)
    body.rect(x - w * 0.05, ground - 150, w * 0.55, 50, color=color, outline=BLACK, fixed=False, density=0.5)
    body.rect(x - w * 0.05, ground - 150, w * 0.45, 34, color=rgb('cfe8ff'), fixed=False, density=0.1)
    pins = []
    for dx in (-w * 0.32, w * 0.32):
        wheel = L.circle(x + dx, ground - 40, 80, color=rgb('222222'), outline=METAL, density=2, sleep=sleep)
        pins.append(L.pin(x + dx, ground - 40, wheel, body, motor=True, torque=6000, speed=motor_speed))
    return body, pins


def mo_01():
    """School Run (easy): the morning commute. Speed bumps, a crossing guard who does not move,
    a school bus you can ride over on plank ramps, a puddle dip, and the drop-off zone (eject a
    kid into it for a parenting award) at the school gates."""
    L = Level('10_irresponsible_mom/01_school_run.xml', 'School Run', 10, (600, G - RIDE), bg=1, bgc=SKY)
    for x in (900, 3800, 7000, 9800):
        cloud(L, x, 3850, 400)
    house(L, 100, G, 520, 300)
    tree(L, 1100, G, 340)
    house(L, 1500, G, 560, 320, wall=rgb('d7e8c4'), roof=rgb('4a6b3a'))
    title(L, 160, G, 'SCHOOL RUN', 'Two kids. One bike. Zero helmets. Late again.\n'
          'Space brakes. Shift / ctrl launch the son / daughter.\n(Please do not launch the children.)')
    L.box(0, G, 11000, G + 500, color=ASPHALT)
    L.box(0, G, 11000, G + 8, color=CURB, inter=False)
    for x in (2300, 2700, 3100):
        bump(L, x, G)
    # crosswalk with the crossing guard
    for k in range(6):
        L.box(3700 + k * 60, G - 4, 3730 + k * 60, G, color=WHITE, inter=False)
    L.npc(3850, G, char=4, sleep=True, hold=True, pose=(0, -170, 0, 0, 0, 0, 0, 0, 0))
    L.sign(3650, G, kind=2)
    L.text(3500, G - 330, 'Crossing guard Gary has stood\nhere since 1987. He will not move.', size=15)
    # the school bus with plank ramps on both sides
    bx1, bx2 = 4900, 5700
    L.box(bx1, G - 190, bx2, G - 40, color=YELLOW, outline=BLACK)
    for k in range(5):
        L.box(bx1 + 40 + k * 150, G - 170, bx1 + 140 + k * 150, G - 120, color=rgb('cfe8ff'), inter=False)
    L.circle(bx1 + 140, G - 40, 80, color=rgb('222222'), fixed=True)
    L.circle(bx2 - 140, G - 40, 80, color=rgb('222222'), fixed=True)
    L.slab(4450, G, bx1, G - 190, thick=24, color=WOOD)
    L.slab(bx2, G - 190, 6150, G, thick=24, color=WOOD)
    L.text(4600, G - 420, 'SCHOOL BUS', size=24, color=BLACK, font=5)
    # puddle dip
    L.ground([(6600, G), (6750, G + 50), (7050, G + 50), (7200, G)], bottom=G + 500, color=ASPHALT)
    L.box(6620, G + 8, 7180, G + 50, color=WATER, opacity=60, inter=False)
    L.sp(0, 7800, G - 58, 0, True, True)                  # a parked van (sleeping)
    L.slab(7420, G, 7730, G - 125, thick=24, color=WOOD)   # planks over it
    L.slab(7870, G - 125, 8180, G, thick=24, color=WOOD)
    L.box(7730, G - 125, 7870, G - 117, color=WOOD, inter=False)
    L.text(7350, G - 330, 'A parked van. Planks. What could go wrong.', size=15)
    # the school and the drop-off zone
    L.box(8800, G - 520, 10600, G, color=rgb('c0504d'), outline=rgb('6b2b2b'), inter=False)
    for k in range(6):
        L.box(8900 + k * 280, G - 440, 9050 + k * 280, G - 320, color=rgb('cfe8ff'), inter=False)
    L.text(9000, G - 400, 'SPRINGFIELD-ISH ELEMENTARY', size=28, color=rgb('6b2b2b'), font=5)
    L.box(8600, G - 500, 8612, G, color=METAL, inter=False)
    L.box(8612, G - 500, 8700, G - 450, color=RED, inter=False)
    zone = L.box(9200, G - 10, 9700, G, color=YELLOW, inter=False, opacity=60)
    award = L.text(9150, G - 300, 'DROP-OFF COMPLETE!\n+10 parenting points', size=18, color=GREEN, font=3, opacity=0)
    L.trigger(9450, G - 120, 500, 240, [show_text(award)], by=2)
    L.text(9200, G - 40, 'DROP-OFF ZONE', size=12, color=BLACK)
    L.finish(10200, G)
    L.box(11000, G - 500, 11100, G + 500, color=BRICK)
    return L


def mo_02():
    """Soccer Practice (easy+): the big game. A field full of balls, two goals (push a ball into
    the far one for a cheer), cones, sideline parents, a grassy bank, a trampoline (spring box)
    over the fence and the snack stand finish."""
    L = Level('10_irresponsible_mom/02_soccer_practice.xml', 'Soccer Practice', 10, (600, G - RIDE), bg=1,
              bgc=SKY)
    for x in (1200, 4500, 8000):
        cloud(L, x, 3800, 420)
    title(L, 160, G, 'SOCCER PRACTICE', 'Your son is the goalkeeper. Your daughter is the ball.\n'
          'Just kidding. Mostly. Score a goal for extra credit.')
    L.box(0, G, 1500, G + 500, color=ASPHALT)
    L.ground([(1500, G), (10600, G)], bottom=G + 500, color=GRASS_LIGHT)
    for x in range(1600, 8000, 400):
        L.box(x, G - 2, x + 200, G, color=GRASS, inter=False)

    def goal(x, facing):                      # (art: the bike rides through the net)
        L.box(x, G - 220, x + 14, G, color=WHITE, inter=False)
        L.box(x + facing * 160, G - 220, x + facing * 160 + 14, G - 120, color=WHITE, inter=False)
        L.box(min(x, x + facing * 160), G - 234, max(x, x + facing * 160) + 14, G - 220, color=WHITE,
              inter=False)
        for k in range(5):
            L.box(min(x, x + facing * 160), G - 200 + k * 40, max(x, x + facing * 160), G - 197 + k * 40,
                  color=WHITE, inter=False, opacity=50)

    goal(1900, -1)
    for k in range(18):
        L.soccer(2300 + (k * 137) % 4200, G - 20 - (k % 3) * 30)
    for x in (3000, 3600, 4200, 4800):
        L.tri(x, G - 20, 40, 60, color=ORANGE, fixed=False, sleep=True, density=0.3)
    for k, x in enumerate((2600, 3300, 5300, 6100)):
        L.npc(x, G, char=[3, 9, 12, 7][k], sleep=True, reverse=(k % 2 == 1))
    L.text(2500, G - 330, 'Sideline parents. Louder than the game.\nAlso flammable.', size=15)
    # the far goal at the bottom of a gentle slope: balls roll in on their own
    L.ground([(6600, G), (7300, G + 120)], bottom=G + 500, color=GRASS_LIGHT)
    L.box(7300, G + 120, 8200, G + 500, color=GRASS_LIGHT)
    L.box(7300, G + 120 - 220, 7314, G + 120 - 120, color=WHITE, inter=False)
    L.box(7300, G + 120 - 234, 7600, G + 120 - 220, color=WHITE, inter=False)
    L.box(7600, G + 120 - 234, 7614, G + 120, color=WHITE, inter=False)
    cheer = L.text(7000, G - 330, 'GOOOOOAAAL!\nThe referee is "reviewing".', size=22, color=GREEN, font=5, opacity=0)
    L.trigger(7500, G + 120 - 60, 160, 100, [show_text(cheer)], by=3)
    L.ground([(8200, G + 120), (8700, G)], bottom=G + 500, color=GRASS_LIGHT)
    # fence and the trampoline
    L.ground([(8900, G), (9150, G - 90), (9170, G - 90)], bottom=G, color=GRASS_LIGHT)
    L.box(9230, G - 80, 9250, G, color=METAL_DARK)
    L.spring_box(8650, G)
    L.text(8450, G - 330, 'The trampoline. Liability\nwaivers are for quitters.', size=15)
    L.table(10100, G)
    L.food(10070, G - 78, kind=1)
    L.food(10130, G - 78, kind=2)
    L.box(9900, G - 300, 10350, G - 280, color=RED, inter=False)
    L.text(9950, G - 340, 'SNACKS', size=24, color=RED_DARK, font=5)
    L.finish(9700, G)
    L.box(10600, G - 500, 10700, G + 500, color=METAL_DARK)
    return L


def mo_03():
    """Mall Madness (medium): Black Friday. Smash the glass doors, weave through shoppers past
    the mall cop, ride the escalator up to the food court, smash the glass railing, take the down
    escalator, and get out before the closing shutter (prismatic joint) slides down behind you,
    past the restroom row to the car park finish."""
    U = G - 600
    L = Level('10_irresponsible_mom/03_mall_madness.xml', 'Mall Madness', 10, (600, G - RIDE), bg=0,
              bgc=rgb('efe7da'))
    title(L, 160, G, 'MALL MADNESS', 'BLACK FRIDAY SALE: EVERYTHING MUST GO.\n'
          'Including you, ma\'am. Ma\'am. MA\'AM.')
    TILE = rgb('d8d2c4')
    L.box(0, G, 12500, G + 400, color=TILE, outline=rgb('b0a890'))
    L.box(0, G - 1300, 12500, G - 1200, color=rgb('b7aea0'))                    # roof
    for x in range(0, 12500, 1200):
        L.box(x, G - 1200, x + 40, G, color=rgb('cfc6b6'), inter=False)
    # entrance glass doors
    for x in (1300, 1340):
        L.glass(x, G - 150, w=12, h=300, strength=3)
    L.text(1450, G - 330, 'Doors: open at 6:00.\nYou: arrived at 5:59.', size=15)
    # shoppers and a mall cop
    for k, x in enumerate(range(1800, 3400, 330)):
        L.npc(x, G, char=1 + (k * 7) % 16, sleep=True, reverse=(k % 2 == 0))
    L.npc(3600, G, char=10, sleep=True, hold=True, pose=(0, -80, -10, -40, 0, 0, 0, 0, 0))
    L.text(3300, G - 330, 'Mall cop Steve. Top speed:\nSegway. You are faster.', size=15)
    # electronics store: TVs on shelves
    L.box(3900, G - 360, 4600, G - 340, color=WOOD, fixed=True)
    for x in (3980, 4120, 4260, 4400, 4540):
        L.tv(x, G - 380)
    # escalator up to the upper floor
    L.ground([(4800, G), (5900, U)], bottom=G, color=METAL, outline=METAL_DARK)
    L.box(5900, U, 9000, U + 60, color=TILE, outline=rgb('b0a890'))
    L.text(4600, G - 420, 'ESCALATOR (out of order,\nlike everything else)', size=15)
    # food court: tables, chairs, food, a fountain gap to jump
    for tx in (6400, 7000):
        L.table(tx, U)
        L.chair(tx - 110, U)
        L.chair(tx + 110, U, reverse=True)
        L.food(tx - 25, U - 78, kind=1 + tx % 3)
        L.food(tx + 25, U - 78, kind=2)
    L.trash(7500, U)
    L.text(6200, U - 330, 'Food court. The pretzel guy\nhas seen things.', size=15)
    # glass railing to smash, then the escalator down
    L.glass(8000, U - 90, w=10, h=180, strength=2)
    L.ground([(9000, U), (10000, G)], bottom=G, color=METAL, outline=METAL_DARK)
    L.text(8700, U - 330, 'Down escalator. Hold on to\nyour children. Or not. Your call.', size=15)
    # closing time: a shutter (prismatic joint) slides down behind you
    sh = L.group()
    sh.rect(10300, G - 870, 40, 600, color=METAL_DARK, outline=BLACK, fixed=False, density=2)
    sj = L.slider(10300, G - 870, sh, LEVEL, axis=90, lower=0, upper=560, motor=True, force=1e5, speed=0)
    L.trigger(10700, G - 100, 60, 200, [(sj, [(1, 4, 0.2)])])
    L.box(10200, G - 1200, 10400, G - 1170, color=METAL_DARK, inter=False)
    L.text(10450, G - 420, 'MALL NOW CLOSED.\nYou are trapped out here now.', size=14, color=RED_DARK)
    # restroom row and exit
    for x in (10600, 10800, 11000):
        L.toilet(x, G)
    L.box(10500, G - 300, 11100, G - 280, color=BLUE, inter=False)
    L.text(10550, G - 340, 'RESTROOMS', size=20, color=BLUE, font=3)
    L.box(11300, G - 1200, 11340, G - 340, color=rgb('b7aea0'))
    L.finish(11800, G)
    L.text(11300, G - 330, 'Car park! You got everything\non the list. Except the kids\' coats.', size=15)
    L.box(12500, G - 1300, 12600, G + 400, color=rgb('b7aea0'))
    return L


def mo_04():
    """Playground (medium+): the local park. A slide from the tower, a pair of see-saws, a
    swing set (pendulums shoved into your path), the roundabout rollers (motorised), a sandbox,
    a trampoline (spring box), a paddle launcher and the ice-cream van finish."""
    L = Level('10_irresponsible_mom/04_playground.xml', 'Playground', 10, (600, G - RIDE), bg=1, bgc=SKY)
    for x in (1200, 5000, 9000):
        cloud(L, x, 3800, 420)
    title(L, 160, G, 'PLAYGROUND', 'Fresh air! Exercise! Liability!\n'
          'Everything here was built in 1974 and never inspected.')
    L.ground([(0, G), (6000, G)], bottom=G + 500, color=GRASS)
    L.ground([(6800, G), (11500, G)], bottom=G + 500, color=GRASS)
    # the slide tower: ramp up the back, slide down the front
    L.ground([(1200, G), (1800, G - 320), (2000, G - 320)], bottom=G, color=RED, outline=RED_DARK)
    L.box(1990, G - 340, 2010, G - 320, color=YELLOW, inter=False)
    L.ground([(2000, G - 320), (2700, G)], bottom=G, color=YELLOW, outline=ORANGE)
    L.box(1800, G - 520, 1820, G - 320, color=METAL, inter=False)
    L.roof(1910, G - 520, 260, 90, color=BLUE)
    L.text(1300, G - 450, 'The Big Slide. Wheeee-\noh no.', size=15)
    # the see-saws were condemned: two gentle humps where they used to be
    for sx in (3300, 3900):
        L.ground([(sx - 250, G), (sx, G - 50), (sx + 250, G)], bottom=G, color=rgb('9b7b4a'))
    L.text(3200, G - 330, 'See-saws: condemned in 2009.\nThe humps remain.', size=15)
    # swings: seats on rigid arms hanging from the frame, pushed when you arrive
    L.box(4500, G - 520, 5600, G - 490, color=METAL_DARK)
    L.box(4500, G - 490, 4520, G, color=METAL_DARK, inter=False)
    L.box(5580, G - 490, 5600, G, color=METAL_DARK, inter=False)
    swings = []
    for sx in (4800, 5300):
        g = L.group()
        g.rect(sx, G - 390, 8, 200, color=METAL, fixed=False, density=0.5, collision=3)
        g.rect(sx, G - 285, 90, 16, color=RED, outline=BLACK, fixed=False, density=3)
        L.pin(sx, G - 490, g, LEVEL)
        swings.append(g)
    L.text(4500, G - 330, 'Swings. Built for very tall children.', size=14)
    # the roundabout rollers: a trench of fast motorised rollers that fling you forward
    L.box(6000, G + 70, 6800, G + 500, color=DIRT)
    for k in range(6):
        x = 6060 + k * 120
        rl = L.circle(x, G + 58, 116, color=[RED, YELLOW][k % 2], outline=BLACK, density=2)
        L.pin(x, G + 58, rl, LEVEL, motor=True, torque=40000, speed=-6)         # tops turn forward
    L.text(5900, G - 330, 'The Roundabout Rollers.\nApproved by no one.', size=15)
    # sandbox, trampoline, paddle
    L.ground([(7200, G), (7300, G + 60), (7800, G + 60), (7900, G)], bottom=G + 500, color=SAND)
    L.spring_box(8400, G)
    L.box(8700, G - 200, 8730, G, color=METAL_DARK)
    L.paddle(9300, G, delay=0.3, angle=45, speed=6)
    for k, x in enumerate((7400, 7600, 9900)):
        L.npc(x, G + (60 if x < 8000 else 0), char=[13, 15, 6][k], sleep=True)
    # ice cream van
    L.sp(0, 10800, G - 58, 0, True, True)
    L.box(10620, G - 220, 10980, G - 190, color=PINK, inter=False)
    L.text(10620, G - 260, 'ICE CREAM', size=18, color=rgb('a0306a'), font=5)
    L.finish(10300, G)
    L.box(11500, G - 500, 11600, G + 500, color=HEDGE)
    return L


def mo_05():
    """Traffic Jam (hard-ish): rush hour. Ride the roofs of a jammed lane (plank ramps) with a
    tailgater on your back wheel (a car whose motorised wheel pins a trigger switches on), jump
    the construction zone's rebar pit, dodge the crane's wrecking ball, climb the overpass and
    drop to the off-ramp finish."""
    L = Level('10_irresponsible_mom/05_traffic_jam.xml', 'Traffic Jam', 10, (900, G - RIDE), bg=2, bgc=rgb('ffd8a8'))
    title(L, 160, G, 'TRAFFIC JAM', 'Rush hour. Bike lane: theoretical.\n'
          'The car behind you is a "you" problem.')
    L.box(0, G, 7700, G + 500, color=ASPHALT)
    L.box(8100, G, 14000, G + 500, color=ASPHALT)
    for x in range(0, 14000, 300):
        L.box(x, G - 2, x + 150, G, color=YELLOW, inter=False)
    # jammed lane: parked cars to ride over on planks
    L.slab(1300, G, 1600, G - 130, thick=24, color=WOOD)
    for k, x in enumerate((1800, 2150, 2500)):
        L.box(x - 160, G - 130, x + 160, G - 60, color=[RED, BLUE, GREEN][k], outline=BLACK)
        L.box(x - 110, G - 190, x + 90, G - 130, color=[RED, BLUE, GREEN][k], outline=BLACK, inter=False)
        L.circle(x - 100, G - 40, 80, color=rgb('222222'), inter=False)
        L.circle(x + 100, G - 40, 80, color=rgb('222222'), inter=False)
    L.slab(2660, G - 130, 2960, G, thick=24, color=WOOD)
    L.text(1250, G - 330, 'Traffic. Go over it.\nAs a family.', size=15)
    # merging traffic: cars waiting on an on-ramp roll out behind you and give chase
    c2, p2 = car(L, 250, G, color=rgb('d35400'))
    L.trigger(1300, G - 100, 60, 200, [(pp, [(1, 16, 0.3)]) for pp in p2])
    L.text(3300, G - 330, 'TAILGATER.\nHe has places to be.', size=16, color=RED_DARK)
    for x in (3500, 4200, 4900):
        L.tri(x, G - 20, 40, 60, color=ORANGE, fixed=False, density=0.3)
    # construction zone
    L.box(6400, G - 6, 8200, G, color=ORANGE, inter=False, opacity=60)
    L.box(6500, G - 40, 6900, G - 10, color=METAL_DARK, inter=False)          # stacked girders (scenery)
    L.box(6550, G - 70, 6850, G - 40, color=METAL_DARK, inter=False)
    L.box(7700, G + 120, 8100, G + 500, color=DIRT_DARK)
    L.spikes_on(7900, G + 120, count=24)
    L.ground([(7400, G), (7650, G - 90), (7700, G - 90)], bottom=G, color=CONCRETE)
    L.box(8100, G, 8140, G + 500, color=ASPHALT)
    for x in (6600, 7100, 8300):
        L.tri(x, G - 20, 40, 60, color=ORANGE, fixed=False, density=0.3)
    L.box(8500, G - 1100, 8540, G, color=YELLOW, inter=False)                   # crane mast
    L.box(8000, G - 1120, 9400, G - 1090, color=YELLOW)
    crane = L.wrecking_ball(9000, G - 1090, rope=900)
    L.trigger(8300, G - 100, 60, 200, [(crane, [])])
    L.text(6500, G - 330, 'Construction zone. Fines doubled.\nSo is the danger.', size=15)
    # overpass and off-ramp
    L.ground([(9800, G), (10700, G - 400), (11800, G - 400), (12600, G)], bottom=G, color=CONCRETE, outline=METAL_DARK)
    L.box(10700, G - 400, 10730, G, color=METAL_DARK, inter=False)
    L.box(11770, G - 400, 11800, G, color=METAL_DARK, inter=False)
    L.sign(13000, G, kind=6)
    L.finish(13300, G)
    L.text(12800, G - 330, 'Exit 42: Home.\nThe kids want McNuggets.', size=15)
    L.box(14000, G - 600, 14100, G + 500, color=CONCRETE)
    return L


def mo_06():
    """Family Vacation (hard): the whole trip in one ride. A canyon jump in the desert (boost
    panels), the beach (harpoon-gun fishermen, sunbathers, beach balls, a boombox), the airport
    (luggage carts with jets, a glass security gate, arrow-gun \"security\"), a mountain log
    avalanche, and the theme park paddles to the hotel finish."""
    L = Level('10_irresponsible_mom/06_family_vacation.xml', 'Family Vacation', 10, (600, G - RIDE), bg=1,
              bgc=rgb('ffe0b0'))
    title(L, 160, G, 'FAMILY VACATION', 'Seven days, six nights, five hazards per mile.\n'
          'Are we there yet? Are we there yet? Are we there yet?')
    # desert and the canyon
    L.ground([(0, G), (1500, G), (2000, G - 160)], bottom=G + 900, color=SAND)
    L.boost(1650, G - 55, panels=1, power=12, rot=-18)
    pit(L, 2000, 2700, G - 160, depth=900, color=rgb('8a5a2a'), walls=rgb('c27a3a'))
    L.ground([(2700, G - 160), (3000, G - 160), (3400, G)], bottom=G + 900, color=SAND)
    L.text(1200, G - 330, 'The Grand-ish Canyon.\nSpeed is your co-pilot.', size=15)
    L.tri(900, G - 150, 160, 450, color=rgb('2e8b57'), inter=False)            # cactus
    # beach
    L.ground([(3400, G), (6200, G)], bottom=G + 900, color=SAND)
    L.box(5200, G + 40, 6200, G + 900, color=WATER, opacity=40, inter=False)
    for x in (4000, 4400):
        L.npc(x, G, char=[7, 3][x % 2], sleep=True)
        L.box(x - 70, G - 4, x + 70, G, color=[RED, BLUE][x % 2], inter=False)
    for k in range(5):
        L.soccer(4600 + k * 90, G - 30)
    L.boombox(4250, G - 14)
    L.harpoon(5300, G - 30, rot=0)
    L.harpoon(5800, G - 30, rot=0)
    L.text(4600, G - 330, 'Fishermen. Fishing for\nmoms, apparently.', size=15)
    # airport
    L.ground([(6200, G), (9000, G)], bottom=G + 900, color=CONCRETE)
    L.box(6300, G - 330, 7500, G - 300, color=METAL_DARK)                     # overhead baggage belt
    for k, x in enumerate((6600, 7200)):
        cart = L.rect(x, G - 360, 160, 60, color=METAL, outline=METAL_DARK, fixed=False, density=1)
        jt = L.jet(x - 90, G - 360, rot=180, power=5, fire_time=15, sleep=True)
        L.pin(x - 80, G - 360, jt, cart)
        L.trigger(x - 500, G - 100, 60, 200, [(jt, [(0,)]), (cart, [(0,)])])
    L.box(7700, G - 360, 7740, G, color=METAL_DARK, inter=False)
    L.box(8040, G - 360, 8080, G, color=METAL_DARK, inter=False)
    L.box(7700, G - 380, 8080, G - 340, color=METAL_DARK)
    L.glass(7890, G - 170, w=12, h=330, strength=2)
    L.arrow_gun(8400, G - 330, rot=180, fixed=True, rate=3)
    L.text(7300, G - 330, 'SECURITY: please remove\nyour children from the bike.', size=15)
    # mountain log avalanche
    L.ground([(9000, G), (10000, G - 300), (11500, G - 300)], bottom=G + 900, color=GRASS)
    L.slab(10300, G - 1000, 11300, G - 700, thick=60, color=STONE)
    logs = [L.log(10450 + k * 160, G - 1000 + k * 48 - 40, 140, 70, rot=17, sleep=True) for k in range(4)]
    L.trigger(11450, G - 400, 60, 200, [(lg, [(0,)]) for lg in logs])     # they drop behind you
    for x in (9800, 10800):
        pine(L, x, G - 300 + (300 if x < 10000 else 0) * 0, 420)
    # theme park and the hotel
    L.ground([(11500, G - 300), (12100, G)], bottom=G + 900, color=GRASS)
    L.ground([(12100, G), (14500, G)], bottom=G + 900, color=rgb('9b7b4a'))
    for k in range(6):                                            # balloon stall: burst through
        L.circle(12500 + k * 150, G - 200 - (k % 2) * 90, 110, color=[RED, YELLOW, BLUE, GREEN, PINK, ORANGE][k],
                 outline=BLACK, sleep=True, density=0.05)
    L.text(12400, G - 330, 'Theme park. The ride is\nyou. You are the ride.', size=15)
    L.box(13800, G - 700, 14400, G, color=rgb('f2d0c9'), outline=rgb('6b3b2b'), inter=False)
    L.text(13850, G - 400, 'HOTEL', size=30, color=rgb('6b3b2b'), font=5)
    L.finish(14000, G)
    L.box(14500, G - 700, 14600, G + 900, color=rgb('6b3b2b'))
    return L


# =============================================================================================
# 11 HELICOPTER MAN (character 11): a one-man helicopter with a magnet on a rope; space turns
# the magnet on / off (it holds any loose body touching its underside), shift / ctrl reel the
# rope in / out. Rotor strikes break the blades: keep tunnels at least ~450 px tall. Delivery
# triggers use "any non-fixed shape" (b=3) and open gates in closed rooms (ceilings included).
# =============================================================================================

def helipad(L, x, ground, w=360):
    L.box(x - w / 2, ground - 16, x + w / 2, ground, color=rgb('55585c'), outline=YELLOW)
    L.text(x - 16, ground - 14, 'H', size=12, color=YELLOW, font=3)


def crate(L, x, ground, size=80, color=WOOD_LIGHT, density=0.3, label=None):
    r = L.rect(x, ground - size / 2, size, size, color=color, outline=WOOD_DARK, fixed=False, density=density)
    if label:
        L.text(x - size / 2 + 4, ground - size - 22, label, size=12, color=BLACK)
    return r


def drop_zone(L, x, ground, w=220, targets=(), label='DROP ZONE', color=RED):
    """A painted pad with a b=3 trigger over it (fires when a loose shape lands on it)."""
    L.box(x - w / 2, ground - 8, x + w / 2, ground, color=color, inter=False)
    L.text(x - w / 2, ground - 40, label, size=14, color=color, font=3)
    return L.trigger(x, ground - 60, w, 110, list(targets), by=3)


def he_01():
    """Lift Off (easy): flight school. Take off from the pad, pick up the crate with the magnet,
    drop it on the red pad to open the hangar door, and land inside on the finish."""
    L = Level('11_helicopter_man/01_lift_off.xml', 'Lift Off', 11, (500, G - 130), bg=1, bgc=SKY)
    for x in (1200, 3600, 6000):
        cloud(L, x, 3700, 420)
    title(L, 160, G, 'LIFT OFF', 'Flight school, lesson one. Up: climb. Left / right: tilt.\n'
          'SPACE: magnet on / off. Shift / ctrl: reel the rope in / out.\n'
          'Bring the crate to the red pad to open the hangar.')
    L.ground([(0, G), (7600, G)], bottom=G + 500, color=GRASS)
    L.box(0, G, 7600, G + 6, color=CONCRETE, inter=False)
    # Native tutorial arrows: lift off, toggle the magnet over the crate, then release over the pad.
    L.system_trigger(750, G - 130, 90, 300, 0)
    L.system_trigger(1050, G - 130, 90, 300, 5)
    L.system_trigger(1450, G - 130, 90, 300, 4)
    L.system_trigger(1950, G - 130, 90, 300, 5)
    L.system_trigger(2850, G - 300, 90, 500, 4)
    L.system_trigger(3550, G - 250, 90, 500, 5)
    helipad(L, 500, G)
    L.box(900, G - 300, 912, G, color=METAL, inter=False)                       # wind sock
    L.tri(950, G - 285, 30, 80, rot=90, color=ORANGE, inter=False)
    c = crate(L, 1700, G, label='CRATE')
    L.text(1500, G - 330, 'Hover over the crate,\nlower it onto the magnet, SPACE.', size=15)
    # the drop pad on a little tower (so the crate has to be lifted, not pushed)
    L.box(3000, G - 250, 3400, G, color=CONCRETE, outline=METAL_DARK)
    # the hangar: closed room with a door (gate) on the left
    hx1, hx2, top = 4600, 6800, G - 700
    L.box(hx1, top, hx2 + 60, top + 60, color=METAL_DARK)                       # roof
    L.box(hx2, top, hx2 + 60, G, color=METAL_DARK)                              # back wall
    L.box(hx1 + 60, top + 60, hx2, G, color=rgb('d6d6d0'), inter=False)         # inside
    door = gate(L, hx1 + 30, top + 60, G, w=60, color=rgb('b03030'), outline=BLACK)
    L.text(hx1 + 100, top + 100, 'HANGAR 1', size=26, color=METAL_DARK, font=5)
    opened = L.text(3000, G - 600, 'Hangar door: open!', size=18, color=GREEN, font=3, opacity=0)
    drop_zone(L, 3200, G - 250, 300, [open_gate(door), show_text(opened)])
    helipad(L, 5900, G, 500)
    L.finish(5900, G)
    L.text(5300, G - 330, 'Land here. Gently.\nThe instructor is watching.', size=15)
    return L


def he_02():
    """Skyscraper (medium): deliver the air conditioner to the roof of Spire Tower. Climb past
    window-washer cradles swinging on pins, harpoon turrets in the windows and a crane's I-beam,
    set the AC unit on the roof pad; the penthouse door opens and the finish is inside."""
    L = Level('11_helicopter_man/02_skyscraper.xml', 'Skyscraper', 11, (500, G - 130), bg=2, bgc=rgb('9fc8ef'))
    title(L, 160, G, 'SKYSCRAPER', 'Spire Tower, 60 floors, one broken air conditioner.\n'
          'Lift the AC unit to the roof pad. Mind the windows.\nThe windows mind you back.')
    L.box(0, G, 4600, G + 400, color=ASPHALT)
    helipad(L, 500, G)
    ac = crate(L, 1300, G, size=90, color=rgb('c0c7cc'), density=0.3, label='AC UNIT')
    # the tower: x 2000..3200, roof at 2600
    TX1, TX2, ROOF = 2000, 3200, 2600
    L.box(TX1, ROOF, TX2, G, color=rgb('5d6d7e'), outline=rgb('2c3e50'))
    for fy in range(ROOF + 80, G - 100, 160):
        for fx in range(TX1 + 60, TX2 - 100, 180):
            L.box(fx, fy, fx + 110, fy + 90, color=rgb('a9cce3'), inter=False, opacity=80)
    # harpoon turrets in windows facing out (left)
    for y in (4300, 3700, 3100):
        L.harpoon(TX1 - 30, y, rot=0, anchor=False)
    # window-washer cradles hanging on pins from the facade
    for y in (4000, 3400):
        g = L.group()
        g.rect(TX1 - 130, y, 220, 24, color=YELLOW, outline=BLACK, fixed=False, density=1)
        g.rect(TX1 - 130, y - 120, 6, 240, color=METAL, fixed=False, density=0.2, collision=3)
        L.pin(TX1 - 130, y - 240, g, LEVEL)
        L.npc(TX1 - 130, y, char=14, sleep=True)
    L.text(1200, 4300, 'Climb up the left side.\nThe turrets only see so far.', size=15)
    # crane with a swinging I-beam on a chain
    L.box(3800, 2200, 3840, G, color=YELLOW, inter=False)
    L.box(3000, 2180, 4300, 2210, color=YELLOW)
    beam = L.ibeam(3500, 2700, 400, 30)
    L.chain(3500, 2210, links=30, scale=1.5)
    # the roof: pad and penthouse with a door
    L.box(2150, ROOF - 8, 2450, ROOF, color=RED, inter=False)
    L.text(2150, ROOF - 40, 'AC PAD', size=14, color=RED, font=3)
    px1, px2, ptop = 2650, 3200, ROOF - 520
    L.box(px1, ptop, px2, ptop + 50, color=rgb('2c3e50'))
    L.box(px2 - 50, ptop, px2, ROOF, color=rgb('2c3e50'))
    L.box(px1, ptop + 50, px2 - 50, ROOF, color=rgb('d5dbdb'), inter=False)
    door = gate(L, px1 + 25, ptop + 50, ROOF, w=50, color=rgb('7b241c'), outline=BLACK)
    L.text(px1 + 70, ptop + 70, 'PENTHOUSE', size=18, color=rgb('2c3e50'), font=3)
    L.trigger(2300, ROOF - 60, 300, 110, [open_gate(door)], by=3)
    L.finish(2950, ROOF)
    L.text(2050, ROOF - 360, 'Set the AC on the pad,\nthen land in the penthouse.', size=15, color=BLACK)
    return L


def he_03():
    """Crane Game (medium+): you ARE the claw. A giant arcade cabinet full of prizes (TV,
    boombox, plush NPC, food, balls, crates). Drop one prize down each of the two chutes: each
    removes one lock bar from the exit, then fly out and land on the finish."""
    L = Level('11_helicopter_man/03_crane_game.xml', 'Crane Game', 11, (600, G - 130), bg=0, bgc=rgb('2a1a3a'))
    title(L, 160, G, 'CRANE GAME', 'MEGA CLAW 9000. Insert coin. You are the coin.\n'
          'Drop a prize down EACH chute to unlock the exit.', color=rgb('ffe6ff'))
    L.box(0, G, 7000, G + 400, color=rgb('3d2a52'))
    helipad(L, 600, G)
    # the cabinet: x 1600..5600, floor at G - 200, ceiling at G - 1400
    CX1, CX2, FL, TOP = 1600, 5600, G - 200, G - 1500
    PINK2 = rgb('ff4fa3')
    L.box(CX1, TOP + 60, CX2, FL, color=rgb('bfe6ff'), inter=False, opacity=18)     # glass (art)
    L.box(CX1 - 60, TOP, CX2 + 60, TOP + 60, color=PINK2)                             # top
    L.box(CX1 - 60, TOP + 60, CX1, FL - 600, color=PINK2)                             # left wall...
    L.box(CX1 - 60, FL - 100, CX1, G, color=PINK2)                                    # ...slot, lip
    L.box(CX2, TOP + 60, CX2 + 60, FL - 500, color=PINK2)                             # right wall
    L.box(CX1 + 400, FL, 7000, G, color=PINK2)                                        # prize floor + exit tunnel floor
    L.box(CX2, FL - 560, 7000, FL - 500, color=PINK2)                                 # exit tunnel roof
    L.box(7000, TOP, 7060, G + 400, color=rgb('3d2a52'))                              # outer wall
    L.text(CX1 + 500, TOP + 90, 'M E G A   C L A W   9 0 0 0', size=34, color=PINK2, font=5)
    # chute A: a pit at the left end of the prize floor
    L.box(CX1, FL + 120, CX1 + 400, G, color=rgb('7a3d6b'))
    # basket B: hanging high on the right wall
    L.box(CX2 - 300, TOP + 700, CX2, TOP + 730, color=PINK2)
    L.box(CX2 - 300, TOP + 560, CX2 - 280, TOP + 700, color=PINK2)
    # prizes
    L.tv(2400, FL - 20)
    L.boombox(2700, FL - 14)
    L.npc(3100, FL, char=16, sleep=False)
    L.food(3400, FL - 25, kind=1)
    L.food(3600, FL - 25, kind=3)
    for x in (3900, 4100, 4300):
        L.soccer(x, FL - 15)
    crate(L, 4700, FL, size=90, color=rgb('ffd166'), label='RARE')
    crate(L, 5000, FL, size=70, color=rgb('06d6a0'))
    L.bottle(5300, FL - 15, 2)
    # the exit tunnel under the right wall, barred by two locks
    lock_a = L.box(CX2 + 140, FL - 500, CX2 + 180, FL, color=YELLOW, outline=BLACK)
    lock_b = L.box(CX2 + 260, FL - 500, CX2 + 300, FL, color=YELLOW, outline=BLACK)
    t_a = L.text(CX1 + 450, FL - 300, 'Chute A: unlocked!', size=16, color=YELLOW, opacity=0)
    t_b = L.text(CX2 - 500, TOP + 760, 'Basket B: unlocked!', size=16, color=YELLOW, opacity=0)
    L.trigger(CX1 + 200, FL + 60, 360, 100, [open_gate(lock_a), show_text(t_a)], by=3)
    L.trigger(CX2 - 150, TOP + 660, 260, 80, [open_gate(lock_b), show_text(t_b)], by=3)
    L.text(CX1 + 20, FL + 10, 'CHUTE A', size=14, color=YELLOW, font=3)
    L.text(CX2 - 290, TOP + 520, 'BASKET B', size=14, color=YELLOW, font=3)
    L.text(CX1 - 600, FL - 700, 'Fly in through the slot.\nThe claw is rigged. You are\nthe claw. You are rigged.',
           size=15, color=rgb('ffe6ff'))
    helipad(L, 6500, FL, 500)
    L.finish(6500, FL)
    L.text(5700, FL - 460, 'PRIZE COLLECTION', size=16, color=YELLOW, font=3)
    return L


def he_04():
    """Air Traffic (hard-ish): a long flight through busy sky. Spinning rotor rings to fly
    through the gaps of (motorised groups), a windmill farm, wind fans, a flock of homing
    "birds", anti-air arrow guns, and a hot-air balloon field to pop on the way to the far pad."""
    L = Level('11_helicopter_man/04_air_traffic.xml', 'Air Traffic', 11, (500, G - 130), bg=1, bgc=rgb('aad8ff'))
    title(L, 160, G, 'AIR TRAFFIC', 'Air traffic control has cleared you for "whatever".\n'
          'Fly through the gaps. Do not touch anything that spins.')
    L.ground([(0, G), (1000, G)], bottom=G + 600, color=CONCRETE)
    helipad(L, 500, G)
    L.box(1000, G + 300, 14000, G + 600, color=GRASS_DARK)                      # far below
    L.box(1000, G + 280, 14000, G + 300, color=GRASS, inter=False)
    for x in (2000, 6000, 10000):
        cloud(L, x, 4200, 500)
    # two spinning rings (gaps on opposite sides) on poles
    for rx, sp in ((2400, 0.5), (4000, -0.6)):
        L.box(rx - 15, G - 300, rx + 15, G + 300, color=METAL_DARK, inter=False)
        ring = ring_group(L, rx, G - 700, 420, 30, 28, gaps=(0, 1, 2, 3, 14, 15, 16, 17), color=RED, density=0.5)
        L.pin(rx, G - 700, ring, LEVEL, motor=True, torque=5e6, speed=sp)
    L.text(1800, G - 330, 'Rotor rings: wait for a\ngap to line up.', size=15)
    # windmill farm: three spinning blades on towers
    for k, wx in enumerate((5600, 6400, 7200)):
        L.box(wx - 20, G - 900, wx + 20, G + 300, color=WHITE, outline=METAL, inter=False)
        bl = L.group()
        for a in (0, 120, 240):
            bl.rect(wx + 200 * math.cos(math.radians(a)), G - 900 + 200 * math.sin(math.radians(a)), 400, 30,
                    rot=a, color=WHITE, outline=METAL_DARK, fixed=False, density=0.5)
        L.pin(wx, G - 900, bl, LEVEL, motor=True, torque=3e6, speed=0.9 + 0.2 * k)
    L.text(5400, G - 330, 'Wind farm. Clean energy.\nDirty rotor.', size=15)
    # updraft fans and birds
    for x in (8200, 8600):
        L.box(x - 160, G - 50, x + 160, G + 300, color=CONCRETE)
        L.fan(x, G - 50)
    for k in range(4):
        L.homing_mine(9000 + k * 250, G - 600 - (k % 2) * 200, speed=1, delay=1)
    L.text(8800, G - 330, 'Birds. Explosive birds.\nNature is healing.', size=15)
    # anti-air guns and balloons
    for x in (10200, 11000):
        L.box(x - 60, G - 60, x + 60, G + 300, color=METAL_DARK)
        L.arrow_gun(x, G - 80, rot=-90, fixed=True, rate=2)
    for k in range(6):
        L.circle(10400 + k * 260, G - 900 + (k % 3) * 150, 160, color=[RED, YELLOW, BLUE, GREEN, PINK, ORANGE][k],
                 outline=BLACK, sleep=True, density=0.05)
    # the far pad
    L.ground([(12600, G), (14000, G)], bottom=G + 600, color=CONCRETE)
    helipad(L, 13300, G, 600)
    L.finish(13300, G)
    L.text(12700, G - 330, 'Touchdown! ATC says\n"we saw nothing".', size=15)
    L.box(14000, G - 1500, 14060, G + 600, color=CONCRETE)
    return L


def he_05():
    """Magnet Mayhem (hard): a junkyard under a long roof. Three magnet jobs, each opening the
    next gate: drop scrap into the crusher bin; hang a weight on the scale hook (a pinned arm
    whose end trips a trigger when it dips); stack the I-beam into the high hopper. Then land."""
    L = Level('11_helicopter_man/05_magnet_mayhem.xml', 'Magnet Mayhem', 11, (500, G - 130), bg=0,
              bgc=rgb('3b3a36'))
    title(L, 160, G, 'MAGNET MAYHEM', "Crazy Earl's Junkyard. Three jobs, three gates.\n"
          'The magnet picks up anything loose. Even Earl.', color=rgb('f0e6c8'))
    ROOF = G - 1100
    L.box(0, ROOF - 60, 11000, ROOF, color=rgb('6e5a42'))
    L.box(0, G, 11000, G + 400, color=rgb('5a4a36'))
    L.box(0, ROOF - 60, 60, G, color=rgb('6e5a42'))
    helipad(L, 500, G)
    # job 1: crusher bin
    for x in (1200, 1350):
        crate(L, x, G, size=80, color=rgb('8a8f94'), label='SCRAP' if x == 1200 else None)
    L.box(2300, G - 320, 2330, G, color=METAL_DARK)
    L.box(2730, G - 320, 2760, G, color=METAL_DARK)
    L.box(2300, G - 40, 2760, G, color=METAL_DARK)
    L.text(2320, G - 360, 'CRUSHER BIN', size=16, color=YELLOW, font=3)
    g1 = gate(L, 3300, ROOF, G, w=70, color=rgb('8e2318'), outline=BLACK)
    L.trigger(2530, G - 100, 380, 100, [open_gate(g1)], by=3)
    L.npc(1800, G, char=9, sleep=True)
    L.text(1500, G - 600, 'Job 1: scrap in the crusher bin.\n(Earl is not scrap. Probably.)', size=15,
           color=rgb('f0e6c8'))
    # job 2: the scale: an arm on a pin, its right end over a trigger when weighed down
    L.box(4200, G - 400, 4240, G, color=METAL_DARK, inter=False)
    arm = L.group()
    arm.rect(4220, G - 420, 700, 24, color=METAL, outline=BLACK, fixed=False, density=0.3)
    arm.rect(4540, G - 440, 60, 60, color=RED, outline=BLACK, fixed=False, density=0.1)
    L.pin(4220, G - 420, arm, LEVEL, limit=True, upper=25, lower=-5)
    L.box(4000, G - 600, 4060, G - 590, color=METAL_DARK, inter=False)
    weight = L.rect(3800, G - 50, 100, 100, color=rgb('2f2f2f'), outline=YELLOW, fixed=False, density=0.6)
    L.text(3700, G - 160, '100 KG', size=14, color=YELLOW, font=3)
    g2 = gate(L, 5300, ROOF, G, w=70, color=rgb('8e2318'), outline=BLACK)
    L.trigger(4560, G - 230, 100, 60, [open_gate(g2)], by=3)
    L.text(3600, G - 700, 'Job 2: weigh down the red end\nof the scale until it clicks.', size=15,
           color=rgb('f0e6c8'))
    # job 3: the high hopper
    beam = L.ibeam(5900, G - 20, 260, 30)
    L.box(7000, G - 800, 7030, G - 560, color=METAL_DARK)
    L.box(7400, G - 800, 7430, G - 560, color=METAL_DARK)
    L.box(7000, G - 590, 7430, G - 560, color=METAL_DARK)
    L.box(7200, G - 560, 7230, G, color=METAL_DARK, inter=False)
    L.text(7000, G - 840, 'HOPPER', size=16, color=YELLOW, font=3)
    g3 = gate(L, 8300, ROOF, G, w=70, color=rgb('8e2318'), outline=BLACK)
    L.trigger(7215, G - 640, 360, 90, [open_gate(g3)], by=3)
    L.text(5700, G - 700, 'Job 3: the I-beam goes\nin the high hopper.', size=15, color=rgb('f0e6c8'))
    # the yard office: land
    L.trash(9000, G)
    L.sp(0, 9400, G - 58, 0, True, True)
    helipad(L, 10300, G, 600)
    L.finish(10300, G)
    L.text(9700, G - 330, 'Earl pays in exposure.\nAnd tetanus.', size=15, color=rgb('f0e6c8'))
    L.box(11000, ROOF - 60, 11060, G + 400, color=rgb('6e5a42'))
    return L


def he_06():
    """Mayday (hard): the engine is coughing. Thread a cave of spikes (floor and ceiling), fans
    that shove you up and down, rock pendulums shoved into a swing as you approach, harpoon
    nests, homing mines and a rotating blade gate; carry the fuel can into the tank to open the
    final hatch."""
    L = Level('11_helicopter_man/06_mayday.xml', 'Mayday', 11, (500, G - 130), bg=0, bgc=rgb('20170f'))
    title(L, 160, G, 'MAYDAY', 'Engine: coughing. Fuel: none. Insurance: expired.\n'
          'Find the fuel can. Fill the tank. Do not touch the walls.', color=rgb('ffd7a0'))
    ROCK = rgb('4d3a2a')
    TOPC = G - 900
    L.box(0, TOPC - 400, 13000, TOPC, color=ROCK)
    L.box(0, G, 13000, G + 400, color=ROCK)
    L.box(0, TOPC, 60, G, color=ROCK)
    helipad(L, 500, G)
    # section 1: spikes floor and ceiling, a low rock bulge
    L.spikes(2000, G - 20, 60, fixed=True)
    L.spikes(2600, TOPC + 20, 60, rot=180, fixed=True)
    L.box(3200, G - 300, 3600, G, color=ROCK)
    L.spikes(3400, G - 320, 26, fixed=True)
    L.text(1300, G - 330, 'Spikes everywhere. Stay\nin the middle of the cave.', size=15, color=rgb('ffd7a0'))
    # section 2: fans pushing up (floor) and down (ceiling, rotated)
    for x in (4200, 4700):
        L.fan(x, G)
    for x in (4450, 4950):
        L.fan(x, TOPC + 25, rot=180)
    L.text(4000, G - 600, 'Air vents. Up, down,\nall around.', size=15, color=rgb('ffd7a0'))
    # section 3: rock pendulums, shoved into a swing as you approach
    for k, x in enumerate((5800, 6500)):
        g = L.group()
        g.rect(x, TOPC + 250, 12, 500, color=METAL, fixed=False, density=0.3)
        g.circle(x, TOPC + 520, 140, color=STONE, outline=STONE_DARK, fixed=False, density=3)
        L.pin(x, TOPC, g, LEVEL, limit=True, upper=70, lower=-70)
        L.trigger(x - 700, G - 450, 80, 800, [(g, [(2, 12 if k == 0 else -12, 0, 0)])])
    # section 4: harpoon nests and the fuel can on a ledge
    L.harpoon(7400, G - 30, rot=0)
    L.harpoon(8000, TOPC + 30, rot=180)
    L.box(7600, G - 360, 7900, G - 330, color=ROCK)
    fuel = crate(L, 7750, G - 360, size=60, color=RED, density=0.3, label='FUEL')
    L.homing_mine(8600, G - 450, speed=1, delay=2)
    L.homing_mine(9000, G - 300, speed=2, delay=2)
    # section 5: the tank and the hatch, then the rotating blade gate
    L.box(9600, G - 260, 9630, G, color=METAL_DARK)
    L.box(9930, G - 260, 9960, G, color=METAL_DARK)
    L.box(9600, G - 30, 9960, G, color=METAL_DARK)
    L.text(9620, G - 300, 'FUEL TANK', size=14, color=YELLOW, font=3)
    hatch = gate(L, 10600, TOPC, G, w=80, color=rgb('8e2318'), outline=BLACK)
    L.trigger(9780, G - 90, 300, 100, [open_gate(hatch)], by=3)
    bx, by = 11400, G - 450
    blade = L.group()
    blade.rect(bx, by, 820, 40, color=METAL, outline=BLACK, fixed=False, density=1)
    L.pin(bx, by, blade, LEVEL, motor=True, torque=5e6, speed=0.6)
    L.text(11000, G - 330, 'The last door. Time it.', size=15, color=rgb('ffd7a0'))
    helipad(L, 12400, G, 600)
    L.finish(12400, G)
    L.box(13000, TOPC - 400, 13060, G + 400, color=ROCK)
    return L


LEVELS = [lm_01, lm_02, lm_03, lm_04, lm_05, lm_06,
          ex_01, ex_02, ex_03, ex_04, ex_05, ex_06,
          sn_01, sn_02, sn_03, sn_04, sn_05, sn_06,
          mo_01, mo_02, mo_03, mo_04, mo_05, mo_06,
          he_01, he_02, he_03, he_04, he_05, he_06]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument('--out', default=os.path.normpath(os.path.join(here, '..', '..', 'res', 'levels', 'restored')))
    ap.add_argument('--only', help='substring of the level path to build')
    args = ap.parse_args(argv)
    bad = 0
    for make in LEVELS:
        L = make()
        if args.only and args.only not in L.path:
            continue
        problems = L.check()
        path = L.write(args.out)
        print('%-60s %4d shapes %3d specials %2d groups %2d joints %2d triggers%s' % (
            os.path.relpath(path), len(L.shapes), len(L.specials), len(L.groups), len(L.joints),
            len(L.triggers), '' if not problems else '  PROBLEMS: ' + '; '.join(problems)))
        bad += bool(problems)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
