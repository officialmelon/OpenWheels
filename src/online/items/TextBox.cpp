// ONLINE (PC addition): see TextBox.h. Port of com.totaljerkface.game.level.userspecials.TextBox.
#include "online/items/TextBox.h"

#include <algorithm>
#include <cmath>

#include "cocos2d.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "online/FlashRuntime.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(16, [] { return (LevelItem*)new (std::nothrow) TextBox(); },
                               FlashSpecialUse::Missing, /*groupable*/ true);

// TextBoxRef.getFontName / getFontBold: 1 Helvetica, 2 Helvetica Med, 3 Helvetica bold,
// 4 Clarendon, 5 Clarendon bold (fonts embedded in the SWF, extracted at build time).
std::string fontFile(int font)
{
    static const char* const names[] = {"helvetica", "helvetica", "helvetica_med", "helvetica_bold",
                                        "clarendon", "clarendon_bold"};
    return std::string("generated/flash/fonts/") + names[std::max(1, std::min(5, font))] + ".ttf";
}

TextHAlignment alignment(int align)
{
    return align == 2 ? TextHAlignment::CENTER : align == 3 ? TextHAlignment::RIGHT : TextHAlignment::LEFT;
}

}  // namespace

TextBox::~TextBox()
{
    if (_root) _root->release();
}

bool TextBox::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupBody;
    (void)groupOffset;
    _inGroup = groupBody != nullptr || element->stringAttribute("fg") != nullptr;
    _x = num(element, "p0", 0.0f);
    _y = num(element, "p1", 0.0f);
    const float angle = num(element, "p2", 0.0f);
    const int color = inum(element, "p3", 0) & 0xffffff;
    const int font = std::max(1, std::min(5, inum(element, "p4", 2)));
    const int size = std::max(10, std::min(100, inum(element, "p5", 15)));
    const int align = std::max(1, std::min(3, inum(element, "p6", 1)));
    std::string caption = text(element, "p7");
    const float opacity = std::max(0.0f, std::min(100.0f, num(element, "p8", 100.0f)));
    // Flash text uses CR (sometimes CR LF) for line breaks.
    for (size_t pos; (pos = caption.find("\r\n")) != std::string::npos;) caption.erase(pos, 1);
    std::replace(caption.begin(), caption.end(), '\r', '\n');

    const float ppf = pointsPerFlashPx();
    const float wanted = size * ppf;                  // em size in points
    const float ttfSize = std::min(wanted, 72.0f);    // keep the glyph atlas small
    const std::string file = fontFile(font);
    if (FileUtils::getInstance()->isFileExist(file)) {
        TTFConfig config(file, ttfSize);
        _label = Label::createWithTTF(config, caption, alignment(align));
    }
    if (!_label) {
        const bool bold = font == 3 || font == 5;
        _label = Label::createWithSystemFont(caption, bold ? "Arial Bold" : "Arial", ttfSize,
                                             Size::ZERO, alignment(align));
    }
    _label->setTextColor(Color4B((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, 255));
    _label->setAnchorPoint(Vec2(0.0f, 1.0f));
    _label->setScale(wanted / ttfSize);
    _label->setPosition(Vec2(2.0f * ppf, -2.0f * ppf));  // TextField's 2 px gutter

    _root = Node::create();
    _root->retain();
    _root->addChild(_label);
    _root->setRotation(angle);  // both clockwise degrees
    Node* layer = element->stringAttribute("fg") ? flashForegroundLayer() : flashBackgroundLayer();
    if (layer) layer->addChild(_root);
    placeAt(_x, _y);

    // Before 1.69 text boxes have no opacity.
    if (flashVersion() > 1.68f) {
        _alpha = opacity * 0.01f;
        _visible = _alpha != 0.0f;
    }
    applyAlpha();
    return true;
}

void TextBox::placeAt(float xPx, float yPx)
{
    if (_inGroup || !_root) return;  // groups position it in paintWithOffsetPoints
    const b2Vec2 m = flashToWorld(xPx, yPx);
    const float ptm = getPtm();
    _root->setPosition(Vec2(m.x * ptm, m.y * ptm));
}

void TextBox::paintWithOffsetPoints(Vec2 offset, float rotation)
{
    // offset: the group-transformed position of (p0, p1); rotation: p2 minus the group angle
    // in the mobile convention (degrees, clockwise = negative body angle), as for signs.
    if (!_root) return;
    _root->setPosition(offset);
    _root->setRotation(rotation);
}

void TextBox::setOpacity(float opacity)
{
    _groupOpacity = opacity;
    applyAlpha();
}

void TextBox::applyAlpha()
{
    if (!_root) return;
    _root->setVisible(_visible);
    const float a = std::max(0.0f, std::min(1.0f, _alpha)) * std::max(0.0f, std::min(1.0f, _groupOpacity));
    _label->setOpacity((GLubyte)std::lround(a * 255.0f));
}

void TextBox::prepareForTrigger()
{
    if (flashVersion() < 1.69f) {
        _visible = false;
        applyAlpha();
    }
}

void TextBox::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    (void)trigger;
    (void)action;
    (void)properties;
    _visible = true;
    applyAlpha();
}

bool TextBox::triggerRepeatActivation(LevelItem* trigger, int action, std::vector<float> properties,
                                      float time)
{
    (void)trigger;
    // Flash runs this once per 30 Hz frame with an integer frame counter; we are called every
    // 60 Hz step with seconds. Only act when the Flash frame changes.
    const int frame = (int)std::floor(time * 30.0f + 1e-3f);
    int& last = _lastFrame[action == 1 ? 1 : 0];
    if (frame == last && time > 0.0f) return false;
    last = frame;
    const bool has0 = properties.size() > 0, has1 = properties.size() > 1, has2 = properties.size() > 2;
    if (action == 0) {  // change opacity: newOpacity (0..100), time (s)
        if (!has0 || !has1) {
            // Old levels (no action properties): Flash ends up with NaN, which shows the text.
            _visible = true;
            applyAlpha();
            return true;
        }
        const float target = properties[0] * 0.01f;
        const int frames = (int)std::lround(properties[1] * 30.0f);
        if (frame == frames || frame > frames) {
            _visible = target != 0.0f;
            _alpha = target;
            applyAlpha();
            return true;
        }
        _alpha = _alpha + (target - _alpha) / (float)(frames - frame);
        _visible = _alpha != 0.0f;
        applyAlpha();
        return false;
    }
    if (action == 1) {  // slide: time (s), newX, newY
        if (!has0 || !has1 || !has2) return true;
        const int frames = (int)std::lround(properties[0] * 30.0f);
        const float nx = properties[1], ny = properties[2];
        if (frame >= frames) {
            _x = nx;
            _y = ny;
            placeAt(_x, _y);
            return true;
        }
        _x = _x + (nx - _x) / (float)(frames - frame);
        _y = _y + (ny - _y) / (float)(frames - frame);
        placeAt(_x, _y);
        return false;
    }
    return true;
}

}  // namespace online
