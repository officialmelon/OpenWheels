#!/usr/bin/env python3
"""ONLINE (PC addition): browser-game sounds the Android build lacks, for browser levels.

The sound table (soundlist.tsv, = Flash SoundList.sfxLookup) names 326 sounds; 69 of them (Santa,
elves, Helicopter Man, both kids, BoomboxHit...) have no file in the Android assets, so browser
sound triggers and items using them were silent. Every sound of the player's own
happy_sounds_v1_72.swf that the Android assets don't have is exported here with JPEXS FFDec and
converted to Ogg Vorbis with ffmpeg:

    <out>/flash/sounds/<FlashName>.ogg      (the Flash class name = the soundlist.tsv file name)
    <out>/flash/sounds/index.txt            marker + list (online::setFlashLevel adds the folder
                                            to the search paths while a browser level runs)

Sounds already produced by tools/assets/extract_character.py (generated/restored/sounds, pass
--skip-dir) are not duplicated. Nothing is committed. With --optional, missing inputs (SWF, FFDec,
Java, ffmpeg) only print a note and exit 0.
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
VERSION = '1'
DEFAULT_SWF = os.path.join(REPO, 'binary', 'flash', 'swf', 'happy_sounds_v1_72.swf')
DEFAULT_FFDEC = os.path.join(REPO, 'binary', 'flash', 'tools', 'ffdec')
DEFAULT_ASSETS = os.path.join(REPO, 'binary', 'HappyWheels_Android', 'HW_Android', 'assets')


def log(msg):
    print('extract_flash_sounds: ' + msg, flush=True)


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


def names_in(folder):
    if not folder or not os.path.isdir(folder):
        return set()
    return {os.path.splitext(f)[0] for f in os.listdir(folder)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--sounds-swf', default=DEFAULT_SWF)
    ap.add_argument('--ffdec', default=DEFAULT_FFDEC)
    ap.add_argument('--assets', default=DEFAULT_ASSETS, help="the Android game's assets/ folder")
    ap.add_argument('--skip-dir', action='append', default=[],
                    help='folders whose sounds already exist (e.g. generated/restored/sounds)')
    ap.add_argument('--java', default=None)
    ap.add_argument('--ffmpeg', default=None)
    ap.add_argument('--out', required=True, help="the exe's generated/ folder")
    ap.add_argument('--optional', action='store_true')
    ap.add_argument('--force', action='store_true')
    args = ap.parse_args()

    def give_up(msg):
        log(msg + (' - skipped (those browser sounds stay silent)' if args.optional else ''))
        return 0 if args.optional else 1

    if not os.path.isfile(args.sounds_swf):
        return give_up('no sound SWF at ' + args.sounds_swf)
    jar = os.path.join(args.ffdec, 'ffdec-cli.jar')
    if not os.path.isfile(jar):
        jar = os.path.join(args.ffdec, 'ffdec.jar')
    if not os.path.isfile(jar):
        return give_up('no FFDec at ' + args.ffdec)
    java = find_java(args.java)
    if not java:
        return give_up('java not found')
    ffmpeg = args.ffmpeg or shutil.which('ffmpeg')
    if not ffmpeg:
        return give_up('ffmpeg not found')
    have = names_in(os.path.join(args.assets, 'sounds'))
    if not have:
        return give_up('no Android sounds at ' + os.path.join(args.assets, 'sounds'))
    skip = set()
    for d in args.skip_dir:
        skip |= names_in(d)

    out_dir = os.path.join(args.out, 'flash', 'sounds')
    h = hashlib.sha1(VERSION.encode())
    st = os.stat(args.sounds_swf)
    h.update(('%d %d %s' % (st.st_size, int(st.st_mtime), sorted(skip))).encode())
    h.update(open(os.path.abspath(__file__), 'rb').read())
    stamp = h.hexdigest()
    stamp_path = os.path.join(out_dir, '.stamp')
    if not args.force and os.path.exists(stamp_path) and open(stamp_path).read() == stamp:
        log('up to date')
        return 0

    with tempfile.TemporaryDirectory() as work:
        res = subprocess.run([java, '-Djava.awt.headless=true', '-jar', jar, '-format', 'sound:mp3',
                              '-export', 'sound', work, args.sounds_swf],
                             capture_output=True, text=True, timeout=900)
        if res.returncode != 0:
            return give_up('FFDec failed: ' + (res.stdout + res.stderr)[-500:])
        files = {}
        for f in os.listdir(work):
            m = re.match(r'\d+_(?:.*\.)?([A-Za-z0-9]+)\.(mp3|wav)$', f)
            if m:
                files[m.group(1)] = os.path.join(work, f)
        if os.path.isdir(out_dir):
            shutil.rmtree(out_dir)
        os.makedirs(out_dir)
        done = []
        for name in sorted(files):
            if name in have or name in skip or name == 'Silence':
                continue
            dst = os.path.join(out_dir, name + '.ogg')
            r = subprocess.run([ffmpeg, '-y', '-loglevel', 'error', '-i', files[name],
                                '-c:a', 'libvorbis', '-q:a', '5', dst],
                               capture_output=True, text=True, timeout=300)
            if r.returncode == 0:
                done.append(name)
            else:
                log('ffmpeg failed for %s: %s' % (name, r.stderr[-300:]))
    with open(os.path.join(out_dir, 'index.txt'), 'w', encoding='utf-8') as f:
        f.write('# browser-game sounds missing from the Android build '
                '(tools/assets/extract_flash_sounds.py)\n')
        f.write('\n'.join(done) + '\n')
    with open(stamp_path, 'w') as f:
        f.write(stamp)
    log('%d sounds -> %s' % (len(done), out_dir))
    return 0


if __name__ == '__main__':
    sys.exit(main())
