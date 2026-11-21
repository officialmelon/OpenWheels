# Tests

Run these before you push; CI runs them on every push and the release workflow runs them before it
publishes anything (a failing check blocks the release).

| What | Command | Needs | Covers |
|---|---|---|---|
| **Unit tests** | `tests/run_unit.sh` | g++, zlib, OpenSSL headers (no engine, no assets) | the site protocol: level lists (stray bytes, BOM, gzip, truncated, bare `&`, Cloudflare / error pages), record decryption, search matching, the update check's version comparison |
| **iOS sound script** | `tests/test_aac_script.sh` | ffmpeg | `tools/release/add_aac_sounds.sh` makes an `.m4a` for every `.ogg` (v0.5.2 shipped one) |
| **Release consistency** | `python3 tests/check_release_consistency.py [vX.Y.Z]` | python3 | `src/qol/AppVersion.h`, `android/app/build.gradle` and the newest `CHANGELOG.md` section agree (and match the tag) |
| **Smoke tests** | `python3 tests/smoke/run_smoke.py --exe <OpenWheels>` | a built game next to its assets, Xvfb, xdotool, ImageMagick, Pillow, numpy, pycryptodome | plays synthetic levels headlessly: a level can be won, a trigger hides a text box, click triggers, the Bad Apple!! 2 tricks (NaN start / NaN interval / trigger links without actions), the online browser opens, and a level is grabbed from a mock site, played and replayed |

Smoke tests never touch the real site or GitHub (`OW_TJF_BASE` / `OW_UPDATE_URL` point at dead local
ports or the mock). Levels are synthetic (`tests/smoke/levels.py`); user levels are never committed.

When something breaks in a release, add the case that would have caught it: a reply the site really
sent goes into `tests/unit/test_protocol.cpp`, a level behaviour into `tests/smoke/levels.py` +
`run_smoke.py`.
