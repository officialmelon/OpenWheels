#pragma once
// ONLINE (PC addition): an automated walk through the account / replay / publish screens for
// testing against tools/online/mock_tjf.py (`--online-test tour`). It drives the UI the way a
// player would (simulated touches, keys and typing), saves screenshots to $OW_TJF_TEST_OUT and
// quits. Refuses to run unless OW_TJF_BASE points at a local host: it logs in, rates, favorites,
// uploads and publishes, which must never happen on the real site without the player.
//
// `--online-test dont-move`: plays a browser level (OW_TJF_TEST_LEVEL_ID, default 900001: the
// mock sample from tools/online/make_dont_move_sample.py) without pressing a key, with browser
// physics on (OW_TJF_TEST_BROWSER_PHYSICS=0: off), until the finish line; checks the character
// survived, counts how many drawn frames moved (smooth drawing), then watches the run as a replay
// and checks every world step of it matches the run bit for bit. Needs no account.

#include <string>

namespace online {

void runTjfTestScenario(const std::string& scenario);

}  // namespace online
