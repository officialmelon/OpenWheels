#pragma once
// ONLINE (PC addition): publishing a level to totaljerkface.com (set_level.hw, see TjfServices.h).
// Publishing is outward-facing: it only ever happens after the player confirmed
//   Publish "<name>" to totaljerkface.com as <user>? Everyone will be able to play it.
// and the level passed checkBrowserLevel() (the browser game must be able to load it).
//
// Hook for the level editor (src/editor): call publishLevel() with the browser-format XML the
// editor saves (flashed::writeBrowserLevel); it asks for a login when needed and opens the
// publish panel pre-filled. The account panel's "PUBLISH A LEVEL" lists the editor's levels
// (LevelStore chapter 5000) that are browser-format, plus *.xml dropped into
// <writable>/online/publish/.

#include <functional>
#include <string>
#include <vector>

#include "online/account/TjfUi.h"

namespace online {
namespace ui {
class Button;
}
namespace account {

struct PublishRequest {
    std::string xml;          // browser-format level XML (<levelXML><info v=... .../>...)
    std::string name;         // level name (4..20 characters, the Flash SaveMenu rules)
    std::string comment;      // author comment (up to 255)
    int existingLevelId = 0;  // > 0: update that level of the player instead of creating one
};

// What the browser editor (Flash v1.87 SaverLoader.createXML) writes, checked before upload.
struct PublishCheck {
    bool ok = false;
    std::string error;                  // why it can't be published
    std::vector<std::string> warnings;  // publishable, but worth knowing
    std::string cleanXml;               // the XML to upload (OpenWheels-only marker attributes removed)
    int playableCharacter = 0;          // set_level.hw playable_character: c when forced, else 0
    double version = 0.0;
    int shapes = 0, artShapes = 0, specials = 0, groups = 0, joints = 0, triggers = 0;
};
PublishCheck checkBrowserLevel(const std::string& xml);

// Editor hook (see above).
void publishLevel(const PublishRequest& request);

class PublishPanel : public tjfui::Panel {
public:
    static PublishPanel* show(const PublishRequest& request);
    // The list of publishable levels (account panel).
    static void showPicker();

protected:
    bool init(const PublishRequest& request);
    bool onKey(cocos2d::EventKeyboard::KeyCode key) override;
    void submit(bool publish);
    void finish(bool publish, int levelId);
    bool validateFields(std::string* name, std::string* comment);

    PublishRequest _request;
    PublishCheck _check;
    tjfui::TextInput* _name = nullptr;
    tjfui::TextInput* _comment = nullptr;
    ui::Button* _publishBtn = nullptr;
    ui::Button* _saveBtn = nullptr;
    cocos2d::Label* _message = nullptr;
    cocos2d::Sprite* _spinner = nullptr;
    bool _busy = false;
};

}  // namespace account
}  // namespace online
