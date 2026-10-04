// ONLINE (PC addition): see PublishPanel.h.
#include "online/account/PublishPanel.h"

#include <algorithm>
#include <cstdlib>

#include "LevelMO.h"
#include "LevelStore.h"
#include "online/OnlineUi.h"
#include "online/account/AccountPanels.h"
#include "online/account/TjfAccount.h"
#include "online/account/TjfServices.h"
#include "tinyxml2/tinyxml2.h"

USING_NS_CC;

namespace online {
namespace account {

namespace {

const Color3B kError(214, 72, 72);
// SaveMenu.commentsText.restrict / nameText.restrict
const char* const kAllowed = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !@#$%^&*()_+-=;'|?/,.<>\"";
const int kMaxShapes = 900;    // Canvas.maxShapes
const int kMaxArt = 10000;     // Canvas.maxArt

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool number(const tinyxml2::XMLElement* e, const char* name, double* out) {
    const char* v = e->Attribute(name);
    if (!v || !*v) return false;
    char* end = nullptr;
    const double d = std::strtod(v, &end);
    if (end == v) return false;
    if (out) *out = d;
    return true;
}

int intAttr(const tinyxml2::XMLElement* e, const char* name, int fallback) {
    int v = fallback;
    e->QueryIntAttribute(name, &v);
    return v;
}

std::string publishDir() { return FileUtils::getInstance()->getWritablePath() + "online/publish/"; }

}  // namespace

PublishCheck checkBrowserLevel(const std::string& xml) {
    PublishCheck c;
    tinyxml2::XMLDocument doc;
    if (xml.empty() || doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) {
        c.error = "The level XML doesn't parse.";
        return c;
    }
    tinyxml2::XMLElement* root = doc.RootElement();
    if (!root || std::string(root->Name()) != "levelXML") {
        c.error = "Not a level (the root element must be <levelXML>).";
        return c;
    }
    tinyxml2::XMLElement* info = root->FirstChildElement("info");
    if (!info) {
        c.error = "The level has no <info> element.";
        return c;
    }
    // Converted / mobile-format levels: different units, the browser game would misplace everything.
    for (const char* a : {"ptm", "sw", "sh", "r", "cw", "src", "fv", "fm"}) {
        if (info->Attribute(a)) {
            c.error = std::string("This is a mobile-format level (<info ") + a +
                      "=...>). Only levels in the browser format (saved by the OpenWheels level editor) can be "
                      "published.";
            return c;
        }
    }
    if (!number(info, "v", &c.version) || c.version < 1.0 || c.version > 2.5) {
        c.error = "The level's version (<info v>) is missing or unknown to the browser game.";
        return c;
    }
    if (c.version < 1.87) c.warnings.push_back("Saved by an older browser editor (v" + std::string(info->Attribute("v")) + ").");
    double x = 0, y = 0, ch = 1, bg = 0;
    if (!number(info, "x", &x) || !number(info, "y", &y)) {
        c.error = "The level has no start position.";
        return c;
    }
    if (x < 0 || x > 20000 || y < 0 || y > 10000) c.warnings.push_back("The start position is outside the editor canvas.");
    number(info, "c", &ch);
    if (ch < 1 || ch > 11) {
        c.error = "Unknown character (<info c>).";
        return c;
    }
    number(info, "bg", &bg);
    if (bg < 0 || bg > 2) {
        c.error = "Background " + std::to_string((int)bg) + " exists only in the mobile game; pick blank, green hills or city.";
        return c;
    }
    const char* forced = info->Attribute("f");
    c.playableCharacter = (forced && std::string(forced) == "t") ? (int)ch : 0;

    auto checkShape = [&c](tinyxml2::XMLElement* sh) -> bool {
        const int t = intAttr(sh, "t", -1);
        if (t < 0 || t > 4) {
            c.error = "Shape type " + std::to_string(t) + " doesn't exist in the browser game (terrain is mobile-only).";
            return false;
        }
        if (t == 4) ++c.artShapes;
        else ++c.shapes;
        return true;
    };
    auto checkSpecial = [&c](tinyxml2::XMLElement* sp) -> bool {
        const int t = intAttr(sp, "t", -1);
        if (t < 0 || t > 35) {
            c.error = t == 5001 ? "The slow-motion panel exists only in the mobile game."
                                : "Item type " + std::to_string(t) + " doesn't exist in the browser game.";
            return false;
        }
        ++c.specials;
        return true;
    };
    for (tinyxml2::XMLElement* e = root->FirstChildElement(); e; e = e->NextSiblingElement()) {
        const std::string tag = e->Name();
        if (tag == "info") continue;
        if (tag == "shapes") {
            for (auto* sh = e->FirstChildElement(); sh; sh = sh->NextSiblingElement())
                if (!checkShape(sh)) return c;
        } else if (tag == "specials") {
            for (auto* sp = e->FirstChildElement(); sp; sp = sp->NextSiblingElement())
                if (!checkSpecial(sp)) return c;
        } else if (tag == "groups") {
            for (auto* g = e->FirstChildElement(); g; g = g->NextSiblingElement()) {
                ++c.groups;
                for (auto* m = g->FirstChildElement(); m; m = m->NextSiblingElement()) {
                    const std::string mt = m->Name();
                    if (mt == "sh" && !checkShape(m)) return c;
                    if (mt == "sp" && !checkSpecial(m)) return c;
                }
            }
        } else if (tag == "joints") {
            for (auto* j = e->FirstChildElement(); j; j = j->NextSiblingElement()) {
                const int t = intAttr(j, "t", -1);
                if (t != 0 && t != 1) {
                    c.error = "Joint type " + std::to_string(t) + " doesn't exist in the browser game.";
                    return c;
                }
                ++c.joints;
            }
        } else if (tag == "triggers") {
            for (auto* t = e->FirstChildElement(); t; t = t->NextSiblingElement()) {
                const int type = intAttr(t, "t", 1);
                if (type < 1 || type > 3) {
                    c.error = "Trigger type " + std::to_string(type) + " exists only in the mobile game.";
                    return c;
                }
                ++c.triggers;
            }
        } else {
            c.error = "The level contains <" + tag + ">, which the browser game doesn't know.";
            return c;
        }
    }
    if (c.shapes > kMaxShapes) {
        c.error = "Too many shapes (" + std::to_string(c.shapes) + "): the browser editor allows " +
                  std::to_string(kMaxShapes) + ".";
        return c;
    }
    if (c.artShapes > kMaxArt) {
        c.error = "Too many art shapes for the browser game.";
        return c;
    }
    // OpenWheels' editor marker (ow="1") - the browser ignores it, but don't upload it.
    if (info->Attribute("ow")) {
        info->DeleteAttribute("ow");
        tinyxml2::XMLPrinter printer;
        doc.Print(&printer);
        c.cleanXml = printer.CStr();
    } else {
        c.cleanXml = xml;
    }
    c.ok = true;
    return c;
}

void publishLevel(const PublishRequest& request) {
    ensureLoggedIn("Log in to publish your level on totaljerkface.com.", [request]() { PublishPanel::show(request); });
}

// ---- PublishPanel -------------------------------------------------------------------------------

PublishPanel* PublishPanel::show(const PublishRequest& request) {
    auto* p = new (std::nothrow) PublishPanel();
    if (p && p->init(request)) {
        p->autorelease();
        p->present();
        return p;
    }
    delete p;
    return nullptr;
}

bool PublishPanel::init(const PublishRequest& request) {
    if (!initPanel(Size(2300.0f, 1760.0f), request.existingLevelId ? "Update Level" : "Publish Level")) return false;
    _request = request;
    _check = checkBrowserLevel(request.xml);
    TjfAccount* account = TjfAccount::get();
    const float L = 130.0f, W = _size.width - 260.0f;
    float y = _top;

    Label* who = tjfui::label("To totaljerkface.com as " + account->displayName() +
                                  (account->userId() > 0 ? " (user #" + std::to_string(account->userId()) + ")" : ""),
                              ui::kFontBody, 46.0f, ui::kInk);
    who->setPosition(L, y);
    _content->addChild(who);
    y -= 110.0f;

    Label* c1 = tjfui::label("LEVEL NAME (4 TO 20 CHARACTERS)", ui::kFontBodyBold, 36.0f, ui::kInkDim);
    c1->setPosition(L, y);
    _content->addChild(c1);
    y -= 90.0f;
    _name = tjfui::TextInput::create(Size(W, 130.0f), "Level name", false, 20);
    _name->setAllowed(kAllowed);
    _name->setText(trim(request.name).substr(0, 20));
    _name->setPosition(L + W * 0.5f, y);
    _content->addChild(_name);
    y -= 130.0f;

    Label* c2 = tjfui::label("AUTHOR'S COMMENT", ui::kFontBodyBold, 36.0f, ui::kInkDim);
    c2->setPosition(L, y);
    _content->addChild(c2);
    y -= 90.0f;
    _comment = tjfui::TextInput::create(Size(W, 130.0f), "Tell players about your level", false, 255);
    _comment->setAllowed(kAllowed);
    _comment->setText(request.comment.substr(0, 255));
    _comment->setPosition(L + W * 0.5f, y);
    _content->addChild(_comment);
    y -= 120.0f;

    // The browser-format check.
    std::string report;
    if (_check.ok) {
        char v[16];
        std::snprintf(v, sizeof v, "%.2f", _check.version);
        report = "\xE2\x9C\x93 Browser level format v" + std::string(v) + "  \xC2\xB7  " + std::to_string(_check.shapes) +
                 " shapes, " + std::to_string(_check.artShapes) + " art, " + std::to_string(_check.specials) +
                 " items, " + std::to_string(_check.groups) + " groups, " + std::to_string(_check.joints) +
                 " joints, " + std::to_string(_check.triggers) + " triggers";
        if (_check.playableCharacter) report += "\nCharacter: " + ui::characterName(_check.playableCharacter) + " (forced)";
        for (const std::string& w : _check.warnings) report += "\n" + w;
    } else {
        report = "Can't publish: " + _check.error;
    }
    Label* rep = tjfui::textBlock(report, ui::kFontBody, 40.0f, _check.ok ? ui::kInk : kError, W);
    rep->setPosition(L, y);
    _content->addChild(rep);

    _message = tjfui::textBlock("", ui::kFontBodyBold, 42.0f, kError, W);
    _message->setPosition(L, 420.0f);
    _content->addChild(_message);
    _spinner = ui::createSpinner(64.0f, ui::kInkDim);
    _spinner->setPosition(L + 32.0f, 330.0f);
    _spinner->setVisible(false);
    _content->addChild(_spinner);

    const float bw = (W - 60.0f) * 0.5f;
    _saveBtn = ui::Button::create(request.existingLevelId ? "SAVE CHANGES" : "SAVE PRIVATELY", Size(bw, 150.0f),
                                  ui::Button::window("yellow"), 52.0f);
    _saveBtn->setPosition(L + bw * 0.5f, 170.0f);
    _saveBtn->setCallback([this]() { submit(false); });
    _content->addChild(_saveBtn);
    _publishBtn = ui::Button::create("PUBLISH", Size(bw, 150.0f), ui::Button::window("blue"), 56.0f);
    _publishBtn->setIcon(tjfui::icon("upload", 64.0f));
    _publishBtn->setPosition(L + bw * 1.5f + 60.0f, 170.0f);
    _publishBtn->setCallback([this]() { submit(true); });
    _content->addChild(_publishBtn);
    _saveBtn->setEnabled(_check.ok);
    _publishBtn->setEnabled(_check.ok);

    Label* foot = tjfui::label("Saved levels stay private (My Levels). The site allows one published level per day.",
                               ui::kFontBody, 34.0f, ui::kInkDim);
    foot->setPosition(L + 100.0f, 330.0f);
    _content->addChild(foot);
    _firstField = _name;
    return true;
}

bool PublishPanel::onKey(EventKeyboard::KeyCode key) {
    using K = EventKeyboard::KeyCode;
    if (key == K::KEY_TAB) {
        if (_name->focused()) _comment->focus();
        else _name->focus();
        return true;
    }
    if (key == K::KEY_V && _ctrlDown) {
        if (_name->focused()) _name->paste();
        else if (_comment->focused()) _comment->paste();
        return true;
    }
    return false;
}

bool PublishPanel::validateFields(std::string* name, std::string* comment) {
    *name = trim(_name->text());
    *comment = trim(_comment->text());
    if (name->size() < 4) {
        _message->setString("Level name must be at least 4 characters.");
        _name->focus();
        return false;
    }
    return true;
}

void PublishPanel::submit(bool publish) {
    if (_busy || !_check.ok) return;
    std::string name, comment;
    if (!validateFields(&name, &comment)) return;
    TjfAccount* account = TjfAccount::get();
    if (!account->loggedIn() || account->userId() <= 0) {
        _message->setString("You are not logged in.");
        return;
    }
    const std::string user = account->displayName();
    const std::string question =
        publish ? "Publish \"" + name + "\" to totaljerkface.com as " + user + "? Everyone will be able to play it."
                : "Save \"" + name + "\" to your totaljerkface.com account as " + user +
                      "? It stays private until you publish it.";
    RefPtr<PublishPanel> self(this);
    tjfui::confirm(publish ? "Publish level?" : "Save level?", question, publish ? "PUBLISH" : "SAVE", "CANCEL",
                   [self, publish, name, comment](bool yes) {
                       PublishPanel* p = self.get();
                       if (!yes || p->_closing) return;
                       p->_busy = true;
                       p->_saveBtn->setEnabled(false);
                       p->_publishBtn->setEnabled(false);
                       p->_spinner->setVisible(true);
                       p->_message->setColor(ui::kInkDim);
                       p->_message->setString(p->_request.existingLevelId ? "Saving over your level..." : "Saving level...");
                       auto saved = [self, publish](const Reply& reply) {
                           PublishPanel* q = self.get();
                           if (!reply.ok) {
                               q->_busy = false;
                               if (q->_closing) return;
                               q->_spinner->setVisible(false);
                               q->_message->setColor(kError);
                               q->_message->setString(reply.message);
                               q->_saveBtn->setEnabled(true);
                               q->_publishBtn->setEnabled(true);
                               return;
                           }
                           const int id = q->_request.existingLevelId ? q->_request.existingLevelId
                                                                      : std::atoi(reply.value.c_str());
                           if (!publish) {
                               q->finish(false, id);
                               return;
                           }
                           q->_message->setString("Publishing...");
                           TjfServices::get()->publishLevel(id, [self, id](const Reply& pub) {
                               PublishPanel* r = self.get();
                               r->_busy = false;
                               if (!pub.ok) {
                                   if (r->_closing) return;
                                   r->_spinner->setVisible(false);
                                   r->_message->setColor(kError);
                                   r->_message->setString("Saved privately as level " + std::to_string(id) +
                                                          ", but not published: " + pub.message);
                                   r->_request.existingLevelId = id;  // a retry updates that level
                                   r->_saveBtn->setEnabled(true);
                                   r->_publishBtn->setEnabled(true);
                                   return;
                               }
                               r->finish(true, id);
                           });
                       };
                       const std::string& xml = p->_check.cleanXml;
                       if (p->_request.existingLevelId) {
                           TjfServices::get()->updateLevel(p->_request.existingLevelId, name, comment,
                                                           p->_check.playableCharacter, xml, saved);
                       } else {
                           TjfServices::get()->createLevel(name, comment, p->_check.playableCharacter, xml, saved);
                       }
                   });
}

void PublishPanel::finish(bool publish, int levelId) {
    _busy = false;
    if (!_closing) dismiss();
    ui::showToast(publish ? "Level published" : "Level saved",
                  {publish ? "Level " + std::to_string(levelId) + " is public on totaljerkface.com."
                           : "Level " + std::to_string(levelId) + " is saved privately in your account (My Levels)."},
                  0.0f);
}

// ---- picker -------------------------------------------------------------------------------------

namespace {

class PublishPicker : public tjfui::Panel {
public:
    static PublishPicker* create() {
        auto* p = new (std::nothrow) PublishPicker();
        if (p && p->init()) {
            p->autorelease();
            return p;
        }
        delete p;
        return nullptr;
    }

private:
    struct Item {
        std::string name;
        std::string xml;
        std::string comment;
        std::string where;
    };

    bool init() {
        if (!initPanel(Size(2000.0f, 1700.0f), "Publish a Level")) return false;
        const float L = 130.0f, W = _size.width - 260.0f;
        std::vector<Item> items;
        // The editor's levels (chapter 5000) that are browser-format.
        for (LevelMO* level : LevelStore::getInstance()->levelsSortedById(LevelStoreChapterUser)) {
            const std::string& xml = level->data();
            PublishCheck c = checkBrowserLevel(xml);
            if (!c.ok) continue;
            items.push_back({level->name(), xml, level->comments(), "your levels"});
        }
        // Drop folder.
        FileUtils* fu = FileUtils::getInstance();
        if (fu->isDirectoryExist(publishDir())) {
            std::vector<std::string> files;
            fu->listFilesRecursively(publishDir(), &files);
            for (const std::string& f : files) {
                if (f.size() < 4 || f.compare(f.size() - 4, 4, ".xml") != 0) continue;
                const size_t slash = f.find_last_of("/\\");
                items.push_back({f.substr(slash + 1, f.size() - slash - 5), fu->getStringFromFile(f), "",
                                 "online/publish folder"});
            }
        }
        float y = _top;
        if (items.empty()) {
            Label* none = tjfui::textBlock(
                "No browser-format levels found.\n\nSave a level with the OpenWheels level editor, or put the level XML "
                "(as exported by the browser editor) into:\n" + publishDir(),
                ui::kFontBody, 44.0f, ui::kInk, W);
            none->setPosition(L, y);
            _content->addChild(none);
            return true;
        }
        Label* hint = tjfui::label("Pick a level. You'll name it and confirm before anything is uploaded.",
                                   ui::kFontBody, 42.0f, ui::kInkDim);
        hint->setPosition(L, y);
        _content->addChild(hint);
        y -= 90.0f;
        const float bh = 120.0f;
        const int maxItems = std::max(1, (int)((y - 80.0f) / (bh + 20.0f)));
        for (int i = 0; i < (int)items.size() && i < maxItems; ++i) {
            ui::Button::Style row;
            row.color = Color3B::WHITE;
            row.opacity = 220;
            row.radius = 24.0f;
            row.textColor = ui::kInk;
            auto* b = ui::Button::create(items[i].name + "   (" + items[i].where + ")", Size(W, bh), row, 46.0f,
                                         ui::kFontBodyBold);
            b->setPosition(L + W * 0.5f, y - bh * 0.5f - i * (bh + 20.0f));
            const Item item = items[i];
            RefPtr<PublishPicker> self(this);
            b->setCallback([self, item]() {
                self->dismiss();
                PublishRequest r;
                r.xml = item.xml;
                r.name = item.name;
                r.comment = item.comment;
                publishLevel(r);
            });
            _content->addChild(b);
        }
        return true;
    }
};

}  // namespace

void PublishPanel::showPicker() {
    if (PublishPicker* p = PublishPicker::create()) p->present();
}

}  // namespace account
}  // namespace online
