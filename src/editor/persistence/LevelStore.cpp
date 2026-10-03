// LevelStore / ChapterMO -- file-backed replacement for the iOS Core Data user-level storage.
// See LevelStore.h for the on-disk layout and the semantics kept from Core Data.
#include "LevelStore.h"

#include "LevelMO.h"
#include "UIKitCompat.h"

#include "tinyxml2/tinyxml2.h"

#include <algorithm>
#include <ctime>
#include <set>

USING_NS_CC;

namespace {

const char* const kIndexFile = "index.plist";
const int kIndexVersion = 1;
// -[AppController application:openURL:...]: (fileSize >> 10) < 0x401
const long kMaxHappyWheelsFileKiB = 0x400;

std::string baseName(const std::string& path)
{
    std::string p = path;
    while (!p.empty() && (p.back() == '/' || p.back() == '\\'))
    {
        p.pop_back();
    }
    size_t slash = p.find_last_of("/\\");
    return slash == std::string::npos ? p : p.substr(slash + 1);
}

std::string stem(const std::string& fileName)
{
    size_t dot = fileName.find_last_of('.');
    return dot == std::string::npos ? fileName : fileName.substr(0, dot);
}

// [[name componentsSeparatedByString:@"."] lastObject]
std::string lastDotComponent(const std::string& fileName)
{
    size_t dot = fileName.find_last_of('.');
    return dot == std::string::npos ? fileName : fileName.substr(dot + 1);
}

bool writeFileAtomically(const std::string& contents, const std::string& path)
{
    FileUtils* fu = FileUtils::getInstance();
    std::string tmp = path + ".tmp";
    if (!fu->writeStringToFile(contents, tmp))
    {
        return false;
    }
    return fu->renameFile(tmp, path);
}

int lastIdIn(const Vector<LevelMO*>& levels)
{
    // [[levels sortedArrayUsingDescriptors:@[id_x asc]] lastObject].id_x.intValue (nil -> 0)
    if (levels.empty())
    {
        return 0;
    }
    int last = levels.at(0)->id_x();
    for (LevelMO* level : levels)
    {
        last = std::max(last, level->id_x());
    }
    return last;
}

}  // namespace

// ==== ChapterMO ================================================================================

ChapterMO::ChapterMO(int chapterIndex, LevelStore* store) : _chapterIndex(chapterIndex), _store(store)
{
}

int ChapterMO::chapterIndex() const
{
    return _chapterIndex;
}

const Vector<LevelMO*>& ChapterMO::levels() const
{
    return _levels;
}

void ChapterMO::addLevelsObject(LevelMO* level)
{
    if (!level || level->_chapter == this)
    {
        return;
    }
    if (level->_chapter)
    {
        level->_chapter->removeLevelsObject(level);
    }
    _levels.pushBack(level);
    level->_chapter = this;
    level->_store = _store;
    level->markDirty();
}

void ChapterMO::removeLevelsObject(LevelMO* level)
{
    if (!level || level->_chapter != this)
    {
        return;
    }
    level->_chapter = nullptr;
    _levels.eraseObject(level);
}

Vector<LevelMO*> ChapterMO::levelsSortedById() const
{
    std::vector<LevelMO*> sorted(_levels.begin(), _levels.end());
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](LevelMO* a, LevelMO* b) { return a->id_x() < b->id_x(); });
    Vector<LevelMO*> result;
    for (LevelMO* level : sorted)
    {
        result.pushBack(level);
    }
    return result;
}

// ==== LevelStore ===============================================================================

static LevelStore* s_sharedLevelStore = nullptr;

const char* const LevelStore::kLevelsChangedNotification = "user_levels_changed";

LevelStore* LevelStore::getInstance()
{
    if (!s_sharedLevelStore)
    {
        s_sharedLevelStore = new LevelStore();
        s_sharedLevelStore->loadIndex();
    }
    return s_sharedLevelStore;
}

void LevelStore::destroyInstance()
{
    delete s_sharedLevelStore;
    s_sharedLevelStore = nullptr;
}

LevelStore::LevelStore()
    : _userChapter(new ChapterMO(LevelStoreChapterUser, this)),
      _importedChapter(new ChapterMO(LevelStoreChapterImported, this)),
      _indexDirty(false)
{
    _rootPath = FileUtils::getInstance()->getWritablePath() + "levels/";
}

LevelStore::~LevelStore()
{
    for (ChapterMO* chapter : {_userChapter, _importedChapter})
    {
        for (LevelMO* level : chapter->_levels)
        {
            level->_chapter = nullptr;
            level->_store = nullptr;
        }
        chapter->release();
    }
    for (LevelMO* level : _inserted)
    {
        level->_store = nullptr;
    }
    for (LevelMO* level : _deleted)
    {
        level->_store = nullptr;
    }
}

// ---- paths ----------------------------------------------------------------------------------

const std::string& LevelStore::rootPath() const { return _rootPath; }
std::string LevelStore::importedPath() const { return _rootPath + "imported/"; }
std::string LevelStore::inboxPath() const { return _rootPath + "import/"; }
std::string LevelStore::sharedPath() const { return _rootPath + "shared/"; }

std::string LevelStore::fullPathForLevel(const LevelMO* level) const
{
    return level && !level->fileName().empty() ? _rootPath + level->fileName() : std::string();
}

// ---- fetches --------------------------------------------------------------------------------

ChapterMO* LevelStore::chapterWithIndex(int chapterIndex)
{
    if (chapterIndex == LevelStoreChapterUser)
    {
        return _userChapter;
    }
    if (chapterIndex == LevelStoreChapterImported)
    {
        return _importedChapter;
    }
    return nullptr;
}

Vector<LevelMO*> LevelStore::levelsSortedById(int chapterIndex)
{
    ChapterMO* chapter = chapterWithIndex(chapterIndex);
    return chapter ? chapter->levelsSortedById() : Vector<LevelMO*>();
}

int LevelStore::lastLevelId(int chapterIndex)
{
    ChapterMO* chapter = chapterWithIndex(chapterIndex);
    return chapter ? lastIdIn(chapter->levels()) : 0;
}

// ---- context --------------------------------------------------------------------------------

LevelMO* LevelStore::insertNewLevel()
{
    LevelMO* level = LevelMO::create();
    level->_store = this;
    level->_inserted = true;
    level->_dataLoaded = true;  // a new object has no file to fault from
    level->markDirty();
    _inserted.pushBack(level);
    return level;
}

void LevelStore::deleteObject(LevelMO* level)
{
    if (!level || level->_deleted)
    {
        return;
    }
    level->retain();
    if (level->_chapter)
    {
        level->_chapter->removeLevelsObject(level);
    }
    level->_deleted = true;
    if (_inserted.contains(level))
    {
        // Inserted and deleted before any save: nothing to remove from disk.
        _inserted.eraseObject(level);
    }
    else
    {
        _deleted.pushBack(level);
    }
    level->release();
}

bool LevelStore::hasChanges() const
{
    if (_indexDirty || !_deleted.empty())
    {
        return true;
    }
    for (LevelMO* level : _inserted)
    {
        if (level->chapter())
        {
            return true;
        }
    }
    for (ChapterMO* chapter : {_userChapter, _importedChapter})
    {
        for (LevelMO* level : chapter->levels())
        {
            if (level->isDirty())
            {
                return true;
            }
        }
    }
    return false;
}

std::string LevelStore::newFileNameForLevel(const LevelMO* level) const
{
    // levels/<id_x>.xml or levels/imported/<id_x>.xml; "_<n>" appended when that name is taken
    // (by an adopted custom file or another level of the same id).
    FileUtils* fu = FileUtils::getInstance();
    std::string dir = level->chapter() && level->chapter()->chapterIndex() == LevelStoreChapterImported
                          ? "imported/"
                          : "";
    std::set<std::string> used;
    for (ChapterMO* chapter : {_userChapter, _importedChapter})
    {
        for (LevelMO* other : chapter->levels())
        {
            if (other != level && !other->fileName().empty())
            {
                used.insert(other->fileName());
            }
        }
    }
    std::string base = dir + StringUtils::format("%d", level->id_x());
    std::string name = base + ".xml";
    for (int n = 2; used.count(name) || fu->isFileExist(_rootPath + name); n++)
    {
        name = base + StringUtils::format("_%d", n) + ".xml";
    }
    return name;
}

bool LevelStore::save(std::string* error)
{
    FileUtils* fu = FileUtils::getInstance();
    if (!fu->createDirectory(_rootPath) || !fu->createDirectory(importedPath()))
    {
        if (error)
        {
            *error = "Could not create " + _rootPath;
        }
        return false;
    }

    // Live file names (a deleted level's file is only removed if nothing else uses it).
    std::set<std::string> liveFiles;
    for (ChapterMO* chapter : {_userChapter, _importedChapter})
    {
        for (LevelMO* level : chapter->levels())
        {
            if (!level->fileName().empty())
            {
                liveFiles.insert(level->fileName());
            }
        }
    }

    // Write new / changed XML files.
    std::time_t now = std::time(nullptr);
    for (ChapterMO* chapter : {_userChapter, _importedChapter})
    {
        for (LevelMO* level : chapter->levels())
        {
            if (!level->isDirty() && !level->isInserted())
            {
                continue;
            }
            if (level->_fileName.empty())
            {
                level->_fileName = newFileNameForLevel(level);
                liveFiles.insert(level->_fileName);
                level->_dataDirty = true;
            }
            std::string path = fullPathForLevel(level);
            if (level->_dataDirty || !fu->isFileExist(path))
            {
                if (!writeFileAtomically(level->data(), path))
                {
                    if (error)
                    {
                        *error = "Could not write " + path;
                    }
                    return false;
                }
            }
        }
    }

    // Rewrite the index (every live level).
    _indexDirty = true;
    if (!writeIndex(error))
    {
        return false;
    }

    // Commit: remove deleted files, clear the dirty state.
    for (LevelMO* level : _deleted)
    {
        if (!level->_fileName.empty() && !liveFiles.count(level->_fileName))
        {
            fu->removeFile(_rootPath + level->_fileName);
        }
        level->_store = nullptr;
    }
    _deleted.clear();
    for (ChapterMO* chapter : {_userChapter, _importedChapter})
    {
        for (LevelMO* level : chapter->levels())
        {
            if (level->_dirty || level->_inserted)
            {
                if (level->_createdOn == 0)
                {
                    level->_createdOn = now;
                }
                level->_modifiedOn = now;
            }
            level->_dirty = false;
            level->_dataDirty = false;
            level->_inserted = false;
            _inserted.eraseObject(level);
        }
    }
    // The dates changed after the index was written: write it once more (cheap, keeps the dates).
    writeIndex(nullptr);
    postNotification(kLevelsChangedNotification);
    return true;
}

bool LevelStore::writeIndex(std::string* error)
{
    ValueVector entries;
    for (ChapterMO* chapter : {_userChapter, _importedChapter})
    {
        for (LevelMO* level : chapter->levelsSortedById())
        {
            if (!level->fileName().empty())
            {
                entries.push_back(Value(level->toIndexEntry()));
            }
        }
    }
    ValueMap index;
    index["version"] = kIndexVersion;
    index["levels"] = entries;
    FileUtils* fu = FileUtils::getInstance();
    fu->createDirectory(_rootPath);
    std::string path = _rootPath + kIndexFile;
    std::string tmp = path + ".tmp";
    if (!fu->writeValueMapToFile(index, tmp) || !fu->renameFile(tmp, path))
    {
        if (error)
        {
            *error = "Could not write " + path;
        }
        return false;
    }
    _indexDirty = false;
    return true;
}

void LevelStore::loadIndex()
{
    FileUtils* fu = FileUtils::getInstance();
    std::string path = _rootPath + kIndexFile;
    bool changed = false;
    if (fu->isFileExist(path))
    {
        ValueMap index = fu->getValueMapFromFile(path);
        auto it = index.find("levels");
        if (it != index.end() && it->second.getType() == Value::Type::VECTOR)
        {
            for (const Value& value : it->second.asValueVector())
            {
                if (value.getType() != Value::Type::MAP)
                {
                    continue;
                }
                const ValueMap& entry = value.asValueMap();
                auto chapterIt = entry.find("chapter");
                int chapterIndex = chapterIt == entry.end() ? 0 : chapterIt->second.asInt();
                ChapterMO* chapter = chapterWithIndex(chapterIndex);
                if (!chapter)
                {
                    changed = true;
                    continue;
                }
                LevelMO* level = LevelMO::create();
                level->loadIndexEntry(entry);
                level->_store = this;
                if (level->fileName().empty() || !fu->isFileExist(fullPathForLevel(level)))
                {
                    // The level's XML is gone: drop the entry.
                    changed = true;
                    continue;
                }
                chapter->_levels.pushBack(level);
                level->_chapter = chapter;
            }
        }
    }
    size_t before = _userChapter->_levels.size() + _importedChapter->_levels.size();
    adoptForeignFiles(LevelStoreChapterUser, _rootPath);
    adoptForeignFiles(LevelStoreChapterImported, importedPath());
    if (changed || before != _userChapter->_levels.size() + _importedChapter->_levels.size())
    {
        writeIndex(nullptr);
    }
}

void LevelStore::adoptForeignFiles(int chapterIndex, const std::string& dir)
{
    FileUtils* fu = FileUtils::getInstance();
    if (!fu->isDirectoryExist(dir))
    {
        return;
    }
    ChapterMO* chapter = chapterWithIndex(chapterIndex);
    std::string prefix = chapterIndex == LevelStoreChapterImported ? "imported/" : "";
    std::set<std::string> known;
    for (ChapterMO* c : {_userChapter, _importedChapter})
    {
        for (LevelMO* level : c->levels())
        {
            known.insert(level->fileName());
        }
    }
    std::vector<std::string> names;
    for (const std::string& path : fu->listFiles(dir))
    {
        if (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        {
            continue;  // directory
        }
        std::string name = baseName(path);
        if (lastDotComponent(name) == "xml" && !known.count(prefix + name))
        {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    for (const std::string& name : names)
    {
        std::string xml = fu->getStringFromFile(dir + name);
        std::string error;
        if (!isValidLevelXML(xml, &error))
        {
            CCLOG("LevelStore: skipping %s%s (%s)", dir.c_str(), name.c_str(), error.c_str());
            continue;
        }
        LevelMO* level = LevelMO::create();
        level->_store = this;
        level->_fileName = prefix + name;
        level->_id_x = lastIdIn(chapter->levels()) + 1;
        level->_name = stem(name);
        int playableCharacter = 1;
        bool forceCharacter = false;
        readCharacterInfo(xml, &playableCharacter, &forceCharacter);
        level->_playable_character = playableCharacter;
        level->_force_character = forceCharacter;
        level->_data = xml;
        level->_dataLoaded = true;
        std::time_t now = std::time(nullptr);
        level->_createdOn = now;
        level->_modifiedOn = now;
        chapter->_levels.pushBack(level);
        level->_chapter = chapter;
        CCLOG("LevelStore: adopted %s%s as level %d", dir.c_str(), name.c_str(), level->id_x());
    }
}

void LevelStore::reload()
{
    for (ChapterMO* chapter : {_userChapter, _importedChapter})
    {
        for (LevelMO* level : chapter->_levels)
        {
            level->_chapter = nullptr;
            level->_store = nullptr;
        }
        chapter->_levels.clear();
    }
    for (LevelMO* level : _inserted)
    {
        level->_store = nullptr;
    }
    for (LevelMO* level : _deleted)
    {
        level->_store = nullptr;
    }
    _inserted.clear();
    _deleted.clear();
    _indexDirty = false;
    loadIndex();
    postNotification(kLevelsChangedNotification);
}

void LevelStore::postNotification(const char* name)
{
    uikit::NotificationCenter::postNotification(name, this);
}

// ---- import -----------------------------------------------------------------------------------

// @ios 100101d70  -[LevelDataInitializer importLevelsFromFileSharingWithManagedObjectContext:]
std::string LevelStore::importLevelsFromFileSharing()
{
    FileUtils* fu = FileUtils::getInstance();
    std::string documents = inboxPath();
    fu->createDirectory(documents);
    std::vector<std::string> files;
    for (const std::string& path : fu->listFiles(documents))
    {
        if (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        {
            continue;
        }
        std::string name = baseName(path);
        std::string extension = lastDotComponent(name);
        if (extension == "txt" || extension == "xml")
        {
            files.push_back(name);
        }
    }
    if (files.empty())
    {
        return "No .xml or .txt files found";
    }
    std::sort(files.begin(), files.end());  // contentsOfDirectoryAtPath: order (port: by name)

    ChapterMO* chapter = chapterWithIndex(LevelStoreChapterUser);  // "chapterIndex == 5000"
    int lastId = lastIdIn(chapter->levels());
    int imported = 0;
    for (const std::string& name : files)
    {
        std::string path = documents + name;
        if (!fu->isFileExist(path))
        {
            return "The file " + name + " couldn't be opened.";
        }
        std::string contents = fu->getStringFromFile(path);
        // Port addition: iOS imported any text; skip what the game could not load.
        std::string error;
        if (!isValidLevelXML(contents, &error))
        {
            CCLOG("LevelStore: not importing %s (%s)", name.c_str(), error.c_str());
            continue;
        }
        imported++;
        int idx = imported + lastId;
        // createLevelWithManagedObjectContext:level:index: with the shared dictionary
        // {name: "%i %@", comments, force_character: 0, playable_character: 1, data}.
        LevelMO* level = insertNewLevel();
        level->setId_x(idx);
        level->setName(StringUtils::format("%d %s", idx, name.c_str()));
        level->setComments("Level imported from the documents folder.");
        level->setData(contents);
        level->setForce_character(false);
        level->setPlayable_character(1);
        chapter->addLevelsObject(level);
    }
    std::string error;
    if (!save(&error))
    {
        return error;
    }
    return "";
}

// @ios 1001021b4  -[LevelDataInitializer deleteImportedLevelFiles]
void LevelStore::deleteImportedLevelFiles()
{
    FileUtils* fu = FileUtils::getInstance();
    for (const std::string& path : fu->listFiles(inboxPath()))
    {
        std::string name = baseName(path);
        std::string extension = lastDotComponent(name);
        if ((extension == "txt" || extension == "xml") && fu->isFileExist(inboxPath() + name))
        {
            if (!fu->removeFile(inboxPath() + name))
            {
                CCLOG("remove text file success? NO");
            }
        }
    }
}

// @ios 10001d1ac  file checks of -[AppController application:openURL:sourceApplication:annotation:]
bool LevelStore::readHappyWheelsFile(const std::string& path, ValueMap& levelDict, std::string* errorKey)
{
    FileUtils* fu = FileUtils::getInstance();
    // urlIsHappyWheelsFile: lastPathComponent containsString ".happywheels"
    if (baseName(path).find(".happywheels") == std::string::npos || !fu->isFileExist(path))
    {
        if (errorKey)
        {
            *errorKey = "";
        }
        return false;
    }
    long size = fu->getFileSize(path);
    if (!((size >> 10) < kMaxHappyWheelsFileKiB + 1))
    {
        if (errorKey)
        {
            *errorKey = "LEVEL TOO LARGE";
        }
        return false;
    }
    levelDict = fu->getValueMapFromFile(path);
    if (levelDict.empty())
    {
        // Port: iOS would go on with a nil dictionary and create an empty level.
        if (errorKey)
        {
            *errorKey = "ERROR";
        }
        return false;
    }
    return true;
}

// @ios 10001d578  -[AppController saveImportedLevel]
LevelMO* LevelStore::saveImportedLevel(const ValueMap& levelDict)
{
    auto get = [&levelDict](const char* key) -> Value {
        auto it = levelDict.find(key);
        return it == levelDict.end() ? Value() : it->second;
    };
    ChapterMO* chapter = chapterWithIndex(LevelStoreChapterImported);  // "chapterIndex == 5001"
    int last = lastIdIn(chapter->levels());
    LevelMO* level = insertNewLevel();
    level->setId_x(last + 1);
    level->setName(get("name").isNull() ? std::string() : get("name").asString());
    level->setData(get("data").isNull() ? std::string() : get("data").asString());
    level->setComments(get("comments").isNull() ? std::string() : get("comments").asString());
    level->setForce_character(get("force_character").asBool());
    level->setPlayable_character(get("playable_character").asInt());
    chapter->addLevelsObject(level);
    save(nullptr);
    return level;
}

LevelMO* LevelStore::importLevelXMLFile(const std::string& path, int chapterIndex, std::string* error)
{
    FileUtils* fu = FileUtils::getInstance();
    ChapterMO* chapter = chapterWithIndex(chapterIndex);
    if (!chapter)
    {
        if (error)
        {
            *error = "Unknown chapter";
        }
        return nullptr;
    }
    if (!fu->isFileExist(path))
    {
        if (error)
        {
            *error = "Cannot read " + path;
        }
        return nullptr;
    }
    std::string xml = fu->getStringFromFile(path);
    if (!isValidLevelXML(xml, error))
    {
        return nullptr;
    }
    int playableCharacter = 1;
    bool forceCharacter = false;
    readCharacterInfo(xml, &playableCharacter, &forceCharacter);
    LevelMO* level = insertNewLevel();
    level->setId_x(lastIdIn(chapter->levels()) + 1);
    level->setName(stem(baseName(path)));
    level->setComments("");
    level->setData(xml);
    level->setForce_character(forceCharacter);
    level->setPlayable_character(playableCharacter);
    chapter->addLevelsObject(level);
    if (!save(error))
    {
        return nullptr;
    }
    return level;
}

bool LevelStore::isValidLevelXML(const std::string& xml, std::string* error)
{
    tinyxml2::XMLDocument doc;
    if (xml.empty() || doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS)
    {
        if (error)
        {
            *error = xml.empty() ? "empty file" : "not well-formed XML";
        }
        return false;
    }
    tinyxml2::XMLElement* root = doc.RootElement();
    if (!root || std::string(root->Name()) != "levelXML")
    {
        if (error)
        {
            *error = "root element is not <levelXML>";
        }
        return false;
    }
    if (!root->FirstChildElement("info"))
    {
        if (error)
        {
            *error = "no <info> element";
        }
        return false;
    }
    return true;
}

void LevelStore::readCharacterInfo(const std::string& xml, int* playableCharacter, bool* forceCharacter)
{
    int c = 1;
    bool f = false;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) == tinyxml2::XML_SUCCESS && doc.RootElement())
    {
        if (tinyxml2::XMLElement* info = doc.RootElement()->FirstChildElement("info"))
        {
            // Same attribute reads as LevelB2D (c = character id, f = forced).
            info->QueryIntAttribute("c", &c);
            info->QueryBoolAttribute("f", &f);
        }
    }
    if (playableCharacter)
    {
        *playableCharacter = c;
    }
    if (forceCharacter)
    {
        *forceCharacter = f;
    }
}
