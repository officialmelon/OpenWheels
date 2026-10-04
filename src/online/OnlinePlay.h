#pragma once
// Download + convert + play an online level through the user-level path the editor uses
// (LevelSession, chapter 5001: no completion times, exits to the main menu).

#include <functional>
#include <string>

#include "online/FlashLevelConverter.h"
#include "online/OnlineLevel.h"

namespace online {

// Fetches the record (cached after the first time; countPlay as in HWApi::downloadLevel),
// converts it and starts it: character select unless the level forces its character.
// done(ok, error, report) runs before the scene change (ok == false: nothing started).
void playOnlineLevel(const OnlineLevelInfo& level, bool countPlay,
                     std::function<void(bool ok, const std::string& error, const ConversionReport& report)> done);

// Converts a downloaded browser-format level and starts it (the second half of the above).
bool startConvertedLevel(const std::string& flashXml, ConversionReport* report, std::string* error);

// PC entry point (--play-online <id>): metadata, then playOnlineLevel. Errors go to the log.
void playOnlineLevelById(int levelId);

// EDITOR (browser features, PC addition): "open in editor" for a downloaded level. The level
// editor (src/editor/) registers the handler at startup; without it (editor-less builds) the
// browser shows no editor button.
using OpenInEditorHandler = std::function<void(const std::string& flashXml, const OnlineLevelInfo& level)>;
void setOpenInEditorHandler(OpenInEditorHandler handler);
const OpenInEditorHandler& openInEditorHandler();

}  // namespace online
