#pragma once
// LevelSession: the level-selection slice of the iOS -[Session ...] singleton that the editor and
// the user-level screens use (Session.levelData dictionary, chapter/level/character/vehicle
// indices, the LevelMO lookups), bridged onto the Android reconstruction's Settings so that
// Gameplay / CharacterSelectLayer / LevelB2D play a user level unchanged.
//
// iOS facts this reproduces:
//   * Session.levelData (+0x190) is an NSMutableDictionary holding at least "data" (level XML),
//     "force_character", "playable_character" and "id_x"; levelData creates it lazily with
//     id_x = -1; clearLevelData drops it; setLevelDataWithManagedObject: copies every non-nil
//     attribute of the LevelMO into it and sets levelIndex = id_x, chapterIndex =
//     chapter.chapterIndex, characterIndex = playable_character, vehicleIndex = 0.
//   * chapterMOWithIndex: returns nil for 5000/5001 ((index >> 1) == 2500), so Session.levelMO is
//     nil while a user level plays: GameplayLayer never records completion times for user levels
//     and advanceLevelIndex returns NO (advancedFromLastLevel = YES).
//   * Exits from CharacterSelectLayer / GameplayLayer go to MainMenuLayer.scene (not the level
//     select) when chapterIndex is 5000 or 5001; MainMenuLayer.addPerspectiveCharacters resets
//     chapterIndex to 0 in that case.
//
// This is THE Session.levelData of the port: EditorLayer::applyLevelDataToSession (E3) writes
// "data"/"force_character"/"playable_character" here (EditorLayer::sessionLevelData() should
// return LevelSession::getInstance()->levelData()), SaveLevelViewController and the level lists
// call setLevelDataWithManagedObject / clearLevelData.
//
// Android bridge (port): the Android game reads the level XML through
// Settings::getSelectedLevelFilePath() (non-empty = XML text, see Settings.h) and the character
// through Settings' selected character / getForceCharacter(). applyToSettings() writes those;
// clearLevelData() resets the file path to "" so campaign levels load from levelData.plist again.

#include "cocos2d.h"

#include <string>

class ChapterMO;
class LevelMO;

class LevelSession
{
public:
    static LevelSession* getInstance();

    // ---- iOS Session selectors ----
    void setCharacterIndex(int characterIndex);                     // @ios 100042aac
    void setForceCharacter(bool forceCharacter);                    // @ios 100042b50  levelData["force_character"]
    bool forceCharacter();                                          // @ios 100042b94  levelData["force_character"].boolValue
    void setVehicleIndex(int vehicleIndex);                         // @ios 100042cc4
    void setLevelDataWithCurrentIndices();                          // @ios 100042d68  setLevelDataWithManagedObject:(levelMO)
    // nullptr for 5000/5001 (see above); campaign chapters are not modelled by LevelStore, so this
    // port returns nullptr for every index.
    ChapterMO* chapterMOWithIndex(unsigned int chapterIndex);       // @ios 100042d90
    ChapterMO* chapterMO();                                         // @ios 100042eac
    LevelMO* levelMO();                                             // @ios 100042eb4
    LevelMO* levelMOWithIndex(unsigned int levelIndex, ChapterMO* chapter);  // @ios 100042ee4
    void setLevelDataWithManagedObject(LevelMO* level);             // @ios 100042f58
    cocos2d::ValueMap& levelData();                                 // @ios 100043110  lazily {id_x: -1}
    void clearLevelData();                                          // @ios 100043180
    void setLevelDataXML(const std::string& xml);                   // @ios 1000431a8  levelData["data"]
    std::string levelDataXML();                                     // @ios 1000431d4
    int characterIndex() const;                                     // @ios 1000445e8
    int vehicleIndex() const;                                       // @ios 1000445f0
    int chapterIndex() const;                                       // @ios 100044630
    void setChapterIndex(int chapterIndex);                         // @ios 100044638
    int levelIndex() const;                                         // @ios 100044640
    void setLevelIndex(int levelIndex);                             // @ios 1000434d8

    // ---- port ----
    // True while chapterIndex() is 5000 or 5001 (a user/imported level is selected).
    bool isUserLevel() const;
    // Pushes levelData()/characterIndex() into the Android Settings (selected level XML via
    // setSelectedLevelFilePath, setForceCharacter, selected character).
    // RE-TODO: iOS characterIndex vs. Android Characters.plist index -- assumed identical order.
    void applyToSettings();
    // The common tail of -[LoadLevelViewController playBtnPressed:] and
    // -[UserLevelSelectUIView playBtnPressed:]: applyToSettings(), then
    // Director::pushScene(forceCharacter ? Gameplay::createScene(xml, nullptr)
    //                                    : CharacterSelectLayer::createScene(...)).
    void playLevel(bool forceCharacter);
    // For the game hooks (E3): what -[MainMenuLayer addPerspectiveCharacters] does when it finds
    // chapterIndex 5000/5001 (chapterIndex = 0), plus clearLevelData() so the next campaign
    // level loads from levelData.plist. Call when returning to the main menu / level select.
    void leaveUserLevel();

    // ---- port of the iOS import entry points (AppController / AdvancedOptions) ----
    // -[AppController application:openURL:sourceApplication:annotation:] @ios 10001d1ac and its
    // alert -[AppController alertView:clickedButtonAtIndex:] @ios 10001dab4 (tag 0): checks the
    // file (LevelStore::readHappyWheelsFile); too large -> ERROR / LEVEL TOO LARGE / OK; otherwise
    // IMPORT LEVEL? / IMPORT LEVEL MESSAGE with CANCEL (the window's close button) /
    // IMPORT AND PLAY (saveImportedLevel + playImportedLevel) / IMPORT (saveImportedLevel).
    // Returns whether the file was accepted (the iOS return value). PC entry: command line or
    // drag & drop of a .happywheels file.
    bool openHappyWheelsFile(const std::string& path);
    // -[AppController playImportedLevel] @ios 10001d7ac: clearLevelData, levelData = the file's
    // data/playable_character/force_character, chapterIndex 5001, LEVEL INCOMPATIBLE alert when
    // the file's buildVersion > the app's (iOS 1.2.7 CFBundleVersion "0.9080"; the level plays
    // anyway), then replaceScene(force ? gameplay : character select) and dismiss any presented
    // view controller.
    void playImportedLevel(const cocos2d::ValueMap& levelDict);
    // -[AdvancedOptions importLevelsBtnPressed] @ios 10008b83c + alert tag 4
    // (-[AdvancedOptions alertView:clickedButtonAtIndex:] @ios 10008bb54): import the drop folder;
    // "Success" / long message / "Keep them" / "DELETE THEM" (index 1 -> deleteImportedLevelFiles),
    // or "Error" / "Level were not imported. Error: %@" / "DAMMIT.".
    void importLevelsFromFileSharingWithAlert();
    // CFBundleVersion of iOS 1.2.7, written as "buildVersion" by ShareAction.
    static const char* const kBuildVersion;   // "0.9080"

private:
    LevelSession();
    cocos2d::ValueMap _levelData;
    bool _hasLevelData;          // Session+0x190 != nil
    int _characterIndex;         // Session+0x74
    int _chapterIndex;           // Session+0x80
    int _levelIndex;             // Session+0x84
    int _vehicleIndex;
};
