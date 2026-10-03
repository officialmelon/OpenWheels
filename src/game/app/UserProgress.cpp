#include "UserProgress.h"

#include "Patch.h"
#include "Settings.h"

#include <cmath>
#include <sstream>

USING_NS_CC;

// @0063799c
UserProgress::UserProgress()
{
    _levelsPerChapter = 15;
    loadRewardData();
}

// @00637a08
void UserProgress::loadRewardData()
{
    if (_rewardData["loaded"].getType() == Value::Type::NONE)
    {
        std::string fullPath = FileUtils::getInstance()->fullPathForFilename("rewards.plist");
        _rewardData = FileUtils::getInstance()->getValueMapFromFile(fullPath.c_str());
    }
}

// @00637c94
UserProgress::~UserProgress()
{
}

// @00637d58
int UserProgress::addCompletionTime(float time, int chapter, int level, bool flush)
{
    std::string key = getKey(chapter, level);
    std::vector<std::string> times = getCompletionTimes(key);
    float roundedTime = roundf(time * 100.0f) / 100.0f;
    std::string timeString = patch::to_string(roundedTime);

    if (times.empty())
    {
        times.push_back(timeString);
        setCompletionTimes(key, times, flush);
        return 0;
    }

    size_t count = times.size();
    size_t checked = count > 2 ? 3 : count;
    for (size_t i = 0; i < checked; i++)
    {
        float storedTime = roundf(std::stof(times[i]) * 100.0f) / 100.0f;
        if (roundedTime == storedTime)
        {
            return (int)i + 1;
        }
        // The stored (rounded) time is compared with the unrounded new time.
        if (storedTime <= time)
        {
            continue;
        }
        if (count < 4)
        {
            times.insert(times.begin() + i, timeString);
        }
        else
        {
            times.at(i) = timeString;
        }
        setCompletionTimes(key, times, flush);
        return (int)i + 1;
    }

    if (count < 4)
    {
        times.push_back(timeString);
        setCompletionTimes(key, times, flush);
        return (int)times.size();
    }

    if (times.size() != 4)
    {
        return 0;
    }
    float fourthTime = roundf(std::stof(times[3]) * 100.0f) / 100.0f;
    if (roundedTime == fourthTime)
    {
        return 4;
    }
    // Original behaviour (kept): the fourth time is replaced when it is BETTER than the new one, and a
    // new time between the third and the fourth is not recorded.
    if (fourthTime < time)
    {
        times.at(3) = timeString;
        setCompletionTimes(key, times, flush);
        return 4;
    }
    return 0;
}

// @00638478
std::string UserProgress::getKey(int chapter, int level)
{
    return "c" + patch::to_string(chapter) + "_l" + patch::to_string(level);
}

// @00638678
std::vector<std::string> UserProgress::getCompletionTimes(std::string key)
{
    std::string timesString = UserDefault::getInstance()->getStringForKey(key.c_str());
    std::vector<std::string> times;
    if (!timesString.empty())
    {
        split(timesString, ':', times);
    }
    return times;
}

// @006388ec
void UserProgress::setCompletionTimes(std::string key, std::vector<std::string> times, bool flush)
{
    UserDefault* userDefault = UserDefault::getInstance();
    std::string timesString;
    for (size_t i = 0; i < times.size(); i++)
    {
        std::string timeString = times[i];
        timesString.append(timeString);
        if (i < times.size() - 1)
        {
            timesString.append(":");
        }
    }
    userDefault->setStringForKey(key.c_str(), timesString.c_str());
    if (flush)
    {
        userDefault->flush();
    }
}

// @00638cd0
void UserProgress::printTimes(int chapter, int level)
{
}

// @00639044
void UserProgress::split(const std::string& s, char delim, std::vector<std::string>& elems)
{
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim))
    {
        elems.push_back(item);
    }
}

// @00639284
std::vector<float> UserProgress::getCompletionTimes(int chapter, int level)
{
    std::vector<std::string> timeStrings = getCompletionTimes(getKey(chapter, level));
    std::vector<float> times;
    for (size_t i = 0; i < timeStrings.size(); i++)
    {
        times.push_back(std::stof(timeStrings[i]));
    }
    return times;
}

// @006395b8
bool UserProgress::isLevelCompleted(int chapter, int level)
{
    return getCompletionTimes(getKey(chapter, level)).size() != 0;
}

// @006396b0
bool UserProgress::isLevelUnlocked(int chapter, int level)
{
    std::string unlockedKey = "c" + patch::to_string(chapter) + "_l" + patch::to_string(level) + "_unlocked";
    if (UserDefault::getInstance()->getBoolForKey(unlockedKey.c_str()))
    {
        checkIfUserHasSeenAlertForUnlockedLevel(chapter, level);
        return true;
    }

    std::string rewardKey = "chapter" + patch::to_string(chapter) + "_level" + patch::to_string(level);
    if (_rewardData[rewardKey].getType() == Value::Type::NONE)
    {
        return false;
    }
    ValueMap reward = _rewardData[rewardKey].asValueMap();
    if (reward["type"].asInt() != 0)
    {
        return false;
    }
    int chapterIndex = reward["chapterIndex"].asInt();
    int numberOfLevelsNeededToUnlock = reward["numberOfLevelsNeededToUnlock"].asInt();
    if (getNumberOfLevelsOfChapterCompleted(chapterIndex) < numberOfLevelsNeededToUnlock)
    {
        return false;
    }
    checkIfUserHasSeenAlertForUnlockedLevel(chapter, level);
    return true;
}

// @00639de0
void UserProgress::checkIfUserHasSeenAlertForUnlockedLevel(int chapter, int level)
{
    std::string alertSeenKey = "c" + patch::to_string(chapter) + "_l" + patch::to_string(level)
                               + "_unlockedLevelAlertSeen";
    UserDefault* userDefault = UserDefault::getInstance();
    if (userDefault->getBoolForKey(alertSeenKey.c_str()))
    {
        return;
    }

    ValueMap levelData = Settings::getInstance()->getLevelData(chapter, level);
    std::string levelName = "";
    if (levelData["name"].getType() != Value::Type::NONE)
    {
        levelName = levelData["name"].asString();
    }
    UnlockedLevelAnnouncement announcement;
    announcement.chapter = chapter;
    announcement.level = level;
    announcement.levelName = levelName;
    _unlockedLevelAnnouncements.push_back(announcement);
    // Not flushed.
    userDefault->setBoolForKey(alertSeenKey.c_str(), true);
}

// @0063a2dc
int UserProgress::getNumberOfLevelsOfChapterCompleted(int chapter)
{
    int completed = 0;
    for (int level = 0; level < _levelsPerChapter; level++)
    {
        if (getCompletionTimes(getKey(chapter, level)).size() != 0)
        {
            completed++;
        }
    }
    return completed;
}

// @0063a410
float UserProgress::getPercentageOfLevelsOfChapterCompleted(int chapter, bool useChapterLevelCount)
{
    int completed = getNumberOfLevelsOfChapterCompleted(chapter);
    float levelCount = (float)_levelsPerChapter;
    if (useChapterLevelCount)
    {
        ValueVector chapters = Settings::getInstance()->getAllChaptersData(false);
        // The chapter index is not range checked.
        if (chapters[chapter].getType() == Value::Type::NONE)
        {
            return 0.0f;
        }
        ValueMap chapterData = chapters[chapter].asValueMap();
        levelCount = (float)chapterData["levels"].asValueVector().size();
    }
    return (float)completed / levelCount;
}

// @0063a63c
void UserProgress::setIsLevelUnlocked(int chapter, int level, bool unlocked)
{
    std::string unlockedKey = "c" + patch::to_string(chapter) + "_l" + patch::to_string(level) + "_unlocked";
    UserDefault* userDefault = UserDefault::getInstance();
    userDefault->setBoolForKey(unlockedKey.c_str(), unlocked);
    userDefault->flush();
}

// @0063a908
bool UserProgress::getAllLevelsCompleted()
{
    ValueVector chapters = Settings::getInstance()->getAllChaptersData(false);
    bool allLevelsCompleted = true;
    for (size_t chapter = 0; chapter < chapters.size(); chapter++)
    {
        ValueMap chapterData = chapters[chapter].asValueMap();
        ValueVector levels = chapterData["levels"].asValueVector();
        for (size_t level = 0; level < levels.size(); level++)
        {
            bool completed = getCompletionTimes((int)chapter, (int)level).size() != 0;
            allLevelsCompleted = allLevelsCompleted && completed;
            if (!completed)
            {
                break;
            }
        }
    }
    return allLevelsCompleted;
}

// @0063abc0
UnlockLevelInstructions UserProgress::getUnlockLevelInstructions(int chapter, int level)
{
    UnlockLevelInstructions instructions = UnlockLevelInstructions();
    std::string rewardKey = "chapter" + patch::to_string(chapter) + "_level" + patch::to_string(level);
    if (_rewardData[rewardKey].getType() != Value::Type::NONE)
    {
        ValueMap reward = _rewardData[rewardKey].asValueMap();
        if (reward["type"].asInt() == 0)
        {
            int chapterIndex = reward["chapterIndex"].asInt();
            int numberOfLevelsNeededToUnlock = reward["numberOfLevelsNeededToUnlock"].asInt();
            std::string chapterName =
                Settings::getInstance()->getChapterData(chapterIndex, false)["name"].asString();
            std::string text = "To unlock this level, complete " + patch::to_string(numberOfLevelsNeededToUnlock)
                               + " or more " + chapterName + " levels.";
            instructions.chapterIndex = chapterIndex;
            instructions.text = text;
        }
    }
    return instructions;
}

// @0063b378
std::vector<UnlockedLevelAnnouncement> UserProgress::getUnlockedLevelAnnouncements()
{
    std::vector<UnlockedLevelAnnouncement> announcements = _unlockedLevelAnnouncements;
    _unlockedLevelAnnouncements.clear();
    return announcements;
}

// @0063b530
void UserProgress::addDebugCompletionTimes()
{
}

// @0063b534
void UserProgress::resetLevelProgress()
{
    std::vector<std::string> noTimes;
    for (int chapter = 0; chapter != 10; chapter++)
    {
        for (int level = 0; level < _levelsPerChapter; level++)
        {
            setCompletionTimes(getKey(chapter, level), noTimes, false);
        }
    }
    UserDefault::getInstance()->flush();
}
