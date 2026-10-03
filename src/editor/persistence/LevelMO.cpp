// LevelMO -- file-backed port of the iOS Core Data entity (see LevelMO.h).
#include "LevelMO.h"

#include "LevelStore.h"

#include <algorithm>

USING_NS_CC;

namespace {

// Index entry keys (the Core Data attribute names + port bookkeeping).
const char* const kKeyFile = "file";
const char* const kKeyCreatedOn = "createdOn";
const char* const kKeyModifiedOn = "modifiedOn";
const char* const kKeyLevelTimes = "levelTimes";

int intFor(const ValueMap& map, const char* key, int fallback)
{
    auto it = map.find(key);
    return it == map.end() || it->second.isNull() ? fallback : it->second.asInt();
}

bool boolFor(const ValueMap& map, const char* key, bool fallback)
{
    auto it = map.find(key);
    return it == map.end() || it->second.isNull() ? fallback : it->second.asBool();
}

double doubleFor(const ValueMap& map, const char* key, double fallback)
{
    auto it = map.find(key);
    return it == map.end() || it->second.isNull() ? fallback : it->second.asDouble();
}

std::string stringFor(const ValueMap& map, const char* key)
{
    auto it = map.find(key);
    return it == map.end() || it->second.isNull() ? std::string() : it->second.asString();
}

}  // namespace

LevelMO* LevelMO::create()
{
    LevelMO* level = new (std::nothrow) LevelMO();
    if (level)
    {
        level->autorelease();
    }
    return level;
}

// Core Data defaults from happy_wheels_2.mom (nil booleans read as NO).
LevelMO::LevelMO()
    : _id_x(0),
      _dataLoaded(false),
      _dataDirty(false),
      _force_character(false),
      _playable_character(0),
      _flags(0),
      _importable(false),
      _active_x(false),
      _rating(0.0),
      _rating_total(0),
      _votes(0),
      _plays(0),
      _chapter(nullptr),
      _store(nullptr),
      _createdOn(0),
      _modifiedOn(0),
      _dirty(false),
      _deleted(false),
      _inserted(false)
{
}

LevelMO::~LevelMO()
{
}

void LevelMO::markDirty()
{
    _dirty = true;
}

// ---- attributes -----------------------------------------------------------------------------

int LevelMO::id_x() const { return _id_x; }
void LevelMO::setId_x(int id_x) { _id_x = id_x; markDirty(); }
const std::string& LevelMO::name() const { return _name; }
void LevelMO::setName(const std::string& name) { _name = name; markDirty(); }
const std::string& LevelMO::comments() const { return _comments; }
void LevelMO::setComments(const std::string& comments) { _comments = comments; markDirty(); }

const std::string& LevelMO::data()
{
    // Core Data fault: the XML lives in its own file and is read on first access.
    if (!_dataLoaded)
    {
        _dataLoaded = true;
        if (_store && !_fileName.empty())
        {
            _data = FileUtils::getInstance()->getStringFromFile(_store->fullPathForLevel(this));
        }
    }
    return _data;
}

void LevelMO::setData(const std::string& data)
{
    _data = data;
    _dataLoaded = true;
    _dataDirty = true;
    markDirty();
}

bool LevelMO::force_character() const { return _force_character; }
void LevelMO::setForce_character(bool forceCharacter) { _force_character = forceCharacter; markDirty(); }
int LevelMO::playable_character() const { return _playable_character; }
void LevelMO::setPlayable_character(int playableCharacter) { _playable_character = playableCharacter; markDirty(); }
int LevelMO::flags() const { return _flags; }
void LevelMO::setFlags(int flags) { _flags = flags; markDirty(); }
bool LevelMO::importable() const { return _importable; }
void LevelMO::setImportable(bool importable) { _importable = importable; markDirty(); }
bool LevelMO::active_x() const { return _active_x; }
void LevelMO::setActive_x(bool active) { _active_x = active; markDirty(); }
double LevelMO::rating() const { return _rating; }
void LevelMO::setRating(double rating) { _rating = rating; markDirty(); }
int LevelMO::rating_total() const { return _rating_total; }
void LevelMO::setRating_total(int ratingTotal) { _rating_total = ratingTotal; markDirty(); }
int LevelMO::votes() const { return _votes; }
void LevelMO::setVotes(int votes) { _votes = votes; markDirty(); }
int LevelMO::plays() const { return _plays; }
void LevelMO::setPlays(int plays) { _plays = plays; markDirty(); }

// ---- relationships ----------------------------------------------------------------------------

ChapterMO* LevelMO::chapter() const { return _chapter; }
const std::vector<float>& LevelMO::levelTimes() const { return _levelTimes; }

void LevelMO::addLevelTimesObject(float time)
{
    _levelTimes.push_back(time);
    markDirty();
}

void LevelMO::removeLevelTimesObject(float time)
{
    auto it = std::find(_levelTimes.begin(), _levelTimes.end(), time);
    if (it != _levelTimes.end())
    {
        _levelTimes.erase(it);
        markDirty();
    }
}

void LevelMO::removeAllLevelTimes()
{
    if (!_levelTimes.empty())
    {
        _levelTimes.clear();
        markDirty();
    }
}

// ---- iOS methods ----------------------------------------------------------------------------

// @ios 100075564
std::vector<float> LevelMO::completionTimesAscending() const
{
    std::vector<float> times = _levelTimes;
    std::stable_sort(times.begin(), times.end());
    return times;
}

// @ios 1000755c0
float LevelMO::bestCompletionTime() const
{
    std::vector<float> times = completionTimesAscending();
    if (!times.empty())
    {
        return times[0];
    }
    return -1.0f;  // [NSNumber numberWithInteger:-1]
}

// @ios 100075620
int LevelMO::addCompletionTime(float time)
{
    // The new LevelTimeMO is told apart from equal existing times by identity on iOS; here the
    // new entry is tracked explicitly (placed after equal times, as a stable sort would).
    struct Entry
    {
        float time;
        bool isNew;
    };
    std::vector<Entry> entries;
    for (float t : _levelTimes)
    {
        entries.push_back({t, false});
    }
    entries.push_back({time, true});
    std::stable_sort(entries.begin(), entries.end(),
                     [](const Entry& a, const Entry& b) { return a.time < b.time; });
    if (4 < entries.size())
    {
        // iOS removes objectAtIndex:3 (the 4th fastest), not the slowest.
        entries.erase(entries.begin() + 3);
    }
    _levelTimes.clear();
    int rank = 0;  // indexOfObject: NSNotFound + 1 wraps to 0
    for (size_t i = 0; i < entries.size(); i++)
    {
        _levelTimes.push_back(entries[i].time);
        if (entries[i].isNew)
        {
            rank = static_cast<int>(i) + 1;
        }
    }
    markDirty();
    if (_store)
    {
        _store->save(nullptr);
    }
    return rank;
}

// @ios 100075700
ValueVector LevelMO::checkForUnlock(LevelMO* level)
{
    // Acts only when level.chapter.chapterIndex == 0 (the first campaign chapter at 100%).
    // File-backed LevelMOs live in chapters 5000/5001, so the result is always nil.
    return ValueVector();
}

// @ios 10007587c
ValueMap LevelMO::dictionaryForUnlockedChapter(int chapter, int level)
{
    ValueMap dict;
    dict["chapter"] = chapter;
    dict["level"] = level;
    return dict;
}

// @ios 1000758f0
void LevelMO::checkForUnlocks()
{
    // Same chapterIndex == 0 guard as checkForUnlock: never true for user levels.
}

// ---- port bookkeeping -------------------------------------------------------------------------

const std::string& LevelMO::fileName() const { return _fileName; }
std::time_t LevelMO::createdOn() const { return _createdOn; }
std::time_t LevelMO::modifiedOn() const { return _modifiedOn; }
bool LevelMO::isDirty() const { return _dirty; }
bool LevelMO::isDeleted() const { return _deleted; }
bool LevelMO::isInserted() const { return _inserted; }

ValueMap LevelMO::toIndexEntry() const
{
    ValueMap entry;
    entry["chapter"] = _chapter ? _chapter->chapterIndex() : 0;
    entry[kKeyFile] = _fileName;
    entry["id_x"] = _id_x;
    entry["name"] = _name;
    entry["comments"] = _comments;
    entry["force_character"] = _force_character;
    entry["playable_character"] = _playable_character;
    entry["flags"] = _flags;
    entry["importable"] = _importable;
    entry["active_x"] = _active_x;
    entry["rating"] = _rating;
    entry["rating_total"] = _rating_total;
    entry["votes"] = _votes;
    entry["plays"] = _plays;
    ValueVector times;
    for (float t : _levelTimes)
    {
        times.push_back(Value(t));
    }
    entry[kKeyLevelTimes] = times;
    entry[kKeyCreatedOn] = static_cast<double>(_createdOn);
    entry[kKeyModifiedOn] = static_cast<double>(_modifiedOn);
    return entry;
}

void LevelMO::loadIndexEntry(const ValueMap& entry)
{
    _fileName = stringFor(entry, kKeyFile);
    _id_x = intFor(entry, "id_x", 0);
    _name = stringFor(entry, "name");
    _comments = stringFor(entry, "comments");
    _force_character = boolFor(entry, "force_character", false);
    _playable_character = intFor(entry, "playable_character", 0);
    _flags = intFor(entry, "flags", 0);
    _importable = boolFor(entry, "importable", false);
    _active_x = boolFor(entry, "active_x", false);
    _rating = doubleFor(entry, "rating", 0.0);
    _rating_total = intFor(entry, "rating_total", 0);
    _votes = intFor(entry, "votes", 0);
    _plays = intFor(entry, "plays", 0);
    _levelTimes.clear();
    auto it = entry.find(kKeyLevelTimes);
    if (it != entry.end() && it->second.getType() == Value::Type::VECTOR)
    {
        for (const Value& t : it->second.asValueVector())
        {
            _levelTimes.push_back(t.asFloat());
        }
    }
    _createdOn = static_cast<std::time_t>(doubleFor(entry, kKeyCreatedOn, 0.0));
    _modifiedOn = static_cast<std::time_t>(doubleFor(entry, kKeyModifiedOn, 0.0));
    _data.clear();
    _dataLoaded = false;
    _dataDirty = false;
    _dirty = false;
    _deleted = false;
    _inserted = false;
}
