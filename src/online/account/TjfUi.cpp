// ONLINE (PC addition): see TjfUi.h.
#include "online/account/TjfUi.h"

#include <algorithm>
#include <cmath>

#include "HWWindow.h"
#include "HWWindowDelegate.h"
#include "Settings.h"
#include "platform/CCImage.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
#include "platform/desktop/CCGLViewImpl-desktop.h"
#endif

USING_NS_CC;

namespace online {
namespace tjfui {

namespace {

// Panels sit under Dropdown menus (z 5000: a dropdown opened in a panel shows above it) and
// HWWindow alerts (globals::ui::alertWindowDepth 10000).
const int kPanelZ = 4500;

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

Texture2D* generate(const std::string& key, int n, const std::function<bool(float, float)>& inside) {
    TextureCache* cache = Director::getInstance()->getTextureCache();
    if (Texture2D* t = cache->getTextureForKey(key)) return t;
    std::vector<unsigned char> px((size_t)n * n * 4);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            int hits = 0;
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx) {
                    const float fx = x + (sx + 0.5f) / 4.0f, fy = y + (sy + 0.5f) / 4.0f;
                    if (inside(fx / n * 2.0f - 1.0f, fy / n * 2.0f - 1.0f)) ++hits;
                }
            const unsigned char a = (unsigned char)std::lround(clamp01(hits / 16.0f) * 255.0f);
            unsigned char* p = &px[((size_t)y * n + x) * 4];
            p[0] = p[1] = p[2] = p[3] = a;
        }
    }
    Image* image = new (std::nothrow) Image();
    image->initWithRawData(px.data(), (ssize_t)px.size(), n, n, 8, true);
    Texture2D* t = cache->addImage(image, key);
    image->release();
    if (t) {
        t->generateMipmap();
        Texture2D::TexParams params = {GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
        t->setTexParameters(params);
    }
    return t;
}

float seg(float px, float py, float ax, float ay, float bx, float by) {
    const float dx = bx - ax, dy = by - ay;
    float t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy);
    t = std::max(0.0f, std::min(1.0f, t));
    const float ex = px - (ax + t * dx), ey = py - (ay + t * dy);
    return std::sqrt(ex * ex + ey * ey);
}

bool heart(float u, float v, float scale) {
    // (x^2 + y^2 - 1)^3 - x^2 y^3 <= 0, y up.
    const float x = u * 1.22f / scale, y = -v * 1.22f / scale + 0.18f;
    const float a = x * x + y * y - 1.0f;
    return a * a * a - x * x * y * y * y <= 0.0f;
}

bool triangleRight(float u, float v, float cx, float cy, float s) {
    // play triangle pointing right, centred at (cx, cy)
    const float x = u - cx, y = v - cy;
    return x > -s * 0.55f && std::fabs(y) < (s * 0.62f - (x + s * 0.55f) * 0.55f);
}

}  // namespace

Sprite* icon(const std::string& name, float size) {
    const int n = 128;
    Texture2D* t = nullptr;
    if (name == "heart") {
        t = generate("tjf_icon_heart", n, [](float u, float v) { return heart(u, v, 1.0f); });
    } else if (name == "heart_outline") {
        t = generate("tjf_icon_heart_outline", n,
                     [](float u, float v) { return heart(u, v, 1.0f) && !heart(u, v + 0.03f, 0.74f); });
    } else if (name == "user") {
        t = generate("tjf_icon_user", n, [](float u, float v) {
            const float hx = u, hy = v + 0.36f;
            if (hx * hx + hy * hy < 0.34f * 0.34f) return true;              // head
            const float bx = u / 0.72f, by = (v - 0.86f) / 0.62f;          // shoulders
            return v < 0.9f && v > 0.12f && bx * bx + by * by < 1.0f;
        });
    } else if (name == "replay") {
        t = generate("tjf_icon_replay", n, [](float u, float v) {
            const float d = std::sqrt(u * u + v * v);
            if (std::fabs(d - 0.8f) < 0.12f) return true;
            return triangleRight(u, v, 0.08f, 0.0f, 0.62f);
        });
    } else if (name == "upload" || name == "save") {
        const bool up = name == "upload";
        t = generate(up ? "tjf_icon_upload" : "tjf_icon_save", n, [up](float u, float v) {
            // arrow (up or down) over a tray
            if (seg(u, v, -0.8f, 0.82f, 0.8f, 0.82f) < 0.11f) return true;
            if (seg(u, v, -0.8f, 0.82f, -0.8f, 0.45f) < 0.11f || seg(u, v, 0.8f, 0.82f, 0.8f, 0.45f) < 0.11f) return true;
            const float tip = up ? -0.85f : 0.45f, tail = up ? 0.45f : -0.85f;
            if (seg(u, v, 0.0f, tip, 0.0f, tail) < 0.12f) return true;
            const float k = up ? 0.42f : -0.42f;
            return seg(u, v, 0.0f, tip, -0.42f, tip + k) < 0.12f || seg(u, v, 0.0f, tip, 0.42f, tip + k) < 0.12f;
        });
    } else if (name == "check") {
        t = generate("tjf_icon_check", n, [](float u, float v) {
            return seg(u, v, -0.7f, 0.0f, -0.2f, 0.5f) < 0.16f || seg(u, v, -0.2f, 0.5f, 0.75f, -0.55f) < 0.16f;
        });
    } else if (name == "trash") {
        t = generate("tjf_icon_trash", n, [](float u, float v) {
            if (seg(u, v, -0.75f, -0.6f, 0.75f, -0.6f) < 0.1f) return true;                 // lid
            if (seg(u, v, -0.22f, -0.78f, 0.22f, -0.78f) < 0.09f) return true;              // handle
            const bool body = v > -0.42f && v < 0.88f && std::fabs(u) < 0.58f - (v + 0.42f) * 0.08f;
            const bool slot = std::fabs(u) < 0.07f || std::fabs(std::fabs(u) - 0.27f) < 0.06f;
            return body && !(slot && v > -0.25f && v < 0.72f);
        });
    } else if (name == "trophy") {
        t = generate("tjf_icon_trophy", n, [](float u, float v) {
            const float cu = u / 0.62f, cv = (v + 0.55f) / 0.95f;   // cup: lower half of an ellipse
            if (v > -0.75f && v < 0.15f && cu * cu + cv * cv < 1.0f) return true;
            if (std::fabs(u) < 0.1f && v >= 0.1f && v < 0.6f) return true;               // stem
            if (std::fabs(u) < 0.48f && v >= 0.6f && v < 0.82f) return true;            // base
            const float d1 = std::sqrt((u + 0.62f) * (u + 0.62f) + (v + 0.4f) * (v + 0.4f));
            const float d2 = std::sqrt((u - 0.62f) * (u - 0.62f) + (v + 0.4f) * (v + 0.4f));
            return (std::fabs(d1 - 0.28f) < 0.08f && u < -0.5f) || (std::fabs(d2 - 0.28f) < 0.08f && u > 0.5f);
        });
    }
    if (!t) return ui::iconSprite(name, size);
    Sprite* s = Sprite::createWithTexture(t);
    s->setScale(size / t->getContentSize().height);
    return s;
}

Label* label(const std::string& text, const std::string& font, float size, const Color3B& color, const Vec2& anchor) {
    Label* l = Label::createWithTTF(text, font, size);
    l->setColor(color);
    l->setAnchorPoint(anchor);
    return l;
}

Label* textBlock(const std::string& text, const std::string& font, float size, const Color3B& color, float width,
                 TextHAlignment align) {
    Label* l = Label::createWithTTF(text, font, size, Size(width, 0.0f), align);
    l->setColor(color);
    l->setAnchorPoint(Vec2(0.0f, 1.0f));
    l->setLineSpacing(6.0f);
    return l;
}

// ---- Panel --------------------------------------------------------------------------------------

bool Panel::initPanel(const Size& size, const std::string& title) {
    if (!Node::init()) return false;
    ui::loadAtlases();
    setName(ui::kModalNodeName);
    const Size vs = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();
    _dim = LayerColor::create(Color4B(0, 0, 0, 190));
    _dim->setContentSize(vs);
    _dim->setPosition(origin);
    addChild(_dim, 0);

    _size = Size(std::min(size.width, vs.width - 120.0f), std::min(size.height, vs.height - 80.0f));
    _content = Node::create();
    _content->setContentSize(_size);
    _content->setAnchorPoint(Vec2(0.5f, 0.5f));
    _content->setPosition(origin.x + vs.width * 0.5f, origin.y + vs.height * 0.5f);
    _content->setCascadeOpacityEnabled(true);
    addChild(_content, 1);
    Sprite* frame = ui::windowPanel(_size);
    frame->setAnchorPoint(Vec2::ZERO);
    _content->addChild(frame, -1);
    const float strip = ui::windowPanelStrip();
    // HWWindow header: Clarendon, white, in the strip's lower part.
    _title = label(title, ui::kFontHeading, 96.0f, Color3B::WHITE, Vec2(0.5f, 0.5f));
    _title->enableShadow(Color4B(0, 0, 0, 70), Size(0.0f, -6.0f));
    _title->setPosition(_size.width * 0.5f, _size.height - strip - 80.0f);
    _content->addChild(_title, 2);
    _top = _size.height - strip - 160.0f;

    auto* close = MenuItemImage::create();
    close->setNormalImage(Sprite::createWithSpriteFrameName("window_btn_close.png"));
    Sprite* down = Sprite::createWithSpriteFrameName("window_btn_close.png");
    down->setColor(Color3B(190, 190, 190));
    close->setSelectedImage(down);
    close->setCallback([this](Ref*) { dismiss(); });
    close->setPosition(_size.width - 30.0f, _size.height - 34.0f);
    Menu* menu = Menu::create(close, nullptr);
    menu->setPosition(Vec2::ZERO);
    _content->addChild(menu, 10);

    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [](Touch*, Event*) { return true; };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);

    auto keys = EventListenerKeyboard::create();
    keys->onKeyReleased = [this](EventKeyboard::KeyCode key, Event*) {
        using K = EventKeyboard::KeyCode;
        if (key == K::KEY_CTRL || key == K::KEY_LEFT_CTRL || key == K::KEY_RIGHT_CTRL) _ctrlDown = false;
    };
    keys->onKeyPressed = [this](EventKeyboard::KeyCode key, Event* e) {
        using K = EventKeyboard::KeyCode;
        if (key == K::KEY_CTRL || key == K::KEY_LEFT_CTRL || key == K::KEY_RIGHT_CTRL) _ctrlDown = true;
        if (_closing || ui::Dropdown::anyOpen()) return;
        Scene* scene = Director::getInstance()->getRunningScene();
        if (scene) {
            const auto& children = scene->getChildren();
            for (auto it = children.rbegin(); it != children.rend(); ++it) {
                if ((*it)->getName() == ui::kModalNodeName || dynamic_cast<HWWindow*>(*it)) {
                    if (*it != this) return;  // only the topmost modal answers
                    break;
                }
            }
        }
        if (key == EventKeyboard::KeyCode::KEY_ESCAPE || key == EventKeyboard::KeyCode::KEY_BACK) {
            e->stopPropagation();
            dismiss();
            return;
        }
        if (onKey(key)) e->stopPropagation();
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keys, this);

    auto mouse = EventListenerMouse::create();
    mouse->onMouseScroll = [this](EventMouse* e) {
        onScroll(Vec2(e->getCursorX(), e->getCursorY()), e->getScrollY());
        e->stopPropagation();
    };
    // Hover stops here, so the screen below does not light up through the dim.
    mouse->onMouseMove = [](EventMouse* e) { e->stopPropagation(); };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, this);
    return true;
}

void Panel::present() {
    Scene* scene = Director::getInstance()->getRunningScene();
    if (!scene) return;
    scene->addChild(this, kPanelZ);
    _dim->setOpacity(0);
    _dim->runAction(FadeTo::create(0.2f, 190));
    _content->setScale(1.12f);
    _content->setOpacity(0);
    _content->runAction(
        Spawn::create(EaseBackOut::create(ScaleTo::create(0.25f, 1.0f)), FadeIn::create(0.18f), nullptr));
    if (_firstField) {
        _firstField->focus();
    } else {
        // Take the keyboard from the screen below (the browser's search field) while open.
        struct Sink : public Node, public IMEDelegate {
            bool canAttachWithIME() override { return true; }
            bool canDetachWithIME() override { return true; }
            void insertText(const char*, size_t) override {}
            void deleteBackward() override {}
            const std::string& getContentText() override { return _empty; }
            std::string _empty;
        };
        auto* sink = new (std::nothrow) Sink();
        if (sink && sink->init()) {
            sink->autorelease();
            addChild(sink);
            sink->attachWithIME();
        } else {
            delete sink;
        }
    }
}

void Panel::dismiss() {
    if (_closing) return;
    _closing = true;
    RefPtr<Panel> keep(this);
    onClosed();
    ui::Dropdown::closeAll();
    _eventDispatcher->removeEventListenersForTarget(this);
    _dim->runAction(FadeTo::create(0.15f, 0));
    _content->runAction(Sequence::create(Spawn::create(ScaleTo::create(0.15f, 0.94f), FadeOut::create(0.15f), nullptr),
                                         CallFunc::create([this]() { removeFromParent(); }), nullptr));
}

// ---- TextInput ----------------------------------------------------------------------------------

TextInput* TextInput::create(const Size& size, const std::string& placeholder, bool password, int maxChars) {
    TextInput* t = new (std::nothrow) TextInput();
    if (t && t->init(size, placeholder, password, maxChars)) {
        t->autorelease();
        return t;
    }
    delete t;
    return nullptr;
}

TextInput::~TextInput() { clearSecret(); }

bool TextInput::init(const Size& size, const std::string& placeholder, bool password, int maxChars) {
    if (!Node::init()) return false;
    _placeholder = placeholder;
    _password = password;
    _maxChars = maxChars;
    setContentSize(size);
    setAnchorPoint(Vec2(0.5f, 0.5f));
    const float r = std::min(size.height * 0.5f, 40.0f);
    _rim = ui::roundedRect(Size(size.width + 16.0f, size.height + 16.0f), r + 8.0f, Color3B(200, 202, 212), 255);
    _rim->setAnchorPoint(Vec2::ZERO);
    _rim->setPosition(-8.0f, -8.0f);
    addChild(_rim);
    auto* bg = ui::roundedRect(size, r, Color3B::WHITE, 255);
    bg->setAnchorPoint(Vec2::ZERO);
    addChild(bg);
    _label = Label::createWithTTF("", ui::kFontBody, size.height * 0.42f);
    _label->setAnchorPoint(Vec2(0.0f, 0.5f));
    _label->setPosition(r * 0.9f + 10.0f, size.height * 0.5f);
    addChild(_label);
    _caret = Sprite::create();
    _caret->setTextureRect(Rect(0, 0, 6.0f, size.height * 0.52f));
    _caret->setColor(ui::kBlue);
    _caret->setAnchorPoint(Vec2(0.0f, 0.5f));
    _caret->setVisible(false);
    addChild(_caret);

    auto touch = EventListenerTouchOneByOne::create();
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (!ui::isShown(this)) return false;
        const Vec2 p = convertToNodeSpace(t->getLocation());
        if (!Rect(0, 0, _contentSize.width, _contentSize.height).containsPoint(p)) return false;
        focus();
        if (GLView* view = Director::getInstance()->getOpenGLView()) view->setIMEKeyboardState(true);
        return true;
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
    refresh();
    return true;
}

void TextInput::setText(const std::string& text) {
    _text = text;
    refresh();
}

void TextInput::clearSecret() {
    std::fill(_text.begin(), _text.end(), '\0');
    _text.clear();
    if (_label) refresh();
}

void TextInput::focus() {
    if (!_focused) attachWithIME();
}

void TextInput::unfocus() {
    if (_focused) detachWithIME();
}

void TextInput::didAttachWithIME() {
    _focused = true;
    _rim->setColor(ui::kBlue);
    _caret->stopAllActions();
    _caret->setVisible(true);
    _caret->runAction(RepeatForever::create(
        Sequence::create(DelayTime::create(0.5f), Hide::create(), DelayTime::create(0.4f), Show::create(), nullptr)));
    refresh();
}

void TextInput::didDetachWithIME() {
    _focused = false;
    _rim->setColor(Color3B(200, 202, 212));
    _caret->stopAllActions();
    _caret->setVisible(false);
    refresh();
}

void TextInput::insertText(const char* text, size_t len) {
    std::string in(text, len);
    if (in.find('\n') != std::string::npos) {
        if (GLView* view = Director::getInstance()->getOpenGLView()) view->setIMEKeyboardState(false);
        if (onSubmit) onSubmit();
    }
    std::string clean;
    for (char c : in) {
        if (c == '\n' || c == '\r' || c == '\t') continue;
        if (!_allowed.empty() && _allowed.find(c) == std::string::npos) continue;
        clean += c;
    }
    if (clean.empty()) return;
    if ((int)StringUtils::getCharacterCountInUTF8String(_text + clean) > _maxChars) return;
    _text += clean;
    refresh();
    if (onChange) onChange();
}

void TextInput::deleteBackward() {
    if (_text.empty()) return;
    std::u32string u32;
    if (StringUtils::UTF8ToUTF32(_text, u32) && !u32.empty()) {
        u32.pop_back();
        std::string s;
        StringUtils::UTF32ToUTF8(u32, s);
        std::fill(_text.begin(), _text.end(), '\0');
        _text = s;
    } else {
        _text.pop_back();
    }
    refresh();
    if (onChange) onChange();
}

void TextInput::paste() {
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
    auto* view = dynamic_cast<GLViewImpl*>(Director::getInstance()->getOpenGLView());
    if (!view) return;
    const char* clip = glfwGetClipboardString(view->getWindow());
    if (!clip) return;
    std::string s = clip;
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    insertText(s.c_str(), s.size());
#endif
}

void TextInput::refresh() {
    const float maxW = _contentSize.width - _label->getPositionX() * 2.0f;
    if (_text.empty()) {
        _label->setString(_placeholder);
        _label->setColor(Color3B(150, 152, 168));
    } else {
        _label->setColor(Color3B(40, 40, 52));
        std::string shown;
        if (_password) {
            const long n = StringUtils::getCharacterCountInUTF8String(_text);
            for (long i = 0; i < n; ++i) shown += "\xE2\x80\xA2";  // bullet
        } else {
            shown = _text;
        }
        _label->setString(shown);
        if (_label->getContentSize().width > maxW) {
            std::u32string u32;
            StringUtils::UTF8ToUTF32(shown, u32);
            size_t start = 0;
            std::string s;
            do {
                ++start;
                StringUtils::UTF32ToUTF8(u32.substr(start), s);
                _label->setString("\xE2\x80\xA6" + s);
            } while (start + 1 < u32.size() && _label->getContentSize().width > maxW);
        }
    }
    const float tw = _text.empty() ? 0.0f : _label->getContentSize().width;
    _caret->setPosition(_label->getPositionX() + tw + 4.0f, _contentSize.height * 0.5f);
}

// ---- CheckBox -----------------------------------------------------------------------------------

CheckBox* CheckBox::create(const std::string& text, bool checked, const Color3B& textColor) {
    CheckBox* c = new (std::nothrow) CheckBox();
    if (c && c->init(text, checked, textColor)) {
        c->autorelease();
        return c;
    }
    delete c;
    return nullptr;
}

bool CheckBox::init(const std::string& text, bool checked, const Color3B& textColor) {
    if (!Node::init()) return false;
    const float box = 70.0f;
    auto* rim = ui::roundedRect(Size(box, box), 16.0f, Color3B(200, 202, 212), 255);
    rim->setAnchorPoint(Vec2::ZERO);
    addChild(rim);
    auto* bg = ui::roundedRect(Size(box - 12.0f, box - 12.0f), 11.0f, Color3B::WHITE, 255);
    bg->setAnchorPoint(Vec2::ZERO);
    bg->setPosition(6.0f, 6.0f);
    addChild(bg);
    _tick = icon("check", box * 0.72f);
    _tick->setColor(ui::kBlue);
    _tick->setPosition(box * 0.5f, box * 0.5f);
    addChild(_tick);
    Label* l = label(text, ui::kFontBodyBold, 44.0f, textColor);
    l->setPosition(box + 24.0f, box * 0.5f);
    addChild(l);
    setContentSize(Size(box + 24.0f + l->getContentSize().width, box));
    setChecked(checked);

    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (!ui::isShown(this)) return false;
        const Vec2 p = convertToNodeSpace(t->getLocation());
        return Rect(-10.0f, -10.0f, _contentSize.width + 20.0f, _contentSize.height + 20.0f).containsPoint(p);
    };
    touch->onTouchEnded = [this](Touch* t, Event*) {
        const Vec2 p = convertToNodeSpace(t->getLocation());
        if (!Rect(-10.0f, -10.0f, _contentSize.width + 20.0f, _contentSize.height + 20.0f).containsPoint(p)) return;
        setChecked(!_checked);
        if (onToggle) onToggle(_checked);
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
    return true;
}

void CheckBox::setChecked(bool checked) {
    _checked = checked;
    _tick->setVisible(checked);
}

// ---- StarPicker ---------------------------------------------------------------------------------

StarPicker* StarPicker::create(float starSize) {
    StarPicker* s = new (std::nothrow) StarPicker();
    if (s && s->init(starSize)) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

bool StarPicker::init(float starSize) {
    if (!Node::init()) return false;
    _starSize = starSize;
    const float gap = starSize * 0.18f;
    for (int i = 0; i < 5; ++i) {
        Sprite* s = ui::iconSprite("star", starSize);
        s->setAnchorPoint(Vec2::ZERO);
        s->setPosition(i * (starSize + gap), 0.0f);
        addChild(s);
        _stars.push_back(s);
    }
    setContentSize(Size(5 * starSize + 4 * gap, starSize));
    show(0);
    auto starAt = [this](const Vec2& world) {
        const Vec2 p = convertToNodeSpace(world);
        if (p.y < -20.0f || p.y > _contentSize.height + 20.0f || p.x < 0.0f || p.x > _contentSize.width) return 0;
        return std::min(5, (int)(p.x / (_contentSize.width / 5.0f)) + 1);
    };
    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this, starAt](Touch* t, Event*) { return ui::isShown(this) && starAt(t->getLocation()) > 0; };
    touch->onTouchEnded = [this, starAt](Touch* t, Event*) {
        const int n = starAt(t->getLocation());
        if (n <= 0) return;
        setRating(n);
        if (onPick) onPick(n);
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
    auto mouse = EventListenerMouse::create();
    mouse->onMouseMove = [this, starAt](EventMouse* e) {
        if (!ui::isShown(this)) return;
        const int n = starAt(Vec2(e->getCursorX(), e->getCursorY()));
        show(n > 0 ? n : _rating);
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, this);
    return true;
}

void StarPicker::setRating(int rating) {
    _rating = std::max(0, std::min(5, rating));
    show(_rating);
}

void StarPicker::show(int stars) {
    for (int i = 0; i < 5; ++i) {
        _stars[i]->setColor(i < stars ? ui::kStarGold : Color3B(176, 176, 182));
    }
}

// ---- alerts -------------------------------------------------------------------------------------

namespace {

class CallbackDelegate : public HWWindowDelegate {
public:
    explicit CallbackDelegate(std::function<void(bool)> done) : _done(std::move(done)) {}
    void hwWindowButtonPressed(int buttonTag, HWWindow*) override {
        auto done = std::move(_done);
        _done = nullptr;
        if (done) done(buttonTag == 1);
    }
    // The window still notifies its delegates when its fade-out ends: only then is it safe to go.
    void hwWindowWasDismissed(HWWindow*) override {
        Director::getInstance()->getScheduler()->performFunctionInCocosThread([this]() { delete this; });
    }

private:
    std::function<void(bool)> _done;
};

}  // namespace

void alert(const std::string& title, const std::string& message) {
    HWWindow* w = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, false, false);
    w->showAlertMessage(title, message, "OK", "", true);
}

void confirm(const std::string& title, const std::string& message, const std::string& yes, const std::string& no,
             std::function<void(bool)> done) {
    auto* delegate = new CallbackDelegate(std::move(done));
    HWWindow* w = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, delegate, true, false);
    w->showAlertMessage(title, message, yes, no, true);
}

}  // namespace tjfui
}  // namespace online
