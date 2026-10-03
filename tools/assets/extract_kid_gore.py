#!/usr/bin/env python3
"""Rebuilds the gore sprites of Irresponsible Dad's kid from the player's own Flash game.

The mobile port ships the kid ("irresponsible_dad_kid") with only his 13 intact body-part frames,
so he can't be dismembered. The browser game still has his art: the level editor's
"Irresponsible Dad's kid" NPC (library symbol NPCSprite4 in the game SWF) carries every body part
with its damage frames, the head/chest/pelvis chunks, brain and heart. This script renders those
with JPEXS FFDec (tools/assets/FlashPartRender.java on ffdec_lib.jar), puts them into the mobile
port's sprite conventions and writes one extra sprite sheet per asset tier:

    <out>/<tier>/characters/irresponsible_dad_kid_gore_sprites.{plist,png}

with the frame names the game requests (irresponsible_dad_kid_head_2.png, ..._upperArm1_5.png).

Conventions, calibrated against the Android 1.1.3 sheets (the dad's NPC symbol rendered this way
matches his mobile frames; the kid's NPC frame 1 matches his 13 mobile frames at IoU 0.95-1.00
with centroids within 0.3 px on every tier):
  * tier scale: large = 2 tier pixels per NPC-symbol pixel, medium 1, small 0.75, tiny 0.5
    (the NPC art is drawn at twice the in-game size);
  * body-part frames: untrimmed canvas centred on the clip's registration point (= body origin),
    the same canvas size as the kid's existing _1 frame of that part; overlays added as children
    with anchor (0,0) (neck, shoulder_wound, hipWound, pelvisWound) use their parent's canvas;
  * chunks, organs and the foot: trimmed, no offset (the sprite centre is the body);
  * NPC frame -> mobile state: upper limbs 1/2/3/4 -> _1/_4/_5/_7, lower arms 2 -> _2, lower
    legs 2 -> _2, head 2 -> head_2, chest 5 -> chest_2.
What the NPC symbol does not have is synthesised from the kid's own art:
  * upper limbs _2/_3/_6/_8 (limb split at the sleeve/shorts hem when a joint dislocates): the
    intact limb cut at the hem, capped with the kid's own shoulder/hip or elbow/knee gore;
  * lower legs _3/_4 (foot torn off): the leg without the shoe, capped with the knee gore
    mirrored to the ankle;
  * intestine and spine (generic, not drawn per NPC): the dad's mobile frames, thinner.

Needs Java 11+ and FFDec (ffdec_lib.jar). With --optional, missing inputs only print a note and
exit 0 (build hooks). Output is skipped when an up-to-date stamp exists.
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

try:
    import numpy as np
    from PIL import Image
except ImportError:  # pragma: no cover
    np = None
    Image = None

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
JAVA_HELPER = os.path.join(HERE, 'FlashPartRender.java')

KID_SYMBOL = 'com.totaljerkface.game.editor.specials.npcsprites.NPCSprite4'
PREFIX = 'irresponsible_dad_kid'
SHEET = PREFIX + '_gore_sprites'
TIERS = {'large': 2.0, 'medium': 1.0, 'small': 0.75, 'tiny': 0.5}  # tier px per NPC px
MASTER = 4.0           # render zoom; downsampled per tier
MASTER_TO_LARGE = 2    # MASTER / TIERS['large']
VERSION = '1'          # bump when the output format changes

DEFAULT_SWF = os.path.join(REPO, 'binary', 'flash', 'swf', 'game_e_v1_87_g.dec.swf')
DEFAULT_FFDEC = os.path.join(REPO, 'binary', 'flash', 'tools', 'ffdec')
DEFAULT_ASSETS = os.path.join(REPO, 'binary', 'HappyWheels_Android', 'HW_Android', 'assets')


def log(msg):
    print('extract_kid_gore: ' + msg, flush=True)


# ------------------------------------------------------------------------------------------
# Flash side
# ------------------------------------------------------------------------------------------

class FlashTree:
    """Display lists of every clip under the kid symbol: clips[id][frame] = {depth: entry}."""

    def __init__(self, text):
        self.root = None
        self.clips = {}
        for line in text.splitlines():
            p = line.split()
            if not p:
                continue
            if p[0] == 'ERROR':
                raise RuntimeError(line)
            if p[0] == 'ROOT':
                self.root = int(p[1])
            elif p[0] == 'S':
                self.clips[int(p[1])] = [dict() for _ in range(int(p[2]))]
            elif p[0] == 'P':
                cid, frame, depth = int(p[1]), int(p[2]), int(p[3])
                self.clips[cid][frame - 1][depth] = dict(
                    cid=int(p[4]), clip=p[5] == '1', name=None if p[6] == '-' else p[6],
                    tx=int(p[7]), ty=int(p[8]))
        if self.root is None:
            raise RuntimeError('no symbol tree')

    def child(self, clip, name, frame=1):
        for depth, e in self.clips[clip][frame - 1].items():
            if e['name'] == name:
                return e
        raise KeyError('%s has no child %s' % (clip, name))

    def frames(self, clip):
        return len(self.clips[clip])

    def depths(self, clip, frame):
        return self.clips[clip][frame - 1]

    def guides(self, clip, frame):
        # Hit-shape guides ("shape", removed by NPCSprite.removeShapes) and the empty spineRef.
        return [d for d, e in self.depths(clip, frame).items() if e['name'] in ('shape', 'spineRef')]

    def named_only(self, clip, frame, name):
        return [d for d, e in self.depths(clip, frame).items() if e['name'] != name]


def find_parts(tree):
    r = tree.root
    parts = {
        'head': tree.child(tree.child(r, 'headOuter')['cid'], 'head')['cid'],
        'chest': tree.child(r, 'chest')['cid'],
        'pelvis': tree.child(r, 'pelvis')['cid'],
    }
    anchors = {}
    for i in '12':
        arm = tree.child(r, 'arm' + i)['cid']
        leg = tree.child(r, 'leg' + i)['cid']
        ua = tree.child(arm, 'upperArm' + i)
        ul = tree.child(leg, 'upperLeg' + i)
        parts['upperArm' + i] = ua['cid']
        parts['upperLeg' + i] = ul['cid']
        parts['lowerArm' + i] = tree.child(tree.child(arm, 'lowerArmOuter' + i)['cid'], 'lowerArm' + i)['cid']
        parts['lowerLeg' + i] = tree.child(tree.child(leg, 'lowerLegOuter' + i)['cid'], 'lowerLeg' + i)['cid']
        # Shoulder / hip joint (= arm/leg clip origin) in upper-limb coordinates, NPC pixels.
        anchors['upperArm' + i] = (-ua['tx'] / 20.0, -ua['ty'] / 20.0)
        anchors['upperLeg' + i] = (-ul['tx'] / 20.0, -ul['ty'] / 20.0)
    return parts, anchors


def foot_depth(tree, lower_leg):
    """The shoe: an unnamed shape kept unchanged between the intact and broken lower leg."""
    f1, f2 = tree.depths(lower_leg, 1), tree.depths(lower_leg, 2)
    for d, e in f1.items():
        if e['name'] is None and d in f2 and f2[d]['cid'] == e['cid']:
            return d
    raise RuntimeError('no foot in clip %d' % lower_leg)


def run_java(java, ffdec, swf, commands, workdir):
    path = os.path.join(workdir, 'commands.txt')
    with open(path, 'w') as f:
        f.write('\n'.join(commands) + '\n')
    lib = os.path.join(ffdec, 'lib')
    cp = os.pathsep.join([os.path.join(lib, 'ffdec_lib.jar'), os.path.join(lib, '*')])
    res = subprocess.run([java, '-Djava.awt.headless=true', '-cp', cp, JAVA_HELPER, swf, path],
                         capture_output=True, text=True, timeout=900)
    if res.returncode != 0:
        raise RuntimeError('FFDec helper failed:\n' + res.stdout[-2000:] + res.stderr[-4000:])
    return res.stdout


# ------------------------------------------------------------------------------------------
# Image helpers. A Layer is an RGBA float array (straight alpha, 0..1) plus the pixel position
# of its registration point; all renders of one clip share size and origin.
# ------------------------------------------------------------------------------------------

class Layer:
    def __init__(self, rgba, origin):
        self.a = rgba
        self.origin = origin  # (x, y) in pixels, may be fractional

    def copy(self):
        return Layer(self.a.copy(), self.origin)

    @property
    def alpha(self):
        return self.a[:, :, 3]


def load_layer(path, origin):
    im = Image.open(path).convert('RGBA')
    return Layer(np.asarray(im).astype(np.float32) / 255.0, origin)


def over(dst, src):
    """Alpha-composite src over dst (same shape, straight alpha)."""
    sa = src[:, :, 3:4]
    da = dst[:, :, 3:4]
    oa = sa + da * (1 - sa)
    rgb = np.where(oa > 0, (src[:, :, :3] * sa + dst[:, :, :3] * da * (1 - sa)) / np.maximum(oa, 1e-6), 0)
    return np.concatenate([rgb, oa], axis=2)


def shift(arr, dx, dy):
    """Integer translation, zero fill."""
    out = np.zeros_like(arr)
    h, w = arr.shape[:2]
    xs, xd = (0, dx) if dx >= 0 else (-dx, 0)
    ys, yd = (0, dy) if dy >= 0 else (-dy, 0)
    cw, ch = w - abs(dx), h - abs(dy)
    if cw > 0 and ch > 0:
        out[yd:yd + ch, xd:xd + cw] = arr[ys:ys + ch, xs:xs + cw]
    return out


def shift_into(arr, shape, dx, dy):
    """arr moved by (dx, dy) integer pixels into a zero canvas of `shape`."""
    big = np.zeros((max(shape[0], arr.shape[0]), max(shape[1], arr.shape[1]), shape[2]), arr.dtype)
    big[:arr.shape[0], :arr.shape[1]] = arr
    return shift(big, dx, dy)[:shape[0], :shape[1]]


def to_pil_premul(a):
    pm = a.copy()
    pm[:, :, :3] *= pm[:, :, 3:4]
    return Image.fromarray(np.clip(pm * 255 + 0.5, 0, 255).astype(np.uint8), 'RGBa')


def from_pil_premul(im):
    a = np.asarray(im.convert('RGBa')).astype(np.float32) / 255.0
    al = a[:, :, 3:4]
    a[:, :, :3] = np.where(al > 0, a[:, :, :3] / np.maximum(al, 1e-6), 0)
    return np.clip(a, 0, 1)


def warp(layer, size, scale, origin_out, angle=0.0, offset=(0.0, 0.0)):
    """Resample layer into a canvas of `size` so that its registration point lands on
    origin_out (+offset), scaled by `scale` and rotated by `angle` degrees around it."""
    im = to_pil_premul(layer.a)
    ox, oy = layer.origin
    # Pre-shrink by an integer factor with a box filter (proper area averaging), then finish
    # with a bicubic affine map.
    r = 1
    while scale * r * 2 <= 1.0 + 1e-9:
        r *= 2
    if r > 1:
        w, h = im.size
        pw, ph = (-w) % r, (-h) % r
        if pw or ph:
            padded = Image.new('RGBa', (w + pw, h + ph))
            padded.paste(im, (0, 0))
            im = padded
        im = im.reduce(r)
        ox, oy, scale = ox / r, oy / r, scale * r
    c, s = math.cos(math.radians(angle)), math.sin(math.radians(angle))
    cx, cy = origin_out[0] + offset[0], origin_out[1] + offset[1]
    k = 1.0 / scale
    # output -> input: p_in = R(-angle) * (p_out - c) / scale + o
    data = (c * k, s * k, ox - (c * cx + s * cy) * k,
            -s * k, c * k, oy - (-s * cx + c * cy) * k)
    if abs(scale - 1.0) < 1e-9 and angle == 0.0 and float(data[2]).is_integer() and float(data[5]).is_integer():
        out = Image.new('RGBa', size)
        out.paste(im, (-int(data[2]), -int(data[5])))
    else:
        out = im.transform(size, Image.AFFINE, data, resample=Image.BICUBIC)
    return from_pil_premul(out)


def bbox(alpha, thr=1.5 / 255):
    ys, xs = np.where(alpha > thr)
    if len(xs) == 0:
        return None
    return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1


def rows_x_centre(alpha, y0, y1):
    y0, y1 = max(0, y0), min(alpha.shape[0], y1)
    ys, xs = np.where(alpha[y0:y1] > 0.5)
    return float(xs.mean()) if len(xs) else None


# ------------------------------------------------------------------------------------------
# Mobile side (the player's Android assets): existing kid canvases, dad's generic frames
# ------------------------------------------------------------------------------------------

def parse_nums(s):
    return [float(x) for x in re.findall(r'-?[\d.]+', s)]


def load_sheet(assets, tier, name):
    plist = os.path.join(assets, tier, 'characters', name + '.plist')
    png = os.path.join(assets, tier, 'characters', name + '.png')
    if not (os.path.isfile(plist) and os.path.isfile(png)):
        return None
    with open(plist, 'rb') as f:
        d = plistlib.load(f)
    tex = Image.open(png).convert('RGBA')
    frames = {}
    for k, v in d['frames'].items():
        x, y, w, h = map(int, parse_nums(v['textureRect']))
        if v.get('textureRotated'):
            crop = tex.crop((x, y, x + h, y + w)).transpose(Image.ROTATE_90)
        else:
            crop = tex.crop((x, y, x + w, y + h))
        sw, sh = map(int, parse_nums(v['spriteSourceSize']))
        ox, oy = parse_nums(v['spriteOffset'])
        canvas = Image.new('RGBA', (sw, sh))
        canvas.alpha_composite(crop, (int(round((sw - w) / 2 + ox)), int(round((sh - h) / 2 - oy))))
        frames[k] = dict(img=crop, src=(sw, sh), canvas=canvas)
    return frames


# ------------------------------------------------------------------------------------------
# The pipeline
# ------------------------------------------------------------------------------------------

PART_REF = {  # existing kid frame that defines a part's canvas (and its alignment reference)
    'head': 'head_1', 'chest': 'chest_1', 'pelvis': 'pelvis',
    'upperArm1': 'upperArm1_1', 'upperArm2': 'upperArm2_1', 'lowerArm1': 'lowerArm1_1',
    'lowerArm2': 'lowerArm2_1', 'upperLeg1': 'upperLeg1_1', 'upperLeg2': 'upperLeg2_1',
    'lowerLeg1': 'lowerLeg1_1', 'lowerLeg2': 'lowerLeg2_1',
}


def render_all(args, tree, parts, workdir):
    jobs = {}  # key -> (clip, frame, ignored depths)

    def add(key, clip, frame, ignore):
        jobs[key] = (clip, frame, sorted(set(ignore)))

    for p, clip in parts.items():
        for f in range(1, tree.frames(clip) + 1):
            add('%s@%d' % (p, f), clip, f, tree.guides(clip, f))
    ch, pv, hd = parts['chest'], parts['pelvis'], parts['head']
    add('neck', ch, 2, tree.named_only(ch, 2, 'neck'))
    add('shoulder_wound', ch, 3, tree.named_only(ch, 3, 'wound'))
    add('pelvisWound', pv, 2, tree.named_only(pv, 2, 'wound'))
    for i in range(1, 5):
        add('headChunk_%d' % i, hd, 3, tree.named_only(hd, 3, 'chunk%d' % i))
        add('chestChunk_%d' % i, ch, 9, tree.named_only(ch, 9, 'chunk%d' % i))
    for i in range(1, 4):
        add('pelvisChunk_%d' % i, pv, 5, tree.named_only(pv, 5, 'chunk%d' % i))
    add('brain', hd, 3, tree.named_only(hd, 3, 'brain'))
    add('heart', ch, 9, tree.named_only(ch, 9, 'heart'))
    for i in '12':
        ll = parts['lowerLeg' + i]
        fd = foot_depth(tree, ll)
        for f in (1, 2):
            add('lowerLeg%s@%d-nofoot' % (i, f), ll, f, tree.guides(ll, f) + [fd])
        if i == '1':
            add('foot', ll, 1, [d for d in tree.depths(ll, 1) if d != fd])

    cmds, files = [], {}
    for key, (clip, frame, ignore) in jobs.items():
        out = os.path.join(workdir, 'r', re.sub(r'[^A-Za-z0-9_.-]', '_', key) + '.png')
        files[out.replace('\\', '/')] = key
        cmds.append('render %s %d %d %g %s' % (out.replace('\\', '/'), clip, frame, MASTER,
                                              ','.join(map(str, ignore)) or '-'))
    text = run_java(args.java, args.ffdec, args.swf, cmds, workdir)
    layers = {}
    for line in text.splitlines():
        p = line.split()
        if p and p[0] == 'R':
            layers[files[p[1]]] = load_layer(p[1], (int(p[2]), int(p[3])))
    missing = set(jobs) - set(layers)
    if missing:
        raise RuntimeError('renders missing: %s' % sorted(missing))
    return layers


def fit_alignment(layer, ref_canvas):
    """Rigid correction (angle, dx, dy in large-tier pixels) that best lines the NPC render of a
    part up with the kid's existing mobile frame (both centred on the registration point)."""
    W = H = 256
    ref = np.zeros((H, W), bool)
    rc = np.asarray(ref_canvas)[:, :, 3] > 64
    rh, rw = rc.shape
    y0, x0 = int(round(H / 2 - rh / 2)), int(round(W / 2 - rw / 2))
    ref[y0:y0 + rh, x0:x0 + rw] = rc
    centre = (x0 + rw / 2, y0 + rh / 2)
    cache = {}

    def mask(angle):
        if angle not in cache:
            cache[angle] = warp(layer, (W, H), 1.0 / MASTER_TO_LARGE, centre, angle)[:, :, 3] > 0.25
        return cache[angle]

    def score(angle, dx, dy):
        m = shift(mask(angle), dx, dy)
        return (m & ref).sum() / max(1, (m | ref).sum())

    base = score(0.0, 0, 0)
    best = (base, 0.0, 0, 0)
    for angle in [float(a) for a in range(-15, 16, 3)]:
        for dx in range(-6, 7):
            for dy in range(-6, 7):
                v = score(angle, dx, dy)
                if v > best[0]:
                    best = (v, angle, dx, dy)
    v, a0, dx0, dy0 = best
    for angle in [a0 + d * 0.5 for d in range(-5, 6)]:
        for dx in range(dx0 - 1, dx0 + 2):
            for dy in range(dy0 - 1, dy0 + 2):
                v = score(angle, dx, dy)
                if v > best[0]:
                    best = (v, angle, dx, dy)
    return base, best


def hem_row(layer):
    """First row (from the top) below which the limb is plain skin: the sleeve / shorts hem.
    The skin tone is taken from the limb's lowest rows (far limbs are drawn darker)."""
    a = layer.a
    opaque = a[:, :, 3] > 0.5
    rows = np.where(opaque.any(axis=1))[0]
    top, bottom = rows.min(), rows.max()
    low = opaque.copy()
    low[:bottom - (bottom - top) // 8] = False
    skin = np.median(a[low][:, :3], axis=0)
    is_skin = (np.abs(a[:, :, :3] - skin).sum(axis=2) < 0.25) & opaque
    cut = bottom + 1
    for y in range(bottom, top - 1, -1):
        if is_skin[y].sum() < 0.5 * max(1, opaque[y].sum()):
            break
        cut = y
    return cut, top, bottom


def gore_mask(a):
    """Blood, flesh and bone colours (red/dark red/brown, or white) - not skin, cloth or shoes."""
    r, g, b = a[:, :, 0], a[:, :, 1], a[:, :, 2]
    red = (r > 0.3) & (g < 0.35) & (b < 0.35) & (r > g + 0.15)
    bone = (r > 0.85) & (g > 0.85) & (b > 0.85)
    m = (red | bone) & (a[:, :, 3] > 0.02)
    # Grow by a pixel to keep the anti-aliased rims.
    grown = m.copy()
    grown[1:] |= m[:-1]
    grown[:-1] |= m[1:]
    grown[:, 1:] |= m[:, :-1]
    grown[:, :-1] |= m[:, 1:]
    return grown


def gore_cap(broken, intact, top_half):
    """The wound of `broken`: its blood/bone pixels that `intact` doesn't have, in one half."""
    d = (np.abs(broken.a - intact.a).sum(axis=2) > 0.15) & gore_mask(broken.a)
    rows = np.where(intact.alpha.any(axis=1) | broken.alpha.any(axis=1))[0]
    mid = (rows.min() + rows.max()) // 2
    if top_half:
        d[mid:] = False
    else:
        d[:mid] = False
    cap = broken.a.copy()
    cap[~d] = 0
    return cap


def compose(layers, parts, anchors):
    """Returns {frame name: (Layer, part or None)}; part-space frames carry their part's name
    (canvas + alignment), None marks trimmed frames."""
    out = {}
    L = layers

    out['head_2'] = (L['head@2'], 'head')
    out['chest_2'] = (L['chest@5'], 'chest')
    out['neck'] = (L['neck'], 'chest')
    out['shoulder_wound'] = (L['shoulder_wound'], 'chest')
    out['pelvisWound'] = (L['pelvisWound'], 'pelvis')
    hip = L['pelvis@3'].copy()
    diff = (np.abs(L['pelvis@3'].a - L['pelvis@1'].a).sum(axis=2) > 0.15) & (hip.alpha > 0.02)
    hip.a[~diff] = 0
    out['hipWound'] = (hip, 'pelvis')
    for i in '12':
        out['lowerArm%s_2' % i] = (L['lowerArm%s@2' % i], 'lowerArm' + i)
        out['lowerLeg%s_2' % i] = (L['lowerLeg%s@2' % i], 'lowerLeg' + i)

    for limb in ('upperArm', 'upperLeg'):
        for i in '12':
            p = limb + i
            f1, f2, f3, f4 = (L['%s@%d' % (p, f)] for f in (1, 2, 3, 4))
            out[p + '_4'] = (f2, p)
            out[p + '_5'] = (f3, p)
            out[p + '_7'] = (f4, p)
            cut, top, bottom = hem_row(f1)
            top_cap = gore_cap(f2, f1, True)      # shoulder / hip wound
            bottom_cap = gore_cap(f3, f1, False)  # elbow / knee wound
            xc_cut = rows_x_centre(f1.alpha, cut - 3, cut + 4)
            xc_top = rows_x_centre(f1.alpha, top, top + 7)
            xc_bot = rows_x_centre(f1.alpha, bottom - 6, bottom + 1)
            down = shift(top_cap, int(round(xc_cut - xc_top)), cut - top)
            up = shift(bottom_cap, int(round(xc_cut - xc_bot)), cut - bottom)
            # Lower piece (flies off): the limb below the hem, wound on top.
            for name, src in ((p + '_3', f1), (p + '_6', f3)):
                a = src.a.copy()
                a[:cut] = 0
                out[name] = (Layer(over(a, down), src.origin), p)
            # Upper piece (stays on the shoulder / hip joint, which is its body origin).
            ax, ay = anchors[p]
            for name, src in ((p + '_2', f1), (p + '_8', f2)):
                a = src.a.copy()
                a[cut:] = 0
                layer = Layer(over(a, up), src.origin)
                layer.anchor = (src.origin[0] + ax * MASTER, src.origin[1] + ay * MASTER)
                out[name] = (layer, p)

    # Foot torn off: the leg without the shoe, with the kid's own elbow wound (the upper arm's,
    # about as wide as the ankle) at the ankle.
    ankle = gore_cap(L['upperArm1@3'], L['upperArm1@1'], False)
    arm = L['upperArm1@1']
    arm_rows = np.where(arm.alpha.max(axis=1) > 0.5)[0]
    arm_end = arm_rows.max() + 1
    arm_xc = rows_x_centre(arm.alpha, arm_end - 7, arm_end)
    for i in '12':
        p = 'lowerLeg' + i
        for state, src in (('3', L[p + '@1-nofoot']), ('4', L[p + '@2-nofoot'])):
            r = np.where(src.alpha.max(axis=1) > 0.5)[0]
            end = r.max() + 1
            xc_end = rows_x_centre(src.alpha, end - 7, end)
            # Arm-canvas pixels -> leg-canvas pixels: the arm's lower end onto the ankle.
            cap = shift_into(ankle, src.a.shape, int(round(xc_end - arm_xc)), end - arm_end)
            out['%s_%s' % (p, state)] = (Layer(over(src.a.copy(), cap), src.origin), p)

    for key in ['brain', 'heart', 'foot'] + ['headChunk_%d' % i for i in range(1, 5)] + \
               ['chestChunk_%d' % i for i in range(1, 5)] + ['pelvisChunk_%d' % i for i in range(1, 4)]:
        out[key] = (L[key], None)
    return out


def apply_correction(layer, corr):
    """Rotates/moves a part-space layer by the part's alignment correction (master pixels)."""
    angle, dx, dy = corr
    if angle == 0 and dx == 0 and dy == 0:
        return layer
    h, w = layer.a.shape[:2]
    pad = 24
    size = (w + 2 * pad, h + 2 * pad)
    origin = (layer.origin[0] + pad, layer.origin[1] + pad)
    a = warp(layer, size, 1.0, origin, angle, (dx * MASTER_TO_LARGE, dy * MASTER_TO_LARGE))
    out = Layer(a, origin)
    if hasattr(layer, 'anchor'):
        # Same rigid map for the anchor point.
        vx, vy = layer.anchor[0] - layer.origin[0], layer.anchor[1] - layer.origin[1]
        c, s = math.cos(math.radians(angle)), math.sin(math.radians(angle))
        rx, ry = c * vx - s * vy, s * vx + c * vy
        out.anchor = (origin[0] + rx + dx * MASTER_TO_LARGE, origin[1] + ry + dy * MASTER_TO_LARGE)
    return out


def tier_frame(layer, part, tier, scale, kid_frames):
    """Image + (size, sourceSize, offset) of one frame for one tier."""
    s = scale / MASTER
    if part is None:
        # Trimmed, no offset: render with margin, then crop to the content.
        h, w = layer.a.shape[:2]
        size = (int(math.ceil(w * s)) + 4, int(math.ceil(h * s)) + 4)
        a = warp(layer, size, s, (layer.origin[0] * s + 2, layer.origin[1] * s + 2))
        b = bbox(a[:, :, 3])
        a = a[b[1]:b[3], b[0]:b[2]]
        return a, (a.shape[1], a.shape[0]), (0, 0)
    ref = kid_frames.get('%s_%s.png' % (PREFIX, PART_REF[part])) if kid_frames else None
    if ref is not None:
        W, H = ref['src']
    else:
        h, w = layer.a.shape[:2]
        W = int(2 * math.ceil(max(layer.origin[0], w - layer.origin[0]) * s)) + 2
        H = int(2 * math.ceil(max(layer.origin[1], h - layer.origin[1]) * s)) + 2
    src = layer
    if hasattr(layer, 'anchor'):  # canvas centred on the joint the piece hangs from
        src = Layer(layer.a, layer.anchor)
    # Margin so content outside the reference canvas is kept (the offset then points outside).
    m = 64
    a = warp(src, (W + 2 * m, H + 2 * m), s, (W / 2 + m, H / 2 + m))
    b = bbox(a[:, :, 3])
    if b is None:
        raise RuntimeError('empty frame')
    x0, y0, x1, y1 = b
    crop = a[y0:y1, x0:x1]
    off = ((x0 + x1) / 2 - m - W / 2, -((y0 + y1) / 2 - m - H / 2))
    return crop, (W, H), off


def pack(images, pad=2, max_width=512):
    """Shelf packer: returns positions {name: (x, y)} and the sheet size."""
    order = sorted(images, key=lambda k: (-images[k].shape[0], k))
    widest = max(images[k].shape[1] for k in order)
    width = max(max_width, widest + 2 * pad)
    x = y = pad
    shelf = 0
    pos = {}
    for k in order:
        h, w = images[k].shape[:2]
        if x + w + pad > width:
            x = pad
            y += shelf + pad
            shelf = 0
        pos[k] = (x, y)
        x += w + pad
        shelf = max(shelf, h)
    used_w = max(pos[k][0] + images[k].shape[1] for k in order) + pad
    return pos, (used_w, y + shelf + pad)


def fmt(v):
    return ('%d' % v) if float(v).is_integer() else ('%g' % v)


def write_sheet(frames, out_dir):
    images = {k: v[0] for k, v in frames.items()}
    pos, (W, H) = pack(images)
    sheet = np.zeros((H, W, 4), np.float32)
    meta = {}
    for k, (img, src, off) in frames.items():
        x, y = pos[k]
        h, w = img.shape[:2]
        sheet[y:y + h, x:x + w] = img
        meta[k] = {
            'aliases': [],
            'spriteOffset': '{%s,%s}' % (fmt(off[0]), fmt(off[1])),
            'spriteSize': '{%d,%d}' % (w, h),
            'spriteSourceSize': '{%d,%d}' % src,
            'textureRect': '{{%d,%d},{%d,%d}}' % (x, y, w, h),
            'textureRotated': False,
        }
    os.makedirs(out_dir, exist_ok=True)
    rgba = np.clip(sheet * 255 + 0.5, 0, 255).astype(np.uint8)
    rgba[rgba[:, :, 3] == 0] = 0
    # Straight (non-premultiplied) alpha like the original sheets; cocos2d-x premultiplies on load.
    Image.fromarray(rgba, 'RGBA').save(os.path.join(out_dir, SHEET + '.png'), optimize=True)
    plist = {
        'frames': {k: meta[k] for k in sorted(meta)},
        'metadata': {
            'format': 3,
            'pixelFormat': 'RGBA8888',
            'premultiplyAlpha': False,
            'realTextureFileName': SHEET + '.png',
            'size': '{%d,%d}' % (W, H),
            'textureFileName': SHEET + '.png',
        },
    }
    with open(os.path.join(out_dir, SHEET + '.plist'), 'wb') as f:
        plistlib.dump(plist, f, sort_keys=False)
    return W, H


def stamp_for(args):
    h = hashlib.sha256()
    h.update(VERSION.encode())
    for p in (__file__, JAVA_HELPER):
        with open(p, 'rb') as f:
            h.update(f.read())
    st = os.stat(args.swf)
    h.update(('%d|%d' % (st.st_size, int(st.st_mtime))).encode())
    for tier in TIERS:
        p = os.path.join(args.assets, tier, 'characters', PREFIX + '_sprites.plist')
        if os.path.isfile(p):
            h.update(('%s|%d' % (tier, os.stat(p).st_size)).encode())
    return h.hexdigest()


def find_java(explicit):
    if explicit:
        return explicit
    home = os.environ.get('JAVA_HOME')
    if home:
        for n in ('java.exe', 'java'):
            p = os.path.join(home, 'bin', n)
            if os.path.isfile(p):
                return p
    return shutil.which('java')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--swf', default=DEFAULT_SWF, help='decrypted Happy Wheels game SWF (v1.87)')
    ap.add_argument('--ffdec', default=DEFAULT_FFDEC, help='JPEXS FFDec folder (with lib/ffdec_lib.jar)')
    ap.add_argument('--assets', default=DEFAULT_ASSETS, help="the Android game's assets/ folder")
    ap.add_argument('--out', required=True, help='output root; sheets go to <out>/<tier>/characters/')
    ap.add_argument('--java', default=None, help='java executable (default: JAVA_HOME or PATH)')
    ap.add_argument('--work', default=None, help='scratch folder (default: <out>/.kid_gore_work)')
    ap.add_argument('--optional', action='store_true', help='exit 0 when inputs are missing')
    ap.add_argument('--force', action='store_true', help='ignore the up-to-date stamp')
    ap.add_argument('--keep-work', action='store_true', help='keep the intermediate renders')
    args = ap.parse_args()

    def give_up(msg):
        if args.optional:
            log('skipped (%s); the kid keeps the mobile gore-free art' % msg)
            return 0
        log('error: ' + msg)
        return 1

    if np is None:
        return give_up('Python packages numpy and Pillow are required')
    if not os.path.isfile(args.swf):
        return give_up('game SWF not found at %s' % args.swf)
    if not os.path.isfile(os.path.join(args.ffdec, 'lib', 'ffdec_lib.jar')):
        return give_up('FFDec not found at %s' % args.ffdec)
    args.java = find_java(args.java)
    if not args.java:
        return give_up('java not found')
    if not os.path.isdir(args.assets):
        return give_up('Android assets not found at %s' % args.assets)

    stamp_path = os.path.join(args.out, '.kid_gore_stamp')
    stamp = stamp_for(args)
    outputs = [os.path.join(args.out, t, 'characters', SHEET + '.plist') for t in TIERS]
    if not args.force and all(os.path.isfile(p) for p in outputs) and os.path.isfile(stamp_path):
        with open(stamp_path) as f:
            if f.read().strip() == stamp:
                log('up to date')
                return 0

    work = args.work or os.path.join(args.out, '.kid_gore_work')
    os.makedirs(os.path.join(work, 'r'), exist_ok=True)
    try:
        tree = FlashTree(run_java(args.java, args.ffdec, args.swf, ['tree ' + KID_SYMBOL], work))
        parts, anchors = find_parts(tree)
        log('kid symbol %d: %s' % (tree.root, ' '.join('%s=%d' % kv for kv in parts.items())))
        layers = render_all(args, tree, parts, work)
    except Exception as e:  # noqa: BLE001 - report and (optionally) skip
        return give_up('rendering failed: %s' % e)

    kid = {t: load_sheet(args.assets, t, PREFIX + '_sprites') for t in TIERS}
    dad = {t: load_sheet(args.assets, t, 'irresponsible_dad_sprites') for t in TIERS}

    # Line every part up with the kid's existing frame (the NPC head, e.g., is tilted ~11 deg).
    corrections = {}
    for p in PART_REF:
        ref = (kid['large'] or {}).get('%s_%s.png' % (PREFIX, PART_REF[p]))
        if ref is None:
            corrections[p] = (0.0, 0, 0)
            continue
        base, (v, angle, dx, dy) = fit_alignment(layers[p + '@1'], ref['canvas'])
        use = v - base > 0.01
        corrections[p] = (angle, dx, dy) if use else (0.0, 0, 0)
        log('align %-10s IoU %.3f -> %.3f  (%s)' % (
            p, base, v, 'rotate %.1f deg, move %d,%d px' % (angle, dx, dy) if use else 'as is'))

    composed = compose(layers, parts, anchors)
    for k, (layer, part) in list(composed.items()):
        if part is not None:
            composed[k] = (apply_correction(layer, corrections[part]), part)

    for tier, scale in TIERS.items():
        frames = {}
        for k, (layer, part) in composed.items():
            frames['%s_%s.png' % (PREFIX, k)] = tier_frame(layer, part, tier, scale, kid[tier])
        # Intestine and spine: generic tubes; the dad's frames, thinner (kid ~3/4 his size; the
        # intestine sprite is stretched along its segment anyway).
        if dad[tier]:
            for name, sx, sy in (('intestine', 1.0, 0.75), ('spine', 0.75, 0.75)):
                src = dad[tier].get('irresponsible_dad_%s.png' % name)
                if src is None:
                    continue
                im = src['img']
                size = (max(1, int(round(im.width * sx))), max(1, int(round(im.height * sy))))
                pm = im.convert('RGBa').resize(size, Image.LANCZOS)
                a = from_pil_premul(pm)
                frames['%s_%s.png' % (PREFIX, name)] = (a, size, (0, 0))
        out_dir = os.path.join(args.out, tier, 'characters')
        W, H = write_sheet(frames, out_dir)
        log('%s: %d frames, %dx%d -> %s' % (tier, len(frames), W, H, out_dir))

    with open(stamp_path, 'w') as f:
        f.write(stamp + '\n')
    if not args.keep_work:
        shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
