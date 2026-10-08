// LevelSession -- the level-selection slice of the iOS Session, bridged to the Android Settings.
// See LevelSession.h.
//
// Character mapping (verified): iOS "characterIndex" / LevelMO.playable_character / the level
// XML's <info c=".."> are character IDs, not list positions: -[Session dictForCharacterIndex:]
// @ios 100042bb4 returns the Characters.plist entry whose "id" equals the value. The iOS and the
// Android shared/Characters.plist list the same six characters in the same order with the same
// ids (1 whe, 2 biz, 3 dad, 4 sho, 5 mop, 9 pogo), and the Android LevelB2D reads <info c> as an
// id too (Settings::setSelectedCharacterId). So the value goes to Settings::setSelectedCharacterId
// unchanged.
#include "LevelSession.h"
#include "FlashLevelIO.h"  // EDITOR (browser features, PC addition)

#include "CharacterSelectLayer.h"
#include "EditorViewController.h"
#include "GameText.h"
#include "Gameplay.h"
#include "HWWindow.h"
#include "HWWindowDelegate.h"
#include "LevelMO.h"
#include "LevelStore.h"
#include "LevelUIHelpers.h"
#include "MainMenu.h"
#include "Settings.h"
#include "platform/common/Localization.h"
#include "qol/CharacterChoice.h"  // QOL (PC addition)

#include <cstdlib>

USING_NS_CC;

const char* const LevelSession::kBuildVersion = "0.9080";

namespace {

// The UIAlertView delegate role of AppController / AdvancedOptions for the import alerts.
class ImportAlertDelegate : public HWWindowDelegate
{
public:
    enum
    {
        kTagImportLevel = 0,      // AppController alert tag 0
        kTagImportedFiles = 4,    // AdvancedOptions alert tag 4
    };

    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override
    {
        long buttonIndex = levelui::buttonIndex(buttonTag, window);
        if (window->getTag() == kTagImportLevel)
        {
            // @ios 10001dab4
            if (buttonIndex == 1)
            {
                LevelStore::getInstance()->saveImportedLevel(_pendingLevel);
                ValueMap level = _pendingLevel;
                _pendingLevel.clear();
                LevelSession::getInstance()->playImportedLevel(level);
                return;
            }
            if (buttonIndex == 2)
            {
                LevelStore::getInstance()->saveImportedLevel(_pendingLevel);
            }
            else if (buttonIndex != 0)
            {
                return;
            }
            _pendingLevel.clear();
            if (!Director::getInstance()->getRunningScene())
            {
                Director::getInstance()->pushScene(MainMenu::createScene(MenuModeMain, nullptr));
            }
        }
        else if (window->getTag() == kTagImportedFiles)
        {
            // @ios 10008bb54 case 4
            if (buttonIndex == 1)
            {
                LevelStore::getInstance()->deleteImportedLevelFiles();
            }
        }
    }

    ValueMap _pendingLevel;   // AppController+0x20 (the opened file's dictionary)
};

ImportAlertDelegate* importAlertDelegate()
{
    static ImportAlertDelegate delegate;
    return &delegate;
}

}  // namespace

LevelSession* LevelSession::getInstance()
{
    static LevelSession instance;
    return &instance;
}

LevelSession::LevelSession()
    : _hasLevelData(false), _characterIndex(0), _chapterIndex(0), _levelIndex(0), _vehicleIndex(0)
{
}

// @ios 100042aac  (iOS also unloads the vocal sounds and loads <key>_bodies.plist; the Android
// game does that itself when it builds the character)
void LevelSession::setCharacterIndex(int characterIndex)
{
    _characterIndex = characterIndex;
}

// @ios 100042b50
void LevelSession::setForceCharacter(bool forceCharacter)
{
    levelData()["force_character"] = forceCharacter;
}

// @ios 100042b94
bool LevelSession::forceCharacter()
{
    ValueMap& data = levelData();
    auto it = data.find("force_character");
    return it != data.end() && !it->second.isNull() && it->second.asBool();
}

// @ios 100042cc4  (iOS also loads <vehicle>_bodies.plist)
void LevelSession::setVehicleIndex(int vehicleIndex)
{
    _vehicleIndex = vehicleIndex;
}

// @ios 100042d68
void LevelSession::setLevelDataWithCurrentIndices()
{
    setLevelDataWithManagedObject(levelMO());
}

// @ios 100042d90
ChapterMO* LevelSession::chapterMOWithIndex(unsigned int chapterIndex)
{
    if (chapterIndex >> 1 == 2500)  // 5000 / 5001
    {
        return nullptr;
    }
    // Campaign chapters (sorted ChapterMOs, excluding the last two) are the Android Settings'
    // business; LevelStore does not model them.
    return nullptr;
}

// @ios 100042eac
ChapterMO* LevelSession::chapterMO()
{
    return chapterMOWithIndex(static_cast<unsigned int>(_chapterIndex));
}

// @ios 100042eb4
LevelMO* LevelSession::levelMO()
{
    return levelMOWithIndex(static_cast<unsigned int>(_levelIndex), chapterMO());
}

// @ios 100042ee4
LevelMO* LevelSession::levelMOWithIndex(unsigned int levelIndex, ChapterMO* chapter)
{
    if (!chapter || chapter->levels().empty())
    {
        return nullptr;
    }
    Vector<LevelMO*> sorted = chapter->levelsSortedById();
    // objectAtIndex: raises for an index out of range; the port returns nil.
    return levelIndex < sorted.size() ? sorted.at(levelIndex) : nullptr;
}

// @ios 100042f58
void LevelSession::setLevelDataWithManagedObject(LevelMO* level)
{
    // [entity attributesByName] -> copy every non-nil attribute value.
    _levelData.clear();
    _hasLevelData = true;
    qol::setCharacterOverride(0);  // QOL (PC addition): another level is selected
    if (level)
    {
        _levelData["rating"] = level->rating();
        _levelData["comments"] = level->comments();
        _levelData["flags"] = level->flags();
        _levelData["name"] = level->name();
        _levelData["votes"] = level->votes();
        _levelData["plays"] = level->plays();
        _levelData["importable"] = level->importable();
        _levelData["id_x"] = level->id_x();
        _levelData["data"] = level->data();
        _levelData["playable_character"] = level->playable_character();
        _levelData["active_x"] = level->active_x();
        _levelData["rating_total"] = level->rating_total();
        _levelData["force_character"] = level->force_character();
    }
    setLevelIndex(level ? level->id_x() : 0);
    setChapterIndex(level && level->chapter() ? level->chapter()->chapterIndex() : 0);
    auto it = _levelData.find("playable_character");
    setCharacterIndex(it == _levelData.end() ? 0 : it->second.asInt());
    setVehicleIndex(0);
}

// @ios 100043110
ValueMap& LevelSession::levelData()
{
    if (!_hasLevelData)
    {
        _levelData.clear();
        _levelData["id_x"] = -1;
        _hasLevelData = true;
    }
    return _levelData;
}

// @ios 100043180
void LevelSession::clearLevelData()
{
    _levelData.clear();
    _hasLevelData = false;
    qol::setCharacterOverride(0);  // QOL (PC addition): another level is selected
    // Port: the Android game takes a non-empty selected level path as the level XML.
    Settings::getInstance()->setSelectedLevelFilePath("");
}

// @ios 1000431a8
void LevelSession::setLevelDataXML(const std::string& xml)
{
    levelData()["data"] = xml;
}

// @ios 1000431d4
std::string LevelSession::levelDataXML()
{
    ValueMap& data = levelData();
    auto it = data.find("data");
    return it == data.end() || it->second.isNull() ? std::string() : it->second.asString();
}

int LevelSession::characterIndex() const { return _characterIndex; }   // @ios 1000445e8
int LevelSession::vehicleIndex() const { return _vehicleIndex; }       // @ios 1000445f0
int LevelSession::chapterIndex() const { return _chapterIndex; }       // @ios 100044630
void LevelSession::setChapterIndex(int chapterIndex) { _chapterIndex = chapterIndex; }  // @ios 100044638
int LevelSession::levelIndex() const { return _levelIndex; }           // @ios 100044640
void LevelSession::setLevelIndex(int levelIndex) { _levelIndex = levelIndex; }  // @ios 1000434d8

// ---- port ------------------------------------------------------------------------------------

bool LevelSession::isUserLevel() const
{
    return _chapterIndex == LevelStoreChapterUser || _chapterIndex == LevelStoreChapterImported;
}

void LevelSession::applyToSettings()
{
    Settings* settings = Settings::getInstance();
    // EDITOR (browser features, PC addition): user levels saved by the editor are browser level
    // XML; the game plays them converted (FlashLevelConverter), like online levels.
    settings->setSelectedLevelFilePath(flashed::playableLevelXml(levelDataXML()));
    settings->setForceCharacter(forceCharacter());
    // QOL (PC addition): a level that lets the player pick no longer replaces the player's own
    // choice with its default character (character select starts on the player's character);
    // a forcing one remembers it (qol/CharacterChoice.h).
    if (!forceCharacter())
    {
        qol::restorePlayerCharacter();
        return;
    }
    qol::rememberPlayerCharacter();
    // characterIndex is a character id (see the top of this file).
    settings->setSelectedCharacterId(_characterIndex);
}

void LevelSession::playLevel(bool forceCharacter)
{
    applyToSettings();
    Scene* scene = forceCharacter ? Gameplay::createScene(Settings::getInstance()->getSelectedLevelFilePath(), nullptr)
                                  : CharacterSelectLayer::createScene(0, 0);
    Director::getInstance()->pushScene(scene);
}

void LevelSession::leaveUserLevel()
{
    if (isUserLevel())
    {
        setChapterIndex(0);   // -[MainMenuLayer addPerspectiveCharacters]
        qol::restorePlayerCharacter();  // QOL (PC addition): the player's own character again
    }
    clearLevelData();
}

// @ios 10001d1ac
bool LevelSession::openHappyWheelsFile(const std::string& path)
{
    ValueMap levelDict;
    std::string errorKey;
    if (!LevelStore::readHappyWheelsFile(path, levelDict, &errorKey))
    {
        if (errorKey.empty())
        {
            return false;  // not a .happywheels file
        }
        levelui::showAlert(-1, Localization::get("ERROR"), Localization::get(errorKey),
                           Localization::get("OK"), "", nullptr);
        return false;
    }
    importAlertDelegate()->_pendingLevel = levelDict;
    levelui::showAlert(ImportAlertDelegate::kTagImportLevel, Localization::get("IMPORT LEVEL?"),
                       Localization::get("IMPORT LEVEL MESSAGE"), Localization::get("CANCEL"),
                       Localization::get("IMPORT AND PLAY"), importAlertDelegate(),
                       Localization::get("IMPORT"));
    return true;
}

// @ios 10001d7ac
void LevelSession::playImportedLevel(const ValueMap& levelDict)
{
    auto get = [&levelDict](const char* key) -> Value {
        auto it = levelDict.find(key);
        return it == levelDict.end() ? Value() : it->second;
    };
    clearLevelData();
    setLevelDataXML(get("data").isNull() ? std::string() : get("data").asString());
    setCharacterIndex(get("playable_character").asInt());
    setVehicleIndex(0);
    bool force = get("force_character").asBool();
    setForceCharacter(force);
    setChapterIndex(LevelStoreChapterImported);

    float fileVersion = get("buildVersion").isNull() ? 0.0f : get("buildVersion").asFloat();
    float appVersion = static_cast<float>(std::atof(kBuildVersion));
    applyToSettings();
    Scene* scene = force ? Gameplay::createScene(Settings::getInstance()->getSelectedLevelFilePath(), nullptr)
                         : CharacterSelectLayer::createScene(0, 0);
    if (appVersion < fileVersion)
    {
        // The UIAlertView outlives the scene change on iOS: put the window into the new scene.
        HWWindow* window = HWWindow::create(HWWindowAppearanceAlert, nullptr, false, false);
        window->showAlertMessage(Localization::get("LEVEL INCOMPATIBLE"),
                                 Localization::get("LEVEL INCOMPATIBLE MESSAGE"),
                                 Localization::get("OK"), "", true);
        scene->addChild(window, uikit::kWindowZOrder + 1);
    }
    // iOS wraps the scene in WorkAroundLayer / InterstitialLayer (ads) and stops the music.
    Director* director = Director::getInstance();
    if (!director->getRunningScene())
    {
        director->pushScene(scene);
    }
    else
    {
        director->replaceScene(scene);
    }
    EditorViewController::dismissRootPresented(false, nullptr);
}

// @ios 10008b83c
void LevelSession::importLevelsFromFileSharingWithAlert()
{
    std::string error = LevelStore::getInstance()->importLevelsFromFileSharing();
    if (error.empty())
    {
        levelui::showAlert(ImportAlertDelegate::kTagImportedFiles, "Success",
                           OW_IOSTEXT(editorLevelsImportedMessage, 0x1015de700), "Keep them",
                           "DELETE THEM", importAlertDelegate());
    }
    else
    {
        levelui::showAlert(-1, "Error",
                           StringUtils::format("Level were not imported. Error: %s", error.c_str()),
                           "DAMMIT.", "", nullptr);
    }
}
