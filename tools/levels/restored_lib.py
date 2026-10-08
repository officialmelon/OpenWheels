"""Small builder for browser (Flash 1.87) Happy Wheels level XML.

Used by make_restored_levels.py (OpenWheels original campaign levels for the restored browser
characters). Stdlib only. Coordinates are Flash pixels, y down, inside the 20000 x 10000 stage,
62.5 px per metre, rotations in degrees clockwise -- exactly what src/online/FlashLevelConverter.cpp
and src/editor/flash/FlashLevelIO.cpp read.

Parameter meaning per element (see docs/FLASH_LEVELS.md section 5 and FlashCatalog.cpp):
  <sh t p0 x p1 y p2 w p3 h p4 rot p5 fixed p6 sleeping p7 density p8 fill p9 outline p10 opacity
      p11 collision [p12 cutout]>  (+ <v id f n v0..> for polygon / art, vertices local to p0/p1)
  <sp t p0 x p1 y p2..>  special-specific (helpers below document each one)
  <g x y r ox oy s f o im fr>  shapes/specials inside use local coords; world = (x,y) + (p + o)
  <j t x y b1 b2 ...>  b = shape index, "s<i>" special, "g<i>" group, "-1" level
  <t x y w h a b t r sd [i] [d]>  targets <sh|sp|g|j|t i=..><a i=action p0..></a></..>
"""

import math
import os
from xml.sax.saxutils import quoteattr

STAGE_W = 20000
STAGE_H = 10000

# ---------------------------------------------------------------------------------------------
# Colours


def rgb(h):
    return int(h.lstrip('#'), 16)


class Ref:
    """A handle to a level item (index in its list) usable by joints and triggers."""

    def __init__(self, kind, index, level=None, info=None):
        self.kind = kind      # 'sh', 'sp', 'g', 'j', 't'
        self.index = index
        self.level = level
        self.info = info or {}

    def body(self):
        if self.kind == 'sh':
            return str(self.index)
        if self.kind == 'sp':
            return 's%d' % self.index
        if self.kind == 'g':
            return 'g%d' % self.index
        raise ValueError('not a body: %s' % self.kind)

    def __repr__(self):
        return '%s%d' % (self.kind, self.index)


LEVEL = 'level'  # joint end: the static level body


def fmt(v):
    if isinstance(v, bool):
        return 't' if v else 'f'
    if isinstance(v, float):
        if abs(v - round(v)) < 1e-9:
            return str(int(round(v)))
        return ('%.3f' % v).rstrip('0').rstrip('.')
    return str(v)


def attrs(pairs):
    return ''.join(' %s=%s' % (k, quoteattr(fmt(v))) for k, v in pairs)


def convex(points):
    """True if the closed polygon is strictly convex (either winding)."""
    n = len(points)
    if n < 3:
        return False
    sign = 0
    for i in range(n):
        ax, ay = points[i]
        bx, by = points[(i + 1) % n]
        cx, cy = points[(i + 2) % n]
        cross = (bx - ax) * (cy - by) - (by - ay) * (cx - bx)
        if abs(cross) < 1e-9:
            continue
        s = 1 if cross > 0 else -1
        if sign == 0:
            sign = s
        elif s != sign:
            return False
    return sign != 0


class _ShapeOwner:
    """Shape/special factory shared by the level and its groups."""

    def _shape_xml(self, t, x, y, w, h, rot, fixed, sleep, density, color, outline, opacity,
                   collision, inter, verts=None, cutout=None):
        pairs = [('t', t)]
        if not inter:
            pairs.append(('i', 'f'))
        pairs += [('p0', x), ('p1', y), ('p2', w), ('p3', h), ('p4', rot), ('p5', bool(fixed)),
                  ('p6', bool(sleep)), ('p7', density), ('p8', color), ('p9', outline),
                  ('p10', opacity), ('p11', collision)]
        if t == 1:
            pairs.append(('p12', cutout or 0))
        s = '<sh%s' % attrs(pairs)
        if verts is None:
            return s + '/>', None
        vid = self.level._next_vid()
        vp = [('f', True), ('id', vid), ('n', len(verts))]
        for i, (vx, vy) in enumerate(verts):
            vp.append(('v%d' % i, '%s_%s' % (fmt(float(vx)), fmt(float(vy)))))
        return s + '><v%s/></sh>' % attrs(vp), None

    # shapes -------------------------------------------------------------------------------
    def rect(self, x, y, w, h, color=0x7f7f7f, rot=0, fixed=True, sleep=False, density=1.0,
             outline=-1, opacity=100, collision=1, inter=True):
        """Rectangle centred on x, y."""
        xml, _ = self._shape_xml(0, x, y, w, h, rot, fixed, sleep, density, color, outline,
                                 opacity, collision, inter)
        return self._add_shape(xml, dict(t=0, x=x, y=y, w=w, h=h, rot=rot, fixed=fixed,
                                         inter=inter, color=color, opacity=opacity,
                                         collision=collision))

    def box(self, x1, y1, x2, y2, **kw):
        """Rectangle from its corners (x1 < x2, y1 < y2)."""
        return self.rect((x1 + x2) / 2.0, (y1 + y2) / 2.0, abs(x2 - x1), abs(y2 - y1), **kw)

    def circle(self, x, y, d, color=0x7f7f7f, fixed=False, sleep=False, density=1.0, outline=-1,
               opacity=100, collision=1, inter=True, cutout=0):
        xml, _ = self._shape_xml(1, x, y, d, d, 0, fixed, sleep, density, color, outline, opacity,
                                 collision, inter, cutout=cutout)
        return self._add_shape(xml, dict(t=1, x=x, y=y, w=d, h=d, rot=0, fixed=fixed,
                                         inter=inter, color=color, opacity=opacity,
                                         collision=collision))

    def tri(self, x, y, w, h, color=0x7f7f7f, rot=0, fixed=True, sleep=False, density=1.0,
            outline=-1, opacity=100, collision=1, inter=True):
        """Isosceles triangle, apex up, x/y = centroid (base at y + h/3)."""
        xml, _ = self._shape_xml(2, x, y, w, h, rot, fixed, sleep, density, color, outline,
                                 opacity, collision, inter)
        return self._add_shape(xml, dict(t=2, x=x, y=y, w=w, h=h, rot=rot, fixed=fixed,
                                         inter=inter, color=color, opacity=opacity,
                                         collision=collision))

    def poly(self, pts, color=0x7f7f7f, fixed=True, sleep=False, density=1.0, outline=-1,
             opacity=100, collision=1, inter=True, origin=None):
        """Polygon from world points. Physical polygons must be convex with <= 8 vertices."""
        pts = [(int(round(px)), int(round(py))) for px, py in pts]
        if origin is None:
            ox = int(round(sum(p[0] for p in pts) / len(pts)))
            oy = int(round(sum(p[1] for p in pts) / len(pts)))
        else:
            ox, oy = origin
        local = [(px - ox, py - oy) for px, py in pts]
        t = 3 if inter else 4
        if inter:
            assert len(local) <= 8 and convex(local), 'physical polygon must be convex <= 8: %r' % pts
        w = max(0, max(p[0] for p in local)) - min(0, min(p[0] for p in local))
        h = max(0, max(p[1] for p in local)) - min(0, min(p[1] for p in local))
        xml, _ = self._shape_xml(t, ox, oy, w, h, 0, fixed, sleep, density, color, outline,
                                 opacity, collision, inter, verts=local)
        return self._add_shape(xml, dict(t=t, x=ox, y=oy, pts=pts, fixed=fixed, inter=inter,
                                         color=color, opacity=opacity, collision=collision))

    def art(self, pts, color=0x7f7f7f, outline=-1, opacity=100):
        """Decoration polygon (no physics), any outline up to 100 vertices."""
        return self.poly(pts, color=color, outline=outline, opacity=opacity, inter=False)

    # specials -----------------------------------------------------------------------------
    def sp(self, t, x, y, *params, caption=None, size=None):
        """Generic special: params are p2, p3, ... in order."""
        pairs = [('t', t), ('p0', x), ('p1', y)]
        for i, v in enumerate(params):
            pairs.append(('p%d' % (i + 2), v))
        if caption is not None:
            # TextBox caption is a CDATA child <p7> (FlashSpecialRef::writeFlash).
            body = '<p7><![CDATA[%s]]></p7>' % caption.replace(']]>', ']] >')
            xml = '<sp%s>%s</sp>' % (attrs(pairs), body)
        else:
            xml = '<sp%s/>' % attrs(pairs)
        info = dict(t=t, x=x, y=y, params=list(params), caption=caption)
        return self._add_special(xml, info)

    def text(self, x, y, caption, size=20, color=0x000000, font=2, align=1, opacity=100, rot=0):
        """Text box, x/y = top-left. font 1..5 (Helvetica, medium, bold, Clarendon, Clarendon bold)."""
        return self.sp(16, x, y, rot, color, font, size, align, opacity, caption=caption)

    def npc(self, x, ground, char=1, sleep=True, reverse=False, hold=False, inter=True,
            pose=(0, 0, 0, 0, 0, 0, 0, 0, 0), destroy=False, angle=0, y=None):
        """Non-player character standing on `ground` (chest sits ~100 px above the feet)."""
        cy = ground - 102 if y is None else y
        return self.sp(17, x, cy, angle, char, sleep, reverse, hold, inter, *pose, destroy)

    def spikes(self, x, y, count=20, rot=0, fixed=True, sleep=False):
        """Spike strip centred on x; y = centre of the spike row (~25 px above its base)."""
        return self.sp(6, x, y, rot, fixed, count, sleep)

    def spikes_on(self, x, ground, count=20, fixed=True):
        return self.spikes(x, ground - 20, count, 0, fixed)

    def finish(self, x, ground):
        """Finish line: a solid 400 x 40 px strip centred on p1 (victory when the rider is above
        it, up to 200 px). Sunk so its top is flush with the ground: a strip standing on the
        ground is a 40 px kerb that wrecks carts and bikes."""
        return self.sp(9, x, ground + 18)

    def mine(self, x, ground, rot=0):
        return self.sp(2, x, ground - 6, rot)

    def homing_mine(self, x, y, speed=2, delay=1):
        return self.sp(25, x, y, speed, delay)

    def ibeam(self, x, y, w=300, h=30, rot=0, fixed=False, sleep=False):
        return self.sp(3, x, y, w, h, rot, fixed, sleep)

    def log(self, x, y, w=200, h=40, rot=0, fixed=False, sleep=False):
        return self.sp(4, x, y, w, h, rot, fixed, sleep)

    def spring_box(self, x, ground, rot=0, delay=0):
        return self.sp(5, x, ground - 18, rot, delay)

    def wrecking_ball(self, x, y, rope=350):
        """Anchor at x/y, the ball hangs `rope` px below."""
        return self.sp(7, x, y, rope)

    def fan(self, x, ground, rot=0):
        """Fan base on the ground blowing along -y (rotated by rot)."""
        return self.sp(8, x, ground - 25, rot)

    def soccer(self, x, y):
        return self.sp(10, x, y)

    def meteor(self, x, y, d=300, fixed=False, sleep=True):
        return self.sp(11, x, y, d, d, fixed, sleep)

    def boost(self, x, ground, panels=2, power=30, rot=0):
        return self.sp(12, x, ground - 4, rot, panels, power)

    def building(self, x, top, floors=3, width=1, kind=13):
        """Building (static): x/top = top-left; type 13 w = width*300, h = floors*165+100."""
        return self.sp(kind, x, top, width, floors)

    def harpoon(self, x, y, rot=0, anchor=True, fixed_turret=False, turret=0, trig=False, off=False):
        return self.sp(15, x, y, rot, anchor, fixed_turret, turret, trig, off)

    def glass(self, x, y, w=10, h=200, rot=0, sleep=False, strength=5, stab=True):
        return self.sp(18, x, y, w, h, rot, sleep, strength, stab)

    def chair(self, x, ground, rot=0, reverse=False, sleep=False, inter=True):
        return self.sp(19, x, ground - 32, rot, reverse, sleep, inter)

    def table(self, x, ground, rot=0, sleep=False, inter=True):
        return self.sp(1, x, ground - 50, rot, sleep, inter)

    def bottle(self, x, y, color=1, rot=0, sleep=False, inter=True):
        return self.sp(20, x, y, rot, color, sleep, inter)

    def tv(self, x, y, rot=0, sleep=False, inter=True):
        return self.sp(21, x, y, rot, sleep, inter)

    def boombox(self, x, y, rot=0, sleep=False, inter=True):
        return self.sp(22, x, y, rot, sleep, inter)

    def sign(self, x, ground, kind=1, post=True, rot=0):
        return self.sp(23, x, ground, rot, kind, post)

    def toilet(self, x, ground, rot=0, reverse=False, sleep=False, inter=True):
        return self.sp(24, x, ground - 40, rot, reverse, sleep, inter)

    def trash(self, x, ground, rot=0, sleep=False, inter=True, trash=True):
        return self.sp(26, x, ground - 33, rot, sleep, inter, trash)

    def rail(self, x, y, w=500, rot=0):
        """Grind rail (static, 18 px thick) centred on x/y; Explorer Guy clamps onto it."""
        return self.sp(27, x, y, w, 18, rot)

    def jet(self, x, y, rot=0, sleep=False, power=3, fire_time=0, accel=1, fixed_rot=False):
        return self.sp(28, x, y, rot, sleep, power, fire_time, accel, fixed_rot)

    def arrow_gun(self, x, y, rot=0, fixed=True, rate=3, dont_shoot_player=False):
        return self.sp(29, x, y, rot, fixed, rate, dont_shoot_player)

    def chain(self, x, y, links=20, rot=0, sleep=False, inter=True, scale=1, curve=0):
        return self.sp(30, x, y, rot, sleep, inter, links, scale, curve)

    def token(self, x, y, kind=1):
        return self.sp(31, x, y, kind)

    def food(self, x, y, kind=1, rot=0, sleep=False, inter=True):
        return self.sp(32, x, y, rot, sleep, inter, kind)

    def cannon(self, x, ground, rot=0, start=0, fire=0, kind=1, delay=2, muzzle=1, power=5):
        return self.sp(33, x, ground - 55, rot, start, fire, kind, delay, muzzle, power)

    def blade(self, x, y, kind=1, rot=0, flipped=False, sleep=False, inter=True):
        return self.sp(34, x, y, rot, flipped, sleep, inter, kind)

    def paddle(self, x, ground, rot=0, delay=0.5, reverse=False, angle=60, speed=6):
        return self.sp(35, x, ground - 20, rot, delay, reverse, angle, speed)


class Group(_ShapeOwner):
    def __init__(self, level, index, sleep=False, fore=False, opacity=100, im=False, fr=False):
        self.level = level
        self.index = index
        self.sleep = sleep
        self.fore = fore
        self.opacity = opacity
        self.im = im
        self.fr = fr
        self.shapes = []      # (xml, info)
        self.specials = []
        self.ref = Ref('g', index, level)
        self.kind = 'g'

    def body(self):
        return 'g%d' % self.index

    def _add_shape(self, xml, info):
        self.shapes.append((xml, info))
        return self.ref

    def _add_special(self, xml, info):
        self.specials.append((xml, info))
        return self.ref

    def center(self):
        pts = []
        for _, s in self.shapes:
            if 'pts' in s:
                pts += s['pts']
            else:
                pts.append((s['x'], s['y']))
        for _, s in self.specials:
            pts.append((s['x'], s['y']))
        cx = sum(p[0] for p in pts) / len(pts)
        cy = sum(p[1] for p in pts) / len(pts)
        return int(round(cx)), int(round(cy))

    def xml(self):
        cx, cy = self.center()
        pairs = [('x', cx), ('y', cy), ('r', 0), ('ox', -cx), ('oy', -cy), ('s', self.sleep),
                 ('f', self.fore), ('o', self.opacity), ('im', self.im), ('fr', self.fr)]
        body = ''.join(x for x, _ in self.shapes) + ''.join(x for x, _ in self.specials)
        return '<g%s>%s</g>' % (attrs(pairs), body)


class Level(_ShapeOwner):
    def __init__(self, path, title, char, start, bg=1, bgc=0xffffff, hide=False):
        self.level = self
        self.path = path
        self.title = title
        self.char = char
        self.start = start
        self.bg = bg
        self.bgc = bgc
        self.hide = hide
        self.shapes = []
        self.specials = []
        self.groups = []
        self.joints = []
        self.triggers = []
        self._vid = 0

    def _next_vid(self):
        self._vid += 1
        return self._vid

    def _add_shape(self, xml, info):
        self.shapes.append((xml, info))
        return Ref('sh', len(self.shapes) - 1, self, info)

    def _add_special(self, xml, info):
        self.specials.append((xml, info))
        return Ref('sp', len(self.specials) - 1, self, info)

    # terrain --------------------------------------------------------------------------------
    def ground(self, profile, bottom=None, color=0x6a8f3a, outline=-1, opacity=100):
        """Static terrain from a surface profile [(x, y), ...], built from rectangles only (they
        draw everywhere): flat runs are boxes; a slope is a rotated slab whose top edge follows
        the segment over a box filling down to `bottom`."""
        if bottom is None:
            bottom = max(p[1] for p in profile) + 600
        refs = []
        for (x1, y1), (x2, y2) in zip(profile, profile[1:]):
            if x2 <= x1:
                raise ValueError('profile must go right: %r' % ((x1, y1), (x2, y2)))
            if y1 == y2:
                refs.append(self.box(x1, y1, x2, bottom, color=color, outline=outline,
                                     opacity=opacity))
            else:
                low = max(y1, y2)
                if bottom - low > 2:
                    refs.append(self.box(x1, low, x2, bottom, color=color, outline=outline,
                                         opacity=opacity))
                # perpendicular thickness that still covers the wedge above the fill box
                cosa = (x2 - x1) / math.hypot(x2 - x1, y2 - y1)
                refs.append(self.slab(x1, y1, x2, y2, thick=round(abs(y2 - y1) * cosa + 24, 1),
                                      color=color, outline=outline, opacity=opacity, extend=3))
        return refs

    def slab(self, x1, y1, x2, y2, thick=40, extend=0, **kw):
        """A rotated rectangle whose top edge runs from (x1, y1) to (x2, y2)."""
        dx, dy = x2 - x1, y2 - y1
        length = math.hypot(dx, dy) + 2 * extend
        ang = math.degrees(math.atan2(dy, dx))
        nx, ny = -dy / math.hypot(dx, dy), dx / math.hypot(dx, dy)   # downward normal (y down)
        cx = (x1 + x2) / 2.0 + nx * thick / 2.0
        cy = (y1 + y2) / 2.0 + ny * thick / 2.0
        return self.rect(round(cx, 2), round(cy, 2), round(length, 2), thick, rot=round(ang, 3), **kw)

    def roof(self, xc, base_y, w, h, **kw):
        """Isosceles triangle with its base centred at (xc, base_y), apex h above (art by default)."""
        kw.setdefault('inter', False)
        return self.tri(xc, base_y - h / 3.0, w, h, **kw)

    def group(self, **kw):
        g = Group(self, len(self.groups), **kw)
        self.groups.append(g)
        return g

    # joints ---------------------------------------------------------------------------------
    def _bref(self, r):
        return '-1' if r is LEVEL or r is None else r.body()

    def pin(self, x, y, b1, b2=LEVEL, motor=False, torque=0, speed=0, limit=False, upper=0,
            lower=0, collide=False):
        pairs = [('t', 0), ('x', x), ('y', y), ('b1', self._bref(b1)), ('b2', self._bref(b2)),
                 ('l', limit), ('ua', upper), ('la', lower), ('m', motor), ('tq', torque),
                 ('sp', speed), ('c', collide)]
        self.joints.append(('<j%s/>' % attrs(pairs), dict(t=0, x=x, y=y, b1=b1, b2=b2)))
        return Ref('j', len(self.joints) - 1, self)

    def slider(self, x, y, b1, b2=LEVEL, axis=90, upper=0, lower=0, limit=True, motor=False,
               force=0, speed=0, collide=False):
        """Prismatic joint; axis in degrees (0 = along +x, 90 = along +y / down)."""
        pairs = [('t', 1), ('x', x), ('y', y), ('b1', self._bref(b1)), ('b2', self._bref(b2)),
                 ('a', axis), ('l', limit), ('ul', upper), ('ll', lower), ('m', motor),
                 ('fo', force), ('sp', speed), ('c', collide)]
        self.joints.append(('<j%s/>' % attrs(pairs), dict(t=1, x=x, y=y, b1=b1, b2=b2,
                                                           axis=axis, upper=upper, lower=lower)))
        return Ref('j', len(self.joints) - 1, self)

    # triggers -------------------------------------------------------------------------------
    def trigger(self, x, y, w, h, targets=(), by=1, kind=1, repeat=1, delay=0, interval=None,
                disabled=False, rot=0):
        """kind 1 activate targets, 3 victory. by: 1 player, 2 any character, 3 any non-fixed
        shape, 4 its targets, 5 other triggers. repeat: 1 once, 2 each touch, 3 while touched,
        4 continuously once triggered.
        targets: [(ref, [(action, p0, p1, ...), ...]), ...]; an empty action list activates a
        special that has none (mine, wrecking ball, fan, boost, homing mine)."""
        pairs = [('x', x), ('y', y), ('w', w), ('h', h), ('a', rot), ('b', by), ('t', kind),
                 ('r', repeat), ('sd', disabled)]
        if repeat > 2:
            pairs.append(('i', interval if interval is not None else 1))
        if kind == 1:
            pairs.append(('d', delay))
        body = ''
        for ref, actions in targets:
            inner = ''
            for act in actions:
                ap = [('i', act[0])] + [('p%d' % k, v) for k, v in enumerate(act[1:])]
                inner += '<a%s/>' % attrs(ap)
            body += '<%s i="%d">%s</%s>' % (ref.kind, ref.index, inner, ref.kind)
        self.triggers.append(('<t%s>%s</t>' % (attrs(pairs), body) if body else
                              '<t%s/>' % attrs(pairs),
                              dict(x=x, y=y, w=w, h=h, by=by, kind=kind, targets=list(targets))))
        return Ref('t', len(self.triggers) - 1, self)

    def victory(self, x, y, w, h, by=1):
        return self.trigger(x, y, w, h, by=by, kind=3)

    # output ---------------------------------------------------------------------------------
    def xml(self):
        sx, sy = self.start
        info = [('v', '1.87'), ('x', sx), ('y', sy), ('c', self.char), ('f', True),
                ('h', self.hide), ('bg', self.bg), ('bgc', self.bgc), ('e', 1)]
        out = ['<levelXML><info%s/>' % attrs(info)]
        if self.shapes:
            out.append('<shapes>' + ''.join(x for x, _ in self.shapes) + '</shapes>')
        if self.specials:
            out.append('<specials>' + ''.join(x for x, _ in self.specials) + '</specials>')
        if self.groups:
            out.append('<groups>' + ''.join(g.xml() for g in self.groups) + '</groups>')
        if self.joints:
            out.append('<joints>' + ''.join(x for x, _ in self.joints) + '</joints>')
        if self.triggers:
            out.append('<triggers>' + ''.join(x for x, _ in self.triggers) + '</triggers>')
        out.append('</levelXML>')
        return '\n'.join(out) + '\n'

    def check(self):
        """Cheap self-checks: everything on stage, references in range, a way to win."""
        problems = []

        def on_stage(x, y, what):
            if not (0 <= x <= STAGE_W and 0 <= y <= STAGE_H):
                problems.append('%s off stage at %r' % (what, (x, y)))

        on_stage(self.start[0], self.start[1], 'start')
        for _, s in self.shapes:
            for p in s.get('pts', [(s['x'], s['y'])]):
                on_stage(p[0], p[1], 'shape')
        for _, s in self.specials:
            on_stage(s['x'], s['y'], 'special %d' % s['t'])
        has_finish = any(s['t'] == 9 for _, s in self.specials)
        has_victory = any(t['kind'] == 3 for _, t in self.triggers)
        if not (has_finish or has_victory):
            problems.append('no finish line or victory trigger')
        counts = dict(sh=len(self.shapes), sp=len(self.specials), g=len(self.groups),
                      j=len(self.joints), t=len(self.triggers))
        for _, t in self.triggers:
            for ref, _a in t['targets']:
                if not (0 <= ref.index < counts[ref.kind]):
                    problems.append('trigger target %r out of range' % ref)
        return problems

    def write(self, root):
        path = os.path.join(root, self.path)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(self.xml())
        return path
