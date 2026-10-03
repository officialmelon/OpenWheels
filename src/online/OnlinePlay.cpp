#include "online/OnlinePlay.h"

#include "cocos2d.h"
#include "LevelSession.h"
#include "LevelStore.h"
#include "online/HWApi.h"

USING_NS_CC;

namespace online {

bool startConvertedLevel(const std::string& flashXml, ConversionReport* report, std::string* error) {
    ConversionReport local;
    ConversionReport& r = report ? *report : local;
    const std::string xml = FlashLevelConverter::toMobile(flashXml, &r);
    if (!r.ok) {
        if (error) *error = r.error.empty() ? "this level could not be converted" : r.error;
        return false;
    }
    LevelSession* session = LevelSession::getInstance();
    session->clearLevelData();
    session->setChapterIndex(LevelStoreChapterImported);
    session->setLevelDataXML(xml);
    session->setForceCharacter(r.forceCharacter);
    session->setCharacterIndex(r.character);
    session->setVehicleIndex(0);
    session->playLevel(r.forceCharacter);
    return true;
}

void playOnlineLevel(const OnlineLevelInfo& level, bool countPlay,
                     std::function<void(bool, const std::string&, const ConversionReport&)> done) {
    HWApi::getInstance()->downloadLevel(level, countPlay, [done](bool ok, const std::string& err, const std::string& xml) {
        ConversionReport report;
        if (!ok) {
            if (done) done(false, err, report);
            return;
        }
        std::string error;
        // done() runs before the scene starts so the caller can tear down its UI.
        const std::string mobile = FlashLevelConverter::toMobile(xml, &report);
        if (!report.ok) {
            if (done) done(false, report.error.empty() ? "this level could not be converted" : report.error, report);
            return;
        }
        if (done) done(true, std::string(), report);
        startConvertedLevel(xml, &report, &error);
    });
}

void playOnlineLevelById(int levelId) {
    HWApi::getInstance()->getLevelInfo(levelId, [levelId](bool ok, const std::string& err, const OnlineLevelInfo& level) {
        if (!ok) {
            log("online: level %d: %s", levelId, err.c_str());
            return;
        }
        log("online: playing %d \"%s\" by %s", level.id, level.name.c_str(), level.authorName.c_str());
        playOnlineLevel(level, true, [levelId](bool ok, const std::string& err, const ConversionReport& r) {
            if (!ok) log("online: level %d: %s", levelId, err.c_str());
            for (const std::string& w : r.warnings) log("online: level %d: %s", levelId, w.c_str());
        });
    });
}

}  // namespace online
