#include "Special.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

#include "EditorSettings.h"
#include "FlashEditor.h"  // EDITOR (browser features, PC addition): trigger-action keys
#include "InputObject.h"
#include "SwitchInputObject.h"
#include "UIKitCompat.h"
#include "platform/common/EditorAssets.h"

USING_NS_CC;

const char* const Special::SHAPE_COUNT_UPDATE = "SHAPE_COUNT_UPDATE";
const char* const Special::ART_COUNT_UPDATE = "ART_COUNT_UPDATE";
const char* const Special::REF_UI_KEYS_WILL_CHANGE = "ref_ui_keys_will_change";
const char* const Special::REF_UI_KEYS_CHANGED = "ref_ui_keys_changed";

namespace {

// iOS: Session.sharedSession.ptmRatio after -[Session setSessionMode:2] on iPad.
float s_sessionPtmRatio = 72.0f;

// CGAffineTransform (double).
struct CGTransform
{
    double a, b, c, d, tx, ty;
};

// CGAffineTransformConcat(t1, t2) = t1 * t2.
CGTransform concat(const CGTransform& t1, const CGTransform& t2)
{
    return {t1.a * t2.a + t1.b * t2.c,  t1.a * t2.b + t1.b * t2.d,
            t1.c * t2.a + t1.d * t2.c,  t1.c * t2.b + t1.d * t2.d,
            t1.tx * t2.a + t1.ty * t2.c + t2.tx,  t1.tx * t2.b + t1.ty * t2.d + t2.ty};
}

// CGRectApplyAffineTransform: bounds of the four transformed corners of the standardised rect.
cg::Rect applyAffineTransform(const cg::Rect& rect, const CGTransform& t)
{
    if (cg::rectIsNull(rect))
    {
        return rect;
    }
    const cg::Rect r = cg::rectStandardize(rect);
    const double xs[2] = {r.origin.x, r.origin.x + r.size.width};
    const double ys[2] = {r.origin.y, r.origin.y + r.size.height};
    double minX = std::numeric_limits<double>::infinity(), maxX = -minX;
    double minY = minX, maxY = -minX;
    for (double px : xs)
    {
        for (double py : ys)
        {
            const double x = t.a * px + t.c * py + t.tx;
            const double y = t.b * px + t.d * py + t.ty;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }
    return cg::Rect(minX, minY, maxX - minX, maxY - minY);
}

// -[NSString boolValue]: skip leading whitespace, an optional sign and leading zeros; YES on
// 'Y', 'y', 'T', 't' or a digit 1-9.
bool stringBoolValue(const std::string& s)
{
    size_t i = 0;
    while (i < s.size() && std::isspace((unsigned char)s[i]))
    {
        ++i;
    }
    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
    {
        ++i;
    }
    while (i < s.size() && s[i] == '0')
    {
        ++i;
    }
    if (i >= s.size())
    {
        return false;
    }
    const char c = s[i];
    return c == 'Y' || c == 'y' || c == 'T' || c == 't' || (c >= '1' && c <= '9');
}

}  // namespace

// ---- port helpers -------------------------------------------------------------------------------

void Special::postNotification(const std::string& name, void* object)
{
    uikit::NotificationCenter::postNotification(name, object);
}

float Special::editorArtScale()
{
    // E3 (stage scaling): stage space is measured in editor-atlas art units (texture pixels /
    // contentScaleFactor), so every ref - whether its own frame is the art (MineRef, SoccerBallRef,
    // HomingMineRef, SpringBoxRef, TokenRef) or the art is a child - shows at its iOS size with
    // node scale 1. One art unit = EditorLayer::stageUnitInPoints() iPad points; the editor ptm
    // ratio is scaled to match (EditorLayer sets sessionPtmRatio = 72 / stageUnitInPoints()), and
    // the stage's display scale absorbs the factor. Inside stage space the art scale is therefore 1.
    return 1.0f;
}

void Special::setSessionPtmRatio(float ptmRatio)
{
    s_sessionPtmRatio = ptmRatio;
}

float Special::sessionPtmRatio()
{
    return s_sessionPtmRatio;
}

float Special::kvcFloat(const Value& v)
{
    return v.isNull() ? 0.0f : v.asFloat();
}

int Special::kvcInt(const Value& v)
{
    if (v.isNull())
    {
        return 0;
    }
    if (v.getType() == Value::Type::UNSIGNED)
    {
        return (int)v.asUnsignedInt();
    }
    return v.asInt();
}

unsigned int Special::kvcUnsigned(const Value& v)
{
    // RE-TODO(@1000da964): NSNumber -unsignedIntValue of a negative float; arm64 fcvtzu
    // saturation assumed (< 0 -> 0, >= 2^32 -> UINT_MAX).
    switch (v.getType())
    {
    case Value::Type::FLOAT:
    case Value::Type::DOUBLE:
    case Value::Type::STRING:
    {
        const double d = v.getType() == Value::Type::FLOAT ? (double)v.asFloat() : v.asDouble();
        if (!(d > 0.0))
        {
            return 0u;
        }
        if (d >= 4294967296.0)
        {
            return 0xffffffffu;
        }
        return (unsigned int)d;
    }
    case Value::Type::INTEGER:
        return (unsigned int)v.asInt();
    case Value::Type::NONE:
        return 0u;
    default:
        return v.asUnsignedInt();
    }
}

bool Special::kvcBool(const Value& v)
{
    switch (v.getType())
    {
    case Value::Type::NONE:
        return false;
    case Value::Type::FLOAT:
        return v.asFloat() != 0.0f;
    case Value::Type::DOUBLE:
        return v.asDouble() != 0.0;
    case Value::Type::STRING:
        return stringBoolValue(v.asString());
    default:
        return v.asBool();
    }
}

// ---- KVO ----------------------------------------------------------------------------------------

Special::KeyValueChange::KeyValueChange(Special* object, const char* key)
    : _object(object), _key(key), _outermost(false)
{
    auto& changing = _object->_kvoChanging;
    if (std::find(changing.begin(), changing.end(), key) == changing.end())
    {
        changing.emplace_back(key);
        _outermost = true;
    }
}

Special::KeyValueChange::~KeyValueChange()
{
    if (!_outermost)
    {
        return;
    }
    auto& changing = _object->_kvoChanging;
    auto it = std::find(changing.begin(), changing.end(), _key);
    if (it != changing.end())
    {
        changing.erase(it);
    }
    _object->didChangeValueForKey(_key);
}

void Special::addObserver(const void* observer, const std::string& keyPath,
                          const KeyValueObserver& callback)
{
    _observations.push_back({observer, keyPath, callback});
}

void Special::removeObserver(const void* observer, const std::string& keyPath)
{
    // Removes the most recent matching registration (one per call, like Foundation).
    for (auto it = _observations.rbegin(); it != _observations.rend(); ++it)
    {
        if (it->observer == observer && it->keyPath == keyPath)
        {
            _observations.erase(std::next(it).base());
            return;
        }
    }
}

// EDITOR (browser features, PC addition)
void Special::removeAllObservers(const void* observer)
{
    _observations.erase(std::remove_if(_observations.begin(), _observations.end(),
                                       [observer](const Observation& o) { return o.observer == observer; }),
                        _observations.end());
}

void Special::didChangeValueForKey(const std::string& key)
{
    std::vector<KeyValueObserver> callbacks;
    for (const auto& o : _observations)
    {
        if (o.keyPath == key)
        {
            callbacks.push_back(o.callback);
        }
    }
    if (callbacks.empty())
    {
        return;
    }
    retain();
    const Value newValue = valueForKey(key);
    for (const auto& cb : callbacks)
    {
        cb(key, this, newValue);
    }
    release();
}

// ---- Special ------------------------------------------------------------------------------------

Special::Special()
    : _x(0.0f)
    , _y(0.0f)
    , _ptmRatio(0.0f)
    , _fixed(false)
    , _sleeping(false)
    , _snaps(false)
    , _interactive(false)
    , _canDragModify(false)
    , _canRotate(false)
    , _locked(false)
    , _levelItemID(0)
    , _artCount(0)
    , _shapeCount(0)
{
}

// @ios 1000b4fc8
bool Special::initWithSpriteFrameName(const std::string& spriteFrameName)
{
    if (!Sprite::initWithSpriteFrameName(spriteFrameName))
    {
        return false;
    }
    _snaps = true;
    _canDragModify = false;
    _locked = false;
    _canRotate = true;
    _interactive = true;
    _ptmRatio = sessionPtmRatio();
    return true;
}

// @ios 1000b506c
std::string Special::name()
{
    return EditorSettings::getInstance()->nameForLevelItem((unsigned int)_levelItemID);
}

// @ios 1000b50a4
bool Special::initWithSpriteFrame(SpriteFrame* spriteFrame)
{
    if (!Sprite::initWithSpriteFrame(spriteFrame))
    {
        return false;
    }
    _propertyKeys = {"xMeters", "yMeters", "angle", "fixed", "sleeping"};
    // port: textureRect size in iPad points (see editorArtScale).
    const Size textureSize = getTextureRect().size;
    _refRect.size.width = (double)(textureSize.width * editorArtScale());
    _refRect.size.height = (double)(textureSize.height * editorArtScale());
    return true;
}

// @ios 1000b5154
InputObject* Special::inputObjectForPropertyWithRect(const std::string& property, const Rect& rect)
{
    if (property == "x")
    {
        return InputObject::create(rect, "X", "x", _x, true);
    }
    if (property == "y")
    {
        return InputObject::create(rect, "Y", "y", _y, true);
    }
    if (property == "angle")
    {
        InputObject* input = InputObject::create(rect, "ANGLE", "angle", getRotation(), true);
        if (input)
        {
            input->setMinValue(0.0f, 360.0f);
            input->setIsAngleValue(true);
        }
        return input;
    }
    if (property == "fixed")
    {
        return SwitchInputObject::create(rect, "FIXED", "fixed", _fixed ? 1.0f : 0.0f);
    }
    if (property == "sleeping")
    {
        return SwitchInputObject::create(rect, "SLEEPING", "sleeping", _sleeping ? 1.0f : 0.0f);
    }
    if (property == "interactive")
    {
        return SwitchInputObject::create(rect, "INTERACTIVE", "interactive",
                                         _interactive ? 1.0f : 0.0f);
    }
    return nullptr;
}

// @ios 1000b54e0
void Special::onEnter()
{
    _x = getPosition().x;
    _y = getPosition().y;
    Sprite::onEnter();
}

// @ios 1000b554c
Special::~Special()
{
}

// @ios 1000b559c
bool Special::containsPoint(const cg::Point& point)
{
    Node* parent = getParent();
    // [nil convertToWorldSpace:] -> CGPointZero
    const Vec2 world = parent ? parent->convertToWorldSpace(point.toVec2()) : Vec2::ZERO;
    const Vec2 local = convertToNodeSpace(world);
    return cg::rectContainsPoint(_refRect, cg::Point(local));
}

// @ios 1000b55fc
void Special::setX(float x, float y)
{
    setX(Value(x));
    setY(Value(y));
}

// @ios 1000b5650
void Special::setX(const Value& x)
{
    KeyValueChange kvo(this, "x");
    _x = kvcFloat(x);
    setPosition(Vec2(_x, _y));
}

// @ios 1000b5698
void Special::setY(const Value& y)
{
    KeyValueChange kvo(this, "y");
    _y = kvcFloat(y);
    setPosition(Vec2(_x, _y));
}

// @ios 1000b56e4
float Special::xMeters()
{
    return (float)((double)getPosition().x / (double)_ptmRatio);
}

// @ios 1000b571c
void Special::setXMeters(const Value& xMeters)
{
    KeyValueChange kvo(this, "xMeters");
    setX(Value(_ptmRatio * kvcFloat(xMeters)));
}

// @ios 1000b5768
float Special::yMeters()
{
    return (float)((double)getPosition().y / (double)_ptmRatio);
}

// @ios 1000b57a0
void Special::setYMeters(const Value& yMeters)
{
    KeyValueChange kvo(this, "yMeters");
    setY(Value(_ptmRatio * kvcFloat(yMeters)));
}

// @ios 1000b57ec
void Special::setAngle(const Value& angle)
{
    KeyValueChange kvo(this, "angle");
    setRotation(kvcFloat(angle));
}

// @ios 1000b5814
bool Special::fixed()
{
    return _fixed;
}

// @ios 1000b5824
void Special::setFixed(const Value& fixed)
{
    KeyValueChange kvo(this, "fixed");
    _fixed = kvcBool(fixed);
}

// @ios 1000b5854
bool Special::sleeping()
{
    return _sleeping;
}

// @ios 1000b5864
void Special::setSleeping(const Value& sleeping)
{
    KeyValueChange kvo(this, "sleeping");
    _sleeping = kvcBool(sleeping);
}

// @ios 1000b5894
bool Special::interactive()
{
    return _interactive;
}

// @ios 1000b58a4
void Special::setInteractive(const Value& interactive)
{
    KeyValueChange kvo(this, "interactive");
    _interactive = kvcBool(interactive);
}

// @ios 1000b58d4
float Special::x()
{
    return getPosition().x;
}

// @ios 1000b58ec
float Special::y()
{
    return getPosition().y;
}

// @ios 1000b5904
float Special::angle()
{
    return getRotation();
}

// @ios 1000b5908
float Special::height()
{
    return 0.0f;
}

// @ios 1000b5910
void Special::setHeight(float height)
{
    KeyValueChange kvo(this, "height");
}

// @ios 1000b5914
float Special::width()
{
    return 0.0f;
}

// @ios 1000b591c
void Special::setWidth(float width)
{
    KeyValueChange kvo(this, "width");
}

// @ios 1000b5920
void Special::setShapeCount(unsigned int shapeCount)
{
    KeyValueChange kvo(this, "shapeCount");
    _shapeCount = shapeCount;
    postNotification(SHAPE_COUNT_UPDATE, nullptr);
}

// @ios 1000b5954
void Special::setArtCount(unsigned int artCount)
{
    KeyValueChange kvo(this, "artCount");
    _artCount = artCount;
    postNotification(ART_COUNT_UPDATE, nullptr);
}

// @ios 1000b5988
cg::Rect Special::refBoundingBox()
{
    const AffineTransform t = getNodeToParentAffineTransform();
    return applyAffineTransform(_refRect, {t.a, t.b, t.c, t.d, t.tx, t.ty});
}

// @ios 1000b59e4
void Special::setProperties(const ValueMap& properties)
{
    log("--ref shape set properties: %s", Value(properties).getDescription().c_str());
    if (properties.find("p0") == properties.end())
    {
        return;
    }
    unsigned int i = 0;
    std::string key = "p0";
    ValueMap::const_iterator it;
    while ((it = properties.find(key)) != properties.end())
    {
        const float value = kvcFloat(it->second);
        if (i < _propertyKeys.size())
        {
            setValueForKey(Value(value), _propertyKeys[i]);
        }
        else
        {
            // iOS: -objectAtIndex: beyond the key list raises NSRangeException. Port: ignore.
            log("Special::setProperties: %s has no property key (levelItemID %d), ignored",
                key.c_str(), _levelItemID);
        }
        ++i;
        key = StringUtils::format("p%i", (int)i);
    }
}

// @ios 1000b5ad0
ValueMap Special::properties()
{
    ValueMap dict;
    dict["t"] = Value(_levelItemID);
    for (unsigned int i = 0; i < _propertyKeys.size(); ++i)
    {
        const float value = kvcFloat(valueForKey(_propertyKeys[i]));
        std::string text = StringUtils::format("%.02f", value);
        if (text.find(".00") != std::string::npos)
        {
            text = text.substr(0, text.length() - 3);
        }
        dict[StringUtils::format("p%i", (int)i)] = Value(text);
    }
    return dict;
}

// @ios 1000b5c38
std::vector<std::string> Special::propertyKeysForUI()
{
    return propertyKeys();
}

// @ios 1000b5c3c
void Special::createRef()
{
}

// @ios 1000b5c40
void Special::update()
{
}

void Special::updateDrawingWithNode(DrawNode* node)
{
}

// @ios 1000b5c44
AffineTransform Special::nodeToParentTransformWithPosition(const Vec2& position)
{
    // cocos2d-iphone 2.x -[CCNode nodeToParentTransform] with `position` substituted.
    float x = position.x;
    float y = position.y;
    const Vec2& anchor = getAnchorPointInPoints();
    const double ax = anchor.x;
    const double ay = anchor.y;
    if (isIgnoreAnchorPointForPosition())
    {
        x = (float)(ax + (double)x);
        y = (float)(ay + (double)y);
    }

    const float rotationX = getRotationSkewX();
    const float rotationY = getRotationSkewY();
    float cx = 1.0f, sx = 0.0f, cy = 1.0f, sy = 0.0f;
    if (!(rotationY == 0.0f && rotationX == 0.0f))
    {
        const float radiansX = rotationX * -0.017453292f;
        const float radiansY = rotationY * -0.017453292f;
        cx = std::cos(radiansX);
        sx = std::sin(radiansX);
        cy = std::cos(radiansY);
        sy = std::sin(radiansY);
    }

    const float scaleX = getScaleX();
    const float scaleY = getScaleY();
    const bool anchorIsZero = (ax == 0.0 && ay == 0.0);
    bool needsSkewMatrix;
    if (getSkewX() == 0.0f && getSkewY() == 0.0f)
    {
        needsSkewMatrix = false;
        if (!anchorIsZero)
        {
            const double px = (double)scaleX * -ax;
            const double py = (double)scaleY * -ay;
            x = (float)((double)x + (double)cy * px + (double)-sx * py);
            y = (float)((double)y + (double)sy * px + (double)cx * py);
        }
    }
    else
    {
        needsSkewMatrix = true;
    }

    CGTransform t = {(double)(scaleX * cy), (double)(scaleX * sy), (double)(-sx * scaleY),
                     (double)(cx * scaleY), (double)x,            (double)y};
    if (needsSkewMatrix)
    {
        const float skewY = getSkewY() * 0.017453292f;
        const float skewX = getSkewX() * 0.017453292f;
        const CGTransform skew = {1.0, (double)std::tan(skewY), (double)std::tan(skewX), 1.0, 0.0, 0.0};
        t = concat(skew, t);
        if (!anchorIsZero)
        {
            // CGAffineTransformTranslate(t, -ax, -ay)
            t = concat({1.0, 0.0, 0.0, 1.0, -ax, -ay}, t);
        }
    }
    return AffineTransformMake((float)t.a, (float)t.b, (float)t.c, (float)t.d, (float)t.tx,
                               (float)t.ty);
}

// @ios 1000b5f04
cg::Rect Special::refRect()
{
    return _refRect;
}

// @ios 1000b5f1c
void Special::setRefRect(const cg::Rect& refRect)
{
    KeyValueChange kvo(this, "refRect");
    _refRect = refRect;
}

void Special::setRefRect(const Rect& refRect)
{
    setRefRect(cg::Rect(refRect));
}

// @ios 1000b5f34
cg::Point Special::startPos()
{
    return _startPos;
}

// @ios 1000b5f48
void Special::setStartPos(const cg::Point& startPos)
{
    KeyValueChange kvo(this, "startPos");
    _startPos = startPos;
}

// @ios 1000b5f5c
void Special::setRefBoundingBox(const cg::Rect& refBoundingBox)
{
    KeyValueChange kvo(this, "refBoundingBox");
    _refBoundingBox = refBoundingBox;
}

// @ios 1000b5f74
std::vector<std::string>& Special::propertyKeys()
{
    return _propertyKeys;
}

// @ios 1000b5f84
bool Special::canDragModify()
{
    return _canDragModify;
}

// @ios 1000b5f94
void Special::setCanDragModify(bool canDragModify)
{
    KeyValueChange kvo(this, "canDragModify");
    _canDragModify = canDragModify;
}

// @ios 1000b5fa4
bool Special::canRotate()
{
    return _canRotate;
}

// @ios 1000b5fb4
void Special::setCanRotate(bool canRotate)
{
    KeyValueChange kvo(this, "canRotate");
    _canRotate = canRotate;
}

// @ios 1000b5fc4
bool Special::locked()
{
    return _locked;
}

// @ios 1000b5fd4
void Special::setLocked(bool locked)
{
    KeyValueChange kvo(this, "locked");
    _locked = locked;
}

// @ios 1000b5fe4
int Special::levelItemID()
{
    return _levelItemID;
}

// @ios 1000b5ff4
void Special::setLevelItemID(int levelItemID)
{
    KeyValueChange kvo(this, "levelItemID");
    _levelItemID = levelItemID;
}

// @ios 1000b6004
unsigned int Special::artCount()
{
    return _artCount;
}

// @ios 1000b6014
unsigned int Special::shapeCount()
{
    return _shapeCount;
}

// ---- KVC (port) -----------------------------------------------------------------------------------

Value Special::valueForKey(const std::string& key)
{
    if (key == "x") return Value(x());
    if (key == "y") return Value(y());
    if (key == "xMeters") return Value(xMeters());
    if (key == "yMeters") return Value(yMeters());
    if (key == "angle") return Value(angle());
    if (key == "fixed") return Value(fixed());
    if (key == "sleeping") return Value(sleeping());
    if (key == "interactive") return Value(interactive());
    if (key == "width") return Value(width());
    if (key == "height") return Value(height());
    if (key == "levelItemID") return Value(levelItemID());
    if (key == "shapeCount") return Value(shapeCount());
    if (key == "artCount") return Value(artCount());
    if (key == "canDragModify") return Value(canDragModify());
    if (key == "canRotate") return Value(canRotate());
    if (key == "locked") return Value(locked());
    {
        // EDITOR (browser features, PC addition): trigger actions of this ref as a target.
        Value action;
        if (flashed::valueForActionKey(this, key, &action)) return action;
    }
    // iOS: NSUnknownKeyException. Port: log and return nil.
    log("Special::valueForKey: unknown key '%s' (levelItemID %d)", key.c_str(), _levelItemID);
    return Value::Null;
}

void Special::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "x") { setX(value); return; }
    if (key == "y") { setY(value); return; }
    if (key == "xMeters") { setXMeters(value); return; }
    if (key == "yMeters") { setYMeters(value); return; }
    if (key == "angle") { setAngle(value); return; }
    if (key == "fixed") { setFixed(value); return; }
    if (key == "sleeping") { setSleeping(value); return; }
    if (key == "interactive") { setInteractive(value); return; }
    if (key == "width") { setWidth(kvcFloat(value)); return; }
    if (key == "height") { setHeight(kvcFloat(value)); return; }
    if (key == "levelItemID") { setLevelItemID(kvcInt(value)); return; }
    if (key == "shapeCount") { setShapeCount(kvcUnsigned(value)); return; }
    if (key == "artCount") { setArtCount(kvcUnsigned(value)); return; }
    if (key == "canDragModify") { setCanDragModify(kvcBool(value)); return; }
    if (key == "canRotate") { setCanRotate(kvcBool(value)); return; }
    if (key == "locked") { setLocked(kvcBool(value)); return; }
    // EDITOR (browser features, PC addition): trigger actions of this ref as a target.
    if (flashed::setValueForActionKey(this, key, value)) return;
    // iOS: NSUnknownKeyException. Port: log and ignore.
    log("Special::setValueForKey: unknown key '%s' (levelItemID %d), ignored", key.c_str(),
        _levelItemID);
}
