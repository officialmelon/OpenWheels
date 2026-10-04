#pragma once
// EDITOR (browser features, PC addition): the editor's level file format is the browser game's
// level XML (Flash editor 1.87 SaverLoader.createXML / buildLevel semantics), the superset of
// what both games can express. This file writes the editor stage as browser XML and reads browser
// XML back into refs. Play-testing and the user-level library run it through
// online::FlashLevelConverter (<info src="flash">), like downloaded browser levels.
//
// Old levels in the iOS editor's format (y-up metres, <info ... e="1" fm="m">) still load through
// EditorLevelXMLParser; saving them writes browser XML (an "upgrade": same items, now browser
// semantics).
//
// Marker: the editor writes <info ... ow="1">. The browser game ignores unknown attributes, so
// the XML stays importable there; the converter uses it to keep a few mobile-only things that a
// browser level cannot contain (backgrounds 3 / 4 / 4001, the slow-motion panel 5001, mobile-only
// parameters of the iOS items).

#include <string>

class EditorSpriteBatchNode;
class CharacterRef;

namespace flashed {

// True for browser-format level XML (written by this editor, the browser editor or downloaded):
// not converted (no src), not the iOS editor's metres format (no fm / ptm), and either marked
// ow="1" or with a start position outside the 320 x 160 metre range a metres level would use.
bool isBrowserLevelXml(const std::string& xml);
// The XML LevelB2D can play: browser XML converted by FlashLevelConverter, anything else as is.
// `forcedCharacter` / `character` receive the converter's character choice when given.
std::string playableLevelXml(const std::string& xml, bool* converted = nullptr, int* character = nullptr,
                             bool* forceCharacter = nullptr);

struct LevelInfo
{
    int background = 0;
    unsigned int backgroundColor = 0xffffff;
};

// Writes the stage (refs[0] = the character) as browser level XML.
std::string writeBrowserLevel(EditorSpriteBatchNode* sbn, const LevelInfo& info);

// Reads browser XML onto an empty stage. `addCharacter(xMetres, yMetres, c, f, h)` creates the
// start ref (EditorLayer::addCharacter). Returns false when the XML doesn't parse.
class LevelReaderDelegate
{
public:
    virtual ~LevelReaderDelegate() = default;
    virtual void readerAddCharacter(float xMetres, float yMetres, int character, bool force, bool hideVehicle) = 0;
};
bool readBrowserLevel(const std::string& xml, EditorSpriteBatchNode* sbn, LevelReaderDelegate* delegate,
                      LevelInfo* info);

}  // namespace flashed
