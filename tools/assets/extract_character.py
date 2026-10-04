#!/usr/bin/env python3
"""Rebuilds the browser game's player characters that the mobile port left out, as mobile assets.

The Android 1.1.3 port ships six of the browser game's eleven player characters. The browser game
(Flash v1.87) loads each player character from its own SWF, characters/character<N>.swf (see
SessionContentLoader.loadCharacter), which holds the body-part clips, the vehicle art and the
"shapeGuide" clip the Flash code builds the Box2D bodies from. This script turns one of those SWFs
into the files the mobile engine reads, in the mobile port's own conventions:

    <out>/shared/characters/bodies/<name>_<vehicle>.plist   character bodies (mobile metres)
    <out>/shared/vehicles/bodies/<vehicle>.plist            vehicle shapes and anchors
    <out>/shared/Characters_restored.plist                  character-select entries
    <out>/<tier>/characters/<name>_sprites.{plist,png}            gameplay sprites
    <out>/<tier>/characters/character_select_<name>_sprites.{...} character-select preview sprites
    <out>/<tier>/vehicles/<vehicle>_sprites.{plist,png}  (+ character_select_ variant)
    <out>/<tier>/menus/character_select/restored_icons.{plist,png}  <key>_icon(_bw).png buttons
    <out>/sounds/<Sound>.ogg                                sounds the mobile build lacks

Conventions (validated with --calibrate, which runs the same conversion on Irresponsible Dad's
character3.swf and compares the result with his Android files):
  * physics: the mobile engine uses the browser game's metres (Flash Session.m_physScale 62.5
    world px per metre, character_scale = 62.5 x mc_scale 2 = 125 symbol px per metre), y up
    instead of down, angles negated; box guides are square symbols (10 px, or 100 px with
    shapeRefScale 50) scaled by scaleX/scaleY, so half extents = scale * side / 2 / 125; polygon
    vertex lists are reversed (the y flip reverses the winding). The Android dad / moped guy plists
    match this within rounding except two hand-narrowed torso widths (chest 0.835, pelvis 0.90).
  * sprites: large = 2 tier px per symbol px (medium 1, small 0.75, tiny 0.5); character-select
    sheets are twice that. Body-part frames are canvases centred on the clip's registration point
    (= the Box2D body origin the engine paints at), stored trimmed with spriteOffset like
    TexturePacker; overlays (neck, shoulder_wound, hipWound, pelvisWound) share the parent's
    canvas (they are added as children with anchor (0,0)); chunks, organs, foot, intestine and
    spine are trimmed with no offset. Flash clip frame k = mobile state _k (upperArm1MC
    .gotoAndStop(7) <-> <name>_upperArm1_7.png, upperArm3MC.gotoAndStop(2) <-> _upperArm1_2 ...).

Needs Java 11+, FFDec (ffdec_lib.jar), numpy and Pillow; ffmpeg for the sounds (skipped without
it). With --optional, missing inputs only print a note and exit 0 (build hooks). Outputs are
skipped when an up-to-date stamp exists.
"""
import argparse
import hashlib
import math
import os
import plistlib
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import extract_kid_gore as kg  # noqa: E402  (shared FFDec helper + image utilities)

np = kg.np
Image = kg.Image

REPO = kg.REPO
VERSION = '1'
TIERS = {'large': 2.0, 'medium': 1.0, 'small': 0.75, 'tiny': 0.5}  # tier px per symbol px
MASTER = 4.0          # render zoom (= character-select large)
ROT_SIGN = 1.0        # kg.warp angle sign for a Flash (clockwise, y down) rotation
PHYS = 1.0 / 125.0    # metres per symbol px (Flash character_scale 125)
DEFAULT_SWF_DIR = os.path.join(REPO, 'binary', 'flash', 'swf', 'characters')
DEFAULT_SOUNDS_SWF = os.path.join(REPO, 'binary', 'flash', 'swf', 'happy_sounds_v1_72.swf')

# ------------------------------------------------------------------------------------------
# Per-character data. Everything not listed here is read from the SWF.
# ------------------------------------------------------------------------------------------

SPECS = {
    6: dict(
        name='lawnmower_man', vehicle='lawnmower', key='mow', vehicle_key='lm',
        title='Lawnmower Man', cls='LawnMowerMan', vehicle_cls='LawnMower', controls=106,
        special_on_left='1',
        # Vehicle frames: (frame name, root clip, mode). 'inner@<guide point>': the clip's
        # "inner" child drawn with its origin on that shapeGuide point (Flash createMovieClips
        # moves mowerMC.inner there); the canvas is centred on the guide origin = the body origin
        # the mobile vehicle uses. 'static'/'spin': the clip without / only its "inner" child.
        # 'centroid': canvas centred on the art's alpha centroid (pieces painted at their mass
        # centre). Frames 'frames:<n>' expand a multi-frame clip to <name>_<k>.
        vehicle_parts=[
            ('mower', 'mower', 'inner@rearVert5'),
            ('bladeCover', 'bladecover', 'inner@rearVert5'),
            # Tyre, spokes ("inner", spun by the Flash paint()) and hub: drawn as one sprite
            # turning with the wheel body.
            ('backWheel', 'backWheel', 'clip'),
            ('frontWheel', 'frontWheel', 'clip'),
            ('mowerFront', 'mowerFront', 'clip'),
            ('mowerRear', 'mowerRear', 'clip'),
            ('front1', 'front1', 'clip'), ('front2', 'front2', 'clip'), ('front3', 'front3', 'clip'),
            ('front4', 'front4', 'clip'), ('front5', 'front5', 'clip'),
            ('rear1', 'rear1', 'clip'), ('rear2', 'rear2', 'clip'), ('rear3', 'rear3', 'clip'),
            ('rear4', 'rear4', 'clip'),
            ('shard', 'mowerShards', 'trimmed-frames'),
        ],
        select_vehicle_parts=['mower', 'bladeCover', 'backWheel', 'frontWheel'],
        sounds=['MowerLoop', 'GrindLoop2'],
        # Character-select offset (vehicles[0].offset, mobile metres): the vehicle centred and its
        # wheels on the platform, like the original entries.
        select_offset=None,
    ),
    10: dict(
        name='irresponsible_mom', vehicle='mom_bike', key='mom', vehicle_key='imb',
        title='Irresponsible Mom', cls='IrresponsibleMom', vehicle_cls='MomBike', controls=110,
        special_on_left='0',
        # Riders in the same SWF: the daughter on the trailer bike, the son in the basket (Flash
        # IMDaughter / IMSon). Their own shapeGuides add the trailer bike and the basket to the
        # vehicle plist under "daughter_" / "son_".
        riders=[('daughter', 'irresponsible_mom_daughter'), ('son', 'irresponsible_mom_son')],
        vehicle_guides=[('daughter', 'daughter_'), ('son', 'son_')],
        vehicle_parts=[
            # Flash BicycleGuy: frame, gear and the smashed pieces are painted at their body's
            # mass centre ('clip'); the wheels' tyre + spokes turn as one sprite.
            ('frame', 'frame', 'clip'),
            ('gear', 'gear', 'clip'),
            ('frontWheel', 'frontWheel', 'wheel'),
            ('backWheel', 'backWheel', 'wheel'),
            ('frontWheelBroken', 'frontWheel', 'wheel-broken'),
            ('backWheelBroken', 'backWheel', 'wheel-broken'),
            ('fork', 'fork', 'clip'),
            ('brokenFrame', 'brokenFrame', 'clip'),
            ('seat', 'seat', 'clip'),
            # IMDaughter: frameMC.inner sits on frameVert1; the rest at the mass centre.
            ('daughterFrame', 'daughter/frame', 'inner@daughter:frameVert1'),
            ('daughterWheel', 'daughter/wheel', 'clip'),
            ('daughterGear', 'daughter/gear', 'clip'),
            ('daughterFork', 'daughter/fork', 'clip'),
            ('daughterBrokenFrame', 'daughter/brokenFrame', 'clip'),
            ('daughterSeat', 'daughter/seat', 'clip'),
            # IMSon: basketMC.inner sits on crateLeft1 (its "frame" child is hidden on reset).
            ('basket', 'son/basket', 'inner@son:crateLeft1', ('frame',)),
            ('basketPiece', 'son/basketPieces', 'trimmed-frames'),
        ],
        select_vehicle_parts=['frame', 'gear', 'frontWheel', 'backWheel', 'daughterFrame',
                              'daughterWheel', 'daughterGear', 'basket'],
        sounds=['Kid2Elbow1', 'Kid2Elbow2', 'Kid2Foot1', 'Kid2Foot2', 'Kid2Hip1', 'Kid2Hip2',
                'Kid2Knee1', 'Kid2Knee2', 'Kid2Pelvis', 'Kid2Shoulder1', 'Kid2Shoulder2',
                'Kid2Spikes', 'Kid2Torso'],
        select_offset=None,
    ),
    7: dict(
        name='explorer_guy', vehicle='mine_cart', key='exp', vehicle_key='emc',
        title='Explorer Guy', cls='ExplorerGuy', vehicle_cls='MineCart', controls=107,
        special_on_left='0',
        # Flash MiddleAgedExplorer: the cart and its pieces are painted at their body's mass
        # centre ('clip'); the wheels' tyre, spokes and hub turn as one sprite.
        vehicle_parts=[
            ('frame', 'frame', 'clip'),
            ('wheel', 'frontWheel', 'clip'),
            ('cartSmashed', 'cartSmashed', 'clip'),
            ('frameLeftSmashed', 'frameLeftSmashed', 'clip'),
            ('frameRightSmashed', 'frameRightSmashed', 'clip'),
            ('frameBottomSmashed', 'frameBottomSmashed', 'clip'),
            ('engineSmashed', 'engineSmashed', 'clip'),
            ('cartShard', 'cartShards', 'trimmed-frames'),
        ],
        select_vehicle_parts=['frame', 'wheel'],
        sounds=['ExplorerRoll1', 'ExplorerRoll2', 'ExplorerRoll3'],
        select_offset=None,
    ),
    8: dict(
        name='santa_claus', vehicle='sleigh', key='santa', vehicle_key='ssl',
        title='Santa Claus', cls='SantaClaus', vehicle_cls='Sleigh', controls=108,
        special_on_left='0',
        # The two elves pulling the sleigh share one look (elf2 is elf1 110 px further on):
        # one rider sheet and body plist, elf2 placed at that offset by the game.
        riders=[('elf1', 'santa_elf')],
        vehicle_guides=[('elf1', 'elf_')],
        vehicle_parts=[
            # Flash SantaClaus: sleighMC.inner sits on rear_1; its "stem" (the shafts) and "ski"
            # children disappear with the stem and ski smashes, so they are frames of their own.
            ('sleigh', 'sleigh', 'inner@rear_1', ('stem', 'ski')),
            ('sleighStem', 'sleigh', 'inner@rear_1', None, ('stem',)),
            ('sleighSki', 'sleigh', 'inner@rear_1', None, ('ski',)),
            ('stem', 'stem', 'clip'),
        ] + [('sleigh%d' % i, 'sleigh%d' % i, 'clip') for i in range(1, 8)]
          + [('ski%d' % i, 'ski%d' % i, 'clip') for i in range(1, 4)]
          + [('box%d' % i, 'box%d' % i, 'clip') for i in range(1, 9)] + [
            ('sleighShard', 'sleighShards', 'trimmed-frames'),
            ('snowflake', 'snowflakes', 'trimmed-frames'),
            ('bell', 'game:ChristmasBellMC', 'clip'),
        ],
        select_vehicle_parts=['sleigh', 'sleighStem', 'sleighSki', 'bell'] + ['box%d' % i for i in range(1, 9)],
        sounds=['SleighBellLoop', 'SkiLoop', 'Step5', 'Step6', 'Step7', 'Step8', 'Step9', 'Step10']
               + ['Santa' + n for n in ('Elbow1', 'Elbow2', 'Foot1', 'Foot2', 'Hip1', 'Hip2', 'Knee1',
                                        'Knee2', 'Pelvis', 'Shoulder1', 'Shoulder2', 'Spikes', 'Torso',
                                        'Mourn1', 'Mourn2')]
               + ['Elf1' + n for n in ('Elbow1', 'Elbow2', 'Foot1', 'Foot2', 'Hip1', 'Hip2', 'Knee1',
                                       'Knee2', 'Pelvis', 'Shoulder1', 'Shoulder2', 'Spikes', 'Torso')],
        select_offset=None,
    ),
    11: dict(
        name='helicopter_man', vehicle='helicopter', key='heli', vehicle_key='hhc',
        title='Helicopter Man', cls='HelicopterMan', vehicle_cls='Helicopter', controls=111,
        special_on_left='0',
        # Flash HelicopterMan: copterMC / copterFrontMC.inner sit on the guide origin (moved by
        # -localCenter, painted at the mass centre). The propeller turns (9 frames), the broken one
        # replaces it; the front legs go with the leg smashes. The magnet blinks while it is on.
        vehicle_parts=[
            ('copter', 'copter', 'inner@origin', ('propeller', 'brokenPropeller')),
            ('propeller', 'copter', 'inner-frames@origin:propeller'),
            ('brokenPropeller', 'copter', 'inner@origin', None, ('brokenPropeller',)),
            ('copterFront', 'copterFront', 'inner@origin', ('leg1', 'leg2')),
            ('leg1', 'copterFront', 'inner@origin', None, ('leg1',)),
            ('leg2', 'copterFront', 'inner@origin', None, ('leg2',)),
            ('magnet', 'magnet', 'clip', ('lights',)),
            ('magnetOn', 'magnet', 'clip-frames'),
        ] + [('broken%d' % i, 'broken%d' % i, 'clip') for i in range(1, 8)] + [
            ('shard', 'copterShards', 'trimmed-frames'),
        ],
        select_vehicle_parts=['copter', 'propeller_1', 'copterFront', 'leg1', 'leg2', 'magnet'],
        sounds=['HeliLoop', 'MagnetBuzz', 'HeliHelp']
               + ['Heli' + n for n in ('Elbow1', 'Elbow2', 'Foot1', 'Foot2', 'Hip1', 'Hip2', 'Knee1',
                                       'Knee2', 'Pelvis', 'Shoulder1', 'Shoulder2', 'Spikes', 'Torso')],
        select_offset=None,
    ),
}

CALIBRATION = dict(swf_id=3, name='irresponsible_dad', vehicle='road_bike')


def log(msg):
    print('extract_character: ' + msg, flush=True)


# ------------------------------------------------------------------------------------------
# Flash side
# ------------------------------------------------------------------------------------------

class Placement:
    __slots__ = ('depth', 'cid', 'clip', 'name', 'tx', 'ty', 'a', 'b', 'c', 'd')

    def __init__(self, p):
        self.depth = int(p[3])
        self.cid = int(p[4])
        self.clip = p[5] == '1'
        self.name = None if p[6] == '-' else p[6]
        self.tx, self.ty = int(p[7]) / 20.0, int(p[8]) / 20.0
        self.a, self.b, self.c, self.d = (float(v) for v in p[9:13])

    @property
    def scale_x(self):
        return math.hypot(self.a, self.b)

    @property
    def scale_y(self):
        return math.hypot(self.c, self.d)

    @property
    def rotation(self):  # radians, Flash (y down) sense
        return math.atan2(self.b, self.a)


class SwfTree:
    def __init__(self, text):
        self.clips = {}
        self.bounds = {}
        for line in text.splitlines():
            p = line.split()
            if not p:
                continue
            if p[0] == 'ERROR':
                raise RuntimeError(line)
            if p[0] == 'S':
                self.clips[int(p[1])] = [dict() for _ in range(int(p[2]))]
            elif p[0] == 'P':
                cid, frame = int(p[1]), int(p[2])
                self.clips.setdefault(cid, [dict()])
                while len(self.clips[cid]) < frame:
                    self.clips[cid].append(dict())
                pl = Placement(p)
                self.clips[cid][frame - 1][pl.depth] = pl
            elif p[0] == 'B':
                self.bounds[int(p[1])] = tuple(int(v) / 20.0 for v in p[2:6])

    def frames(self, cid):
        return len(self.clips.get(cid, [None]))

    def depths(self, cid, frame=1):
        return self.clips[cid][frame - 1]

    def child(self, cid, name, frame=1):
        for e in self.depths(cid, frame).values():
            if e.name == name:
                return e
        raise KeyError('clip %d has no child %s' % (cid, name))

    def has_child(self, cid, name, frame=1):
        return any(e.name == name for e in self.depths(cid, frame).values())

    def root(self, name):
        return self.child(0, name)

    def depths_named(self, cid, frame, names):
        return [d for d, e in self.depths(cid, frame).items() if e.name in names]

    def depths_except(self, cid, frame, name):
        return [d for d, e in self.depths(cid, frame).items() if e.name != name]


def java_tree(args, swf, work):
    text = kg.run_java(args.java, args.ffdec, swf, ['tree @root'], work)
    return SwfTree(text)


class Renderer:
    """Batches FFDec renders: add() returns a key, run() renders everything at once."""

    def __init__(self, args, swf, work):
        self.args, self.swf, self.work = args, swf, work
        self.jobs = {}

    def add(self, cid, frame=1, ignore=(), zoom=MASTER):
        key = (cid, frame, tuple(sorted(set(ignore))), zoom)
        self.jobs.setdefault(key, None)
        return key

    def run(self):
        todo = [k for k, v in self.jobs.items() if v is None]
        if not todo:
            return
        cmds, files = [], {}
        rdir = os.path.join(self.work, 'r')
        os.makedirs(rdir, exist_ok=True)
        for i, (cid, frame, ignore, zoom) in enumerate(todo):
            out = os.path.join(rdir, 'r%d_%d_%d.png' % (len(files), cid, frame)).replace('\\', '/')
            files[out] = (cid, frame, ignore, zoom)
            cmds.append('render %s %d %d %g %s' % (out, cid, frame, zoom, ','.join(map(str, ignore)) or '-'))
        text = kg.run_java(self.args.java, self.args.ffdec, self.swf, cmds, self.work)
        for line in text.splitlines():
            p = line.split()
            if p and p[0] == 'R':
                self.jobs[files[p[1]]] = kg.load_layer(p[1], (int(p[2]), int(p[3])))
        missing = [k for k in todo if self.jobs[k] is None]
        if missing:
            raise RuntimeError('renders missing: %s' % missing[:5])

    def __getitem__(self, key):
        return self.jobs[key]


# ------------------------------------------------------------------------------------------
# Image helpers
# ------------------------------------------------------------------------------------------

def place(dst, src, matrix):
    """Composites src (a child clip's render, master px) into dst (its parent's render) through
    the child's placement matrix (a, b, c, d, tx, ty in symbol px)."""
    a, b, c, d, tx, ty = matrix
    det = a * d - b * c
    ia, ib, ic, id_ = d / det, -b / det, -c / det, a / det
    ox, oy = dst.origin
    sx, sy = src.origin
    M = MASTER
    # dst pixel (X, Y) -> parent px p = ((X-ox)/M, (Y-oy)/M) -> child local q = inv(m)(p - t)
    # -> src pixel = q * M + s.  Child: x' = a x + c y + tx, y' = b x + d y + ty.
    # q.x = ia*(px-tx) + ic*(py-ty), q.y = ib*(px-tx) + id*(py-ty)
    k = 1.0 / M
    cx = -ox * k - tx
    cy = -oy * k - ty
    coeffs = (ia, ic, (ia * cx + ic * cy) * M + sx,
              ib, id_, (ib * cx + id_ * cy) * M + sy)
    coeffs = (coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4], coeffs[5])
    im = kg.to_pil_premul(src.a)
    h, w = dst.a.shape[:2]
    out = im.transform((w, h), Image.AFFINE, coeffs, resample=Image.BICUBIC)
    return kg.Layer(kg.over(dst.a, kg.from_pil_premul(out)), dst.origin)


def pad_layer(layer, pad):
    h, w = layer.a.shape[:2]
    a = np.zeros((h + 2 * pad, w + 2 * pad, 4), np.float32)
    a[pad:pad + h, pad:pad + w] = layer.a
    return kg.Layer(a, (layer.origin[0] + pad, layer.origin[1] + pad))


def diff_layer(changed, base):
    """Pixels of `changed` that differ from `base` (same render canvas)."""
    out = changed.a.copy()
    d = (np.abs(changed.a - base.a).sum(axis=2) > 0.15) & (changed.alpha > 0.02)
    out[~d] = 0
    return kg.Layer(out, changed.origin)


def canvas_for(layers, s):
    """Even-sized canvas (tier px) centred on the registration point covering every layer."""
    half_w = half_h = 1
    for L in layers:
        b = kg.bbox(L.alpha)
        if b is None:
            continue
        x0, y0, x1, y1 = b
        ox, oy = L.origin
        half_w = max(half_w, max(ox - x0, x1 - ox) * s / MASTER)
        half_h = max(half_h, max(oy - y0, y1 - oy) * s / MASTER)
    return int(2 * math.ceil(half_w)) + 2, int(2 * math.ceil(half_h)) + 2


def tier_image(layer, scale, canvas=None):
    """(image, sourceSize, offset) of a layer at `scale` tier px per symbol px. With a canvas the
    frame is centred on the registration point (stored trimmed + spriteOffset), else trimmed."""
    s = scale / MASTER
    h, w = layer.a.shape[:2]
    if canvas is None:
        size = (int(math.ceil(w * s)) + 4, int(math.ceil(h * s)) + 4)
        a = kg.warp(layer, size, s, (layer.origin[0] * s + 2, layer.origin[1] * s + 2))
        b = kg.bbox(a[:, :, 3])
        if b is None:
            a = np.zeros((2, 2, 4), np.float32)
            return a, (2, 2), (0, 0)
        a = a[b[1]:b[3], b[0]:b[2]]
        return a, (a.shape[1], a.shape[0]), (0, 0)
    W, H = canvas
    m = 8
    a = kg.warp(layer, (W + 2 * m, H + 2 * m), s, (W / 2 + m, H / 2 + m))
    b = kg.bbox(a[:, :, 3])
    if b is None:
        a = np.zeros((2, 2, 4), np.float32)
        return a, (W, H), (0, 0)
    x0, y0, x1, y1 = b
    # Clamp to the canvas (anything outside was included in canvas_for already).
    x0, y0 = max(x0, m), max(y0, m)
    x1, y1 = min(x1, W + m), min(y1, H + m)
    crop = a[y0:y1, x0:x1]
    off = ((x0 + x1) / 2 - m - W / 2, -((y0 + y1) / 2 - m - H / 2))
    return crop, (W, H), off


def write_sheet(frames, out_dir, sheet):
    images = {k: v[0] for k, v in frames.items()}
    area = sum(i.shape[0] * i.shape[1] for i in images.values())
    width = max(256, 1 << int(math.ceil(math.log2(max(1.0, math.sqrt(area * 1.15))))))
    pos, (W, H) = kg.pack(images, max_width=min(width, 4096))
    canvas = np.zeros((H, W, 4), np.float32)
    meta = {}
    for k, (img, src, off) in frames.items():
        x, y = pos[k]
        h, w = img.shape[:2]
        canvas[y:y + h, x:x + w] = img
        meta[k] = {
            'aliases': [],
            'spriteOffset': '{%s,%s}' % (kg.fmt(round(off[0], 1)), kg.fmt(round(off[1], 1))),
            'spriteSize': '{%d,%d}' % (w, h),
            'spriteSourceSize': '{%d,%d}' % src,
            'textureRect': '{{%d,%d},{%d,%d}}' % (x, y, w, h),
            'textureRotated': False,
        }
    os.makedirs(out_dir, exist_ok=True)
    rgba = np.clip(canvas * 255 + 0.5, 0, 255).astype(np.uint8)
    rgba[rgba[:, :, 3] == 0] = 0
    Image.fromarray(rgba, 'RGBA').save(os.path.join(out_dir, sheet + '.png'), optimize=True)
    plist = {
        'frames': {k: meta[k] for k in sorted(meta)},
        'metadata': {
            'format': 3, 'pixelFormat': 'RGBA8888', 'premultiplyAlpha': False,
            'realTextureFileName': sheet + '.png', 'size': '{%d,%d}' % (W, H),
            'textureFileName': sheet + '.png',
        },
    }
    with open(os.path.join(out_dir, sheet + '.plist'), 'wb') as f:
        plistlib.dump(plist, f, sort_keys=False)
    return W, H


# ------------------------------------------------------------------------------------------
# Character body parts
# ------------------------------------------------------------------------------------------

LIMBS = ['upperArm1', 'upperArm2', 'upperLeg1', 'upperLeg2']
TRIMMED = ['brain', 'heart', 'foot1:foot', 'intestine1:intestine', 'spine1:spine'] + \
    ['headChunk%d:headChunk_%d' % (i, i) for i in range(1, 5)] + \
    ['chestChunk%d:chestChunk_%d' % (i, i) for i in range(1, 5)] + \
    ['pelvisChunk%d:pelvisChunk_%d' % (i, i) for i in range(1, 4)]


def character_layers(tree, R, root=0):
    """Schedules the renders of every body-part frame; returns a builder that, once R.run() is
    done, yields {frame suffix: (Layer, canvas group or None)}. `root` is the clip holding the
    body parts (0 = the SWF root; a rider clip such as Irresponsible Mom's "daughter")."""
    plan = []  # (suffix, group, thunk)

    def clip(name):
        return tree.child(root, name).cid

    def frame_job(cid, frame, ignore_names=()):
        return R.add(cid, frame, tree.depths_named(cid, frame, ignore_names))

    head = clip('head')
    has_helmet = tree.has_child(head, 'helmet')
    for f in range(1, min(2, tree.frames(head)) + 1):
        k = frame_job(head, f, ['helmet'])
        plan.append(('head_%d' % f, 'head', lambda k=k: R[k]))
    if has_helmet:
        k = R.add(head, 1, tree.depths_except(head, 1, 'helmet'))
        plan.append(('helmet', 'head', lambda k=k: R[k]))

    chest = clip('chest')
    for f in range(1, min(2, tree.frames(chest)) + 1):
        k = frame_job(chest, f, ['neck', 'wound'])
        plan.append(('chest_%d' % f, 'chest', lambda k=k: R[k]))
    for child, suffix in (('neck', 'neck'), ('wound', 'shoulder_wound')):
        if tree.has_child(chest, child):
            k = R.add(chest, 1, tree.depths_except(chest, 1, child))
            plan.append((suffix, 'chest', lambda k=k: R[k]))

    pelvis = clip('pelvis')
    k1 = frame_job(pelvis, 1, ['wound'])
    plan.append(('pelvis', 'pelvis', lambda k=k1: R[k]))
    if tree.frames(pelvis) >= 2:
        k2 = frame_job(pelvis, 2, ['wound'])
        plan.append(('hipWound', 'pelvis', lambda a=k2, b=k1: diff_layer(R[a], R[b])))
    if tree.has_child(pelvis, 'wound'):
        k = R.add(pelvis, 1, tree.depths_except(pelvis, 1, 'wound'))
        plan.append(('pelvisWound', 'pelvis', lambda k=k: R[k]))

    # Dislocated limbs: Flash paints the severed piece (upperArm3MC/upperLeg3MC frames 2, 8) on a
    # new body at the shoulder/hip anchor whose box sits sev*Shape.y below it, and re-shapes the
    # thigh (frames 3, 6) to sevLeg2Shape, always painting at the mass centre; the mobile engine
    # paints at the body origin, so those frames are re-centred by the guide box's y offset.
    guide = guide_entries(tree, root)

    def sev_y(shape):
        return guide[shape].ty if shape in guide else 0.0

    shifts = {}
    for limb in LIMBS:
        if limb.startswith('upperArm'):
            shifts[(limb, 2)] = shifts[(limb, 8)] = sev_y('sevArm1Shape')
        else:
            shifts[(limb, 2)] = shifts[(limb, 8)] = sev_y('sevLeg1Shape')
            shifts[(limb, 3)] = shifts[(limb, 6)] = sev_y('sevLeg2Shape')

    def shifted(layer, dy):
        return kg.Layer(layer.a, (layer.origin[0], layer.origin[1] - dy * MASTER))

    for limb in LIMBS:
        cid = clip(limb)
        for f in range(1, 9):
            k = frame_job(cid, min(f, tree.frames(cid)))
            dy = shifts.get((limb, f), 0.0)
            plan.append(('%s_%d' % (limb, f), limb, lambda k=k, dy=dy: shifted(R[k], dy)))
    for i in '12':
        cid = clip('lowerArm' + i)
        for f in (1, 2):
            k = frame_job(cid, min(f, tree.frames(cid)))
            plan.append(('lowerArm%s_%d' % (i, f), 'lowerArm' + i, lambda k=k: R[k]))
        hand = tree.child(cid, 'hand')
        base = R.add(cid, 1, [hand.depth])
        open_hand = R.add(hand.cid, min(2, tree.frames(hand.cid)))
        m = (hand.a, hand.b, hand.c, hand.d, hand.tx, hand.ty)
        plan.append(('lowerArm%s_1_open' % i, 'lowerArm' + i,
                     lambda b=base, o=open_hand, m=m: place(pad_layer(R[b], 40), R[o], m)))
        cid = clip('lowerLeg' + i)
        for f in range(1, 5):
            k = frame_job(cid, min(f, tree.frames(cid)))
            plan.append(('lowerLeg%s_%d' % (i, f), 'lowerLeg' + i, lambda k=k: R[k]))

    for item in TRIMMED:
        src, _, dst = item.partition(':')
        dst = dst or src
        try:
            cid = clip(src)
        except KeyError:
            continue
        k = R.add(cid, 1)
        plan.append((dst, None, lambda k=k: R[k]))
    return plan


# ------------------------------------------------------------------------------------------
# Physics
# ------------------------------------------------------------------------------------------

CHAR_BODIES = ['headShape', 'chestShape', 'pelvisShape', 'upperArm1Shape', 'upperArm2Shape',
               'lowerArm1Shape', 'lowerArm2Shape', 'upperLeg1Shape', 'upperLeg2Shape',
               'lowerLeg1Shape', 'lowerLeg2Shape', 'sevArm1Shape', 'sevLeg1Shape', 'sevLeg2Shape',
               'footShape', 'intestineShape', 'ligamentShape', 'headChunkShape',
               'pelvisChunkShape', 'chestChunkShape']
CHAR_JOINTS = ['headAnchor', 'upperArmAnchor', 'lowerArm1Anchor', 'lowerArm2Anchor',
               'pelvisAnchor', 'upperLegAnchor', 'lowerLeg1Anchor', 'lowerLeg2Anchor',
               'spineAnchor']


def num(v):
    v = round(v, 3)
    if v == 0:
        v = 0.0
    s = ('%.3f' % v).rstrip('0').rstrip('.')
    return s if s not in ('', '-0') else '0'


def pt(x, y):
    return '{%s,%s}' % (num(x), num(y))


def guide_entries(tree, root=0):
    guide = tree.child(root, 'shapeGuide').cid
    return {e.name: e for e in tree.depths(guide, 1).values() if e.name}


def point_cids(guide):
    return {e.cid for n, e in guide.items() if re.search(r'(Anchor|[Vv]ert)\d*$', n)}


def to_mobile(x, y):
    return x * PHYS, -y * PHYS


CIRCLE_NAMES = ('headShape', 'headChunkShape', 'chestChunkShape', 'pelvisChunkShape')


def shape_entry(e, circle_cids, tree, name=''):
    """Box or circle guide. The guide symbols are squares of any size (10 px for most characters,
    100 px with shapeRefScale 50 for Irresponsible Mom and her kids): the extents come from the
    symbol bounds times the placement scale, as Flash's shape.width / scaleX * shapeRefScale."""
    x, y = to_mobile(e.tx, e.ty)
    rot = -e.rotation
    b = tree.bounds.get(e.cid, (-5, -5, 5, 5))
    if e.cid in circle_cids or name in CIRCLE_NAMES:
        width = (b[2] - b[0]) * e.scale_x
        return {'pos': pt(x, y), 'radius': round(width / 2 * PHYS, 3), 'rot': round(rot, 3)}
    hw = (b[2] - b[0]) / 2.0
    hh = (b[3] - b[1]) / 2.0
    return {'pos': pt(x, y), 'size': pt(e.scale_x * hw * PHYS, e.scale_y * hh * PHYS),
            'rot': round(rot, 3)}


def polygon(guide, prefix, count, start):
    verts = []
    for i in range(start, start + count):
        e = guide[prefix + str(i)]
        verts.append(to_mobile(e.tx, e.ty))
    return ':'.join(pt(x, y) for x, y in reversed(verts))


def character_bodies(tree, root=0):
    guide = guide_entries(tree, root)
    circles = point_cids(guide)
    bodies, joints = {}, {}
    for n in CHAR_BODIES:
        if n in guide:
            bodies[n] = shape_entry(guide[n], circles, tree, n)
    for part, count in (('chest', 6), ('pelvis', 5)):
        if part + 'Vert0' in guide:  # polygon torso (Effective Shopper, Lawnmower Man)
            n = sum(1 for k in guide if re.fullmatch(part + r'Vert\d+', k))
            e = bodies[part + 'Shape']
            bodies[part + 'Shape'] = {'pos': e['pos'], 'verts': polygon(guide, part + 'Vert', n, 0),
                                      'rot': e['rot']}
    if 'helmetVert1' in guide:  # knocked-off helmet, head-local (Flash helmetSmash)
        n = sum(1 for k in guide if re.fullmatch(r'helmetVert\d+', k))
        bodies['helmetShape'] = {'verts': polygon(guide, 'helmetVert', n, 1)}
    for n in CHAR_JOINTS:
        if n in guide:
            joints[n] = pt(*to_mobile(guide[n].tx, guide[n].ty))
    return {'bodies': bodies, 'joints': joints, 'shirtAbovePants': False}


def vehicle_bodies(tree, guides=((0, ''),)):
    """Every other shapeGuide child: boxes/circles in "bodies", points (vertices, anchors) in
    "joints". The vehicle class picks what it needs by name. `guides`: (root clip, key prefix)
    pairs - a rider clip's own vehicle parts (Irresponsible Mom's "daughter" trailer bike, the
    "son"'s basket) are merged in under a prefix."""
    bodies, joints = {}, {}
    for root, prefix in guides:
        guide = guide_entries(tree, root)
        circles = point_cids(guide)
        for n, e in sorted(guide.items()):
            if n in CHAR_BODIES or n in CHAR_JOINTS or re.fullmatch(r'(chest|pelvis|helmet)Vert\d+', n):
                continue
            if re.search(r'(Anchor|[Vv]ert|Left|Right|Bottom|Point\d+_|_\d+)\d*$', n):
                joints[prefix + n] = pt(*to_mobile(e.tx, e.ty))
            else:
                bodies[prefix + n] = shape_entry(e, circles, tree, n)
    return {'bodies': bodies, 'joints': joints}


def compare_plists(mine, theirs):
    """Calibration report: largest deviations between two body plists."""
    worst = []

    def nums(s):
        return [float(v) for v in re.findall(r'-?[\d.]+', str(s))]

    for sect in ('bodies', 'joints'):
        for k, v in theirs.get(sect, {}).items():
            if k not in mine.get(sect, {}):
                worst.append((9.9, '%s.%s missing' % (sect, k)))
                continue
            m = mine[sect][k]
            if isinstance(v, dict):
                for f in ('pos', 'size', 'radius', 'rot'):
                    if f in v and f in m:
                        d = max(abs(a - b) for a, b in zip(nums(v[f]), nums(m[f])))
                        worst.append((d, '%s.%s.%s ours %s theirs %s' % (sect, k, f, m[f], v[f])))
                if 'verts' in v and 'verts' in m:
                    d = max(abs(a - b) for a, b in zip(nums(v['verts']), nums(m['verts'])))
                    worst.append((d, '%s.%s.verts' % (sect, k)))
            else:
                d = max(abs(a - b) for a, b in zip(nums(v), nums(m)))
                worst.append((d, '%s.%s ours %s theirs %s' % (sect, k, m, v)))
    worst.sort(reverse=True)
    return worst


# ------------------------------------------------------------------------------------------
# Icons (character select buttons): head and shoulders of the rest pose
# ------------------------------------------------------------------------------------------

def rest_pose(tree, R, plan_layers, parts):
    """Composites body-part layers at their shapeGuide body positions (master px, y down)."""
    guide = guide_entries(tree)
    order = ['lowerArm2', 'upperArm2', 'upperLeg2', 'lowerLeg2', 'head', 'chest', 'pelvis',
             'upperLeg1', 'lowerLeg1', 'upperArm1', 'lowerArm1']
    size = 3000
    centre = (size / 2, size / 2)
    canvas = np.zeros((size, size, 4), np.float32)
    head_pos = None
    for part in order:
        e = guide[part + 'Shape']
        x, y = e.tx * MASTER, e.ty * MASTER
        layers = [plan_layers[parts[part]]]
        angle = ROT_SIGN * math.degrees(e.rotation)
        if part == 'head':
            # Profile like the mobile icons: the head upright, with its helmet on.
            angle = 0.0
            if 'helmet' in plan_layers:
                layers.append(plan_layers['helmet'])
        for layer in layers:
            a = kg.warp(layer, (size, size), 1.0, (centre[0] + x, centre[1] + y), angle=angle)
            canvas = kg.over(canvas, a)
        if part == 'head':
            head_pos = (centre[0] + x, centre[1] + y)
    return canvas, head_pos


def make_icons(canvas, head_pos, key):
    """250x250 colour + light grey icons like the mobile ones: the head in profile, cropped to
    head and shoulders, anchored bottom-left."""
    alpha = canvas[:, :, 3]
    hx, hy = head_pos
    # Head extent: rows above the chest
    ys, xs = np.where(alpha > 0.5)
    top = ys.min()
    span = (hy - top) * 2.3
    x0 = hx - span * 0.55
    y0 = top - span * 0.04
    box = (int(x0), int(y0), int(x0 + span), int(y0 + span))
    im = kg.to_pil_premul(canvas).crop(box).resize((250, 250), Image.LANCZOS)
    col = kg.from_pil_premul(im)
    grey = col.copy()
    lum = (col[:, :, 0] * 0.3 + col[:, :, 1] * 0.59 + col[:, :, 2] * 0.11)
    lum = 0.35 + lum * 0.65
    for c in range(3):
        grey[:, :, c] = lum
    return {key + '_icon.png': (col, (250, 250), (0, 0)),
            key + '_icon_bw.png': (grey, (250, 250), (0, 0))}


# ------------------------------------------------------------------------------------------
# Touch-control buttons (restored control sets)
# ------------------------------------------------------------------------------------------
# The mobile controls atlas (controls/gameplay/controls_gameplay.plist) draws every small button
# the same way: a 327 px (large tier) disc, a white ring at 40 % opacity (outer radius 161.5,
# inner radius ~120) around a black disc at 60 % opacity, and a flat white glyph at 40 % opacity
# replacing the disc where it is drawn (not composited over it). New glyphs for the restored
# characters' extra buttons are drawn here in that style, at each tier's own resolution
# (large 1, medium 1/2, small 3/8, tiny 1/4 of the large size, like the atlas).

CONTROL_SIZE = 326          # large-tier button size (the atlas' small buttons are 325-328 px)
CONTROL_TIER = {'large': 1.0, 'medium': 0.5, 'small': 0.375, 'tiny': 0.25}


def _glyph_kid(d, s, cx, cy, girl):
    """A child figure standing right of centre (the girl in a dress, with pigtails), in the
    stroke weight of the atlas' pose glyphs (round caps, ~26 px at large)."""
    w = 24 * s
    hx, hy = cx + 58 * s, cy - 56 * s

    def seg(a, b):
        d.line((a, b), fill=255, width=int(round(w)))
        for p in (a, b):
            d.ellipse((p[0] - w / 2, p[1] - w / 2, p[0] + w / 2, p[1] + w / 2), fill=255)

    def disc(x, y, r):
        d.ellipse((x - r, y - r, x + r, y + r), fill=255)

    disc(hx, hy, 19 * s)
    seg((hx, hy + 38 * s), (hx - 30 * s, hy + 70 * s))
    seg((hx, hy + 38 * s), (hx + 30 * s, hy + 70 * s))
    if girl:
        disc(hx - 21 * s, hy - 4 * s, 9 * s)
        disc(hx + 21 * s, hy - 4 * s, 9 * s)
        d.polygon(((hx, hy + 26 * s), (hx - 30 * s, hy + 88 * s), (hx + 30 * s, hy + 88 * s)), fill=255)
        seg((hx - 11 * s, hy + 86 * s), (hx - 13 * s, hy + 118 * s))
        seg((hx + 11 * s, hy + 86 * s), (hx + 13 * s, hy + 118 * s))
    else:
        seg((hx, hy + 32 * s), (hx, hy + 76 * s))
        seg((hx, hy + 76 * s), (hx - 18 * s, hy + 118 * s))
        seg((hx, hy + 76 * s), (hx + 18 * s, hy + 118 * s))


def _glyph_eject_small(d, s, cx, cy):
    """The atlas' eject glyph (triangle over a bar), smaller and left of centre."""
    d.polygon(((cx - 48 * s, cy - 66 * s), (cx - 104 * s, cy - 8 * s), (cx + 8 * s, cy - 8 * s)), fill=255)
    d.rectangle((cx - 104 * s, cy + 8 * s, cx + 8 * s, cy + 28 * s), fill=255)


def _glyph_deck_lift(d, s, cx, cy):
    """Lawnmower Man's deck lift: an up arrow over the mower deck on its two wheels."""
    d.polygon(((cx, cy - 102 * s), (cx - 50 * s, cy - 50 * s), (cx + 50 * s, cy - 50 * s)), fill=255)
    d.rectangle((cx - 16 * s, cy - 52 * s, cx + 16 * s, cy - 12 * s), fill=255)
    d.rounded_rectangle((cx - 84 * s, cy + 2 * s, cx + 84 * s, cy + 32 * s), radius=12 * s, fill=255)
    for wx, wr in ((-54, 22), (56, 17)):
        x, y = cx + wx * s, cy + (54 if wr > 20 else 59) * s
        d.ellipse((x - wr * s, y - wr * s, x + wr * s, y + wr * s), fill=255)


def _stroke(d, s, w):
    def seg(a, b):
        d.line((a, b), fill=255, width=int(round(w * s)))
        for p in (a, b):
            d.ellipse((p[0] - w * s / 2, p[1] - w * s / 2, p[0] + w * s / 2, p[1] + w * s / 2), fill=255)
    return seg


def _glyph_rail(d, s, cx, cy):
    """Explorer Guy's rail clamp: a cart wheel locked onto a rail (wheel, hub, clamp, rail)."""
    r_out, r_in = 58 * s, 38 * s
    wy = cy - 22 * s
    d.ellipse((cx - r_out, wy - r_out, cx + r_out, wy + r_out), fill=255)
    d.ellipse((cx - r_in, wy - r_in, cx + r_in, wy + r_in), fill=0)
    d.ellipse((cx - 13 * s, wy - 13 * s, cx + 13 * s, wy + 13 * s), fill=255)
    d.rectangle((cx - 100 * s, cy + 44 * s, cx + 100 * s, cy + 66 * s), fill=255)
    for side in (-1, 1):  # the clamp's jaws under the rail
        x = cx + side * 30 * s
        d.rectangle((min(x, x + side * 14 * s), cy + 36 * s, max(x, x + side * 14 * s), cy + 86 * s), fill=255)
    d.rectangle((cx - 44 * s, cy + 74 * s, cx + 44 * s, cy + 88 * s), fill=255)


def _glyph_stand(d, s, cx, cy):
    """Standing up in the cart: an upright figure, arms forward on the cart rim."""
    seg = _stroke(d, s, 26)
    d.ellipse((cx - 6 * s - 20 * s, cy - 96 * s - 20 * s, cx - 6 * s + 20 * s, cy - 96 * s + 20 * s), fill=255)
    seg((cx - 8 * s, cy - 62 * s), (cx - 10 * s, cy + 6 * s))
    seg((cx - 8 * s, cy - 54 * s), (cx + 44 * s, cy - 26 * s))
    seg((cx - 10 * s, cy + 6 * s), (cx - 2 * s, cy + 92 * s))
    seg((cx - 10 * s, cy + 6 * s), (cx - 30 * s, cy + 92 * s))


def _glyph_crouch(d, s, cx, cy):
    """Ducking in the cart: a crouched figure, head low, knees bent."""
    seg = _stroke(d, s, 26)
    d.ellipse((cx + 34 * s - 20 * s, cy - 56 * s - 20 * s, cx + 34 * s + 20 * s, cy - 56 * s + 20 * s), fill=255)
    seg((cx + 4 * s, cy - 18 * s), (cx - 40 * s, cy + 22 * s))
    seg((cx - 4 * s, cy - 10 * s), (cx + 42 * s, cy + 8 * s))
    seg((cx - 40 * s, cy + 22 * s), (cx + 10 * s, cy + 48 * s))
    seg((cx + 10 * s, cy + 48 * s), (cx - 34 * s, cy + 86 * s))


def _glyph_elf(d, s, cx, cy):
    """Santa's elves let go: the eject glyph and a small elf in a pointed hat."""
    _glyph_eject_small(d, s, cx, cy)
    seg = _stroke(d, s, 24)
    hx, hy = cx + 56 * s, cy - 40 * s
    d.ellipse((hx - 18 * s, hy - 18 * s, hx + 18 * s, hy + 18 * s), fill=255)
    d.polygon(((hx - 20 * s, hy - 8 * s), (hx + 18 * s, hy - 14 * s), (hx + 22 * s, hy - 46 * s)), fill=255)
    seg((hx, hy + 36 * s), (hx - 28 * s, hy + 66 * s))
    seg((hx, hy + 36 * s), (hx + 28 * s, hy + 66 * s))
    seg((hx, hy + 30 * s), (hx, hy + 76 * s))
    seg((hx, hy + 76 * s), (hx - 18 * s, hy + 108 * s))
    seg((hx, hy + 76 * s), (hx + 18 * s, hy + 108 * s))


CONTROL_GLYPHS = {
    'restored_btn_eject_son': lambda d, s, cx, cy: (_glyph_eject_small(d, s, cx, cy),
                                                    _glyph_kid(d, s, cx, cy, False)),
    'restored_btn_eject_daughter': lambda d, s, cx, cy: (_glyph_eject_small(d, s, cx, cy),
                                                         _glyph_kid(d, s, cx, cy, True)),
    'restored_btn_deck_lift': _glyph_deck_lift,
    'restored_btn_rail': _glyph_rail,
    'restored_btn_stand': _glyph_stand,
    'restored_btn_crouch': _glyph_crouch,
    'restored_btn_release_elves': _glyph_elf,
}


def control_button(name, size):
    """One button at `size` px (4x supersampled), straight-alpha float RGBA."""
    from PIL import ImageDraw
    ss = 4
    S = size * ss
    s = S / float(CONTROL_SIZE)
    c = (S - 1) / 2.0

    def mask(draw_fn):
        im = Image.new('L', (S, S), 0)
        draw_fn(ImageDraw.Draw(im))
        return np.asarray(im.resize((size, size), Image.LANCZOS)).astype(np.float32) / 255.0

    r_out = 161.5 * s
    r_in = 120.0 * s
    outer = mask(lambda d: d.ellipse((c - r_out, c - r_out, c + r_out, c + r_out), fill=255))
    inner = mask(lambda d: d.ellipse((c - r_in, c - r_in, c + r_in, c + r_in), fill=255))
    glyph = mask(lambda d: CONTROL_GLYPHS[name](d, s, c, c)) * inner
    # premultiplied: ring white .4, disc black .6, glyph white .4 (replacing the disc)
    disc_a = 0.6 * (1 - glyph) + 0.4 * glyph
    disc_c = 0.4 * glyph
    a = outer * (0.4 * (1 - inner) + disc_a * inner)
    pc = outer * (0.4 * (1 - inner) + disc_c * inner)
    rgb = np.where(a > 0, pc / np.maximum(a, 1e-6), 0)
    out = np.zeros((size, size, 4), np.float32)
    out[:, :, 0] = out[:, :, 1] = out[:, :, 2] = np.clip(rgb, 0, 1)
    out[:, :, 3] = np.clip(a, 0, 1)
    return out


def write_control_buttons(out):
    for tier, f in CONTROL_TIER.items():
        size = int(round(CONTROL_SIZE * f))
        frames = {}
        for name in CONTROL_GLYPHS:
            frames[name + '.png'] = (control_button(name, size), (size, size), (0, 0))
        write_sheet(frames, os.path.join(out, tier, 'controls'), 'restored_controls')


# ------------------------------------------------------------------------------------------
# Sounds
# ------------------------------------------------------------------------------------------

def export_sounds(args, names, out_dir, work):
    if not names:
        return []
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        log('ffmpeg not found: sounds %s skipped' % ', '.join(names))
        return []
    if not os.path.isfile(args.sounds_swf):
        log('sound SWF not found at %s: sounds skipped' % args.sounds_swf)
        return []
    sdir = os.path.join(work, 'snd')
    if not os.path.isdir(sdir) or not os.listdir(sdir):
        os.makedirs(sdir, exist_ok=True)
        jar = os.path.join(args.ffdec, 'ffdec-cli.jar')
        if not os.path.isfile(jar):
            jar = os.path.join(args.ffdec, 'ffdec.jar')
        subprocess.run([args.java, '-Djava.awt.headless=true', '-jar', jar, '-format', 'sound:mp3',
                        '-export', 'sound', sdir, args.sounds_swf],
                       capture_output=True, text=True, timeout=900)
    files = {}
    for f in os.listdir(sdir):
        m = re.match(r'\d+_(?:.*\.)?([A-Za-z0-9]+)\.(mp3|wav)$', f)
        if m:
            files[m.group(1)] = os.path.join(sdir, f)
    os.makedirs(out_dir, exist_ok=True)
    done = []
    for n in names:
        if n not in files:
            log('sound %s not in %s' % (n, args.sounds_swf))
            continue
        dst = os.path.join(out_dir, n + '.ogg')
        res = subprocess.run([ffmpeg, '-y', '-loglevel', 'error', '-i', files[n], '-c:a', 'libvorbis',
                              '-q:a', '5', dst], capture_output=True, text=True, timeout=300)
        if res.returncode == 0:
            done.append(n)
        else:
            log('ffmpeg failed for %s: %s' % (n, res.stderr[-300:]))
    return done


# ------------------------------------------------------------------------------------------
# Main pipeline
# ------------------------------------------------------------------------------------------

def resolve_clip(tree, path):
    """'frame' or 'daughter/frame': a named clip under the SWF root (or under a rider clip)."""
    cid = 0
    entry = None
    for part in path.split('/'):
        entry = tree.child(cid, part)
        cid = entry.cid
    return entry


def resolve_root(tree, path):
    return 0 if not path else resolve_clip(tree, path).cid


class _Origin:
    tx = ty = 0.0


def guide_point(tree, ref):
    """'rearVert5' (main shapeGuide), 'son:crateLeft1' (a rider clip's shapeGuide) or 'origin'
    (the guide origin itself: Flash moves <mc>.inner by -localCenter, i.e. onto the origin)."""
    root, _, name = ref.rpartition(':')
    if name == 'origin':
        return _Origin()
    return guide_entries(tree, resolve_root(tree, root))[name]


def vehicle_plan(tree, R, parts):
    """Vehicle frames: (frame name, clip path, mode[, hidden child names]). Modes:
      'inner@<guide point>'  the clip's "inner" child drawn with its origin on that shapeGuide
                             point (Flash createMovieClips moves <mc>.inner there); the canvas is
                             centred on the guide origin = the body origin the mobile vehicle uses
      'clip'                 the whole clip, centred on its registration point (Flash paints it at
                             the body's mass centre)
      'static' / 'spin'      the clip without / only its "inner" child
      'wheel'                a bicycle wheel: tyre, hub and the inner spokes (not inner.broken)
      'wheel-broken'         frame 2 of a bicycle wheel: inner.broken only (Flash wheelSmash)
      'trimmed-frames'       every frame of the clip, trimmed (particles)"""
    plan = []
    for part in parts:
        frame, path, mode = part[:3]
        hide = part[3] if len(part) > 3 and part[3] else ()
        only = part[4] if len(part) > 4 else None
        if path.startswith('game:'):
            continue  # symbols of the main game SWF: see game_symbol_plan
        e = resolve_clip(tree, path)

        def hidden(cid, f=1, extra=(), hide=hide, only=only):
            if only:  # draw only these named children
                return [d for d, c in tree.depths(cid, f).items() if c.name not in only]
            return tree.depths_named(cid, f, set(hide) | set(extra))

        if mode.startswith('inner@'):
            inner = tree.child(e.cid, 'inner')
            k = R.add(inner.cid, 1, hidden(inner.cid))
            g = guide_point(tree, mode.split('@', 1)[1])
            plan.append((frame, 'canvas', lambda k=k, g=g: kg.Layer(
                R[k].a, (R[k].origin[0] - g.tx * MASTER, R[k].origin[1] - g.ty * MASTER))))
        elif mode == 'static':
            k = R.add(e.cid, 1, hidden(e.cid, extra=('inner',)))
            plan.append((frame, 'canvas', lambda k=k: R[k]))
        elif mode == 'spin':
            k = R.add(e.cid, 1, tree.depths_except(e.cid, 1, 'inner'))
            plan.append((frame, 'canvas', lambda k=k: R[k]))
        elif mode == 'clip':
            k = R.add(e.cid, 1, hidden(e.cid))
            plan.append((frame, 'canvas', lambda k=k: R[k]))
        elif mode == 'wheel':
            inner = tree.child(e.cid, 'inner')
            spokes = R.add(inner.cid, 1, tree.depths_named(inner.cid, 1, ['broken']))
            rest = R.add(e.cid, 1, [inner.depth])
            plan.append((frame, 'canvas', lambda s_=spokes, r_=rest: place(
                pad_layer(R[s_], 400), R[r_], (1, 0, 0, 1, 0, 0))))
        elif mode == 'wheel-broken':
            inner = tree.child(e.cid, 'inner')
            k = R.add(inner.cid, 1, tree.depths_except(inner.cid, 1, 'broken'))
            plan.append((frame, 'canvas', lambda k=k: R[k]))
        elif mode.startswith('inner-frames@'):
            # 'inner-frames@<guide point>:<child>': every frame of the inner clip's child <child>
            # (an animation such as Helicopter Man's propeller), placed as in the inner clip.
            point, _, child_name = mode.split('@', 1)[1].partition(':')
            inner = tree.child(e.cid, 'inner')
            child = tree.child(inner.cid, child_name)
            g = guide_point(tree, point)
            for f in range(1, tree.frames(child.cid) + 1):
                k = R.add(child.cid, f)
                plan.append(('%s_%d' % (frame, f), 'canvas', lambda k=k, g=g, c=child: kg.Layer(
                    R[k].a, (R[k].origin[0] - (g.tx + c.tx) * MASTER, R[k].origin[1] - (g.ty + c.ty) * MASTER))))
        elif mode == 'clip-frames':
            # every frame of the clip, centred on its registration point (hidden children apply)
            for f in range(1, tree.frames(e.cid) + 1):
                k = R.add(e.cid, f, hidden(e.cid, f))
                plan.append(('%s_%d' % (frame, f), 'canvas', lambda k=k: R[k]))
        elif mode == 'trimmed-frames':
            for f in range(1, tree.frames(e.cid) + 1):
                k = R.add(e.cid, f)
                plan.append(('%s_%d' % (frame, f), None, lambda k=k: R[k]))
        else:
            raise ValueError('unknown vehicle part mode ' + mode)
    return plan


def game_symbol_plan(args, parts, work):
    """Vehicle frames drawn from the main game SWF ('game:<class>' paths, e.g. Santa's
    ChristmasBellMC, which the game library holds, not character8.swf). Rendered whole ('clip').
    Skipped with a note when the game SWF is missing."""
    wanted = [p for p in parts if p[1].startswith('game:')]
    if not wanted:
        return []
    if not os.path.isfile(args.game_swf):
        log('game SWF not found at %s: %s skipped' % (args.game_swf, ', '.join(p[0] for p in wanted)))
        return []
    gwork = os.path.join(work, 'game')
    os.makedirs(gwork, exist_ok=True)
    R = Renderer(args, args.game_swf, gwork)
    plan = []
    for frame, path, mode in (p[:3] for p in wanted):
        text = kg.run_java(args.java, args.ffdec, args.game_swf, ['tree ' + path[5:]], gwork)
        root = next((int(l.split()[1]) for l in text.splitlines() if l.startswith('ROOT ')), None)
        if root is None:
            log('no symbol %s in the game SWF' % path[5:])
            continue
        # Library symbols are added to the stage unscaled (world px), character art at
        # 1 / mc_scale: twice the zoom keeps them at their on-screen size.
        k = R.add(root, 1, zoom=MASTER * 2)
        plan.append((frame, 'canvas', lambda k=k: R[k]))
    R.run()
    return plan


SEL_SUFFIXES = ('chest_1', 'head_1', 'helmet', 'lowerArm1_1', 'lowerArm1_1_open', 'lowerArm2_1',
                'lowerArm2_1_open', 'lowerLeg1_1', 'lowerLeg2_1', 'pelvis', 'upperArm1_1',
                'upperArm2_1', 'upperLeg1_1', 'upperLeg2_1')


def write_character_sheets(out, name, layers):
    groups = {}
    for suffix, (layer, group) in layers.items():
        if group:
            groups.setdefault(group, []).append(layer)
    for tier, scale in TIERS.items():
        for select in (False, True):
            sc = scale * (2 if select else 1)
            canv = {g: canvas_for(ls, sc) for g, ls in groups.items()}
            frames = {}
            for suffix, (layer, group) in layers.items():
                if select and suffix not in SEL_SUFFIXES:
                    continue
                frames['%s_%s.png' % (name, suffix)] = tier_image(layer, sc, canv.get(group))
            sheet = ('character_select_' if select else '') + name + '_sprites'
            W, H = write_sheet(frames, os.path.join(out, tier, 'characters'), sheet)
            if tier == 'large':
                log('%s%s %s: %d frames %dx%d' % ('select ' if select else '', tier, name,
                                                   len(frames), W, H))


def build_character(args, cid, spec, work):
    swf = os.path.join(args.swf_dir, 'character%d.swf' % cid)
    os.makedirs(work, exist_ok=True)
    tree = java_tree(args, swf, work)
    R = Renderer(args, swf, work)
    name, veh = spec['name'], spec['vehicle']

    # The player character, then the riders that come in the same SWF (Irresponsible Mom's
    # "daughter" and "son" clips: body parts and a shapeGuide of their own, in the same frame).
    riders = [('', name)] + list(spec.get('riders', []))
    plans = [(rname, character_layers(tree, R, resolve_root(tree, path))) for path, rname in riders]
    vplan = vehicle_plan(tree, R, spec['vehicle_parts'])
    log('character%d: rendering %d clip frames' % (cid, len(R.jobs)))
    R.run()
    vplan += game_symbol_plan(args, spec['vehicle_parts'], work)

    out = args.out
    all_layers = {}
    for rname, plan in plans:
        layers = {suffix: (fn(), group) for suffix, group, fn in plan}
        all_layers[rname] = layers
        write_character_sheets(out, rname, layers)

    vlayers = {frame: (fn(), group) for frame, group, fn in vplan}
    for tier, scale in TIERS.items():
        for select in (False, True):
            sc = scale * (2 if select else 1)
            vframes = {}
            for frame, (layer, group) in vlayers.items():
                if select and frame not in spec['select_vehicle_parts']:
                    continue
                cv = canvas_for([layer], sc) if group else None
                vframes['%s_%s.png' % (veh, frame)] = tier_image(layer, sc, cv)
            vsheet = ('character_select_' if select else '') + veh + '_sprites'
            VW, VH = write_sheet(vframes, os.path.join(out, tier, 'vehicles'), vsheet)
            if tier == 'large':
                log('%svehicle %s: %d frames %dx%d' % ('select ' if select else '', veh,
                                                     len(vframes), VW, VH))

    # Bodies
    os.makedirs(os.path.join(out, 'shared', 'characters', 'bodies'), exist_ok=True)
    os.makedirs(os.path.join(out, 'shared', 'vehicles', 'bodies'), exist_ok=True)
    for path, rname in riders:
        cb = character_bodies(tree, resolve_root(tree, path))
        with open(os.path.join(out, 'shared', 'characters', 'bodies', '%s_%s.plist' % (rname, veh)), 'wb') as f:
            plistlib.dump(cb, f)
    guides = [(0, '')] + [(resolve_root(tree, path), prefix)
                          for path, prefix in spec.get('vehicle_guides', [])]
    vb = vehicle_bodies(tree, guides)
    with open(os.path.join(out, 'shared', 'vehicles', 'bodies', veh + '.plist'), 'wb') as f:
        plistlib.dump(vb, f)

    # Icon frames
    parts = {p: '%s_1' % p for p in LIMBS}
    parts.update({'head': 'head_1', 'chest': 'chest_1', 'pelvis': 'pelvis', 'lowerArm1': 'lowerArm1_1',
                  'lowerArm2': 'lowerArm2_1', 'lowerLeg1': 'lowerLeg1_1', 'lowerLeg2': 'lowerLeg2_1'})
    plain = {k: v[0] for k, v in all_layers[name].items()}
    canvas, head_pos = rest_pose(tree, R, plain, parts)
    icons = make_icons(canvas, head_pos, spec['key'])

    # Character-select entry
    offset = spec.get('select_offset')
    if offset is None:
        wheels = [vb['bodies'][k] for k in vb['bodies'] if k.lower().endswith('wheelshape')]
        lows, xs = [], []
        for w in wheels:
            x, y = (float(v) for v in re.findall(r'-?[\d.]+', w['pos']))
            lows.append(y - w['radius'])
            xs.append(x)
        offset = (-(min(xs) + max(xs)) / 2, -min(lows)) if wheels else (0.0, 1.5)
    entry = {
        'id': cid, 'key': spec['key'], 'controls': spec['controls'], 'name': spec['title'],
        'class': spec['cls'], 'restored': True,
        'vehicles': [{'offset': pt(*offset), 'class': spec['vehicle_cls'], 'key': spec['vehicle_key'],
                      'specialOnLeft': spec['special_on_left']}],
    }
    return entry, icons


def calibrate(args, work):
    """Runs the conversion on Irresponsible Dad (character3.swf) and compares with his Android
    files: body plist deviations and per-frame IoU / centre offset of the large sprites."""
    c = CALIBRATION
    swf = os.path.join(args.swf_dir, 'character%d.swf' % c['swf_id'])
    tree = java_tree(args, swf, work)
    mine = character_bodies(tree)
    with open(os.path.join(args.assets, 'shared', 'characters', 'bodies',
                           '%s_%s.plist' % (c['name'], c['vehicle'])), 'rb') as f:
        theirs = plistlib.load(f)
    worst = compare_plists(mine, theirs)
    log('calibration bodies: %d values, worst deviations:' % len(worst))
    for d, what in worst[:6]:
        log('   %.3f  %s' % (d, what))
    log('   median %.4f m' % sorted(d for d, _ in worst)[len(worst) // 2])

    R = Renderer(args, swf, work)
    plan = character_layers(tree, R)
    R.run()
    layers = {suffix: (fn(), group) for suffix, group, fn in plan}
    groups = {}
    for suffix, (layer, group) in layers.items():
        if group:
            groups.setdefault(group, []).append(layer)
    ref = kg.load_sheet(args.assets, 'large', c['name'] + '_sprites')
    scale = TIERS['large']
    canv = {g: canvas_for(ls, scale) for g, ls in groups.items()}
    ious = []
    for suffix, (layer, group) in sorted(layers.items()):
        key = '%s_%s.png' % (c['name'], suffix)
        if key not in ref or group is None:
            continue
        img, src, off = tier_image(layer, scale, canv[group])
        rc = np.asarray(ref[key]['canvas'])[:, :, 3] > 96
        # our frame on a canvas of the reference's size, both centred on the registration point
        W, H = ref[key]['src']
        mine_c = np.zeros((H, W), bool)
        h, w = img.shape[:2]
        x0 = int(round((W - w) / 2 + off[0]))
        y0 = int(round((H - h) / 2 - off[1]))
        m = img[:, :, 3] > 96 / 255.0
        for yy in range(h):
            ty = y0 + yy
            if 0 <= ty < H:
                xs0, xs1 = max(0, x0), min(W, x0 + w)
                if xs1 > xs0:
                    mine_c[ty, xs0:xs1] = m[yy, xs0 - x0:xs1 - x0]
        inter = (mine_c & rc).sum()
        union = max(1, (mine_c | rc).sum())
        iou = inter / union

        def centroid(mm):
            ys, xs = np.where(mm)
            return (xs.mean(), ys.mean()) if len(xs) else (0, 0)
        ca, cb = centroid(mine_c), centroid(rc)
        ious.append((iou, suffix, ca[0] - cb[0], ca[1] - cb[1]))
    ious.sort()
    log('calibration sprites (large): %d frames, median IoU %.3f; lowest:' % (
        len(ious), ious[len(ious) // 2][0] if ious else 0))
    for iou, suffix, dx, dy in (sorted(ious, key=lambda t: t[1]) if args.verbose else ious[:8]):
        log('   %-22s IoU %.3f  centroid offset %+.1f,%+.1f px' % (suffix, iou, dx, dy))
    return 0


def stamp_for(args, ids):
    h = hashlib.sha256()
    h.update(VERSION.encode())
    for p in (__file__, kg.__file__, kg.JAVA_HELPER):
        with open(p, 'rb') as f:
            h.update(f.read())
    for cid in ids:
        st = os.stat(os.path.join(args.swf_dir, 'character%d.swf' % cid))
        h.update(('%d|%d|%d' % (cid, st.st_size, int(st.st_mtime))).encode())
    if os.path.isfile(args.sounds_swf):
        h.update(str(os.stat(args.sounds_swf).st_size).encode())
    if os.path.isfile(args.game_swf):
        h.update(str(os.stat(args.game_swf).st_size).encode())
    h.update(str(bool(shutil.which('ffmpeg'))).encode())
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--swf-dir', default=DEFAULT_SWF_DIR, help='folder with the browser character<N>.swf')
    ap.add_argument('--sounds-swf', default=DEFAULT_SOUNDS_SWF, help='the browser game\'s happy_sounds SWF')
    ap.add_argument('--game-swf', default=kg.DEFAULT_SWF, help='the decrypted browser game SWF (its library symbols)')
    ap.add_argument('--ffdec', default=kg.DEFAULT_FFDEC, help='JPEXS FFDec folder (with lib/ffdec_lib.jar)')
    ap.add_argument('--assets', default=kg.DEFAULT_ASSETS, help="the Android game's assets/ folder (calibration)")
    ap.add_argument('--out', help='output root (e.g. <exe dir>/generated/restored)')
    ap.add_argument('--ids', default=','.join(str(i) for i in sorted(SPECS)), help='character ids')
    ap.add_argument('--java', default=None)
    ap.add_argument('--work', default=None)
    ap.add_argument('--optional', action='store_true', help='exit 0 when inputs are missing')
    ap.add_argument('--force', action='store_true')
    ap.add_argument('--keep-work', action='store_true')
    ap.add_argument('--calibrate', action='store_true', help='validate the conversion on Irresponsible Dad')
    ap.add_argument('--verbose', action='store_true', help='--calibrate: list every frame')
    args = ap.parse_args()

    def give_up(msg):
        if args.optional:
            log('skipped (%s); the restored characters stay unavailable' % msg)
            return 0
        log('error: ' + msg)
        return 1

    if np is None:
        return give_up('Python packages numpy and Pillow are required')
    if not os.path.isfile(os.path.join(args.ffdec, 'lib', 'ffdec_lib.jar')):
        return give_up('FFDec not found at %s' % args.ffdec)
    args.java = kg.find_java(args.java)
    if not args.java:
        return give_up('java not found')
    work = args.work or os.path.join(args.out or os.path.join(REPO, 'build', 'tmp'), '.restored_work')
    os.makedirs(work, exist_ok=True)
    if args.calibrate:
        return calibrate(args, work)
    if not args.out:
        ap.error('--out is required')

    ids = [int(v) for v in args.ids.split(',') if v]
    ids = [i for i in ids if i in SPECS and os.path.isfile(os.path.join(args.swf_dir, 'character%d.swf' % i))]
    if not ids:
        return give_up('no character SWFs in %s' % args.swf_dir)

    stamp_path = os.path.join(args.out, '.restored_stamp')
    stamp = stamp_for(args, ids)
    if not args.force and os.path.isfile(stamp_path):
        with open(stamp_path) as f:
            if f.read().strip() == stamp:
                log('up to date')
                return 0

    entries, icons, sounds = [], {}, []
    try:
        for cid in ids:
            entry, ic = build_character(args, cid, SPECS[cid], os.path.join(work, str(cid)))
            entries.append(entry)
            icons.update(ic)
            sounds += SPECS[cid].get('sounds', [])
    except Exception as e:  # noqa: BLE001
        import traceback
        traceback.print_exc()
        return give_up('building failed: %s' % e)

    for tier, scale in TIERS.items():
        f = {}
        for k, (img, src, off) in icons.items():
            s = scale / 2.0  # icons are 250 px on large
            size = (max(1, int(round(src[0] * s))), max(1, int(round(src[1] * s))))
            im = kg.to_pil_premul(img).resize(size, Image.LANCZOS)
            f[k] = (kg.from_pil_premul(im), size, (0, 0))
        write_sheet(f, os.path.join(args.out, tier, 'menus', 'character_select'), 'restored_icons')
    write_control_buttons(args.out)

    with open(os.path.join(args.out, 'shared', 'Characters_restored.plist'), 'wb') as f:
        plistlib.dump(entries, f)
    done = export_sounds(args, sorted(set(sounds)), os.path.join(args.out, 'sounds'), work)
    log('characters %s, sounds %s -> %s' % (','.join(str(e['id']) for e in entries), ','.join(done) or '-', args.out))

    with open(stamp_path, 'w') as f:
        f.write(stamp + '\n')
    if not args.keep_work:
        shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
