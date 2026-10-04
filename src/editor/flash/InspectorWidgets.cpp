// EDITOR (browser features, PC addition): see InspectorWidgets.h.
#include "InspectorWidgets.h"

#include <algorithm>
#include <cmath>

#include "online/OnlineUi.h"

USING_NS_CC;
namespace oui = online::ui;

namespace flashed {
namespace ui {

namespace {

ValueField* s_focused = nullptr;
Node* s_popup = nullptr;

bool hit(Node* node, const Vec2& world, const Size& size, const HitFilter& filter)
{
    if (!oui::isShown(node)) return false;
    if (filter && !filter(world)) return false;
    const Vec2 p = node->convertToNodeSpace(world);
    return Rect(0, 0, size.width, size.height).containsPoint(p);
}

std::string hex(int rgb)
{
    return rgb < 0 ? "none" : StringUtils::format("#%06X", rgb & 0xffffff);
}

}  // namespace

Label* makeLabel(const std::string& text, float size, bool bold, const Color3B& color)
{
    Label* l = Label::createWithTTF(text, bold ? oui::kFontBodyBold : oui::kFontBody, size);
    l->setTextColor(Color4B(color));
    l->setAnchorPoint(Vec2(0.0f, 0.5f));
    return l;
}

Label* makeHeading(const std::string& text, float size)
{
    Label* l = Label::createWithTTF(text, oui::kFontHeading, size);
    l->setTextColor(Color4B(oui::kInk));
    l->setAnchorPoint(Vec2(0.0f, 0.5f));
    return l;
}

bool textEditing() { return s_focused != nullptr; }

// ---- ValueField ------------------------------------------------------------------------------------

ValueField* ValueField::create(const Size& size, bool numeric, bool multiline)
{
    ValueField* f = new (std::nothrow) ValueField();
    if (f && f->init(size, numeric, multiline))
    {
        f->autorelease();
        return f;
    }
    delete f;
    return nullptr;
}

ValueField* ValueField::current() { return s_focused; }

bool ValueField::init(const Size& size, bool numeric, bool multiline)
{
    if (!Node::init()) return false;
    _numeric = numeric;
    _multiline = multiline;
    setContentSize(size);
    _bg = oui::roundedRect(size, 18.0f, Color3B::WHITE, 255);
    _bg->setAnchorPoint(Vec2::ZERO);
    addChild(_bg);
    _label = Label::createWithTTF("", oui::kFontBody, 40.0f);
    _label->setTextColor(Color4B(oui::kInk));
    _label->setAnchorPoint(numeric ? Vec2(1.0f, 0.5f) : Vec2(0.0f, 0.5f));
    _label->setPosition(numeric ? Vec2(size.width - 24.0f, size.height * 0.5f) : Vec2(24.0f, size.height * 0.5f));
    _label->setDimensions(0, 0);
    addChild(_label, 1);
    _caret = DrawNode::create();
    addChild(_caret, 2);
    _selection = DrawNode::create();
    addChild(_selection, 0);

    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (!hit(this, t->getLocation(), _contentSize, hitFilter))
        {
            if (_focused) unfocus(true);   // a click elsewhere commits (and is not swallowed)
            return false;
        }
        return true;
    };
    touch->onTouchEnded = [this](Touch* t, Event*) {
        if (hit(this, t->getLocation(), _contentSize, hitFilter) && !_focused) focus();
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);

    auto keys = EventListenerKeyboard::create();
    keys->onKeyPressed = [this](EventKeyboard::KeyCode k, Event* e) {
        if (k == EventKeyboard::KeyCode::KEY_SHIFT || k == EventKeyboard::KeyCode::KEY_LEFT_SHIFT ||
            k == EventKeyboard::KeyCode::KEY_RIGHT_SHIFT)
            _shift = true;
        if (!_focused) return;
        if (k == EventKeyboard::KeyCode::KEY_ENTER || k == EventKeyboard::KeyCode::KEY_KP_ENTER)
        {
            if (_multiline && _shift)
            {
                _text += "\n";
                refresh();
            }
            else
                unfocus(true);
            e->stopPropagation();
        }
    };
    keys->onKeyReleased = [this](EventKeyboard::KeyCode k, Event*) {
        if (k == EventKeyboard::KeyCode::KEY_SHIFT || k == EventKeyboard::KeyCode::KEY_LEFT_SHIFT ||
            k == EventKeyboard::KeyCode::KEY_RIGHT_SHIFT)
            _shift = false;
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keys, this);
    schedule([this](float) {
        _caret->setVisible(_focused && std::fmod(utils::gettime(), 1.0) < 0.6);
    }, 0.1f, "caret");
    refresh();
    return true;
}

void ValueField::onExit()
{
    if (_focused) unfocus(true);
    Node::onExit();
}

void ValueField::setText(const std::string& text)
{
    if (_focused) return;  // don't overwrite what the user is typing
    _text = text;
    refresh();
}

void ValueField::refresh()
{
    std::string shown = _text;
    std::replace(shown.begin(), shown.end(), '\n', ' ');
    _label->setString(shown);
    // keep the end visible
    const float maxW = _contentSize.width - 48.0f;
    if (_label->getContentSize().width > maxW && !_numeric)
    {
        _label->setAnchorPoint(Vec2(1.0f, 0.5f));
        _label->setPositionX(_contentSize.width - 24.0f);
    }
    else if (!_numeric)
    {
        _label->setAnchorPoint(Vec2(0.0f, 0.5f));
        _label->setPositionX(24.0f);
    }
    _caret->clear();
    const float x = _numeric || _label->getAnchorPoint().x > 0.5f ? _contentSize.width - 18.0f
                                                                  : 24.0f + _label->getContentSize().width + 4.0f;
    _caret->drawSolidRect(Vec2(x, _contentSize.height * 0.2f), Vec2(x + 4.0f, _contentSize.height * 0.8f),
                          Color4F(oui::kBlue));
    _bg->setColor(_focused ? Color3B(255, 255, 240) : Color3B::WHITE);
    // Focused with everything selected (like a desktop text field): typing replaces it.
    _selection->clear();
    if (_focused && _replaceAll && !_text.empty())
    {
        const Rect box = _label->getBoundingBox();
        _selection->drawSolidRect(Vec2(box.getMinX() - 4.0f, _contentSize.height * 0.18f),
                                  Vec2(box.getMaxX() + 4.0f, _contentSize.height * 0.82f),
                                  Color4F(0.24f, 0.55f, 0.78f, 0.28f));
    }
}

void ValueField::focus()
{
    if (_focused) return;
    if (s_focused && s_focused != this) s_focused->unfocus(true);
    _original = _text;
    attachWithIME();
    _focused = true;
    _replaceAll = true;
    s_focused = this;
    refresh();
}

void ValueField::unfocus(bool commit)
{
    if (!_focused) return;
    _focused = false;
    if (s_focused == this) s_focused = nullptr;
    detachWithIME();
    if (!commit) _text = _original;
    refresh();
    if (commit && _text != _original && onCommit)
    {
        RefPtr<ValueField> keep(this);
        onCommit(_text);
    }
}

void ValueField::didAttachWithIME() {}
void ValueField::didDetachWithIME()
{
    if (_focused) unfocus(true);
}

void ValueField::insertText(const char* text, size_t len)
{
    if (!_focused) return;
    std::string add(text, len);
    if (add == "\n" || add == "\r")
    {
        unfocus(true);
        return;
    }
    if (_numeric)
    {
        std::string filtered;
        for (char c : add)
            if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '#' || (c >= 'a' && c <= 'f') ||
                (c >= 'A' && c <= 'F') || c == 'x')
                filtered += c;
        add = filtered;
    }
    if (_replaceAll)
    {
        _text.clear();
        _replaceAll = false;
    }
    if (_text.size() + add.size() > 200) return;
    _text += add;
    refresh();
}

void ValueField::deleteBackward()
{
    if (!_focused || _text.empty()) return;
    if (_replaceAll)
    {
        _replaceAll = false;
        _text.clear();
        refresh();
        return;
    }
    // drop one UTF-8 code point
    size_t n = _text.size() - 1;
    while (n > 0 && ((unsigned char)_text[n] & 0xC0) == 0x80) --n;
    _text.erase(n);
    refresh();
}

void ValueField::controlKey(EventKeyboard::KeyCode keyCode)
{
    if (keyCode == EventKeyboard::KeyCode::KEY_ESCAPE) unfocus(false);
    if ((keyCode == EventKeyboard::KeyCode::KEY_LEFT_ARROW || keyCode == EventKeyboard::KeyCode::KEY_RIGHT_ARROW ||
         keyCode == EventKeyboard::KeyCode::KEY_END) && _replaceAll)
    {
        _replaceAll = false;  // keep the text, type after it
        refresh();
    }
}

// ---- SliderBar ------------------------------------------------------------------------------------------

SliderBar* SliderBar::create(float width)
{
    SliderBar* s = new (std::nothrow) SliderBar();
    if (s && s->init(width))
    {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool SliderBar::init(float width)
{
    if (!Node::init()) return false;
    setContentSize(Size(width, 70.0f));
    _draw = DrawNode::create();
    addChild(_draw);
    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (!hit(this, t->getLocation(), _contentSize, hitFilter)) return false;
        _dragging = true;
        _start = _value;
        const float v = valueAt(convertToNodeSpace(t->getLocation()).x);
        if (v != _value)
        {
            _value = v;
            redraw();
            if (onChange) onChange(_value, false, _start);
        }
        return true;
    };
    touch->onTouchMoved = [this](Touch* t, Event*) {
        const float v = valueAt(convertToNodeSpace(t->getLocation()).x);
        if (v == _value) return;
        _value = v;
        redraw();
        if (onChange) onChange(_value, false, _start);
    };
    auto end = [this](Touch*, Event*) {
        _dragging = false;
        redraw();
        if (onChange && _value != _start) onChange(_value, true, _start);
    };
    touch->onTouchEnded = end;
    touch->onTouchCancelled = end;
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
    redraw();
    return true;
}

void SliderBar::setRange(float min, float max, int segments)
{
    _min = min;
    _max = max > min ? max : min + 1.0f;
    _segments = segments;
    redraw();
}

void SliderBar::setValue(float value)
{
    if (_dragging) return;
    _value = std::max(_min, std::min(_max, value));
    redraw();
}

float SliderBar::valueAt(float x) const
{
    const float pad = 30.0f;
    float t = (x - pad) / std::max(1.0f, _contentSize.width - pad * 2.0f);
    t = std::max(0.0f, std::min(1.0f, t));
    if (_segments > 0) t = std::round(t * _segments) / _segments;
    float v = _min + t * (_max - _min);
    if (_segments > 0 && std::fabs((_max - _min) - (float)_segments) < 1e-3f) v = std::round(v);
    return v;
}

void SliderBar::redraw()
{
    _draw->clear();
    const float pad = 30.0f, y = _contentSize.height * 0.5f;
    const float w = _contentSize.width - pad * 2.0f;
    const float t = (_value - _min) / (_max - _min);
    const float kx = pad + w * std::max(0.0f, std::min(1.0f, t));
    _draw->drawSegment(Vec2(pad, y), Vec2(pad + w, y), 7.0f, Color4F(0.62f, 0.62f, 0.66f, 1.0f));
    _draw->drawSegment(Vec2(pad, y), Vec2(kx, y), 7.0f, Color4F(oui::kBlue));
    _draw->drawDot(Vec2(kx, y - 2.0f), 27.0f, Color4F(0, 0, 0, 0.18f));
    _draw->drawDot(Vec2(kx, y), 25.0f, Color4F::WHITE);
    _draw->drawDot(Vec2(kx, y), _dragging ? 12.0f : 9.0f, Color4F(oui::kBlue));
}

// ---- Toggle -------------------------------------------------------------------------------------------------

Toggle* Toggle::create()
{
    Toggle* t = new (std::nothrow) Toggle();
    if (t && t->init())
    {
        t->autorelease();
        return t;
    }
    delete t;
    return nullptr;
}

bool Toggle::init()
{
    if (!Node::init()) return false;
    setContentSize(Size(140.0f, 76.0f));
    _draw = DrawNode::create();
    addChild(_draw);
    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) { return hit(this, t->getLocation(), _contentSize, hitFilter); };
    touch->onTouchEnded = [this](Touch* t, Event*) {
        if (!hit(this, t->getLocation(), _contentSize, nullptr)) return;
        setOn(!_on);
        if (onChange) onChange(_on);
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
    redraw();
    return true;
}

void Toggle::setOn(bool on)
{
    _on = on;
    redraw();
}

void Toggle::redraw()
{
    _draw->clear();
    const float h = _contentSize.height, w = _contentSize.width, r = h * 0.5f;
    const Color4F c = _on ? Color4F(oui::kBlue) : Color4F(0.62f, 0.62f, 0.66f, 1.0f);
    _draw->drawDot(Vec2(r, r), r, c);
    _draw->drawDot(Vec2(w - r, r), r, c);
    _draw->drawSolidRect(Vec2(r, 0), Vec2(w - r, h), c);
    _draw->drawDot(Vec2(_on ? w - r : r, r), r - 7.0f, Color4F::WHITE);
}

// ---- Swatch ---------------------------------------------------------------------------------------------------

Swatch* Swatch::create(const Size& size)
{
    Swatch* s = new (std::nothrow) Swatch();
    if (s && s->init(size))
    {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool Swatch::init(const Size& size)
{
    if (!Node::init()) return false;
    setContentSize(size);
    Sprite* bg = oui::roundedRect(size, 18.0f, Color3B::WHITE, 255);
    bg->setAnchorPoint(Vec2::ZERO);
    addChild(bg);
    _draw = DrawNode::create();
    addChild(_draw, 1);
    _label = Label::createWithTTF("", oui::kFontBody, 38.0f);
    _label->setTextColor(Color4B(oui::kInk));
    _label->setAnchorPoint(Vec2(1.0f, 0.5f));
    _label->setPosition(Vec2(size.width - 24.0f, size.height * 0.5f));
    addChild(_label, 1);
    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) { return hit(this, t->getLocation(), _contentSize, hitFilter); };
    touch->onTouchEnded = [this](Touch* t, Event*) {
        if (hit(this, t->getLocation(), _contentSize, nullptr) && onTap) onTap();
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
    setColor(0);
    return true;
}

void Swatch::setColor(int rgb)
{
    _rgb = rgb;
    _draw->clear();
    const float h = _contentSize.height;
    const Vec2 a(14.0f, 12.0f), b(14.0f + (h - 24.0f) * 1.6f, h - 12.0f);
    if (rgb >= 0)
        _draw->drawSolidRect(a, b, Color4F(((rgb >> 16) & 0xff) / 255.0f, ((rgb >> 8) & 0xff) / 255.0f, (rgb & 0xff) / 255.0f, 1.0f));
    _draw->drawRect(a, b, Color4F(0.3f, 0.3f, 0.35f, 1.0f));
    if (rgb < 0) _draw->drawSegment(Vec2(a.x, a.y), Vec2(b.x, b.y), 3.0f, Color4F(0.85f, 0.2f, 0.2f, 1.0f));
    _label->setString(hex(rgb));
}

// ---- popups ---------------------------------------------------------------------------------------------------

bool popupOpen() { return s_popup != nullptr; }

void closePopups()
{
    if (s_focused) s_focused->unfocus(false);
    if (s_popup)
    {
        Node* p = s_popup;
        s_popup = nullptr;
        p->removeFromParent();
    }
}

namespace {

// A modal layer with a window panel of `size` centred; returns the panel (content goes on it).
Node* openPanel(const std::string& title, const Size& size, Node** layerOut)
{
    closePopups();
    Scene* scene = Director::getInstance()->getRunningScene();
    if (!scene) return nullptr;
    const Size vs = Director::getInstance()->getVisibleSize();
    const Vec2 vo = Director::getInstance()->getVisibleOrigin();
    Node* layer = Node::create();
    layer->setContentSize(vs);
    scene->addChild(layer, 200000);
    s_popup = layer;
    LayerColor* dim = LayerColor::create(Color4B(0, 0, 0, 110), vs.width, vs.height);
    dim->setPosition(vo);
    layer->addChild(dim);
    Sprite* panel = oui::windowPanel(size);
    panel->setAnchorPoint(Vec2::ZERO);
    panel->setPosition(vo.x + (vs.width - size.width) * 0.5f, vo.y + (vs.height - size.height) * 0.5f);
    layer->addChild(panel);
    Label* heading = makeHeading(title, 64.0f);
    heading->setPosition(Vec2(60.0f, size.height - oui::windowPanelStrip() - 70.0f));
    panel->addChild(heading);
    const Rect panelRect(panel->getPosition(), size);
    auto catcher = EventListenerTouchOneByOne::create();
    catcher->setSwallowTouches(true);
    catcher->onTouchBegan = [](Touch*, Event*) { return true; };
    catcher->onTouchEnded = [panelRect](Touch* t, Event*) {
        if (!panelRect.containsPoint(t->getLocation()))
            Director::getInstance()->getScheduler()->performFunctionInCocosThread([]() { closePopups(); });
    };
    layer->getEventDispatcher()->addEventListenerWithSceneGraphPriority(catcher, dim);
    auto keys = EventListenerKeyboard::create();
    keys->onKeyPressed = [](EventKeyboard::KeyCode k, Event* e) {
        if (k == EventKeyboard::KeyCode::KEY_ESCAPE && !textEditing())
        {
            Director::getInstance()->getScheduler()->performFunctionInCocosThread([]() { closePopups(); });
            e->stopPropagation();
        }
    };
    layer->getEventDispatcher()->addEventListenerWithSceneGraphPriority(keys, dim);
    *layerOut = layer;
    return panel;
}

}  // namespace

void openListPicker(const std::string& title, const std::vector<std::string>& items, int selected,
                    std::function<void(int)> onPick, bool searchable)
{
    const Size vs = Director::getInstance()->getVisibleSize();
    // As tall as the list needs (title, optional filter, rows), up to most of the screen.
    size_t count = 0;
    for (const std::string& i : items)
        if (!i.empty()) ++count;
    const float needed = oui::windowPanelStrip() + 140.0f + (searchable ? 140.0f : 0.0f) + 92.0f * count + 60.0f;
    const Size size(1100.0f, std::min(std::min(vs.height - 160.0f, 1700.0f), needed));
    Node* layer = nullptr;
    Node* panel = openPanel(title, size, &layer);
    if (!panel) return;
    const float strip = oui::windowPanelStrip();
    float top = size.height - strip - 140.0f;
    oui::SearchField* field = nullptr;
    if (searchable)
    {
        field = oui::SearchField::create(Size(size.width - 120.0f, 110.0f), "Filter...");
        field->setPosition(Vec2(size.width * 0.5f, top - 60.0f));  // SearchField is centre-anchored
        panel->addChild(field);
        top -= 140.0f;
    }
    const Rect listRect(50.0f, 50.0f, size.width - 100.0f, top - 50.0f);
    auto* clip = ClippingRectangleNode::create(listRect);
    panel->addChild(clip);
    Node* list = Node::create();
    clip->addChild(list);

    struct State
    {
        std::vector<int> visible;
        float offset = 0.0f;
        float contentH = 0.0f;
        Vec2 down;
        bool dragged = false;
    };
    auto state = std::make_shared<State>();
    const float rowH = 92.0f;
    auto rebuild = [=]() {
        list->removeAllChildren();
        state->visible.clear();
        std::string filter = field ? field->text() : "";
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (items[i].empty()) continue;
            std::string lower = items[i];
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (!filter.empty() && lower.find(filter) == std::string::npos) continue;
            state->visible.push_back((int)i);
        }
        state->contentH = rowH * state->visible.size();
        state->offset = 0.0f;
        for (size_t r = 0; r < state->visible.size(); ++r)
        {
            const int i = state->visible[r];
            const float y = listRect.getMaxY() - rowH * (r + 0.5f);
            if (i == selected)
            {
                auto* bg = oui::roundedRect(Size(listRect.size.width, rowH - 8.0f), 16.0f, oui::kBlue, 255);
                bg->setPosition(Vec2(listRect.getMidX(), y));
                list->addChild(bg);
            }
            Label* l = makeLabel(items[i], 42.0f, i == selected, i == selected ? Color3B::WHITE : oui::kInk);
            l->setPosition(Vec2(listRect.origin.x + 30.0f, y));
            list->addChild(l);
        }
        // start with the selection in view
        auto it = std::find(state->visible.begin(), state->visible.end(), selected);
        if (it != state->visible.end())
        {
            const float rowTop = rowH * (it - state->visible.begin());
            state->offset = std::max(0.0f, std::min(rowTop - listRect.size.height * 0.4f,
                                                    state->contentH - listRect.size.height));
            state->offset = std::max(0.0f, state->offset);
        }
        list->setPositionY(state->offset);
    };
    rebuild();
    if (field) field->onChange = rebuild;

    auto scrollBy = [=](float dy) {
        const float maxOff = std::max(0.0f, state->contentH - listRect.size.height);
        state->offset = std::max(0.0f, std::min(maxOff, state->offset + dy));
        list->setPositionY(state->offset);
    };
    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [=](Touch* t, Event*) {
        const Vec2 p = panel->convertToNodeSpace(t->getLocation());
        if (!listRect.containsPoint(p)) return false;
        state->down = p;
        state->dragged = false;
        return true;
    };
    touch->onTouchMoved = [=](Touch* t, Event*) {
        const Vec2 p = panel->convertToNodeSpace(t->getLocation());
        if (std::fabs(p.y - state->down.y) > 20.0f) state->dragged = true;
        if (state->dragged) scrollBy(t->getDelta().y);
    };
    touch->onTouchEnded = [=](Touch* t, Event*) {
        if (state->dragged) return;
        const Vec2 p = panel->convertToNodeSpace(t->getLocation());
        const int r = (int)std::floor((listRect.getMaxY() - p.y + state->offset) / rowH);
        if (r < 0 || r >= (int)state->visible.size()) return;
        const int index = state->visible[r];
        auto pick = onPick;
        Director::getInstance()->getScheduler()->performFunctionInCocosThread([pick, index]() {
            closePopups();
            if (pick) pick(index);
        });
    };
    panel->getEventDispatcher()->addEventListenerWithSceneGraphPriority(touch, list);
    auto mouse = EventListenerMouse::create();
    mouse->onMouseScroll = [=](EventMouse* e) { scrollBy(e->getScrollY() * 90.0f); };
    panel->getEventDispatcher()->addEventListenerWithSceneGraphPriority(mouse, list);
    if (field) field->focus();
}

void openColorPicker(const std::string& title, int rgb, bool allowNone, std::function<void(int)> onPick)
{
    // A palette in the spirit of Flash's ColorSelector swatches.
    static const int kPalette[] = {
        0x000000, 0x333333, 0x666666, 0x999999, 0xcccccc, 0xffffff, 0x3d88c7, 0x5ab4e6,
        0x990000, 0xcc0000, 0xff3333, 0xff9999, 0xff6600, 0xff9933, 0xffcc00, 0xffff66,
        0x006600, 0x339933, 0x66cc33, 0x99ff99, 0x003366, 0x0066cc, 0x3399ff, 0x99ccff,
        0x330066, 0x663399, 0x9966cc, 0xcc99ff, 0x663300, 0x996633, 0xcc9966, 0xffcc99,
    };
    const Size size(1180.0f, allowNone ? 1020.0f : 900.0f);
    Node* layer = nullptr;
    Node* panel = openPanel(title, size, &layer);
    if (!panel) return;
    auto pick = [onPick](int c) {
        Director::getInstance()->getScheduler()->performFunctionInCocosThread([onPick, c]() {
            closePopups();
            if (onPick) onPick(c);
        });
    };
    const float strip = oui::windowPanelStrip();
    const float cell = 128.0f, gap = 4.0f;
    const float x0 = 60.0f, y0 = size.height - strip - 160.0f;
    for (int i = 0; i < 32; ++i)
    {
        const int c = kPalette[i];
        oui::Button::Style st;
        st.color = Color3B((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
        st.radius = 14.0f;
        oui::Button* b = oui::Button::create("", Size(cell - gap, cell - gap), st);
        b->setPosition(Vec2(x0 + (i % 8) * cell + cell * 0.5f, y0 - (i / 8) * cell - cell * 0.5f));
        b->setCallback([pick, c]() { pick(c); });
        panel->addChild(b);
        if (c == rgb)
        {
            auto* ring = DrawNode::create();
            ring->drawRect(Vec2(-cell * 0.5f, -cell * 0.5f), Vec2(cell * 0.5f, cell * 0.5f), Color4F(oui::kBlue));
            ring->setPosition(b->getPosition());
            panel->addChild(ring, 2);
        }
    }
    float y = y0 - 4 * cell - 80.0f;
    Label* hexLabel = makeLabel("hex", 40.0f, true, oui::kInkDim);
    hexLabel->setPosition(Vec2(x0, y));
    panel->addChild(hexLabel);
    ValueField* field = ValueField::create(Size(420.0f, 96.0f), false);
    field->setPosition(Vec2(x0 + 140.0f, y - 48.0f));
    field->setText(rgb >= 0 ? StringUtils::format("%06X", rgb & 0xffffff) : "");
    field->onCommit = [pick](const std::string& text) {
        std::string t = text;
        t.erase(std::remove(t.begin(), t.end(), '#'), t.end());
        if (t.empty()) return;
        pick((int)(std::strtol(t.c_str(), nullptr, 16) & 0xffffff));
    };
    panel->addChild(field);
    oui::Button* ok = oui::Button::create("OK", Size(260.0f, 110.0f), oui::Button::window("blue"), 52.0f);
    ok->setPosition(Vec2(size.width - 60.0f - 130.0f, y));
    ok->setCallback([field, pick, rgb]() {
        std::string t = field->text();
        t.erase(std::remove(t.begin(), t.end(), '#'), t.end());
        pick(t.empty() ? rgb : (int)(std::strtol(t.c_str(), nullptr, 16) & 0xffffff));
    });
    panel->addChild(ok);
    if (allowNone)
    {
        oui::Button* none = oui::Button::create("No outline", Size(420.0f, 100.0f), oui::Button::window("pink"), 46.0f);
        none->setPosition(Vec2(x0 + 210.0f, y - 130.0f));
        none->setCallback([pick]() { pick(-1); });
        panel->addChild(none);
    }
}

}  // namespace ui
}  // namespace flashed
