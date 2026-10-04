// EDITOR (browser features, PC addition): see JointRef.h.
#include "JointRef.h"

#include <algorithm>
#include <cmath>

#include "EditorSpriteBatchNode.h"
#include "EditorUndoManager.h"
#include "FlashEditor.h"
#include "GroupRef.h"
#include "UIKitCompat.h"

USING_NS_CC;
using namespace flashed;

JointRef* JointRef::create(bool prismatic)
{
    JointRef* ref = new (std::nothrow) JointRef();
    if (ref && ref->initWithType(prismatic))
    {
        ref->autorelease();
        return ref;
    }
    delete ref;
    return nullptr;
}

bool JointRef::initWithType(bool prismatic)
{
    if (!Special::initWithSpriteFrameName("e_1x1.png")) return false;
    setOpacity(0);
    _prismatic = prismatic;
    setLevelItemID(prismatic ? kSlideLevelItemID : kPinLevelItemID);
    propertyKeys() = {"flashX", "flashY"};
    setCanRotate(false);
    if (prismatic)
    {
        upper = 100.0f;   // PrisJoint defaults
        lower = -100.0f;
    }
    const float r = pxToStageLength(12.0f);
    const Vec2 o(getContentSize() * 0.5f);
    setRefRect(cg::Rect(o.x - r, o.y - r, r * 2.0f, r * 2.0f));
    return true;
}

void JointRef::onEnter()
{
    Special::onEnter();
    if (_identifyOnEnter)
    {
        _identifyOnEnter = false;
        identifyBodies(false);
    }
}

void JointRef::setBodies(Special* body1, Special* body2)
{
    _identifyOnEnter = false;
    _body1 = body1;
    _body2 = body2;
}

bool JointRef::vehicleAttached() const
{
    auto isVehicle = [](Special* s) {
        GroupRef* g = s ? dynamic_cast<GroupRef*>(s) : nullptr;
        return g && g->vehicle;
    };
    return isVehicle(_body1.get()) || isVehicle(_body2.get());
}

void JointRef::identifyBodies(bool registerUndo)
{
    EditorSpriteBatchNode* sbn = stage();
    if (!sbn) return;
    std::vector<Special*> hits;
    const cg::Point p(getPosition());
    for (Special* ref : sbn->refs())
    {
        if (ref == this || dynamic_cast<JointRef*>(ref) || ref->locked()) continue;
        if (ref->containsPoint(p)) hits.push_back(ref);
    }
    // topmost first (later children draw on top among equal z)
    std::stable_sort(hits.begin(), hits.end(), [sbn](Special* a, Special* b) {
        if (a->getLocalZOrder() != b->getLocalZOrder()) return a->getLocalZOrder() > b->getLocalZOrder();
        return sbn->refs().getIndex(a) > sbn->refs().getIndex(b);
    });
    Special* b1 = nullptr;
    Special* b2 = nullptr;
    for (Special* hit : hits)
    {
        Special* unit = unitOf(hit);
        if (!joinable(unit)) continue;
        if (!b1) b1 = unit;
        else if (unit != b1) { b2 = unit; break; }
    }
    if (b1 == _body1.get() && b2 == _body2.get()) return;
    if (registerUndo && undoManager())
    {
        RefPtr<Special> o1 = _body1, o2 = _body2;
        undoManager()->prepareWithInvocationTarget(this, [o1, o2](JointRef* j) {
            j->_body1 = o1;
            j->_body2 = o2;
        });
        uikit::NotificationCenter::postNotification(uikit::notification::kUndoStackUpdated, undoManager());
    }
    _body1 = b1;
    _body2 = b2;
    uiKeysChangedLater(this);
}

std::vector<std::string> JointRef::propertyKeysForUI()
{
    std::vector<std::string> keys = {"flashX", "flashY"};
    if (_prismatic)
    {
        keys.push_back("axisAngle");
        keys.push_back("limitPris");
        if (limit) keys.insert(keys.end(), {"upperLimit", "lowerLimit"});
        keys.push_back("motorPris");
        if (motor) keys.insert(keys.end(), {"force", "speedPris"});
    }
    else
    {
        keys.push_back("limit");
        if (limit) keys.insert(keys.end(), {"upperAngle", "lowerAngle"});
        keys.push_back("motor");
        if (motor) keys.insert(keys.end(), {"torque", "speed"});
    }
    keys.push_back("collideSelf");
    if (vehicleAttached()) keys.push_back("vehicleControlled");
    return keys;
}

Value JointRef::valueForKey(const std::string& key)
{
    if (key == "flashX") return Value(stageToPx(getPosition()).x);
    if (key == "flashY") return Value(stageToPx(getPosition()).y);
    if (key == "limit" || key == "limitPris") return Value(limit);
    if (key == "motor" || key == "motorPris") return Value(motor);
    if (key == "upperAngle" || key == "upperLimit") return Value(upper);
    if (key == "lowerAngle" || key == "lowerLimit") return Value(lower);
    if (key == "speed" || key == "speedPris") return Value(speed);
    if (key == "torque" || key == "force") return Value(torque);
    if (key == "axisAngle") return Value(axisAngle);
    if (key == "collideSelf") return Value(collideSelf);
    if (key == "vehicleControlled") return Value(vehicleControlled);
    return Special::valueForKey(key);
}

void JointRef::setValueForKey(const Value& value, const std::string& key)
{
    const float v = kvcFloat(value);
    if (key == "flashX" || key == "flashY")
    {
        Vec2 px = stageToPx(getPosition());
        (key == "flashX" ? px.x : px.y) = v;
        const Vec2 s = pxToStage(px.x, px.y);
        {
            KeyValueChange kvo(this, key.c_str());
            setX(s.x, s.y);
        }
        if (onStage(this)) identifyBodies(true);  // moved by the inspector: re-attach
        return;
    }
    KeyValueChange kvo(this, key.c_str());
    const float maxAngle = 180.0f, maxLimit = 3000.0f;
    // PinJoint / PrisJoint setter clamps (upper >= 0, lower <= 0).
    if (key == "limit" || key == "limitPris") { limit = kvcBool(value); uiKeysChangedLater(this); return; }
    if (key == "motor" || key == "motorPris") { motor = kvcBool(value); uiKeysChangedLater(this); return; }
    if (key == "upperAngle") { upper = std::max(0.0f, std::min(maxAngle, std::round(v))); return; }
    if (key == "lowerAngle") { lower = -std::min(maxAngle, std::fabs(std::round(v))); return; }
    if (key == "upperLimit") { upper = std::max(0.0f, std::min(maxLimit, std::round(v))); return; }
    if (key == "lowerLimit") { lower = -std::min(maxLimit, std::fabs(std::round(v))); return; }
    if (key == "speed") { speed = std::max(-20.0f, std::min(20.0f, v)); return; }
    if (key == "speedPris") { speed = std::max(-50.0f, std::min(50.0f, v)); return; }
    if (key == "torque" || key == "force") { torque = std::max(0.0f, std::min(100000000.0f, v)); return; }
    if (key == "axisAngle") { axisAngle = std::max(-180.0f, std::min(180.0f, v)); return; }
    if (key == "collideSelf") { collideSelf = kvcBool(value); return; }
    if (key == "vehicleControlled") { vehicleControlled = kvcBool(value); return; }
    Special::setValueForKey(value, key);
}

ValueMap JointRef::properties()
{
    ValueMap d;
    d["t"] = Value(_prismatic ? kSlideLevelItemID : kPinLevelItemID);
    d["x"] = Value(getPosition().x);
    d["y"] = Value(getPosition().y);
    d["limit"] = Value(limit);
    d["motor"] = Value(motor);
    d["collide"] = Value(collideSelf);
    d["vc"] = Value(vehicleControlled);
    d["upper"] = Value(upper);
    d["lower"] = Value(lower);
    d["speed"] = Value(speed);
    d["torque"] = Value(torque);
    d["axis"] = Value(axisAngle);
    return d;
}

void JointRef::setProperties(const ValueMap& d)
{
    auto get = [&d](const char* k, float def) {
        auto it = d.find(k);
        return it == d.end() ? def : it->second.asFloat();
    };
    setX(get("x", getPosition().x), get("y", getPosition().y));
    limit = get("limit", 0) != 0.0f;
    motor = get("motor", 0) != 0.0f;
    collideSelf = get("collide", 0) != 0.0f;
    vehicleControlled = get("vc", 1) != 0.0f;
    upper = get("upper", upper);
    lower = get("lower", lower);
    speed = get("speed", speed);
    torque = get("torque", torque);
    axisAngle = get("axis", 0);
}

void JointRef::updateOverlayWithNode(DrawNode* node)
{
    const float px = pxToStageLength(1.0f);
    const Vec2 c = getPosition();
    // Arms to the bodies (Flash drawArms: hairline 0xff6600).
    for (Special* b : {_body1.get(), _body2.get()})
        if (b && onStage(b)) node->drawSegment(c, b->getPosition(), 1.5f * px, kJointColor);
    const Color4F ring = _body1 && onStage(_body1.get()) ? kJointColor : Color4F(0.85f, 0.1f, 0.1f, 1.0f);
    if (_prismatic)
    {
        // The axis, with arrow heads (Flash's sliding joint icon is a double arrow).
        const float a = -axisAngle * 3.14159265f / 180.0f;  // Flash clockwise -> stage
        const Vec2 u(std::cos(a), std::sin(a));
        const Vec2 n(-u.y, u.x);
        const Vec2 p0 = c - u * 18.0f * px, p1 = c + u * 18.0f * px;
        node->drawSegment(p0, p1, 2.0f * px, ring);
        Vec2 h1[3] = {p1 + u * 6.0f * px, p1 + n * 5.0f * px, p1 - n * 5.0f * px};
        Vec2 h2[3] = {p0 - u * 6.0f * px, p0 + n * 5.0f * px, p0 - n * 5.0f * px};
        node->drawSolidPoly(h1, 3, ring);
        node->drawSolidPoly(h2, 3, ring);
        node->drawDot(c, 4.0f * px, Color4F::WHITE);
        node->drawDot(c, 2.5f * px, ring);
    }
    else
    {
        node->drawDot(c, 9.0f * px, ring);
        node->drawDot(c, 6.5f * px, Color4F::WHITE);
        node->drawDot(c, 3.0f * px, ring);
    }
}
