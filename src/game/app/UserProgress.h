#pragma once

#include "cocos2d.h"

#include <string>
#include <vector>

// Element of UserProgress::_unlockedLevelAnnouncements (type name from the binary:
// std::vector<UnlockedLevelAnnouncement>; arm64 sizeof 0x20). Queued by
// checkIfUserHasSeenAlertForUnlockedLevel, drained by LevelSelectMenu through
// getUnlockedLevelAnnouncements().
struct UnlockedLevelAnnouncement
{
    std::string levelName;                      // +0x00 level dictionary "name" ("" if missing)
    int chapter;                                // +0x18
    int level;                                  // +0x1c
};

// Return value of UserProgress::getUnlockLevelInstructions (arm64 sizeof 0x20, value-initialised).
// RE-TODO(@0063abc0): the type name is not in the binary (returned by value; only the layout
// {std::string at +0x00, int at +0x18} is known). LevelSelectMenu shows text and keeps chapterIndex
// (+0x18) for its "go to chapter" button.
struct UnlockLevelInstructions
{
    std::string text;                           // +0x00 "To unlock this level, complete <n> or more <chapter name> levels."
    int chapterIndex;                           // +0x18 chapter whose levels have to be completed
};

// Save data (no base class, no RTTI, arm64 sizeof 0x48; owned by Settings). Everything lives in
// cocos2d::UserDefault (Android SharedPreferences, UserDefault.xml on Windows):
//   "c<chapter>_l<level>"                         string  best times, ':'-separated, sorted ascending,
//                                                         at most 4, each patch::to_string(float) of
//                                                         (int)(t * 100) / 100.0f
//   "c<chapter>_l<level>_unlocked"                bool    explicitly unlocked level
//   "c<chapter>_l<level>_unlockedLevelAlertSeen"  bool    the "level unlocked" announcement was queued
// A level is completed when it has at least one time.
// Unlock rules come from rewards.plist (loaded once into _rewardData): key "chapter<c>_level<l>" ->
// {"type": int (0 = complete N levels of a chapter), "chapterIndex": int,
//  "numberOfLevelsNeededToUnlock": int}.
class UserProgress
{
public:
    // _levelsPerChapter = 15, then loadRewardData().
    UserProgress();                                                            // @0063799c
    // rewards.plist -> _rewardData unless already loaded (key "loaded" present).
    void loadRewardData();                                                     // @00637a08
    ~UserProgress();                                                           // @00637c94

    // Inserts the time into the level's best-time list (ascending, max 4) and saves it (flush ->
    // UserDefault::flush()). Returns the 1-based rank, or 0 when it is not a best time.
    int addCompletionTime(float time, int chapter, int level, bool flush);    // @00637d58
    // "c" + chapter + "_l" + level
    std::string getKey(int chapter, int level);                                // @00638478
    // UserDefault string for key split at ':' (empty when the string is empty).
    std::vector<std::string> getCompletionTimes(std::string key);              // @00638678
    // Joins times with ':' into UserDefault key; flush -> UserDefault::flush().
    void setCompletionTimes(std::string key, std::vector<std::string> times, bool flush); // @006388ec
    void printTimes(int chapter, int level);                                   // @00638cd0 (empty)
    // std::getline over a std::stringstream, pushing every item (including empty ones).
    void split(const std::string& s, char delim, std::vector<std::string>& elems); // @00639044
    // getCompletionTimes(getKey(chapter, level)) converted with std::stof.
    std::vector<float> getCompletionTimes(int chapter, int level);             // @00639284
    bool isLevelCompleted(int chapter, int level);                             // @006395b8
    // "_unlocked" flag, else the rewards.plist rule (type 0: completed levels of chapterIndex >= N);
    // queues the unlock announcement when unlocked. RESTORED (PC addition): a level marked
    // "unlock_after_previous" (OpenWheels' campaign chapters, src/restored) without a rewards rule
    // is unlocked once the level before it is completed (no announcement).
    bool isLevelUnlocked(int chapter, int level);                              // @006396b0
    void checkIfUserHasSeenAlertForUnlockedLevel(int chapter, int level);      // @00639de0
    // Levels 0.._levelsPerChapter-1 of the chapter that have a time.
    int getNumberOfLevelsOfChapterCompleted(int chapter);                      // @0063a2dc
    // completed / _levelsPerChapter, or / the chapter's real level count (levelData.plist) when
    // useChapterLevelCount (0 when the chapter does not exist).
    float getPercentageOfLevelsOfChapterCompleted(int chapter, bool useChapterLevelCount); // @0063a410
    // UserDefault "c<c>_l<l>_unlocked" = unlocked, flush.
    void setIsLevelUnlocked(int chapter, int level, bool unlocked);            // @0063a63c
    // Every level of every chapter in levelData.plist has a time (RESTORED: the campaign
    // chapters, index 100+, are not counted).
    bool getAllLevelsCompleted();                                              // @0063a908
    UnlockLevelInstructions getUnlockLevelInstructions(int chapter, int level); // @0063abc0
    // Returns the queued announcements and clears the queue.
    std::vector<UnlockedLevelAnnouncement> getUnlockedLevelAnnouncements();    // @0063b378
    void addDebugCompletionTimes();                                            // @0063b530 (empty)
    // Clears the times of chapters 0..9 x levels 0.._levelsPerChapter-1 (not the "_unlocked" flags),
    // then UserDefault::flush(). RESTORED (PC addition): the campaign chapters' times as well.
    void resetLevelProgress();                                                 // @0063b534

private:
    int _levelsPerChapter;                                          // +0x00 15
    std::vector<UnlockedLevelAnnouncement> _unlockedLevelAnnouncements; // +0x08
    cocos2d::ValueMap _rewardData;                                  // +0x20 rewards.plist
};
