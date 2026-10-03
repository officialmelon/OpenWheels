#pragma once
// LevelMO: one saved level (iOS 1.2.7 Core Data entity "LevelMO", happy_wheels_2.mom), now a
// file-backed object owned by LevelStore.
//
// iOS model (all attributes optional; type codes from the compiled model):
//   rating            double   default 0.0
//   comments          string   the level description shown/edited by Load/SaveLevelViewController
//   flags             int16    default 0
//   name              string
//   votes             int32    default 0
//   plays             int32    default 0
//   importable        bool     default nil (NO)
//   id_x              int32    default 0   per-chapter sequence: new = (max id_x in chapter) + 1
//   data              string   the level XML (<levelXML>...</levelXML>, -[EditorLayer levelData])
//   playable_character int16   default 0   character index the level starts with
//   active_x          bool     default nil (NO)
//   rating_total      int32    default 0
//   force_character   bool     default NO
//   chapter    -> ChapterMO (to-one, inverse "levels"); user levels live in chapterIndex 5000,
//                 levels imported from a .happywheels file in 5001
//   levelTimes -> LevelTimeMO {time: float} (to-many, inverse "level")
//   replays    -> ReplayMO (to-many, cascade) -- never created for user levels; not ported
//
// Port: Core Data faulting/dirty tracking is reproduced by LevelStore. Every setter marks the
// object dirty; nothing reaches disk until LevelStore::save() (= -[NSManagedObjectContext save:]).
// `data` is faulted in lazily from the level's XML file the first time data() is called.
// createdOn/modifiedOn/fileName are port additions for the on-disk index (iOS kept no dates).
//
// The six real methods of the iOS class are ported below. The unlock helpers only act on the
// built-in chapter 0, which never contains a LevelMO in this port (built-in progress lives in the
// Android Settings/UserProgress), so they are faithful no-ops here.

#include "cocos2d.h"

#include <ctime>
#include <string>
#include <vector>

class ChapterMO;
class LevelStore;

class LevelMO : public cocos2d::Ref
{
public:
    // -[NSEntityDescription insertNewObjectForEntityForName:@"LevelMO" inManagedObjectContext:]
    // goes through LevelStore::insertNewLevel(); create() alone makes a detached object.
    static LevelMO* create();

    // ---- Core Data attributes (iOS property names) ----
    int id_x() const;
    void setId_x(int id_x);
    const std::string& name() const;
    void setName(const std::string& name);
    const std::string& comments() const;
    void setComments(const std::string& comments);
    // Faults the XML in from fileName() on first access ("" if the file is unreadable).
    const std::string& data();
    void setData(const std::string& data);
    bool force_character() const;
    void setForce_character(bool forceCharacter);
    int playable_character() const;
    void setPlayable_character(int playableCharacter);
    int flags() const;
    void setFlags(int flags);
    bool importable() const;
    void setImportable(bool importable);
    bool active_x() const;
    void setActive_x(bool active);
    double rating() const;
    void setRating(double rating);
    int rating_total() const;
    void setRating_total(int ratingTotal);
    int votes() const;
    void setVotes(int votes);
    int plays() const;
    void setPlays(int plays);

    // ---- relationships ----
    ChapterMO* chapter() const;                       // nullptr while detached
    // LevelTimeMO.time values in insertion order (an NSSet on iOS; order is irrelevant there).
    const std::vector<float>& levelTimes() const;
    void addLevelTimesObject(float time);
    void removeLevelTimesObject(float time);          // removes one matching entry
    void removeAllLevelTimes();                       // SaveLevelViewController::saveOverBtn deletes every LevelTimeMO

    // ---- iOS methods ----
    std::vector<float> completionTimesAscending() const;   // @ios 100075564  levelTimes sorted by time asc
    // -1 (the NSNumber numberWithInteger:-1) when there are no times.
    float bestCompletionTime() const;                      // @ios 1000755c0
    // Inserts the time; when more than 4 times exist afterwards removes the 4th fastest (index 3 --
    // not the slowest; iOS quirk kept), saves the store, returns the new time's 1-based rank in the
    // ascending list (0 if it was the one removed: indexOfObject: NSNotFound + 1 wraps to 0).
    int addCompletionTime(float time);                     // @ios 100075620
    // Built-in chapter 0 unlock helpers. Return empty / do nothing for chapters 5000/5001.
    cocos2d::ValueVector checkForUnlock(LevelMO* level);   // @ios 100075700
    static cocos2d::ValueMap dictionaryForUnlockedChapter(int chapter, int level);  // @ios 10007587c
    void checkForUnlocks();                                // @ios 1000758f0

    // ---- port: file-backed bookkeeping (not in the iOS model) ----
    const std::string& fileName() const;              // XML file, relative to LevelStore root
    std::time_t createdOn() const;
    std::time_t modifiedOn() const;
    bool isDirty() const;
    bool isDeleted() const;
    bool isInserted() const;                          // inserted and not yet saved
    // Index entry (ValueMap with the attribute names above + "file", "createdOn", "modifiedOn",
    // "levelTimes"); used by LevelStore to read/write index.plist.
    cocos2d::ValueMap toIndexEntry() const;
    void loadIndexEntry(const cocos2d::ValueMap& entry);

protected:
    LevelMO();
    ~LevelMO() override;
    void markDirty();

private:
    friend class LevelStore;
    friend class ChapterMO;

    int _id_x;
    std::string _name;
    std::string _comments;
    std::string _data;
    bool _dataLoaded;              // false until data() faulted it in or setData() set it
    bool _dataDirty;               // the XML file must be rewritten on save
    bool _force_character;
    int _playable_character;
    int _flags;
    bool _importable;
    bool _active_x;
    double _rating;
    int _rating_total;
    int _votes;
    int _plays;
    std::vector<float> _levelTimes;

    ChapterMO* _chapter;           // weak; ChapterMO owns its levels
    LevelStore* _store;            // weak; null for detached objects
    std::string _fileName;
    std::time_t _createdOn;
    std::time_t _modifiedOn;
    bool _dirty;
    bool _deleted;
    bool _inserted;
};
