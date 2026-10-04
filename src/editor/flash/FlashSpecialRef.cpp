// EDITOR (browser features, PC addition): see FlashSpecialRef.h.
#include "FlashSpecialRef.h"

#include <algorithm>
#include <cmath>

#include "external/tinyxml2/tinyxml2.h"
#include "FlashEditor.h"
#include "InputObject.h"

USING_NS_CC;
using namespace flashed;

namespace {

std::string formatNumber(float v)
{
    if (std::fabs(v - std::round(v)) < 1e-4f) return StringUtils::format("%d", (int)std::lround(v));
    std::string s = StringUtils::format("%.3f", v);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

bool isBoolAttr(const std::string& key)
{
    const Attr* a = attribute(key);
    return a && a->input == Input::Switch;
}

}  // namespace

FlashSpecialRef* FlashSpecialRef::create(int type)
{
    FlashSpecialRef* ref = new (std::nothrow) FlashSpecialRef();
    if (ref && ref->initWithType(type))
    {
        ref->autorelease();
        return ref;
    }
    delete ref;
    return nullptr;
}

bool FlashSpecialRef::initWithType(int type)
{
    _info = specialInfo(type);
    if (!_info || !Special::initWithSpriteFrameName("e_1x1.png")) return false;
    setLevelItemID(type);
    setOpacity(0);  // the 1x1 placeholder pixel; children carry the art
    for (const char* key : _info->props) _params[key] = 0.0f;
    for (const auto& d : _info->defaults) _params[d.first] = d.second;
    propertyKeys() = {"flashX", "flashY"};
    setCanRotate(_info->rotatable);
    rebuildArt();
    updateCounts();
    return true;
}

float FlashSpecialRef::param(const std::string& key) const
{
    auto it = _params.find(key);
    return it == _params.end() ? 0.0f : it->second;
}

bool FlashSpecialRef::interactiveItem() const
{
    auto it = _params.find("interactive");
    return it == _params.end() || it->second != 0.0f;
}

float FlashSpecialRef::clampParam(const std::string& key, float value) const
{
    // Setter clamps of the Flash refs (size limits) and the attribute ranges.
    if (key == "shapeWidth" || key == "shapeHeight")
    {
        switch (_info->type)
        {
        case 11: return std::max(200.0f, std::min(600.0f, value));                   // Meteor 0.5..1.5 x 400
        case 18: return key == "shapeWidth" ? std::max(5.0f, std::min(50.0f, value))  // Glass 0.05..0.5 x 100
                                            : std::max(50.0f, std::min(500.0f, value));
        case 27: return key == "shapeWidth" ? std::max(100.0f, std::min(2000.0f, value)) : 18.0f;  // Rail
        default: return std::max(1.0f, value);
        }
    }
    if (key == "caption") return value;
    const Attr* a = attribute(key);
    if (!a) return value;
    if (a->input == Input::Switch) return value != 0.0f ? 1.0f : 0.0f;
    if (a->input == Input::Color) return (float)((int)value & 0xffffff);
    if (a->input == Input::Slider || a->input == Input::Choice)
    {
        value = std::max(a->min, std::min(a->max, value));
        if (a->segments > 0 && std::fabs(a->max - a->min - (float)a->segments) < 1e-3f) value = std::round(value);
    }
    return value;
}

void FlashSpecialRef::setParam(const std::string& key, float value)
{
    value = clampParam(key, value);
    if (_info->type == 11 && (key == "shapeWidth" || key == "shapeHeight"))
    {
        _params["shapeWidth"] = _params["shapeHeight"] = value;  // MeteorRef keeps it round
    }
    else
    {
        _params[key] = value;
    }
    const bool keysChange = key == "interactive";
    rebuildArt();
    updateCounts();
    if (keysChange) uiKeysChangedLater(this);
}

void FlashSpecialRef::updateCounts()
{
    unsigned int shapes = 0, art = 0;
    if (_info->type == 30)
    {
        const unsigned int links = (unsigned int)param("linkCount");
        shapes = interactiveItem() ? links : 0;
        art = interactiveItem() ? 0 : links;
    }
    else if (interactiveItem())
    {
        shapes = (unsigned int)_info->shapes;
    }
    else
    {
        art = (unsigned int)_info->art;
    }
    if (shapes != shapeCount()) setShapeCount(shapes);
    if (art != artCount()) setArtCount(art);
}

void FlashSpecialRef::rebuildArt()
{
    if (_art) _art->removeFromParent();
    _art = Node::create();
    addChild(_art);
    _drawnBox = false;
    const int t = _info->type;
    const float px = pxToStageLength(1.0f);
    // getContentSize of this placeholder is 1x1: children are positioned around (0.5, 0.5).
    const Vec2 origin(getContentSize().width * 0.5f, getContentSize().height * 0.5f);
    _art->setPosition(origin);

    std::string artName = _info->art0;
    if (t == 17) artName = StringUtils::format("ed_npc_%d", (int)param("charIndex"));
    if (t == 23) artName = StringUtils::format("ed_sign_%d", (int)param("signPostType"));
    if (t == 32) artName = StringUtils::format("ed_food_%d", (int)param("foodItemType"));
    if (t == 31) artName = StringUtils::format("coin_face_%d", std::max(1, std::min(6, (int)param("tokenType"))));

    Rect box(-_info->width * 0.5f * px, -_info->height * 0.5f * px, _info->width * px, _info->height * px);
    if (t == 16)
    {
        // TextBoxRef: a TextField at (x, y) top-left, 2 px gutter.
        const int font = (int)param("font");
        const float size = param("fontSize");
        const float ttf = std::min(72.0f, std::max(10.0f, size));
        Label* label = nullptr;
        const std::string file = fontFile(font);
        std::string text = _caption;
        std::replace(text.begin(), text.end(), '\r', '\n');
        const TextHAlignment align = param("align") == 2 ? TextHAlignment::CENTER
                                     : param("align") == 3 ? TextHAlignment::RIGHT
                                                           : TextHAlignment::LEFT;
        if (!file.empty()) label = Label::createWithTTF(TTFConfig(file, ttf), text, align);
        if (!label)
            label = Label::createWithSystemFont(text, (font == 3 || font == 5) ? "Arial Bold" : "Arial", ttf,
                                                Size::ZERO, align);
        const int c = (int)param("color");
        label->setTextColor(Color4B((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff, 255));
        label->setOpacity((GLubyte)std::lround(std::max(0.15f, param("opacity") * 0.01f) * 255.0f));
        label->setAnchorPoint(Vec2(0.0f, 1.0f));
        label->setScale(size / ttf * px);
        label->setPosition(Vec2(2.0f * px, -2.0f * px));
        _art->addChild(label);
        const Size ls = label->getContentSize() * label->getScale();
        box = Rect(0.0f, -(ls.height + 4.0f * px), ls.width + 4.0f * px, ls.height + 4.0f * px);
        _drawnBox = true;  // Flash draws the text field's outline in the editor
    }
    else if (t == 13 || t == 14)
    {
        const float w = param("floorWidth") * 300.0f * px;
        const float h = (param("numFloors") * 165.0f + 100.0f) * px;
        box = Rect(0.0f, -h, w, h);  // x/y is the top-left corner
        _drawnBox = true;
    }
    else if (t == 18 || t == 30)
    {
        const float w = (t == 18 ? param("shapeWidth") : 12.0f * (1.0f + (param("linkScale") - 1.0f) / 4.5f)) * px;
        const float h = (t == 18 ? param("shapeHeight")
                                 : param("linkCount") * 9.0f * (1.0f + (param("linkScale") - 1.0f) / 4.5f)) * px;
        box = t == 18 ? Rect(-w * 0.5f, -h * 0.5f, w, h) : Rect(-w * 0.5f, -h, w, h);
        _drawnBox = true;
    }
    else if (Sprite* sprite = flashArtSprite(artName))
    {
        if (t == 11)
        {
            sprite->setScale(sprite->getScale() * param("shapeWidth") / 400.0f);
        }
        else if (t == 17)
        {
            sprite->setScale(sprite->getScale() * 0.5f);  // NPCharacterRef shows its NPCSprite at half size
        }
        else if (t == 27)
        {
            const float artW = sprite->getContentSize().width * sprite->getScaleX();
            if (artW > 0.0f) sprite->setScaleX(sprite->getScaleX() * param("shapeWidth") * px / artW);
        }
        if (param("reverse") != 0.0f && (t == 19 || t == 24 || t == 17 || t == 35)) sprite->setScaleX(-sprite->getScaleX());
        _art->addChild(sprite);
        box = sprite->getBoundingBox();
    }
    else
    {
        if (t == 11) box = Rect(-param("shapeWidth") * 0.5f * px, -param("shapeWidth") * 0.5f * px,
                                param("shapeWidth") * px, param("shapeWidth") * px);
        if (t == 27) box = Rect(-param("shapeWidth") * 0.5f * px, -9.0f * px, param("shapeWidth") * px, 18.0f * px);
        _drawnBox = true;
        Label* label = Label::createWithSystemFont(_info->name, "Arial", 64.0f);
        label->setScale(std::min(box.size.width * 0.8f / std::max(1.0f, label->getContentSize().width),
                                 box.size.height * 0.5f / std::max(1.0f, label->getContentSize().height)));
        label->setTextColor(Color4B(60, 60, 70, 255));
        label->setPosition(Vec2(box.getMidX(), box.getMidY()));
        _art->addChild(label);
    }
    _footprint = box.size;
    setRefRect(cg::Rect(box.origin.x + origin.x, box.origin.y + origin.y, box.size.width, box.size.height));
}

void FlashSpecialRef::setRotation(float rotation)
{
    Special::setRotation(_info && !_info->rotatable ? 0.0f : rotation);
}

void FlashSpecialRef::updateOverlayWithNode(DrawNode* node)
{
    if (!_drawnBox) return;
    const cg::Rect r = refRect();
    const AffineTransform t = getNodeToParentAffineTransform();
    Vec2 v[4] = {Vec2((float)r.origin.x, (float)r.origin.y),
                 Vec2((float)(r.origin.x + r.size.width), (float)r.origin.y),
                 Vec2((float)(r.origin.x + r.size.width), (float)(r.origin.y + r.size.height)),
                 Vec2((float)r.origin.x, (float)(r.origin.y + r.size.height))};
    for (Vec2& p : v) p = PointApplyAffineTransform(p, t);
    const int type = _info->type;
    Color4F fill(0.85f, 0.85f, 0.88f, 0.6f);
    Color4F line(0.35f, 0.35f, 0.4f, 1.0f);
    if (type == 18) { fill = Color4F(0.75f, 0.9f, 1.0f, 0.45f); line = Color4F(0.45f, 0.65f, 0.8f, 1.0f); }
    if (type == 13 || type == 14) { fill = type == 13 ? Color4F(0.44f, 0.44f, 0.47f, 0.9f) : Color4F(0.55f, 0.48f, 0.42f, 0.9f); }
    if (type == 16) { fill = Color4F(0, 0, 0, 0); line = Color4F(0.99f, 0.5f, 0.5f, 1.0f); }  // TextBoxRef outline 0xfd8181
    if (type == 30)
    {
        // Chain: the links along its curve.
        const float px = pxToStageLength(1.0f);
        const int links = (int)param("linkCount");
        const float s = 1.0f + (param("linkScale") - 1.0f) / 9.0f * 2.0f;
        const float step = 9.0f * s;
        const float turn = -(param("linkAngle") / 10.0f * (360.0f / (links * 2))) * 3.14159265f / 180.0f;
        Vec2 p(0, 0);
        for (int i = 0; i < links; ++i)
        {
            const float a = 3.14159265f / 2.0f + ((i + 1) * turn - turn * 0.5f);
            const Vec2 d(std::cos(a) * step, std::sin(a) * step);
            const Vec2 c = p + d * 0.5f;
            // Flash y down -> stage y up
            const Vec2 local(c.x * px, -c.y * px);
            node->drawDot(PointApplyAffineTransform(local + Vec2(getContentSize() * 0.5f), t), 3.0f * s * px,
                          Color4F(0.29f, 0.29f, 0.29f, 1.0f));
            p += d;
        }
        return;
    }
    node->drawPolygon(v, 4, fill, 1.0f / std::max(0.01f, getParent() && getParent()->getParent() ? getParent()->getParent()->getScale() : 1.0f), line);
    if (type == 13 || type == 14)
    {
        // Window rows, one per floor.
        const float px = pxToStageLength(1.0f);
        const int floors = (int)param("numFloors");
        const int columns = (int)param("floorWidth") * 2;
        for (int f = 0; f < floors; ++f)
        {
            for (int c = 0; c < columns; ++c)
            {
                const float x0 = (float)r.origin.x + (20.0f + c * 150.0f) * px;
                const float y0 = (float)(r.origin.y + r.size.height) - (130.0f + f * 165.0f) * px;
                Vec2 w[4] = {Vec2(x0, y0), Vec2(x0 + 110 * px, y0), Vec2(x0 + 110 * px, y0 + 90 * px),
                             Vec2(x0, y0 + 90 * px)};
                for (Vec2& p : w) p = PointApplyAffineTransform(p, t);
                node->drawPolygon(w, 4, Color4F(0.62f, 0.75f, 0.85f, 0.9f), 0, Color4F());
            }
        }
    }
}

// ---- KVC --------------------------------------------------------------------------------------------

Value FlashSpecialRef::valueForKey(const std::string& key)
{
    if (key == "flashX") return Value(stageToPx(getPosition()).x);
    if (key == "flashY") return Value(stageToPx(getPosition()).y);
    if (key == "caption") return Value(_caption);
    if (key == "angle") return Value(normalizedAngle(getRotation()));
    if (_params.count(key)) return Value(_params[key]);
    return Special::valueForKey(key);
}

void FlashSpecialRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "flashX" || key == "flashY")
    {
        Vec2 px = stageToPx(getPosition());
        (key == "flashX" ? px.x : px.y) = kvcFloat(value);
        const Vec2 s = pxToStage(px.x, px.y);
        KeyValueChange kvo(this, key.c_str());
        setX(s.x, s.y);
        return;
    }
    if (key == "caption")
    {
        KeyValueChange kvo(this, "caption");
        _caption = value.getType() == Value::Type::STRING ? value.asString() : value.asString();
        if (_caption.size() > 200) _caption.resize(200);  // TextField.maxChars
        rebuildArt();
        return;
    }
    if (_params.count(key))
    {
        KeyValueChange kvo(this, key.c_str());
        setParam(key, kvcFloat(value));
        return;
    }
    Special::setValueForKey(value, key);
}

std::vector<std::string> FlashSpecialRef::propertyKeysForUI()
{
    std::vector<std::string> keys = {"flashX", "flashY"};
    if (_info->rotatable) keys.push_back("angle");
    for (const char* k : _info->ui) keys.push_back(k);
    return keys;
}

InputObject* FlashSpecialRef::inputObjectForPropertyWithRect(const std::string& property, const Rect& rect)
{
    // The Inspector builds its own rows from FlashCatalog; this serves the iOS-style panel.
    if (property == "flashX" || property == "flashY")
    {
        InputObject* io = InputObject::create(rect, property == "flashX" ? "x" : "y", property,
                                              valueForKey(property).asFloat(), true);
        return io;
    }
    if (const Attr* a = attribute(property))
        if (a->input != Input::Text) return makeInput(*a, property, valueForKey(property).asFloat(), rect);
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// ---- copy / paste, XML --------------------------------------------------------------------------

ValueMap FlashSpecialRef::properties()
{
    ValueMap dict;
    dict["t"] = Value(_info->type);
    dict["flash"] = Value(true);
    for (const auto& p : _params) dict[p.first] = Value(p.second);
    dict["caption"] = Value(_caption);
    dict["angle"] = Value(getRotation());
    dict["x"] = Value(getPosition().x);
    dict["y"] = Value(getPosition().y);
    return dict;
}

void FlashSpecialRef::setProperties(const ValueMap& properties)
{
    for (const auto& kv : properties)
    {
        if (kv.first == "caption") _caption = kv.second.asString();
        else if (_params.count(kv.first)) _params[kv.first] = clampParam(kv.first, kv.second.asFloat());
    }
    auto it = properties.find("angle");
    if (it != properties.end()) setRotation(it->second.asFloat());
    auto x = properties.find("x");
    auto y = properties.find("y");
    if (x != properties.end() && y != properties.end()) setX(x->second.asFloat(), y->second.asFloat());
    rebuildArt();
    updateCounts();
}

void FlashSpecialRef::readFlash(const tinyxml2::XMLElement* e)
{
    for (size_t i = 0; i < _info->props.size(); ++i)
    {
        const std::string key = _info->props[i];
        const std::string name = "p" + std::to_string(i);
        if (key == "x" || key == "y") continue;
        if (key == "caption")
        {
            const tinyxml2::XMLElement* c = e->FirstChildElement(name.c_str());
            const char* text = c ? c->GetText() : e->Attribute(name.c_str());
            if (text) _caption = text;
            continue;
        }
        const char* v = e->Attribute(name.c_str());
        if (!v || !*v) continue;
        float value;
        if (!strcmp(v, "t") || !strcmp(v, "true")) value = 1.0f;
        else if (!strcmp(v, "f") || !strcmp(v, "false")) value = 0.0f;
        else value = (float)std::atof(v);
        if (key == "angle") setRotation(value);
        else if (key == "opacity" && _info->type == 16) _params[key] = std::max(0.0f, std::min(100.0f, value));
        else _params[key] = clampParam(key, value);
    }
    rebuildArt();
    updateCounts();
}

void FlashSpecialRef::writeFlash(std::string& attrs, std::string& childXml, const Vec2& positionPx, float angle)
{
    for (size_t i = 0; i < _info->props.size(); ++i)
    {
        const std::string key = _info->props[i];
        const std::string name = "p" + std::to_string(i);
        std::string text;
        if (key == "x") text = formatNumber(positionPx.x);
        else if (key == "y") text = formatNumber(positionPx.y);
        else if (key == "angle") text = formatNumber(angle);
        else if (key == "caption")
        {
            std::string caption = _caption;
            // ]]> can't appear inside CDATA
            for (size_t pos; (pos = caption.find("]]>")) != std::string::npos;) caption.replace(pos, 3, "]] >");
            childXml += "<" + name + "><![CDATA[" + caption + "]]></" + name + ">";
            continue;
        }
        else if (isBoolAttr(key)) text = param(key) != 0.0f ? "t" : "f";
        else text = formatNumber(param(key));
        attrs += " " + name + "=\"" + text + "\"";
    }
}
