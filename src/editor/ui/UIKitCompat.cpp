#include "UIKitCompat.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <unordered_map>

#include "platform/common/AppleImage.h"
#include "platform/common/EditorAssets.h"
#include "platform/common/IOSBundle.h"

USING_NS_CC;

namespace uikit {

// ================================================================================================
// geometry / colour / conversions
// ================================================================================================

float pointsToDesign() { return EditorAssets::pointsToDesign(); }

Size windowSize()
{
    const Size visible = Director::getInstance()->getVisibleSize();
    const float k = pointsToDesign();
    return Size(visible.width / k, visible.height / k);
}

float notchOffset() { return 0.0f; }

static GLubyte toByte(double c)
{
    if (!(c > 0.0)) return 0;
    if (c >= 1.0) return 255;
    return static_cast<GLubyte>(std::lround(c * 255.0));
}

Color4B color(double r, double g, double b, double a) { return Color4B(toByte(r), toByte(g), toByte(b), toByte(a)); }
Color4B whiteColor() { return Color4B(255, 255, 255, 255); }
Color4B blackColor() { return Color4B(0, 0, 0, 255); }
Color4B grayColor() { return color(0.5, 0.5, 0.5, 1.0); }
Color4B clearColor() { return Color4B(0, 0, 0, 0); }

int fcvtzs(double value)
{
    if (std::isnan(value)) return 0;
    if (value >= 2147483647.0) return std::numeric_limits<int>::max();
    if (value <= -2147483648.0) return std::numeric_limits<int>::min();
    return static_cast<int>(value);
}

unsigned int fcvtzu(double value)
{
    if (std::isnan(value) || value <= 0.0) return 0;
    if (value >= 4294967295.0) return std::numeric_limits<unsigned int>::max();
    return static_cast<unsigned int>(value);
}

// ================================================================================================
// NSString helpers
// ================================================================================================

std::string capitalizedString(const std::string& s)
{
    std::string out(s);
    bool startOfWord = true;
    for (char& c : out)
    {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u >= 0x80)
        {
            startOfWord = false;  // part of a multi-byte letter
        }
        else if (std::isalnum(u))
        {
            if (std::isalpha(u)) c = static_cast<char>(startOfWord ? std::toupper(u) : std::tolower(u));
            startOfWord = false;
        }
        else
        {
            startOfWord = true;
        }
    }
    return out;
}

std::string lowercaseString(const std::string& s)
{
    std::string out(s);
    for (char& c : out)
    {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x80) c = static_cast<char>(std::tolower(u));
    }
    return out;
}

std::string trimmedString(const std::string& s)
{
    const char* ws = " \t\n\r\v\f";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return std::string();
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

// ================================================================================================
// fonts / labels
// ================================================================================================

std::string fontFile(bool bold)
{
    static const char* const kBold[] = {"C:/Windows/Fonts/arialbd.ttf", "/Library/Fonts/Arial Bold.ttf",
                                        "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf"};
    static const char* const kRegular[] = {"C:/Windows/Fonts/arial.ttf", "/Library/Fonts/Arial.ttf",
                                           "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf"};
    static std::string cached[2];
    static bool resolved[2] = {false, false};
    const int i = bold ? 1 : 0;
    if (!resolved[i])
    {
        resolved[i] = true;
        cached[i] = "Arial";
        for (const char* path : (bold ? kBold : kRegular))
        {
            if (FileUtils::getInstance()->isFileExist(path))
            {
                cached[i] = path;
                break;
            }
        }
    }
    return cached[i];
}

ui::Text* makeLabel(const std::string& text, bool bold, float pointSize, int alignment)
{
    ui::Text* label = ui::Text::create(text, fontFile(bold), pointSize * pointsToDesign());
    label->ignoreContentAdaptWithSize(false);
    label->setTextVerticalAlignment(TextVAlignment::CENTER);
    label->setTextHorizontalAlignment(alignment == 1 ? TextHAlignment::CENTER
                                      : alignment == 2 ? TextHAlignment::RIGHT
                                                       : TextHAlignment::LEFT);
    label->setTextColor(blackColor());
    return label;
}

void setLabelShadow(ui::Text* label, const Color4B& shadowColor, const Size& offset)
{
    const float k = pointsToDesign();
    label->enableShadow(shadowColor, Size(offset.width * k, -offset.height * k), 0);
}

void fitLabelWidth(ui::Text* label, float pointSize)
{
    const float k = pointsToDesign();
    const float width = label->getContentSize().width;
    float size = pointSize;
    label->setFontSize(size * k);
    if (width <= 0.0f) return;
    // Measure unconstrained: a temporary label with the same font.
    while (size > 6.0f)
    {
        Label* probe = Label::createWithTTF(label->getString(), label->getFontName(), size * k);
        if (probe == nullptr) probe = Label::createWithSystemFont(label->getString(), label->getFontName(), size * k);
        float widest = 0.0f;
        if (probe != nullptr)
        {
            widest = probe->getContentSize().width;
        }
        if (widest <= width) break;
        size -= 1.0f;
    }
    label->setFontSize(size * k);
}

// ================================================================================================
// images
// ================================================================================================

static std::string registerTexture(const std::string& key, Texture2D* texture)
{
    // SpriteFrame rects are in points (pixels / content scale factor), so the whole texture is its
    // content size, not its pixel size.
    SpriteFrame* frame = SpriteFrame::createWithTexture(
        texture, Rect(Vec2::ZERO, texture->getContentSize()));
    SpriteFrameCache::getInstance()->addSpriteFrame(frame, key);
    return key;
}

std::string imageNamed(const std::string& name, float* scale)
{
    static std::map<std::string, std::pair<std::string, float>> cache;
    auto it = cache.find(name);
    if (it != cache.end())
    {
        if (scale) *scale = it->second.second;
        return it->second.first;
    }
    std::string result;
    float imageScale = 1.0f;
    if (openwheels::hasIOSBundle())
    {
        std::string base = name;
        std::string ext = ".png";
        const size_t dot = base.rfind('.');
        if (dot != std::string::npos)
        {
            ext = base.substr(dot);
            base = base.substr(0, dot);
        }
        const std::string candidates[2] = {base + "@2x" + ext, base + ext};
        for (int i = 0; i < 2 && result.empty(); ++i)
        {
            const std::string path = openwheels::iosBundlePath() + candidates[i];
            if (!FileUtils::getInstance()->isFileExist(path)) continue;
            const Data data = FileUtils::getInstance()->getDataFromFile(path);
            if (data.isNull()) continue;
            Texture2D* texture = nullptr;
            Image* image = openwheels::createImage(data);  // CgBI-aware (platform/common/AppleImage)
            if (image)
            {
                texture = new (std::nothrow) Texture2D();
                if (texture && texture->initWithImage(image))
                {
                    result = registerTexture("uikit/" + name, texture);
                    // frame units per iOS point: the file's scale over the content scale factor
                    // (frames measure pixels / content scale factor)
                    imageScale = (i == 0 ? 2.0f : 1.0f) / Director::getInstance()->getContentScaleFactor();
                }
                CC_SAFE_RELEASE(texture);
                image->release();
            }
        }
    }
    if (result.empty()) log("uikit: image %s not found in the iOS bundle", name.c_str());
    cache[name] = std::make_pair(result, imageScale);
    if (scale) *scale = imageScale;
    return result;
}

std::string generatedImage(const std::string& key, int width, int height,
                           const std::function<Color4B(int x, int y)>& pixel)
{
    const std::string frameKey = "uikit/gen/" + key;
    if (SpriteFrameCache::getInstance()->getSpriteFrameByName(frameKey) != nullptr) return frameKey;
    std::vector<unsigned char> rgba(static_cast<size_t>(width) * height * 4);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const Color4B c = pixel(x, y);
            unsigned char* out = rgba.data() + (static_cast<size_t>(y) * width + x) * 4;
            // premultiplied, like every other texture cocos2d-x loads
            out[0] = static_cast<unsigned char>(c.r * c.a / 255);
            out[1] = static_cast<unsigned char>(c.g * c.a / 255);
            out[2] = static_cast<unsigned char>(c.b * c.a / 255);
            out[3] = c.a;
        }
    }
    Image* image = new (std::nothrow) Image();
    Texture2D* texture = new (std::nothrow) Texture2D();
    if (image->initWithRawData(rgba.data(), static_cast<ssize_t>(rgba.size()), width, height, 8, true) &&
        texture->initWithImage(image))
    {
        registerTexture(frameKey, texture);
    }
    texture->release();
    image->release();
    return frameKey;
}

// ================================================================================================
// UIButton helpers
// ================================================================================================

void skinButton(ui::Button* button, const std::string& prefix)
{
    if (button == nullptr) return;
    float scale = 1.0f;
    const std::string normal = imageNamed(prefix + "Button.png", &scale);
    const std::string highlight = imageNamed(prefix + "ButtonHighlight.png");
    if (normal.empty()) return;
    const Size size = button->getContentSize();
    button->loadTextures(normal, highlight.empty() ? normal : highlight, "", ui::Widget::TextureResType::PLIST);
    button->setScale9Enabled(true);
    const Size px = button->getNormalTextureSize();
    const float cap = 4.0f * scale;  // 4 pt caps in image pixels
    button->setCapInsets(Rect(cap, cap, std::max(1.0f, px.width - cap * 2), std::max(1.0f, px.height - cap * 2)));
    button->ignoreContentAdaptWithSize(false);
    if (size.width > 0.0f && size.height > 0.0f) button->setContentSize(size);  // keep the placed frame
    button->setZoomScale(0.0f);
    button->setPressedActionEnabled(false);
}

void setButtonTitle(ui::Button* button, const std::string& title, float pointSize, const Color4B& c)
{
    button->setTitleFontName(fontFile(false));
    button->setTitleFontSize(pointSize * pointsToDesign());
    button->setTitleText(title);
    button->setTitleColor(Color3B(c.r, c.g, c.b));
    if (button->getTitleRenderer()) button->getTitleRenderer()->setOpacity(c.a);
}

// ================================================================================================
// View
// ================================================================================================

View::View() : _frame(Rect::ZERO) {}

View::~View() {}

View* View::create(const Rect& frame)
{
    View* view = new (std::nothrow) View();
    if (view && view->initWithFrame(frame))
    {
        view->autorelease();
        return view;
    }
    delete view;
    return nullptr;
}

bool View::initWithFrame(const Rect& frame)
{
    if (!ui::Layout::init()) return false;
    _frame = frame;
    setAnchorPoint(Vec2::ZERO);
    setContentSize(frame.size * pointsToDesign());
    setBackGroundColorType(BackGroundColorType::NONE);
    setTouchEnabled(true);
    setSwallowTouches(true);
    return true;
}

void View::setFrame(const Rect& frame)
{
    _frame = frame;
    setContentSize(frame.size * pointsToDesign());
    if (View* parentView = superview()) parentView->setSubviewFrame(this, frame);
    layoutSubviews();
}

Rect View::bounds() const { return Rect(0, 0, _frame.size.width, _frame.size.height); }

void View::setBackgroundColor(const Color4B& c)
{
    if (c.a == 0)
    {
        setBackGroundColorType(BackGroundColorType::NONE);
        return;
    }
    setBackGroundColorType(BackGroundColorType::SOLID);
    setBackGroundColor(Color3B(c.r, c.g, c.b));
    setBackGroundColorOpacity(c.a);
}

void View::setUserInteractionEnabled(bool enabled)
{
    setTouchEnabled(enabled);
    setSwallowTouches(enabled);
}

void View::addSubview(View* view)
{
    if (view == nullptr) return;
    addSubview(view, view->frame());
}

void View::addSubview(Node* node, const Rect& frame)
{
    if (node == nullptr) return;
    if (node->getParent() != contentNode())
    {
        if (node->getParent() != nullptr) node->removeFromParentAndCleanup(false);
        contentNode()->addChild(node);
    }
    setSubviewFrame(node, frame);
}

void View::setSubviewFrame(Node* node, const Rect& frame)
{
    bool found = false;
    for (auto& entry : _subviewFrames)
    {
        if (entry.first.get() == node)
        {
            entry.second = frame;
            found = true;
            break;
        }
    }
    if (!found) _subviewFrames.emplace_back(RefPtr<Node>(node), frame);
    if (View* view = dynamic_cast<View*>(node))
    {
        if (!view->_frame.equals(frame))
        {
            view->_frame = frame;
            view->setContentSize(frame.size * pointsToDesign());
            view->layoutSubviews();
        }
    }
    placeSubview(node, frame);
}

Rect View::subviewFrame(Node* node) const
{
    for (const auto& entry : _subviewFrames)
    {
        if (entry.first.get() == node) return entry.second;
    }
    return Rect::ZERO;
}

View* View::superview() const
{
    Node* p = const_cast<View*>(this)->getParent();
    for (int i = 0; p != nullptr && i < 3; ++i, p = p->getParent())
    {
        if (View* view = dynamic_cast<View*>(p)) return view;
    }
    return nullptr;
}

void View::removeFromSuperview()
{
    if (getParent() == nullptr) return;
    removeFromParentAndCleanup(true);
}

void View::layoutSubviews()
{
    Node* content = contentNode();
    for (auto it = _subviewFrames.begin(); it != _subviewFrames.end();)
    {
        if (it->first->getParent() != content)
        {
            it = _subviewFrames.erase(it);
            continue;
        }
        placeSubview(it->first.get(), it->second);
        ++it;
    }
}

void View::removeChild(Node* child, bool cleanup)
{
    RefPtr<Node> keep(child);  // the frame list may hold the last reference
    for (auto it = _subviewFrames.begin(); it != _subviewFrames.end(); ++it)
    {
        if (it->first.get() == child)
        {
            ui::Layout::removeChild(child, cleanup);
            _subviewFrames.erase(it);
            return;
        }
    }
    ui::Layout::removeChild(child, cleanup);
}

Node* View::contentNode() { return this; }

float View::contentHeightForLayout() const { return _frame.size.height; }

void View::placeSubview(Node* node, const Rect& frame)
{
    const float k = pointsToDesign();
    const float h = contentHeightForLayout();
    if (dynamic_cast<View*>(node) != nullptr)
    {
        node->setAnchorPoint(Vec2::ZERO);
        node->setPosition(frame.origin.x * k, (h - frame.origin.y - frame.size.height) * k);
        return;
    }
    node->setAnchorPoint(Vec2(0.5f, 0.5f));
    node->setPosition((frame.origin.x + frame.size.width * 0.5f) * k,
                      (h - frame.origin.y - frame.size.height * 0.5f) * k);
    if (ui::Widget* widget = dynamic_cast<ui::Widget*>(node))
    {
        const float sx = widget->getScaleX() != 0.0f ? widget->getScaleX() : 1.0f;
        const float sy = widget->getScaleY() != 0.0f ? widget->getScaleY() : 1.0f;
        widget->ignoreContentAdaptWithSize(false);
        widget->setContentSize(Size(frame.size.width * k / sx, frame.size.height * k / sy));
    }
}

// ================================================================================================
// ScrollView
// ================================================================================================

ScrollView::ScrollView()
    : _scroller(nullptr), _scrollContentSize(Size::ZERO), _insetTop(0), _insetLeft(0), _insetBottom(0), _insetRight(0)
{
}

ScrollView* ScrollView::create(const Rect& frame)
{
    ScrollView* view = new (std::nothrow) ScrollView();
    if (view && view->initWithFrame(frame))
    {
        view->autorelease();
        return view;
    }
    delete view;
    return nullptr;
}

bool ScrollView::initWithFrame(const Rect& frame)
{
    if (!View::initWithFrame(frame)) return false;
    _scroller = ui::ScrollView::create();
    _scroller->setDirection(ui::ScrollView::Direction::VERTICAL);
    _scroller->setBounceEnabled(true);
    _scroller->setScrollBarEnabled(true);
    _scroller->setAnchorPoint(Vec2::ZERO);
    _scroller->setPosition(Vec2::ZERO);
    _scroller->setContentSize(getContentSize());
    _scroller->setSwallowTouches(true);
    ui::Layout::addChild(_scroller);
    applyScrollContentSize();
    return true;
}

void ScrollView::setScrollContentSize(const Size& size)
{
    _scrollContentSize = size;
    applyScrollContentSize();
}

void ScrollView::applyScrollContentSize()
{
    if (_scroller == nullptr) return;
    const float k = pointsToDesign();
    // Keep the visible top where it was (UIScrollView keeps contentOffset).
    const Vec2 offset = contentOffset();
    _scroller->setContentSize(getContentSize());
    const float w = std::max(_scrollContentSize.width + _insetLeft + _insetRight, _frame.size.width);
    const float h = std::max(_scrollContentSize.height + _insetTop + _insetBottom, _frame.size.height);
    _scroller->setInnerContainerSize(Size(w * k, h * k));
    layoutSubviews();
    setContentOffset(offset, false);
}

Vec2 ScrollView::contentOffset() const
{
    if (_scroller == nullptr) return Vec2::ZERO;
    const float k = pointsToDesign();
    const Vec2 inner = _scroller->getInnerContainerPosition();
    const float innerH = _scroller->getInnerContainerSize().height;
    const float viewH = _scroller->getContentSize().height;
    return Vec2(-inner.x / k, (inner.y - (viewH - innerH)) / k);
}

void ScrollView::setContentOffset(const Vec2& offset, bool /*animated*/)
{
    if (_scroller == nullptr) return;
    const float k = pointsToDesign();
    const float innerH = _scroller->getInnerContainerSize().height;
    const float viewH = _scroller->getContentSize().height;
    float y = (viewH - innerH) + offset.y * k;
    y = std::min(0.0f, std::max(viewH - innerH, y));
    _scroller->setInnerContainerPosition(Vec2(-offset.x * k, y));
}

void ScrollView::setContentInset(float top, float left, float bottom, float right)
{
    _insetTop = top;
    _insetLeft = left;
    _insetBottom = bottom;
    _insetRight = right;
    applyScrollContentSize();
}

Rect ScrollView::contentInset() const { return Rect(_insetTop, _insetLeft, _insetBottom, _insetRight); }

void ScrollView::setScrollIndicatorInsets(float, float, float, float) {}

void ScrollView::setScrollEnabled(bool enabled)
{
    if (_scroller) _scroller->setTouchEnabled(enabled);
}

Node* ScrollView::contentNode() { return _scroller ? static_cast<Node*>(_scroller->getInnerContainer()) : this; }

float ScrollView::contentHeightForLayout() const
{
    if (_scroller == nullptr) return _frame.size.height;
    return _scroller->getInnerContainerSize().height / pointsToDesign();
}

// ================================================================================================
// Slider / Switch (system-drawn controls on iOS)
// ================================================================================================

namespace {
const float kThumbRadius = 14.0f;  // points (iOS 7+ UISlider thumb 28 pt)
const float kThumbSlop = 10.0f;

Color4F toColor4F(const Color4B& c) { return Color4F(c); }

void drawPill(DrawNode* draw, const Vec2& from, const Vec2& to, float radius, const Color4F& color)
{
    if (to.x - from.x > 0.0f) draw->drawSolidRect(Vec2(from.x, from.y - radius), Vec2(to.x, to.y + radius), color);
    draw->drawSolidCircle(from, radius, 0.0f, 32, color);
    draw->drawSolidCircle(to, radius, 0.0f, 32, color);
}

void drawKnob(DrawNode* draw, const Vec2& center, float radius)
{
    const float k = pointsToDesign();
    draw->drawSolidCircle(center + Vec2(0.0f, -1.5f * k), radius + 0.5f * k, 0.0f, 40, Color4F(0, 0, 0, 0.12f));
    draw->drawSolidCircle(center, radius + 0.5f * k, 0.0f, 40, Color4F(0, 0, 0, 0.08f));
    draw->drawSolidCircle(center, radius, 0.0f, 40, Color4F::WHITE);
}
}  // namespace

Slider::Slider()
    : _draw(nullptr), _minimumTrackTintColor(color(0.0, 0.478, 1.0, 1.0)), _value(0.0f), _touchStartValue(0.0f),
      _touchStartX(0.0f), _tracking(false), _sliderEnabled(true)
{
}

Slider* Slider::create()
{
    Slider* slider = new (std::nothrow) Slider();
    if (slider && slider->init())
    {
        slider->autorelease();
        return slider;
    }
    delete slider;
    return nullptr;
}

bool Slider::init()
{
    if (!ui::Widget::init()) return false;
    _draw = DrawNode::create();
    addProtectedChild(_draw);
    setTouchEnabled(true);
    setSwallowTouches(true);
    setPropagateTouchEvents(false);
    setCascadeOpacityEnabled(true);
    ignoreContentAdaptWithSize(false);
    setContentSize(Size(100.0f, 2 * kThumbRadius) * pointsToDesign());
    return true;
}

void Slider::setValue(float value)
{
    if (std::isnan(value)) value = 0.0f;
    _value = std::min(1.0f, std::max(0.0f, value));
    redraw();
}

void Slider::setMinimumTrackTintColor(const Color4B& c)
{
    _minimumTrackTintColor = c;
    redraw();
}

void Slider::setSliderEnabled(bool enabled)
{
    _sliderEnabled = enabled;
    setOpacity(enabled ? 255 : 128);
}

float Slider::thumbCenterX() const
{
    const float r = kThumbRadius * pointsToDesign();
    return r + _value * std::max(0.0f, getContentSize().width - 2.0f * r);
}

void Slider::onSizeChanged()
{
    ui::Widget::onSizeChanged();
    redraw();
}

void Slider::redraw()
{
    if (_draw == nullptr) return;
    _draw->clear();
    const float k = pointsToDesign();
    const Size size = getContentSize();
    const float y = size.height * 0.5f;
    const float r = kThumbRadius * k;
    const float trackR = 1.0f * k;
    const float x = thumbCenterX();
    drawPill(_draw, Vec2(r, y), Vec2(std::max(r, size.width - r), y), trackR,
             Color4F(0.717f, 0.717f, 0.717f, 1.0f));
    drawPill(_draw, Vec2(r, y), Vec2(x, y), trackR, toColor4F(_minimumTrackTintColor));
    drawKnob(_draw, Vec2(x, y), r);
}

bool Slider::onTouchBegan(Touch* touch, cocos2d::Event* event)
{
    if (!_sliderEnabled) return false;
    const bool pass = ui::Widget::onTouchBegan(touch, event);
    if (!_hitted) return pass;
    const Vec2 local = convertToNodeSpace(touch->getLocation());
    const float k = pointsToDesign();
    if (std::fabs(local.x - thumbCenterX()) > (kThumbRadius + kThumbSlop) * k) return false;
    _tracking = true;
    _touchStartValue = _value;
    _touchStartX = local.x;
    if (_callback) _callback(this, Event::TouchDown);
    return true;
}

void Slider::onTouchMoved(Touch* touch, cocos2d::Event* event)
{
    ui::Widget::onTouchMoved(touch, event);
    if (!_tracking) return;
    const Vec2 local = convertToNodeSpace(touch->getLocation());
    const float r = kThumbRadius * pointsToDesign();
    const float length = std::max(1.0f, getContentSize().width - 2.0f * r);
    const float before = _value;
    setValue(_touchStartValue + (local.x - _touchStartX) / length);
    if (_value != before && _callback) _callback(this, Event::ValueChanged);
}

void Slider::onTouchEnded(Touch* touch, cocos2d::Event* event)
{
    ui::Widget::onTouchEnded(touch, event);
    if (!_tracking) return;
    _tracking = false;
    if (_callback) _callback(this, Event::TouchUp);
}

void Slider::onTouchCancelled(Touch* touch, cocos2d::Event* event)
{
    ui::Widget::onTouchCancelled(touch, event);
    if (!_tracking) return;
    _tracking = false;
    if (_callback) _callback(this, Event::TouchUp);
}

Switch::Switch() : _draw(nullptr), _onTintColor(color(0.298, 0.851, 0.392, 1.0)), _on(false), _switchEnabled(true) {}

Switch* Switch::create()
{
    Switch* control = new (std::nothrow) Switch();
    if (control && control->init())
    {
        control->autorelease();
        return control;
    }
    delete control;
    return nullptr;
}

bool Switch::init()
{
    if (!ui::Widget::init()) return false;
    _draw = DrawNode::create();
    addProtectedChild(_draw);
    setTouchEnabled(true);
    setSwallowTouches(true);
    setCascadeOpacityEnabled(true);
    ignoreContentAdaptWithSize(false);
    setContentSize(Size(kWidth, kHeight) * pointsToDesign());
    return true;
}

void Switch::setOn(bool on, bool /*animated*/)
{
    _on = on;
    redraw();
}

void Switch::setOnTintColor(const Color4B& c)
{
    _onTintColor = c;
    redraw();
}

void Switch::setSwitchEnabled(bool enabled)
{
    _switchEnabled = enabled;
    setOpacity(enabled ? 255 : 128);
}

void Switch::onSizeChanged()
{
    ui::Widget::onSizeChanged();
    redraw();
}

void Switch::releaseUpEvent()
{
    ui::Widget::releaseUpEvent();
    if (!_switchEnabled) return;
    setOn(!_on, true);
    if (_callback) _callback(this);
}

void Switch::redraw()
{
    if (_draw == nullptr) return;
    _draw->clear();
    const float k = pointsToDesign();
    // Draw at the intrinsic size, centred in the content (UISwitch ignores its frame size).
    const Size size = getContentSize();
    const float w = kWidth * k, h = kHeight * k;
    const Vec2 o((size.width - w) * 0.5f, (size.height - h) * 0.5f);
    const float r = h * 0.5f;
    const Vec2 left = o + Vec2(r, r), right = o + Vec2(w - r, r);
    if (_on)
    {
        drawPill(_draw, left, right, r, toColor4F(_onTintColor));
    }
    else
    {
        drawPill(_draw, left, right, r, Color4F(0.898f, 0.898f, 0.898f, 1.0f));
        drawPill(_draw, left, right, r - 1.5f * k, Color4F::WHITE);
    }
    drawKnob(_draw, _on ? right : left, r - 2.0f * k);
}

// ================================================================================================
// window
// ================================================================================================

View* window()
{
    static RefPtr<View> s_window;
    Scene* scene = Director::getInstance()->getRunningScene();
    if (scene == nullptr) return nullptr;
    if (!s_window || s_window->getParent() != scene)
    {
        if (s_window && s_window->getParent() != nullptr) s_window->removeFromParentAndCleanup(true);
        const Size size = windowSize();
        s_window = View::create(Rect(0, 0, size.width, size.height));
        s_window->setUserInteractionEnabled(false);  // touches pass through where no panel is
        s_window->setPosition(Director::getInstance()->getVisibleOrigin());
        scene->addChild(s_window.get(), kWindowZOrder);
    }
    return s_window.get();
}

// ================================================================================================
// NotificationCenter
// ================================================================================================

namespace {
struct Observation
{
    std::string name;
    void* objectFilter;
    EventListenerCustom* listener;
};
std::unordered_map<const void*, std::vector<Observation>>& observations()
{
    static std::unordered_map<const void*, std::vector<Observation>> map;
    return map;
}
void* s_currentUserInfo = nullptr;
}  // namespace

void NotificationCenter::postNotification(const std::string& name, void* object, void* userInfo)
{
    void* const savedUserInfo = s_currentUserInfo;
    s_currentUserInfo = userInfo;
    EventCustom event(name);
    event.setUserData(object);
    Director::getInstance()->getEventDispatcher()->dispatchEvent(&event);
    s_currentUserInfo = savedUserInfo;
}

void NotificationCenter::addObserver(const void* owner, const std::string& name, void* objectFilter, Callback callback)
{
    EventListenerCustom* listener = EventListenerCustom::create(name, [objectFilter, callback](EventCustom* event) {
        void* object = event->getUserData();
        if (objectFilter != nullptr && object != objectFilter) return;
        callback(object, s_currentUserInfo);
    });
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(listener, 1);
    observations()[owner].push_back(Observation{name, objectFilter, listener});
}

void NotificationCenter::removeObserver(const void* owner)
{
    auto it = observations().find(owner);
    if (it == observations().end()) return;
    std::vector<Observation> list;
    list.swap(it->second);
    observations().erase(it);
    for (const Observation& o : list) Director::getInstance()->getEventDispatcher()->removeEventListener(o.listener);
}

void NotificationCenter::removeObserver(const void* owner, const std::string& name, void* objectFilter)
{
    auto it = observations().find(owner);
    if (it == observations().end()) return;
    std::vector<Observation>& list = it->second;
    for (auto o = list.begin(); o != list.end();)
    {
        // removeObserver:name:object: with object nil removes every object for that name.
        if (o->name == name && (objectFilter == nullptr || o->objectFilter == objectFilter))
        {
            Director::getInstance()->getEventDispatcher()->removeEventListener(o->listener);
            o = list.erase(o);
        }
        else
        {
            ++o;
        }
    }
    if (list.empty()) observations().erase(it);
}

}  // namespace uikit
