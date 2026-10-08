// PC verification mode (Windows, Linux, macOS): define OW_WITH_PC_LAYER (see CMakeLists.txt).
#ifdef OW_WITH_PC_LAYER
#include "platform/desktop/WorldDumpRunner.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "cocos2d.h"
#include "Gameplay.h"
#include "ReplayData.h"
#include "Session.h"
#include "Settings.h"
#include "platform/debug/WorldDump.h"
#include "qol/QoL.h"

USING_NS_CC;

namespace openwheels {
namespace pc {

int runWorldDump(const std::string& outPath, const std::string& levelPath, int frames,
                 const std::string& script, const std::string& dumpAt) {
    // Same boot as the original (search paths, design resolution, content scale, first scene).
    auto app = Application::getInstance();
    if (!app->applicationDidFinishLaunching()) return 2;
    // The game's 1/60 s ticks, whatever the player's QoL frame rate. OW_DUMP_FPS=30 runs the QoL
    // 30 FPS mode instead (two ticks per mainLoop of 1/30 s): the dump must come out identical.
    const char* fpsEnv = std::getenv("OW_DUMP_FPS");
    const int fps = fpsEnv && std::atoi(fpsEnv) == 30 ? 30 : 60;
    qol::installFrameRate(fps);
    // The original's behaviour: a grabbed vehicle is held, never re-mounted (QoL re-grab vehicle).
    qol::suspendRegrabVehicle(true);
    const int ticksPerLoop = fps == 30 ? 2 : 1;
    const float loopDt = fps == 30 ? 1.0f / 30.0f : 1.0f / 60.0f;

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
    std::vector<int> extra;
    {
        std::stringstream ds(dumpAt);
        while (std::getline(ds, item, ','))
            if (!item.empty()) extra.push_back(std::atoi(item.c_str()));
    }
    std::string base = outPath;
    if (base.size() > 5 && base.compare(base.size() - 5, 5, ".json") == 0) base.resize(base.size() - 5);
    auto dumpExtra = [&](int frame) {
        for (int e : extra) {
            if (e != frame) continue;
            Session* s = Settings::getInstance()->getCurrentSession();
            if (s && s->getWorld())
                debug::dumpWorldJson(s->getWorld(), base + "_f" + std::to_string(frame) + ".json", frame);
        }
    };
    director->mainLoop(loopDt);
    dumpExtra(0);
    for (int f = 0; f < frames; f += ticksPerLoop) {
        director->mainLoop(loopDt);
        dumpExtra(f + ticksPerLoop);
    }

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
