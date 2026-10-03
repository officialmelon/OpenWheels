"""Behaviour oracle: boots the ORIGINAL libMyGame.so (cocos2d-x + Box2D + game code) inside the
arm64 emulator so we can run real game code paths (level loading, physics steps) and dump
the resulting state for comparison with the reconstruction.

  python tools/re/oracle.py boot                 sanity-check the engine boot
"""
import argparse
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import Emu, EmuError, GHIDRA_BASE  # noqa: E402
from emu_runtime import Runtime  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_ASSETS = os.path.join(ROOT, "binary", "HappyWheels_Android", "HW_Android", "assets")


class Oracle:
    def __init__(self, assets=DEFAULT_ASSETS, verbose=False, tier="large", frame=(1920.0, 1080.0)):
        self.e = Emu()
        self.rt = Runtime(self.e, assets_dir=assets, verbose=verbose)
        self.verbose = verbose
        self.tier = tier
        self.frame = frame

    def sym(self, name):
        return self.e.sym_addr(name)

    def call(self, name_or_addr, *args, **kw):
        a = name_or_addr if isinstance(name_or_addr, int) else self.sym(name_or_addr)
        return self.e.call(a, *args, **kw)

    def run_init_array(self):
        sec = self.e.elf.sections[".init_array"]
        n = sec.size // 8
        for i in range(n):
            ptr = self.e.u64(GHIDRA_BASE + sec.addr + i * 8)
            if ptr in (0, 0xFFFFFFFFFFFFFFFF):
                continue
            try:
                self.e.call(ptr)
            except EmuError as ex:
                print(f"init_array[{i}] @ {ptr:#x} failed: {ex}")
                raise

    def boot(self):
        t = time.time()
        self.run_init_array()
        # FileUtilsAndroid reads APK assets through this static AAssetManager*
        am = self.sym("_ZN7cocos2d16FileUtilsAndroid12assetmanagerE")
        self.e.w(am, struct.pack("<Q", 0xA55E7))
        # JniHelper::_psJavaVM -> fake VM answering with neutral defaults
        self.e.w(self.sym("_ZN7cocos2d9JniHelper9_psJavaVME"), struct.pack("<Q", self.rt.java_vm))
        # cocos_android_app_init: the AppDelegate constructor registers Application::getInstance()
        app = self.e.malloc(0x100)
        self.call("_ZN11AppDelegateC1Ev", app)
        self.app = app
        fu = self.call("_ZN7cocos2d9FileUtils11getInstanceEv")
        director = self.call("_ZN7cocos2d8Director11getInstanceEv")
        # Display bring-up as AppDelegate::applicationDidFinishLaunching() does it, with the
        # frame size a device would report (Java normally pushes it in via nativeInit).
        glview = self.call("_ZN7cocos2d10GLViewImpl6createERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE",
                           self.std_string_arg("hwcpp"))
        self.e.call(self.sym("_ZN7cocos2d6GLView12setFrameSizeEff"), glview, fargs=self.frame)
        self.call("_ZN7cocos2d8Director13setOpenGLViewEPNS_6GLViewE", director, glview)
        self.e.call(self.sym("_ZN7cocos2d8Director20setAnimationIntervalEf"), director, fargs=(0.016666668,))
        self.e.call(self.sym("_ZN7cocos2d6GLView23setDesignResolutionSizeEff16ResolutionPolicy"), glview, 3,
                    fargs=(3600.0, 2000.0))
        self.glview = glview
        # Same search paths AppDelegate::applicationDidFinishLaunching() installs.
        add = self.sym("_ZN7cocos2d9FileUtils13addSearchPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEb")
        for p in ("shared", "sounds", self.tier):
            self.e.call(add, fu, self.std_string_arg(p), 0)
        # content scale = tier height / design height (2000)
        tier_h = {"large": 2000.0, "medium": 1000.0, "small": 750.0, "tiny": 500.0}[self.tier]
        self.e.call(self.sym("_ZN7cocos2d8Director21setContentScaleFactorEf"), director, fargs=(tier_h / 2000.0,))
        print(f"boot ok in {time.time() - t:.1f}s: FileUtils={fu:#x} Director={director:#x}")
        return fu, director


    # ------------------------------------------------------------------ game
    def std_string_arg(self, s):
        """Allocate a libc++ std::string in emulated memory (for by-value string params)."""
        p = self.e.malloc(24)
        self.e.make_std_string(p, s)
        return p

    def load_level(self, level_path):
        """Replicates Gameplay::beginGameplayFollowingInterstitial()'s session/level setup."""
        call = self.call
        settings = call("_ZN8Settings11getInstanceEv")
        sc = call("_ZN8Settings18getSoundControllerEv", settings)
        # Session::create(float version, SoundController*, SessionMode)
        session = self.e.call(self.sym("_ZN7Session6createEfP15SoundController11SessionMode"), sc, 0,
                              fargs=(1.0,))
        call("_ZN8Settings17setCurrentSessionEP7Session", settings, session)
        call("_ZN7Session11createWorldEv", session)
        bg = call("_ZN15BackgroundLayer6createEv")
        call("_ZN7Session18setBackgroundLayerEP15BackgroundLayer", session, bg)
        # The level string is the XML *contents* (LevelXMLParser::init -> XMLDocument::Parse); read it
        # through the original FileUtils exactly like the game does. std::string results come back
        # through x8 (sret); by-value std::string params are pointers to caller-owned temporaries.
        fu = call("_ZN7cocos2d9FileUtils11getInstanceEv")
        xml = self.e.malloc(24)
        self.e.call(self.sym("_ZNK7cocos2d9FileUtils17getStringFromFileERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"),
                    fu, self.std_string_arg(level_path), x8=xml)
        if not self.e.std_string(xml):
            raise RuntimeError(f"original FileUtils could not read {level_path}")
        call("_ZN7Session10setupLevelENSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEb",
             session, xml, 0)
        self.session = session
        return session

    def world(self):
        return self.call("_ZN7Session8getWorldEv", self.session)

    def characters(self):
        level = self.call("_ZN7Session8getLevelEv", self.session)
        vec = self.e.malloc(24)
        self.e.call(self.sym("_ZN8LevelB2D13getCharactersEv"), level, x8=vec)
        return [self.e.u64(p) for p in self.e.std_vector(vec, 8)]

    def start_gameplay(self, level_path, states):
        """Run the ORIGINAL Gameplay scene in replay mode: per-frame control bytes come from
        ReplayData exactly as when the game replays a run, so all start/timer gating is original."""
        call = self.call
        fu = call("_ZN7cocos2d9FileUtils11getInstanceEv")
        xml = self.e.malloc(24)
        self.e.call(self.sym("_ZNK7cocos2d9FileUtils17getStringFromFileERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"),
                    fu, self.std_string_arg(level_path), x8=xml)
        replay = self.e.malloc(0x4660)
        call("_ZN10ReplayDataC1Ev", replay)
        add = self.sym("_ZN10ReplayData8addEntryEh")
        for s in states:
            self.e.call(add, replay, s & 0xFF)
        call("_ZN10ReplayData13resetPositionEv", replay)
        scene = call("_ZN8Gameplay11createSceneENSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEP10ReplayData",
                     xml, replay)
        director = call("_ZN7cocos2d8Director11getInstanceEv")
        call("_ZN7cocos2d8Director12runWithSceneEPNS_5SceneE", director, scene)
        call("_ZN7cocos2d8Director12setNextSceneEv", director)  # what drawScene() does first
        self.director = director
        self.scheduler = self.e.u64(director + 160)  # Director::_scheduler (getScheduler() is inline)
        self.replay = replay

    def tick(self, dt=0.016666668):
        """One Director frame minus rendering: advance the clock and run the original Scheduler."""
        self.rt.fake_time += dt
        self.e.call(self.sym("_ZN7cocos2d9Scheduler6updateEf"), self.scheduler, fargs=(dt,))

    def current_session(self):
        settings = self.call("_ZN8Settings11getInstanceEv")
        return self.call("_ZN8Settings17getCurrentSessionEv", settings)

    def step(self, state):
        """One frame of Gameplay::update's physics-relevant work: input -> characters, Session::update."""
        set_state = self.sym("_ZN12CharacterB2D8setStateEh")
        for ch in self.characters():
            vt = self.e.u64(ch)
            fn = self.e.u64(vt + 0x118)  # virtual CharacterB2D::setState(unsigned char)
            self.e.call(fn, ch, state & 0xFF)
        self.e.call(self.sym("_ZN7Session6updateEf"), self.session, fargs=(0.016666668,))

    # ------------------------------------------------------------------ dump
    def dump_world(self, world):
        e = self.e
        f32, u64, u32 = e.f32, e.u64, e.u32

        def vec(a):
            return [f32(a), f32(a + 4)]

        bodies, index = [], {}
        b = u64(world + 103200)
        while b:
            index[b] = len(bodies)
            bodies.append(b)
            b = u64(b + 104)
        out = {"bodies": [], "joints": [], "gravity": vec(world + 103224)}
        for b in bodies:
            # drop transient solver bits (e_islandFlag 0x1, e_toiFlag 0x40): not observable via the
            # public API the reconstruction's dumper uses
            flags = struct.unpack("<H", e.r(b + 4, 2))[0] & ~0x41
            body = {
                "type": u32(b), "pos": vec(b + 12), "angle": f32(b + 56), "linvel": vec(b + 64),
                "angvel": f32(b + 72), "flags": flags, "mass": f32(b + 144), "I": f32(b + 152),
                "linDamp": f32(b + 160), "angDamp": f32(b + 164), "gravityScale": f32(b + 168),
                "localCenter": vec(b + 28), "fixtures": [],
            }
            fx = u64(b + 112)
            while fx:
                sh = u64(fx + 24)
                st = u32(sh + 8)
                cat, mask, grp = struct.unpack("<HHh", e.r(fx + 52, 6))
                fd = {"density": f32(fx), "friction": f32(fx + 32), "restitution": f32(fx + 36),
                      "sensor": e.r(fx + 58, 1)[0], "filter": [cat, mask, grp], "shape": st,
                      "radius": f32(sh + 12)}
                if st == 0:
                    fd["center"] = vec(sh + 16)
                elif st == 1:
                    fd["v"] = [vec(sh + 16), vec(sh + 24)]
                elif st == 2:
                    n = u32(sh + 152)
                    fd["v"] = [vec(sh + 24 + 8 * i) for i in range(n)]
                elif st == 3:
                    n = u32(sh + 24)
                    vp = u64(sh + 16)
                    fd["v"] = [vec(vp + 8 * i) for i in range(n)]
                body["fixtures"].append(fd)
                fx = u64(fx + 8)
            out["bodies"].append(body)
        j = u64(world + 103208)
        while j:
            jt = u32(j + 8)
            jd = {"type": jt, "a": index.get(u64(j + 96), -1), "b": index.get(u64(j + 104), -1),
                  "collide": e.r(j + 117, 1)[0]}
            if jt == 1:  # revolute
                jd.update(anchorA=vec(j + 128), anchorB=vec(j + 136), motor=e.r(j + 160, 1)[0],
                          maxTorque=f32(j + 164), speed=f32(j + 168), limit=e.r(j + 172, 1)[0],
                          ref=f32(j + 176), lower=f32(j + 180), upper=f32(j + 184))
            elif jt == 2:  # prismatic
                jd.update(anchorA=vec(j + 128), anchorB=vec(j + 136), axis=vec(j + 144), ref=f32(j + 160),
                          lower=f32(j + 180), upper=f32(j + 184), maxForce=f32(j + 188), speed=f32(j + 192),
                          limit=e.r(j + 196, 1)[0], motor=e.r(j + 197, 1)[0])
            else:  # raw joint-specific floats so any difference is still visible
                jd["raw"] = [round(f32(j + 128 + 4 * i), 6) for i in range(24)]
            out["joints"].append(jd)
            j = u64(j + 24)
        return out


def main():
    import json
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["boot", "level", "step", "play"])
    ap.add_argument("--dump-at", default="", help="comma list of frames to dump (play mode), e.g. 0,60,120")
    ap.add_argument("--level", default="levels/01_business_guy/01_business_guy_tutorial_level.xml")
    ap.add_argument("--out", default=None)
    ap.add_argument("--frames", type=int, default=60)
    ap.add_argument("--script", default="0:00",
                    help="frame:statehex changes, e.g. '0:01,90:00' (bits are GameplayBtn state values)")
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()
    o = Oracle(verbose=a.verbose)
    o.boot()
    if a.cmd == "play":
        changes = sorted((int(f), int(s, 16)) for f, s in (p.split(":") for p in a.script.split(",")))
        states, state = [], 0
        for frame in range(a.frames + 8):
            for f, s in changes:
                if f == frame:
                    state = s
            states.append(state)
        t = time.time()
        o.start_gameplay(a.level, states)
        dump_at = {int(x) for x in a.dump_at.split(",") if x}
        base = os.path.splitext(a.out or "reports/oracle_play.json")[0]
        for frame in range(a.frames + 1):
            if frame in dump_at or frame == a.frames:
                sess = o.current_session()
                if sess:
                    o.session = sess
                    d = o.dump_world(o.world())
                    d["frames"] = frame
                    path = f"{base}_f{frame}.json"
                    with open(path, "w") as f:
                        json.dump(d, f, indent=1)
                    print(f"frame {frame}: {len(d['bodies'])} bodies -> {path}")
                else:
                    print(f"frame {frame}: no session yet")
            if frame < a.frames:
                o.tick()
        print(f"played {a.frames} frames in {time.time() - t:.1f}s")
        return
    if a.cmd in ("level", "step"):
        t = time.time()
        o.load_level(a.level)
        print(f"level loaded in {time.time() - t:.1f}s")
        frames = 0
        if a.cmd == "step":
            changes = sorted((int(f), int(s, 16)) for f, s in (p.split(":") for p in a.script.split(",")))
            state = 0
            t = time.time()
            for frame in range(a.frames):
                for f, s in changes:
                    if f == frame:
                        state = s
                o.step(state)
            frames = a.frames
            print(f"stepped {a.frames} frames in {time.time() - t:.1f}s")
        dump = o.dump_world(o.world())
        dump["frames"] = frames
        print(f"{len(dump['bodies'])} bodies, {len(dump['joints'])} joints")
        if a.out:
            with open(a.out, "w") as f:
                json.dump(dump, f, indent=1)


if __name__ == "__main__":
    main()
