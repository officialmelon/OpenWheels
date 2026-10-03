// ONLINE (PC addition): see OnlineUi.h.
#include "online/OnlineUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HWWindow.h"
#include "platform/CCImage.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
#include "platform/desktop/CCGLViewImpl-desktop.h"
#endif

USING_NS_CC;

namespace online {
namespace ui {

const char* const kFontHeading = "fonts/ClarendonLTStd-Bold.ttf";
const char* const kFontBody = "fonts/Arial.ttf";
const char* const kFontBodyBold = "fonts/Arial Bold.ttf";

const Color3B kPink(253, 129, 129);
const Color3B kBlue(61, 139, 199);
const Color3B kTextDim(178, 180, 200);
const Color3B kStarGold(255, 196, 38);
const Color3B kInk(62, 62, 72);
const Color3B kInkDim(118, 118, 128);

void loadAtlases() {
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    for (const char* plist : {"menus/main/menu_main.plist", "menus/alert/menu_alert.plist"})
        if (!cache->isSpriteFramesWithFileLoaded(plist)) cache->addSpriteFramesWithFile(plist);
}

namespace {

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Generates a white, premultiplied texture from a per-pixel coverage function (pixel centre
// coordinates, y down). Cached under `key`.
Texture2D* generateTexture(const std::string& key, int w, int h, const std::function<float(float, float)>& coverage,
                           bool mipmaps) {
    TextureCache* cache = Director::getInstance()->getTextureCache();
    if (Texture2D* t = cache->getTextureForKey(key)) return t;
    std::vector<unsigned char> px((size_t)w * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const unsigned char a = (unsigned char)std::lround(clamp01(coverage(x + 0.5f, y + 0.5f)) * 255.0f);
            unsigned char* p = &px[((size_t)y * w + x) * 4];
            p[0] = p[1] = p[2] = p[3] = a;
        }
    }
    Image* image = new (std::nothrow) Image();
    image->initWithRawData(px.data(), (ssize_t)px.size(), w, h, 8, true);
    Texture2D* t = cache->addImage(image, key);
    image->release();
    if (t && mipmaps) {
        t->generateMipmap();
        Texture2D::TexParams params = {GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
        t->setTexParameters(params);
    } else if (t) {
        t->setAntiAliasTexParameters();
    }
    return t;
}

// 4x4 supersampled coverage of a predicate over unit coordinates u, v in [-1, 1] (y down).
std::function<float(float, float)> supersampled(int size, const std::function<bool(float, float)>& inside) {
    return [size, inside](float px, float py) {
        int hits = 0;
        for (int sy = 0; sy < 4; ++sy)
            for (int sx = 0; sx < 4; ++sx) {
                const float x = px - 0.5f + (sx + 0.5f) / 4.0f;
                const float y = py - 0.5f + (sy + 0.5f) / 4.0f;
                if (inside(x / size * 2.0f - 1.0f, y / size * 2.0f - 1.0f)) ++hits;
            }
        return hits / 16.0f;
    };
}

float segmentDistance(float px, float py, float ax, float ay, float bx, float by) {
    const float dx = bx - ax, dy = by - ay;
    float t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy);
    t = std::max(0.0f, std::min(1.0f, t));
    const float ex = px - (ax + t * dx), ey = py - (ay + t * dy);
    return std::sqrt(ex * ex + ey * ey);
}

bool starInside(float u, float v) {
    // 5-point star, one point up; inner radius 0.47 of the outer.
    const float kPi = 3.14159265f;
    const float outer = 0.98f, inner = outer * 0.47f;
    float vx[10], vy[10];
    for (int i = 0; i < 10; ++i) {
        const float r = (i % 2 == 0) ? outer : inner;
        const float a = -kPi / 2.0f + i * kPi / 5.0f;
        vx[i] = r * std::cos(a);
        vy[i] = r * std::sin(a) + 0.08f;  // optical centring
    }
    bool in = false;
    for (int i = 0, j = 9; i < 10; j = i++) {
        if (((vy[i] > v) != (vy[j] > v)) && (u < (vx[j] - vx[i]) * (v - vy[i]) / (vy[j] - vy[i]) + vx[i])) in = !in;
    }
    return in;
}

bool globeInside(float u, float v) {
    const float R = 0.9f, t = 0.085f, tl = 0.07f;
    const float len = std::sqrt(u * u + v * v);
    if (std::fabs(len - R) < t) return true;
    if (len >= R) return false;
    if (std::fabs(u) < tl) return true;                       // central meridian
    if (std::fabs(v) < tl) return true;                       // equator
    if (std::fabs(std::fabs(v) - R * 0.52f) < tl) return true;  // latitudes
    const float a = R * 0.48f;                                // meridian ellipse
    const float e = std::sqrt((u / a) * (u / a) + (v / R) * (v / R));
    return std::fabs(e - 1.0f) * a < tl;
}

bool magnifierInside(float u, float v) {
    const float cx = -0.16f, cy = -0.16f, r = 0.52f, t = 0.13f;
    const float d = std::sqrt((u - cx) * (u - cx) + (v - cy) * (v - cy));
    if (std::fabs(d - r) < t) return true;
    const float k = 0.7071f * (r + t * 0.6f);
    return segmentDistance(u, v, cx + k, cy + k, 0.82f, 0.82f) < 0.15f;
}

bool clearInside(float u, float v) {
    // filled circle with an X cut out
    if (u * u + v * v > 0.92f * 0.92f) return false;
    const float k = 0.36f;
    return segmentDistance(u, v, -k, -k, k, k) > 0.11f && segmentDistance(u, v, -k, k, k, -k) > 0.11f;
}

bool chevronInside(float u, float v) {
    // a thick "V"
    return segmentDistance(u, v, -0.62f, -0.28f, 0.0f, 0.34f) < 0.15f ||
           segmentDistance(u, v, 0.62f, -0.28f, 0.0f, 0.34f) < 0.15f;
}

bool spinnerInside(float u, float v) {
    const float d = std::sqrt(u * u + v * v);
    return std::fabs(d - 0.78f) < 0.17f;
}

Texture2D* iconTexture(const std::string& name) {
    const int n = name == "globe" ? 256 : 128;
    if (name == "star") return generateTexture("online_icon_star", n, n, supersampled(n, starInside), true);
    if (name == "globe") return generateTexture("online_icon_globe", n, n, supersampled(n, globeInside), true);
    if (name == "search") return generateTexture("online_icon_search", n, n, supersampled(n, magnifierInside), true);
    if (name == "chevron") return generateTexture("online_icon_chevron", n, n, supersampled(n, chevronInside), true);
    if (name == "clear") return generateTexture("online_icon_clear", n, n, supersampled(n, clearInside), true);
    if (name == "spinner") {
        auto ring = supersampled(n, spinnerInside);
        return generateTexture("online_icon_spinner", n, n,
                               [ring, n](float x, float y) {
                                   // 300 degree arc fading towards its tail.
                                   const float u = x / n * 2.0f - 1.0f, v = y / n * 2.0f - 1.0f;
                                   float a = std::atan2(v, u) / (2.0f * 3.14159265f);
                                   if (a < 0.0f) a += 1.0f;
                                   const float fade = a < 0.84f ? a / 0.84f : 0.0f;
                                   return ring(x, y) * fade;
                               },
                               true);
    }
    return nullptr;
}

}  // namespace

cocos2d::ui::Scale9Sprite* roundedRect(const Size& size, float radius, const Color3B& color, GLubyte opacity) {
    const float csf = Director::getInstance()->getContentScaleFactor();
    const int pr = std::max(2, (int)std::lround(radius * csf));
    const int n = pr * 2 + 4;
    const std::string key = "online_rr_" + std::to_string(pr);
    Texture2D* t = generateTexture(key, n, n,
                                   [n, pr](float x, float y) {
                                       // signed distance to a rounded box filling the texture
                                       const float h = n * 0.5f;
                                       const float qx = std::fabs(x - h) - (h - pr);
                                       const float qy = std::fabs(y - h) - (h - pr);
                                       const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
                                       const float d = std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - pr;
                                       return 0.5f - d;
                                   },
                                   false);
    const Size ts = t->getContentSize();
    SpriteFrame* frame = SpriteFrame::createWithTexture(t, Rect(0, 0, ts.width, ts.height));
    const float inset = (pr + 1) / csf;
    auto* sprite = cocos2d::ui::Scale9Sprite::createWithSpriteFrame(frame, Rect(inset, inset, ts.width - 2 * inset, ts.height - 2 * inset));
    sprite->setContentSize(Size(std::max(size.width, ts.width), std::max(size.height, ts.height)));
    sprite->setColor(color);
    sprite->setOpacity(opacity);
    return sprite;
}

Sprite* iconSprite(const std::string& name, float size) {
    Texture2D* t = iconTexture(name);
    if (!t) return Sprite::create();
    Sprite* s = Sprite::createWithTexture(t);
    s->setScale(size / t->getContentSize().height);
    return s;
}

Sprite* frameSprite(const std::string& frame, const Size& size, const Rect& centre) {
    loadAtlases();
    Sprite* s = Sprite::createWithSpriteFrameName(frame);
    s->setCenterRectNormalized(centre);
    s->setContentSize(size);
    return s;
}

Sprite* windowPanel(const Size& size) {
    // HWWindow::init's slicing of window_frame.png.
    return frameSprite("window_frame.png", size, Rect(0.1f, 0.62f, 0.8f, 0.28f));
}

float windowPanelStrip() {
    loadAtlases();
    SpriteFrame* f = SpriteFrameCache::getInstance()->getSpriteFrameByName("window_frame.png");
    return f ? f->getOriginalSize().height * 0.62f : 62.0f;
}

void setMenuButtonIcon(MenuItemSprite* button, const std::string& icon) {
    for (Node* child : Vector<Node*>(button->getChildren()))
        if (child != button->getNormalImage() && child != button->getSelectedImage()) button->removeChild(child, true);
    Sprite* sprite = iconSprite(icon, 196.0f);  // the atlas icons are ~180-210 units
    sprite->setPosition(button->getContentSize() / 2.0f);
    button->addChild(sprite);
}

// ---- StarBar --------------------------------------------------------------------------------------

StarBar* StarBar::create(float height, const Color3B& emptyColor, GLubyte emptyOpacity) {
    StarBar* bar = new (std::nothrow) StarBar();
    if (!bar || !bar->init()) {
        delete bar;
        return nullptr;
    }
    bar->autorelease();
    const float gap = height * 0.10f;
    for (int i = 0; i < 5; ++i) {
        Sprite* back = iconSprite("star", height);
        back->setColor(emptyColor);
        back->setOpacity(emptyOpacity);
        back->setAnchorPoint(Vec2(0.0f, 0.0f));
        back->setPosition(i * (height + gap), 0.0f);
        bar->addChild(back);
        Sprite* fill = iconSprite("star", height);
        fill->setColor(kStarGold);
        fill->setAnchorPoint(Vec2(0.0f, 0.0f));
        fill->setPosition(back->getPosition());
        bar->addChild(fill);
        bar->_fills.push_back(fill);
    }
    bar->setContentSize(Size(5 * height + 4 * gap, height));
    bar->setCascadeOpacityEnabled(true);
    return bar;
}

void StarBar::setRating(float rating) {
    for (int i = 0; i < 5; ++i) {
        Sprite* fill = _fills[i];
        const float f = clamp01(rating - i);
        Texture2D* t = fill->getTexture();
        const Size ts = t->getContentSize();
        // Round tiny fractions so a 4.97 doesn't show a sliver-less last star.
        const float frac = f > 0.97f ? 1.0f : (f < 0.03f ? 0.0f : f);
        fill->setVisible(frac > 0.0f);
        fill->setTextureRect(Rect(0, 0, ts.width * frac, ts.height));
    }
}

// ---- text -------------------------------------------------------------------------------------

std::string formatCount(int n) {
    auto fmt = [](double v, const char* suffix) {
        char buf[32];
        std::snprintf(buf, sizeof buf, v < 9.95 ? "%.1f%s" : "%.0f%s", v, suffix);
        std::string s = buf;
        const size_t dot0 = s.find(".0");  // "1.0K" -> "1K"
        if (dot0 != std::string::npos) s.erase(dot0, 2);
        return s;
    };
    if (n >= 999500000) return fmt(n / 1e9, "B");
    if (n >= 999500) return fmt(n / 1e6, "M");
    if (n >= 1000) return fmt(n / 1e3, "K");
    return std::to_string(n);
}

std::string formatThousands(int n) {
    std::string digits = std::to_string(n < 0 ? -n : n);
    std::string out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count && count % 3 == 0) out.insert(out.begin(), ',');
        out.insert(out.begin(), *it);
        ++count;
    }
    return n < 0 ? "-" + out : out;
}

std::string formatDate(const std::string& ymd) {
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int y = 0, m = 0, d = 0;
    if (std::sscanf(ymd.c_str(), "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12) return ymd;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%s %d, %d", months[m - 1], d, y);
    return buf;
}

std::string formatRating(float rating) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.2f", rating);
    return buf;
}

void setEllipsized(Label* label, const std::string& text, float maxWidth) {
    label->setString(text);
    if (label->getContentSize().width <= maxWidth) return;
    std::u32string u32;
    if (!StringUtils::UTF8ToUTF32(text, u32)) return;
    size_t lo = 0, hi = u32.size();
    std::string best = "\xE2\x80\xA6";
    while (lo < hi) {
        const size_t mid = (lo + hi + 1) / 2;
        std::u32string cut = u32.substr(0, mid);
        while (!cut.empty() && cut.back() == U' ') cut.pop_back();
        std::string s;
        StringUtils::UTF32ToUTF8(cut, s);
        s += "\xE2\x80\xA6";
        label->setString(s);
        if (label->getContentSize().width <= maxWidth) {
            best = s;
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    label->setString(best);
}

std::string characterName(int c) {
    static const char* names[] = {"Any character", "Wheelchair Guy", "Segway Guy", "Irresponsible Dad",
                                  "Effective Shopper", "Moped Couple", "Lawnmower Man", "Explorer Guy",
                                  "Santa Claus", "Pogostick Man", "Irresponsible Mom", "Helicopter Man"};
    if (c >= 0 && c <= 11) return names[c];
    return "Character " + std::to_string(c);
}

std::string characterTag(int c) {
    switch (c) {
        case 0: return "ANY";
        case 6: return "MOWER";
        case 7: return "EXPLORER";
        case 8: return "SANTA";
        case 10: return "MOM";
        case 11: return "HELI";
        default: return characterOnMobile(c) ? "" : "?";
    }
}

bool characterOnMobile(int c) { return (c >= 1 && c <= 5) || c == 9; }

std::string characterPortrait(int c) {
    if (characterOnMobile(c)) return "menus/main/portraits/char" + std::to_string(c) + "_portrait_25p.png";
    return "menus/main/portraits/generic_25p.png";
}

// ---- toast -------------------------------------------------------------------------------------

void showToast(const std::string& title, const std::vector<std::string>& lines, float delay) {
    static int s_serial = 0;
    const std::string key = "online_toast_" + std::to_string(++s_serial);
    Director::getInstance()->getScheduler()->schedule(
        [title, lines, key](float) {
            Scene* scene = Director::getInstance()->getRunningScene();
            if (!scene || dynamic_cast<TransitionScene*>(scene)) {
                showToast(title, lines, 0.3f);  // try again once the transition has finished
                return;
            }
            const Size vs = Director::getInstance()->getVisibleSize();
            const Vec2 origin = Director::getInstance()->getVisibleOrigin();
            const float width = std::min(2000.0f, vs.width - 200.0f);
            Node* toast = Node::create();
            toast->setCascadeOpacityEnabled(true);
            Label* head = Label::createWithTTF(title, kFontHeading, 62.0f);
            head->setColor(kInk);
            head->setAnchorPoint(Vec2(0.0f, 1.0f));
            std::string body;
            for (size_t i = 0; i < lines.size() && i < 5; ++i) body += (i ? "\n" : "") + std::string("\xE2\x80\xA2 ") + lines[i];
            if (lines.size() > 5) body += "\n\xE2\x80\xA2 ...";
            Label* text = Label::createWithTTF(body, kFontBody, 46.0f, Size(width - 140.0f, 0.0f));
            text->setColor(kInk);
            text->setAnchorPoint(Vec2(0.0f, 1.0f));
            const float strip = windowPanelStrip();
            const float h = strip + 50.0f + head->getContentSize().height + 24.0f + text->getContentSize().height + 60.0f;
            Sprite* bg = windowPanel(Size(width, h));
            bg->setAnchorPoint(Vec2(0.0f, 0.0f));
            toast->addChild(bg);
            head->setPosition(70.0f, h - strip - 50.0f);
            text->setPosition(70.0f, h - strip - 50.0f - head->getContentSize().height - 24.0f);
            toast->addChild(head);
            toast->addChild(text);
            toast->setContentSize(Size(width, h));
            toast->setPosition(origin.x + (vs.width - width) * 0.5f, origin.y + vs.height - h - 60.0f);
            toast->setOpacity(0);
            scene->addChild(toast, 1000000);
            toast->runAction(Sequence::create(FadeIn::create(0.25f), DelayTime::create(6.0f), FadeOut::create(0.6f),
                                              RemoveSelf::create(), nullptr));
        },
        Director::getInstance(), 0.0f, 0, delay, false, key);
}

bool isShown(Node* node) {
    for (Node* n = node; n; n = n->getParent())
        if (!n->isVisible()) return false;
    return node && node->isRunning();
}

namespace {
Node* s_openPopup = nullptr;  // the open Dropdown menu (child of the running scene)
}  // namespace

bool modalOpen() {
    if (s_openPopup) return true;
    Scene* scene = Director::getInstance()->getRunningScene();
    if (!scene) return false;
    for (Node* child : scene->getChildren())
        if (dynamic_cast<HWWindow*>(child)) return true;
    return false;
}

// ---- Button -----------------------------------------------------------------------------------

Button::Style Button::chunky(const std::string& colour) {
    Style s;
    s.frame = "menu_main_btn_" + colour + "_normal.png";
    s.frameDown = "menu_main_btn_" + colour + "_down.png";
    s.centre = Rect(0.25f, 0.25f, 0.5f, 0.5f);
    return s;
}

Button::Style Button::playButton() {
    Style s;
    s.frame = "menu_main_playbtn_blue_normal.png";
    s.frameDown = "menu_main_playbtn_blue_down.png";
    s.centre = Rect(0.2f, 0.25f, 0.6f, 0.5f);
    return s;
}

Button::Style Button::window(const std::string& colour) {
    Style s;
    s.frame = "window_btn_" + colour + ".png";
    s.centre = Rect(0.35f, 0.35f, 0.3f, 0.3f);
    if (colour == "yellow") s.textColor = Color3B(70, 70, 70);
    return s;
}

Button::Style Button::ghost() {
    Style s;
    s.color = Color3B(0, 0, 0);
    s.opacity = 90;
    s.radius = 26.0f;
    return s;
}

Button* Button::create(const std::string& text, const Size& size, const Style& style, float fontSize,
                       const std::string& font) {
    Button* b = new (std::nothrow) Button();
    if (b && b->init(text, size, style, fontSize, font)) {
        b->autorelease();
        return b;
    }
    delete b;
    return nullptr;
}

bool Button::init(const std::string& text, const Size& size, const Style& style, float fontSize,
                  const std::string& font) {
    if (!Node::init()) return false;
    loadAtlases();
    _style = style;
    setContentSize(size);
    setAnchorPoint(Vec2(0.5f, 0.5f));
    setCascadeOpacityEnabled(true);
    rebuildBackground();
    _label = Label::createWithTTF(text, font, fontSize);
    _label->setAnchorPoint(Vec2(0.5f, 0.5f));
    addChild(_label, 1);
    layoutContent();

    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (!_enabled || !isShown(this) || !hitTest(t->getLocation())) return false;
        _pressed = true;
        refresh();
        return true;
    };
    touch->onTouchMoved = [this](Touch* t, Event*) {
        const bool in = hitTest(t->getLocation());
        if (in != _pressed) {
            _pressed = in;
            refresh();
        }
    };
    touch->onTouchEnded = [this](Touch* t, Event*) {
        const bool fire = _pressed && hitTest(t->getLocation());
        _pressed = false;
        refresh();
        if (fire && _enabled && _callback) {
            RefPtr<Button> keep(this);
            auto cb = _callback;
            cb();
        }
    };
    touch->onTouchCancelled = [this](Touch*, Event*) {
        _pressed = false;
        refresh();
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);

    auto mouse = EventListenerMouse::create();
    mouse->onMouseMove = [this](EventMouse* e) {
        const bool h = _enabled && isShown(this) && hitTest(Vec2(e->getCursorX(), e->getCursorY()));
        if (h != _hover) {
            _hover = h;
            refresh();
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, this);
    refresh();
    return true;
}

void Button::rebuildBackground() {
    if (_bg) _bg->removeFromParent();
    if (!_style.frame.empty()) {
        _bg = frameSprite(_style.frame, _contentSize, _style.centre);
        _bg->setColor(_style.color);
    } else {
        _bg = roundedRect(_contentSize, _style.radius, _style.color, _style.opacity);
    }
    _bg->setAnchorPoint(Vec2(0.5f, 0.5f));
    _bg->setPosition(_contentSize.width * 0.5f, _contentSize.height * 0.5f);
    addChild(_bg, 0);
}

void Button::setStyle(const Style& style) {
    _style = style;
    rebuildBackground();
    refresh();
}

bool Button::hitTest(const Vec2& worldPoint) const {
    const Vec2 p = convertToNodeSpace(worldPoint);
    return Rect(0, 0, _contentSize.width, _contentSize.height).containsPoint(p);
}

void Button::setText(const std::string& text) {
    _label->setString(text);
    layoutContent();
}

void Button::setButtonSize(const Size& size) {
    setContentSize(size);
    rebuildBackground();
    layoutContent();
    refresh();
}

void Button::setEnabled(bool enabled) {
    if (_enabled == enabled) return;
    _enabled = enabled;
    if (!enabled) _hover = _pressed = false;
    refresh();
}

void Button::setIcon(Sprite* icon, bool iconRight) {
    if (_icon) _icon->removeFromParent();
    _icon = icon;
    _iconRight = iconRight;
    if (_icon) addChild(_icon, 1);
    layoutContent();
    refresh();
}

void Button::layoutContent() {
    const Size s = _contentSize;
    const float cy = s.height * 0.5f + (_style.frame.empty() ? 0.0f : 4.0f);  // the art's bevel sits low
    if (_icon) {
        const float iw = _icon->getBoundingBox().size.width;
        const bool hasText = !_label->getString().empty();
        const float gap = hasText ? std::min(36.0f, s.height * 0.18f) : 0.0f;
        const float tw = hasText ? _label->getContentSize().width : 0.0f;
        const float x0 = (s.width - (iw + gap + tw)) * 0.5f;
        if (_iconRight) {
            _label->setPosition(x0 + tw * 0.5f, cy);
            _icon->setPosition(x0 + tw + gap + iw * 0.5f, cy);
        } else {
            _icon->setPosition(x0 + iw * 0.5f, cy);
            _label->setPosition(x0 + iw + gap + tw * 0.5f, cy);
        }
    } else {
        _label->setPosition(s.width * 0.5f, cy);
    }
}

void Button::refresh() {
    if (!_style.frame.empty()) {
        const std::string frame = (_pressed && !_style.frameDown.empty()) ? _style.frameDown : _style.frame;
        SpriteFrame* f = SpriteFrameCache::getInstance()->getSpriteFrameByName(frame);
        if (f && _bg->getSpriteFrame() != f) {
            _bg->setSpriteFrame(f);
            _bg->setCenterRectNormalized(_style.centre);
            _bg->setContentSize(_contentSize);
        }
        Color3B c = _style.color;
        if (_pressed && _style.frameDown.empty()) c = Color3B((GLubyte)(c.r * 0.82f), (GLubyte)(c.g * 0.82f), (GLubyte)(c.b * 0.82f));
        _bg->setColor(c);
        _bg->setOpacity(_style.opacity);
    } else {
        Color3B c = _style.color;
        GLubyte o = _style.opacity;
        if (_pressed) {
            c = Color3B((GLubyte)(c.r * 0.82f), (GLubyte)(c.g * 0.82f), (GLubyte)(c.b * 0.82f));
            if (o < 255) o = (GLubyte)std::min(255, o + 50);
        } else if (_hover) {
            c = Color3B((GLubyte)std::min(255, c.r + 22), (GLubyte)std::min(255, c.g + 22), (GLubyte)std::min(255, c.b + 22));
            if (o < 255) o = (GLubyte)std::min(255, o + 35);
        }
        _bg->setColor(c);
        _bg->setOpacity(o);
    }
    _label->setColor(_style.textColor);
    if (_icon) _icon->setColor(_style.textColor);
    const float scale = _pressed ? 0.97f : (_hover ? 1.025f : 1.0f);
    setScale(scale);
    setOpacity(_enabled ? 255 : 105);
}

// ---- Dropdown ---------------------------------------------------------------------------------

Dropdown* Dropdown::create(const std::vector<std::string>& options, const Size& size, const Style& style,
                           float fontSize) {
    Dropdown* d = new (std::nothrow) Dropdown();
    if (d && d->init(options.empty() ? std::string() : options[0], size, style, fontSize, kFontHeading)) {
        d->autorelease();
        d->_options = options;
        d->setIcon(iconSprite("chevron", fontSize * 0.62f), true);
        d->setCallback([d]() { d->open(); });
        return d;
    }
    delete d;
    return nullptr;
}

void Dropdown::setSelectedIndex(int index) {
    if (index < 0 || index >= (int)_options.size()) return;
    _selected = index;
    setText(_options[index]);
}

bool Dropdown::anyOpen() { return s_openPopup != nullptr; }

void Dropdown::closeAll() {
    if (!s_openPopup) return;
    Node* p = s_openPopup;
    s_openPopup = nullptr;
    p->removeFromParent();
}

void Dropdown::open() {
    closeAll();
    Scene* scene = getScene();
    if (!scene) return;
    const float rowH = 118.0f;
    const float pad = 26.0f;
    const float strip = windowPanelStrip();
    float width = _contentSize.width;
    Label* probe = Label::createWithTTF("", kFontHeading, 52.0f);
    for (const std::string& o : _options) {
        probe->setString(o);
        width = std::max(width, probe->getContentSize().width + 200.0f);
    }
    const float height = strip + pad * 2.0f + rowH * _options.size();

    Node* layer = Node::create();
    layer->setContentSize(Director::getInstance()->getVisibleSize());
    scene->addChild(layer, 5000);
    s_openPopup = layer;

    // Below the button (or above when it doesn't fit), left-aligned with it.
    const Vec2 bl = convertToWorldSpace(Vec2::ZERO);
    const Vec2 tl = convertToWorldSpace(Vec2(0.0f, _contentSize.height));
    float y = bl.y - 16.0f - height;
    if (y < Director::getInstance()->getVisibleOrigin().y + 20.0f) y = tl.y + 16.0f;
    Sprite* panel = windowPanel(Size(width, height));
    panel->setAnchorPoint(Vec2::ZERO);
    panel->setPosition(bl.x, y);
    layer->addChild(panel);

    const Rect panelRect(bl.x, y, width, height);
    for (size_t i = 0; i < _options.size(); ++i) {
        Style row;
        row.color = Color3B(60, 60, 70);
        row.opacity = (int)i == _selected ? 40 : 0;
        row.radius = 20.0f;
        row.textColor = (int)i == _selected ? kBlue : kInk;
        Button* b = Button::create(_options[i], Size(width - pad * 2.0f, rowH - 12.0f), row, 52.0f);
        b->label()->setAnchorPoint(Vec2(0.0f, 0.5f));
        b->setPosition(bl.x + width * 0.5f, y + height - strip - pad - rowH * (i + 0.5f));
        b->label()->setPositionX(40.0f);
        const int index = (int)i;
        RefPtr<Dropdown> self(this);
        b->setCallback([self, index]() {
            Dropdown* d = self.get();
            closeAll();
            const bool changed = index != d->_selected;
            d->setSelectedIndex(index);
            if (changed && d->onSelect) d->onSelect(index);
        });
        layer->addChild(b);
    }

    // Any touch outside the menu closes it (and is swallowed).
    auto catcher = EventListenerTouchOneByOne::create();
    catcher->setSwallowTouches(true);
    catcher->onTouchBegan = [](Touch*, Event*) { return true; };
    catcher->onTouchEnded = [panelRect](Touch* t, Event*) {
        if (!panelRect.containsPoint(t->getLocation())) closeAll();
    };
    layer->getEventDispatcher()->addEventListenerWithSceneGraphPriority(catcher, layer);
    layer->setOnExitCallback([layer]() {
        if (s_openPopup == layer) s_openPopup = nullptr;
    });
    layer->setCascadeOpacityEnabled(true);
    panel->setOpacity(0);
    panel->runAction(FadeIn::create(0.12f));
}

// ---- SearchField ------------------------------------------------------------------------------

SearchField* SearchField::create(const Size& size, const std::string& placeholder) {
    SearchField* f = new (std::nothrow) SearchField();
    if (f && f->init(size, placeholder)) {
        f->autorelease();
        return f;
    }
    delete f;
    return nullptr;
}

bool SearchField::init(const Size& size, const std::string& placeholder) {
    if (!Node::init()) return false;
    _placeholder = placeholder;
    setContentSize(size);
    setAnchorPoint(Vec2(0.5f, 0.5f));
    const float r = size.height * 0.5f;
    // A thick white rim around a white field, like the menus' white-bordered buttons.
    auto* rim = roundedRect(Size(size.width + 20.0f, size.height + 20.0f), r + 10.0f, Color3B::WHITE, 255);
    rim->setAnchorPoint(Vec2::ZERO);
    rim->setPosition(-10.0f, -10.0f);
    addChild(rim);
    auto* bg = roundedRect(size, r, Color3B(242, 242, 246), 255);
    bg->setAnchorPoint(Vec2::ZERO);
    addChild(bg);
    _icon = iconSprite("search", size.height * 0.44f);
    _icon->setColor(Color3B(140, 142, 158));
    _icon->setPosition(r * 1.05f, size.height * 0.5f);
    addChild(_icon);
    _textLeft = r * 1.05f + size.height * 0.40f;
    _label = Label::createWithTTF("", kFontBody, size.height * 0.40f);
    _label->setAnchorPoint(Vec2(0.0f, 0.5f));
    _label->setPosition(_textLeft, size.height * 0.5f);
    addChild(_label);
    _clear = iconSprite("clear", size.height * 0.42f);
    _clear->setColor(Color3B(160, 162, 176));
    _clear->setPosition(size.width - r * 0.95f, size.height * 0.5f);
    addChild(_clear);
    _caret = Sprite::create();
    _caret->setTextureRect(Rect(0, 0, 6.0f, size.height * 0.5f));
    _caret->setColor(kBlue);
    _caret->setAnchorPoint(Vec2(0.0f, 0.5f));
    _caret->setVisible(false);
    addChild(_caret);

    auto touch = EventListenerTouchOneByOne::create();
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (!isShown(this)) return false;
        const Vec2 p = convertToNodeSpace(t->getLocation());
        if (Rect(0, 0, _contentSize.width, _contentSize.height).containsPoint(p)) {
            if (_clear->isVisible() && p.x > _contentSize.width - _contentSize.height) {
                setText("");
                if (onChange) onChange();
            }
            focus();
            return true;
        }
        return false;
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
    refresh();
    return true;
}

void SearchField::setText(const std::string& text) {
    _text = text;
    refresh();
}

void SearchField::setPlaceholder(const std::string& placeholder) {
    _placeholder = placeholder;
    refresh();
}

void SearchField::focus() {
    if (!_focused) attachWithIME();
}

void SearchField::didAttachWithIME() {
    _focused = true;
    _caret->stopAllActions();
    _caret->setVisible(true);
    _caret->runAction(RepeatForever::create(Sequence::create(DelayTime::create(0.5f), Hide::create(),
                                                             DelayTime::create(0.4f), Show::create(), nullptr)));
    refresh();
}

void SearchField::didDetachWithIME() {
    _focused = false;
    _caret->stopAllActions();
    _caret->setVisible(false);
    refresh();
}

void SearchField::insertText(const char* text, size_t len) {
    std::string in(text, len);
    in.erase(std::remove_if(in.begin(), in.end(), [](char c) { return c == '\n' || c == '\r' || c == '\t'; }),
             in.end());
    if (in.empty()) return;
    if (StringUtils::getCharacterCountInUTF8String(_text + in) > 64) return;
    _text += in;
    refresh();
    if (onChange) onChange();
}

void SearchField::deleteBackward() {
    if (_text.empty()) return;
    std::u32string u32;
    if (StringUtils::UTF8ToUTF32(_text, u32) && !u32.empty()) {
        u32.pop_back();
        StringUtils::UTF32ToUTF8(u32, _text);
    } else {
        _text.pop_back();
    }
    refresh();
    if (onChange) onChange();
}

void SearchField::paste() {
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
    auto* view = dynamic_cast<GLViewImpl*>(Director::getInstance()->getOpenGLView());
    if (!view) return;
    const char* clip = glfwGetClipboardString(view->getWindow());
    if (!clip) return;
    std::string s = clip;
    for (char& c : s)
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    insertText(s.c_str(), s.size());
#endif
}

void SearchField::refresh() {
    const float maxW = _contentSize.width - _textLeft - _contentSize.height * 0.95f;
    _clear->setVisible(!_text.empty());
    if (_text.empty()) {
        _label->setString(_placeholder);
        _label->setColor(Color3B(150, 152, 168));
    } else {
        _label->setColor(Color3B(40, 40, 52));
        _label->setString(_text);
        // Too long: show the tail, like a scrolled text field.
        if (_label->getContentSize().width > maxW) {
            std::u32string u32;
            StringUtils::UTF8ToUTF32(_text, u32);
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
    _caret->setPosition(_textLeft + tw + 4.0f, _contentSize.height * 0.5f);
}

// ---- spinner ----------------------------------------------------------------------------------

Sprite* createSpinner(float size, const Color3B& color) {
    Sprite* s = iconSprite("spinner", size);
    s->setColor(color);
    s->runAction(RepeatForever::create(RotateBy::create(0.9f, 360.0f)));
    return s;
}

}  // namespace ui
}  // namespace online
