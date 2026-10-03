#include "Settings.h"

#include "qol/QoL.h"  // QOL (PC addition)

#include "AdController.h"
#include "GameText.h"
#include "Globals.h"
#include "HWWindow.h"
#include "IAPController.h"
#include "Obfuscation.h"
#include "Session.h"
#include "SoundController.h"
#include "Tracker.h"
#include "UserProgress.h"

#include <cmath>

USING_NS_CC;

// Unused file-scope strings of this TU (dynamically initialised by _INIT_18 @00612a58, in this order).
// Leftovers of an earlier plist-based save format; nothing in 1.1.3 reads them.
static std::string s_userProgressFileName = "userProgress.plist";                  // @00ac6560
static std::string s_uniqueFolderName = ".Happy Wheels Unique Folder Name";        // @00ac6580

// The singleton pointer (file-static, no symbol).
static Settings* s_sharedSettings = nullptr;                                       // @00ac6598

// @00610134
Settings* Settings::getInstance()
{
    if (!s_sharedSettings)
    {
        s_sharedSettings = new (std::nothrow) Settings();
    }
    return s_sharedSettings;
}

// @0061019c
Settings::Settings()
{
    // port: the original never sets this; it relies on fresh (zeroed) bionic heap memory from
    // operator new(400). The Win32 heap gives no such guarantee, and a garbage `true` opens an
    // empty alert on the first pause.
    _hasCachedAlertMessage = false;
    _selectedChapter = UserDefault::getInstance()->getIntegerForKey("selected_chapter");
    _currentSession = nullptr;
    _selectedLevel = -1;
    _selectedLevelFilePath.assign("");
    _advancedFromLastLevel = false;
    _forceCharacter = false;
    _userProgress = nullptr;
    _adController = nullptr;
    _iapController = nullptr;
    _debugLogLabel = nullptr;
    _soundController = new (std::nothrow) SoundController();
    initUserProgress();
    calculateScreenDimensions();
    _tracker = new Tracker();
    _obfuscation = new Obfuscation();
    _obfuscation->init();
}

// @006104b4
void Settings::initUserProgress()
{
    if (!_userProgress)
    {
        _userProgress = new UserProgress();
    }
}

// @00610500
void Settings::calculateScreenDimensions()
{
    Director* director = Director::getInstance();
    _dpi = Device::getDPI();
    // Two more getWinSizeInPixels() calls whose results are not used.
    director->getWinSizeInPixels();
    director->getWinSizeInPixels();
    _screenWidthInches = director->getWinSizeInPixels().width / (float)_dpi;
    _screenHeightInches = director->getWinSizeInPixels().height / (float)_dpi;
    _screenDiagonalInches = roundf(sqrtf(_screenWidthInches * _screenWidthInches
                                         + _screenHeightInches * _screenHeightInches) * 100.0f) / 100.0f;
}

// @006105d4
void Settings::initAdNetwork()
{
}

// @006105d8
int Settings::getSelectedChapter()
{
    return _selectedChapter;
}

// @006105e0
int Settings::getSelectedLevel()
{
    return _selectedLevel;
}

// @006105e8
void Settings::setSelectedLevel(int chapter, int level)
{
    if (_selectedChapter == chapter && _selectedLevel == level)
    {
        return;
    }
    _selectedChapter = chapter;
    _selectedLevel = level;
    updateLevelDataFromFile();
    UserDefault* userDefault = UserDefault::getInstance();
    userDefault->setIntegerForKey("selected_chapter", chapter);
    userDefault->flush();
}

// @00610654
void Settings::updateLevelDataFromFile()
{
    if (_selectedChapter == -1 || _selectedLevel == -1)
    {
        _selectedChapter = 0;
        _selectedLevel = 0;
    }
    ValueMap levelData = getLevelData(_selectedChapter, _selectedLevel);
    std::string dataFile = levelData["dataFile"].asString();
    _levelXMLData = FileUtils::getInstance()->getStringFromFile("levels/" + dataFile);
}

// @0061085c
bool Settings::isTablet()
{
    return false;
}

// @00610864
bool Settings::isFourInch()
{
    return Director::getInstance()->getWinSize().width > 480.0f;
}

// @00610890
float Settings::getTabletControlsScale()
{
    if (_screenDiagonalInches <= 8.5f)
    {
        return 1.0f;
    }
    return 8.5f / _screenDiagonalInches;
}

// @006108ac
bool Settings::getShouldDisplayPersistentBannerDuringGameplay()
{
    return false;
}

// @006108b4
Settings::~Settings()
{
    Director::getInstance()->getScheduler()->unscheduleUpdate(this);
    CC_SAFE_DELETE(_tracker);
    CC_SAFE_DELETE(_iapController);
    CC_SAFE_DELETE(_adController);
    CC_SAFE_DELETE(_obfuscation);
    CC_SAFE_DELETE(_soundController);
    CC_SAFE_DELETE(_userProgress);
}

// @00610b0c
void Settings::setCurrentSession(Session* session)
{
    _currentSession = session;
}

// @00610b14
Session* Settings::getCurrentSession()
{
    return _currentSession;
}

// @00610b1c
void Settings::killSession()
{
    if (_currentSession)
    {
        _currentSession->die();
        _currentSession->removeFromParent();
        _currentSession = nullptr;
    }
    _debugLogLabel = nullptr;
}

// @00610b5c
SoundController* Settings::getSoundController()
{
    return _soundController;
}

// @00610b64
AdController* Settings::getAdController()
{
    if (!_adController)
    {
        _adController = new AdController();
    }
    return _adController;
}

// @00610bb4
IAPController* Settings::getIAPController()
{
    if (!_iapController)
    {
        _iapController = new IAPController();
    }
    return _iapController;
}

// @00610c04
std::vector<float> Settings::getCompletionTimes(int chapter, int level)
{
    return std::vector<float>();
}

// @00610c10
void Settings::addCompletionTime(int chapter, int level, float time)
{
}

// @00610c14
void Settings::saveUserProgress()
{
}

// @00610c18
ValueVector Settings::getAllChaptersData(bool forceReload)
{
    if (forceReload || _levelData["chapters"].getType() == Value::Type::NONE)
    {
        std::string fullPath = FileUtils::getInstance()->fullPathForFilename("levels/levelData.plist");
        _levelData = FileUtils::getInstance()->getValueMapFromFile(fullPath.c_str());
    }
    return _levelData["chapters"].asValueVector();
}

// @00610f10
ValueVector Settings::getAllCharactersData()
{
    if (_charactersData.empty())
    {
        std::string fullPath = FileUtils::getInstance()->fullPathForFilename("Characters.plist");
        _charactersData = FileUtils::getInstance()->getValueVectorFromFile(fullPath.c_str());
    }
    return _charactersData;
}

// @00611160
ValueMap Settings::getSelectedCharacterData()
{
    if (_selectedCharacterData["id"].getType() == Value::Type::NONE)
    {
        updateSelectedCharacterData(0, -1);
    }
    return _selectedCharacterData;
}

// @00611254
void Settings::updateSelectedCharacterData(int characterIndex, int characterId)
{
    ValueVector characters = getAllCharactersData();
    if (characterIndex == -1)
    {
        for (size_t i = 0; i < characters.size(); i++)
        {
            ValueMap character = characters[i].asValueMap();
            // No break: the last match wins.
            if (character["id"].asInt() == characterId)
            {
                _selectedCharacterData = character;
            }
        }
    }
    else
    {
        _selectedCharacterData = characters[characterIndex].asValueMap();
    }
}

// @006114ac
int Settings::getSelectedCharacterControlType()
{
    return getSelectedCharacterData()["controls"].asInt();
}

// @006115dc
void Settings::setSelectedCharacterIndex(int characterIndex)
{
    updateSelectedCharacterData(characterIndex, -1);
}

// @006115e4
void Settings::setSelectedCharacterId(int characterId)
{
    updateSelectedCharacterData(-1, characterId);
}

// @006115f0
int Settings::getSelectedCharacterIndex()
{
    ValueVector characters = getAllCharactersData();
    ValueMap selectedCharacter = getSelectedCharacterData();
    int selectedId = selectedCharacter["id"].asInt();
    for (size_t i = 0; i < characters.size(); i++)
    {
        ValueMap character = characters[i].asValueMap();
        if (character["id"].asInt() == selectedId)
        {
            return (int)i;
        }
    }
    return 0;
}

// @00611894
int Settings::getSelectedCharacterId()
{
    return getSelectedCharacterData()["id"].asInt();
}

// @006119b8
void Settings::setSelectedLevelFilePath(std::string path)
{
    _selectedLevelFilePath = path;
}

// @006119c0
std::string Settings::getSelectedLevelFilePath()
{
    return _selectedLevelFilePath;
}

// @006119cc
bool Settings::getForceCharacter()
{
    return _forceCharacter;
}

// @006119d4
void Settings::setForceCharacter(bool forceCharacter)
{
    _forceCharacter = forceCharacter;
}

// @006119dc
ValueMap Settings::getChapterData(int chapterIndex, bool forceReload)
{
    ValueVector chapters = getAllChaptersData(forceReload);
    for (size_t i = 0; i < chapters.size(); i++)
    {
        ValueMap chapter = chapters[i].asValueMap();
        if (chapter["index"].asInt() == chapterIndex)
        {
            return chapter;
        }
    }
    return ValueMap();
}

// @00611bec
ValueMap Settings::getLevelData(int chapter, int level)
{
    ValueMap chapterData = getChapterData(chapter, false);
    if (chapterData["levels"].getType() == Value::Type::NONE)
    {
        return ValueMap();
    }
    ValueVector levels = chapterData["levels"].asValueVector();
    // The level index is not range checked (only for an empty list).
    if (levels.empty() || levels[level].getType() == Value::Type::NONE)
    {
        return ValueMap();
    }
    return levels[level].asValueMap();
}

// @00611e88
std::string Settings::getLevelXMLData()
{
    if (_levelXMLData.empty())
    {
        updateLevelDataFromFile();
    }
    return _levelXMLData;
}

// @00611ecc
std::string Settings::getLevelFilePath(int chapter, int level)
{
    if ((chapter & level) == -1)
    {
        chapter = _selectedChapter;
        level = _selectedLevel;
    }
    return getLevelData(chapter, level)["dataFile"].asString();
}

// @00612010
bool Settings::advanceLevelIndex()
{
    ValueMap chapter = getAllChaptersData(false)[_selectedChapter].asValueMap();
    ValueVector levels = chapter["levels"].asValueVector();
    if (_selectedLevel + 1 < (int)levels.size())
    {
        _advancedFromLastLevel = false;
        if (isLevelUnlocked(_selectedChapter, _selectedLevel + 1))
        {
            _selectedLevel++;
            _levelXMLData.assign("");
            return true;
        }
    }
    else
    {
        _advancedFromLastLevel = true;
        _levelXMLData.assign("");
    }
    return false;
}

// @00612278
bool Settings::isLevelUnlocked(int chapter, int level)
{
    if (qol::unlockAllLevels())  // QOL (PC addition): "unlock all levels"
    {
        return true;
    }
    ValueMap levelData = getLevelData(chapter, level);
    if (levelData["locked"].asBool())
    {
        return _userProgress->isLevelUnlocked(chapter, level);
    }
    return true;
}

// @006123ec
// Returns whether a next chapter existed (cset w0,hi @0061247c) - LevelSelectMenu uses it.
bool Settings::advanceChapterIndex()
{
    ValueVector chapters = getAllChaptersData(false);
    const size_t next = (size_t)(_selectedChapter + 1);
    if (next < chapters.size())
    {
        _selectedChapter++;
        _selectedLevel = 0;
    }
    return next < chapters.size();
}

// @006124a4
void Settings::setAdvancedFromLastLevel(bool advancedFromLastLevel)
{
    _advancedFromLastLevel = advancedFromLastLevel;
}

// @006124ac
bool Settings::getAdvancedFromLastLevel()
{
    return _advancedFromLastLevel;
}

// @006124b4
void Settings::setLevelWasCompleted(bool levelWasCompleted)
{
    _levelWasCompleted = levelWasCompleted;
}

// @006124bc
bool Settings::getLevelWasCompleted()
{
    return _levelWasCompleted;
}

// @006124c4
bool Settings::getAllLevelsCompletedForChapter(int chapter)
{
    return _userProgress->getPercentageOfLevelsOfChapterCompleted(chapter, true) == 1.0f;
}

// @006124ec
int Settings::getNumberOfLevelsCompletedForChapter(int chapter)
{
    return 0;
}

// @006124f4
int Settings::addCompletionTime(float time)
{
    _levelWasCompleted = true;
    return _userProgress->addCompletionTime(time, _selectedChapter, _selectedLevel, true);
}

// @00612510
bool Settings::isLevelCompleted(int chapter, int level)
{
    return _userProgress->isLevelCompleted(chapter, level);
}

// @00612518
void Settings::setIsLevelUnlocked(int chapter, int level, bool unlocked)
{
    _userProgress->setIsLevelUnlocked(chapter, level, unlocked);
}

// @00612520
void Settings::setDebugLogLabel(Label* label)
{
    _debugLogLabel = label;
}

// @00612528
void Settings::debugLogString(std::string text)
{
    if (!_debugLogLabel)
    {
        return;
    }
    if (_debugLog.empty())
    {
        _debugLog = text;
    }
    else
    {
        _debugLog = _debugLog + "\n" + text;
    }
    _debugLogLabel->setString(_debugLog);
}

// @00612710
HWWindow* Settings::createWindow(HWWindowAppearance appearance, HWWindowDelegate* delegate, bool addCloseBtn,
                                 bool addMascot)
{
    Scene* runningScene = Director::getInstance()->getRunningScene();
    HWWindow* window = HWWindow::create(appearance, delegate, addCloseBtn, addMascot);
    runningScene->addChild(window, globals::ui::alertWindowDepth);
    return window;
}

// @0061278c
void Settings::showBetaTestingFeatureDisabledWindow()
{
    HWWindow* window = createWindow(HWWindowAppearanceAlert, nullptr, true, true);
    window->showAlertMessage("Feature disabled", OW_GAMETEXT(settings_beta_feature_disabled, 0x0040c818), "Ok", "",
                             true);
}

// @00612980
void Settings::showBanner()
{
}

// @00612984
void Settings::hideBanner()
{
}

// @00612988
void Settings::showInterstitial()
{
}

// @0061298c
void Settings::cacheAlertMessage(std::string title, std::string message, std::string confirmLabel,
                                 std::string cancelLabel, bool animated)
{
    _hasCachedAlertMessage = true;
    _cachedAlertTitle = title;
    _cachedAlertMessage = message;
    _cachedAlertConfirmLabel = confirmLabel;
    _cachedAlertCancelLabel = cancelLabel;
    _cachedAlertAnimated = animated;
}

// @00612a00
UserProgress* Settings::getUserProgress()
{
    if (!_userProgress)
    {
        _userProgress = new UserProgress();
    }
    return _userProgress;
}

// @00612a50
Tracker* Settings::getTracker()
{
    return _tracker;
}
