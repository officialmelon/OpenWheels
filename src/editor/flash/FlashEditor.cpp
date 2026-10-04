// EDITOR (browser features, PC addition): see FlashEditor.h.
#include "FlashEditor.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

#include "CharacterRef.h"
#include "EditorSettings.h"
#include "EditorSpriteBatchNode.h"
#include "EditorUndoManager.h"
#include "FlashSpecialRef.h"
#include "GroupRef.h"
#include "InputObject.h"
#include "JointRef.h"
#include "PolygonRefShape.h"
#include "RefShape.h"
#include "SliderInputObject.h"
#include "Special.h"
#include "SwitchInputObject.h"
#include "TriggerRef.h"
#include "UIKitCompat.h"

USING_NS_CC;

namespace flashed {

const Color4F kLinkColor(0.4f, 0.4f, 0.4f, 0.9f);
const Color4F kJointColor(1.0f, 0.4f, 0.0f, 1.0f);
const Color4F kGroupColor(0.24f, 0.53f, 0.78f, 0.9f);

namespace {
EditorSpriteBatchNode* s_stage = nullptr;
int s_uid = 0;

std::vector<std::string> split(const std::string& s, char c)
{
    std::vector<std::string> out;
    std::string cur;
    for (char ch : s)
    {
        if (ch == c)
        {
            out.push_back(cur);
            cur.clear();
        }
        else
            cur += ch;
    }
    out.push_back(cur);
    return out;
}

struct ArtEntry
{
    float originX = 0, originY = 0, zoom = 1;
};

const std::map<std::string, ArtEntry>& artIndex()
{
    static std::map<std::string, ArtEntry> index;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const std::string path = FileUtils::getInstance()->fullPathForFilename("generated/flash/index.tsv");
        if (!path.empty())
        {
            std::istringstream in(FileUtils::getInstance()->getStringFromFile(path));
            std::string line;
            while (std::getline(in, line))
            {
                if (line.empty() || line[0] == '#') continue;
                std::istringstream fields(line);
                std::string name;
                ArtEntry e;
                if (fields >> name >> e.originX >> e.originY >> e.zoom) index[name] = e;
            }
        }
    }
    return index;
}

// "act:<uid>:<target>:<action>[:<param>]" -> parts. False when malformed.
struct ActionKey
{
    TriggerRef* trigger = nullptr;
    int target = -1;
    int action = -1;
    std::string param;
};

bool parseActionKey(const std::string& key, ActionKey* out)
{
    if (key.compare(0, 4, "act:") != 0) return false;
    std::vector<std::string> p = split(key.substr(4), ':');
    if (p.size() < 3) return false;
    out->trigger = triggerWithUid(std::atoi(p[0].c_str()));
    out->target = std::atoi(p[1].c_str());
    out->action = std::atoi(p[2].c_str());
    out->param = p.size() > 3 ? p[3] : "";
    if (!out->trigger || out->target < 0 || out->target >= (int)out->trigger->targets().size()) return false;
    const auto& actions = out->trigger->targets()[out->target].actions;
    return out->action >= 0 && out->action < (int)actions.size();
}

}  // namespace

// ---- context ---------------------------------------------------------------------------------------

void setStage(EditorSpriteBatchNode* sbn) { s_stage = sbn; }
EditorSpriteBatchNode* stage() { return s_stage; }
EditorUndoManager* undoManager() { return s_stage ? s_stage->undoManager() : nullptr; }

std::vector<TriggerRef*> stageTriggers()
{
    std::vector<TriggerRef*> list;
    if (!s_stage) return list;
    for (Special* ref : s_stage->refs())
        if (TriggerRef* t = dynamic_cast<TriggerRef*>(ref)) list.push_back(t);
    return list;
}

int triggerNumber(TriggerRef* trigger)
{
    std::vector<TriggerRef*> list = stageTriggers();
    auto it = std::find(list.begin(), list.end(), trigger);
    return it == list.end() ? 0 : (int)(it - list.begin()) + 1;
}

TriggerRef* triggerWithUid(int uid)
{
    for (TriggerRef* t : stageTriggers())
        if (t->uid() == uid) return t;
    return nullptr;
}

bool onStage(Special* ref)
{
    return ref && s_stage && ref->getParent() == s_stage;
}

int nextUid() { return ++s_uid; }

namespace {
bool s_shift = false;
}
bool shiftDown() { return s_shift; }
void setShiftDown(bool down) { s_shift = down; }

// ---- units -------------------------------------------------------------------------------------------

float ptm() { return Special::sessionPtmRatio(); }

Vec2 stageToPx(const Vec2& p)
{
    return Vec2(p.x / ptm() * kPxPerMetre, kCanvasHeight - p.y / ptm() * kPxPerMetre);
}

Vec2 pxToStage(float xPx, float yPx)
{
    return Vec2(xPx / kPxPerMetre * ptm(), (kCanvasHeight - yPx) / kPxPerMetre * ptm());
}

float normalizedAngle(float degrees)
{
    float a = std::fmod(degrees, 360.0f);
    if (a > 180.0f) a -= 360.0f;
    if (a < -180.0f) a += 360.0f;
    return a;
}

// ---- kinds -------------------------------------------------------------------------------------------

bool targetKind(Special* ref, TargetKind* kind, int* specialType)
{
    if (!ref || dynamic_cast<CharacterRef*>(ref)) return false;
    *specialType = -1;
    if (dynamic_cast<RefShape*>(ref)) *kind = TargetKind::Shape;
    else if (dynamic_cast<GroupRef*>(ref)) *kind = TargetKind::Group;
    else if (JointRef* j = dynamic_cast<JointRef*>(ref)) *kind = j->prismatic() ? TargetKind::PrisJoint : TargetKind::PinJoint;
    else if (dynamic_cast<TriggerRef*>(ref)) *kind = TargetKind::Trigger;
    else
    {
        *kind = TargetKind::Special;
        *specialType = ref->levelItemID();
    }
    return targetActions(*kind, *specialType) != nullptr;
}

const std::vector<ActionInfo>* actionsFor(Special* ref)
{
    TargetKind kind;
    int type;
    if (!targetKind(ref, &kind, &type)) return nullptr;
    return targetActions(kind, type);
}

bool joinable(Special* ref)
{
    if (!ref || dynamic_cast<CharacterRef*>(ref) || dynamic_cast<TriggerRef*>(ref) || dynamic_cast<JointRef*>(ref))
        return false;
    if (GroupRef* g = dynamic_cast<GroupRef*>(ref)) return !g->immovable && g->hasShapes();
    if (ref->group()) return false;
    if (RefShape* s = dynamic_cast<RefShape*>(ref)) return s->interactive() && !s->fixed();
    if (FlashSpecialRef* f = dynamic_cast<FlashSpecialRef*>(ref))
    {
        switch (f->type())
        {
        case 1: case 17: case 19: case 21: case 22: case 24: case 26: case 30: case 32:
            return f->interactiveItem();
        case 18: return true;
        case 11: return f->param("immovable2") == 0.0f;
        default: return false;
        }
    }
    // The iOS editor's refs (Flash _joinable): van, bottle, blade (interactive), IBeam / log /
    // spikes / arrow gun when not fixed, jet.
    switch (ref->levelItemID())
    {
    case 0: case 20: case 34: return ref->interactive();
    case 3: case 4: case 6: case 29: return !ref->fixed() || ref->levelItemID() == 6;
    case 28: return true;
    default: return false;
    }
}

Special* unitOf(Special* ref)
{
    if (ref && ref->group() && onStage((Special*)ref->group())) return (Special*)ref->group();
    return ref;
}

std::string displayName(Special* ref)
{
    if (!ref) return "";
    if (TriggerRef* t = dynamic_cast<TriggerRef*>(ref)) return StringUtils::format("trigger %d", triggerNumber(t));
    if (JointRef* j = dynamic_cast<JointRef*>(ref)) return j->prismatic() ? "sliding joint" : "pin joint";
    if (GroupRef* g = dynamic_cast<GroupRef*>(ref)) return g->vehicle ? "vehicle" : "group";
    if (PolygonRefShape* p = dynamic_cast<PolygonRefShape*>(ref)) return p->isArt() ? "art shape" : "polygon";
    if (FlashSpecialRef* f = dynamic_cast<FlashSpecialRef*>(ref)) return f->displayName();
    if (dynamic_cast<CharacterRef*>(ref)) return "start";
    std::string name = EditorSettings::getInstance()->nameForLevelItem((unsigned int)ref->levelItemID());
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return name;
}

// ---- action keys ---------------------------------------------------------------------------------------

bool valueForActionKey(Special* ref, const std::string& key, Value* out)
{
    // Positions / sizes in Flash px for every ref (the inspector shows browser units).
    if (key == "flashX") { *out = Value(stageToPx(ref->getPosition()).x); return true; }
    if (key == "flashY") { *out = Value(stageToPx(ref->getPosition()).y); return true; }
    if (key == "flashWidth") { *out = Value(stageToPxLength(ref->width())); return true; }
    if (key == "flashHeight") { *out = Value(stageToPxLength(ref->height())); return true; }
    if (key == "vehicleHandle")
    {
        GroupRef* g = ref->group();
        *out = Value(!(g && g->nonHandles.count(ref)));
        return true;
    }
    ActionKey k;
    if (!parseActionKey(key, &k)) return key.compare(0, 4, "act:") == 0 ? (*out = Value(0.0f), true) : false;
    const TriggerAction& a = k.trigger->targets()[k.target].actions[k.action];
    if (k.param.empty())
    {
        *out = Value(a.index);
        return true;
    }
    const std::vector<ActionInfo>* list = actionsFor(k.trigger->targets()[k.target].ref.get());
    float v = 0.0f;
    if (list && a.index < (int)list->size())
    {
        const auto& params = (*list)[a.index].params;
        for (size_t i = 0; i < params.size(); ++i)
            if (k.param == params[i] && i < a.params.size()) v = a.params[i];
    }
    *out = Value(v);
    return true;
}

bool setValueForActionKey(Special* ref, const std::string& key, const Value& value)
{
    if (key == "flashX" || key == "flashY")
    {
        Vec2 px = stageToPx(ref->getPosition());
        (key == "flashX" ? px.x : px.y) = value.asFloat();
        const Vec2 s = pxToStage(px.x, px.y);
        ref->setX(s.x, s.y);
        ref->didChangeValueForKey(key);
        return true;
    }
    if (key == "flashWidth") { ref->setValueForKey(Value(pxToStageLength(value.asFloat())), "width"); ref->didChangeValueForKey(key); return true; }
    if (key == "flashHeight") { ref->setValueForKey(Value(pxToStageLength(value.asFloat())), "height"); ref->didChangeValueForKey(key); return true; }
    if (key == "vehicleHandle")
    {
        if (GroupRef* g = ref->group())
        {
            if (value.asBool()) g->nonHandles.erase(ref);
            else g->nonHandles.insert(ref);
        }
        return true;
    }
    ActionKey k;
    if (!parseActionKey(key, &k)) return key.compare(0, 4, "act:") == 0;
    TriggerTarget& target = k.trigger->targets()[k.target];
    TriggerAction& a = target.actions[k.action];
    const std::vector<ActionInfo>* list = actionsFor(target.ref.get());
    if (!list) return true;
    if (k.param.empty())
    {
        const int index = std::max(0, std::min((int)list->size() - 1, (int)std::lround(value.asFloat())));
        if (index != a.index)
        {
            a.index = index;
            a.params = TriggerRef::defaultParams(target.ref.get(), index);
            refreshPanelLater();
        }
        return true;
    }
    const auto& params = (*list)[a.index].params;
    for (size_t i = 0; i < params.size(); ++i)
    {
        if (k.param != params[i]) continue;
        if (a.params.size() < params.size()) a.params.resize(params.size(), 0.0f);
        float v = value.asFloat();
        if (const ActionParam* p = actionParam(params[i]))
        {
            v = std::max(p->min, std::min(p->max, v));
            if (p->segments > 0 && std::fabs(p->max - p->min - (float)p->segments) < 1e-3f) v = std::round(v);
        }
        a.params[i] = v;
    }
    return true;
}

static void appendTargetRows(std::vector<std::string>& keys, TriggerRef* trigger, int targetIndex)
{
    const TriggerTarget& target = trigger->targets()[targetIndex];
    const std::vector<ActionInfo>* list = actionsFor(target.ref.get());
    const std::string base = StringUtils::format("%d:%d", trigger->uid(), targetIndex);
    if (!list || list->empty() || !trigger->hasActions()) return;
    for (size_t a = 0; a < target.actions.size(); ++a)
    {
        const std::string ak = "act:" + base + StringUtils::format(":%d", (int)a);
        keys.push_back(ak);
        const int index = target.actions[a].index;
        if (index >= 0 && index < (int)list->size())
            for (const char* p : (*list)[index].params) keys.push_back(ak + ":" + p);
    }
    keys.push_back("actadd:" + base);
}

std::vector<std::string> targetUIKeys(Special* ref)
{
    std::vector<std::string> keys;
    for (TriggerRef* t : stageTriggers())
    {
        const int i = t->indexOfTarget(ref);
        if (i < 0 || !t->hasTargets()) continue;
        keys.push_back(StringUtils::format("tgt:%d:%d", t->uid(), i));  // section header
        appendTargetRows(keys, t, i);
    }
    return keys;
}

std::vector<std::string> triggerTargetUIKeys(TriggerRef* trigger)
{
    std::vector<std::string> keys;
    if (!trigger->hasTargets()) return keys;
    keys.push_back(StringUtils::format("link:%d", trigger->uid()));
    for (int i = 0; i < (int)trigger->targets().size(); ++i)
    {
        Special* ref = trigger->targets()[i].ref.get();
        if (!onStage(ref)) continue;
        keys.push_back(StringUtils::format("unlink:%d:%d", trigger->uid(), i));
        appendTargetRows(keys, trigger, i);
    }
    return keys;
}

InputObject* actionInputObject(Special* ref, const std::string& key, const Rect& rect)
{
    // Legacy (iOS-style) panel: sliders for the parameters only.
    ActionKey k;
    if (!parseActionKey(key, &k) || k.param.empty()) return nullptr;
    const ActionParam* p = actionParam(k.param);
    if (!p) return nullptr;
    Value v;
    valueForActionKey(ref, key, &v);
    return SliderInputObject::create(rect, p->label, key, v.asFloat(), p->min, p->max, (unsigned int)p->segments);
}

// ---- panels ----------------------------------------------------------------------------------------------

InputObject* makeInput(const Attr& attr, const std::string& property, float value, const Rect& rect)
{
    switch (attr.input)
    {
    case Input::Switch: return SwitchInputObject::create(rect, attr.label, property, value);
    case Input::Slider:
    case Input::Choice:
        return SliderInputObject::create(rect, attr.label, property, value, attr.min, attr.max,
                                         (unsigned int)std::max(0, attr.segments));
    default:
    {
        InputObject* io = InputObject::create(rect, attr.label, property, value, true);
        if (io) io->setMinValue(attr.min, attr.max);
        return io;
    }
    }
}

void uiKeysChangedLater(Special* ref)
{
    RefPtr<Special> keep(ref);
    Director::getInstance()->getScheduler()->performFunctionInCocosThread([keep]() {
        Special::postNotification(Special::REF_UI_KEYS_WILL_CHANGE, keep.get());
        Special::postNotification(Special::REF_UI_KEYS_CHANGED, keep.get());
    });
}

void refreshPanelLater()
{
    Director::getInstance()->getScheduler()->performFunctionInCocosThread([]() {
        Special::postNotification(Special::REF_UI_KEYS_WILL_CHANGE, nullptr);
        Special::postNotification(Special::REF_UI_KEYS_CHANGED, nullptr);
    });
}

// ---- art ----------------------------------------------------------------------------------------------------

Sprite* flashIconSprite(const std::string& name)
{
    const auto& index = artIndex();
    auto it = index.find(name);
    if (it == index.end()) return nullptr;
    const std::string file = "generated/flash/" + name + ".png";
    if (!FileUtils::getInstance()->isFileExist(file)) return nullptr;
    Sprite* sprite = Sprite::create(file);
    if (!sprite) return nullptr;
    const Size size = sprite->getContentSize();
    // Content size is in points of the content scale; anchor on the registration point.
    const float csf = Director::getInstance()->getContentScaleFactor();
    const float w = size.width * csf, h = size.height * csf;
    if (w > 0 && h > 0) sprite->setAnchorPoint(Vec2(it->second.originX / w, 1.0f - it->second.originY / h));
    return sprite;
}

Sprite* flashArtSprite(const std::string& name)
{
    Sprite* sprite = flashIconSprite(name);
    if (!sprite) return nullptr;
    const auto& e = artIndex().at(name);
    const float csf = Director::getInstance()->getContentScaleFactor();
    // image px / zoom = Flash px; Flash px -> stage units.
    sprite->setScale(csf / e.zoom * pxToStageLength(1.0f));
    return sprite;
}

std::string fontFile(int font)
{
    static const char* const names[] = {"helvetica", "helvetica", "helvetica_med", "helvetica_bold", "clarendon",
                                        "clarendon_bold"};
    const std::string file =
        std::string("generated/flash/fonts/") + names[std::max(1, std::min(5, font))] + ".ttf";
    return FileUtils::getInstance()->isFileExist(file) ? file : std::string();
}

}  // namespace flashed
