// Verification-only stand-ins used when OW_WITH_EDITOR=OFF (private parity build trees such as
// build_parity/): the level editor (src/editor/) is left out, but the menus reference a few
// LevelSession members. None of these run in --dump-world mode. Not part of the game build.
#include "LevelSession.h"

LevelSession::LevelSession()
    : _hasLevelData(false), _characterIndex(0), _chapterIndex(0), _levelIndex(0), _vehicleIndex(0)
{
}

LevelSession* LevelSession::getInstance()
{
    static LevelSession* instance = new LevelSession();
    return instance;
}

bool LevelSession::isUserLevel() const { return false; }
void LevelSession::clearLevelData() { _levelData.clear(); _hasLevelData = false; }
void LevelSession::setChapterIndex(int chapterIndex) { _chapterIndex = chapterIndex; }
bool LevelSession::openHappyWheelsFile(const std::string&) { return false; }
void LevelSession::importLevelsFromFileSharingWithAlert() {}

// Scene factories of editor classes whose headers are not needed here (link-level stand-ins).
class EditorLayer { public: static cocos2d::Scene* createScene(); };
class UserLevelSelectUIView { public: static cocos2d::Scene* scene(); static cocos2d::Scene* sceneForReturnFromLevel(); };
cocos2d::Scene* EditorLayer::createScene() { return nullptr; }
cocos2d::Scene* UserLevelSelectUIView::scene() { return nullptr; }
cocos2d::Scene* UserLevelSelectUIView::sceneForReturnFromLevel() { return nullptr; }

// NET (PC addition): src/net/ is left out with the editor.
#include "net/NetLevels.h"
void net::startLevelSharing() {}
void net::stopLevelSharing() {}

// EDITOR (browser features, PC addition): --edit has no editor to open here.
#include "editor/flash/FlashEditorHooks.h"
void flashed::openLevelInEditor(const std::string&, const std::string&) {}

// NET (PC addition): the ghost race (src/net/race/) is left out with src/net/.
#include "net/race/RaceHooks.h"
void race::openRaceMenu() {}
bool race::suppressVictoryMenu() { return false; }
