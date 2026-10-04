// NET (PC addition): see NearbyPanels.h.
#include "net/NearbyPanels.h"

#include <algorithm>
#include <cmath>

#include "net/NetUi.h"
#include "online/OnlineUi.h"

USING_NS_CC;

namespace net {

namespace {

namespace oui = online::ui;

const Color3B kOk(46, 140, 70);
const Color3B kError(196, 56, 56);
const Color3B kLink(46, 120, 186);
const Color3B kSelectedSub(225, 236, 255);

std::string quoted(const std::string& s) {
    return "\xE2\x80\x9C" + s + "\xE2\x80\x9D";   // curly quotes
}

oui::Button::Style linkStyle() {
    oui::Button::Style link;
    link.color = Color3B(60, 60, 70);
    link.opacity = 0;
    link.radius = 20.0f;
    link.textColor = kLink;
    return link;
}

// "You appear as [name]": a caption and a small field, centred on cx. Applies the name when the
// player stops typing and when the field loses focus.
class NameLine : public Node {
public:
    static NameLine* create(float fieldW) {
        NameLine* n = new (std::nothrow) NameLine();
        if (n && n->init(fieldW)) {
            n->autorelease();
            return n;
        }
        delete n;
        return nullptr;
    }
    bool init(float fieldW) {
        if (!Node::init()) return false;
        Label* caption = ui::label("You appear as", oui::kFontBody, 44.0f, oui::kInkDim, Vec2(1.0f, 0.5f));
        const float total = caption->getContentSize().width + 28.0f + fieldW;
        caption->setPosition(-total * 0.5f + caption->getContentSize().width, 0.0f);
        addChild(caption);
        _field = oui::SearchField::create(Size(fieldW, 96.0f), "Your name");
        _field->setIconVisible(false);
        _field->setText(LevelTransfer::getInstance()->playerName());
        _field->setPosition(total * 0.5f - fieldW * 0.5f, 0.0f);
        _field->onChange = [this]() {
            unschedule("apply_name");
            scheduleOnce([this](float) { apply(); }, 0.8f, "apply_name");
        };
        addChild(_field);
        return true;
    }
    void apply() {
        LevelTransfer* t = LevelTransfer::getInstance();
        const std::string name = LevelTransfer::sanitizeName(_field->text());
        if (!name.empty() && name != t->playerName()) t->setPlayerName(name);
    }
    void finish() {
        apply();
        _field->detachWithIME();
        if (LevelTransfer::sanitizeName(_field->text()).empty()) _field->setText(LevelTransfer::getInstance()->playerName());
    }
    oui::SearchField* field() const { return _field; }

private:
    oui::SearchField* _field = nullptr;
};

bool hits(Node* node, const Vec2& world) {
    if (!node || !oui::isShown(node)) return false;
    const Vec2 p = node->convertToNodeSpace(world);
    return Rect(Vec2::ZERO, node->getContentSize()).containsPoint(p);
}

// ---- Send to Nearby ------------------------------------------------------------------------

class SendPanel : public ui::Modal {
public:
    static SendPanel* create(const LevelPackage& level) {
        SendPanel* p = new (std::nothrow) SendPanel();
        if (p && p->init(level)) {
            p->autorelease();
            return p;
        }
        delete p;
        return nullptr;
    }

    ~SendPanel() override {
        if (_sendId > 0) LevelTransfer::getInstance()->cancelSend(_sendId);
        if (_observer) LanDiscovery::getInstance()->removeObserver(_observer);
    }

private:
    struct Row {
        Node* node = nullptr;
        Node* bg = nullptr;
        Node* selectedBg = nullptr;
        Node* icon = nullptr;
        Node* iconSelected = nullptr;
        Label* name = nullptr;
        Label* device = nullptr;
    };

    static constexpr float kW = 1800.0f;
    static constexpr float kBoxH = 640.0f;
    static constexpr float kRowH = 186.0f;
    static constexpr float kRowGap = 16.0f;
    static constexpr float kBoxBottom = 640.0f;

    bool init(const LevelPackage& level) {
        _level = level;
        const float strip = oui::windowPanelStrip();
        const float h = kBoxBottom + kBoxH + 250.0f + strip;
        if (!initModal(Size(kW, h), "Send to Nearby")) return false;
        LevelTransfer::getInstance()->start();

        Label* levelName = ui::label("", oui::kFontBodyBold, 54.0f, oui::kInk, Vec2(0.5f, 0.5f));
        oui::setEllipsized(levelName, quoted(level.name.empty() ? std::string("Untitled") : level.name), kW - 300.0f);
        levelName->setPosition(kW * 0.5f, _contentTop - 190.0f);
        _panel->addChild(levelName);

        // The players box.
        const float boxX = 90.0f;
        const float boxW = kW - 180.0f;
        _box = Rect(boxX, kBoxBottom, boxW, kBoxH);
        auto* box = oui::roundedRect(_box.size, 34.0f, Color3B::WHITE, 150);
        box->setAnchorPoint(Vec2::ZERO);
        box->setPosition(_box.origin);
        _panel->addChild(box);
        _rowsRect = Rect(boxX + 22.0f, kBoxBottom + 22.0f, boxW - 44.0f, kBoxH - 44.0f);
        auto* clip = ClippingRectangleNode::create(_rowsRect);
        _panel->addChild(clip);
        _rowsNode = Node::create();
        clip->addChild(_rowsNode);

        _empty = Node::create();
        _empty->setPosition(_box.getMidX(), _box.getMidY());
        _panel->addChild(_empty);
        Sprite* spin = oui::createSpinner(120.0f, oui::kInkDim);
        spin->setPosition(0.0f, 130.0f);
        _empty->addChild(spin);
        Label* looking = ui::label("Looking for players on your Wi-Fi\xE2\x80\xA6", oui::kFontHeading, 64.0f, oui::kInk,
                                   Vec2(0.5f, 0.5f));
        looking->setPosition(0.0f, -10.0f);
        _empty->addChild(looking);
        Label* hint = Label::createWithTTF(
            "OpenWheels has to be open on the other phone or PC, on the same Wi-Fi.", oui::kFontBody, 44.0f,
            Size(boxW - 200.0f, 0.0f), TextHAlignment::CENTER);
        hint->setColor(oui::kInkDim);
        hint->setAnchorPoint(Vec2(0.5f, 1.0f));
        hint->setPosition(0.0f, -80.0f);
        _empty->addChild(hint);

        _name = NameLine::create(560.0f);
        _name->setPosition(kW * 0.5f, kBoxBottom - 82.0f);
        _panel->addChild(_name);

        // Status line (progress / result) above the big button.
        _statusSpinner = oui::createSpinner(56.0f, oui::kInkDim);
        _statusSpinner->setVisible(false);
        _panel->addChild(_statusSpinner);
        _status = ui::label("", oui::kFontBodyBold, 46.0f, oui::kInk, Vec2(0.5f, 0.5f));
        _status->setPosition(kW * 0.5f, kBoxBottom - 188.0f);
        _panel->addChild(_status);

        _sendBtn = oui::Button::create("Send", Size(1000.0f, 200.0f), oui::Button::chunky("blue"), 84.0f);
        _sendBtn->setPosition(kW * 0.5f, kBoxBottom - 345.0f);
        _sendBtn->setCallback([this]() { sendPressed(); });
        _panel->addChild(_sendBtn);

        // Code fallback: a link that expands into a field + button.
        _codeLink = oui::Button::create("Not listed? Use a code", Size(700.0f, 90.0f), linkStyle(), 46.0f,
                                        oui::kFontBodyBold);
        _codeLink->setPosition(kW * 0.5f, kBoxBottom - 530.0f);
        _codeLink->setCallback([this]() { expandCode(); });
        _panel->addChild(_codeLink);
        _codeRow = Node::create();
        _codeRow->setPosition(kW * 0.5f, kBoxBottom - 530.0f);
        _codeRow->setVisible(false);
        _panel->addChild(_codeRow);
        Label* codeCaption = ui::label("Their code", oui::kFontBody, 44.0f, oui::kInkDim, Vec2(1.0f, 0.5f));
        codeCaption->setPosition(-390.0f, 0.0f);
        _codeRow->addChild(codeCaption);
        _codeField = oui::SearchField::create(Size(560.0f, 100.0f), "e.g. K7QM-2D9A");
        _codeField->setIconVisible(false);
        _codeField->setPosition(-80.0f, 0.0f);
        _codeField->onChange = [this]() { refresh(); };
        _codeRow->addChild(_codeField);
        _codeBtn = oui::Button::create("Send", Size(300.0f, 112.0f), oui::Button::window("blue"), 56.0f);
        _codeBtn->setPosition(390.0f, 0.0f);
        _codeBtn->setCallback([this]() { sendToCode(); });
        _codeRow->addChild(_codeBtn);

        _observer = LanDiscovery::getInstance()->addObserver([this](const std::vector<Peer>& peers) { peersChanged(peers); });
        refresh();
        return true;
    }

    void onClosed() override {
        _name->finish();
        _codeField->detachWithIME();
        if (_sendId > 0) LevelTransfer::getInstance()->cancelSend(_sendId);
        _sendId = 0;
        if (_observer) LanDiscovery::getInstance()->removeObserver(_observer);
        _observer = 0;
    }

    bool onKey(EventKeyboard::KeyCode key) override {
        using K = EventKeyboard::KeyCode;
        if (key == K::KEY_ENTER || key == K::KEY_KP_ENTER) {
            if (_codeRow->isVisible() && !_codeField->text().empty()) sendToCode();
            else if (_sendId == 0) sendPressed();
            return true;
        }
        if (key == K::KEY_UP_ARROW || key == K::KEY_DOWN_ARROW) {
            if (_peers.empty()) return true;
            int i = selectedIndex();
            i = key == K::KEY_UP_ARROW ? std::max(0, i - 1) : std::min((int)_peers.size() - 1, i + 1);
            select(i);
            return true;
        }
        return false;
    }

    void onHover(const Vec2& world) override {
        const int row = rowAt(world);
        if (row != _hover) {
            _hover = row;
            layoutRows();
        }
    }

    void onScroll(const Vec2& world, float amount) override {
        setOffset(_offset + amount * kRowH * 0.6f);
    }

    void onTouch(const Vec2& world) override {
        if (!hits(_name->field(), world)) _name->finish();
        if (!hits(_codeField, world)) _codeField->detachWithIME();
        const int row = rowAt(world);
        if (row >= 0) select(row);
    }

    // ---- peers ----
    void peersChanged(const std::vector<Peer>& peers) {
        _peers.clear();
        for (const Peer& p : peers) {
            if (p.offers(kLevelsService)) _peers.push_back(p);
        }
        if (selectedIndex() < 0) _selectedId.clear();
        if (_peers.size() == 1 && _selectedId.empty() && _sendId == 0) _selectedId = _peers[0].id;   // the usual case
        rebuildRows();
        refresh();
    }

    int selectedIndex() const {
        for (size_t i = 0; i < _peers.size(); ++i) {
            if (_peers[i].id == _selectedId) return (int)i;
        }
        return -1;
    }

    void select(int index) {
        if (index < 0 || index >= (int)_peers.size() || _sendId != 0) return;
        _selectedId = _peers[index].id;
        const float top = index * (kRowH + kRowGap);
        if (top < _offset) setOffset(top);
        else if (top + kRowH > _offset + _rowsRect.size.height) setOffset(top + kRowH - _rowsRect.size.height);
        layoutRows();
        refresh();
    }

    void rebuildRows() {
        _rowsNode->removeAllChildren();
        _rows.clear();
        const float w = _rowsRect.size.width;
        for (const Peer& p : _peers) {
            Row r;
            r.node = Node::create();
            r.node->setContentSize(Size(w, kRowH));
            _rowsNode->addChild(r.node);
            auto* bg = oui::roundedRect(Size(w, kRowH), 28.0f, Color3B::WHITE, 235);
            bg->setAnchorPoint(Vec2::ZERO);
            r.node->addChild(bg);
            r.bg = bg;
            r.selectedBg = oui::frameSprite("menu_main_btn_blue_normal.png", Size(w, kRowH), Rect(0.25f, 0.25f, 0.5f, 0.5f));
            r.selectedBg->setAnchorPoint(Vec2::ZERO);
            r.node->addChild(r.selectedBg);
            r.icon = ui::deviceIcon(p.platform, 110.0f, Color4F(Color3B(70, 70, 82)));
            r.icon->setPosition(110.0f, kRowH * 0.5f);
            r.node->addChild(r.icon);
            r.iconSelected = ui::deviceIcon(p.platform, 110.0f, Color4F::WHITE);
            r.iconSelected->setPosition(110.0f, kRowH * 0.5f);
            r.node->addChild(r.iconSelected);
            r.name = ui::label("", oui::kFontHeading, 66.0f, oui::kInk);
            oui::setEllipsized(r.name, p.name, w - 280.0f);
            r.name->setPosition(210.0f, kRowH * 0.5f + 24.0f);
            r.node->addChild(r.name);
            std::string dev = p.device.empty() ? std::string() : p.device;
            dev += (dev.empty() ? "" : "  \xC2\xB7  ") + std::string(p.platform == "phone" ? "Phone" : "PC");
            r.device = ui::label("", oui::kFontBody, 40.0f, oui::kInkDim);
            oui::setEllipsized(r.device, dev, w - 280.0f);
            r.device->setPosition(212.0f, kRowH * 0.5f - 42.0f);
            r.node->addChild(r.device);
            _rows.push_back(r);
        }
        setOffset(_offset);
    }

    float maxOffset() const {
        const float total = _peers.size() * (kRowH + kRowGap) - kRowGap;
        return std::max(0.0f, total - _rowsRect.size.height);
    }

    void setOffset(float offset) {
        _offset = std::max(0.0f, std::min(offset, maxOffset()));
        layoutRows();
    }

    void layoutRows() {
        const int sel = selectedIndex();
        for (size_t i = 0; i < _rows.size(); ++i) {
            Row& r = _rows[i];
            const float yTop = _rowsRect.getMaxY() - (i * (kRowH + kRowGap) - _offset);
            r.node->setPosition(_rowsRect.origin.x, yTop - kRowH);
            const bool selected = (int)i == sel;
            r.selectedBg->setVisible(selected);
            r.bg->setVisible(!selected);
            r.bg->setOpacity((int)i == _hover ? 255 : 215);
            r.icon->setVisible(!selected);
            r.iconSelected->setVisible(selected);
            r.name->setColor(selected ? Color3B::WHITE : oui::kInk);
            r.device->setColor(selected ? kSelectedSub : oui::kInkDim);
        }
    }

    int rowAt(const Vec2& world) const {
        const Vec2 p = _panel->convertToNodeSpace(world);
        if (!_rowsRect.containsPoint(p)) return -1;
        const float fromTop = _rowsRect.getMaxY() - p.y + _offset;
        const int index = (int)std::floor(fromTop / (kRowH + kRowGap));
        if (fromTop - index * (kRowH + kRowGap) > kRowH) return -1;
        return (index >= 0 && index < (int)_peers.size()) ? index : -1;
    }

    // ---- sending ----
    void expandCode() {
        _codeLink->setVisible(false);
        _codeRow->setVisible(true);
        _codeField->focus();
        refresh();
    }

    void sendPressed() {
        if (_sendId != 0) {
            // Cancel
            LevelTransfer::getInstance()->cancelSend(_sendId);
            _sendId = 0;
            setStatus("Cancelled.", oui::kInkDim, false);
            refresh();
            return;
        }
        const int i = selectedIndex();
        if (i < 0) return;
        SendTarget target;
        target.address = _peers[i].address;
        target.port = _peers[i].port;
        target.name = _peers[i].name;
        start(target);
    }

    void sendToCode() {
        if (_sendId != 0) return;
        SendTarget target;
        if (!LevelTransfer::parseTarget(_codeField->text(), &target.address, &target.port)) {
            setStatus("That code doesn't look right. It looks like K7QM-2D9A.", kError, false);
            return;
        }
        _codeField->detachWithIME();
        start(target);
    }

    void start(const SendTarget& target) {
        _name->apply();
        _sendId = -1;   // the callback may report a final state inside send()
        const int id = LevelTransfer::getInstance()->send(
            target, _level, [this](LevelTransfer::SendState state, const std::string& message) {
                using S = LevelTransfer::SendState;
                switch (state) {
                case S::Connecting:
                case S::Waiting:
                case S::Sending: setStatus(message, oui::kInk, true); break;
                case S::Done:
                    _sendId = 0;
                    setStatus("Sent! " + message, kOk, false);
                    break;
                case S::Declined:
                    _sendId = 0;
                    setStatus(message, kError, false);
                    break;
                case S::Failed:
                    _sendId = 0;
                    setStatus(message, kError, false);
                    break;
                }
                refresh();
            });
        if (_sendId == -1) _sendId = id;
        refresh();
    }

    void setStatus(const std::string& text, const Color3B& color, bool busy) {
        oui::setEllipsized(_status, text, kW - 260.0f);
        _status->setColor(color);
        _statusSpinner->setVisible(busy);
        const float w = _status->getContentSize().width;
        const float x = kW * 0.5f + (busy ? 44.0f : 0.0f);
        _status->setPositionX(x);
        _statusSpinner->setPosition(x - w * 0.5f - 52.0f, _status->getPositionY());
    }

    void refresh() {
        const bool busy = _sendId != 0;
        _empty->setVisible(_peers.empty());
        if (busy) {
            _sendBtn->setStyle(oui::Button::chunky("grey"));
            _sendBtn->setText("Cancel");
            _sendBtn->setEnabled(true);
        } else {
            _sendBtn->setStyle(oui::Button::chunky("blue"));
            const int i = selectedIndex();
            _sendBtn->setText(i >= 0 ? "Send to " + _peers[i].name : "Send");
            if (_sendBtn->label()->getContentSize().width > 900.0f) _sendBtn->setText("Send");
            _sendBtn->setEnabled(i >= 0);
        }
        _codeBtn->setEnabled(!busy && !_codeField->text().empty());
        layoutRows();
    }

    LevelPackage _level;
    std::vector<Peer> _peers;
    std::vector<Row> _rows;
    std::string _selectedId;
    int _observer = 0;
    int _sendId = 0;
    int _hover = -1;
    float _offset = 0.0f;
    Rect _box, _rowsRect;
    Node* _rowsNode = nullptr;
    Node* _empty = nullptr;
    NameLine* _name = nullptr;
    Label* _status = nullptr;
    Sprite* _statusSpinner = nullptr;
    oui::Button* _sendBtn = nullptr;
    oui::Button* _codeLink = nullptr;
    Node* _codeRow = nullptr;
    oui::SearchField* _codeField = nullptr;
    oui::Button* _codeBtn = nullptr;
};

// ---- Receive Levels ----------------------------------------------------------------------------

class ReceivePanel : public ui::Modal {
public:
    static ReceivePanel* create() {
        ReceivePanel* p = new (std::nothrow) ReceivePanel();
        if (p && p->init()) {
            p->autorelease();
            return p;
        }
        delete p;
        return nullptr;
    }

private:
    static constexpr float kW = 1700.0f;

    bool init() {
        const float strip = oui::windowPanelStrip();
        const float h = 1400.0f + strip;
        if (!initModal(Size(kW, h), "Receive Levels")) return false;
        LevelTransfer* t = LevelTransfer::getInstance();
        t->start();
        const bool ready = t->isListening();

        float y = _contentTop - 200.0f;
        // Ready line with a green dot.
        Label* readyLabel = ui::label(ready ? "Ready \xE2\x80\x94 nearby players can send you levels"
                                            : "Receiving is off: ports 47811-47826 are in use",
                                      oui::kFontBodyBold, 48.0f, ready ? kOk : kError, Vec2(0.5f, 0.5f));
        readyLabel->setPosition(kW * 0.5f + 30.0f, y);
        _panel->addChild(readyLabel);
        auto* dot = oui::roundedRect(Size(34.0f, 34.0f), 17.0f, ready ? Color3B(70, 190, 100) : kError, 255);
        dot->setAnchorPoint(Vec2(0.5f, 0.5f));
        dot->setPosition(kW * 0.5f + 30.0f - readyLabel->getContentSize().width * 0.5f - 40.0f, y);
        _panel->addChild(dot);
        if (ready) dot->runAction(RepeatForever::create(Sequence::create(FadeTo::create(0.8f, 110), FadeTo::create(0.8f, 255), nullptr)));

        y -= 120.0f;
        _name = NameLine::create(560.0f);
        _name->setPosition(kW * 0.5f, y);
        _panel->addChild(_name);

        // The code card.
        y -= 250.0f;
        const Size card(1000.0f, 300.0f);
        auto* cardBg = oui::roundedRect(card, 40.0f, Color3B::WHITE, 255);
        cardBg->setAnchorPoint(Vec2(0.5f, 0.5f));
        cardBg->setPosition(kW * 0.5f, y);
        _panel->addChild(cardBg);
        Label* caption = ui::label("YOUR RECEIVE CODE", oui::kFontBodyBold, 36.0f, oui::kInkDim, Vec2(0.5f, 0.5f));
        caption->setPosition(kW * 0.5f, y + 98.0f);
        _panel->addChild(caption);
        Label* code = ui::label(ready ? t->receiveCode() : "\xE2\x80\x94", oui::kFontHeading, 150.0f, oui::kInk,
                                Vec2(0.5f, 0.5f));
        code->setPosition(kW * 0.5f, y - 22.0f);
        _panel->addChild(code);
        y -= card.height * 0.5f + 50.0f;
        if (ready) {
            Label* address = ui::label(t->receiveAddressText(), oui::kFontBody, 40.0f, oui::kInkDim, Vec2(0.5f, 0.5f));
            address->setPosition(kW * 0.5f, y);
            _panel->addChild(address);
        }

        y -= 70.0f;
        Label* help = Label::createWithTTF(
            "Your friend opens a level, taps Send to Nearby and picks you. If you are not in their list, they tap "
            "\xE2\x80\x9CNot listed? Use a code\xE2\x80\x9D and type this code. You are always asked before a level "
            "is saved.",
            oui::kFontBody, 44.0f, Size(kW - 320.0f, 0.0f), TextHAlignment::CENTER);
        help->setColor(oui::kInk);
        help->setLineSpacing(6.0f);
        help->setAnchorPoint(Vec2(0.5f, 1.0f));
        help->setPosition(kW * 0.5f, y);
        _panel->addChild(help);

        auto* done = oui::Button::create("Done", Size(640.0f, 180.0f), oui::Button::chunky("blue"), 88.0f);
        done->setPosition(kW * 0.5f, 150.0f);
        done->setCallback([this]() { dismiss(); });
        _panel->addChild(done);
        return true;
    }

    void onClosed() override { _name->finish(); }
    bool onKey(EventKeyboard::KeyCode key) override {
        if (key == EventKeyboard::KeyCode::KEY_ENTER || key == EventKeyboard::KeyCode::KEY_KP_ENTER) {
            dismiss();
            return true;
        }
        return false;
    }
    void onTouch(const Vec2& world) override {
        if (!hits(_name->field(), world)) _name->finish();
    }

    NameLine* _name = nullptr;
};

}  // namespace

void showSendToNearby(const LevelPackage& level) {
    if (SendPanel* p = SendPanel::create(level)) p->present();
}

void showReceiveLevels() {
    if (ReceivePanel* p = ReceivePanel::create()) p->present();
}

}  // namespace net
