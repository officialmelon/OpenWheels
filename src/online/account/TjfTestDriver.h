#pragma once
// ONLINE (PC addition): an automated walk through the account / replay / publish screens for
// testing against tools/online/mock_tjf.py (`--online-test tour`). It drives the UI the way a
// player would (simulated touches, keys and typing), saves screenshots to $OW_TJF_TEST_OUT and
// quits. Refuses to run unless OW_TJF_BASE points at a local host: it logs in, rates, favorites,
// uploads and publishes, which must never happen on the real site without the player.

#include <string>

namespace online {

void runTjfTestScenario(const std::string& scenario);

}  // namespace online
