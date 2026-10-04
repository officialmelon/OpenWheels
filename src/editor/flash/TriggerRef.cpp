// EDITOR (browser features, PC addition): see TriggerRef.h.
#include "TriggerRef.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "EditorUndoManager.h"
#include "FlashEditor.h"
#include "UIKitCompat.h"

USING_NS_CC;
using namespace flashed;

bool TriggerRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png")) return false;
    setOpacity(0);
    setLevelItemID(kLevelItemID);
    _uid = nextUid();
    propertyKeys() = {"flashX", "flashY"};
    setCanRotate(true);
    setShapeCount(1);  // RefTrigger._shapesUsed
    setArtCount(0);
    _numberLabel = Label::createWithSystemFont("", "Arial Bold", 64.0f);
    _numberLabel->setTextColor(Color4B(61, 136, 199, 255));  // 0x3d88c7
    _numberLabel->setAnchorPoint(Vec2(0.0f, 1.0f));
    addChild(_numberLabel, 1);
    updateRefRect();
    return true;
}

void TriggerRef::setNumber(int number)
{
    if (number == _number) return;
    _number = number;
    _numberLabel->setString(StringUtils::format("%d", number));
}

void TriggerRef::updateRefRect()
{
    const float w = pxToStageLength(_widthPx);
    const float h = pxToStageLength(_heightPx);
    const Vec2 o(getContentSize() * 0.5f);
    setRefRect(cg::Rect(o.x - w * 0.5f, o.y - h * 0.5f, w, h));
    // The number in the top-left corner (Flash numLabel at 3, 3), 15 px text.
    const float px = pxToStageLength(1.0f);
    _numberLabel->setScale(15.0f * px / 64.0f * 1.4f);
    _numberLabel->setPosition(Vec2(o.x - w * 0.5f + 3.0f * px, o.y + h * 0.5f - 3.0f * px));
}

int TriggerRef::indexOfTarget(Special* ref) const
{
    for (size_t i = 0; i < _targets.size(); ++i)
        if (_targets[i].ref.get() == ref) return (int)i;
    return -1;
}

std::vector<float> TriggerRef::defaultParams(Special* ref, int actionIndex)
{
    std::vector<float> values;
    const std::vector<ActionInfo>* list = actionsFor(ref);
    if (!list || actionIndex < 0 || actionIndex >= (int)list->size()) return values;
    const Vec2 pos = stageToPx(ref->getPosition());
    for (const char* key : (*list)[actionIndex].params)
    {
        const ActionParam* p = actionParam(key);
        float v = p ? p->def : 0.0f;
        if (!strcmp(key, "newX")) v = std::round(pos.x);
        if (!strcmp(key, "newY")) v = std::round(pos.y);
        values.push_back(v);
    }
    return values;
}

bool TriggerRef::addTarget(Special* ref, bool registerUndo)
{
    TargetKind kind;
    int type;
    if (!ref || ref == this || !targetKind(ref, &kind, &type) || indexOfTarget(ref) >= 0) return false;
    TriggerTarget target;
    target.ref = ref;
    const std::vector<ActionInfo>* list = actionsFor(ref);
    if (list && !list->empty())
    {
        TriggerAction a;
        a.index = 0;
        a.params = defaultParams(ref, 0);
        target.actions.push_back(a);
    }
    _targets.push_back(target);
    if (registerUndo && undoManager())
    {
        RefPtr<Special> keep(ref);
        undoManager()->prepareWithInvocationTarget(this, [keep](TriggerRef* t) {
            const int i = t->indexOfTarget(keep.get());
            if (i >= 0) t->removeTargetAt(i, true);
        });
        uikit::NotificationCenter::postNotification(uikit::notification::kUndoStackUpdated, undoManager());
    }
    refreshPanelLater();
    return true;
}

void TriggerRef::removeTargetAt(int index, bool registerUndo)
{
    if (index < 0 || index >= (int)_targets.size()) return;
    TriggerTarget removed = _targets[index];
    _targets.erase(_targets.begin() + index);
    if (registerUndo && undoManager())
    {
        undoManager()->prepareWithInvocationTarget(this, [removed, index](TriggerRef* t) {
            auto& list = t->targets();
            list.insert(list.begin() + std::min(index, (int)list.size()), removed);
            if (undoManager())
            {
                RefPtr<Special> keep = removed.ref;
                undoManager()->prepareWithInvocationTarget(t, [keep](TriggerRef* t2) {
                    const int i = t2->indexOfTarget(keep.get());
                    if (i >= 0) t2->removeTargetAt(i, true);
                });
            }
            refreshPanelLater();
        });
        uikit::NotificationCenter::postNotification(uikit::notification::kUndoStackUpdated, undoManager());
    }
    refreshPanelLater();
}

std::vector<std::string> TriggerRef::propertyKeysForUI()
{
    // RefTrigger.setAttributes
    std::vector<std::string> keys = {"flashX", "flashY", "shapeWidth", "shapeHeight", "angle", "triggeredBy", "repeatType"};
    if (repeatType > 2) keys.push_back("repeatInterval");
    keys.push_back("triggerType");
    if (typeIndex == 1)
    {
        keys.push_back("triggerDelay");
    }
    else if (typeIndex == 2)
    {
        keys.insert(keys.end(), {"soundEffect", "triggerDelay", "soundLocation", "volume"});
        if (soundLocation == 1) keys.push_back("panning");
    }
    keys.push_back("startDisabled");
    return keys;
}

Value TriggerRef::valueForKey(const std::string& key)
{
    if (key == "flashX") return Value(stageToPx(getPosition()).x);
    if (key == "flashY") return Value(stageToPx(getPosition()).y);
    if (key == "shapeWidth") return Value(_widthPx);
    if (key == "shapeHeight") return Value(_heightPx);
    if (key == "angle") return Value(normalizedAngle(getRotation()));
    if (key == "triggeredBy") return Value(triggeredBy);
    if (key == "triggerType") return Value(typeIndex);
    if (key == "repeatType") return Value(repeatType);
    if (key == "repeatInterval") return Value(repeatInterval);
    if (key == "triggerDelay") return Value(delay);
    if (key == "startDisabled") return Value(startDisabled);
    if (key == "soundEffect") return Value(sound);
    if (key == "soundLocation") return Value(soundLocation);
    if (key == "panning") return Value(panning);
    if (key == "volume") return Value(volume);
    return Special::valueForKey(key);
}

void TriggerRef::setValueForKey(const Value& value, const std::string& key)
{
    const float v = kvcFloat(value);
    if (key == "flashX" || key == "flashY")
    {
        Vec2 px = stageToPx(getPosition());
        (key == "flashX" ? px.x : px.y) = v;
        const Vec2 s = pxToStage(px.x, px.y);
        KeyValueChange kvo(this, key.c_str());
        setX(s.x, s.y);
        return;
    }
    KeyValueChange kvo(this, key.c_str());
    // RefTrigger setter clamps.
    if (key == "shapeWidth") { _widthPx = std::max(5.0f, std::min(5000.0f, v)); updateRefRect(); return; }
    if (key == "shapeHeight") { _heightPx = std::max(5.0f, std::min(5000.0f, v)); updateRefRect(); return; }
    if (key == "triggeredBy")
    {
        const int old = triggeredBy;
        triggeredBy = std::max(1, std::min(6, (int)std::lround(v)));
        if ((old == 4) != (triggeredBy == 4)) uiKeysChangedLater(this);
        return;
    }
    if (key == "triggerType")
    {
        typeIndex = std::max(1, std::min(3, (int)std::lround(v)));
        uiKeysChangedLater(this);
        return;
    }
    if (key == "repeatType")
    {
        repeatType = std::max(1, std::min(4, (int)std::lround(v)));
        uiKeysChangedLater(this);
        return;
    }
    if (key == "repeatInterval") { repeatInterval = std::max(0.1f, std::min(30.0f, v)); return; }
    if (key == "triggerDelay") { delay = std::max(0.0f, std::min(30.0f, v)); return; }
    if (key == "startDisabled") { startDisabled = kvcBool(value); return; }
    if (key == "soundEffect") { sound = std::max(0, (int)std::lround(v)); return; }
    if (key == "soundLocation")
    {
        soundLocation = std::max(1, std::min(2, (int)std::lround(v)));
        uiKeysChangedLater(this);
        return;
    }
    if (key == "panning") { panning = std::max(-1.0f, std::min(1.0f, v)); return; }
    if (key == "volume") { volume = std::max(0.0f, std::min(1.0f, v)); return; }
    Special::setValueForKey(value, key);
}

ValueMap TriggerRef::properties()
{
    ValueMap d;
    d["t"] = Value(kLevelItemID);
    d["x"] = Value(getPosition().x);
    d["y"] = Value(getPosition().y);
    d["angle"] = Value(getRotation());
    d["w"] = Value(_widthPx);
    d["h"] = Value(_heightPx);
    d["b"] = Value(triggeredBy);
    d["type"] = Value(typeIndex);
    d["r"] = Value(repeatType);
    d["i"] = Value(repeatInterval);
    d["d"] = Value(delay);
    d["sd"] = Value(startDisabled);
    d["s"] = Value(sound);
    d["l"] = Value(soundLocation);
    d["p"] = Value(panning);
    d["v"] = Value(volume);
    return d;
}

void TriggerRef::setProperties(const ValueMap& d)
{
    auto get = [&d](const char* k, float def) {
        auto it = d.find(k);
        return it == d.end() ? def : it->second.asFloat();
    };
    setX(get("x", getPosition().x), get("y", getPosition().y));
    setRotation(get("angle", 0));
    _widthPx = get("w", 100);
    _heightPx = get("h", 100);
    triggeredBy = (int)get("b", 1);
    typeIndex = (int)get("type", 1);
    repeatType = (int)get("r", 1);
    repeatInterval = get("i", 1);
    delay = get("d", 0);
    startDisabled = get("sd", 0) != 0.0f;
    sound = (int)get("s", 0);
    soundLocation = (int)get("l", 1);
    panning = get("p", 0);
    volume = get("v", 1);
    updateRefRect();
}

void TriggerRef::updateOverlayWithNode(DrawNode* node)
{
    // Flash: an orange / yellow hatch (grey when start-disabled) under a 5 px grey outline.
    const cg::Rect r = refRect();
    const AffineTransform t = getNodeToParentAffineTransform();
    Vec2 v[4] = {Vec2((float)r.origin.x, (float)r.origin.y),
                 Vec2((float)(r.origin.x + r.size.width), (float)r.origin.y),
                 Vec2((float)(r.origin.x + r.size.width), (float)(r.origin.y + r.size.height)),
                 Vec2((float)r.origin.x, (float)(r.origin.y + r.size.height))};
    for (Vec2& p : v) p = PointApplyAffineTransform(p, t);
    const Color4F fill = startDisabled ? Color4F(0.6f, 0.6f, 0.6f, 0.35f) : Color4F(1.0f, 0.6f, 0.0f, 0.35f);
    const Color4F stripe = startDisabled ? Color4F(0.8f, 0.8f, 0.8f, 0.35f) : Color4F(1.0f, 1.0f, 0.4f, 0.45f);
    const float px = pxToStageLength(1.0f);
    node->drawPolygon(v, 4, fill, 2.5f * px, Color4F(0.4f, 0.4f, 0.4f, 0.9f));
    // Diagonal stripes every 20 px, clipped to the rectangle (in its local frame).
    const float x0 = (float)r.origin.x, y0 = (float)r.origin.y;
    const float w = (float)r.size.width, h = (float)r.size.height;
    const float step = 20.0f * px;
    for (float d = step; d < w + h; d += step)
    {
        // line x + y = d (local, from the bottom-left corner), clipped to [0,w] x [0,h]
        Vec2 a(std::max(0.0f, d - h), std::min(h, d));
        Vec2 b(std::min(w, d), std::max(0.0f, d - w));
        a = PointApplyAffineTransform(Vec2(x0 + a.x, y0 + a.y), t);
        b = PointApplyAffineTransform(Vec2(x0 + b.x, y0 + b.y), t);
        node->drawSegment(a, b, 3.0f * px, stripe);
    }

    if (!hasTargets()) return;
    // Arms to the targets (Flash drawArms: 3 px grey; arrows towards other triggers).
    const Vec2 from = getPosition();
    for (const TriggerTarget& target : _targets)
    {
        Special* ref = target.ref.get();
        if (!ref || !onStage(ref)) continue;
        Vec2 to = ref->getPosition();
        node->drawSegment(from, to, 1.5f * px, kLinkColor);
        node->drawDot(to, 4.0f * px, kLinkColor);
        if (dynamic_cast<TriggerRef*>(ref))
        {
            const Vec2 d = to - from;
            const float len = d.getLength();
            if (len < 1.0f) continue;
            const Vec2 u = d / len, n(-u.y, u.x);
            for (float s = 50.0f * px; s < len - 10.0f * px; s += 50.0f * px)
            {
                const Vec2 c = from + u * s;
                Vec2 tri[3] = {c + u * 10.0f * px, c + n * 5.0f * px, c - n * 5.0f * px};
                node->drawSolidPoly(tri, 3, kLinkColor);
            }
        }
    }
}
