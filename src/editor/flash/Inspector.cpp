// EDITOR (browser features, PC addition): see Inspector.h.
#include "Inspector.h"

#include <algorithm>
#include <cmath>

#include "CharacterRef.h"
#include "CircleRefShape.h"
#include "ColorInputObject.h"
#include "EditorUndoManager.h"
#include "FlashEditor.h"
#include "FlashSpecialRef.h"
#include "GroupRef.h"
#include "InputObject.h"
#include "InspectorWidgets.h"
#include "JointRef.h"
#include "PolygonRefShape.h"
#include "RefShape.h"
#include "SliderInputObject.h"
#include "Special.h"
#include "SwitchInputObject.h"
#include "TriggerRef.h"
#include "UIKitCompat.h"
#include "online/OnlineUi.h"
#include "platform/common/Localization.h"

USING_NS_CC;
namespace oui = online::ui;

namespace flashed {

namespace {

const float kPad = 50.0f;
const float kRowH = 112.0f;
const float kSliderRowH = 176.0f;
const float kHeaderH = 96.0f;
const float kButtonRowH = 136.0f;

std::vector<std::string> split(const std::string& s, char c)
{
    std::vector<std::string> out(1);
    for (char ch : s)
    {
        if (ch == c) out.emplace_back();
        else out.back() += ch;
    }
    return out;
}

bool isBrowserRef(Special* ref)
{
    return dynamic_cast<FlashSpecialRef*>(ref) || dynamic_cast<TriggerRef*>(ref) || dynamic_cast<JointRef*>(ref) ||
           dynamic_cast<GroupRef*>(ref);
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string formatValue(float v, bool decimals)
{
    if (!decimals || std::fabs(v - std::round(v)) < 1e-4f) return StringUtils::format("%d", (int)std::lround(v));
    std::string s = StringUtils::format("%.2f", v);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

void specFromAttr(const Attr& a, RowSpec* spec)
{
    spec->label = a.label;
    spec->help = a.help ? a.help : "";
    spec->min = a.min;
    spec->max = a.max;
    spec->segments = a.segments;
    spec->limits = true;
    switch (a.input)
    {
    case Input::Field: spec->kind = RowSpec::Field; break;
    case Input::Slider: spec->kind = RowSpec::Slider; break;
    case Input::Switch: spec->kind = RowSpec::Toggle; break;
    case Input::Color: spec->kind = RowSpec::Color; spec->allowNone = a.min < 0; break;
    case Input::Text: spec->kind = RowSpec::Text; break;
    case Input::Sound: spec->kind = RowSpec::Sound; break;
    case Input::Choice:
        spec->kind = RowSpec::Choice;
        for (size_t i = 0; i < a.choices.size(); ++i)
        {
            spec->choices.push_back(a.choices[i]);
            spec->choiceValues.push_back(a.min + (float)i);
        }
        break;
    }
    spec->decimals = !(spec->segments > 0 && std::fabs(spec->max - spec->min - (float)spec->segments) < 1e-3f);
}

// The iOS editor's input for `key`, read back as a row.
bool specFromIOS(Special* ref, const std::string& key, RowSpec* spec)
{
    InputObject* io = ref->inputObjectForPropertyWithRect(key, Rect(0, 0, 300, 27));
    if (!io) return false;
    RefPtr<InputObject> keep(io);
    spec->label = lower(Localization::get(io->labelKey()));
    if (spec->label.empty()) spec->label = lower(key);
    spec->limits = io->limitsEnabled();
    spec->min = io->minValue();
    spec->max = io->maxValue();
    spec->angle = io->isAngleValue();
    spec->decimals = io->showDecimal();
    if (dynamic_cast<SwitchInputObject*>(io)) spec->kind = RowSpec::Toggle;
    else if (SliderInputObject* s = dynamic_cast<SliderInputObject*>(io))
    {
        spec->kind = RowSpec::Slider;
        spec->segments = (int)s->segments();
    }
    else if (ColorInputObject* c = dynamic_cast<ColorInputObject*>(io))
    {
        spec->kind = RowSpec::Slider;
        spec->segments = (int)c->segments();
    }
    else
        spec->kind = RowSpec::Field;
    if (spec->kind == RowSpec::Slider)
        spec->decimals = !(spec->segments > 0 && std::fabs(spec->max - spec->min - (float)spec->segments) < 1e-3f);
    return true;
}

}  // namespace

// ---- keys and specs ------------------------------------------------------------------------------------

std::vector<std::string> Inspector::keysFor(Special* ref)
{
    std::vector<std::string> keys;
    if (dynamic_cast<CharacterRef*>(ref))
        return {"flashX", "flashY", "defaultCharacter", "forceCharacter", "hideVehicle"};
    for (const std::string& k : ref->propertyKeysForUI())
    {
        if (isBrowserRef(ref))
        {
            keys.push_back(k);
            continue;
        }
        if (k == "x") keys.push_back("flashX");
        else if (k == "y") keys.push_back("flashY");
        else if (k == "width") keys.push_back("flashWidth");
        else if (k == "height") keys.push_back("flashHeight");
        else if (k == "innerRed")
        {
            keys.push_back("color");
            keys.push_back("outlineColor");
        }
        else if (k == "innerGreen" || k == "innerBlue") continue;
        else if (k == "xMeters" || k == "yMeters") continue;
        else keys.push_back(k);
    }
    if (dynamic_cast<CircleRefShape*>(ref)) keys.push_back("innerCutout");
    if (dynamic_cast<RefShape*>(ref) && ref->group() && ref->group()->vehicle && ref->interactive())
        keys.push_back("vehicleHandle");
    if (TriggerRef* t = dynamic_cast<TriggerRef*>(ref))
        for (const std::string& k : triggerTargetUIKeys(t)) keys.push_back(k);
    for (const std::string& k : targetUIKeys(ref)) keys.push_back(k);
    return keys;
}

bool Inspector::specFor(Special* ref, const std::string& key, RowSpec* spec)
{
    spec->key = key;
    const std::vector<std::string> parts = split(key, ':');
    if (parts[0] == "tgt" && parts.size() == 3)
    {
        TriggerRef* t = triggerWithUid(std::atoi(parts[1].c_str()));
        spec->kind = RowSpec::Header;
        spec->label = StringUtils::format("trigger %d", t ? triggerNumber(t) : 0);
        spec->secondButtonText = "Unlink";
        spec->secondKey = "unlink:" + parts[1] + ":" + parts[2];
        return true;
    }
    if (parts[0] == "unlink" && parts.size() == 3)
    {
        TriggerRef* t = triggerWithUid(std::atoi(parts[1].c_str()));
        const int i = std::atoi(parts[2].c_str());
        if (!t || i < 0 || i >= (int)t->targets().size()) return false;
        spec->kind = RowSpec::Header;
        spec->label = "target: " + displayName(t->targets()[i].ref.get());
        spec->secondButtonText = "Unlink";
        spec->secondKey = key;
        return true;
    }
    if (parts[0] == "link")
    {
        TriggerRef* t = triggerWithUid(std::atoi(parts[1].c_str()));
        spec->kind = RowSpec::Button;
        spec->label = StringUtils::format("targets: %d", t ? (int)t->targets().size() : 0);
        spec->buttonText = "Link targets";
        return true;
    }
    if (parts[0] == "actadd")
    {
        spec->kind = RowSpec::Button;
        spec->label = "";
        spec->buttonText = "+ Add action";
        spec->buttonColor = "yellow";
        return true;
    }
    if (parts[0] == "fn")
    {
        spec->kind = RowSpec::Button;
        const std::string f = parts[1];
        spec->buttonText = f == "group" ? "Group" : f == "ungroup" ? "Ungroup" : f == "vehicle" ? "Make vehicle"
                         : f == "unvehicle" ? "Make plain group" : f == "delete" ? "Delete" : f == "edittext" ? "Edit text" : f;
        spec->buttonColor = f == "delete" || f == "ungroup" ? "pink" : "blue";
        return true;
    }
    if (parts[0] == "act" && parts.size() >= 4)
    {
        TriggerRef* t = triggerWithUid(std::atoi(parts[1].c_str()));
        const int i = std::atoi(parts[2].c_str()), a = std::atoi(parts[3].c_str());
        if (!t || i < 0 || i >= (int)t->targets().size()) return false;
        const TriggerTarget& target = t->targets()[i];
        const std::vector<ActionInfo>* list = actionsFor(target.ref.get());
        if (!list) return false;
        if (parts.size() == 4)
        {
            spec->kind = RowSpec::Choice;
            spec->label = StringUtils::format("action %d", a + 1);
            for (size_t k = 0; k < list->size(); ++k)
            {
                spec->choices.push_back((*list)[k].name);
                spec->choiceValues.push_back((float)k);
            }
            if (target.actions.size() > 1)
            {
                spec->secondButtonText = "x";
                spec->secondKey = StringUtils::format("actdel:%s:%s:%s", parts[1].c_str(), parts[2].c_str(), parts[3].c_str());
            }
            return true;
        }
        const ActionParam* p = actionParam(parts[4]);
        if (!p) return false;
        spec->label = p->label;
        spec->kind = p->input == Input::Field ? RowSpec::Field : RowSpec::Slider;
        spec->min = p->min;
        spec->max = p->max;
        spec->segments = p->segments;
        spec->limits = true;
        spec->decimals = !(p->segments > 0 && std::fabs(p->max - p->min - (float)p->segments) < 1e-3f);
        return true;
    }
    if (key == "flashX" || key == "flashY")
    {
        spec->kind = RowSpec::Field;
        spec->label = key == "flashX" ? "x" : "y";
        spec->decimals = false;
        spec->limits = true;
        spec->min = 0;
        spec->max = key == "flashX" ? kCanvasWidth : kCanvasHeight;
        return true;
    }
    if (key == "flashWidth" || key == "flashHeight")
    {
        RowSpec ios;
        if (!specFromIOS(ref, key == "flashWidth" ? "width" : "height", &ios)) return false;
        spec->kind = RowSpec::Field;
        spec->label = ios.label;
        spec->limits = ios.limits;
        spec->min = stageToPxLength(ios.min);
        spec->max = stageToPxLength(ios.max);
        return true;
    }
    if (dynamic_cast<CharacterRef*>(ref) && key == "defaultCharacter")
    {
        spec->kind = RowSpec::Choice;
        spec->label = "character";
        for (int c = 1; c <= characterCount(); ++c)
        {
            if (!characterPlayable(c)) continue;
            spec->choices.push_back(characterName(c));
            spec->choiceValues.push_back((float)c);
        }
        return true;
    }
    if (dynamic_cast<RefShape*>(ref))
    {
        if (key == "color") { spec->kind = RowSpec::Color; spec->label = "color"; return true; }
        if (key == "outlineColor") { spec->kind = RowSpec::Color; spec->label = "outline"; spec->allowNone = true; return true; }
        if (key == "shapeOpacity")
        {
            spec->kind = RowSpec::Slider; spec->label = "opacity"; spec->min = 0; spec->max = 100; spec->segments = 100;
            spec->limits = true; spec->decimals = false;
            return true;
        }
        if (key == "collision")
        {
            const Attr* a = attribute("collision");
            specFromAttr(*a, spec);
            return true;
        }
        if (key == "innerCutout")
        {
            specFromAttr(*attribute("innerCutout"), spec);
            return true;
        }
        if (key == "vehicleHandle")
        {
            specFromAttr(*attribute("vehicleHandle"), spec);
            return true;
        }
    }
    if (isBrowserRef(ref))
    {
        if (key == "angle")
        {
            spec->kind = RowSpec::Field;
            spec->label = "rotation";
            spec->limits = true;
            spec->min = -180;
            spec->max = 180;
            spec->angle = true;
            return true;
        }
        if (const Attr* a = attribute(key))
        {
            specFromAttr(*a, spec);
            if (dynamic_cast<FlashSpecialRef*>(ref) && key == "charIndex")
            {
                // NPC skins by number (the browser editor has no names for them).
                spec->kind = RowSpec::Choice;
                spec->choices.clear();
                spec->choiceValues.clear();
                for (int i = 1; i <= 16; ++i)
                {
                    spec->choices.push_back(StringUtils::format("character %d", i));
                    spec->choiceValues.push_back((float)i);
                }
            }
            return true;
        }
    }
    return specFromIOS(ref, key, spec);
}

// ---- panel ----------------------------------------------------------------------------------------------

Inspector* Inspector::create(const Size& size)
{
    Inspector* i = new (std::nothrow) Inspector();
    if (i && i->init(size))
    {
        i->autorelease();
        return i;
    }
    delete i;
    return nullptr;
}

bool Inspector::init(const Size& size)
{
    if (!Node::init()) return false;
    oui::loadAtlases();
    setContentSize(size);
    _panel = oui::windowPanel(size);
    _panel->setAnchorPoint(Vec2::ZERO);
    addChild(_panel);
    const float strip = oui::windowPanelStrip();
    _title = ui::makeHeading("", 72.0f);
    _title->setPosition(Vec2(kPad, size.height - strip - 72.0f));
    addChild(_title, 1);
    _subtitle = ui::makeLabel("", 34.0f, true, oui::kInkDim);
    _subtitle->setPosition(Vec2(kPad + 4.0f, size.height - strip - 140.0f));
    addChild(_subtitle, 1);
    oui::Button* close = oui::Button::create("", Size(110.0f, 110.0f), oui::Button::window("pink"));
    close->setIcon(oui::iconSprite("clear", 54.0f));
    close->setPosition(Vec2(size.width - kPad - 55.0f, size.height - strip - 72.0f));
    close->setCallback([this]() {
        RefPtr<Inspector> keep(this);
        if (onClose) onClose();
    });
    addChild(close, 2);

    const float top = size.height - strip - 190.0f;
    _viewport = Rect(kPad * 0.5f, kPad * 0.6f, size.width - kPad, top - kPad * 0.6f);
    _clip = ClippingRectangleNode::create(_viewport);
    addChild(_clip, 1);
    _content = Node::create();
    _clip->addChild(_content);

    // Touches on the panel never reach the stage; drags on empty space scroll.
    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (!oui::isShown(this)) return false;
        const Vec2 p = convertToNodeSpace(t->getLocation());
        if (!Rect(Vec2::ZERO, _contentSize).containsPoint(p)) return false;
        _dragging = false;
        _dragStart = p;
        if (ui::ValueField* f = ui::ValueField::current()) f->unfocus(true);
        return true;
    };
    touch->onTouchMoved = [this](Touch* t, Event*) {
        if (!_dragging && std::fabs(convertToNodeSpace(t->getLocation()).y - _dragStart.y) > 20.0f) _dragging = true;
        if (_dragging) scrollBy(t->getDelta().y);
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, _panel);
    auto mouse = EventListenerMouse::create();
    mouse->onMouseScroll = [this](EventMouse* e) {
        if (!oui::isShown(this) || ui::popupOpen()) return;
        const Vec2 p = convertToNodeSpace(Vec2(e->getCursorX(), e->getCursorY()));
        if (!Rect(Vec2::ZERO, _contentSize).containsPoint(p)) return;
        scrollBy(e->getScrollY() * 110.0f);
        e->stopPropagation();
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, _panel);
    return true;
}

void Inspector::onEnter()
{
    Node::onEnter();
    using uikit::NotificationCenter;
    NotificationCenter::addObserver(this, uikit::notification::kSelRefChange, nullptr, [this](void* object, void*) {
        Vector<Special*> refs;
        if (object) refs = *static_cast<Vector<Special*>*>(object);
        setSelection(refs);
    });
    NotificationCenter::addObserver(this, uikit::notification::kRefUIKeysChanged, nullptr, [this](void*, void*) {
        // Deferred: the change may come from one of our own controls' callbacks.
        RefPtr<Inspector> keep(this);
        Director::getInstance()->getScheduler()->performFunctionInCocosThread([keep]() {
            if (keep->isRunning()) keep->rebuild();
        });
    });
    NotificationCenter::addObserver(this, uikit::notification::kUndoStackUpdated, nullptr,
                                    [this](void*, void*) { refreshValues(); });
}

void Inspector::onExit()
{
    uikit::NotificationCenter::removeObserver(this);
    for (Special* r : _selection) r->removeAllObservers(this);
    ui::closePopups();
    Node::onExit();
}

bool Inspector::inViewport(const Vec2& world) const
{
    return _viewport.containsPoint(convertToNodeSpace(world));
}

void Inspector::setSelection(const Vector<Special*>& refs)
{
    for (Special* r : _selection) r->removeAllObservers(this);
    _selection = refs;
    _scroll = 0.0f;
    rebuild();
}

void Inspector::clearRows()
{
    for (Row& r : _rows) r.node->removeFromParent();
    _rows.clear();
}

void Inspector::rebuild()
{
    if (ui::ValueField::current() && ui::ValueField::current()->isRunning())
    {
        // Don't yank a field away while it's being typed in: rebuild after it commits.
        ui::ValueField::current()->unfocus(true);
    }
    for (Special* r : _selection) r->removeAllObservers(this);
    clearRows();
    // A group and its members count as the group.
    Special* single = nullptr;
    std::vector<Special*> units;
    for (Special* r : _selection)
    {
        Special* u = unitOf(r);
        if (u != r && !_selection.contains(u)) u = r;  // a member selected on its own
        if (std::find(units.begin(), units.end(), u) == units.end()) units.push_back(u);
    }
    if (units.size() == 1) single = units[0];
    _ref = single;
    if (!single)
    {
        _title->setString(units.empty() ? "Nothing selected" : StringUtils::format("%d items", (int)units.size()));
        _subtitle->setString(units.empty() ? "tap an item to edit it" : "");
        int groupable = 0;
        for (Special* u : units)
            if (GroupRef::groupable(u) || dynamic_cast<GroupRef*>(u)) ++groupable;
        if (groupable >= 2 && groupable == (int)units.size())
        {
            RowSpec s;
            specFor(nullptr, "fn:group", &s);
            addRow(s);
        }
        if (!units.empty())
        {
            RowSpec s;
            specFor(nullptr, "fn:delete", &s);
            addRow(s);
        }
        layoutRows();
        return;
    }
    std::string name = displayName(single);
    if (!name.empty()) name[0] = (char)std::toupper((unsigned char)name[0]);
    _title->setString(name);
    oui::setEllipsized(_title, name, _contentSize.width - kPad * 2.0f - 140.0f);
    std::string sub;
    if (single->group() && unitOf(single) == single->group() && !_selection.contains(single->group()))
        sub = "inside a group";
    else if (GroupRef* g = dynamic_cast<GroupRef*>(single))
        sub = StringUtils::format("%d items", (int)g->liveMembers().size());
    else if (TriggerRef* t = dynamic_cast<TriggerRef*>(single))
        sub = StringUtils::format("%d targets", (int)t->targets().size());
    else if (JointRef* j = dynamic_cast<JointRef*>(single))
        sub = j->body1() ? (j->body2() ? displayName(j->body1()) + " + " + displayName(j->body2())
                                       : displayName(j->body1()) + " + world")
                         : "not attached - drop it on an item";
    _subtitle->setString(sub);
    for (const std::string& key : keysFor(single))
    {
        RowSpec spec;
        if (specFor(single, key, &spec)) addRow(spec);
    }
    if (GroupRef* g = dynamic_cast<GroupRef*>(single))
    {
        for (const char* f : {"fn:ungroup", g->vehicle ? "fn:unvehicle" : "fn:vehicle"})
        {
            RowSpec s;
            specFor(single, f, &s);
            addRow(s);
        }
    }
    // KVO: refresh rows when the model changes (dragging, undo).
    for (const Row& r : _rows)
    {
        if (r.spec.kind == RowSpec::Button || r.spec.kind == RowSpec::Header) continue;
        auto refresh = r.refresh;
        single->addObserver(this, r.spec.key, [refresh](const std::string&, Special*, const Value&) {
            if (refresh) refresh();
        });
    }
    layoutRows();
}

void Inspector::refreshValues()
{
    for (Row& r : _rows)
        if (r.refresh) r.refresh();
}

void Inspector::setValue(const std::string& key, const Value& value, bool undo, const Value& previous)
{
    if (!_ref) return;
    RefPtr<Special> ref(_ref);
    EditorUndoManager* um = undoManager();
    if (undo && um)
    {
        um->beginUndoGrouping();
        const Value old = previous.isNull() ? ref->valueForKey(key) : previous;
        um->prepareWithInvocationTarget(ref.get(), [old, key](Special* s) { s->setValueForKey(old, key); });
        um->endUndoGrouping();
    }
    ref->setValueForKey(value, key);
    if (undo && um) uikit::NotificationCenter::postNotification(uikit::notification::kUndoStackUpdated, um);
    uikit::NotificationCenter::postNotification(uikit::notification::kSelRectChanged, nullptr);
}

void Inspector::addRow(const RowSpec& spec)
{
    Row row;
    row.spec = spec;
    const float w = _viewport.size.width;
    Node* node = Node::create();
    row.node = node;
    const std::string key = spec.key;
    RefPtr<Special> ref(_ref);
    auto filter = [this](const Vec2& p) { return inViewport(p); };
    auto current = [ref, key]() { return ref ? ref->valueForKey(key) : Value(); };
    const float controlW = w * 0.46f;

    auto addLabel = [&](float y) {
        Label* l = ui::makeLabel(spec.label, 40.0f, true, oui::kInk);
        l->setPosition(Vec2(kPad * 0.5f, y));
        oui::setEllipsized(l, spec.label, w - controlW - kPad * 1.5f);
        node->addChild(l);
        return l;
    };

    switch (spec.kind)
    {
    case RowSpec::Field:
    case RowSpec::Text:
    {
        row.height = kRowH;
        addLabel(row.height * 0.5f);
        const bool text = spec.kind == RowSpec::Text;
        if (text) row.height = kRowH * 1.0f;
        ui::ValueField* f = ui::ValueField::create(Size(text ? w - kPad * 0.5f - 260.0f : controlW, 92.0f), !text, text);
        f->hitFilter = filter;
        f->setPosition(Vec2(w - (text ? w - kPad * 0.5f - 260.0f : controlW) - kPad * 0.25f, (row.height - 92.0f) * 0.5f));
        node->addChild(f);
        const RowSpec s = spec;
        f->onCommit = [this, s, key](const std::string& t) {
            if (s.kind == RowSpec::Text)
            {
                setValue(key, Value(t), true, Value::Null);
                return;
            }
            const std::string trimmed = uikit::trimmedString(t);
            if (trimmed.empty()) return;
            float v = std::strtof(trimmed.c_str(), nullptr);
            if (s.limits)
            {
                if (s.angle && (s.min >= 0.0f))
                {
                    v = std::fmod(v, 360.0f);
                    if (v < 0) v += 360.0f;
                }
                else if (s.angle)
                {
                    v = normalizedAngle(v);
                }
                else
                    v = std::max(s.min, std::min(s.max, v));
            }
            setValue(key, Value(v), true, Value::Null);
            refreshValues();
        };
        row.refresh = [f, current, s]() {
            const Value v = current();
            if (s.kind == RowSpec::Text) f->setText(v.getType() == Value::Type::STRING ? v.asString() : "");
            else f->setText(formatValue(v.isNull() ? 0.0f : v.asFloat(), s.decimals));
        };
        break;
    }
    case RowSpec::Slider:
    {
        row.height = kSliderRowH;
        addLabel(row.height - 46.0f);
        ui::ValueField* f = ui::ValueField::create(Size(250.0f, 84.0f), true);
        f->hitFilter = filter;
        f->setPosition(Vec2(w - 250.0f - kPad * 0.25f, row.height - 46.0f - 42.0f));
        node->addChild(f);
        ui::SliderBar* bar = ui::SliderBar::create(w - kPad * 0.25f);
        bar->hitFilter = filter;
        bar->setRange(spec.min, spec.max, spec.segments);
        bar->setPosition(Vec2(0.0f, 8.0f));
        node->addChild(bar);
        const RowSpec s = spec;
        bar->onChange = [this, key, f, s](float v, bool final, float start) {
            f->setText(formatValue(v, s.decimals));
            if (final) setValue(key, Value(v), true, Value(start));
            else setValue(key, Value(v), false, Value::Null);
        };
        f->onCommit = [this, key, s](const std::string& t) {
            const std::string trimmed = uikit::trimmedString(t);
            if (trimmed.empty()) return;
            const float v = std::max(s.min, std::min(s.max, std::strtof(trimmed.c_str(), nullptr)));
            setValue(key, Value(v), true, Value::Null);
            refreshValues();
        };
        row.refresh = [f, bar, current, s]() {
            const Value v = current();
            const float x = v.isNull() ? 0.0f : v.asFloat();
            f->setText(formatValue(x, s.decimals));
            bar->setValue(x);
        };
        break;
    }
    case RowSpec::Toggle:
    {
        row.height = kRowH;
        addLabel(row.height * 0.5f);
        ui::Toggle* t = ui::Toggle::create();
        t->hitFilter = filter;
        t->setPosition(Vec2(w - 140.0f - kPad * 0.25f, (row.height - 76.0f) * 0.5f));
        node->addChild(t);
        t->onChange = [this, key](bool on) { setValue(key, Value(on ? 1.0f : 0.0f), true, Value::Null); };
        row.refresh = [t, current]() {
            const Value v = current();
            t->setOn(!v.isNull() && v.asFloat() != 0.0f);
        };
        break;
    }
    case RowSpec::Choice:
    case RowSpec::Sound:
    {
        row.height = kRowH;
        addLabel(row.height * 0.5f);
        const float bw = spec.secondButtonText.empty() ? controlW : controlW - 104.0f;
        oui::Button* b = oui::Button::create("", Size(bw, 92.0f), oui::Button::window("blue"), 40.0f, oui::kFontBodyBold);
        b->setIcon(oui::iconSprite("chevron", 30.0f), true);
        b->setPosition(Vec2(w - kPad * 0.25f - controlW + bw * 0.5f, row.height * 0.5f));
        node->addChild(b);
        const RowSpec s = spec;
        b->setCallback([this, key, s, current]() {
            std::vector<std::string> items;
            int selected = -1;
            const Value v = current();
            const float value = v.isNull() ? 0.0f : v.asFloat();
            if (s.kind == RowSpec::Sound)
            {
                items = soundNames();
                selected = (int)value;
                ui::openListPicker("Sound effect", items, selected, [this, key](int i) {
                    setValue(key, Value((float)i), true, Value::Null);
                    refreshValues();
                }, true);
                return;
            }
            for (size_t i = 0; i < s.choices.size(); ++i)
            {
                items.push_back(s.choices[i]);
                if (std::fabs(s.choiceValues[i] - value) < 1e-3f) selected = (int)i;
            }
            const std::vector<float> values = s.choiceValues;
            ui::openListPicker(s.label, items, selected, [this, key, values](int i) {
                if (i >= 0 && i < (int)values.size()) setValue(key, Value(values[i]), true, Value::Null);
                refreshValues();
            }, items.size() > 12);
        });
        row.refresh = [b, current, s]() {
            const Value v = current();
            const float value = v.isNull() ? 0.0f : v.asFloat();
            std::string text = "-";
            if (s.kind == RowSpec::Sound)
            {
                const auto& names = soundNames();
                const int i = (int)value;
                text = i >= 0 && i < (int)names.size() ? names[i] : StringUtils::format("sound %d", i);
            }
            else
            {
                for (size_t i = 0; i < s.choices.size(); ++i)
                    if (std::fabs(s.choiceValues[i] - value) < 1e-3f) text = s.choices[i];
            }
            oui::setEllipsized(b->label(), text, b->getContentSize().width - 110.0f);
            b->setText(b->label()->getString());  // re-lays out the chevron after the text
        };
        break;
    }
    case RowSpec::Color:
    {
        row.height = kRowH;
        addLabel(row.height * 0.5f);
        ui::Swatch* sw = ui::Swatch::create(Size(controlW, 92.0f));
        sw->hitFilter = filter;
        sw->setPosition(Vec2(w - controlW - kPad * 0.25f, (row.height - 92.0f) * 0.5f));
        node->addChild(sw);
        const RowSpec s = spec;
        sw->onTap = [this, key, s, current]() {
            const Value v = current();
            ui::openColorPicker(s.label, v.isNull() ? 0 : (int)v.asFloat(), s.allowNone, [this, key](int c) {
                setValue(key, Value((float)c), true, Value::Null);
                refreshValues();
            });
        };
        row.refresh = [sw, current]() {
            const Value v = current();
            sw->setColor(v.isNull() ? 0 : (int)v.asFloat());
        };
        break;
    }
    case RowSpec::Header:
    case RowSpec::Info:
    {
        row.height = kHeaderH;
        auto* line = DrawNode::create();
        line->drawSegment(Vec2(kPad * 0.5f, row.height - 6.0f), Vec2(w - kPad * 0.5f, row.height - 6.0f), 2.0f,
                          Color4F(0, 0, 0, 0.12f));
        node->addChild(line);
        Label* l = ui::makeLabel(uikit::capitalizedString(spec.label), 34.0f, true, oui::kInkDim);
        l->setPosition(Vec2(kPad * 0.5f, row.height * 0.45f));
        oui::setEllipsized(l, spec.label, w - 300.0f);
        node->addChild(l);
        break;
    }
    case RowSpec::Button:
    {
        row.height = kButtonRowH;
        float bx = kPad * 0.25f;
        if (!spec.label.empty())
        {
            addLabel(row.height * 0.5f);
            bx = w - controlW - kPad * 0.25f;
        }
        const float bw = spec.label.empty() ? w - kPad * 0.5f : controlW;
        oui::Button* b = oui::Button::create(spec.buttonText, Size(bw, 110.0f), oui::Button::window(spec.buttonColor), 46.0f);
        b->setPosition(Vec2(bx + bw * 0.5f, row.height * 0.5f));
        node->addChild(b);
        b->setCallback([this, key]() {
            RefPtr<Inspector> keep(this);
            const std::vector<std::string> p = split(key, ':');
            if (p[0] == "actadd" && p.size() == 3)
            {
                TriggerRef* t = triggerWithUid(std::atoi(p[1].c_str()));
                const int i = std::atoi(p[2].c_str());
                if (!t || i < 0 || i >= (int)t->targets().size()) return;
                if ((int)t->targets()[i].actions.size() >= TriggerRef::kMaxActions) return;
                TriggerAction a;
                a.params = TriggerRef::defaultParams(t->targets()[i].ref.get(), 0);
                t->targets()[i].actions.push_back(a);
                if (EditorUndoManager* um = undoManager())
                {
                    um->prepareWithInvocationTarget(t, [i](TriggerRef* tr) {
                        if (i < (int)tr->targets().size() && tr->targets()[i].actions.size() > 1)
                            tr->targets()[i].actions.pop_back();
                        refreshPanelLater();
                    });
                    uikit::NotificationCenter::postNotification(uikit::notification::kUndoStackUpdated, um);
                }
                refreshPanelLater();
                return;
            }
            if (onFunction) onFunction(key);
        });
        break;
    }
    }
    if (!spec.secondButtonText.empty())
    {
        const bool tiny = spec.secondButtonText == "x";
        const Size bs(tiny ? 92.0f : 220.0f, tiny ? 92.0f : 84.0f);
        oui::Button* b2 = oui::Button::create(tiny ? "" : spec.secondButtonText, bs, oui::Button::window("pink"), 38.0f);
        if (tiny) b2->setIcon(oui::iconSprite("clear", 40.0f));
        b2->setPosition(Vec2(w - kPad * 0.25f - bs.width * 0.5f, row.height * 0.5f));
        node->addChild(b2);
        const std::string second = spec.secondKey;
        b2->setCallback([this, second]() {
            RefPtr<Inspector> keep(this);
            const std::vector<std::string> p = split(second, ':');
            if (p[0] == "unlink" && p.size() == 3)
            {
                if (TriggerRef* t = triggerWithUid(std::atoi(p[1].c_str()))) t->removeTargetAt(std::atoi(p[2].c_str()), true);
                return;
            }
            if (p[0] == "actdel" && p.size() == 4)
            {
                TriggerRef* t = triggerWithUid(std::atoi(p[1].c_str()));
                const int i = std::atoi(p[2].c_str()), a = std::atoi(p[3].c_str());
                if (!t || i < 0 || i >= (int)t->targets().size()) return;
                auto& actions = t->targets()[i].actions;
                if (actions.size() <= 1 || a < 0 || a >= (int)actions.size()) return;
                const TriggerAction removed = actions[a];
                actions.erase(actions.begin() + a);
                if (EditorUndoManager* um = undoManager())
                {
                    um->prepareWithInvocationTarget(t, [i, a, removed](TriggerRef* tr) {
                        if (i >= (int)tr->targets().size()) return;
                        auto& list = tr->targets()[i].actions;
                        list.insert(list.begin() + std::min(a, (int)list.size()), removed);
                        refreshPanelLater();
                    });
                    uikit::NotificationCenter::postNotification(uikit::notification::kUndoStackUpdated, um);
                }
                refreshPanelLater();
            }
        });
    }
    if (row.refresh) row.refresh();
    _content->addChild(node);
    _rows.push_back(row);
}

void Inspector::layoutRows()
{
    float total = 0.0f;
    for (const Row& r : _rows) total += r.height;
    _contentHeight = total;
    const float maxScroll = std::max(0.0f, _contentHeight - _viewport.size.height);
    _scroll = std::max(0.0f, std::min(maxScroll, _scroll));
    float y = _viewport.getMaxY() + _scroll;
    for (Row& r : _rows)
    {
        y -= r.height;
        r.node->setPosition(Vec2(_viewport.origin.x, y));
        // Rows (and their controls) outside the viewport are hidden so they can't be hit.
        const bool visible = y + r.height > _viewport.origin.y - 1.0f && y < _viewport.getMaxY() + 1.0f;
        r.node->setVisible(visible);
    }
}

void Inspector::scrollBy(float dy)
{
    _scroll += dy;
    layoutRows();
}

}  // namespace flashed
