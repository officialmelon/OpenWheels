#pragma once
// ShareAction (iOS 1.2.7): exports a level for sharing. Used by -[LoadLevelViewController
// shareBtnPressed:] and -[EditorMenuViewController shareBtnPressed:] (tracker "editor" /
// "share_level"). Class methods only.
//
// The .happywheels file (shareLevelDataFile...) is an XML plist dictionary:
//   buildVersion  CFBundleVersion of the exporting app (string)
//   name          level name, "Untitled" when nil
//   comments      level description, "" when nil
//   data          level XML
//   force_character    bool
//   playable_character int
// written to Documents/"%@.happywheels" (the level name) and offered through a
// UIActivityViewController together with the SHARE LEVEL MESSAGE text. The importer is
// LevelStore::readHappyWheelsFile / saveImportedLevel (AppController on iOS); creatorName is
// accepted but never stored.
//
// Port: there is no share sheet on the desktop. The file goes to LevelStore::sharedPath() and an
// HWWindow tells the player SHARE LEVEL MESSAGE plus the written path. The URL variants
// (happywheels://playlevel?...) only ever reached the share sheet; they build the same string and
// show it the same way.

#include "cocos2d.h"

#include <string>

class ShareAction
{
public:
    // UIActivityViewController with [url] -> port: HWWindow showing `url`.
    static void shareLevelWithURL(const std::string& url, cocos2d::Node* viewController);  // @ios 10006533c
    // "%@\n\nhappywheels://playlevel/?fc=%i&ci=%i&ld=%@" (SHARE LEVEL MESSAGE, forceCharacter,
    // characterIndex, percent-escaped level data) -> shareLevelWithURL (no callers in 1.2.7).
    static void shareLevelData(const std::string& levelData, unsigned int characterIndex,
                               bool forceCharacter, cocos2d::Node* viewController);         // @ios 1000653b8
    // Writes the .happywheels plist described above; returns the written path ("" on failure).
    static std::string shareLevelDataFile(const std::string& levelData, unsigned int characterIndex,
                                          bool forceCharacter, const std::string& levelName,
                                          const std::string& creatorName,
                                          const std::string& levelComments,
                                          cocos2d::Node* viewController);                    // @ios 100065460
};
