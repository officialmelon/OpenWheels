#!/usr/bin/env python3
"""Main-menu portraits for the restored characters from their already-generated sprites.

tools/assets/extract_character.py draws each restored character's main-menu portraits
(<out>/<tier>/menus/main/portraits/char<id>_portrait.png and char<id>_portrait_25p.png) from the
player's browser-game SWFs with FFDec. Installs generated before it did (or without FFDec at the
time) have the restored characters but no portraits, and the main menu shows the generic one.
This script draws the same head-and-shoulders bust (extract_character.make_portrait) from what
is already generated: the character-select sprite sheets (<out>/<tier>/characters/
character_select_<name>_sprites.{plist,png}, the sharpest copy of the body parts) and the body
plist (<out>/shared/characters/bodies/<name>_<vehicle>.plist) for the rest pose. The parts are
scaled up from the sheet instead of re-rendered from vector art, so the result is a little
softer than extract_character's.

Only missing portraits are written (never over the SWF-rendered ones, unless --force). Needs numpy and Pillow;
with --optional, missing inputs only print a note and exit 0 (build hooks).

    restored_portraits.py --out <exe dir>/generated/restored [--optional] [--force]
"""
import argparse
import os
import plistlib
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import extract_character as ec  # noqa: E402  (portrait layout, SPECS, tier conventions)

kg = ec.kg
np = ec.np
Image = ec.Image


def log(msg):
    print('restored_portraits: ' + msg, flush=True)


def nums(s):
    return [float(v) for v in re.findall(r'-?[\d.]+', str(s))]


def load_frames(out, name):
    """{frame: (PIL RGBA image, registration point (x, y) in it)} and the sheet's tier px per
    symbol px, from the best generated sheet (character select, large first)."""
    for tier in ('large', 'medium', 'small', 'tiny'):
        for select in (True, False):
            sheet = os.path.join(out, tier, 'characters', ('character_select_' if select else '') + name + '_sprites')
            if not (os.path.isfile(sheet + '.plist') and os.path.isfile(sheet + '.png')):
                continue
            with open(sheet + '.plist', 'rb') as f:
                meta = plistlib.load(f)
            png = Image.open(sheet + '.png').convert('RGBA')
            frames = {}
            for key, m in meta.get('frames', {}).items():
                x, y, w, h = nums(m['textureRect'])
                ox, oy = nums(m['spriteOffset'])
                if m.get('textureRotated'):
                    continue  # (extract_character never rotates frames)
                im = png.crop((int(x), int(y), int(x + w), int(y + h)))
                # The canvas is centred on the registration point; the trimmed frame's centre sits
                # spriteOffset (y up) from it.
                frames[key] = (im, (w / 2.0 - ox, h / 2.0 + oy))
            return frames, ec.TIERS[tier] * (2 if select else 1)
    return None, None


class E:
    """A shapeGuide entry rebuilt from the body plist (symbol px, Flash y down / rotation)."""

    def __init__(self, tx, ty, rotation):
        self.tx, self.ty, self.rotation = tx, ty, rotation


def load_guide(out, name, vehicle):
    path = os.path.join(out, 'shared', 'characters', 'bodies', '%s_%s.plist' % (name, vehicle))
    if not os.path.isfile(path):
        return None
    with open(path, 'rb') as f:
        bodies = plistlib.load(f).get('bodies', {})
    guide = {}
    for key, body in bodies.items():
        if 'pos' in body:
            x, y = nums(body['pos'])
            # ec.to_mobile: metres = px * PHYS with y up; the plist rot is the Flash one negated
            guide[key] = E(x / ec.PHYS, -y / ec.PHYS, -float(body.get('rot', 0.0)))
    return guide


class SheetTree:
    """The bits of extract_character.SwfTree make_portrait asks for, for the sheet renderer."""

    def __init__(self, has_helmet):
        self.has_helmet = has_helmet

    def child(self, cid, name):
        return type('Clip', (), {'cid': name})()

    def has_child(self, cid, name):
        return name == 'helmet' and self.has_helmet

    def depths_named(self, cid, frame, names):
        return []

    def depths_except(self, cid, frame, name):
        return ['only:' + name]   # head without everything but the helmet = the helmet


class SheetRenderer:
    """extract_character.Renderer stand-in: the generated frames, scaled to the asked zoom."""

    FRAMES = {'head': 'head_1', 'chest': 'chest_1', 'pelvis': 'pelvis'}

    def __init__(self, frames, name, scale):
        self.frames, self.name, self.scale = frames, name, scale

    def add(self, cid, frame=1, ignore=(), zoom=ec.MASTER):
        return (cid, tuple(ignore), zoom)

    def run(self):
        pass

    def __getitem__(self, key):
        cid, ignore, zoom = key
        suffix = 'helmet' if ignore == ('only:helmet',) else self.FRAMES.get(cid, cid + '_1')
        im, (ox, oy) = self.frames['%s_%s.png' % (self.name, suffix)]
        k = zoom / self.scale
        size = (max(1, int(round(im.width * k))), max(1, int(round(im.height * k))))
        big = kg.to_pil_premul(np.asarray(im).astype(np.float32) / 255.0).resize(size, Image.LANCZOS)
        return kg.Layer(kg.from_pil_premul(big), (ox * k, oy * k))


def portrait_files(out, cid):
    return [os.path.join(out, tier, 'menus', 'main', 'portraits', 'char%d_%s.png' % (cid, kind))
            for tier in ec.PORTRAIT_TIER for kind in ('portrait', 'portrait_25p')]


def build(out, cid, spec):
    name = spec['name']
    frames, scale = load_frames(out, name)
    guide = load_guide(out, name, spec['vehicle'])
    if not frames or not guide:
        log('char%d (%s): sprites or body plist not generated, skipped' % (cid, name))
        return False
    R = SheetRenderer(frames, name, scale)
    plain = {}
    for suffix in ('head_1', 'helmet'):
        if '%s_%s.png' % (name, suffix) in frames:
            plain[suffix] = R[('head', ('only:helmet',) if suffix == 'helmet' else (), ec.MASTER)]
    tree = SheetTree('helmet' in plain)
    portrait = ec.make_portrait(tree, R, plain, guide)
    # A light unsharp mask makes up a little for the upscaling.
    from PIL import ImageFilter
    im = kg.to_pil_premul(portrait).filter(ImageFilter.UnsharpMask(radius=3, percent=60, threshold=2))
    ec.write_portraits(out, cid, kg.from_pil_premul(im), overwrite=False)
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', required=True, help='the generated restored folder (<exe dir>/generated/restored)')
    ap.add_argument('--optional', action='store_true', help='exit 0 when inputs are missing')
    ap.add_argument('--force', action='store_true',
                    help='redraw every portrait, also over the ones extract_character.py drew')
    args = ap.parse_args()

    def give_up(msg):
        log(('skipped (%s)' if args.optional else 'error: %s') % msg)
        return 0 if args.optional else 1

    if np is None or Image is None:
        return give_up('Python packages numpy and Pillow are required')
    listing = os.path.join(args.out, 'shared', 'Characters_restored.plist')
    if not os.path.isfile(listing):
        return give_up('no restored characters in %s' % args.out)
    with open(listing, 'rb') as f:
        ids = [e['id'] for e in plistlib.load(f) if isinstance(e, dict) and 'id' in e]
    done = []
    for cid in ids:
        spec = ec.SPECS.get(cid)
        if spec is None:
            continue
        files = portrait_files(args.out, cid)
        if all(os.path.isfile(p) for p in files) and not args.force:
            continue
        if args.force:
            for p in files:
                if os.path.isfile(p):
                    os.remove(p)
        try:
            if build(args.out, cid, spec):
                done.append(str(cid))
        except Exception as e:  # noqa: BLE001
            log('char%d: failed (%s)' % (cid, e))
    log('portraits drawn for %s' % (', '.join(done) or 'none (all present)'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
