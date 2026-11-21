#!/usr/bin/env python3
"""OpenWheels' own gameplay button icons, drawn in the style of the game's touch controls.

The game's buttons (controls_gameplay_btn_*.png) are a ring and a pictogram in white at 40 % opacity
over a disc of black at 60 %. The extra Shift / Ctrl actions of the characters (straight legs,
squat, jump + pose, fist and wheel poses, jet tilt, eject the girl / kid, brake, switch camera)
had no art and used a text label on the generic jump button. This draws one icon for each, with
the same ring, disc, stick-figure proportions and opacity, so they sit with the originals.

The artwork is original (shapes drawn here, nothing copied from the game's files). Output, per asset
tier (large / medium / small / tiny, the sizes of controls_gameplay_btn_*.png):

    <out>/<tier>/controls/ow_controls.png + ow_controls.plist     (cocos2d-x sprite sheet, format 3)

The game finds it through the restored characters' search paths (generated/restored/<tier>/), so use
    ow_button_icons.py --out <exe dir>/generated/restored
Run again any time: the sheets are rewritten. Needs Pillow. `--preview file.png` also writes a
contact sheet of every icon at the large size.
"""
import argparse
import math
import os
import plistlib
import sys

from PIL import Image, ImageDraw

# Frame size of a gameplay button per asset tier (controls_gameplay_btn_jump.png of the game).
TIERS = {"large": (328, 325), "medium": (165, 163), "small": (124, 122), "tiny": (83, 81)}

SS = 4  # supersampling
RING_ALPHA = 102  # 40 %
DISC_ALPHA = 153  # 60 %

# All geometry is in a unit square; the disc has radius 0.37 around (0.5, 0.5), the ring 0.5.
DISC_R = 0.37
RING_R = 0.5
LIMB = 0.052
HEAD = 0.056


class Canvas:
    """A glyph mask (L mode, white = drawn) in unit coordinates."""

    def __init__(self, size):
        self.w, self.h = size[0] * SS, size[1] * SS
        self.mask = Image.new("L", (self.w, self.h), 0)
        self.d = ImageDraw.Draw(self.mask)

    def px(self, p):
        return (p[0] * self.w, p[1] * self.h)

    def r(self, v):  # length (unit of width) -> pixels
        return v * self.w

    def circle(self, c, radius, fill=255):
        x, y = self.px(c)
        rr = self.r(radius)
        self.d.ellipse((x - rr, y - rr, x + rr, y + rr), fill=fill)

    def ring(self, c, radius, width):
        self.circle(c, radius, 255)
        self.circle(c, radius - width, 0)

    def line(self, a, b, width=LIMB, round_caps=True):
        ax, ay = self.px(a)
        bx, by = self.px(b)
        w = self.r(width)
        self.d.line((ax, ay, bx, by), fill=255, width=max(1, int(w)))
        if round_caps:
            for x, y in ((ax, ay), (bx, by)):
                self.d.ellipse((x - w / 2, y - w / 2, x + w / 2, y + w / 2), fill=255)

    def poly(self, pts, fill=255):
        self.d.polygon([self.px(p) for p in pts], fill=fill)

    def rect(self, a, b, fill=255):
        ax, ay = self.px(a)
        bx, by = self.px(b)
        self.d.rectangle((ax, ay, bx, by), fill=fill)

    def rrect(self, a, b, radius, fill=255):
        ax, ay = self.px(a)
        bx, by = self.px(b)
        self.d.rounded_rectangle((ax, ay, bx, by), radius=self.r(radius), fill=fill)

    def arc(self, c, radius, start_deg, end_deg, width=LIMB):
        """A thick arc from start to end (degrees, clockwise from 3 o'clock, y down), as a polyline."""
        steps = max(8, int(abs(end_deg - start_deg) / 3))
        pts = []
        for i in range(steps + 1):
            a = math.radians(start_deg + (end_deg - start_deg) * i / steps)
            pts.append((c[0] + radius * math.cos(a), c[1] + radius * math.sin(a)))
        for a, b in zip(pts, pts[1:]):
            self.line(a, b, width)
        return pts

    def arrow_head(self, tip, direction_deg, length=0.11, half_width=0.075):
        a = math.radians(direction_deg)
        dx, dy = math.cos(a), math.sin(a)
        nx, ny = -dy, dx
        base = (tip[0] - dx * length, tip[1] - dy * length)
        self.poly([tip, (base[0] + nx * half_width, base[1] + ny * half_width),
                   (base[0] - nx * half_width, base[1] - ny * half_width)])


# ---- pictogram parts ---------------------------------------------------------------------------

def person(c, head, torso, arms, legs, head_r=HEAD, limb=LIMB):
    """head = centre, torso = (top, bottom), arms / legs = lists of (start, end) or polylines."""
    c.circle(head, head_r)
    c.line(torso[0], torso[1], limb * 1.15)
    for seg in arms + legs:
        pts = seg if isinstance(seg[0], tuple) and isinstance(seg[0][0], tuple) else [seg[0], seg[1]]
        for a, b in zip(pts, pts[1:]):
            c.line(a, b, limb)


def stand_figure(c, ox=0.0, oy=0.0, s=1.0):
    def P(x, y):
        return (0.5 + ox + (x - 0.5) * s, 0.5 + oy + (y - 0.5) * s)
    person(c, P(0.5, 0.275), (P(0.5, 0.33), P(0.5, 0.55)),
           [(P(0.5, 0.37), P(0.42, 0.52)), (P(0.5, 0.37), P(0.58, 0.52))],
           [(P(0.5, 0.55), P(0.46, 0.76)), (P(0.5, 0.55), P(0.54, 0.76))], head_r=HEAD * s, limb=LIMB * s)


def squat_figure(c, ox=0.0, oy=0.0, s=1.0):
    def P(x, y):
        return (0.5 + ox + (x - 0.5) * s, 0.5 + oy + (y - 0.5) * s)
    person(c, P(0.44, 0.30), (P(0.45, 0.36), P(0.5, 0.56)),
           [(P(0.47, 0.42), P(0.62, 0.47))],
           [[P(0.5, 0.56), P(0.66, 0.6), P(0.54, 0.76)]], head_r=HEAD * s, limb=LIMB * s)


def eject_symbol(c, ox=0.0, oy=0.0, s=1.0):
    """The game's eject mark: a triangle over a bar."""
    def P(x, y):
        return (0.5 + ox + (x - 0.5) * s, 0.5 + oy + (y - 0.5) * s)
    c.poly([P(0.5, 0.38), P(0.65, 0.52), P(0.35, 0.52)])
    c.rect(P(0.35, 0.56), P(0.65, 0.61))


def up_arrow(c, ox=0.0, oy=0.0, s=1.0):
    def P(x, y):
        return (0.5 + ox + (x - 0.5) * s, 0.5 + oy + (y - 0.5) * s)
    c.poly([P(0.5, 0.3), P(0.62, 0.44), P(0.555, 0.44), P(0.555, 0.58), P(0.445, 0.58), P(0.445, 0.44), P(0.38, 0.44)])
    c.rect(P(0.4, 0.62), P(0.6, 0.67))


def wheel(c, centre, radius, spokes=6, width=0.045):
    c.ring(centre, radius, width)
    c.circle(centre, radius * 0.18)
    for i in range(spokes):
        a = math.radians(360 * i / spokes + 15)
        c.line(centre, (centre[0] + radius * 0.85 * math.cos(a), centre[1] + radius * 0.85 * math.sin(a)), width * 0.7)


# ---- the icons ---------------------------------------------------------------------------------

def icon_legs(c):  # straight legs
    stand_figure(c, oy=-0.01)


def icon_squat(c):
    squat_figure(c)


def icon_jump_legs(c):  # up arrow + straight-legged figure
    up_arrow(c, ox=-0.14, s=0.8)
    stand_figure(c, ox=0.15, oy=0.0, s=0.88)


def icon_jump_squat(c):
    up_arrow(c, ox=-0.14, s=0.8)
    squat_figure(c, ox=0.14, oy=0.0, s=0.9)


def fist_figure(c, left):
    f = -1 if left else 1
    def P(x, y):
        return (0.5 + (x - 0.5) * f, y)
    c.circle(P(0.46, 0.30), HEAD)
    c.line(P(0.52, 0.37), P(0.52, 0.58), LIMB * 1.15)
    c.line(P(0.52, 0.42), P(0.70, 0.38), LIMB)       # arm thrown out
    c.circle(P(0.74, 0.37), 0.062)                    # the fist
    c.line(P(0.52, 0.45), P(0.40, 0.55), LIMB)       # other arm
    c.line(P(0.52, 0.58), P(0.42, 0.76), LIMB)
    c.line(P(0.52, 0.58), P(0.62, 0.76), LIMB)


def icon_fist_l(c):
    fist_figure(c, True)


def icon_fist_r(c):
    fist_figure(c, False)


def wheel_pose(c, left):
    f = -1 if left else 1
    wheel(c, (0.5, 0.58), 0.19)
    # a turn arrow over the wheel
    cx = 0.5
    pts = c.arc((cx, 0.58), 0.26, 200 if left else 340, 285 if left else 255, width=0.045) if False else None
    sign = 1 if not left else -1
    start, end = (205, 335) if not left else (335, 205)
    c.arc((0.5, 0.58), 0.285, start, end, width=0.042)
    ang = end
    tip = (0.5 + 0.285 * math.cos(math.radians(ang)), 0.58 + 0.285 * math.sin(math.radians(ang)))
    direction = ang + (90 if end > start else -90)
    c.arrow_head(tip, direction, length=0.1, half_width=0.065)


def icon_wheel_l(c):
    wheel_pose(c, True)


def icon_wheel_r(c):
    wheel_pose(c, False)


def jet_body(c, cx=0.5, cy=0.5, s=1.0):
    def P(x, y):
        return (cx + (x - 0.5) * s, cy + (y - 0.5) * s)
    c.rrect(P(0.42, 0.44), P(0.60, 0.56), 0.02 * s)           # the jet's body
    c.poly([P(0.42, 0.45), P(0.30, 0.50), P(0.42, 0.55)])     # flame
    c.rect(P(0.62, 0.42), P(0.655, 0.58))


def jet_turn(c, cw):
    jet_body(c, 0.5, 0.5, 0.95)
    start, end = (200, 340) if cw else (340, 200)
    c.arc((0.5, 0.5), 0.265, start, end, width=0.04)
    tip = (0.5 + 0.265 * math.cos(math.radians(end)), 0.5 + 0.265 * math.sin(math.radians(end)))
    c.arrow_head(tip, end + (90 if end > start else -90), length=0.1, half_width=0.065)


def icon_jet_cw(c):
    jet_turn(c, True)


def icon_jet_ccw(c):
    jet_turn(c, False)


def icon_girl(c):  # eject + a girl (dress, pigtails)
    eject_symbol(c, ox=-0.13, oy=0.04, s=0.85)
    gx = 0.66
    c.circle((gx, 0.30), HEAD * 0.95)
    c.circle((gx - 0.07, 0.30), 0.027)
    c.circle((gx + 0.07, 0.30), 0.027)
    c.poly([(gx, 0.37), (gx + 0.095, 0.60), (gx - 0.095, 0.60)])  # dress
    c.line((gx - 0.03, 0.60), (gx - 0.03, 0.72), LIMB * 0.9)
    c.line((gx + 0.03, 0.60), (gx + 0.03, 0.72), LIMB * 0.9)
    c.line((gx - 0.03, 0.41), (gx - 0.11, 0.53), LIMB * 0.8)
    c.line((gx + 0.03, 0.41), (gx + 0.11, 0.53), LIMB * 0.8)


def icon_kid(c):  # eject + a small kid on a seat
    eject_symbol(c, ox=-0.13, oy=0.04, s=0.85)
    kx = 0.66
    c.circle((kx, 0.32), HEAD * 0.9)
    c.line((kx, 0.38), (kx, 0.55), LIMB)
    c.line((kx, 0.42), (kx - 0.09, 0.52), LIMB * 0.85)
    c.line((kx, 0.42), (kx + 0.09, 0.52), LIMB * 0.85)
    c.line((kx, 0.55), (kx + 0.08, 0.64), LIMB * 0.9)
    c.line((kx + 0.08, 0.64), (kx + 0.08, 0.73), LIMB * 0.9)
    c.rrect((kx - 0.14, 0.56), (kx + 0.02, 0.60), 0.015)       # the seat
    c.rect((kx - 0.14, 0.60), (kx - 0.115, 0.72))


def icon_brake(c):  # a wheel with a brake pad squeezing it
    wheel(c, (0.5, 0.5), 0.2, spokes=5)
    c.arc((0.5, 0.5), 0.29, 120, 240, width=0.06)              # the pad
    c.line((0.20, 0.30), (0.26, 0.34), 0.03)
    c.line((0.20, 0.70), (0.26, 0.66), 0.03)


def icon_camera(c):  # a camera with a swap arrow
    c.rrect((0.28, 0.40), (0.72, 0.66), 0.035)                 # body
    c.rrect((0.40, 0.33), (0.52, 0.42), 0.02)                  # top hump
    c.circle((0.50, 0.53), 0.085, 0)                           # lens (cut out)
    c.circle((0.50, 0.53), 0.052, 255)
    c.arc((0.5, 0.5), 0.3, 205, 265, width=0.035)
    c.arrow_head((0.5 + 0.3 * math.cos(math.radians(265)), 0.5 + 0.3 * math.sin(math.radians(265))), 295, 0.08, 0.055)
    c.arc((0.5, 0.5), 0.3, 25, 85, width=0.035)
    c.arrow_head((0.5 + 0.3 * math.cos(math.radians(85)), 0.5 + 0.3 * math.sin(math.radians(85))), 115, 0.08, 0.055)


ICONS = {
    "ow_btn_legs.png": icon_legs,
    "ow_btn_squat.png": icon_squat,
    "ow_btn_jump_legs.png": icon_jump_legs,
    "ow_btn_jump_squat.png": icon_jump_squat,
    "ow_btn_fist_l.png": icon_fist_l,
    "ow_btn_fist_r.png": icon_fist_r,
    "ow_btn_wheel_l.png": icon_wheel_l,
    "ow_btn_wheel_r.png": icon_wheel_r,
    "ow_btn_jet_cw.png": icon_jet_cw,
    "ow_btn_jet_ccw.png": icon_jet_ccw,
    "ow_btn_eject_girl.png": icon_girl,
    "ow_btn_eject_kid.png": icon_kid,
    "ow_btn_brake.png": icon_brake,
    "ow_btn_camera.png": icon_camera,
}


def render(draw_fn, size):
    """One button: ring (white 40 %), disc (black 60 %), pictogram (white 40 %)."""
    w, h = size
    big = Canvas(size)
    draw_fn(big)
    glyph = big.mask.resize((w, h), Image.LANCZOS)

    ring_mask = Image.new("L", (w * SS, h * SS), 0)
    d = ImageDraw.Draw(ring_mask)
    cx, cy = w * SS / 2.0, h * SS / 2.0
    for radius, fill in ((RING_R, 255), (DISC_R, 0)):
        rx, ry = radius * w * SS - SS, radius * h * SS - SS
        d.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=fill)
    disc_mask = Image.new("L", (w * SS, h * SS), 0)
    d = ImageDraw.Draw(disc_mask)
    rx, ry = DISC_R * w * SS, DISC_R * h * SS
    d.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=255)
    ring_mask = ring_mask.resize((w, h), Image.LANCZOS)
    disc_mask = disc_mask.resize((w, h), Image.LANCZOS)

    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    black = Image.new("RGBA", (w, h), (0, 0, 0, DISC_ALPHA))
    out.paste(black, (0, 0), disc_mask)
    white = Image.new("RGBA", (w, h), (255, 255, 255, RING_ALPHA))
    # the pictogram stays inside the disc
    glyph_in = Image.composite(glyph, Image.new("L", (w, h), 0), disc_mask)
    out.paste(white, (0, 0), glyph_in)
    out.paste(white, (0, 0), ring_mask)
    return out


def pack(images):
    """A simple shelf layout: (sheet, {name: (x, y, w, h)})."""
    cols = 5
    w, h = next(iter(images.values())).size
    pad = 2
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * (w + pad), rows * (h + pad)), (0, 0, 0, 0))
    rects = {}
    for i, (name, img) in enumerate(images.items()):
        x, y = (i % cols) * (w + pad), (i // cols) * (h + pad)
        sheet.paste(img, (x, y))
        rects[name] = (x, y, w, h)
    return sheet, rects


def write_sheet(out_dir, images):
    sheet, rects = pack(images)
    os.makedirs(out_dir, exist_ok=True)
    png = os.path.join(out_dir, "ow_controls.png")
    sheet.save(png, optimize=True)
    frames = {}
    for name, (x, y, w, h) in rects.items():
        frames[name] = {
            "aliases": [],
            "spriteOffset": "{0,0}",
            "spriteSize": "{%d,%d}" % (w, h),
            "spriteSourceSize": "{%d,%d}" % (w, h),
            "textureRect": "{{%d,%d},{%d,%d}}" % (x, y, w, h),
            "textureRotated": False,
        }
    plist = {
        "frames": frames,
        "metadata": {
            "format": 3,
            "pixelFormat": "RGBA8888",
            "premultiplyAlpha": False,
            "realTextureFileName": "ow_controls.png",
            "size": "{%d,%d}" % sheet.size,
            "smartupdate": "$OpenWheels ow_button_icons.py$",
            "textureFileName": "ow_controls.png",
        },
    }
    with open(os.path.join(out_dir, "ow_controls.plist"), "wb") as f:
        plistlib.dump(plist, f)
    return png


# ---- pause-menu icons (white glyph on the transparent, untrimmed button frame) ------------------

PAUSE_TIERS = {"large": 256, "medium": 128, "small": 96, "tiny": 64}


def pause_touch_icon(c, slashed):
    """A phone with a finger tap; slashed = 'hide the touch controls'."""
    c.rrect((0.30, 0.16), (0.70, 0.84), 0.07)                  # phone body
    c.rrect((0.345, 0.225), (0.655, 0.70), 0.03, fill=0)       # screen (cut out)
    c.circle((0.5, 0.775), 0.036, 0)                           # home button (cut out)
    c.circle((0.5, 0.46), 0.052)                               # the touch point
    c.arc((0.5, 0.46), 0.115, 0, 360, width=0.026)             # ripple
    if slashed:
        c.line((0.18, 0.88), (0.82, 0.12), 0.085)
        # a gap beside the slash so it reads over the phone
        c.line((0.215, 0.88), (0.855, 0.12), 0.0)


def render_pause(slashed, size):
    big = Canvas((size, size))
    pause_touch_icon(big, slashed)
    mask = big.mask.resize((size, size), Image.LANCZOS)
    out = Image.new("RGBA", (size, size), (255, 255, 255, 0))
    out.putalpha(mask)
    return out


def write_pause(out_dir):
    for tier, size in PAUSE_TIERS.items():
        images = {"ow_pause_icon_touch_hide.png": render_pause(True, size),
                  "ow_pause_icon_touch_show.png": render_pause(False, size)}
        d = os.path.join(out_dir, tier, "menus", "pause")
        os.makedirs(d, exist_ok=True)
        sheet = Image.new("RGBA", (size * 2 + 2, size), (0, 0, 0, 0))
        frames = {}
        for i, (name, img) in enumerate(images.items()):
            x = i * (size + 2)
            sheet.paste(img, (x, 0))
            frames[name] = {"aliases": [], "spriteOffset": "{0,0}", "spriteSize": "{%d,%d}" % (size, size),
                            "spriteSourceSize": "{%d,%d}" % (size, size),
                            "textureRect": "{{%d,0},{%d,%d}}" % (x, size, size), "textureRotated": False}
        sheet.save(os.path.join(d, "ow_pause.png"), optimize=True)
        meta = {"format": 3, "pixelFormat": "RGBA8888", "premultiplyAlpha": False,
                "realTextureFileName": "ow_pause.png", "textureFileName": "ow_pause.png",
                "size": "{%d,%d}" % sheet.size, "smartupdate": "$OpenWheels ow_button_icons.py$"}
        with open(os.path.join(d, "ow_pause.plist"), "wb") as f:
            plistlib.dump({"frames": frames, "metadata": meta}, f)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", required=True, help="e.g. <exe dir>/generated/restored")
    ap.add_argument("--preview", help="also write a contact sheet of the large icons to this PNG")
    ap.add_argument("--optional", action="store_true", help="exit 0 even when Pillow work fails (build hooks)")
    a = ap.parse_args()
    for tier, size in TIERS.items():
        images = {name: render(fn, size) for name, fn in ICONS.items()}
        write_sheet(os.path.join(a.out, tier, "controls"), images)
    write_pause(a.out)
    print(f"ow_button_icons: {len(ICONS)} icons x {len(TIERS)} tiers -> {a.out}")
    if a.preview:
        big = {name: render(fn, TIERS["large"]) for name, fn in ICONS.items()}
        for slashed in (True, False):  # the pause-menu icon on a pause-menu button colour
            btn = Image.new("RGBA", TIERS["large"], (0, 0, 0, 0))
            ImageDraw.Draw(btn).ellipse((36, 34, 292, 290), fill=(61, 134, 201, 255))
            btn.alpha_composite(render_pause(slashed, 256), (36, 34))
            big["pause%d" % slashed] = btn
        cols = 5
        w, h = TIERS["large"]
        rows = (len(big) + cols - 1) // cols
        sheet = Image.new("RGBA", (cols * (w + 16), rows * (h + 16)), (63, 120, 190, 255))
        for i, img in enumerate(big.values()):
            sheet.alpha_composite(img, ((i % cols) * (w + 16) + 8, (i // cols) * (h + 16) + 8))
        sheet.convert("RGB").save(a.preview)


if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # noqa: BLE001
        if "--optional" in sys.argv:
            print(f"ow_button_icons: skipped ({e})")
            sys.exit(0)
        raise
