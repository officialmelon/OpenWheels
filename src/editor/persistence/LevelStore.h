#pragma once
// LevelStore: file-backed replacement for the iOS Core Data stack as far as user levels are
// concerned (NSManagedObjectContext + the ChapterMO fetches "chapterIndex == %i" + the import code
// in LevelDataInitializer / AppController). Port class; it has no iOS counterpart of its own.
//
// On-disk layout (under FileUtils::getWritablePath()):
//   levels/index.plist          the index (see below); written atomically (tmp + rename)
//   levels/<id_x>.xml           level XML of chapter 5000 ("your levels"), verbatim LevelMO.data
//   levels/imported/<id_x>.xml  level XML of chapter 5001 (levels imported from .happywheels files)
//   levels/import/              drop folder for *.xml / *.txt level files: the iOS "Documents"
//                               (iTunes file sharing) folder read by importLevelsFromFileSharing()
//   levels/shared/              .happywheels files written by ShareAction (the iOS share sheet)
//
// index.plist (cocos2d ValueMap plist):
//   "version"  int 1
//   "levels"   array of entries, one per LevelMO:
//      "chapter" int (5000|5001), "file" string (relative to levels/), "id_x", "name", "comments",
//      "force_character", "playable_character", "flags", "importable", "active_x", "rating",
//      "rating_total", "votes", "plays", "levelTimes" (array of float),
//      "createdOn", "modifiedOn" (double, seconds since the epoch)
//   The XML is never stored in the index; LevelMO::data() faults it in from "file".
//
// Custom levels: reload() adopts every *.xml in levels/ (chapter 5000) and levels/imported/
// (chapter 5001) that the index does not know yet, provided isValidLevelXML() accepts it: the
// file keeps its name, id_x = max+1, name = file name without extension, playable_character and
// force_character are read from <info c=".." f=".."/>. Index entries whose file has vanished are
// dropped. So any valid level XML copied into levels/ shows up in LoadLevelViewController and can
// be played (LevelSession::playLevel) or opened in the editor
// (EditorLayer::createSceneWithLevelMO).
//
// Semantics kept from Core Data: inserts, edits and deletes are pending until save(); a failed
// save leaves them pending and returns the error text (shown by SaveLevelViewController as
// "Error" / "%@" like the NSError userInfo).

#include "cocos2d.h"

#include <string>

class LevelMO;
class LevelStore;

// User-level chapter indices (iOS ChapterMO.chapterIndex of the two non-campaign chapters).
enum LevelStoreChapter
{
    LevelStoreChapterUser = 5000,      // "YOUR LEVELS": editor saves + LevelDataInitializer imports
    LevelStoreChapterImported = 5001,  // "IMPORTED LEVELS": AppController saveImportedLevel
};

// iOS ChapterMO, reduced to what the user-level code touches (chapterIndex, levels,
// addLevelsObject:). The campaign selectors (percentComplete, levelsAscending, ...) belong to the
// built-in chapters, which the Android Settings/UserProgress already model; they are not ported.
class ChapterMO : public cocos2d::Ref
{
public:
    int chapterIndex() const;
    // The to-many "levels" relationship (unordered on iOS; kept in insertion order here).
    const cocos2d::Vector<LevelMO*>& levels() const;
    // Inserts `level` into this chapter (sets level->chapter()); marks the store dirty.
    void addLevelsObject(LevelMO* level);
    void removeLevelsObject(LevelMO* level);
    // [levels sortedArrayUsingDescriptors:@[id_x ascending]]
    cocos2d::Vector<LevelMO*> levelsSortedById() const;

private:
    friend class LevelStore;
    ChapterMO(int chapterIndex, LevelStore* store);
    int _chapterIndex;
    LevelStore* _store;                 // weak; the store owns its chapters
    cocos2d::Vector<LevelMO*> _levels;
};

class LevelStore
{
public:
    static LevelStore* getInstance();   // loads the index on first use
    static void destroyInstance();

    // Port notification (uikit::NotificationCenter, object = this store) after every successful
    // save() / reload(), so open level lists can refresh. The iOS notifications of the save flow
    // ("save_level_done", "save_level_cancel") are uikit::notification::kSaveLevelDone/-Cancel.
    static const char* const kLevelsChangedNotification;    // "user_levels_changed"

    // ---- paths ----
    const std::string& rootPath() const;      // "<writable>/levels/"
    std::string importedPath() const;         // rootPath() + "imported/"
    std::string inboxPath() const;            // rootPath() + "import/"
    std::string sharedPath() const;           // rootPath() + "shared/"
    std::string fullPathForLevel(const LevelMO* level) const;

    // ---- fetches ----
    // Fetch request "ChapterMO where chapterIndex == %i" -> objectAtIndex:0. Only 5000 and 5001
    // exist; any other index returns nullptr.
    ChapterMO* chapterWithIndex(int chapterIndex);
    // chapterWithIndex(chapterIndex)->levelsSortedById() (empty for unknown chapters).
    cocos2d::Vector<LevelMO*> levelsSortedById(int chapterIndex);
    // Highest id_x in the chapter ([sorted lastObject].id_x; 0 for an empty chapter, as messaging
    // nil yields 0 on iOS).
    int lastLevelId(int chapterIndex);

    // ---- NSManagedObjectContext ----
    // insertNewObjectForEntityForName:@"LevelMO": a new, chapter-less, pending-insert object.
    // The caller adds it to a chapter with ChapterMO::addLevelsObject before save().
    LevelMO* insertNewLevel();
    // deleteObject: removes it from its chapter immediately; the file goes away on save().
    void deleteObject(LevelMO* level);
    // save: writes changed XML files, deletes removed ones, rewrites index.plist. On failure
    // returns false and sets *error (if given) to a description; pending changes are kept.
    bool save(std::string* error = nullptr);
    bool hasChanges() const;
    // Drops pending changes and re-reads index.plist, adopting/dropping files as described above.
    void reload();

    // ---- level import (custom levels) ----
    // -[LevelDataInitializer importLevelsFromFileSharingWithManagedObjectContext:] @ios 100101d70
    // Every *.txt / *.xml in inboxPath() becomes a chapter-5000 level: id_x = last + n,
    // name "%i %@" (id_x, file name), comments "Level imported from the documents folder.",
    // force_character 0, playable_character 1, then save(). Returns "" on success, otherwise
    // "No .xml or .txt files found" or the error text (iOS returns nil / the message).
    // Port addition: files that fail isValidLevelXML() are skipped (iOS imported anything).
    std::string importLevelsFromFileSharing();
    // -[LevelDataInitializer deleteImportedLevelFiles] @ios 1001021b4: removes the *.txt/*.xml in
    // inboxPath().
    void deleteImportedLevelFiles();
    // -[AppController application:openURL:...] @ios 10001d1ac file checks: the path must contain
    // ".happywheels" and be at most 1 MiB ((size >> 10) <= 0x400); the file is an XML plist with
    // buildVersion, name, comments, data, force_character, playable_character. Returns false and
    // the localization key of the error ("LEVEL TOO LARGE") when it cannot be read.
    static bool readHappyWheelsFile(const std::string& path, cocos2d::ValueMap& levelDict,
                                    std::string* errorKey = nullptr);
    // -[AppController saveImportedLevel] @ios 10001d578: inserts a chapter-5001 level (id_x =
    // last + 1, name/data/comments/force_character/playable_character from the dictionary), saves.
    LevelMO* saveImportedLevel(const cocos2d::ValueMap& levelDict);
    // Port addition for custom levels: imports one level XML file (any name) into `chapterIndex`
    // with name = file name without extension and character settings from <info c f>. Saves.
    // Returns nullptr (and *error) if the file is unreadable or not valid level XML.
    LevelMO* importLevelXMLFile(const std::string& path, int chapterIndex = LevelStoreChapterUser,
                                std::string* error = nullptr);
    // Parses with tinyxml2: root element <levelXML> with an <info> child.
    static bool isValidLevelXML(const std::string& xml, std::string* error = nullptr);
    // <info c=".."> and f=".." of a level XML (defaults 1 / false when missing, like the importer).
    static void readCharacterInfo(const std::string& xml, int* playableCharacter, bool* forceCharacter);

private:
    LevelStore();
    ~LevelStore();
    void loadIndex();
    bool writeIndex(std::string* error);
    void adoptForeignFiles(int chapterIndex, const std::string& dir);
    std::string newFileNameForLevel(const LevelMO* level) const;
    void postNotification(const char* name);

    std::string _rootPath;
    ChapterMO* _userChapter;            // 5000
    ChapterMO* _importedChapter;        // 5001
    cocos2d::Vector<LevelMO*> _inserted;
    cocos2d::Vector<LevelMO*> _deleted;
    bool _indexDirty;
};
