#pragma once
// Download + convert + play an online level through the user-level path the editor uses
// (LevelSession, chapter 5001: no completion times, exits to the main menu - or to the menu that
// started it, see LevelReturn).

#include <functional>
#include <string>

#include "cocos2d.h"
#include "online/FlashLevelConverter.h"
#include "online/OnlineLevel.h"

namespace online {

// Fetches the record (cached after the first time; countPlay as in HWApi::downloadLevel),
// converts it and starts it: character select unless the level forces its character.
// done(ok, error, report) runs before the scene change (ok == false: nothing started).
void playOnlineLevel(const OnlineLevelInfo& level, bool countPlay,
                     std::function<void(bool ok, const std::string& error, const ConversionReport& report)> done);

// Converts a downloaded browser-format level and starts it (the second half of the above).
// chooseCharacter (QOL, PC addition: "any character", qol/CharacterChoice.h): character select
// first even when the level forces its character.
bool startConvertedLevel(const std::string& flashXml, ConversionReport* report, std::string* error,
                         bool chooseCharacter = false);

// ONLINE (PC addition): the way back to a menu that started a level over its own scene
// (LevelSession::playLevel pushes the level). The online browser and Your Levels keep one each;
// MainMenu::createScene asks them before it builds the main menu, so EXIT, back from character
// select and NEXT without a next level come back to that menu. Restarts, VIEW REPLAY and CHANGE
// CHARACTER keep it: only leaving the level (MainMenu::createScene) takes it.
struct LevelReturn {
    bool pending = false;
    cocos2d::RefPtr<cocos2d::Scene> parked;   // the menu scene the level was pushed over

    void park();     // before the level is pushed: the running scene is the menu
    void cancel();
    // Once, when pending and a user level is being left: drops the parked scene from the
    // Director's stack (it sits right below the running scene) and leaves the user level
    // (LevelSession::leaveUserLevel). The caller then shows a fresh scene of the menu.
    bool take();
};

// PC entry point (--play-online <id>): metadata, then playOnlineLevel. Errors go to the log.
void playOnlineLevelById(int levelId);

// EDITOR (browser features, PC addition): "open in editor" for a downloaded level. The level
// editor (src/editor/) registers the handler at startup; without it (editor-less builds) the
// browser shows no editor button.
using OpenInEditorHandler = std::function<void(const std::string& flashXml, const OnlineLevelInfo& level)>;
void setOpenInEditorHandler(OpenInEditorHandler handler);
const OpenInEditorHandler& openInEditorHandler();

}  // namespace online
