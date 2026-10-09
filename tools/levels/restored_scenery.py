"""Scenery kit for the restored characters' campaign levels (make_restored_levels.py).

Stdlib only, deterministic (a small LCG instead of `random`). Every helper here draws ART: shapes
with no physics (art polygons, or rect / circle / triangle with i="f"), so nothing in this module
can put an invisible wall on a route. The only exception is `terrain()`, which also builds the
route's collision, from the same surface profile the art follows, as invisible convex quads (one
per profile segment, top edge exactly on the profile), so what you see is what you drive on.

Style (from the original mobile campaign levels, assets/shared/levels): no outlines, flat fills
with a light and a dark tone per material, organic art polygons (up to 100 vertices), invisible
collision under the art, distant layers in pale desaturated colours. The gameplay camera shows
about 900 x 500 Flash px, so details are sized for that view (5-40 px trim, 100-600 px objects).

Coordinates: Flash px, y down (restored_lib.py).
"""

import math

# ---------------------------------------------------------------------------------------------
# Colour


def rgb(h):
    return int(h.lstrip('#'), 16)


def _split(c):
    return (c >> 16) & 255, (c >> 8) & 255, c & 255


def _join(r, g, b):
    return (int(round(max(0, min(255, r)))) << 16) | (int(round(max(0, min(255, g)))) << 8) | \
        int(round(max(0, min(255, b))))


def mix(a, b, t):
    """Blend colour a towards b by t (0..1)."""
    ar, ag, ab = _split(a)
    br, bg, bb = _split(b)
    return _join(ar + (br - ar) * t, ag + (bg - ag) * t, ab + (bb - ab) * t)


def shade(c, k):
    """k < 1 darkens (towards black), k > 1 lightens (towards white by k - 1)."""
    if k <= 1:
        r, g, b = _split(c)
        return _join(r * k, g * k, b * k)
    return mix(c, 0xffffff, min(1.0, k - 1))


def haze(c, sky, t):
    """Atmospheric perspective: distant things fade towards the sky colour."""
    return mix(c, sky, t)


# ---------------------------------------------------------------------------------------------
# Deterministic noise


class Rng:
    """Tiny LCG (Numerical Recipes constants): the same seed gives the same level on any Python."""

    def __init__(self, seed):
        self.s = (seed * 2654435761 + 12345) & 0xffffffff

    def random(self):
        self.s = (1664525 * self.s + 1013904223) & 0xffffffff
        return self.s / 4294967296.0

    def uniform(self, a, b):
        return a + (b - a) * self.random()

    def choice(self, seq):
        return seq[int(self.random() * len(seq)) % len(seq)]


def wobble(t, seed, amp=1.0):
    """Smooth periodic pseudo-noise in [-amp, amp] for t in turns."""
    return amp * (0.55 * math.sin(2 * math.pi * t * 3 + seed * 1.7) +
                  0.3 * math.sin(2 * math.pi * t * 5 + seed * 3.1) +
                  0.15 * math.sin(2 * math.pi * t * 9 + seed * 0.7))


# ---------------------------------------------------------------------------------------------
# Geometry


def blob(cx, cy, rx, ry, seed=0, n=18, rough=0.12, flat_bottom=False):
    """An organic closed outline (bush, crown, rock, cloud)."""
    pts = []
    for i in range(n):
        t = i / float(n)
        a = 2 * math.pi * t
        k = 1.0 + wobble(t, seed, rough)
        x = cx + math.cos(a) * rx * k
        y = cy + math.sin(a) * ry * k
        if flat_bottom and y > cy:
            y = cy + (y - cy) * 0.35
        pts.append((x, y))
    return pts


def ellipse(cx, cy, rx, ry, n=20, a1=0.0, a2=360.0):
    out = []
    closed = abs(a2 - a1) >= 360
    m = n if closed else n + 1
    for i in range(m):
        a = math.radians(a1 + (a2 - a1) * i / float(n))
        out.append((cx + rx * math.cos(a), cy + ry * math.sin(a)))
    return out


def smooth_profile(knots, step=60):
    """Surface points through `knots` [(x, y), ...] with cosine easing between knots, sampled
    about every `step` px: hills with flat tops and soft feet, no kinks the vehicles catch on."""
    out = [knots[0]]
    for (x1, y1), (x2, y2) in zip(knots, knots[1:]):
        n = max(1, int(round((x2 - x1) / float(step)))) if y1 != y2 else 1
        for i in range(1, n + 1):
            t = i / float(n)
            e = (1 - math.cos(math.pi * t)) / 2
            out.append((round(x1 + (x2 - x1) * t, 1), round(y1 + (y2 - y1) * e, 1)))
    return out


def y_at(profile, x):
    """Surface height of a profile at x (linear between its points)."""
    for (x1, y1), (x2, y2) in zip(profile, profile[1:]):
        if x1 <= x <= x2:
            if x2 == x1:
                return y1
            return y1 + (y2 - y1) * (x - x1) / float(x2 - x1)
    return profile[0][1] if x < profile[0][0] else profile[-1][1]


def clip_profile(profile, x1, x2):
    pts = [(x1, y_at(profile, x1))]
    pts += [p for p in profile if x1 < p[0] < x2]
    pts.append((x2, y_at(profile, x2)))
    return pts


def offset_down(pts, d):
    return [(x, y + d) for x, y in pts]


def art_strip(L, top, bottom, color, opacity=100, max_pts=48):
    """Art polygon between two polylines that share their x samples (top above bottom).
    Split into pieces of at most 2 * max_pts vertices (the art polygon limit is 100)."""
    i = 0
    while i < len(top) - 1:
        j = min(len(top) - 1, i + max_pts - 1)
        poly = top[i:j + 1] + list(reversed(bottom[i:j + 1]))
        L.art(poly, color=color, opacity=opacity)
        i = j


def band_below(L, profile, d1, d2, color, opacity=100):
    """Art band following a surface profile from depth d1 to d2 below it."""
    art_strip(L, offset_down(profile, d1), offset_down(profile, d2), color, opacity)


def plank_rect(L, x1, y1, x2, y2, color, rot=0):
    L.rect((x1 + x2) / 2.0, (y1 + y2) / 2.0, abs(x2 - x1), abs(y2 - y1), color=color, rot=rot,
           inter=False)


def seg(L, x1, y1, x2, y2, w, color, opacity=100):
    """A straight art bar from (x1, y1) to (x2, y2), w px thick (beams, poles, ropes, struts)."""
    ln = math.hypot(x2 - x1, y2 - y1)
    if ln < 0.5:
        return
    L.rect(round((x1 + x2) / 2.0, 2), round((y1 + y2) / 2.0, 2), round(ln, 2), w,
           rot=round(math.degrees(math.atan2(y2 - y1, x2 - x1)), 3), color=color, inter=False,
           opacity=opacity)


def polyline_art(L, pts, w, color, opacity=100):
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        seg(L, x1, y1, x2, y2, w, color, opacity)


# ---------------------------------------------------------------------------------------------
# Terrain: collision + finished surface art from one profile


def solid_profile(L, profile, bottom, color=0x555555, visible=False, extend=4.0):
    """Collision for a surface profile: one convex quad per segment, top edge on the profile,
    filled down to `bottom`. Invisible by default (the art is drawn by terrain()).

    Each quad's top edge runs `extend` px past both ends of its segment, so neighbours overlap
    instead of meeting at a shared corner: a fast wheel crossing exactly abutting polygons can
    catch the next one's corner (Box2D internal-edge contact) and stop dead, which throws the
    rider. At a crest the overlap stands up by extend * sin(angle change), well under 1 px.

    Collinear neighbours (the flat runs smooth_profile subdivides) become one quad: a sliding box
    such as a knocked-off idol snags on the corner of the next quad even when they overlap."""
    refs = []
    pts = [profile[0]]
    for p in profile[1:]:
        if len(pts) >= 2:
            (ax, ay), (bx, by) = pts[-2], pts[-1]
            if abs((bx - ax) * (p[1] - by) - (by - ay) * (p[0] - bx)) < 1e-6:
                pts[-1] = p
                continue
        pts.append(p)
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        if x2 <= x1:
            raise ValueError('profile must go right: %r' % ((x1, y1), (x2, y2)))
        ln = math.hypot(x2 - x1, y2 - y1)
        ex, ey = (x2 - x1) / ln * extend, (y2 - y1) / ln * extend
        a, b = (x1 - ex, y1 - ey), (x2 + ex, y2 + ey)
        if y1 == y2:
            refs.append(L.box(a[0], y1, b[0], bottom, color=color, opacity=100 if visible else 0))
        else:
            refs.append(L.poly([a, b, (b[0], bottom), (a[0], bottom)], color=color,
                               opacity=100 if visible else 0))
    return refs


def terrain(L, profile, bottom, body, lip=None, lip_depth=16, strata=(), solid=True,
            texture=None, seed=0):
    """A finished piece of ground: invisible collision, a body fill, strata bands, a top lip
    (grass, paving edge, stone kerb) and an optional texture callback per 100 px.

    strata: [(depth1, depth2, color), ...] bands under the surface (soil layers, foundations).
    texture(L, x, y, rng): called along the surface for small details (tufts, pebbles, cracks)."""
    if solid:
        solid_profile(L, profile, bottom)
    floor = [(x, bottom) for x, _ in profile]
    art_strip(L, profile, floor, body)
    for d1, d2, c in strata:
        band_below(L, profile, d1, d2, c)
    if lip is not None:
        band_below(L, profile, -1, lip_depth, lip)
    if texture is not None:
        rng = Rng(seed + 101)
        x = profile[0][0] + 40
        while x < profile[-1][0] - 20:
            texture(L, x, y_at(profile, x), rng)
            x += rng.uniform(70, 140)


# ---------------------------------------------------------------------------------------------
# Common props


def cloud(L, x, y, w=320, color=0xffffff, opacity=92, seed=0):
    L.art(blob(x, y, w * 0.5, w * 0.17, seed, 22, 0.18, flat_bottom=True), color=color,
          opacity=opacity)
    L.art(blob(x - w * 0.12, y - w * 0.1, w * 0.26, w * 0.16, seed + 3, 16, 0.12), color=color,
          opacity=opacity)
    L.art(blob(x + w * 0.14, y - w * 0.07, w * 0.22, w * 0.14, seed + 5, 16, 0.12), color=color,
          opacity=opacity)


def sun(L, x, y, r=90, color=0xfff3b0):
    L.circle(x, y, r * 3.2, color=color, opacity=18, inter=False)
    L.circle(x, y, r * 2.2, color=color, opacity=30, inter=False)
    L.circle(x, y, r * 2, color=color, inter=False)


def tree(L, x, ground, h=420, seed=0, leaf=rgb('4f8f2f'), trunk=rgb('6b4423'), lean=0.0,
         haze_to=None, haze_t=0.0, fruit=None):
    """Deciduous tree: tapered trunk with two boughs, a crown of layered leaf blobs (dark core,
    light top-left highlights)."""
    def c(col):
        return haze(col, haze_to, haze_t) if haze_to is not None else col
    rng = Rng(seed)
    tw = h * 0.075
    top_x = x + lean * h * 0.25
    trunk_top = ground - h * 0.55
    L.art([(x - tw, ground), (x + tw, ground), (top_x + tw * 0.5, trunk_top), (top_x - tw * 0.5, trunk_top)],
          color=c(trunk))
    L.art([(x + tw * 0.2, ground), (x + tw, ground), (top_x + tw * 0.5, trunk_top), (top_x + tw * 0.1, trunk_top)],
          color=c(shade(trunk, 0.75)))
    for side in (-1, 1):
        bx = top_x + side * h * 0.22
        seg(L, top_x, ground - h * 0.42, bx, ground - h * 0.68, tw * 0.55, c(trunk))
    cx, cy = top_x, ground - h * 0.72
    R = h * 0.36
    dark, mid, light = shade(leaf, 0.72), leaf, shade(leaf, 1.22)
    for k in range(5):
        a = -math.pi * (0.1 + 0.8 * k / 4.0)
        L.art(blob(cx + math.cos(a) * R * 0.55, cy + math.sin(a) * R * 0.35 + R * 0.25,
                   R * rng.uniform(0.5, 0.62), R * rng.uniform(0.42, 0.52), seed + k, 16, 0.14),
              color=c(dark))
    L.art(blob(cx, cy, R * 0.95, R * 0.75, seed + 11, 24, 0.12), color=c(mid))
    for k in range(4):
        L.art(blob(cx - R * 0.35 + k * R * 0.22, cy - R * 0.38 + (k % 2) * R * 0.12,
                   R * 0.34, R * 0.24, seed + 20 + k, 14, 0.15), color=c(light))
    if fruit is not None:
        for k in range(6):
            L.circle(cx + rng.uniform(-R * 0.7, R * 0.7), cy + rng.uniform(-R * 0.3, R * 0.5), 14,
                     color=c(fruit), inter=False)


def shrub(L, x, ground, w=160, h=None, seed=0, leaf=rgb('3f7f2a'), flowers=None):
    h = h or w * 0.6
    L.art(blob(x, ground - h * 0.45, w * 0.5, h * 0.55, seed, 20, 0.14, flat_bottom=True),
          color=shade(leaf, 0.8))
    L.art(blob(x - w * 0.08, ground - h * 0.6, w * 0.36, h * 0.36, seed + 7, 16, 0.15),
          color=shade(leaf, 1.12))
    if flowers is not None:
        rng = Rng(seed + 3)
        for k in range(int(w / 28)):
            L.circle(x + rng.uniform(-w * 0.4, w * 0.4), ground - rng.uniform(h * 0.2, h * 0.9), 13,
                     color=flowers, inter=False)


def grass_tuft(L, x, y, color, h=22):
    L.art([(x - 12, y + 2), (x - 9, y - h * 0.7), (x - 4, y - 2), (x, y - h), (x + 4, y - 2),
           (x + 10, y - h * 0.8), (x + 13, y + 2)], color=color)


def pebble(L, x, y, color, r=7, seed=0):
    L.art(blob(x, y, r * 1.4, r, seed, 10, 0.15), color=color)


def flower(L, x, ground, h, color, stem=rgb('3e7a2a'), r=11):
    seg(L, x, ground, x, ground - h, 4, stem)
    L.art([(x, ground - h * 0.45), (x + 16, ground - h * 0.6), (x + 4, ground - h * 0.4)], color=stem)
    for k in range(5):
        a = 2 * math.pi * k / 5
        L.circle(x + math.cos(a) * r * 0.75, ground - h + math.sin(a) * r * 0.75, r, color=color,
                 inter=False)
    L.circle(x, ground - h, r * 0.8, color=rgb('f6d548'), inter=False)


def flower_bed(L, x1, x2, ground, colors, seed=0, soil=rgb('5b3a22'), edge=rgb('c8c2b4'),
               tall=(40, 90)):
    """A raised bed: brick / stone edging, dark soil mound, rows of flowers (art)."""
    rng = Rng(seed)
    L.art(blob((x1 + x2) / 2.0, ground - 6, (x2 - x1) / 2.0, 14, seed, 20, 0.05), color=soil)
    x = x1 + 18
    while x < x2 - 14:
        flower(L, x, ground - 8, rng.uniform(*tall), rng.choice(colors), r=rng.uniform(8, 12))
        x += rng.uniform(22, 38)
    L.box(x1, ground - 12, x2, ground, color=edge, inter=False)
    bx = x1
    while bx < x2 - 2:
        L.box(bx + 1, ground - 11, min(x2, bx + 30) - 1, ground - 1, color=shade(edge, 1.08), inter=False)
        bx += 31


def picket_fence(L, x1, x2, ground, h=96, color=0xffffff, shadow=None, spacing=28):
    shadow = shadow if shadow is not None else shade(color, 0.82)
    L.box(x1, ground - h * 0.72, x2, ground - h * 0.62, color=shadow, inter=False)
    L.box(x1, ground - h * 0.32, x2, ground - h * 0.22, color=shadow, inter=False)
    x = x1
    while x <= x2 - 14:
        L.art([(x, ground), (x, ground - h + 12), (x + 7, ground - h), (x + 14, ground - h + 12),
               (x + 14, ground)], color=color)
        L.box(x + 10, ground - h + 12, x + 14, ground, color=shadow, inter=False)
        x += spacing


def board_fence(L, x1, x2, ground, h=150, color=rgb('a8794a'), seed=0):
    """Tall privacy fence of vertical boards with rails (art)."""
    rng = Rng(seed)
    x = x1
    while x < x2 - 4:
        w = 34
        c = shade(color, rng.uniform(0.9, 1.06))
        top = ground - h + rng.uniform(-4, 4)
        L.art([(x, ground), (x, top + 8), (x + w / 2.0, top), (x + w, top + 8), (x + w, ground)], color=c)
        L.box(x + w - 4, top + 8, x + w, ground, color=shade(color, 0.7), inter=False)
        x += w + 2
    for yy in (ground - h * 0.78, ground - h * 0.25):
        L.box(x1, yy, x2, yy + 10, color=shade(color, 0.75), inter=False)


def hedge(L, x1, x2, ground, h=160, color=rgb('2f6b2a'), seed=0, clipped=True):
    """A clipped hedge (flat top, soft edges) or a wild one."""
    w = x2 - x1
    if clipped:
        pts = [(x1, ground)]
        n = max(4, int(w / 40))
        for i in range(n + 1):
            x = x1 + w * i / float(n)
            pts.append((x, ground - h + wobble(i / float(n), seed, 6)))
        pts.append((x2, ground))
        pts = [(x1 + 6, ground)] + [(x1, ground - h * 0.5)] + pts[1:-1] + [(x2, ground - h * 0.5), (x2 - 6, ground)]
        L.art(pts[:100], color=color)
        L.box(x1 + 8, ground - h + 10, x2 - 8, ground - h + 26, color=shade(color, 1.18), inter=False)
        L.box(x1, ground - 20, x2, ground, color=shade(color, 0.75), inter=False)
    else:
        L.art(blob((x1 + x2) / 2.0, ground - h / 2.0, w / 2.0, h / 2.0, seed, 28, 0.1, True), color=color)


def lamp_post(L, x, ground, h=330, color=rgb('3b3f45'), glow=None):
    L.box(x - 9, ground - 24, x + 9, ground, color=shade(color, 0.8), inter=False)
    seg(L, x, ground - 20, x, ground - h, 8, color)
    L.art([(x - 22, ground - h), (x + 22, ground - h), (x + 14, ground - h - 34), (x - 14, ground - h - 34)],
          color=color)
    L.box(x - 15, ground - h - 4, x + 15, ground - h + 22, color=rgb('fff2b0'), inter=False)
    if glow:
        L.circle(x, ground - h + 10, 130, color=glow, opacity=14, inter=False)


def warning_stripes(L, x1, y1, x2, y2, a=rgb('f2c12e'), b=rgb('222222'), w=26):
    """Yellow / black hazard stripes in a box (diagonal parallelograms clipped to the box)."""
    L.box(x1, y1, x2, y2, color=a, inter=False)
    h = y2 - y1
    x = x1 - h
    while x < x2:
        pts = [(x, y2), (x + w, y2), (x + w + h, y1), (x + h, y1)]
        clipped = []
        for px, py in pts:
            clipped.append((min(max(px, x1), x2), py))
        # keep the quad convex after clipping (it stays a trapezoid or triangle)
        if max(p[0] for p in clipped) - min(p[0] for p in clipped) > 1:
            L.art(clipped, color=b)
        x += 2 * w


def text_plate(L, x, y, w, h, color, edge):
    L.box(x - 3, y - 3, x + w + 3, y + h + 3, color=edge, inter=False)
    L.box(x, y, x + w, y + h, color=color, inter=False)


# ---------------------------------------------------------------------------------------------
# Suburbia


def window(L, x, y, w, h, frame=0xffffff, glass=rgb('8ec9e8'), shutters=None, curtain=None,
           sill=True, lit=False):
    """A sash window, (x, y) = top-left of the opening."""
    if shutters is not None:
        L.box(x - w * 0.42, y - 2, x - 6, y + h + 2, color=shutters, inter=False)
        L.box(x + w + 6, y - 2, x + w * 1.42, y + h + 2, color=shutters, inter=False)
        for sx in (x - w * 0.42, x + w + 6):
            for k in range(1, 6):
                L.box(sx + 4, y + h * k / 6.0 - 1, sx + w * 0.36 - 10, y + h * k / 6.0 + 2,
                      color=shade(shutters, 0.8), inter=False)
    L.box(x - 7, y - 7, x + w + 7, y + h + 7, color=frame, inter=False)
    L.box(x, y, x + w, y + h, color=rgb('fff1b8') if lit else glass, inter=False)
    L.art([(x, y + h), (x, y + h * 0.35), (x + w * 0.55, y)] if not lit else
          [(x, y), (x + 1, y), (x, y + 1)], color=shade(glass, 1.25), opacity=55)
    if curtain is not None:
        L.art([(x, y), (x + w * 0.3, y), (x + w * 0.12, y + h)], color=curtain)
        L.art([(x + w, y), (x + w * 0.7, y), (x + w * 0.88, y + h)], color=curtain)
    L.box(x + w / 2.0 - 3, y, x + w / 2.0 + 3, y + h, color=frame, inter=False)
    L.box(x, y + h / 2.0 - 3, x + w, y + h / 2.0 + 3, color=frame, inter=False)
    if sill:
        L.box(x - 14, y + h + 6, x + w + 14, y + h + 16, color=shade(frame, 0.85), inter=False)


def door(L, x, ground, w=90, h=180, color=rgb('8b3a2e'), frame=0xffffff, glass=rgb('8ec9e8')):
    L.box(x - 8, ground - h - 10, x + w + 8, ground, color=frame, inter=False)
    L.box(x, ground - h, x + w, ground, color=color, inter=False)
    for py in (ground - h + 16, ground - h * 0.5 + 6):
        L.box(x + 12, py, x + w - 12, py + h * 0.36, color=shade(color, 0.85), inter=False)
    L.box(x + 18, ground - h + 22, x + w - 18, ground - h + 52, color=glass, inter=False)
    L.circle(x + w - 16, ground - h * 0.48, 9, color=rgb('e8c64a'), inter=False)


def siding(L, x1, y1, x2, y2, color, gap=18):
    L.box(x1, y1, x2, y2, color=color, inter=False)
    y = y1 + gap
    dark = shade(color, 0.9)
    while y < y2 - 2:
        L.box(x1, y, x2, y + 3, color=dark, inter=False)
        y += gap


def bricks(L, x1, y1, x2, y2, color, mortar=None, bw=44, bh=20, seed=0):
    mortar = mortar if mortar is not None else shade(color, 1.35)
    L.box(x1, y1, x2, y2, color=mortar, inter=False)
    rng = Rng(seed)
    row = 0
    y = y1 + 2
    while y < y2 - 2:
        x = x1 + (bw / 2.0 if row % 2 else 0) - bw
        while x < x2:
            a, b = max(x1 + 1, x + 2), min(x2 - 1, x + bw - 2)
            if b - a > 4:
                L.box(a, y, b, min(y2 - 1, y + bh - 3), color=shade(color, rng.uniform(0.88, 1.08)),
                      inter=False)
            x += bw
        y += bh
        row += 1


def roof(L, x1, x2, eave_y, h, color, overhang=40, ridge_inset=0.0, rows=True):
    """A pitched roof seen from the front: gable (ridge_inset 0) or hip (ridge_inset > 0), shingle
    rows, a fascia board and a shadow under the eaves."""
    a, b = x1 - overhang, x2 + overhang
    cx = (x1 + x2) / 2.0
    ri = (b - a) * ridge_inset / 2.0
    top = eave_y - h
    if ri > 0:
        pts = [(a, eave_y), (b, eave_y), (b - ri - h * 0.9, top), (a + ri + h * 0.9, top)]
    else:
        pts = [(a, eave_y), (b, eave_y), (cx, top)]
    L.art([(x1, eave_y), (x2, eave_y), (x2, eave_y + 16), (x1, eave_y + 16)], color=shade(color, 0.35),
          opacity=40)
    L.art(pts, color=color)
    if rows:
        k = 1
        while True:
            y = eave_y - k * 22
            if y < top + 12:
                break
            t = (eave_y - y) / float(h)
            if ri > 0:
                xa = a + (ri + h * 0.9) * t
                xb = b - (ri + h * 0.9) * t
            else:
                xa = a + (cx - a) * t
                xb = b - (b - cx) * t
            L.box(xa + 4, y, xb - 4, y + 4, color=shade(color, 0.8), inter=False)
            k += 1
    L.box(a - 4, eave_y - 4, b + 4, eave_y + 10, color=shade(color, 0.6), inter=False)


def suburban_house(L, x, ground, w=620, h=300, wall=rgb('e9dcc0'), roof_c=rgb('7a3b32'),
                   trim=0xffffff, door_c=rgb('3c5a8a'), shutters=None, two_story=False,
                   chimney=True, porch=False, brick_base=rgb('9c5a3c'), seed=0, hip=False,
                   lit=False, garage=False, garage_c=None):
    """A detailed house facade, left edge at x: siding, brick foundation, windows with trim and
    shutters, front door, roof with shingle rows, gutter and downspout, chimney, optional porch
    and attached garage. Art only."""
    rng = Rng(seed)
    top = ground - h
    if chimney:
        cx = x + w * 0.74
        bricks(L, cx, top - h * 0.55, cx + 56, top + 20, rgb('9a4b35'), seed=seed + 1, bw=28, bh=14)
        L.box(cx - 6, top - h * 0.55 - 12, cx + 62, top - h * 0.55, color=rgb('6b6b6b'), inter=False)
    siding(L, x, top, x + w, ground - 40, wall)
    bricks(L, x - 4, ground - 44, x + w + 4, ground, brick_base, seed=seed, bw=40, bh=16)
    L.box(x - 8, top, x + 6, ground - 44, color=trim, inter=False)
    L.box(x + w - 6, top, x + w + 8, ground - 44, color=trim, inter=False)
    roof(L, x, x + w, top, h * (0.55 if two_story else 0.62), roof_c, ridge_inset=0.28 if hip else 0)
    L.box(x - 40, top + 8, x + w + 40, top + 18, color=shade(trim, 0.85), inter=False)   # gutter
    L.box(x + w + 22, top + 14, x + w + 32, ground - 6, color=shade(trim, 0.85), inter=False)
    L.box(x + w + 16, ground - 14, x + w + 42, ground - 4, color=shade(trim, 0.85), inter=False)
    dw = 88
    dx = x + w * 0.5 - dw / 2.0 if not garage else x + w * 0.22
    door(L, dx, ground - 40, dw, min(176, h * 0.62), door_c, trim)
    L.box(dx - 30, ground - 40, dx + dw + 30, ground - 26, color=rgb('b9b4aa'), inter=False)
    L.box(dx - 44, ground - 26, dx + dw + 44, ground - 12, color=rgb('aaa59b'), inter=False)
    L.box(dx - 58, ground - 12, dx + dw + 58, ground, color=rgb('9b968c'), inter=False)
    ww, wh = 96, 104
    floors = [top + h * 0.18] if not two_story else [top + h * 0.1, top + h * 0.56]
    for fy in floors:
        if garage:
            xs = [x + w * 0.62 - ww / 2] if fy == floors[-1] else [x + w * 0.22, x + w * 0.62]
        else:
            xs = [x + w * 0.16, x + w * 0.84 - ww]
            if two_story and fy == floors[0]:
                xs = [x + w * 0.16, x + w * 0.5 - ww / 2.0, x + w * 0.84 - ww]
        for wx in xs:
            if garage and fy == floors[-1] and wx < x + w * 0.5:
                continue
            window(L, wx, fy, ww, wh, frame=trim, shutters=shutters,
                   curtain=rng.choice([rgb('f2e6c9'), rgb('d9a7a0'), rgb('c9d8b4')]), lit=lit)
    if garage:
        gx1, gx2 = x + w * 0.5, x + w - 30
        gc = garage_c if garage_c is not None else shade(trim, 0.95)
        L.box(gx1 - 10, ground - 210, gx2 + 10, ground - 40, color=trim, inter=False)
        L.box(gx1, ground - 200, gx2, ground - 40, color=gc, inter=False)
        y = ground - 200
        while y < ground - 50:
            L.box(gx1, y + 36, gx2, y + 40, color=shade(gc, 0.8), inter=False)
            px = gx1 + 12
            while px < gx2 - 40:
                L.box(px, y + 8, px + (gx2 - gx1 - 60) / 4.0, y + 30, color=shade(gc, 0.92), inter=False)
                px += (gx2 - gx1 - 12) / 4.0
            y += 40
        L.box(gx1 + 14, ground - 196, gx2 - 14, ground - 172, color=rgb('8ec9e8'), inter=False)
    if porch:
        px1, px2 = dx - 90, dx + dw + 90
        L.art([(px1 - 20, top + h * 0.22), (px2 + 20, top + h * 0.22), (px2 + 4, top + h * 0.12), (px1 - 4, top + h * 0.12)],
              color=roof_c)
        for px in (px1, px2 - 16):
            L.box(px, top + h * 0.22, px + 16, ground - 40, color=trim, inter=False)
        L.box(px1, ground - 70, px2, ground - 64, color=trim, inter=False)


def distant_houses(L, x1, x2, base, sky, seed=0, t=0.6, scale=0.55):
    """A row of far-away houses and trees as pale silhouettes (depth, not detail)."""
    rng = Rng(seed)
    x = x1
    walls = [rgb('e9dcc0'), rgb('cfe0ea'), rgb('f0d3c6'), rgb('dfe8c8')]
    roofs = [rgb('7a3b32'), rgb('3b4f7a'), rgb('5a4a3a'), rgb('4a6b3a')]
    while x < x2:
        w = rng.uniform(260, 420) * scale
        h = rng.uniform(150, 230) * scale
        k = int(rng.random() * 4)
        L.box(x, base - h, x + w, base, color=haze(walls[k], sky, t), inter=False)
        L.art([(x - 18 * scale, base - h), (x + w + 18 * scale, base - h), (x + w / 2.0, base - h - h * 0.6)],
              color=haze(roofs[k], sky, t))
        for wx in (x + w * 0.2, x + w * 0.62):
            L.box(wx, base - h * 0.7, wx + w * 0.18, base - h * 0.4, color=haze(rgb('8ec9e8'), sky, t + 0.1),
                  inter=False)
        x += w + rng.uniform(40, 120) * scale
        if rng.random() < 0.6:
            th = rng.uniform(220, 340) * scale
            L.art(blob(x - 20 * scale, base - th * 0.7, th * 0.32, th * 0.34, seed + int(x), 16, 0.14),
                  color=haze(rgb('4f8f2f'), sky, t))
            seg(L, x - 20 * scale, base, x - 20 * scale, base - th * 0.4, 10 * scale, haze(rgb('6b4423'), sky, t))


def station_wagon(L, x, ground, color=rgb('b8432f'), facing=1):
    """A parked car (art), x = centre."""
    def X(dx):
        return x + dx * facing
    body = [(X(-190), ground - 34), (X(-196), ground - 92), (X(-150), ground - 104), (X(-100), ground - 160),
            (X(110), ground - 160), (X(150), ground - 106), (X(196), ground - 96), (X(200), ground - 40),
            (X(190), ground - 30)]
    L.art(body, color=color)
    L.art([(X(-188), ground - 60), (X(198), ground - 60), (X(198), ground - 36), (X(-188), ground - 36)],
          color=shade(color, 0.75))
    L.art([(X(-90), ground - 150), (X(-20), ground - 150), (X(-20), ground - 106), (X(-136), ground - 106)],
          color=rgb('a7d3ea'))
    L.art([(X(-8), ground - 150), (X(104), ground - 150), (X(138), ground - 106), (X(-8), ground - 106)],
          color=rgb('a7d3ea'))
    L.box(min(X(-190), X(-160)), ground - 74, max(X(-190), X(-160)), ground - 64, color=rgb('ffe28a'), inter=False)
    L.box(min(X(186), X(200)), ground - 82, max(X(186), X(200)), ground - 66, color=rgb('d63a2a'), inter=False)
    L.box(min(X(-30), X(-6)), ground - 96, max(X(-30), X(-6)), ground - 90, color=shade(color, 0.6), inter=False)
    for wx in (X(-120), X(120)):
        L.circle(wx, ground - 30, 64, color=rgb('222222'), inter=False)
        L.circle(wx, ground - 30, 34, color=rgb('b9bec4'), inter=False)
        L.circle(wx, ground - 30, 12, color=rgb('6d737a'), inter=False)


def mailbox(L, x, ground, color=rgb('2d5ea8')):
    L.box(x - 5, ground - 110, x + 5, ground, color=rgb('6b4423'), inter=False)
    L.art(ellipse(x, ground - 128, 34, 24, 14, 180, 360) + [(x + 34, ground - 104), (x - 34, ground - 104)],
          color=color)
    L.box(x + 18, ground - 160, x + 24, ground - 126, color=rgb('d63a2a'), inter=False)
    L.box(x + 18, ground - 160, x + 40, ground - 148, color=rgb('d63a2a'), inter=False)


def umbrella(L, x, ground, h=230, w=230, a=rgb('e8463c'), b=0xffffff):
    seg(L, x, ground, x + 10, ground - h, 7, rgb('dcdcdc'))
    n = 6
    for k in range(n):
        x1 = x + 10 - w / 2.0 + w * k / float(n)
        x2 = x1 + w / float(n)
        L.art([(x1, ground - h + 36), (x2, ground - h + 36), (x + 10, ground - h - 34)],
              color=a if k % 2 == 0 else b)


def towel(L, x, ground, w=170, color=rgb('e8463c'), stripe=0xffffff):
    L.box(x - w / 2.0, ground - 6, x + w / 2.0, ground, color=color, inter=False)
    for k in range(3):
        sx = x - w / 2.0 + 20 + k * (w - 40) / 2.0
        L.box(sx, ground - 6, sx + 10, ground, color=stripe, inter=False)


def bbq_grill(L, x, ground):
    for dx in (-36, 30):
        seg(L, x + dx, ground, x + dx * 0.6, ground - 70, 6, rgb('333333'))
    L.art(ellipse(x, ground - 84, 56, 34, 14, 0, 180) + [(x - 56, ground - 84)], color=rgb('2b2b2b'))
    L.art(ellipse(x, ground - 88, 56, 36, 14, 180, 360), color=rgb('3d3d3d'))
    L.box(x - 60, ground - 90, x + 60, ground - 84, color=rgb('888888'), inter=False)
    for k in range(3):
        L.art(blob(x - 20 + k * 20, ground - 150 - k * 14, 16, 22, k, 10, 0.2), color=rgb('d9d9d9'),
              opacity=45)


def gnome(L, x, ground, hat=rgb('d63a2a'), shirt=rgb('2d6ea8')):
    L.art([(x - 14, ground), (x + 14, ground), (x + 12, ground - 30), (x - 12, ground - 30)], color=shirt)
    L.art(ellipse(x, ground - 36, 11, 16, 10, 0, 180) + [(x - 11, ground - 36)], color=0xffffff)
    L.circle(x, ground - 46, 18, color=rgb('f2c9a0'), inter=False)
    L.art([(x - 12, ground - 50), (x + 12, ground - 50), (x + 2, ground - 84)], color=hat)


def flamingo(L, x, ground):
    pink = rgb('f07fa8')
    seg(L, x, ground, x, ground - 70, 3, rgb('333333'))
    L.art(blob(x + 4, ground - 84, 24, 14, 3, 12, 0.1), color=pink)
    polyline_art(L, [(x - 14, ground - 90), (x - 18, ground - 118), (x - 8, ground - 128)], 6, pink)
    L.art([(x - 8, ground - 130), (x + 4, ground - 126), (x - 6, ground - 122)], color=rgb('333333'))


def birdbath(L, x, ground, stone=rgb('c9c4b8')):
    L.art([(x - 26, ground), (x + 26, ground), (x + 10, ground - 20), (x + 8, ground - 80), (x - 8, ground - 80),
           (x - 10, ground - 20)], color=stone)
    L.art(ellipse(x, ground - 84, 50, 16, 12, 0, 180) + [(x - 50, ground - 84)], color=shade(stone, 0.9))
    L.box(x - 46, ground - 90, x + 46, ground - 84, color=rgb('7cc0e6'), inter=False)


def arbor(L, x, ground, w=220, h=300, wood=0xffffff, rose=rgb('d6336c'), leaf=rgb('3f7f2a'), seed=0):
    """Rose arch: two posts and a curved top dressed in climbing roses (art)."""
    for px in (x, x + w - 16):
        L.box(px, ground - h, px + 16, ground, color=wood, inter=False)
    arc = ellipse(x + w / 2.0, ground - h, w / 2.0 + 6, 70, 16, 180, 360)
    inner = ellipse(x + w / 2.0, ground - h, w / 2.0 - 14, 52, 16, 360, 180)
    L.art(arc + inner, color=wood)
    rng = Rng(seed)
    for k in range(18):
        t = k / 17.0
        if t < 0.3:
            px, py = x + 8 + rng.uniform(-14, 20), ground - h * (t / 0.3) * 0.9
        elif t > 0.7:
            px, py = x + w - 8 + rng.uniform(-20, 14), ground - h * ((1 - t) / 0.3) * 0.9
        else:
            a = math.pi * (1 + (t - 0.3) / 0.4)
            px, py = x + w / 2.0 + math.cos(a) * w / 2.0, ground - h + math.sin(a) * 64
        L.art(blob(px, py, 22, 16, seed + k, 9, 0.2), color=leaf)
        if k % 2 == 0:
            L.circle(px + 4, py - 3, 13, color=rose, inter=False)


def topiary(L, x, ground, kind=0, leaf=rgb('2f6b2a'), pot=rgb('b5623a')):
    L.art([(x - 34, ground), (x + 34, ground), (x + 42, ground - 50), (x - 42, ground - 50)], color=pot)
    L.box(x - 46, ground - 58, x + 46, ground - 48, color=shade(pot, 0.85), inter=False)
    seg(L, x, ground - 50, x, ground - 110, 8, rgb('6b4423'))
    if kind == 0:
        L.circle(x, ground - 140, 92, color=leaf, inter=False)
        L.circle(x - 12, ground - 150, 46, color=shade(leaf, 1.15), inter=False)
    else:
        L.circle(x, ground - 120, 60, color=leaf, inter=False)
        L.circle(x, ground - 180, 80, color=leaf, inter=False)
        L.circle(x, ground - 240, 52, color=leaf, inter=False)
        L.circle(x - 10, ground - 190, 34, color=shade(leaf, 1.15), inter=False)


def greenhouse(L, x1, x2, ground, h=260, frame=0xffffff, glass=rgb('bfe6ef')):
    L.box(x1, ground - h, x2, ground, color=glass, opacity=55, inter=False)
    L.art([(x1 - 10, ground - h), (x2 + 10, ground - h), ((x1 + x2) / 2.0, ground - h - 110)], color=glass, opacity=55)
    x = x1
    while x <= x2:
        L.box(x - 4, ground - h, x + 4, ground, color=frame, inter=False)
        x += (x2 - x1) / 4.0
    for yy in (ground - h, ground - h * 0.5, ground - 30):
        L.box(x1, yy - 3, x2, yy + 3, color=frame, inter=False)
    seg(L, x1 - 10, ground - h, (x1 + x2) / 2.0, ground - h - 110, 7, frame)
    seg(L, x2 + 10, ground - h, (x1 + x2) / 2.0, ground - h - 110, 7, frame)


# ---------------------------------------------------------------------------------------------
# Jungle temple


def stone_wall(L, x1, y1, x2, y2, color, seed=0, bw=(90, 150), bh=56, mortar=None, vary=0.08,
               cracks=0.15):
    """Masonry: a mortar fill with staggered blocks of varied width and tone, a light top edge
    per block and the odd crack. Rows are clipped to the box."""
    rng = Rng(seed)
    mortar = mortar if mortar is not None else shade(color, 0.62)
    L.box(x1, y1, x2, y2, color=mortar, inter=False)
    y = y1
    row = 0
    while y < y2 - 6:
        yb = min(y2, y + bh)
        x = x1 - rng.uniform(0, bw[0])
        while x < x2:
            w = rng.uniform(*bw)
            a, b = max(x1, x + 3), min(x2, x + w - 3)
            if b - a > 12 and yb - y > 10:
                c = shade(color, rng.uniform(1 - vary, 1 + vary * 0.6))
                L.box(a, y + 3, b, yb - 3, color=c, inter=False)
                L.box(a, y + 3, b, y + 9, color=shade(c, 1.12), inter=False)
                if rng.random() < cracks:
                    cx = rng.uniform(a + 10, b - 10)
                    polyline_art(L, [(cx, y + 6), (cx + rng.uniform(-14, 14), (y + yb) / 2.0),
                                     (cx + rng.uniform(-10, 10), yb - 6)], 3, shade(c, 0.6))
            x += w
        y += bh
        row += 1


def carved_band(L, x1, x2, y, h, color, kind=0):
    """A frieze: a raised band with a repeating carved motif (steps, spirals, eyes)."""
    L.box(x1, y, x2, y + h, color=color, inter=False)
    L.box(x1, y, x2, y + 5, color=shade(color, 1.18), inter=False)
    L.box(x1, y + h - 5, x2, y + h, color=shade(color, 0.65), inter=False)
    dark = shade(color, 0.6)
    x = x1 + 10
    step = h * 1.6
    while x + step < x2:
        if kind == 0:   # stepped fret
            L.art([(x, y + h - 8), (x + step * 0.25, y + h - 8), (x + step * 0.25, y + h * 0.55),
                   (x + step * 0.5, y + h * 0.55), (x + step * 0.5, y + 10), (x + step * 0.8, y + 10),
                   (x + step * 0.8, y + 16), (x + step * 0.56, y + 16), (x + step * 0.56, y + h * 0.55 + 6),
                   (x + step * 0.31, y + h * 0.55 + 6), (x + step * 0.31, y + h - 2), (x, y + h - 2)], color=dark)
        elif kind == 1:  # eyes
            L.art(ellipse(x + step / 2.0, y + h / 2.0, step * 0.3, h * 0.22, 12), color=dark)
            L.circle(x + step / 2.0, y + h / 2.0, h * 0.25, color=shade(color, 1.2), inter=False)
        else:            # circles
            L.circle(x + step / 2.0, y + h / 2.0, h * 0.6, color=dark, inter=False)
            L.circle(x + step / 2.0, y + h / 2.0, h * 0.3, color=color, inter=False)
        x += step


def column(L, x, top, bottom, w=70, color=rgb('b39b6c'), broken=False, seed=0):
    """A fluted stone column with capital and base (art)."""
    L.box(x - w * 0.7, bottom - 26, x + w * 0.7, bottom, color=shade(color, 0.85), inter=False)
    L.box(x - w * 0.6, bottom - 40, x + w * 0.6, bottom - 24, color=color, inter=False)
    t = top + 34
    if broken:
        rng = Rng(seed)
        t = top + (bottom - top) * rng.uniform(0.3, 0.6)
        L.art([(x - w / 2.0, t + 20), (x - w * 0.2, t - 6), (x + w * 0.1, t + 14), (x + w / 2.0, t - 10),
               (x + w / 2.0, t + 30), (x - w / 2.0, t + 30)], color=color)
    L.box(x - w / 2.0, t + 20, x + w / 2.0, bottom - 40, color=color, inter=False)
    for k in (-1, 0, 1):
        L.box(x + k * w * 0.28 - 4, t + 26, x + k * w * 0.28 + 4, bottom - 46, color=shade(color, 0.8),
              inter=False)
    L.box(x + w * 0.3, t + 20, x + w / 2.0, bottom - 40, color=shade(color, 0.75), inter=False)
    if not broken:
        L.box(x - w * 0.75, top, x + w * 0.75, top + 22, color=shade(color, 0.9), inter=False)
        L.box(x - w * 0.62, top + 20, x + w * 0.62, top + 36, color=color, inter=False)


def vine(L, x, top, length, seed=0, color=rgb('3f7a2a'), leaf=rgb('5aa040')):
    """A hanging vine: a wavy stem with leaves (art)."""
    rng = Rng(seed)
    pts = []
    n = max(3, int(length / 40))
    for i in range(n + 1):
        t = i / float(n)
        pts.append((x + math.sin(t * 5 + seed) * 10 * t, top + length * t))
    polyline_art(L, pts, 5, color)
    for i in range(1, n + 1):
        px, py = pts[i]
        s = 1 if i % 2 else -1
        L.art([(px, py), (px + s * 22, py - 8 + rng.uniform(-4, 4)), (px + s * 10, py + 6)], color=leaf)


def fern(L, x, ground, h=90, color=rgb('3e8a34'), seed=0):
    for k in range(7):
        a = math.radians(-160 + k * 23.3)
        tip = (x + math.cos(a) * h, ground + math.sin(a) * h * 0.9)
        L.art([(x - 4, ground), (tip[0], tip[1]), (x + 4, ground)], color=shade(color, 0.9 + (k % 3) * 0.08))


def jungle_tree(L, x, ground, h=520, seed=0, leaf=rgb('2f7a3a'), trunk=rgb('5a4030'), haze_to=None,
                haze_t=0.0):
    """A tall rainforest tree: buttressed trunk, high layered canopy, a hanging vine or two."""
    def c(col):
        return haze(col, haze_to, haze_t) if haze_to is not None else col
    w = h * 0.07
    L.art([(x - w * 2.4, ground), (x - w, ground - h * 0.12), (x - w * 0.8, ground - h * 0.8),
           (x + w * 0.8, ground - h * 0.8), (x + w, ground - h * 0.12), (x + w * 2.2, ground)], color=c(trunk))
    L.box(x + w * 0.2, ground - h * 0.8, x + w * 0.8, ground - h * 0.1, color=c(shade(trunk, 0.75)), inter=False)
    for k in range(3):
        cy = ground - h * (0.78 + 0.08 * k)
        rx = h * (0.42 - 0.08 * k)
        L.art(blob(x + (k - 1) * h * 0.06, cy, rx, h * 0.1, seed + k, 22, 0.16),
              color=c(shade(leaf, 0.8 + 0.15 * k)))
    if haze_to is None:
        vine(L, x - h * 0.25, ground - h * 0.74, h * 0.35, seed)
        vine(L, x + h * 0.3, ground - h * 0.72, h * 0.25, seed + 3)


def far_ruins(L, x1, x2, base, sky, seed=0, t=0.6):
    """Pale silhouettes of a lost city on the horizon: stepped pyramids and broken towers."""
    rng = Rng(seed)
    x = x1
    stone = rgb('8a8a6a')
    while x < x2:
        kind = rng.random()
        if kind < 0.45:
            w = rng.uniform(300, 480)
            h = rng.uniform(150, 230)
            steps = 5
            for k in range(steps):
                sw = w * (1 - k / float(steps + 1))
                L.box(x + (w - sw) / 2.0, base - h * (k + 1) / steps, x + (w + sw) / 2.0, base - h * k / steps,
                      color=haze(shade(stone, 1 - k * 0.04), sky, t), inter=False)
            L.box(x + w * 0.42, base - h - 40, x + w * 0.58, base - h, color=haze(stone, sky, t), inter=False)
            x += w + rng.uniform(80, 200)
        else:
            w = rng.uniform(60, 110)
            h = rng.uniform(120, 220)
            L.art([(x, base), (x, base - h), (x + w * 0.3, base - h - 18), (x + w * 0.6, base - h + 8),
                   (x + w, base - h - 6), (x + w, base)], color=haze(stone, sky, t))
            x += w + rng.uniform(60, 160)
        L.art(blob(x - 30, base - 30, rng.uniform(80, 160), 40, int(x), 14, 0.2), color=haze(rgb('2f6a3a'), sky, t))


def torch(L, x, y, lit=True):
    """A wall torch: bracket, stick, flame and a soft glow (art)."""
    L.art([(x - 14, y + 30), (x + 14, y + 30), (x + 8, y + 44), (x - 8, y + 44)], color=rgb('3a3a3a'))
    seg(L, x, y + 40, x, y - 10, 8, rgb('6b4423'))
    if lit:
        L.circle(x, y - 26, 150, color=rgb('ffb347'), opacity=12, inter=False)
        L.circle(x, y - 26, 70, color=rgb('ffcc66'), opacity=22, inter=False)
        L.art([(x - 14, y - 8), (x + 14, y - 8), (x + 6, y - 40), (x + 2, y - 30), (x - 4, y - 52), (x - 8, y - 28)],
              color=rgb('ff8c1a'))
        L.art([(x - 7, y - 8), (x + 7, y - 8), (x + 2, y - 30), (x - 4, y - 22)], color=rgb('ffe066'))


def skull(L, x, y, s=1.0, color=rgb('e8e0c8')):
    L.circle(x, y, 26 * s, color=color, inter=False)
    L.box(x - 8 * s, y + 8 * s, x + 8 * s, y + 18 * s, color=color, inter=False)
    L.circle(x - 6 * s, y - 2 * s, 7 * s, color=rgb('2a2018'), inter=False)
    L.circle(x + 6 * s, y - 2 * s, 7 * s, color=rgb('2a2018'), inter=False)


def stone_face(L, cx, cy, w, h, color, mouth=True, eye=rgb('ffd34d')):
    """A great carved mask (temple doors, trap housings): brow, eyes, nose, a mouth slot."""
    dark = shade(color, 0.55)
    L.art([(cx - w / 2.0, cy - h / 2.0), (cx + w / 2.0, cy - h / 2.0), (cx + w * 0.42, cy + h / 2.0),
           (cx - w * 0.42, cy + h / 2.0)], color=color)
    L.box(cx - w / 2.0, cy - h / 2.0, cx + w / 2.0, cy - h * 0.36, color=shade(color, 1.12), inter=False)
    L.box(cx - w * 0.44, cy - h * 0.26, cx + w * 0.44, cy - h * 0.16, color=dark, inter=False)
    for s in (-1, 1):
        L.art(ellipse(cx + s * w * 0.22, cy - h * 0.06, w * 0.13, h * 0.08, 12), color=dark)
        L.circle(cx + s * w * 0.22, cy - h * 0.06, h * 0.07, color=eye, inter=False)
    L.art([(cx - w * 0.06, cy - h * 0.12), (cx + w * 0.06, cy - h * 0.12), (cx + w * 0.1, cy + h * 0.14),
           (cx - w * 0.1, cy + h * 0.14)], color=shade(color, 0.85))
    if mouth:
        L.box(cx - w * 0.3, cy + h * 0.2, cx + w * 0.3, cy + h * 0.36, color=dark, inter=False)
        for k in range(6):
            tx = cx - w * 0.27 + k * w * 0.108
            L.art([(tx, cy + h * 0.2), (tx + w * 0.07, cy + h * 0.2), (tx + w * 0.035, cy + h * 0.27)],
                  color=shade(color, 1.05))


def gold_pile(L, x, ground, w=160, h=50, seed=0):
    rng = Rng(seed)
    L.art(blob(x, ground - h * 0.35, w / 2.0, h * 0.6, seed, 18, 0.12, flat_bottom=True), color=rgb('c9971a'))
    for k in range(int(w / 12)):
        L.circle(x + rng.uniform(-w * 0.42, w * 0.42), ground - rng.uniform(4, h * 0.8), rng.uniform(10, 16),
                 color=rng.choice([rgb('f2c94c'), rgb('e5b80b'), rgb('ffe08a')]), inter=False)
    L.art([(x + w * 0.2, ground - h * 0.7), (x + w * 0.34, ground - h * 1.2), (x + w * 0.4, ground - h * 0.7)],
          color=rgb('9b59b6'))


def light_shaft(L, x_top, y_top, x_bot, y_bot, w_top, w_bot, color=rgb('fff2b0'), opacity=18):
    L.art([(x_top - w_top / 2.0, y_top), (x_top + w_top / 2.0, y_top), (x_bot + w_bot / 2.0, y_bot),
           (x_bot - w_bot / 2.0, y_bot)], color=color, opacity=opacity)
    L.art([(x_top - w_top / 4.0, y_top), (x_top + w_top / 4.0, y_top), (x_bot + w_bot / 4.0, y_bot),
           (x_bot - w_bot / 4.0, y_bot)], color=color, opacity=opacity)


def crystal(L, x, ground, h=120, color=rgb('6fd3e8'), rot=0.0):
    a = math.radians(rot)

    def P(dx, dy):
        return (x + dx * math.cos(a) - dy * math.sin(a), ground + dx * math.sin(a) + dy * math.cos(a))
    L.art([P(-h * 0.18, 0), P(-h * 0.2, -h * 0.75), P(0, -h), P(h * 0.2, -h * 0.75), P(h * 0.18, 0)], color=color)
    L.art([P(0, 0), P(0, -h), P(h * 0.2, -h * 0.75), P(h * 0.18, 0)], color=shade(color, 0.75))
    L.circle(x, ground - h * 0.5, h * 1.2, color=color, opacity=10, inter=False)


# ---------------------------------------------------------------------------------------------
# Airfield and hangars


def corrugated(L, x1, y1, x2, y2, color, rib=22):
    """Corrugated metal cladding: base colour with light and dark vertical ribs."""
    L.box(x1, y1, x2, y2, color=color, inter=False)
    x = x1
    k = 0
    while x < x2 - 4:
        L.box(x, y1, min(x2, x + 6), y2, color=shade(color, 1.1 if k % 2 == 0 else 0.86), inter=False)
        x += rib
        k += 1


def truss(L, x1, x2, y_top, depth, color, panel=None, w=10):
    """A steel roof truss (art): top and bottom chords with diagonal and vertical webs."""
    panel = panel or depth * 1.2
    L.box(x1, y_top, x2, y_top + w + 4, color=color, inter=False)
    L.box(x1, y_top + depth - w, x2, y_top + depth, color=color, inter=False)
    x = x1
    k = 0
    while x < x2 - 2:
        nx = min(x2, x + panel)
        seg(L, x, y_top + depth - w / 2.0, nx, y_top + w / 2.0, w * 0.7, shade(color, 0.85)) if k % 2 == 0 else \
            seg(L, x, y_top + w / 2.0, nx, y_top + depth - w / 2.0, w * 0.7, shade(color, 0.85))
        L.box(x - w * 0.4, y_top, x + w * 0.4, y_top + depth, color=color, inter=False)
        x = nx
        k += 1
    L.box(x2 - w * 0.4, y_top, x2 + w * 0.4, y_top + depth, color=color, inter=False)


def i_beam_art(L, x1, y1, x2, y2, color, flange=8):
    """A vertical or horizontal I-section (art) from its bounding box."""
    L.box(x1, y1, x2, y2, color=shade(color, 0.8), inter=False)
    if x2 - x1 < y2 - y1:
        L.box(x1, y1, x1 + flange, y2, color=color, inter=False)
        L.box(x2 - flange, y1, x2, y2, color=color, inter=False)
    else:
        L.box(x1, y1, x2, y1 + flange, color=color, inter=False)
        L.box(x1, y2 - flange, x2, y2, color=color, inter=False)
    for k in range(int(max(x2 - x1, y2 - y1) / 60)):
        if x2 - x1 < y2 - y1:
            L.circle((x1 + x2) / 2.0, y1 + 30 + k * 60, 5, color=shade(color, 1.2), inter=False)
        else:
            L.circle(x1 + 30 + k * 60, (y1 + y2) / 2.0, 5, color=shade(color, 1.2), inter=False)


def crate_art(L, x, ground, w=80, h=None, color=rgb('b07a45'), label=None):
    h = h or w
    L.box(x - w / 2.0, ground - h, x + w / 2.0, ground, color=shade(color, 0.7), inter=False)
    L.box(x - w / 2.0 + 5, ground - h + 5, x + w / 2.0 - 5, ground - 5, color=color, inter=False)
    seg(L, x - w / 2.0 + 8, ground - 8, x + w / 2.0 - 8, ground - h + 8, 7, shade(color, 0.8))
    if label:
        L.box(x - w * 0.3, ground - h * 0.62, x + w * 0.3, ground - h * 0.38, color=label, inter=False)


def drum(L, x, ground, color=rgb('2e6fb0'), h=90, w=60):
    L.box(x - w / 2.0, ground - h, x + w / 2.0, ground, color=color, inter=False)
    for yy in (ground - h * 0.3, ground - h * 0.7):
        L.box(x - w / 2.0, yy - 4, x + w / 2.0, yy + 4, color=shade(color, 0.75), inter=False)
    L.box(x - w / 2.0, ground - h, x + w / 2.0, ground - h + 6, color=shade(color, 1.25), inter=False)


def storage_rack(L, x1, x2, ground, h=330, levels=3, color=rgb('d35400'), upright=rgb('2c5f8a'), seed=0):
    """Pallet racking: blue uprights, orange beams, boxes and drums on each level (art)."""
    rng = Rng(seed)
    span = x2 - x1
    for ux in (x1, x1 + span / 2.0, x2):
        L.box(ux - 7, ground - h, ux + 7, ground, color=upright, inter=False)
        for yy in range(int(ground - h + 20), int(ground), 40):
            seg(L, ux - 6, yy, ux + 6, yy + 30, 3, shade(upright, 1.3))
    for k in range(levels):
        by = ground - 18 - k * (h - 30) / float(levels)
        L.box(x1, by - 10, x2, by, color=color, inter=False)
        x = x1 + 16
        while x < x2 - 60:
            if rng.random() < 0.75:
                w = rng.uniform(44, 76)
                if rng.random() < 0.7:
                    crate_art(L, x + w / 2.0, by - 10, w, w * rng.uniform(0.6, 0.9),
                              rng.choice([rgb('b07a45'), rgb('a06a3a'), rgb('c9a46a')]))
                else:
                    drum(L, x + 22, by - 10, rng.choice([rgb('2e6fb0'), rgb('c0392b'), rgb('27ae60')]), 64, 40)
                x += w + 8
            else:
                x += 50


def toolbox(L, x, ground, color=rgb('c0392b')):
    L.box(x - 50, ground - 110, x + 50, ground, color=color, inter=False)
    for k in range(4):
        L.box(x - 44, ground - 104 + k * 26, x + 44, ground - 84 + k * 26, color=shade(color, 0.85), inter=False)
        L.box(x - 14, ground - 96 + k * 26, x + 14, ground - 92 + k * 26, color=rgb('dddddd'), inter=False)
    for wx in (x - 36, x + 36):
        L.circle(wx, ground - 6, 12, color=rgb('333333'), inter=False)


def plane_art(L, x, ground, color=rgb('e9e4d8'), trim=rgb('c0392b'), s=1.0):
    """A small parked propeller plane (side view, art), x = centre."""
    body = [(x - 220 * s, ground - 120 * s), (x - 160 * s, ground - 150 * s), (x + 150 * s, ground - 150 * s),
            (x + 220 * s, ground - 130 * s), (x + 236 * s, ground - 110 * s), (x + 210 * s, ground - 96 * s),
            (x - 120 * s, ground - 96 * s), (x - 220 * s, ground - 110 * s)]
    L.art([(x - 230 * s, ground - 118 * s), (x - 200 * s, ground - 220 * s), (x - 170 * s, ground - 220 * s),
           (x - 150 * s, ground - 140 * s)], color=trim)
    L.art(body, color=color)
    L.box(x - 160 * s, ground - 134 * s, x + 200 * s, ground - 126 * s, color=trim, inter=False)
    L.art([(x - 40 * s, ground - 118 * s), (x + 80 * s, ground - 118 * s), (x + 60 * s, ground - 104 * s),
           (x - 60 * s, ground - 104 * s)], color=shade(color, 0.85))
    L.art([(x + 120 * s, ground - 150 * s), (x + 170 * s, ground - 150 * s), (x + 160 * s, ground - 172 * s),
           (x + 130 * s, ground - 172 * s)], color=rgb('a7d3ea'))
    L.box(x + 236 * s, ground - 150 * s, x + 244 * s, ground - 70 * s, color=rgb('555555'), inter=False)
    for wx in (x - 100 * s, x + 130 * s):
        seg(L, wx, ground - 96 * s, wx, ground - 26 * s, 8 * s, rgb('555555'))
        L.circle(wx, ground - 22 * s, 44 * s, color=rgb('222222'), inter=False)
        L.circle(wx, ground - 22 * s, 18 * s, color=rgb('9a9a9a'), inter=False)


def control_tower(L, x, ground, h=420, color=rgb('d8d4c8'), glass=rgb('6fb6d6')):
    L.art([(x - 50, ground), (x + 50, ground), (x + 38, ground - h + 90), (x - 38, ground - h + 90)], color=color)
    L.art([(x - 90, ground - h + 90), (x + 90, ground - h + 90), (x + 110, ground - h), (x - 110, ground - h)],
          color=glass)
    L.box(x - 116, ground - h - 20, x + 116, ground - h, color=shade(color, 0.8), inter=False)
    for k in range(4):
        L.box(x - 85 + k * 48, ground - h + 4, x - 81 + k * 48, ground - h + 86, color=shade(color, 0.6), inter=False)
    seg(L, x, ground - h - 20, x, ground - h - 90, 4, rgb('555555'))
    L.circle(x, ground - h - 92, 10, color=rgb('e74c3c'), inter=False)


def windsock(L, x, ground, h=260):
    seg(L, x, ground, x, ground - h, 7, rgb('9aa3ab'))
    for k in range(4):
        L.art([(x + 4 + k * 26, ground - h + 2 + k * 3), (x + 30 + k * 26, ground - h + 5 + k * 3),
               (x + 30 + k * 26, ground - h + 33 - k * 3), (x + 4 + k * 26, ground - h + 36 - k * 3)],
              color=rgb('ff7a1a') if k % 2 == 0 else 0xffffff)


def hazard_lamp(L, x, y, color=rgb('ff9f1a'), lit=True):
    L.box(x - 12, y - 4, x + 12, y + 10, color=rgb('333333'), inter=False)
    L.art(ellipse(x, y - 4, 14, 16, 10, 180, 360), color=color)
    if lit:
        L.circle(x, y - 8, 60, color=color, opacity=18, inter=False)


# ---------------------------------------------------------------------------------------------
# Temple: room transitions and wall dressing


def blend_wall(L, x1, x2, y1, y2, c1, c2, steps=6):
    """A soft hand-over between two rooms' wall colours: vertical bands mixing c1 into c2."""
    w = (x2 - x1) / float(steps)
    for k in range(steps):
        L.box(x1 + k * w, y1, x1 + (k + 1) * w + 1, y2, color=mix(c1, c2, (k + 0.5) / steps), inter=False)


def portal(L, x, floor, top, w=150, color=rgb('9c8458'), opening=None, keystone=rgb('e5b80b')):
    """A massive stone door frame between two rooms (art): two jambs with bases and capitals,
    a lintel with a carved band and a keystone. `opening` = (half width, height) of the passage
    it frames (default: the whole height up to the lintel)."""
    jw = 46
    ow = w / 2.0 if opening is None else opening[0]
    lintel = top if opening is None else floor - opening[1]
    dark, light = shade(color, 0.7), shade(color, 1.15)
    for side in (-1, 1):
        jx1 = x + side * ow if side > 0 else x - ow - jw
        L.box(jx1, lintel, jx1 + jw, floor, color=color, inter=False)
        L.box(jx1 + (jw - 10 if side < 0 else 0), lintel, jx1 + (jw if side < 0 else 10), floor, color=dark,
              inter=False)
        L.box(jx1 - 8, floor - 26, jx1 + jw + 8, floor, color=light, inter=False)
        L.box(jx1 - 6, lintel + 4, jx1 + jw + 6, lintel + 22, color=light, inter=False)
        for yy in range(int(lintel + 60), int(floor - 40), 70):
            L.box(jx1 + 6, yy, jx1 + jw - 6, yy + 4, color=dark, inter=False)
    L.box(x - ow - jw - 14, lintel - 60, x + ow + jw + 14, lintel + 4, color=color, inter=False)
    L.box(x - ow - jw - 14, lintel - 60, x + ow + jw + 14, lintel - 50, color=light, inter=False)
    L.box(x - ow - jw - 14, lintel - 8, x + ow + jw + 14, lintel + 4, color=dark, inter=False)
    L.art([(x - 24, lintel - 64), (x + 24, lintel - 64), (x + 16, lintel - 2), (x - 16, lintel - 2)], color=keystone)
    L.circle(x, lintel - 34, 12, color=shade(keystone, 0.7), inter=False)


def relief_panel(L, x, y, w, h, color, kind=0):
    """A carved wall panel (art): a frame with a scene: 0 serpent, 1 sun and rays, 2 procession."""
    dark, light = shade(color, 0.72), shade(color, 1.15)
    L.box(x, y, x + w, y + h, color=dark, inter=False)
    L.box(x + 8, y + 8, x + w - 8, y + h - 8, color=color, inter=False)
    cx, cy = x + w / 2.0, y + h / 2.0
    if kind == 0:
        pts = [(x + 20 + (w - 40) * t / 10.0, cy + math.sin(t * 1.3) * h * 0.22) for t in range(11)]
        polyline_art(L, pts, 14, dark)
        L.art(ellipse(pts[-1][0] + 8, pts[-1][1], 18, 13, 10), color=dark)
        L.circle(pts[-1][0] + 12, pts[-1][1] - 4, 5, color=light, inter=False)
    elif kind == 1:
        for k in range(10):
            a = 2 * math.pi * k / 10
            seg(L, cx, cy, cx + math.cos(a) * h * 0.4, cy + math.sin(a) * h * 0.4, 6, dark)
        L.circle(cx, cy, h * 0.42, color=dark, inter=False)
        L.circle(cx, cy, h * 0.3, color=light, inter=False)
    else:
        n = max(2, int(w / 60))
        for k in range(n):
            fx = x + 30 + k * (w - 60) / float(max(1, n - 1))
            L.circle(fx, y + h * 0.3, 11, color=dark, inter=False)
            L.art([(fx - 12, y + h * 0.38), (fx + 12, y + h * 0.38), (fx + 16, y + h - 18), (fx - 16, y + h - 18)],
                  color=dark)
            seg(L, fx + 10, y + h * 0.45, fx + 26, y + h * 0.3, 5, dark)


def urn(L, x, ground, h=70, color=rgb('a0522d')):
    L.art([(x - 14, ground), (x + 14, ground), (x + 26, ground - h * 0.45), (x + 14, ground - h * 0.85),
           (x + 18, ground - h), (x - 18, ground - h), (x - 14, ground - h * 0.85), (x - 26, ground - h * 0.45)],
          color=color)
    L.box(x - 26, ground - h * 0.5, x + 26, ground - h * 0.42, color=shade(color, 0.7), inter=False)


def roots(L, x, top, length, seed=0, color=rgb('5a4030')):
    """Tree roots breaking through a ceiling, hanging down (art)."""
    rng = Rng(seed)
    for k in range(3):
        pts = [(x + k * 18, top)]
        for i in range(1, 6):
            px, py = pts[-1]
            pts.append((px + rng.uniform(-14, 14), py + length / 5.0))
        polyline_art(L, pts, 9 - k * 2, shade(color, 1 - 0.08 * k))


def hanging_chain(L, x, top, length, color=rgb('4a4a4a')):
    y = top
    k = 0
    while y < top + length:
        if k % 2 == 0:
            L.art(ellipse(x, y + 8, 6, 9, 8), color=color)
        else:
            L.box(x - 2, y, x + 2, y + 16, color=color, inter=False)
        y += 13
        k += 1


def cobweb(L, x, y, r=60, corner=0):
    """A corner cobweb (art): radial threads and a few rings. corner 0 = top-left, 1 = top-right."""
    s = 1 if corner == 0 else -1
    for k in range(5):
        a = math.radians(k * 22.5)
        seg(L, x, y, x + s * math.cos(a) * r, y + math.sin(a) * r, 2, 0xffffff, opacity=45)
    for rr in (r * 0.4, r * 0.7, r):
        pts = [(x + s * math.cos(math.radians(k * 22.5)) * rr, y + math.sin(math.radians(k * 22.5)) * rr)
               for k in range(5)]
        polyline_art(L, pts, 2, 0xffffff, opacity=40)


def rubble(L, x, ground, w=120, color=rgb('8d7550'), seed=0):
    rng = Rng(seed)
    for k in range(int(w / 22)):
        L.art(blob(x - w / 2.0 + rng.uniform(0, w), ground - rng.uniform(4, 16), rng.uniform(10, 22),
                   rng.uniform(7, 14), seed + k, 8, 0.2), color=shade(color, rng.uniform(0.75, 1.1)))
