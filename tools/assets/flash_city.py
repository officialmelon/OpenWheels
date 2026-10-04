"""ONLINE (PC addition): the browser game's city background (level background 2), for
extract_flash_items.py.

Flash UserLevel.createBackDrops (bdIndex 2) stacks three BackDrops at the bottom of the session:
  CitySource3            multiplier 0     (static sky, opaque)
  CityBackDrop2          multiplier 0.25  (CitySource2: b1 drawn whole, b2-b4 as "buildings", blur 5)
  CityBackDrop1          multiplier 0.5   (CitySource1: b1-b4 as "buildings", blur 2)
StageCamera.adjustBackDrops moves each by round(container.x * multiplier, container.y * ...).
BackDrop.drawBuilding(clip, top, row) draws the clip's first `top` px as they are and then tiles
the `row` px below them down to y = 10000 * multiplier + 500 * (1 - multiplier) (backdrop space);
every backdrop is drawn twice, the copy shifted right by 5000 (CityBackDrop1) / 2500 (2).

Output in <out_dir>/city/: PNGs (blurred like Flash's BlurFilter, rendered at ZOOM image px per
Flash px) and city.tsv, one line per piece:
    layer multiplier name x y kind tileTo repeatDx tileH
x/y: Flash px of the piece's top-left in backdrop space; kind: full | top | strip (strip images
hold `tileH` Flash px of repeated rows and are stacked from y down to tileTo).
"""
import math
import os

try:
    from PIL import Image, ImageFilter
except ImportError:  # pragma: no cover
    Image = None

ZOOM = 2.0
PAD = 4  # FlashPartRender canvas padding (image px)

# (layer, symbol, multiplier, blur, repeatDx, [(child, mode, top, row)])
LAYERS = [
    (0, 'CitySource3', 0.0, 0, 0, [(None, 'full', 0, 0)]),
    (1, 'CitySource2', 0.25, 5, 2500, [('b1', 'full', 0, 0), ('b2', 'building', 40, 50),
                                        ('b3', 'building', 46, 50), ('b4', 'building', 15, 50)]),
    (2, 'CitySource1', 0.5, 2, 5000, [('b1', 'building', 90, 88), ('b2', 'building', 140, 90),
                                       ('b3', 'building', 60, 102), ('b4', 'building', 160, 101)]),
]


def _sigma(blur):
    # Flash BlurFilter quality 3 = three box passes of width `blur`.
    return math.sqrt(max(0.0, (blur * blur - 1) / 4.0)) * ZOOM


def _blur_premultiplied(im, sigma):
    """Gaussian blur on premultiplied colour (Flash filters work premultiplied)."""
    import numpy as np
    a = np.asarray(im, dtype=np.float32) / 255.0
    pre = a.copy()
    pre[..., :3] *= pre[..., 3:4]
    chans = []
    for c in range(4):
        ch = Image.fromarray(np.clip(pre[..., c] * 255.0, 0, 255).astype(np.uint8), 'L')
        chans.append(np.asarray(ch.filter(ImageFilter.GaussianBlur(sigma)), dtype=np.float32) / 255.0)
    out = np.stack(chans, axis=-1)
    alpha = out[..., 3:4]
    out[..., :3] = np.where(alpha > 1e-4, out[..., :3] / np.maximum(alpha, 1e-4), 0.0)
    return Image.fromarray(np.clip(out * 255.0 + 0.5, 0, 255).astype(np.uint8), 'RGBA')


def build_city(run_java, java, ffdec, swf, out_dir, work, log):
    if Image is None:
        log('Pillow missing: city background skipped')
        return 0
    text = run_java(java, ffdec, swf, ['tree ' + l[1] for l in LAYERS], work)
    roots, children, current = {}, {}, None
    order = iter([l[1] for l in LAYERS])
    for line in text.splitlines():
        p = line.split()
        if line.startswith('ROOT '):
            current = next(order)
            roots[current] = int(p[1])
        elif line.startswith('ERROR'):
            next(order)
        elif line.startswith('P ') and current and int(p[1]) == roots.get(current) and p[2] == '1':
            # P clip frame depth char isClip name tx ty ...
            children[(current, p[6])] = (int(p[4]), int(p[7]) / 20.0, int(p[8]) / 20.0)
    jobs, renders = [], {}
    for layer, sym, mult, blur, dx, parts in LAYERS:
        if sym not in roots:
            log('city: symbol %s missing' % sym)
            return 0
        for child, mode, top, row in parts:
            cid, x, y = (roots[sym], 0.0, 0.0) if child is None else children[(sym, child)]
            png = os.path.join(work, 'city_%s_%s.png' % (sym, child or 'all'))
            jobs.append('render %s %d 1 %g -' % (png, cid, ZOOM))
            renders[png] = (layer, sym, mult, blur, dx, child, mode, top, row, x, y)
    text = run_java(java, ffdec, swf, jobs, work)
    city_dir = os.path.join(out_dir, 'city')
    os.makedirs(city_dir, exist_ok=True)
    lines = []
    for line in text.splitlines():
        if not line.startswith('R '):
            continue
        p = line.split()
        png, ox, oy = ' '.join(p[1:-2]), int(p[-2]), int(p[-1])
        key = next((k for k in renders if os.path.normcase(os.path.abspath(k)) ==
                    os.path.normcase(os.path.abspath(png))), None)
        if key is None:
            continue
        layer, sym, mult, blur, dx, child, mode, top, row, x, y = renders[key]
        im = Image.open(key).convert('RGBA')
        # Content from the clip's origin (Flash draws it translated by +blur into the bitmap,
        # which clips anything left of / above the origin).
        w = im.width - PAD - ox
        h = im.height - PAD - oy
        b = int(round(blur * ZOOM))
        canvas = Image.new('RGBA', (w + 2 * b, h + 2 * b), (255, 255, 255, 0))
        canvas.alpha_composite(im.crop((ox, oy, ox + w, oy + h)), (b, b))
        if blur > 0:
            canvas = _blur_premultiplied(canvas, _sigma(blur))
        if mult == 0:  # opaque BitmapData on white
            bg = Image.new('RGBA', canvas.size, (255, 255, 255, 255))
            bg.alpha_composite(canvas)
            canvas = bg
        tile_to = 10000 * mult + 500 * (1 - mult)
        base = 'city%d_%s' % (layer, child or 'all')
        if mode == 'full':
            canvas.save(os.path.join(city_dir, base + '.png'))
            lines.append('%d\t%g\t%s\t%g\t%g\tfull\t0\t%d\t0' % (layer, mult, base, x - blur, y - blur, dx))
            continue
        z = ZOOM
        top_px = int(round((top + blur) * z))   # rows -blur .. top
        row_px = int(round(row * z))
        canvas.crop((0, 0, canvas.width, min(canvas.height, top_px))).save(
            os.path.join(city_dir, base + '_top.png'))
        strip = canvas.crop((0, top_px, canvas.width, min(canvas.height, top_px + row_px)))
        reps = max(1, min(16, 2048 // max(1, strip.height)))
        tall = Image.new('RGBA', (strip.width, strip.height * reps), (0, 0, 0, 0))
        for i in range(reps):
            tall.paste(strip, (0, i * strip.height))
        tall.save(os.path.join(city_dir, base + '_strip.png'))
        lines.append('%d\t%g\t%s_top\t%g\t%g\ttop\t0\t%d\t0' % (layer, mult, base, x - blur, y - blur, dx))
        lines.append('%d\t%g\t%s_strip\t%g\t%g\tstrip\t%g\t%d\t%g' %
                     (layer, mult, base, x - blur, y + top, tile_to, dx, row * reps))
    with open(os.path.join(city_dir, 'city.tsv'), 'w', encoding='utf-8') as f:
        f.write('# layer\tmultiplier\tname\tx\ty\tkind\ttileTo\trepeatDx\ttileH (zoom %g; '
                'tools/assets/flash_city.py)\n' % ZOOM)
        f.write('\n'.join(lines) + '\n')
    log('city background: %d pieces' % len(lines))
    return len(lines)
