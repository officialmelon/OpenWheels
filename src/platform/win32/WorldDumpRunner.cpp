// Disabled until the reconstructed game links: define OW_WITH_PC_LAYER (see main.cpp).
#ifdef OW_WITH_PC_LAYER
#include "platform/win32/WorldDumpRunner.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <vector>

#include "cocos2d.h"
#include "Gameplay.h"
#include "ReplayData.h"
#include "Session.h"
#include "Settings.h"
#include "platform/debug/WorldDump.h"

USING_NS_CC;

namespace openwheels {
namespace pc {

int runWorldDump(const std::string& outPath, const std::string& levelPath, int frames,
                 const std::string& script) {
    // Same boot as the original (search paths, design resolution, content scale, first scene).
    auto app = Application::getInstance();
    if (!app->applicationDidFinishLaunching()) return 2;

    // Control-byte script -> ReplayData, exactly like the oracle (tools/re/oracle.py play).
    std::vector<std::pair<int, int>> changes;
    std::stringstream ss(script);
    std::string item;
    while (std::getline(ss, item, ',')) {
        const size_t colon = item.find(':');
        if (colon == std::string::npos) continue;
        changes.emplace_back(std::atoi(item.substr(0, colon).c_str()),
                             (int)std::strtol(item.substr(colon + 1).c_str(), nullptr, 16));
    }
    auto replay = new ReplayData();
    int state = 0;
    for (int f = 0; f < frames + 8; ++f) {
        for (auto& c : changes)
            if (c.first == f) state = c.second;
        replay->addEntry((unsigned char)state);
    }
    replay->resetPosition();

    const std::string xml = FileUtils::getInstance()->getStringFromFile(levelPath);
    if (xml.empty()) {
        cocos2d::log("dump-world: cannot read %s", levelPath.c_str());
        return 3;
    }
    auto director = Director::getInstance();
    director->replaceScene(Gameplay::createScene(xml, replay));

    // Frame 0 enters the scene (drawScene: scheduler tick, then setNextScene); the oracle's
    // start_gameplay() corresponds to it. Then `frames` fixed-dt ticks, like oracle.tick().
    director->mainLoop(1.0f / 60.0f);
    for (int f = 0; f < frames; ++f) director->mainLoop(1.0f / 60.0f);

    Session* session = Settings::getInstance()->getCurrentSession();
    if (!session || !session->getWorld()) {
        cocos2d::log("dump-world: no session/world after %d frames", frames);
        return 4;
    }
    const bool ok = debug::dumpWorldJson(session->getWorld(), outPath, frames);
    cocos2d::log("dump-world: %s -> %s", ok ? "written" : "FAILED", outPath.c_str());
    std::fflush(stdout);
    std::_Exit(ok ? 0 : 5);  // skip teardown: the dump is all this mode is for
}

}  // namespace pc
}  // namespace openwheels
#endif  // OW_WITH_PC_LAYER
