// EDITOR (browser features, PC addition): see ItemPalette.h.
#include "ItemPalette.h"

#include <algorithm>
#include <cmath>

#include "EditorSettings.h"
#include "FlashEditor.h"
#include "InspectorWidgets.h"
#include "UIKitCompat.h"
#include "online/OnlineUi.h"

USING_NS_CC;
namespace oui = online::ui;

namespace flashed {

namespace {
float s_lastScroll = 0.0f;   // like the iOS panel, reopen where it was left
const float kPad = 46.0f;
const float kHeadH = 110.0f;
}  // namespace

ItemPalette* ItemPalette::create(const Size& size)
{
    ItemPalette* p = new (std::nothrow) ItemPalette();
    if (p && p->init(size))
    {
        p->autorelease();
        return p;
    }
    delete p;
    return nullptr;
}

bool ItemPalette::init(const Size& size)
{
    if (!Node::init()) return false;
    oui::loadAtlases();
    setContentSize(size);
    Sprite* panel = oui::windowPanel(size);
    panel->setAnchorPoint(Vec2::ZERO);
    addChild(panel);
    const float strip = oui::windowPanelStrip();
    Label* title = ui::makeHeading("Add items", 72.0f);
    title->setPosition(Vec2(kPad, size.height - strip - 72.0f));
    addChild(title, 1);
    oui::Button* close = oui::Button::create("", Size(110.0f, 110.0f), oui::Button::window("pink"));
    close->setIcon(oui::iconSprite("clear", 54.0f));
    close->setPosition(Vec2(size.width - kPad - 55.0f, size.height - strip - 72.0f));
    close->setCallback([this]() {
        RefPtr<ItemPalette> keep(this);
        if (onClose) onClose();
    });
    addChild(close, 2);
    const float top = size.height - strip - 150.0f;
    _viewport = Rect(kPad * 0.5f, kPad * 0.5f, size.width - kPad, top - kPad * 0.5f);
    auto* clip = ClippingRectangleNode::create(_viewport);
    addChild(clip, 1);
    _content = Node::create();
    clip->addChild(_content);
    build();
    _scroll = s_lastScroll;
    layout();

    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (!oui::isShown(this)) return false;
        const Vec2 p = convertToNodeSpace(t->getLocation());
        if (!Rect(Vec2::ZERO, _contentSize).containsPoint(p)) return false;
        _down = p;
        _dragged = false;
        return true;
    };
    touch->onTouchMoved = [this](Touch* t, Event*) {
        const Vec2 p = convertToNodeSpace(t->getLocation());
        if (!_dragged && p.distance(_down) > 24.0f) _dragged = true;
        if (_dragged)
        {
            _scroll += t->getDelta().y;
            layout();
        }
    };
    touch->onTouchEnded = [this](Touch* t, Event*) {
        if (_dragged) return;
        const Vec2 p = convertToNodeSpace(t->getLocation());
        if (!_viewport.containsPoint(p)) return;
        for (const Tile& tile : _tiles)
        {
            if (!tile.node->isVisible()) continue;
            const Vec2 local = tile.node->convertToNodeSpace(t->getLocation());
            if (Rect(Vec2::ZERO, tile.node->getContentSize()).containsPoint(local))
            {
                const int id = tile.id;
                RefPtr<ItemPalette> keep(this);
                Director::getInstance()->getScheduler()->performFunctionInCocosThread([keep, id]() {
                    if (keep->onPick) keep->onPick(id);
                });
                return;
            }
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, panel);
    auto mouse = EventListenerMouse::create();
    mouse->onMouseScroll = [this](EventMouse* e) {
        const Vec2 p = convertToNodeSpace(Vec2(e->getCursorX(), e->getCursorY()));
        if (!oui::isShown(this) || !Rect(Vec2::ZERO, _contentSize).containsPoint(p)) return;
        _scroll += e->getScrollY() * 130.0f;
        layout();
        e->stopPropagation();
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, panel);
    return true;
}

Node* ItemPalette::makeTile(int itemID, const Size& size)
{
    EditorSettings* settings = EditorSettings::getInstance();
    Node* tile = Node::create();
    tile->setContentSize(size);
    Sprite* bg = oui::roundedRect(size, 24.0f, Color3B::WHITE, 235);
    bg->setAnchorPoint(Vec2::ZERO);
    tile->addChild(bg);
    // Icon: the iOS cell icon, else the browser art / Flash tool icon.
    const float box = size.height - 110.0f;
    Sprite* icon = nullptr;
    const std::string key = settings->keyForLevelItem((unsigned int)itemID);
    float scale = 1.0f;
    const std::string frame = uikit::imageNamed(key.substr(0, key.find("Ref")) + "_cellIcon.png", &scale);
    if (!frame.empty()) icon = Sprite::createWithSpriteFrameName(frame);
    std::string iconName = settings->iconForLevelItem((unsigned int)itemID);
    if (!icon) icon = flashIconSprite(iconName);
    if (icon)
    {
        icon->setAnchorPoint(Vec2(0.5f, 0.5f));
        if (iconName.compare(0, 3, "ic_") == 0 && frame.empty()) icon->setColor(oui::kBlue);
        const Size s = icon->getContentSize();
        icon->setScale(std::min(box * 0.92f / std::max(1.0f, s.width), box * 0.92f / std::max(1.0f, s.height)));
        icon->setPosition(Vec2(size.width * 0.5f, size.height - 16.0f - box * 0.5f));
        tile->addChild(icon);
    }
    std::string name = settings->nameForLevelItem((unsigned int)itemID);
    // iOS names are upper case ("I-BEAM"), the browser ones lower case ("trash can", "TV").
    const bool hasLower = std::any_of(name.begin(), name.end(), [](char c) { return c >= 'a' && c <= 'z'; });
    const bool hasUpper = std::any_of(name.begin(), name.end(), [](char c) { return c >= 'A' && c <= 'Z'; });
    if (!(hasLower && hasUpper) && !(hasUpper && name.size() <= 3)) name = uikit::capitalizedString(uikit::lowercaseString(name));
    Label* label = Label::createWithTTF(name, oui::kFontBodyBold, 34.0f, Size(size.width - 24.0f, 84.0f),
                                        TextHAlignment::CENTER, TextVAlignment::CENTER);
    label->setTextColor(Color4B(oui::kInk));
    label->setPosition(Vec2(size.width * 0.5f, 52.0f));
    tile->addChild(label);
    return tile;
}

void ItemPalette::build()
{
    EditorSettings* settings = EditorSettings::getInstance();
    const auto& sections = settings->sectionedLevelItemIDs();
    const auto& names = settings->sectionNames();
    const int columns = 3;
    const float gap = 22.0f;
    const float tileW = (_viewport.size.width - gap * (columns - 1)) / columns;
    const Size tileSize(tileW, tileW * 0.92f);
    float y = 0.0f;  // distance from the content top
    for (size_t s = 0; s < sections.size(); ++s)
    {
        Label* head = ui::makeLabel(uikit::capitalizedString(uikit::lowercaseString(names[s])), 40.0f, true, oui::kInkDim);
        head->setAnchorPoint(Vec2(0.0f, 0.5f));
        _content->addChild(head);
        _items.push_back({head, 6.0f, y + kHeadH * 0.5f, true});
        y += kHeadH;
        const auto& ids = sections[s];
        for (size_t i = 0; i < ids.size(); ++i)
        {
            Node* tile = makeTile(ids[i], tileSize);
            const int col = (int)(i % columns);
            const int row = (int)(i / columns);
            _content->addChild(tile);
            _items.push_back({tile, col * (tileW + gap), y + row * (tileSize.height + gap), false});
            _tiles.push_back({tile, ids[i]});
        }
        const int rows = (int)((ids.size() + columns - 1) / columns);
        y += rows * (tileSize.height + gap) + 10.0f;
    }
    _contentHeight = y;
}

void ItemPalette::layout()
{
    const float maxScroll = std::max(0.0f, _contentHeight - _viewport.size.height);
    _scroll = std::max(0.0f, std::min(maxScroll, _scroll));
    s_lastScroll = _scroll;
    const float top = _viewport.getMaxY() + _scroll;
    for (const Item& it : _items)
    {
        const float h = it.heading ? 0.0f : it.node->getContentSize().height;
        const float y = top - it.top - h;
        it.node->setPosition(Vec2(_viewport.origin.x + it.x, y));
        it.node->setVisible(y + h > _viewport.origin.y - (it.heading ? 40.0f : 0.0f) &&
                            y < _viewport.getMaxY() + (it.heading ? 40.0f : 0.0f));
    }
}

}  // namespace flashed
