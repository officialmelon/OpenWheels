// NET (PC addition): ghost race screens, see RaceUi.h.
#include "net/race/RaceUi.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "PauseLayer.h"
#include "Settings.h"
#include "net/LevelTransfer.h"
#include "net/race/RaceSession.h"
#include "online/OnlineUi.h"

USING_NS_CC;

namespace race {

namespace {

namespace oui = online::ui;

const Color3B kOk(46, 140, 70);
const Color3B kError(196, 56, 56);

Label* text(const std::string& s, const std::string& font, float size, const Color3B& color,
            const Vec2& anchor = Vec2(0.0f, 0.5f)) {
    return net::ui::label(s, font, size, color, anchor);
}

Node* dot(const Color3B& color, float size) {
    auto* d = oui::roundedRect(Size(size, size), size * 0.5f, color, 255);
    d->setAnchorPoint(Vec2(0.5f, 0.5f));
    return d;
}

Node* box(Node* parent, const Rect& r, GLubyte opacity = 150) {
    auto* b = oui::roundedRect(r.size, 34.0f, Color3B::WHITE, opacity);
    b->setAnchorPoint(Vec2::ZERO);
    b->setPosition(r.origin);
    parent->addChild(b);
    return b;
}

Label* caption(Node* parent, const std::string& s, float x, float y) {
    Label* l = text(s, oui::kFontBodyBold, 34.0f, oui::kInkDim);
    l->setPosition(x, y);
    parent->addChild(l);
    return l;
}

bool hits(Node* node, const Vec2& world) {
    if (!node || !oui::isShown(node)) return false;
    const Vec2 p = node->convertToNodeSpace(world);
    return Rect(Vec2::ZERO, node->getContentSize()).containsPoint(p);
}

std::string statusText(const RacePlayer& p, bool lobby) {
    if (lobby) return p.ready ? "Ready" : "Not ready";
    switch (p.status) {
    case Status::Lobby: return "In lobby";
    case Status::Loading: return "Loading\xE2\x80\xA6";
    case Status::Loaded: return "Ready";
    case Status::Racing: return "Racing";
    case Status::Ejected: return "Ejected!";
    case Status::Dead: return "Dead";
    case Status::Finished: return formatTime(p.timeMs);
    case Status::GaveUp: return "Gave up";
    case Status::Left: return "Left";
    }
    return "";
}

// A small checkered flag (race icon).
Node* flagIcon(float size, const Color3B& color) {
    DrawNode* d = DrawNode::create();
    const Color4F c(color);
    const float w = size * 0.62f, h = size * 0.42f, x0 = -size * 0.28f, y0 = size * 0.02f;
    d->drawSegment(Vec2(x0, -size * 0.46f), Vec2(x0, y0 + h), size * 0.035f, c);
    const int nx = 4, ny = 3;
    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
            const Vec2 a(x0 + w * i / nx, y0 + h * j / ny);
            const Vec2 b(x0 + w * (i + 1) / nx, y0 + h * (j + 1) / ny);
            if ((i + j) % 2 == 0) d->drawSolidRect(a, b, c);
            else d->drawSolidRect(a, b, Color4F(c.r, c.g, c.b, 0.25f));
        }
    }
    return d;
}

}  // namespace

std::string formatTime(int ms) {
    if (ms < 0) return "--:--";
    const int cs = ms / 10;
    return StringUtils::format("%d:%02d.%02d", cs / 6000, (cs / 100) % 60, cs % 100);
}

std::string placeText(int place) {
    if (place <= 0) return "\xE2\x80\x94";
    if (place == 1) return "1st";
    if (place == 2) return "2nd";
    if (place == 3) return "3rd";
    return std::to_string(place) + "th";
}

// ---- LobbyPanel ------------------------------------------------------------------------------------

LobbyPanel* LobbyPanel::create() {
    LobbyPanel* p = new (std::nothrow) LobbyPanel();
    if (p && p->init()) {
        p->autorelease();
        return p;
    }
    delete p;
    return nullptr;
}

LobbyPanel::~LobbyPanel() {
    if (_observer) net::LanDiscovery::getInstance()->removeObserver(_observer);
}

bool LobbyPanel::init() {
    const Size vs = Director::getInstance()->getVisibleSize();
    _w = std::min(2700.0f, vs.width - 160.0f);
    const float strip = oui::windowPanelStrip();
    const float h = std::min(1880.0f, vs.height - 60.0f);
    if (!initModal(Size(_w, h), "Race")) return false;
    (void)strip;
    RaceSession* s = RaceSession::get();

    _levelRow = Node::create();
    _panel->addChild(_levelRow, 2);

    const float top = _contentTop - 300.0f;   // boxes start below the level row
    const float bottom = 420.0f;
    const float split = _w * 0.47f;
    _playersBox = Rect(90.0f, bottom, split - 120.0f, top - bottom);
    _riderBox = Rect(split, top - 400.0f, _w - split - 90.0f, 400.0f);
    _nearbyBox = Rect(split, bottom + 120.0f, _w - split - 90.0f, top - 400.0f - 70.0f - bottom - 120.0f);
    box(_panel, _playersBox);
    box(_panel, _riderBox);
    caption(_panel, "RIDERS", _playersBox.origin.x + 10.0f, top + 34.0f);
    caption(_panel, "YOUR RIDER", _riderBox.origin.x + 10.0f, top + 34.0f);
    _playersNode = Node::create();
    _panel->addChild(_playersNode, 1);
    _riderNode = Node::create();
    _panel->addChild(_riderNode, 1);
    _nearbyNode = Node::create();
    _panel->addChild(_nearbyNode, 1);

    if (s->isHost()) {
        box(_panel, _nearbyBox);
        caption(_panel, "PLAYERS NEARBY", _nearbyBox.origin.x + 10.0f, _nearbyBox.getMaxY() + 34.0f);
        // invite by code (when the other device does not show up)
        Label* c = text("Not listed? Their code:", oui::kFontBody, 40.0f, oui::kInkDim);
        c->setPosition(_nearbyBox.origin.x + 10.0f, bottom + 50.0f);
        _panel->addChild(c);
        const float fieldW = 430.0f;
        _codeField = oui::SearchField::create(Size(fieldW, 92.0f), "K7QM-2D9A");
        _codeField->setIconVisible(false);
        _codeField->setPosition(_nearbyBox.origin.x + c->getContentSize().width + 40.0f + fieldW * 0.5f, bottom + 50.0f);
        _codeField->onChange = [this]() { if (_codeBtn) _codeBtn->setEnabled(!_codeField->text().empty()); };
        _panel->addChild(_codeField);
        _codeBtn = oui::Button::create("Invite", Size(260.0f, 96.0f), oui::Button::window("blue"), 48.0f);
        _codeBtn->setPosition(_nearbyBox.getMaxX() - 130.0f, bottom + 50.0f);
        _codeBtn->setCallback([this]() { inviteCode(); });
        _codeBtn->setEnabled(false);
        _panel->addChild(_codeBtn);

    } else {
        Label* help = Label::createWithTTF(
            "Everyone rides the same level in their own world. The other riders show up as see-through "
            "ghosts in their colour, behind you \xE2\x80\x94 crashes, lost limbs and all. First to the finish wins.",
            oui::kFontBody, 44.0f, Size(_nearbyBox.size.width - 60.0f, 0.0f), TextHAlignment::LEFT);
        help->setColor(oui::kInk);
        help->setLineSpacing(6.0f);
        help->setAnchorPoint(Vec2(0.0f, 1.0f));
        help->setPosition(_nearbyBox.origin.x + 20.0f, _nearbyBox.getMaxY() - 10.0f);
        _panel->addChild(help);
    }

    _status = text("", oui::kFontBodyBold, 46.0f, oui::kInk, Vec2(0.5f, 0.5f));
    _status->setPosition(_w * 0.5f, 330.0f);
    _panel->addChild(_status);
    _mainBtn = oui::Button::create("Start Race", Size(1000.0f, 190.0f), oui::Button::chunky("blue"), 84.0f);
    _mainBtn->setPosition(_w * 0.5f, 150.0f);
    _mainBtn->setCallback([this]() { mainPressed(); });
    _panel->addChild(_mainBtn);
    if (s->isHost()) {
        // last: the observer reports the current peers at once
        _observer = net::LanDiscovery::getInstance()->addObserver([this](const std::vector<net::Peer>& peers) {
            _peers.clear();
            for (const net::Peer& p : peers) {
                if (p.offers(kRaceService)) _peers.push_back(p);
            }
            _nearbyKey.clear();
            refresh();
        });
    }
    refresh();
    return true;
}

void LobbyPanel::onClosed() {
    if (_codeField) _codeField->detachWithIME();
    if (_observer) net::LanDiscovery::getInstance()->removeObserver(_observer);
    _observer = 0;
    if (oui::Dropdown::anyOpen()) oui::Dropdown::closeAll();
    RaceSession::get()->panelClosed(this);
}

bool LobbyPanel::onKey(EventKeyboard::KeyCode key) {
    using K = EventKeyboard::KeyCode;
    if (key == K::KEY_ENTER || key == K::KEY_KP_ENTER) {
        if (_codeField && !_codeField->text().empty()) inviteCode();
        else mainPressed();
        return true;
    }
    if (key == K::KEY_LEFT_ARROW) {
        cycleCharacter(-1);
        return true;
    }
    if (key == K::KEY_RIGHT_ARROW) {
        cycleCharacter(1);
        return true;
    }
    return false;
}

void LobbyPanel::onTouch(const Vec2& world) {
    if (_codeField && !hits(_codeField, world)) _codeField->detachWithIME();
}

void LobbyPanel::inviteCode() {
    if (!_codeField) return;
    uint32_t address = 0;
    uint16_t port = 0;
    if (!net::LevelTransfer::parseTarget(_codeField->text(), &address, &port)) {
        _status->setString("That code doesn't look right. It looks like K7QM-2D9A.");
        _status->setColor(kError);
        return;
    }
    _codeField->detachWithIME();
    _codeField->setText("");
    RaceSession::get()->inviteTarget(address, port, "");
}

void LobbyPanel::mainPressed() {
    RaceSession* s = RaceSession::get();
    if (s->phase() != Phase::Lobby) return;
    if (s->isHost()) {
        s->start();
    } else if (const RacePlayer* me = s->localPlayer()) {
        s->setReady(!me->ready);
    }
}

void LobbyPanel::cycleCharacter(int step) {
    RaceSession* s = RaceSession::get();
    const RacePlayer* me = s->localPlayer();
    if (!me || s->level().forced || s->phase() != Phase::Lobby) return;
    const std::vector<int> ids = RaceSession::characterIds();
    if (ids.empty()) return;
    int index = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] == me->character) index = static_cast<int>(i);
    }
    index = (index + step + static_cast<int>(ids.size())) % static_cast<int>(ids.size());
    s->pickCharacter(ids[index]);
}

void LobbyPanel::buildLevelRow() {
    RaceSession* s = RaceSession::get();
    const RaceLevel& l = s->level();
    const bool picker = s->isHost() && l.kind == "campaign" && s->phase() == Phase::Lobby;
    const std::string key = l.kind + "|" + l.name + "|" + std::to_string(l.chapter) + "|" + std::to_string(l.level) +
                            (picker ? "p" : "");
    if (key == _levelKey) return;
    _levelKey = key;
    _levelRow->removeAllChildren();
    _chapterDrop = nullptr;
    _levelDrop = nullptr;
    const float y = _contentTop - 200.0f;
    Label* cap = text(l.kindLabel().c_str(), oui::kFontBodyBold, 34.0f, oui::kInkDim);
    std::string kind = l.kindLabel();
    std::transform(kind.begin(), kind.end(), kind.begin(), ::toupper);
    cap->setString(kind);
    cap->setPosition(100.0f, y + 52.0f);
    _levelRow->addChild(cap);
    Label* name = text("", oui::kFontHeading, 66.0f, oui::kInk);
    oui::setEllipsized(name, "\xE2\x80\x9C" + (l.name.empty() ? std::string("?") : l.name) + "\xE2\x80\x9D",
                       picker ? _w * 0.42f : _w - 300.0f);
    name->setPosition(100.0f, y - 12.0f);
    _levelRow->addChild(name);
    if (l.forced) {
        Label* f = text("Rider: " + RaceSession::characterName(l.forcedCharacter), oui::kFontBody, 40.0f, oui::kInkDim);
        f->setPosition(110.0f + name->getContentSize().width + 30.0f, y - 12.0f);
        if (!picker) _levelRow->addChild(f);
    }
    if (!picker) return;

    // campaign chapter / level pickers
    Settings* settings = Settings::getInstance();
    ValueVector chapters = settings->getAllChaptersData();
    std::vector<std::string> chapterNames;
    std::vector<int> chapterIndices;
    int chapterSel = 0;
    for (const Value& v : chapters) {
        ValueMap c = v.asValueMap();
        chapterIndices.push_back(c["index"].asInt());
        chapterNames.push_back(c["name"].asString());
        if (c["index"].asInt() == l.chapter) chapterSel = static_cast<int>(chapterNames.size()) - 1;
    }
    std::vector<std::string> levelNames;
    ValueMap chapter = settings->getChapterData(l.chapter);
    if (chapter.count("levels")) {
        int n = 1;
        for (const Value& v : chapter["levels"].asValueVector()) {
            ValueMap lv = v.asValueMap();
            std::string ln = lv["name"].asString();
            levelNames.push_back(std::to_string(n++) + ". " + (ln.empty() ? std::string("Level") : ln));
        }
    }
    if (chapterNames.empty() || levelNames.empty()) return;
    const float dw = (_w * 0.5f - 160.0f) * 0.5f;
    _chapterDrop = oui::Dropdown::create(chapterNames, Size(dw, 110.0f), oui::Button::window("blue"), 44.0f);
    _chapterDrop->setSelectedIndex(chapterSel);
    _chapterDrop->setPosition(_w * 0.5f + 40.0f + dw * 0.5f, y + 10.0f);
    _chapterDrop->onSelect = [chapterIndices](int i) {
        RaceLevel nl;
        nl.kind = "campaign";
        nl.chapter = chapterIndices[i];
        nl.level = 0;
        RaceSession::get()->setLevel(nl);
    };
    _levelRow->addChild(_chapterDrop);
    _levelDrop = oui::Dropdown::create(levelNames, Size(dw, 110.0f), oui::Button::window("blue"), 44.0f);
    _levelDrop->setSelectedIndex(std::min(l.level, static_cast<int>(levelNames.size()) - 1));
    _levelDrop->setPosition(_w * 0.5f + 70.0f + dw * 1.5f, y + 10.0f);
    const int ch = l.chapter;
    _levelDrop->onSelect = [ch](int i) {
        RaceLevel nl;
        nl.kind = "campaign";
        nl.chapter = ch;
        nl.level = i;
        RaceSession::get()->setLevel(nl);
    };
    _levelRow->addChild(_levelDrop);
}

void LobbyPanel::rebuildPlayers() {
    RaceSession* s = RaceSession::get();
    _playersNode->removeAllChildren();
    const float rowH = 160.0f, gap = 14.0f;
    float y = _playersBox.getMaxY() - 22.0f;
    const float x = _playersBox.origin.x + 22.0f;
    const float w = _playersBox.size.width - 44.0f;
    for (const RacePlayer& p : s->players()) {
        auto* bg = oui::roundedRect(Size(w, rowH), 28.0f, Color3B::WHITE, p.id == s->localId() ? 255 : 215);
        bg->setAnchorPoint(Vec2::ZERO);
        bg->setPosition(x, y - rowH);
        _playersNode->addChild(bg);
        Node* d = dot(RaceSession::slotColor(p.slot), 64.0f);
        d->setPosition(x + 70.0f, y - rowH * 0.5f);
        _playersNode->addChild(d);
        std::string name = p.name;
        if (p.id == 1) name += "  \xC2\xB7  host";
        if (p.id == s->localId()) name += " (you)";
        Label* n = text("", oui::kFontHeading, 56.0f, oui::kInk);
        oui::setEllipsized(n, name, w - 470.0f);
        n->setPosition(x + 130.0f, y - rowH * 0.5f + 26.0f);
        _playersNode->addChild(n);
        Label* sub = text(RaceSession::characterName(p.character) + "  \xC2\xB7  " + (p.platform == "phone" ? "Phone" : "PC"),
                          oui::kFontBody, 38.0f, oui::kInkDim);
        sub->setPosition(x + 132.0f, y - rowH * 0.5f - 34.0f);
        _playersNode->addChild(sub);
        const bool lobby = s->phase() == Phase::Lobby;
        Label* st = text(statusText(p, lobby), oui::kFontBodyBold, 44.0f,
                         (lobby ? p.ready : p.status != Status::Left) ? kOk : oui::kInkDim, Vec2(1.0f, 0.5f));
        st->setPosition(x + w - 30.0f, y - rowH * 0.5f);
        _playersNode->addChild(st);
        y -= rowH + gap;
    }
    const int free = kMaxPlayers - static_cast<int>(s->players().size());
    for (int i = 0; i < free; ++i) {
        auto* bg = oui::roundedRect(Size(w, rowH), 28.0f, Color3B::WHITE, 90);
        bg->setAnchorPoint(Vec2::ZERO);
        bg->setPosition(x, y - rowH);
        _playersNode->addChild(bg);
        Label* e = text(s->isHost() ? "Invite a nearby player" : "Free spot", oui::kFontBody, 40.0f, oui::kInkDim);
        e->setPosition(x + 50.0f, y - rowH * 0.5f);
        _playersNode->addChild(e);
        y -= rowH + gap;
    }
}

void LobbyPanel::rebuildRider() {
    RaceSession* s = RaceSession::get();
    const RacePlayer* me = s->localPlayer();
    const int character = s->level().forced ? s->level().forcedCharacter : (me ? me->character : 2);
    const std::string key = std::to_string(character) + (s->level().forced ? "f" : "") +
                            std::to_string(static_cast<int>(s->phase()));
    if (key == _riderKey) return;
    _riderKey = key;
    _riderNode->removeAllChildren();
    const Rect& r = _riderBox;
    const std::string portrait = oui::characterPortrait(character);
    if (Sprite* p = Sprite::create(portrait)) {
        const float scale = (r.size.height - 60.0f) / p->getContentSize().height;
        p->setScale(scale);
        p->setPosition(r.origin.x + 40.0f + p->getContentSize().width * scale * 0.5f, r.getMidY());
        _riderNode->addChild(p);
    }
    const float tx = r.origin.x + r.size.height + 40.0f;
    Label* n = text("", oui::kFontHeading, 64.0f, oui::kInk);
    oui::setEllipsized(n, RaceSession::characterName(character), r.getMaxX() - tx - 40.0f);
    n->setPosition(tx, r.getMidY() + 70.0f);
    _riderNode->addChild(n);
    if (s->level().forced) {
        Label* f = text("This level picks the rider.", oui::kFontBody, 40.0f, oui::kInkDim);
        f->setPosition(tx, r.getMidY() - 30.0f);
        _riderNode->addChild(f);
        return;
    }
    auto* prev = oui::Button::create("<", Size(150.0f, 120.0f), oui::Button::window("blue"), 64.0f);
    prev->setPosition(tx + 75.0f, r.getMidY() - 70.0f);
    prev->setCallback([this]() { cycleCharacter(-1); });
    _riderNode->addChild(prev);
    auto* next = oui::Button::create(">", Size(150.0f, 120.0f), oui::Button::window("blue"), 64.0f);
    next->setPosition(tx + 245.0f, r.getMidY() - 70.0f);
    next->setCallback([this]() { cycleCharacter(1); });
    _riderNode->addChild(next);
    const bool changeable = s->phase() == Phase::Lobby;
    prev->setEnabled(changeable);
    next->setEnabled(changeable);
}

void LobbyPanel::rebuildNearby() {
    RaceSession* s = RaceSession::get();
    if (!s->isHost()) return;
    std::string key;
    for (const net::Peer& p : _peers) {
        key += p.id + "|" + p.name + "|";
        auto it = s->invites().find(p.id);
        if (it != s->invites().end()) key += std::to_string(it->second.state) + it->second.text;
        key += ";";
    }
    key += std::to_string(s->players().size());
    if (key == _nearbyKey) return;
    _nearbyKey = key;
    _nearbyNode->removeAllChildren();
    const Rect& r = _nearbyBox;
    if (_peers.empty()) {
        Sprite* spin = oui::createSpinner(100.0f, oui::kInkDim);
        spin->setPosition(r.getMidX(), r.getMidY() + 60.0f);
        _nearbyNode->addChild(spin);
        Label* l = text("Looking for players on your Wi-Fi\xE2\x80\xA6", oui::kFontHeading, 52.0f, oui::kInk, Vec2(0.5f, 0.5f));
        l->setPosition(r.getMidX(), r.getMidY() - 50.0f);
        _nearbyNode->addChild(l);
        return;
    }
    const float rowH = 130.0f, gap = 12.0f;
    float y = r.getMaxY() - 20.0f;
    const float x = r.origin.x + 20.0f, w = r.size.width - 40.0f;
    for (const net::Peer& p : _peers) {
        if (y - rowH < r.origin.y) break;
        auto* bg = oui::roundedRect(Size(w, rowH), 26.0f, Color3B::WHITE, 230);
        bg->setAnchorPoint(Vec2::ZERO);
        bg->setPosition(x, y - rowH);
        _nearbyNode->addChild(bg);
        Node* icon = net::ui::deviceIcon(p.platform, 80.0f, Color4F(Color3B(70, 70, 82)));
        icon->setPosition(x + 70.0f, y - rowH * 0.5f);
        _nearbyNode->addChild(icon);
        Label* n = text("", oui::kFontHeading, 52.0f, oui::kInk);
        oui::setEllipsized(n, p.name, w - 520.0f);
        n->setPosition(x + 140.0f, y - rowH * 0.5f);
        _nearbyNode->addChild(n);
        bool inRace = false;
        for (const RacePlayer& rp : s->players()) {
            if (rp.name == p.name && rp.id != s->localId()) inRace = true;
        }
        auto it = s->invites().find(p.id);
        const bool busy = it != s->invites().end() &&
                          (it->second.state == InviteInfo::Connecting || it->second.state == InviteInfo::Waiting);
        if (inRace || (it != s->invites().end() && it->second.state == InviteInfo::Joined)) {
            Label* st = text("In the race", oui::kFontBodyBold, 42.0f, kOk, Vec2(1.0f, 0.5f));
            st->setPosition(x + w - 30.0f, y - rowH * 0.5f);
            _nearbyNode->addChild(st);
        } else if (busy) {
            Label* st = text(it->second.text, oui::kFontBodyBold, 42.0f, oui::kInkDim, Vec2(1.0f, 0.5f));
            st->setPosition(x + w - 30.0f, y - rowH * 0.5f);
            _nearbyNode->addChild(st);
        } else {
            if (it != s->invites().end()) {
                Label* st = text(it->second.text, oui::kFontBody, 36.0f, kError, Vec2(1.0f, 0.5f));
                st->setPosition(x + w - 300.0f, y - rowH * 0.5f);
                _nearbyNode->addChild(st);
            }
            const net::Peer peer = p;
            auto* b = oui::Button::create("Invite", Size(250.0f, 96.0f), oui::Button::window("blue"), 46.0f);
            b->setPosition(x + w - 145.0f, y - rowH * 0.5f);
            b->setCallback([peer]() { RaceSession::get()->invite(peer); });
            _nearbyNode->addChild(b);
        }
        y -= rowH + gap;
    }
}

void LobbyPanel::refresh() {
    if (_closing) return;
    RaceSession* s = RaceSession::get();
    if (s->phase() != Phase::Lobby) return;
    buildLevelRow();
    rebuildPlayers();
    rebuildRider();
    rebuildNearby();
    _status->setString(s->statusLine());
    _status->setColor(oui::kInk);
    if (s->isHost()) {
        _mainBtn->setText("Start Race");
        _mainBtn->setStyle(oui::Button::chunky("blue"));
        _mainBtn->setEnabled(s->canStart(nullptr));
    } else {
        const RacePlayer* me = s->localPlayer();
        const bool ready = me && me->ready;
        _mainBtn->setText(ready ? "Not Ready" : "Ready!");
        _mainBtn->setStyle(oui::Button::chunky(ready ? "grey" : "blue"));
        _mainBtn->setEnabled(me != nullptr);
    }
}

// ---- ResultsPanel ------------------------------------------------------------------------------------

ResultsPanel* ResultsPanel::create() {
    ResultsPanel* p = new (std::nothrow) ResultsPanel();
    if (p && p->init()) {
        p->autorelease();
        return p;
    }
    delete p;
    return nullptr;
}

bool ResultsPanel::init() {
    _w = 2000.0f;
    const float h = oui::windowPanelStrip() + 1250.0f;
    if (!initModal(Size(_w, h), "Race Results")) return false;
    Label* level = text("", oui::kFontBodyBold, 50.0f, oui::kInk, Vec2(0.5f, 0.5f));
    oui::setEllipsized(level, "\xE2\x80\x9C" + RaceSession::get()->level().name + "\xE2\x80\x9D", _w - 300.0f);
    level->setPosition(_w * 0.5f, _contentTop - 190.0f);
    _panel->addChild(level);
    _rows = Node::create();
    _panel->addChild(_rows);
    _buttons = Node::create();
    _panel->addChild(_buttons);
    refresh();
    return true;
}

void ResultsPanel::onClosed() {
    RaceSession::get()->resultsClosed(this);
}

bool ResultsPanel::onKey(EventKeyboard::KeyCode key) {
    if (key == EventKeyboard::KeyCode::KEY_ENTER || key == EventKeyboard::KeyCode::KEY_KP_ENTER) {
        if (RaceSession::get()->isHost()) RaceSession::get()->rematch();
        return true;
    }
    return false;
}

void ResultsPanel::refresh() {
    if (_closing) return;
    RaceSession* s = RaceSession::get();
    std::vector<RacePlayer> players = s->players();
    std::stable_sort(players.begin(), players.end(), [](const RacePlayer& a, const RacePlayer& b) {
        const int pa = a.place > 0 ? a.place : 99, pb = b.place > 0 ? b.place : 99;
        return pa < pb;
    });
    std::string key = std::to_string(static_cast<int>(s->phase()));
    for (const RacePlayer& p : players) key += p.name + statusName(p.status) + std::to_string(p.timeMs) + std::to_string(p.place);
    if (key == _key) return;
    _key = key;
    _rows->removeAllChildren();
    const float rowH = 150.0f, gap = 16.0f, x = 120.0f, w = _w - 240.0f;
    float y = _contentTop - 280.0f;
    int best = -1;
    for (const RacePlayer& p : players) {
        if (p.place == 1) best = p.timeMs;
    }
    for (const RacePlayer& p : players) {
        auto* bg = oui::roundedRect(Size(w, rowH), 28.0f, Color3B::WHITE, p.id == s->localId() ? 255 : 210);
        bg->setAnchorPoint(Vec2::ZERO);
        bg->setPosition(x, y - rowH);
        _rows->addChild(bg);
        Label* place = text(placeText(p.place), oui::kFontHeading, 72.0f, p.place == 1 ? Color3B(214, 160, 20) : oui::kInk,
                            Vec2(0.5f, 0.5f));
        place->setPosition(x + 120.0f, y - rowH * 0.5f);
        _rows->addChild(place);
        Node* d = dot(RaceSession::slotColor(p.slot), 56.0f);
        d->setPosition(x + 260.0f, y - rowH * 0.5f);
        _rows->addChild(d);
        Label* n = text("", oui::kFontHeading, 58.0f, oui::kInk);
        oui::setEllipsized(n, p.name + (p.id == s->localId() ? " (you)" : ""), w - 900.0f);
        n->setPosition(x + 320.0f, y - rowH * 0.5f + 22.0f);
        _rows->addChild(n);
        Label* c = text(RaceSession::characterName(p.character), oui::kFontBody, 38.0f, oui::kInkDim);
        c->setPosition(x + 322.0f, y - rowH * 0.5f - 36.0f);
        _rows->addChild(c);
        std::string t;
        Color3B tc = oui::kInk;
        if (p.status == Status::Finished) {
            t = formatTime(p.timeMs);
            if (best >= 0 && p.place > 1) t += "  (+" + formatTime(p.timeMs - best) + ")";
        } else if (p.status == Status::Left) {
            t = "Left";
            tc = oui::kInkDim;
        } else if (p.status == Status::GaveUp) {
            t = "DNF";
            tc = kError;
        } else {
            t = "Still riding\xE2\x80\xA6";
            tc = oui::kInkDim;
        }
        Label* tl = text(t, oui::kFontHeading, 60.0f, tc, Vec2(1.0f, 0.5f));
        tl->setPosition(x + w - 40.0f, y - rowH * 0.5f);
        _rows->addChild(tl);
        y -= rowH + gap;
    }

    _buttons->removeAllChildren();
    if (s->isHost() && s->phase() == Phase::Results) {
        auto* rematch = oui::Button::create("Rematch", Size(640.0f, 180.0f), oui::Button::chunky("blue"), 80.0f);
        rematch->setPosition(_w * 0.5f - 680.0f, 150.0f);
        rematch->setCallback([]() { RaceSession::get()->rematch(); });
        _buttons->addChild(rematch);
        auto* lobby = oui::Button::create("Lobby", Size(560.0f, 180.0f), oui::Button::chunky("pink"), 80.0f);
        lobby->setPosition(_w * 0.5f, 150.0f);
        lobby->setCallback([]() { RaceSession::get()->backToLobby(); });
        _buttons->addChild(lobby);
        auto* leave = oui::Button::create("Close Race", Size(560.0f, 180.0f), oui::Button::chunky("grey"), 72.0f);
        leave->setPosition(_w * 0.5f + 640.0f, 150.0f);
        leave->setCallback([this]() {
            RaceSession::get()->leave();
            dismiss();
        });
        _buttons->addChild(leave);
    } else {
        Label* wait = text(s->phase() == Phase::Results ? "Waiting for " + s->hostName() + " \xE2\x80\x94 rematch or lobby\xE2\x80\xA6" : "",
                           oui::kFontBodyBold, 44.0f, oui::kInkDim, Vec2(0.5f, 0.5f));
        wait->setPosition(_w * 0.5f, 290.0f);
        _buttons->addChild(wait);
        auto* leave = oui::Button::create("Leave Race", Size(640.0f, 170.0f), oui::Button::chunky("grey"), 76.0f);
        leave->setPosition(_w * 0.5f, 140.0f);
        leave->setCallback([this]() {
            RaceSession::get()->leave();
            dismiss();
        });
        _buttons->addChild(leave);
    }
}

// ---- RaceHud -------------------------------------------------------------------------------------------

RaceHud* RaceHud::create() {
    RaceHud* h = new (std::nothrow) RaceHud();
    if (h && h->init()) {
        h->autorelease();
        return h;
    }
    delete h;
    return nullptr;
}

bool RaceHud::init() {
    if (!Node::init()) return false;
    setName("ow_race_hud");
    oui::loadAtlases();
    const Size vs = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();

    _panel = Node::create();
    _panel->setPosition(origin.x + 40.0f, origin.y + vs.height - 330.0f);   // below the pause button
    addChild(_panel);
    Node* flag = flagIcon(70.0f, Color3B::WHITE);
    flag->setPosition(46.0f, -40.0f);
    _panel->addChild(flag, 2);
    Label* title = text("RACE", oui::kFontHeading, 52.0f, Color3B::WHITE);
    title->setPosition(96.0f, -40.0f);
    title->enableShadow(Color4B(0, 0, 0, 120), Size(0, -4));
    _panel->addChild(title, 2);
    _clock = text("0:00.00", oui::kFontHeading, 60.0f, Color3B::WHITE, Vec2(1.0f, 0.5f));
    _clock->enableShadow(Color4B(0, 0, 0, 120), Size(0, -4));
    _clock->setPosition(_panelW - 30.0f, -40.0f);
    _panel->addChild(_clock, 2);
    _rows = Node::create();
    _panel->addChild(_rows, 1);

    _countdown = text("", oui::kFontHeading, 420.0f, Color3B::WHITE, Vec2(0.5f, 0.5f));
    _countdown->enableOutline(Color4B(0, 0, 0, 200), 14);
    _countdown->setPosition(origin.x + vs.width * 0.5f, origin.y + vs.height * 0.55f);
    addChild(_countdown, 5);
    _banner = text("", oui::kFontHeading, 96.0f, Color3B::WHITE, Vec2(0.5f, 0.5f));
    _banner->enableOutline(Color4B(0, 0, 0, 190), 8);
    _banner->setPosition(origin.x + vs.width * 0.5f, origin.y + vs.height - 420.0f);
    addChild(_banner, 5);

    _giveUp = oui::Button::create("Give Up", Size(380.0f, 110.0f), oui::Button::ghost(), 46.0f);
    _giveUp->setCallback([]() { RaceSession::get()->giveUp(); });
    addChild(_giveUp, 3);
    _endRace = oui::Button::create("End Race", Size(380.0f, 110.0f), oui::Button::ghost(), 46.0f);
    _endRace->setCallback([]() { RaceSession::get()->endRace(); });
    addChild(_endRace, 3);
    _results = oui::Button::create("Results", Size(380.0f, 110.0f), oui::Button::window("blue"), 46.0f);
    _results->setCallback([]() {
        ResultsPanel* p = ResultsPanel::create();
        if (p) p->present();
    });
    addChild(_results, 3);
    refresh();
    return true;
}

void RaceHud::layoutButtons() {
    const float rowsBottom = _panel->getPositionY() - 80.0f - _rows->getContentSize().height;
    float y = rowsBottom - 80.0f;
    const float x = _panel->getPositionX() + 190.0f;
    for (online::ui::Button* b : {_giveUp, _endRace, _results}) {
        if (!b->isVisible()) continue;
        b->setPosition(x, y);
        y -= 125.0f;
    }
}

void RaceHud::refresh() {
    RaceSession* s = RaceSession::get();
    // hidden under the pause menu
    bool paused = false;
    if (getParent()) {
        for (Node* c : getParent()->getChildren()) {
            if (dynamic_cast<PauseLayer*>(c)) paused = true;
        }
    }
    setVisible(!paused && s->active());
    if (paused) return;

    const RacePlayer* self = s->localPlayer();
    const bool done = self && self->status == Status::Finished;
    _clock->setString(formatTime(done ? self->timeMs : static_cast<int>(s->raceClock() * 1000.0)));
    _clock->setColor(done ? Color3B(140, 240, 150) : Color3B::WHITE);

    // rows: finished first (by time), then by progress
    std::vector<RacePlayer> players = s->players();
    std::stable_sort(players.begin(), players.end(), [s](const RacePlayer& a, const RacePlayer& b) {
        const bool fa = a.status == Status::Finished, fb = b.status == Status::Finished;
        if (fa != fb) return fa;
        if (fa) return a.timeMs < b.timeMs;
        return s->progressOf(a.id) > s->progressOf(b.id);
    });
    std::string key;
    for (const RacePlayer& p : players) {
        const float pr = s->progressOf(p.id);
        key += std::to_string(p.id) + statusName(p.status) + std::to_string(p.timeMs) + "|" +
               std::to_string(static_cast<int>(pr * 100.0f)) + ";";
    }
    if (key != _rowsKey) {
        _rowsKey = key;
        _rows->removeAllChildren();
        const float rowH = 92.0f;
        const float h = 80.0f + players.size() * rowH + 20.0f;
        auto* bg = oui::roundedRect(Size(_panelW, h), 30.0f, Color3B(20, 24, 34), 165);
        bg->setAnchorPoint(Vec2(0.0f, 1.0f));
        bg->setPosition(0.0f, 0.0f);
        _rows->addChild(bg);
        float y = -80.0f;
        for (const RacePlayer& p : players) {
            const Color3B c = RaceSession::slotColor(p.slot);
            Node* d = dot(c, 40.0f);
            d->setPosition(46.0f, y - rowH * 0.5f + 8.0f);
            _rows->addChild(d);
            Label* n = text("", oui::kFontBodyBold, 42.0f, p.id == s->localId() ? Color3B::WHITE : Color3B(225, 230, 240));
            oui::setEllipsized(n, p.name, _panelW - 420.0f);
            n->setPosition(84.0f, y - rowH * 0.5f + 12.0f);
            _rows->addChild(n);
            std::string st;
            Color3B sc(225, 230, 240);
            switch (p.status) {
            case Status::Finished: st = formatTime(p.timeMs); sc = Color3B(140, 240, 150); break;
            case Status::Dead: st = "Dead"; sc = Color3B(255, 120, 110); break;
            case Status::Ejected: st = "Ejected"; sc = Color3B(255, 200, 110); break;
            case Status::GaveUp: st = "Gave up"; sc = Color3B(170, 170, 180); break;
            case Status::Left: st = "Left"; sc = Color3B(170, 170, 180); break;
            case Status::Loading: st = "Loading"; break;
            default: st = ""; break;
            }
            const float pr = s->progressOf(p.id);
            if (st.empty()) st = pr >= 0.0f ? StringUtils::format("%d%%", static_cast<int>(pr * 100.0f + 0.5f)) : "Racing";
            Label* sl = text(st, oui::kFontBodyBold, 40.0f, sc, Vec2(1.0f, 0.5f));
            sl->setPosition(_panelW - 30.0f, y - rowH * 0.5f + 12.0f);
            _rows->addChild(sl);
            // progress bar
            const float bw = _panelW - 114.0f;
            auto* track = oui::roundedRect(Size(bw, 12.0f), 6.0f, Color3B::WHITE, 50);
            track->setAnchorPoint(Vec2::ZERO);
            track->setPosition(84.0f, y - rowH + 10.0f);
            _rows->addChild(track);
            if (pr > 0.0f) {
                auto* fill = oui::roundedRect(Size(std::max(12.0f, bw * pr), 12.0f), 6.0f, c, 255);
                fill->setAnchorPoint(Vec2::ZERO);
                fill->setPosition(84.0f, y - rowH + 10.0f);
                _rows->addChild(fill);
            }
            y -= rowH;
        }
        _rows->setContentSize(Size(_panelW, h - 80.0f));
    }

    // countdown
    const Phase phase = s->phase();
    if (phase == Phase::Countdown || phase == Phase::Loading) {
        const int n = phase == Phase::Loading ? 0 : static_cast<int>(std::ceil(s->countdownLeft()));
        if (n != _lastCount) {
            _lastCount = n;
            _countdown->setString(n > 0 ? std::to_string(n) : (phase == Phase::Loading ? "" : "GO!"));
            _countdown->setOpacity(255);
            _countdown->setScale(1.4f);
            _countdown->stopAllActions();
            _countdown->runAction(EaseBackOut::create(ScaleTo::create(0.25f, 1.0f)));
        }
        _banner->setString(phase == Phase::Loading ? "Waiting for the other riders\xE2\x80\xA6" : "Get ready!");
    } else {
        if (_lastCount != 0 && _lastCount != -2) {
            _lastCount = -2;
            _countdown->setString("GO!");
            _countdown->setScale(1.4f);
            _countdown->stopAllActions();
            _countdown->setOpacity(255);
            _countdown->runAction(Sequence::create(EaseBackOut::create(ScaleTo::create(0.25f, 1.0f)), DelayTime::create(0.5f),
                                                   FadeOut::create(0.3f), nullptr));
        } else if (_lastCount == 0) {
            _lastCount = -2;
            _countdown->stopAllActions();
            _countdown->runAction(Sequence::create(DelayTime::create(0.6f), FadeOut::create(0.3f), nullptr));
        }
        const RacePlayer* me = s->localPlayer();
        std::string banner;
        if (me && me->status == Status::Finished) {
            int place = 1;
            for (const RacePlayer& p : s->players()) {
                if (p.status == Status::Finished && p.timeMs >= 0 && p.timeMs < me->timeMs) ++place;
            }
            banner = "Finished " + placeText(place) + "!  " + formatTime(me->timeMs);
        } else if (me && me->status == Status::GaveUp) {
            banner = "You gave up";
        }
        if (phase == Phase::Results) banner = "Race over";
        _banner->setString(banner);
    }

    const RacePlayer* me = s->localPlayer();
    const bool racing = phase == Phase::Racing;
    _giveUp->setVisible(racing && me && !statusIsFinal(me->status));
    _endRace->setVisible(racing && s->isHost());
    _results->setVisible(phase == Phase::Results && !oui::modalOpen());
    layoutButtons();
}

}  // namespace race
