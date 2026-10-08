#!/usr/bin/env python3
"""Render a browser (Flash) Happy Wheels level XML to a PNG and sanity-check its layout.

    python3 tools/levels/preview_level.py level.xml [-o out.png] [--width 3000]
                                          [--crop x1,y1,x2,y2] [--no-labels]

Draws shapes (fill, outline, opacity; group shapes placed through their group transform),
specials as labelled footprints, joints, triggers (yellow boxes, lines to their targets), the
start (magenta) and the finish line (checkered). Prints checks: start position vs. the ground
below it, finish line support, items off the stage. Needs Pillow.

Coordinates: Flash px, y down, 20000 x 10000 stage (docs/FLASH_LEVELS.md section 5).
"""

import argparse
import math
import sys
import xml.etree.ElementTree as ET

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:  # pragma: no cover
    sys.exit('preview_level.py needs Pillow (pip install --user Pillow)')

SPECIAL_NAMES = [
    "van", "table", "mine", "I-beam", "log", "spring box", "spikes", "wrecking ball", "fan",
    "finish", "soccer ball", "meteor", "boost", "building", "building", "harpoon gun",
    "text", "NPC", "glass", "chair", "bottle", "TV", "boombox", "sign",
    "toilet", "homing mine", "trash can", "rail", "jet", "arrow gun", "chain", "token",
    "food", "cannon", "blade", "paddle"]


def fnum(e, k, d=0.0):
    v = e.get(k)
    if v is None:
        return d
    try:
        return float(v)
    except ValueError:
        return d


def fbool(e, k, d=False):
    v = e.get(k)
    if v is None:
        return d
    return v in ('t', '1', 'true')


def rot_pt(x, y, deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return x * c - y * s, x * s + y * c


def color_of(v, alpha=255):
    v = int(v) & 0xffffff
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255, alpha)


# ---------------------------------------------------------------------------------------------
# Geometry of items (world polygons, Flash px)

def shape_outline(e, gx=0.0, gy=0.0, gr=0.0, gox=0.0, goy=0.0):
    """World outline points (or ('circle', cx, cy, r)) of a <sh>, optionally in a group."""
    t = int(fnum(e, 't', 0))
    x, y = fnum(e, 'p0'), fnum(e, 'p1')
    w, h = fnum(e, 'p2', 100), fnum(e, 'p3', 100 if t != 2 else 300)
    rot = fnum(e, 'p4')

    def to_world(px, py):
        # shape local -> group local -> world
        lx, ly = px + gox, py + goy
        rx, ry = rot_pt(lx, ly, gr)
        return gx + rx, gy + ry

    if t == 1:
        cx, cy = to_world(x, y)
        return ('circle', cx, cy, w / 2.0)
    if t == 0:
        local = [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]
    elif t == 2:
        local = [(-w / 2, h / 3), (w / 2, h / 3), (0, -2 * h / 3)]
    else:
        v = e.find('v')
        local = []
        if v is not None:
            n = int(fnum(v, 'n', 0))
            for i in range(n):
                s = v.get('v%d' % i)
                if not s:
                    continue
                parts = s.split('_')
                try:
                    local.append((float(parts[0]), float(parts[1])))
                except (ValueError, IndexError):
                    pass
    pts = []
    for px, py in local:
        rx, ry = rot_pt(px, py, rot)
        pts.append(to_world(x + rx, y + ry))
    return pts


def point_in_poly(px, py, pts):
    inside = False
    n = len(pts)
    j = n - 1
    for i in range(n):
        xi, yi = pts[i]
        xj, yj = pts[j]
        if (yi > py) != (yj > py):
            xc = xi + (py - yi) * (xj - xi) / (yj - yi)
            if px < xc:
                inside = not inside
        j = i
    return inside


def inside(geom, px, py):
    if isinstance(geom, tuple) and geom and geom[0] == 'circle':
        _, cx, cy, r = geom
        return (px - cx) ** 2 + (py - cy) ** 2 <= r * r
    return len(geom) >= 3 and point_in_poly(px, py, geom)


def box_pts(cx, cy, w, h, rot=0.0, ox=0.0, oy=0.0):
    pts = []
    for lx, ly in [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]:
        rx, ry = rot_pt(lx + ox, ly + oy, rot)
        pts.append((cx + rx, cy + ry))
    return pts


def special_footprint(e):
    """(outline points, solid?) of a special, approximate (Flash px)."""
    t = int(fnum(e, 't', -1))
    x, y = fnum(e, 'p0'), fnum(e, 'p1')
    p = lambda k, d=0.0: fnum(e, 'p%d' % k, d)
    if t == 0:
        return box_pts(x, y, 132, 116, p(2)), True
    if t == 1:
        return box_pts(x, y, 156, 56, p(2), 0, 22), True
    if t == 2:
        return box_pts(x, y, 25, 10, p(2)), False
    if t in (3, 4):
        return box_pts(x, y, p(2, 300), p(3, 30), p(4)), True
    if t == 5:
        return box_pts(x, y, 350, 36, p(2)), True
    if t == 6:
        n = max(20, min(150, int(p(4, 20))))
        return box_pts(x, y, n * 15, 50, p(2), 0, 8), False
    if t == 7:
        r = max(200, min(1000, p(2, 350)))
        return [('circle', x, y + r, 75)], False
    if t == 8:
        return box_pts(x, y, 300, 50, p(2)), True
    if t == 9:
        return box_pts(x, y, 400, 40), True
    if t == 10:
        return [('circle', x, y, 10)], False
    if t == 11:
        d = max(200, min(600, p(2, 400)))
        return [('circle', x, y, d / 2)], fbool(e, 'p4')
    if t == 12:
        n = max(1, min(6, int(p(3, 2))))
        return box_pts(x, y, n * 180, 14, p(2)), False
    if t in (13, 14):
        fw = max(1, min(10, int(p(2, 1))))
        fl = max(3, min(50, int(p(3, 3))))
        w = fw * 300 if t == 13 else fw * 500 + 226
        h = fl * 165 + 100 if t == 13 else fl * 180 + 120
        return [(x, y), (x + w, y), (x + w, y + h), (x, y + h)], True
    if t == 15:
        return box_pts(x, y, 50, 32, p(2)), False
    if t == 16:
        return box_pts(x + 60, y + 10, 120, 20), False
    if t == 17:
        return box_pts(x, y + 15, 40, 190, p(2)), False
    if t == 18:
        return box_pts(x, y, p(2, 10), p(3, 100), p(4)), False
    if t == 19:
        return box_pts(x, y, 42, 88, p(2), 0, -12), False
    if t == 20:
        return box_pts(x, y, 10, 29, p(2)), False
    if t == 21:
        return box_pts(x, y, 55, 40, p(2)), False
    if t == 22:
        return box_pts(x, y, 48, 28, p(2)), False
    if t == 23:
        return box_pts(x, y - 45, 110, 90, p(2)), False
    if t == 24:
        return box_pts(x, y, 62, 78, p(2)), False
    if t == 25:
        return [('circle', x, y, 12)], False
    if t == 26:
        return box_pts(x, y, 46, 64, p(2)), False
    if t == 27:
        return box_pts(x, y, max(100, min(2000, p(2, 250))), 18, p(4)), True
    if t == 28:
        return box_pts(x, y, 30, 36, p(2)), False
    if t == 29:
        return box_pts(x, y, 28, 16, p(2)), False
    if t == 30:
        n = max(2, min(40, int(p(5, 20))))
        s = 1 + (max(1, min(10, p(6, 1))) - 1) / 9 * 2
        return box_pts(x, y + n * 9 * s / 2, 6 * s, n * 9 * s, p(2)), False
    if t == 31:
        return [('circle', x, y, 23)], False
    if t == 32:
        return box_pts(x, y, 50, 40, p(2)), False
    if t == 33:
        return [(x - 49, y + 55), (x + 49, y + 55), (x + 30, y - 171), (x - 30, y - 171)], True
    if t == 34:
        return box_pts(x, y, 100, 20, p(2)), False
    if t == 35:
        return box_pts(x, y, 350, 40, p(2)), True
    return box_pts(x, y, 30, 30), False


# ---------------------------------------------------------------------------------------------

class Model:
    def _list(self, tag):
        e = self.root.find(tag)
        return list(e) if e is not None else []

    def __init__(self, path):
        self.root = ET.parse(path).getroot()
        self.info = self.root.find('info')
        self.shapes = self._list('shapes')
        self.specials = self._list('specials')
        self.groups = self._list('groups')
        self.joints = self._list('joints')
        self.triggers = self._list('triggers')
        # Solids for the checks: (geometry, static?, label)
        self.solids = []
        for i, e in enumerate(self.shapes):
            if e.get('i') == 'f' or int(fnum(e, 't')) == 4:
                continue
            self.solids.append((shape_outline(e), fbool(e, 'p5'), 'shape %d' % i))
        for i, e in enumerate(self.specials):
            geom, solid = special_footprint(e)
            if solid:
                if geom and isinstance(geom[0], tuple) and geom[0][0] == 'circle':
                    geom = geom[0]
                self.solids.append((geom, True, 'special %d (%s)' % (i, self.special_name(e))))
        for gi, g in enumerate(self.groups):
            for e in g.findall('sh'):
                if e.get('i') == 'f' or int(fnum(e, 't')) == 4:
                    continue
                geom = shape_outline(e, fnum(g, 'x'), fnum(g, 'y'), fnum(g, 'r'),
                                     fnum(g, 'ox'), fnum(g, 'oy'))
                self.solids.append((geom, fbool(g, 'im'), 'group %d' % gi))

    @staticmethod
    def special_name(e):
        t = int(fnum(e, 't', -1))
        return SPECIAL_NAMES[t] if 0 <= t < len(SPECIAL_NAMES) else 'sp%d' % t

    def solid_at(self, x, y, static_only=False):
        for geom, static, label in self.solids:
            if static_only and not static:
                continue
            if inside(geom, x, y):
                return label
        return None

    def ground_below(self, x, y, limit=1500, static_only=False):
        """Distance from y down to the first solid at x (None if nothing within limit)."""
        step = 4
        d = 0
        while d <= limit:
            if self.solid_at(x, y + d, static_only):
                return d
            d += step
        return None

    def item_pos(self, kind, index):
        lists = dict(sh=self.shapes, sp=self.specials, g=self.groups, j=self.joints,
                     t=self.triggers)
        lst = lists.get(kind)
        if lst is None or not (0 <= index < len(lst)):
            return None
        e = lst[index]
        if kind in ('sh', 'sp'):
            return fnum(e, 'p0'), fnum(e, 'p1')
        if kind == 'g':
            # centre of its shapes
            pts = []
            for s in e.findall('sh'):
                geom = shape_outline(s, fnum(e, 'x'), fnum(e, 'y'), fnum(e, 'r'), fnum(e, 'ox'),
                                     fnum(e, 'oy'))
                if isinstance(geom, tuple):
                    pts.append((geom[1], geom[2]))
                else:
                    pts += geom
            if not pts:
                return fnum(e, 'x'), fnum(e, 'y')
            return sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts)
        return fnum(e, 'x'), fnum(e, 'y')

    def bounds(self):
        xs, ys = [], []

        def add(pts):
            if isinstance(pts, tuple) and pts and pts[0] == 'circle':
                _, cx, cy, r = pts
                xs.extend([cx - r, cx + r])
                ys.extend([cy - r, cy + r])
            else:
                for p in pts:
                    if isinstance(p, tuple) and p and p[0] == 'circle':
                        add(p)
                    else:
                        xs.append(p[0])
                        ys.append(p[1])

        for e in self.shapes:
            add(shape_outline(e))
        for e in self.specials:
            add(special_footprint(e)[0])
        for g in self.groups:
            for e in g.findall('sh'):
                add(shape_outline(e, fnum(g, 'x'), fnum(g, 'y'), fnum(g, 'r'), fnum(g, 'ox'),
                                  fnum(g, 'oy')))
        sx, sy = fnum(self.info, 'x'), fnum(self.info, 'y')
        xs.append(sx)
        ys.append(sy)
        return min(xs), min(ys), max(xs), max(ys)

    def checks(self):
        out = []
        sx, sy = fnum(self.info, 'x'), fnum(self.info, 'y')
        for name, (x, y) in [('start', (sx, sy))]:
            if not (0 <= x <= 20000 and 0 <= y <= 10000):
                out.append('ERROR %s off stage' % name)
        # The rider's body sits around the start point; the vehicle below it.
        for dx, dy in [(0, 0), (-60, 0), (60, 0), (0, -60), (-80, 40), (80, 40)]:
            hit = self.solid_at(sx + dx, sy + dy)
            if hit:
                out.append('ERROR start overlaps %s at offset %r' % (hit, (dx, dy)))
        d = self.ground_below(sx, sy)
        if d is None:
            out.append('ERROR no ground below the start')
        elif d > 260:
            out.append('WARN start is %d px above the ground' % d)
        elif d < 70:
            out.append('WARN start only %d px above the ground (vehicle may overlap it)' % d)
        else:
            out.append('ok start %d px above the ground' % d)
        finishes = [e for e in self.specials if int(fnum(e, 't', -1)) == 9]
        victories = [t for t in self.triggers if int(fnum(t, 't', 1)) == 3]
        if not finishes and not victories:
            out.append('ERROR no finish line and no victory trigger')
        for f in finishes:
            fx, fy = fnum(f, 'p0'), fnum(f, 'p1')
            d = self.ground_below(fx - 150, fy - 17, 400, static_only=False)
            if d is None or d > 40:
                out.append('WARN finish line at %r has no support close below (%r)' % ((fx, fy), d))
            else:
                out.append('ok finish at %d,%d' % (fx, fy))
            for dx in (-150, 0, 150):
                hit = self.solid_at(fx + dx, fy - 110)
                if hit:
                    out.append('WARN finish line blocked above by %s' % hit)
        for i, e in enumerate(self.specials):
            x, y = fnum(e, 'p0'), fnum(e, 'p1')
            if not (0 <= x <= 20000 and 0 <= y <= 10000):
                out.append('ERROR special %d off stage' % i)
            if int(fnum(e, 't', -1)) == 16 and self.info.get('c') != '11':  # (helicopter: texts sit on the flight path)
                # The in-game camera shows ~560 px of height with the ground in its lower part:
                # a caption far above whatever is below it is never seen.
                d = self.ground_below(x + 60, y, 2000, static_only=True)
                if d is None or d > 420:
                    cap = e.find('p7')
                    text = ((cap.text or '') if cap is not None else '').split('\n')[0][:30]
                    out.append('WARN text %d %r is %s px above the ground (likely off screen)' % (i, text, d))
        return out


def render(model, out_path, width=3000, crop=None, labels=True):
    if crop:
        x1, y1, x2, y2 = crop
    else:
        x1, y1, x2, y2 = model.bounds()
        pad = 150
        x1, y1, x2, y2 = x1 - pad, y1 - pad, x2 + pad, y2 + pad
    scale = width / float(x2 - x1)
    height = int((y2 - y1) * scale) + 1
    if height > 4000:
        scale = 4000 / float(y2 - y1)
        height = 4000
        width = int((x2 - x1) * scale) + 1
    bgc = int(fnum(model.info, 'bgc', 0xffffff))
    img = Image.new('RGB', (int(width), int(height)), color_of(bgc)[:3])
    draw = ImageDraw.Draw(img, 'RGBA')
    try:
        font = ImageFont.load_default(size=max(10, int(14)))
    except TypeError:
        font = ImageFont.load_default()

    def P(x, y):
        return ((x - x1) * scale, (y - y1) * scale)

    def poly(pts, fill, outline=None, w=1):
        if isinstance(pts, tuple) and pts and pts[0] == 'circle':
            _, cx, cy, r = pts
            a, b = P(cx - r, cy - r), P(cx + r, cy + r)
            draw.ellipse([a, b], fill=fill, outline=outline, width=w)
            return
        if len(pts) >= 2:
            draw.polygon([P(*p) for p in pts], fill=fill, outline=outline)

    # grid (every 1000 px)
    gx = int(math.floor(x1 / 1000.0)) * 1000
    while gx < x2:
        draw.line([P(gx, y1), P(gx, y2)], fill=(0, 0, 0, 30))
        if labels:
            draw.text(P(gx + 5, y1 + 5), str(gx), fill=(0, 0, 0, 120), font=font)
        gx += 1000
    gy = int(math.floor(y1 / 1000.0)) * 1000
    while gy < y2:
        draw.line([P(x1, gy), P(x2, gy)], fill=(0, 0, 0, 30))
        if labels:
            draw.text(P(x1 + 5, gy + 2), str(gy), fill=(0, 0, 0, 120), font=font)
        gy += 1000

    def draw_shape(e, g=None):
        if g is not None:
            geom = shape_outline(e, fnum(g, 'x'), fnum(g, 'y'), fnum(g, 'r'), fnum(g, 'ox'),
                                 fnum(g, 'oy'))
            gop = fnum(g, 'o', 100) / 100.0
        else:
            geom = shape_outline(e)
            gop = 1.0
        op = fnum(e, 'p10', 100) / 100.0 * gop
        fill = color_of(fnum(e, 'p8', 4032711), int(255 * op))
        ol = int(fnum(e, 'p9', -1))
        outline = color_of(ol, 255) if ol >= 0 else None
        inter = not (e.get('i') == 'f' or int(fnum(e, 't')) == 4)
        if inter and not fbool(e, 'p5') and g is None:
            outline = outline or (200, 0, 0, 255)       # dynamic shapes: red edge
        if g is not None and inter:
            outline = outline or (0, 0, 200, 255)       # group shapes: blue edge
        poly(geom, fill, outline)

    for e in model.shapes:
        draw_shape(e)
    for g in model.groups:
        if fbool(g, 'f'):
            continue
        for e in g.findall('sh'):
            draw_shape(e, g)

    # specials
    for i, e in enumerate(model.specials):
        t = int(fnum(e, 't', -1))
        geom, solid = special_footprint(e)
        x, y = fnum(e, 'p0'), fnum(e, 'p1')
        if t == 9:
            # checkered finish strip + pole
            for k in range(10):
                c = (0, 0, 0, 255) if k % 2 == 0 else (255, 255, 255, 255)
                poly(box_pts(x - 180 + k * 40, y, 40, 40), c)
            draw.line([P(x - 197, y), P(x - 197, y - 217)], fill=(0, 0, 0, 255), width=3)
            if labels:
                draw.text(P(x - 190, y - 230), 'FINISH sp%d' % i, fill=(0, 0, 0, 255), font=font)
            continue
        if t == 16:
            cap = e.find('p7')
            text = (cap.text or '') if cap is not None else ''
            size = max(10, min(100, fnum(e, 'p5', 15)))
            try:
                tf = ImageFont.load_default(size=max(8, int(size * scale)))
            except TypeError:
                tf = font
            draw.multiline_text(P(x + 2, y + 2), text.replace('\r', '\n'),
                                fill=color_of(fnum(e, 'p3', 0), 255), font=tf)
            continue
        if t == 7:
            r = max(200, min(1000, fnum(e, 'p2', 350)))
            draw.line([P(x, y), P(x, y + r)], fill=(60, 60, 60, 255), width=2)
        if t == 8:
            poly(box_pts(x, y, 300, 500, fnum(e, 'p2'), 0, -300), (120, 200, 255, 50))
        if t == 29:
            poly(box_pts(x, y, 800, 400, fnum(e, 'p2')), (255, 0, 0, 25))
        if t == 30:
            pass
        colr = (90, 90, 90, 200) if solid else (230, 120, 0, 170)
        if t in (13, 14):
            colr = (110, 110, 125, 255) if t == 13 else (140, 123, 107, 255)
        if t == 6:
            colr = (150, 150, 150, 255)
        if t == 17:
            colr = (240, 190, 150, 220)
        if t == 31:
            colr = (255, 215, 0, 230)
        if t == 27:
            colr = (154, 154, 154, 255)
        if isinstance(geom, list) and geom and isinstance(geom[0], tuple) and geom[0][0] == 'circle':
            poly(geom[0], colr, (0, 0, 0, 255))
        else:
            poly(geom, colr, (0, 0, 0, 255))
        if t == 6:
            # spikes: little teeth
            n = max(20, min(150, int(fnum(e, 'p4', 20))))
            rot = fnum(e, 'p2')
            for k in range(0, n, 2):
                lx = -n * 7.5 + k * 15 + 7.5
                a = rot_pt(lx - 7, 0, rot)
                b = rot_pt(lx + 7, 0, rot)
                c = rot_pt(lx, -25, rot)
                draw.polygon([P(x + a[0], y + a[1]), P(x + b[0], y + b[1]), P(x + c[0], y + c[1])],
                             fill=(220, 220, 220, 255), outline=(0, 0, 0, 255))
        if labels:
            draw.text(P(x, y), '%s%d' % (Model.special_name(e)[:8], i), fill=(120, 0, 120, 255),
                      font=font)

    for g in model.groups:
        if not fbool(g, 'f'):
            continue
        for e in g.findall('sh'):
            draw_shape(e, g)

    # joints
    for i, j in enumerate(model.joints):
        x, y = fnum(j, 'x'), fnum(j, 'y')
        a, b = P(x - 15, y - 15), P(x + 15, y + 15)
        if int(fnum(j, 't')) == 0:
            draw.ellipse([a, b], outline=(220, 0, 0, 255), width=2)
        else:
            draw.rectangle([a, b], outline=(0, 0, 220, 255), width=2)
            ang = fnum(j, 'a')
            dx, dy = math.cos(math.radians(ang)), math.sin(math.radians(ang))
            lo, hi = fnum(j, 'll'), fnum(j, 'ul')
            draw.line([P(x + dx * lo, y + dy * lo), P(x + dx * hi, y + dy * hi)], fill=(0, 0, 220, 200),
                      width=2)
        if labels:
            draw.text(P(x + 12, y - 12), 'J%d' % i, fill=(200, 0, 0, 255), font=font)

    # triggers
    for i, t in enumerate(model.triggers):
        x, y, w, h = fnum(t, 'x'), fnum(t, 'y'), fnum(t, 'w'), fnum(t, 'h')
        kind = int(fnum(t, 't', 1))
        col = (255, 200, 0, 255) if kind != 3 else (0, 200, 0, 255)
        poly(box_pts(x, y, w, h, fnum(t, 'a')), (255, 230, 0, 35) if kind != 3 else (0, 255, 0, 50), col)
        if labels:
            draw.text(P(x - w / 2 + 4, y - h / 2 + 2), 'T%d b%d%s' % (i, int(fnum(t, 'b')),
                                                                     ' WIN' if kind == 3 else ''),
                      fill=(150, 110, 0, 255), font=font)
        for c in t:
            try:
                pos = model.item_pos(c.tag, int(c.get('i')))
            except (TypeError, ValueError):
                pos = None
            if pos:
                draw.line([P(x, y), P(*pos)], fill=(200, 160, 0, 140), width=1)

    # start
    sx, sy = fnum(model.info, 'x'), fnum(model.info, 'y')
    poly(box_pts(sx, sy + 10, 180, 150), (255, 0, 255, 60), (255, 0, 255, 255))
    draw.ellipse([P(sx - 12, sy - 12), P(sx + 12, sy + 12)], fill=(255, 0, 255, 255))
    if labels:
        draw.text(P(sx - 40, sy - 110), 'START c%s' % model.info.get('c'), fill=(200, 0, 200, 255),
                  font=font)
    img.save(out_path)
    return out_path


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('level')
    ap.add_argument('-o', '--out')
    ap.add_argument('--width', type=int, default=3000)
    ap.add_argument('--crop', help='x1,y1,x2,y2 in Flash px')
    ap.add_argument('--no-labels', action='store_true')
    ap.add_argument('--check-only', action='store_true')
    args = ap.parse_args(argv)
    model = Model(args.level)
    for line in model.checks():
        print(line)
    if args.check_only:
        return 0
    crop = tuple(float(v) for v in args.crop.split(',')) if args.crop else None
    out = args.out or args.level.rsplit('.', 1)[0] + '.png'
    render(model, out, args.width, crop, not args.no_labels)
    print('wrote', out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
