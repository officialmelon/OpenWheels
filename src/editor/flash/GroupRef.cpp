// EDITOR (browser features, PC addition): see GroupRef.h.
#include "GroupRef.h"

#include <algorithm>
#include <cmath>

#include "FlashEditor.h"
#include "FlashSpecialRef.h"
#include "JointRef.h"
#include "RefShape.h"
#include "TriggerRef.h"

USING_NS_CC;
using namespace flashed;

bool GroupRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png")) return false;
    setOpacity(0);
    setLevelItemID(kLevelItemID);
    propertyKeys() = {"flashX", "flashY"};
    setCanRotate(true);
    setRefRect(cg::Rect(0, 0, 0, 0));  // never hit itself; its members are
    return true;
}

void GroupRef::setMembers(const Vector<Special*>& members)
{
    for (Special* m : _members)
        if (m->group() == this) m->setGroup(nullptr);
    _members = members;
    for (Special* m : _members) m->setGroup(this);
    recenter();
}

Vector<Special*> GroupRef::liveMembers() const
{
    Vector<Special*> live;
    for (Special* m : _members)
        if (onStage(m) && m->group() == this) live.pushBack(m);
    return live;
}

bool GroupRef::groupable(Special* ref)
{
    if (!ref || dynamic_cast<GroupRef*>(ref) || dynamic_cast<JointRef*>(ref) || dynamic_cast<TriggerRef*>(ref))
        return false;
    if (dynamic_cast<RefShape*>(ref)) return true;
    const int t = ref->levelItemID();
    bool interactive = true;
    if (FlashSpecialRef* f = dynamic_cast<FlashSpecialRef*>(ref)) interactive = f->interactiveItem();
    else if (t == 0 || t == 20) interactive = ref->interactive();
    switch (t)
    {
    case 3: case 6: case 29: case 34: case 23: case 16:   // IBeam, spikes, arrow gun, blade, sign, text
        return true;
    case 0: case 1: case 17: case 19: case 20: case 21: case 22: case 24: case 26: case 32:
        return !interactive;                             // art-only furniture / NPCs
    default:
        return false;
    }
}

bool GroupRef::hasShapes() const
{
    for (Special* m : liveMembers())
        if (m->shapeCount() > 0) return true;
    return false;
}

cg::Rect GroupRef::refBoundingBox()
{
    Vector<Special*> live = liveMembers();
    if (live.empty()) return cg::Rect(getPosition().x, getPosition().y, 0, 0);
    cg::Rect r = live.at(0)->refBoundingBox();
    for (ssize_t i = 1; i < live.size(); ++i) r = cg::rectUnion(r, live.at(i)->refBoundingBox());
    return r;
}

void GroupRef::recenter()
{
    Vector<Special*> live = liveMembers();
    if (live.empty()) return;
    const cg::Rect r = refBoundingBox();
    const Vec2 c((float)(r.origin.x + r.size.width * 0.5), (float)(r.origin.y + r.size.height * 0.5));
    if (c != getPosition()) setPosition(c);
}

std::vector<std::string> GroupRef::propertyKeysForUI()
{
    // RefGroup / RefVehicle.setAttributes
    if (vehicle)
        return {"flashX", "flashY", "sleeping", "foreground", "acceleration", "lockJoints", "leaningStrength",
                "spaceAction", "shiftAction", "ctrlAction", "characterPose"};
    return {"flashX", "flashY", "sleeping", "foreground", "opacity", "immovable3", "fixedRotation"};
}

Value GroupRef::valueForKey(const std::string& key)
{
    if (key == "flashX") return Value(stageToPx(getPosition()).x);
    if (key == "flashY") return Value(stageToPx(getPosition()).y);
    if (key == "sleeping") return Value(sleeping);
    if (key == "foreground") return Value(foreground);
    if (key == "opacity") return Value(opacity);
    if (key == "immovable3") return Value(immovable);
    if (key == "fixedRotation") return Value(fixedRotation);
    if (key == "acceleration") return Value(acceleration);
    if (key == "lockJoints") return Value(lockJoints);
    if (key == "leaningStrength") return Value(leaningStrength);
    if (key == "spaceAction") return Value(spaceAction);
    if (key == "shiftAction") return Value(shiftAction);
    if (key == "ctrlAction") return Value(ctrlAction);
    if (key == "characterPose") return Value(characterPose);
    if (key == "vehicle") return Value(vehicle);
    return Special::valueForKey(key);
}

void GroupRef::setValueForKey(const Value& value, const std::string& key)
{
    const float v = kvcFloat(value);
    if (key == "flashX" || key == "flashY")
    {
        // Moves the whole group.
        Vec2 px = stageToPx(getPosition());
        (key == "flashX" ? px.x : px.y) = v;
        const Vec2 delta = pxToStage(px.x, px.y) - getPosition();
        KeyValueChange kvo(this, key.c_str());
        for (Special* m : liveMembers()) m->setX(m->getPosition().x + delta.x, m->getPosition().y + delta.y);
        recenter();
        return;
    }
    KeyValueChange kvo(this, key.c_str());
    if (key == "sleeping") { sleeping = kvcBool(value); return; }
    if (key == "foreground") { foreground = kvcBool(value); return; }
    if (key == "opacity") { opacity = std::max(0.0f, std::min(100.0f, std::round(v))); return; }
    if (key == "immovable3") { immovable = kvcBool(value); return; }
    if (key == "fixedRotation") { fixedRotation = kvcBool(value); return; }
    if (key == "acceleration") { acceleration = std::max(1.0f, std::min(10.0f, v)); return; }
    if (key == "lockJoints") { lockJoints = kvcBool(value); return; }
    if (key == "leaningStrength") { leaningStrength = std::max(0, std::min(10, (int)std::lround(v))); return; }
    if (key == "spaceAction") { spaceAction = std::max(0, std::min(3, (int)std::lround(v))); return; }
    if (key == "shiftAction") { shiftAction = std::max(0, std::min(3, (int)std::lround(v))); return; }
    if (key == "ctrlAction") { ctrlAction = std::max(0, std::min(3, (int)std::lround(v))); return; }
    if (key == "characterPose") { characterPose = std::max(0, std::min(3, (int)std::lround(v))); return; }
    if (key == "vehicle") { vehicle = kvcBool(value); uiKeysChangedLater(this); return; }
    Special::setValueForKey(value, key);
}

ValueMap GroupRef::properties()
{
    ValueMap d;
    d["t"] = Value(kLevelItemID);
    for (const char* k : {"sleeping", "foreground", "opacity", "immovable3", "fixedRotation", "acceleration",
                          "lockJoints", "leaningStrength", "spaceAction", "shiftAction", "ctrlAction",
                          "characterPose", "vehicle"})
        d[k] = valueForKey(k);
    return d;
}

void GroupRef::setProperties(const ValueMap& d)
{
    for (const auto& kv : d)
        if (kv.first != "t") setValueForKey(kv.second, kv.first);
}

void GroupRef::updateOverlayWithNode(DrawNode* node)
{
    Vector<Special*> live = liveMembers();
    if (live.empty()) return;
    const cg::Rect r = refBoundingBox();
    const float px = pxToStageLength(1.0f);
    const float pad = 6.0f * px;
    const Vec2 a((float)r.origin.x - pad, (float)r.origin.y - pad);
    const Vec2 b((float)(r.origin.x + r.size.width) + pad, (float)(r.origin.y + r.size.height) + pad);
    const Color4F c = vehicle ? Color4F(0.25f, 0.6f, 0.3f, 0.9f) : kGroupColor;
    // Dashed outline.
    const Vec2 corners[5] = {a, Vec2(b.x, a.y), b, Vec2(a.x, b.y), a};
    const float dash = 12.0f * px;
    for (int i = 0; i < 4; ++i)
    {
        const Vec2 p0 = corners[i], p1 = corners[i + 1];
        const float len = p0.distance(p1);
        const Vec2 u = (p1 - p0) / std::max(len, 0.001f);
        for (float s = 0; s < len; s += dash * 2.0f)
            node->drawSegment(p0 + u * s, p0 + u * std::min(len, s + dash), 1.2f * px, c);
    }
}
