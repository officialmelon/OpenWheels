// ShareAction -- level export (see ShareAction.h). The desktop has no share sheet: the file is
// written to LevelStore::sharedPath() and an alert shows where.
#include "ShareAction.h"

#include "LevelSession.h"
#include "LevelStore.h"
#include "LevelUIHelpers.h"
#include "platform/common/Localization.h"

#include <cstring>

USING_NS_CC;

namespace {

// -[NSString stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]: escapes the bytes
// that are not legal in a URL (controls, space, " # % < > [ \ ] ^ ` { | } and non-ASCII).
std::string percentEscaped(const std::string& text)
{
    static const char* const kEscaped = " \"#%<>[\\]^`{|}";
    std::string result;
    for (unsigned char c : text)
    {
        if (c < 0x20 || c >= 0x7f || std::strchr(kEscaped, c))
        {
            result += StringUtils::format("%%%02X", c);
        }
        else
        {
            result += static_cast<char>(c);
        }
    }
    return result;
}

// Port: a level name may contain characters Windows file names cannot.
std::string fileSafe(const std::string& name)
{
    std::string result = name;
    for (char& c : result)
    {
        if (std::strchr("<>:\"/\\|?*", c) || static_cast<unsigned char>(c) < 0x20)
        {
            c = '_';
        }
    }
    return result;
}

}  // namespace

// @ios 10006533c
void ShareAction::shareLevelWithURL(const std::string& url, Node* viewController)
{
    // UIActivityViewController with the text -> an alert showing it.
    levelui::showAlert(0, Localization::get("SHARE LEVEL"), url, Localization::get("OK"), "", nullptr);
}

// @ios 1000653b8
void ShareAction::shareLevelData(const std::string& levelData, unsigned int characterIndex, bool forceCharacter,
                                 Node* viewController)
{
    std::string text = StringUtils::format("%s\n\nhappywheels://playlevel/?fc=%i&ci=%i&ld=%s",
                                           Localization::get("SHARE LEVEL MESSAGE").c_str(),
                                           forceCharacter ? 1 : 0, static_cast<int>(characterIndex),
                                           percentEscaped(levelData).c_str());
    shareLevelWithURL(text, viewController);
}

// @ios 100065460
std::string ShareAction::shareLevelDataFile(const std::string& levelData, unsigned int characterIndex,
                                            bool forceCharacter, const std::string& levelName,
                                            const std::string& creatorName, const std::string& levelComments,
                                            Node* viewController)
{
    std::string name = levelName.empty() ? std::string("Untitled") : levelName;   // nil -> @"Untitled"
    ValueMap dict;
    dict["buildVersion"] = LevelSession::kBuildVersion;   // CFBundleVersion of iOS 1.2.7
    dict["name"] = name;
    dict["comments"] = levelComments;                     // nil -> @""
    dict["data"] = levelData;
    dict["force_character"] = forceCharacter;
    dict["playable_character"] = static_cast<int>(characterIndex);
    // creatorName is accepted but not written (iOS builds the dictionary without it).

    FileUtils* fu = FileUtils::getInstance();
    std::string folder = LevelStore::getInstance()->sharedPath();
    fu->createDirectory(folder);
    std::string base = folder + fileSafe(name);
    std::string path = base + ".happywheels";              // "%@.happywheels"
    if (!fu->writeValueMapToFile(dict, path))
    {
        return "";
    }
    // Port: the plain level XML next to it, for custom-level folders / other tools.
    fu->writeStringToFile(levelData, base + ".xml");

    // UIActivityViewController(@[SHARE LEVEL MESSAGE, fileURL]) -> tell the player where it is.
    levelui::showAlert(0, Localization::get("SHARE LEVEL"),
                       Localization::get("SHARE LEVEL MESSAGE") + "\n\n" + path, Localization::get("OK"), "",
                       nullptr);
    return path;
}
