#!/usr/bin/env python3
"""ONLINE (PC addition): art and fonts for the browser-level items, from the player's own SWF.

Browser (Flash/HTML5) levels use items the Android game doesn't have (text boxes, NPCs, chairs,
glass, ...). Their PC ports (src/online/items/) draw the browser game's own art, rendered here at
build time from the decrypted v1.87 game SWF with JPEXS FFDec (FlashPartRender.java, shared with
extract_kid_gore.py) into

    <out>/flash/<name>.png         one library symbol frame, registration point recorded
    <out>/flash/index.tsv          name, origin x, origin y (image px), zoom (image px per Flash px)
    <out>/flash/fonts/*.ttf        the embedded text-box fonts (Helvetica Neue LT, Clarendon LT)

Which symbols to render comes from manifests, tools/assets/flash_items/*.txt, one per item family
so ports can be added without touching each other's lists. Line format (# comments):

    <name> <symbol class> [frame=1] [zoom=2] [hide=<depth>,<depth>...]

(<symbol class> may also be clip:<character id> for an unexported nested clip.)

Nothing here is committed: the output lands next to the exe (gitignored build trees). With
--optional, missing inputs (SWF, FFDec, Java) only print a note and exit 0.
"""
import argparse
import glob
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
JAVA_HELPER = os.path.join(HERE, 'FlashPartRender.java')
sys.path.insert(0, HERE)
MANIFESTS = os.path.join(HERE, 'flash_items')
VERSION = '1'

DEFAULT_SWF = os.path.join(REPO, 'binary', 'flash', 'swf', 'game_e_v1_87_g.dec.swf')
DEFAULT_FFDEC = os.path.join(REPO, 'binary', 'flash', 'tools', 'ffdec')

# Embedded fonts of game_e_v1_87_g (DefineFont3 character ids) -> TextBoxRef font names.
# 361 Helvetica Neue LT Std (regular), 359 ... Med, 363 bold; 88 Clarendon LT Std, 81 bold.
FONTS = {361: 'helvetica', 359: 'helvetica_med', 363: 'helvetica_bold',
         88: 'clarendon', 81: 'clarendon_bold'}


def log(msg):
    print('extract_flash_items: ' + msg, flush=True)


def find_java(explicit):
    if explicit:
        return explicit
    home = os.environ.get('JAVA_HOME')
    if home:
        for n in ('java.exe', 'java'):
            p = os.path.join(home, 'bin', n)
            if os.path.exists(p):
                return p
    return shutil.which('java')


def read_manifests():
    entries = []
    for path in sorted(glob.glob(os.path.join(MANIFESTS, '*.txt'))):
        with open(path, encoding='utf-8') as f:
            for raw in f:
                line = raw.split('#', 1)[0].strip()
                if not line:
                    continue
                parts = line.split()
                e = {'name': parts[0], 'cls': parts[1], 'frame': 1, 'zoom': 2.0, 'hide': '-'}
                for p in parts[2:]:
                    if p.startswith('hide='):
                        e['hide'] = p[5:]
                    elif p.startswith('zoom='):
                        e['zoom'] = float(p[5:])
                    elif p.startswith('frame='):
                        e['frame'] = int(p[6:])
                    else:
                        e['frame'] = int(p)
                entries.append(e)
    return entries


def run_java(java, ffdec, swf, commands, workdir):
    path = os.path.join(workdir, 'commands.txt')
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(commands) + '\n')
    lib = os.path.join(ffdec, 'lib')
    cp = os.pathsep.join([os.path.join(lib, 'ffdec_lib.jar'), os.path.join(lib, '*')])
    res = subprocess.run([java, '-Djava.awt.headless=true', '-cp', cp, JAVA_HELPER, swf, path],
                         capture_output=True, text=True)
    if res.returncode != 0:
        raise RuntimeError('FlashPartRender failed:\n' + res.stdout[-2000:] + res.stderr[-2000:])
    return res.stdout


def export_fonts(java, ffdec, swf, out_dir, workdir):
    cli = os.path.join(ffdec, 'ffdec-cli.jar')
    tmp = os.path.join(workdir, 'fonts')
    os.makedirs(tmp, exist_ok=True)
    res = subprocess.run([java, '-jar', cli, '-format', 'font:ttf', '-export', 'font', tmp, swf],
                         capture_output=True, text=True)
    if res.returncode != 0:
        raise RuntimeError('font export failed: ' + res.stdout[-1000:] + res.stderr[-1000:])
    os.makedirs(out_dir, exist_ok=True)
    found = 0
    for path in glob.glob(os.path.join(tmp, '**', '*.ttf'), recursive=True):
        base = os.path.basename(path)
        try:
            chid = int(base.split('_', 1)[0])
        except ValueError:
            continue
        if chid in FONTS:
            shutil.copyfile(path, os.path.join(out_dir, FONTS[chid] + '.ttf'))
            found += 1
    log('%d text-box fonts' % found)


def stamp_for(args, entries):
    h = hashlib.sha1()
    h.update(VERSION.encode())
    for p in (args.swf, JAVA_HELPER, os.path.abspath(__file__), os.path.join(HERE, 'flash_city.py')):
        st = os.stat(p)
        h.update(('%s %d %d' % (p, st.st_size, int(st.st_mtime))).encode())
    h.update(repr(entries).encode())
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--swf', default=DEFAULT_SWF)
    ap.add_argument('--ffdec', default=DEFAULT_FFDEC)
    ap.add_argument('--java', default=None)
    ap.add_argument('--out', required=True, help='the exe\'s generated/ folder')
    ap.add_argument('--optional', action='store_true')
    ap.add_argument('--force', action='store_true')
    args = ap.parse_args()

    def give_up(msg):
        if args.optional:
            log(msg + ' - skipped (browser-level items fall back to plain shapes)')
            return 0
        log(msg)
        return 1

    if not os.path.exists(args.swf):
        return give_up('no SWF at ' + args.swf)
    if not os.path.exists(os.path.join(args.ffdec, 'lib', 'ffdec_lib.jar')):
        return give_up('no FFDec at ' + args.ffdec)
    args.java = find_java(args.java)
    if not args.java:
        return give_up('java not found')

    entries = read_manifests()
    out_dir = os.path.join(args.out, 'flash')
    stamp_path = os.path.join(out_dir, '.stamp')
    stamp = stamp_for(args, entries)
    if not args.force and os.path.exists(stamp_path) and open(stamp_path).read() == stamp:
        log('up to date')
        return 0
    os.makedirs(out_dir, exist_ok=True)

    with tempfile.TemporaryDirectory() as work:
        export_fonts(args.java, args.ffdec, args.swf, os.path.join(out_dir, 'fonts'), work)

        # Resolve symbol classes to clip ids.
        # "clip:<id>" names an unexported nested clip of game_e_v1_87_g directly by character id
        # (e.g. a child whose frame the game selects with gotoAndStop).
        ids = {e['cls']: int(e['cls'][5:]) for e in entries if e['cls'].startswith('clip:')}
        classes = sorted({e['cls'] for e in entries if not e['cls'].startswith('clip:')})
        if classes:
            text = run_java(args.java, args.ffdec, args.swf, ['tree ' + c for c in classes], work)
            current = iter(classes)
            for line in text.splitlines():
                if line.startswith('ROOT ') or line.startswith('ERROR'):
                    cls = next(current)
                    if line.startswith('ROOT '):
                        ids[cls] = int(line.split()[1])
                    else:
                        log('missing symbol ' + cls)
        cmds = []
        for e in entries:
            if e['cls'] not in ids:
                continue
            png = os.path.join(out_dir, e['name'] + '.png')
            cmds.append('render %s %d %d %g %s' % (png, ids[e['cls']], e['frame'], e['zoom'], e['hide']))
        index = []
        if cmds:
            text = run_java(args.java, args.ffdec, args.swf, cmds, work)
            by_png = {os.path.normcase(os.path.abspath(os.path.join(out_dir, e['name'] + '.png'))): e
                      for e in entries}
            for line in text.splitlines():
                if not line.startswith('R '):
                    continue
                parts = line.split()
                png = os.path.normcase(os.path.abspath(' '.join(parts[1:-2])))
                e = by_png.get(png)
                if e:
                    index.append('%s\t%s\t%s\t%g' % (e['name'], parts[-2], parts[-1], e['zoom']))
        # ONLINE (PC addition): the city background (level background 2), see flash_city.py.
        try:
            import flash_city
            flash_city.build_city(run_java, args.java, args.ffdec, args.swf, out_dir, work, log)
        except Exception as exc:  # the items must not fail because of the backdrop
            log('city background failed: %s' % exc)
        with open(os.path.join(out_dir, 'index.tsv'), 'w', encoding='utf-8') as f:
            f.write('# name\toriginX\toriginY\tzoom (generated by tools/assets/extract_flash_items.py)\n')
            f.write('\n'.join(sorted(index)) + '\n')
        log('%d symbol frames -> %s' % (len(index), out_dir))
    with open(stamp_path, 'w') as f:
        f.write(stamp)
    return 0


if __name__ == '__main__':
    sys.exit(main())
