#pragma once

#include "cocos2d.h"
#include "HWWindow.h" // HWWindowAppearance

#include <string>
#include <vector>

class AdController;
class HWWindow;
class HWWindowDelegate;
class IAPController;
class Obfuscation;
class Session;
class SoundController;
class Tracker;
class UserProgress;

// Process-wide game state singleton (no base class, no RTTI, arm64 sizeof 0x190; created with
// new (std::nothrow) by getInstance and never destroyed).
//
// Owns the SoundController, UserProgress, Tracker and Obfuscation (all created in the constructor) and,
// lazily, the AdController and IAPController. Holds the selected chapter/level (the chapter is
// persisted in UserDefault "selected_chapter"), the parsed levels/levelData.plist and Characters.plist,
// the selected character's dictionary and the XML text of the selected level.
//
// Level data model (levels/levelData.plist):
//   root["chapters"]  : ValueVector of chapter dictionaries; each has "index" (int), "name" (string),
//                       "levels" (ValueVector of level dictionaries)
//   level dictionary  : "dataFile" (level XML file name, relative to "levels/"), "locked" (bool),
//                       "name" (string), ...
// Characters.plist: ValueVector of character dictionaries with "id" (int), "controls" (int), ...
//
// Selected level XML: updateLevelDataFromFile() stores the *contents* of "levels/" + dataFile in
// _levelXMLData (+0x80); LevelB2D::init(std::string xml) falls back to getLevelXMLData() when it is
// given an empty string. getSelectedLevelFilePath() returns _selectedLevelFilePath (+0x150), which the
// constructor sets to "" and nothing in 1.1.3 ever changes (setSelectedLevelFilePath has no callers).
// Gameplay::init / CharacterSelectLayer pass that empty string down to LevelB2D::init, so the parser
// always receives getLevelXMLData(). Despite its name a non-empty value would be parsed as XML text.
class Settings
{
public:
    static Settings* getInstance();                                            // @00610134

    Settings();                                                                // @0061019c
    ~Settings();                                                               // @006108b4

    void initUserProgress();                                                   // @006104b4
    // DPI and screen size in inches from Device::getDPI() and the window size in pixels.
    void calculateScreenDimensions();                                          // @00610500
    void initAdNetwork();                                                      // @006105d4 (empty)

    int getSelectedChapter();                                                  // @006105d8
    int getSelectedLevel();                                                    // @006105e0
    // Only acts when (chapter, level) changes: stores both, updateLevelDataFromFile(), then
    // UserDefault "selected_chapter" = chapter and flush().
    void setSelectedLevel(int chapter, int level);                             // @006105e8
    // (-1 chapter or level -> 0,0); _levelXMLData = getStringFromFile("levels/" + level["dataFile"]).
    void updateLevelDataFromFile();                                            // @00610654

    bool isTablet();                                                           // @0061085c (false)
    bool isFourInch();                                                         // @00610864 (winSize.width > 480)
    float getTabletControlsScale();                                            // @00610890
    bool getShouldDisplayPersistentBannerDuringGameplay();                     // @006108ac (false)

    void setCurrentSession(Session* session);                                  // @00610b0c
    Session* getCurrentSession();                                              // @00610b14
    // Session::die() + release, clears the session and the debug log label.
    void killSession();                                                        // @00610b1c

    SoundController* getSoundController();                                     // @00610b5c
    AdController* getAdController();                                           // @00610b64 (lazy new)
    IAPController* getIAPController();                                         // @00610bb4 (lazy new)

    std::vector<float> getCompletionTimes(int chapter, int level);             // @00610c04 (stub: empty)
    void addCompletionTime(int chapter, int level, float time);                // @00610c10 (stub)
    void saveUserProgress();                                                   // @00610c14 (stub)

    // levelData.plist "chapters" (reloads the plist when forced or when not loaded yet). Every caller
    // in 1.1.3 passes false.
    cocos2d::ValueVector getAllChaptersData(bool forceReload = false);         // @00610c18
    // Characters.plist (loaded once into _charactersData), returned by value.
    cocos2d::ValueVector getAllCharactersData();                               // @00610f10
    // Copy of _selectedCharacterData; selects index 0 first when it has no "id" yet.
    cocos2d::ValueMap getSelectedCharacterData();                              // @00611160
    // characterIndex == -1: select the character whose "id" == characterId, else by index.
    void updateSelectedCharacterData(int characterIndex, int characterId);     // @00611254
    // getSelectedCharacterData()["controls"].asInt() (GameplayControls ControlsType value).
    int getSelectedCharacterControlType();                                     // @006114ac
    void setSelectedCharacterIndex(int characterIndex);                        // @006115dc
    void setSelectedCharacterId(int characterId);                              // @006115e4
    int getSelectedCharacterIndex();                                           // @006115f0
    int getSelectedCharacterId();                                              // @00611894

    void setSelectedLevelFilePath(std::string path);                           // @006119b8
    std::string getSelectedLevelFilePath();                                    // @006119c0

    bool getForceCharacter();                                                  // @006119cc
    void setForceCharacter(bool forceCharacter);                               // @006119d4

    // The chapter dictionary whose "index" == chapterIndex (empty map if none).
    cocos2d::ValueMap getChapterData(int chapterIndex, bool forceReload = false); // @006119dc
    // RESTORED (PC addition): chapter "index" <-> position in getAllChaptersData(). The original
    // chapters have index == position (1.1.3 relies on it); OpenWheels' campaign chapters
    // appended after them (src/restored, index 100+) do not. -1 when the index is not listed;
    // the position itself for an entry without "index".
    int getChapterPosition(int chapterIndex);
    int getChapterIndexAt(int position);
    // getChapterData(chapter)["levels"][level] (empty map if missing).
    cocos2d::ValueMap getLevelData(int chapter, int level);                    // @00611bec
    // XML text of the selected level (loads it when _levelXMLData is empty).
    std::string getLevelXMLData();                                             // @00611e88
    // getLevelData(...)["dataFile"]; (-1, -1) means the selected level.
    std::string getLevelFilePath(int chapter, int level);                      // @00611ecc
    // Moves to the next unlocked level of the chapter; false (and _advancedFromLastLevel = true when the
    // chapter is exhausted) otherwise. Clears _levelXMLData.
    bool advanceLevelIndex();                                                  // @00612010
    // Levels without "locked" are always unlocked; otherwise UserProgress::isLevelUnlocked.
    bool isLevelUnlocked(int chapter, int level);                              // @00612278
    bool advanceChapterIndex();                                                // @006123ec

    void setAdvancedFromLastLevel(bool advancedFromLastLevel);                 // @006124a4
    bool getAdvancedFromLastLevel();                                           // @006124ac
    void setLevelWasCompleted(bool levelWasCompleted);                         // @006124b4
    bool getLevelWasCompleted();                                               // @006124bc

    bool getAllLevelsCompletedForChapter(int chapter);                         // @006124c4
    int getNumberOfLevelsCompletedForChapter(int chapter);                     // @006124ec (stub: 0)
    // Records a finish time for the selected level (UserProgress, flushed); returns its rank 1..4 or 0.
    int addCompletionTime(float time);                                         // @006124f4
    bool isLevelCompleted(int chapter, int level);                             // @00612510
    void setIsLevelUnlocked(int chapter, int level, bool unlocked);            // @00612518

    void setDebugLogLabel(cocos2d::Label* label);                              // @00612520
    // Appends "\n" + text to _debugLog and shows it on the debug label (when one is set).
    void debugLogString(std::string text);                                     // @00612528

    // HWWindow::create(...), added to the running scene at globals::ui::alertWindowDepth.
    HWWindow* createWindow(HWWindowAppearance appearance, HWWindowDelegate* delegate, bool addCloseBtn,
                           bool addMascot);                                    // @00612710
    void showBetaTestingFeatureDisabledWindow();                               // @0061278c

    void showBanner();                                                         // @00612980 (empty)
    void hideBanner();                                                         // @00612984 (empty)
    void showInterstitial();                                                   // @00612988 (empty)

    // Stores an alert to be shown later by OptionsMenu::update / PauseLayer::update (IAP restore
    // results can arrive while the app is in the background).
    void cacheAlertMessage(std::string title, std::string message, std::string confirmLabel,
                           std::string cancelLabel, bool animated);            // @0061298c

    UserProgress* getUserProgress();                                           // @00612a00 (lazy new)
    Tracker* getTracker();                                                     // @00612a50

    // ---- cached alert (written by cacheAlertMessage) ---------------------------------------------
    // Public: OptionsMenu::update (@005fc964) and PauseLayer::update read and clear these directly:
    // if (_hasCachedAlertMessage) { _hasCachedAlertMessage = false; createWindow(...)->showAlertMessage(
    // _cachedAlertTitle, _cachedAlertMessage, _cachedAlertConfirmLabel, "", true); }
    bool _hasCachedAlertMessage;                 // +0x000
    std::string _cachedAlertTitle;               // +0x008
    std::string _cachedAlertMessage;             // +0x020
    std::string _cachedAlertConfirmLabel;        // +0x038
    std::string _cachedAlertCancelLabel;         // +0x050
    bool _cachedAlertAnimated;                   // +0x068

private:
    // RE-TODO: 8 bytes at +0x070 are never initialised or accessed in 1.1.3 (layout placeholder so
    // _selectedChapter stays at +0x078).
    void* _unk0x70;                              // +0x070
    int _selectedChapter;                        // +0x078 (UserDefault "selected_chapter")
    int _selectedLevel;                          // +0x07c (-1 at startup)
    std::string _levelXMLData;                   // +0x080 contents of the selected level's XML file
    bool _forceCharacter;                        // +0x098
    int _dpi;                                    // +0x09c Device::getDPI()
    float _screenWidthInches;                    // +0x0a0 winSizeInPixels.width / dpi
    float _screenHeightInches;                   // +0x0a4 winSizeInPixels.height / dpi
    float _screenDiagonalInches;                 // +0x0a8 (int)(sqrt(w*w + h*h) * 100) / 100
    Session* _currentSession;                    // +0x0b0
    SoundController* _soundController;           // +0x0b8
    AdController* _adController;                 // +0x0c0
    cocos2d::ValueMap _levelData;                // +0x0c8 levels/levelData.plist
    cocos2d::ValueVector _charactersData;        // +0x0f0 Characters.plist
    cocos2d::ValueMap _selectedCharacterData;    // +0x108
    // RE-TODO: one byte at +0x130 is never initialised or accessed (layout placeholder).
    bool _unk0x130;                              // +0x130
    bool _advancedFromLastLevel;                 // +0x131
    bool _levelWasCompleted;                     // +0x132
    UserProgress* _userProgress;                 // +0x138
    Tracker* _tracker;                           // +0x140
    IAPController* _iapController;               // +0x148
    std::string _selectedLevelFilePath;          // +0x150 always "" in 1.1.3 (see class comment)
    cocos2d::Label* _debugLogLabel;              // +0x168
    std::string _debugLog;                       // +0x170
    Obfuscation* _obfuscation;                   // +0x188
};
