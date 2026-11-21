#!/usr/bin/env python3
"""ow_button_icons.py: every frame the game asks for exists, at each tier's button size, not blank."""
import os, plistlib, re, subprocess, sys, tempfile
from PIL import Image

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sizes = {"large": (328, 325), "medium": (165, 163), "small": (124, 122), "tiny": (83, 81)}
src = open(os.path.join(root, "src/restored/Restored.cpp")).read() + open(os.path.join(root, "src/game/gameplay/GameplayControls.cpp")).read()
wanted = set(re.findall(r'"(ow_btn_[a-z_]+\.png)"', src))
assert len(wanted) >= 14, wanted

with tempfile.TemporaryDirectory() as out:
    subprocess.check_call([sys.executable, os.path.join(root, "tools/assets/ow_button_icons.py"), "--out", out])
    for tier, (w, h) in sizes.items():
        d = os.path.join(out, tier, "controls")
        plist = plistlib.load(open(os.path.join(d, "ow_controls.plist"), "rb"))
        sheet = Image.open(os.path.join(d, "ow_controls.png")).convert("RGBA")
        missing = wanted - set(plist["frames"])
        assert not missing, f"{tier}: missing frames {missing}"
        for name in wanted:
            nums = list(map(int, re.findall(r"\d+", plist["frames"][name]["textureRect"])))
            x, y, fw, fh = nums
            assert (fw, fh) == (w, h), f"{tier}/{name}: {fw}x{fh}"
            alpha = sheet.crop((x, y, x + fw, y + fh)).getchannel("A")
            glyph = sum(1 for v in list(alpha.crop((fw // 4, fh // 4, 3 * fw // 4, 3 * fh // 4)).tobytes()) if v > 0)
            assert glyph > 0.4 * (fw // 2) * (fh // 2), f"{tier}/{name} looks empty"
    for tier, size in {"large": 256, "medium": 128, "small": 96, "tiny": 64}.items():
        d = os.path.join(out, tier, "menus", "pause")
        plist = plistlib.load(open(os.path.join(d, "ow_pause.plist"), "rb"))
        assert set(plist["frames"]) == {"ow_pause_icon_touch_hide.png", "ow_pause_icon_touch_show.png"}, plist["frames"].keys()
        assert Image.open(os.path.join(d, "ow_pause.png")).size[1] == size
print("button icons ok:", len(wanted), "frames x", len(sizes), "tiers")
