// EDITOR (browser features, PC addition): see FlashLevelIO.h.
#include "FlashLevelIO.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

#include "cocos2d.h"
#include "external/tinyxml2/tinyxml2.h"

#include "CharacterRef.h"
#include "CircleRefShape.h"
#include "EditorSettings.h"
#include "EditorSpriteBatchNode.h"
#include "FlashEditor.h"
#include "FlashSpecialRef.h"
#include "GroupRef.h"
#include "JointRef.h"
#include "PolygonRefShape.h"
#include "RectangleRefShape.h"
#include "RefShape.h"
#include "TriangleRefShape.h"
#include "TriggerRef.h"
#include "online/FlashGeometry.h"
#include "online/FlashLevelConverter.h"

USING_NS_CC;
using tinyxml2::XMLElement;

namespace flashed {

namespace {

std::string num(double v)
{
    if (!std::isfinite(v)) v = 0.0;
    if (std::fabs(v - std::round(v)) < 1e-4) return StringUtils::format("%lld", (long long)std::llround(v));
    std::string s = StringUtils::format("%.3f", v);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

std::string attr(const char* name, const std::string& value) { return std::string(" ") + name + "=\"" + value + "\""; }
std::string attrN(const char* name, double v) { return attr(name, num(v)); }
std::string attrB(const char* name, bool v) { return attr(name, v ? "t" : "f"); }

bool isLengthKey(const std::string& key)
{
    return key.size() > 6 && key.compare(key.size() - 6, 6, "Meters") == 0 && key != "xMeters" && key != "yMeters";
}

float parseFloat(const char* s, float def)
{
    if (!s || !*s) return def;
    if (!strcmp(s, "t") || !strcmp(s, "true")) return 1.0f;
    if (!strcmp(s, "f") || !strcmp(s, "false")) return 0.0f;
    char* end = nullptr;
    const double d = std::strtod(s, &end);
    return (end == s || !std::isfinite(d)) ? def : (float)d;
}

float attrF(const XMLElement* e, const char* key, float def) { return parseFloat(e->Attribute(key), def); }

bool attrBool(const XMLElement* e, const char* key, bool def)
{
    const char* v = e->Attribute(key);
    if (!v) return def;
    return !strcmp(v, "t") || !strcmp(v, "true") || !strcmp(v, "1");
}

// ---- writer --------------------------------------------------------------------------------------

struct Indices
{
    std::map<Special*, int> shapes, specials, groups, joints, triggers;
};

std::string shapeNode(Special* ref, bool inVehicle, GroupRef* group, int* vertexId)
{
    RefShape* shape = static_cast<RefShape*>(ref);
    const int t = shape->levelItemID() - 6000;
    const Vec2 pos = stageToPx(shape->getPosition());
    std::string s = "<sh" + attrN("t", t);
    if (!shape->interactive()) s += attr("i", "f");
    else if (inVehicle) s += attrB("h", !(group && group->nonHandles.count(ref)));
    PolygonRefShape* poly = dynamic_cast<PolygonRefShape*>(ref);
    const float w = poly ? poly->extentPx(true) : stageToPxLength(shape->width());
    const float h = poly ? poly->extentPx(false) : stageToPxLength(shape->height());
    s += attrN("p0", pos.x) + attrN("p1", pos.y) + attrN("p2", w) + attrN("p3", t == 1 ? w : h);
    s += attrN("p4", normalizedAngle(shape->getRotation()));
    s += attrB("p5", shape->fixed()) + attrB("p6", shape->sleeping()) +
         (std::isnan(shape->density()) ? attr("p7", "NaN") : attrN("p7", shape->density()));
    // (p8 -1: no fill, kept from a browser level; EDITOR (browser features, PC addition))
    s += attrN("p8", shape->noFill() ? -1.0 : (double)shape->color()) + attrN("p9", shape->outlineColor()) +
         attrN("p10", shape->shapeOpacity());
    s += attrN("p11", shape->collision());
    if (t == 1) s += attrN("p12", static_cast<CircleRefShape*>(ref)->innerCutout());
    if (!poly)
    {
        s += "/>";
        return s;
    }
    s += "><v" + attrB("f", poly->closed()) + attrN("id", (*vertexId)++) + attrN("n", poly->vertsPx().size());
    int i = 0;
    const bool handles = poly->hasHandles();
    for (size_t k = 0; k < poly->vertsPx().size(); ++k)
    {
        const Vec2& v = poly->vertsPx()[k];
        std::string text = num(v.x) + "_" + num(v.y);
        if (handles)
        {
            const Vec2& a = poly->handlesIn()[k];
            const Vec2& b = poly->handlesOut()[k];
            if (a != Vec2::ZERO || b != Vec2::ZERO)
                text += "_" + num(a.x) + "_" + num(a.y) + "_" + num(b.x) + "_" + num(b.y);
        }
        s += attr(("v" + std::to_string(i++)).c_str(), text);
    }
    s += "/></sh>";
    return s;
}

std::string specialNode(Special* ref)
{
    const Vec2 pos = stageToPx(ref->getPosition());
    if (FlashSpecialRef* f = dynamic_cast<FlashSpecialRef*>(ref))
    {
        std::string attrs, children;
        f->writeFlash(attrs, children, pos, normalizedAngle(f->getRotation()));
        std::string s = "<sp" + attrN("t", f->type()) + attrs;
        return children.empty() ? s + "/>" : s + ">" + children + "</sp>";
    }
    // The iOS editor's refs: same p-order as the browser classes; positions / lengths in px.
    std::string s = "<sp" + attrN("t", ref->levelItemID());
    const std::vector<std::string> keys = ref->propertyKeys();
    for (size_t i = 0; i < keys.size(); ++i)
    {
        const std::string& key = keys[i];
        const std::string name = "p" + std::to_string(i);
        if (key == "xMeters") { s += attrN(name.c_str(), pos.x); continue; }
        if (key == "yMeters") { s += attrN(name.c_str(), pos.y); continue; }
        Value v = ref->valueForKey(key);
        if (v.getType() == Value::Type::BOOLEAN) { s += attrB(name.c_str(), v.asBool()); continue; }
        float f = v.isNull() ? 0.0f : v.asFloat();
        if (key == "angle") f = normalizedAngle(f);
        else if (isLengthKey(key) && ref->levelItemID() != 5001) f *= kPxPerMetre;
        s += attrN(name.c_str(), f);
    }
    return s + "/>";
}

bool isShapeRef(Special* r) { return dynamic_cast<RefShape*>(r) != nullptr; }
bool isLogicRef(Special* r)
{
    return dynamic_cast<GroupRef*>(r) || dynamic_cast<JointRef*>(r) || dynamic_cast<TriggerRef*>(r) ||
           dynamic_cast<CharacterRef*>(r);
}

std::string bodyRef(Special* body, const Indices& ix)
{
    if (!body) return "-1";
    auto s = ix.shapes.find(body);
    if (s != ix.shapes.end()) return num(s->second);
    auto p = ix.specials.find(body);
    if (p != ix.specials.end()) return "s" + num(p->second);
    auto g = ix.groups.find(body);
    if (g != ix.groups.end()) return "g" + num(g->second);
    return "";
}

}  // namespace

// ---- format detection --------------------------------------------------------------------------------

bool isBrowserLevelXml(const std::string& xml)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return false;
    const XMLElement* root = doc.RootElement();
    const XMLElement* info = root ? root->FirstChildElement("info") : nullptr;
    if (!info) return false;
    if (info->Attribute("src") || info->Attribute("fm") || info->Attribute("ptm")) return false;
    if (info->Attribute("ow")) return true;
    return attrF(info, "x", 0) > 330.0f || attrF(info, "y", 0) > 170.0f;
}

std::string playableLevelXml(const std::string& xml, bool* converted, int* character, bool* forceCharacter)
{
    if (converted) *converted = false;
    if (!isBrowserLevelXml(xml)) return xml;
    online::ConversionReport report;
    std::string mobile = online::FlashLevelConverter::toMobile(xml, &report);
    if (!report.ok) return xml;
    for (const std::string& w : report.warnings) log("editor level: %s", w.c_str());
    if (converted) *converted = true;
    if (character) *character = report.character;
    if (forceCharacter) *forceCharacter = report.forceCharacter;
    return mobile;
}

// ---- writer ------------------------------------------------------------------------------------------

std::string writeBrowserLevel(EditorSpriteBatchNode* sbn, const LevelInfo& info)
{
    const Vector<Special*>& refs = sbn->refs();
    CharacterRef* character = sbn->characterRef();
    std::vector<Special*> shapes, specials, joints, triggers;
    std::vector<GroupRef*> groups;
    for (Special* r : refs)
    {
        if (r == character) continue;
        if (GroupRef* g = dynamic_cast<GroupRef*>(r))
        {
            if (!g->liveMembers().empty()) groups.push_back(g);
            continue;
        }
        if (dynamic_cast<JointRef*>(r)) { joints.push_back(r); continue; }
        if (dynamic_cast<TriggerRef*>(r)) { triggers.push_back(r); continue; }
        if (r->group() && onStage(r->group())) continue;  // written inside its group
        if (isShapeRef(r)) shapes.push_back(r);
        else if (!isLogicRef(r)) specials.push_back(r);
    }
    Indices ix;
    for (size_t i = 0; i < shapes.size(); ++i) ix.shapes[shapes[i]] = (int)i;
    for (size_t i = 0; i < specials.size(); ++i) ix.specials[specials[i]] = (int)i;
    for (size_t i = 0; i < groups.size(); ++i) ix.groups[groups[i]] = (int)i;
    std::vector<Special*> liveJoints;
    for (Special* j : joints)
    {
        JointRef* joint = static_cast<JointRef*>(j);
        Special* b1 = onStage(joint->body1()) ? joint->body1() : nullptr;
        Special* b2 = onStage(joint->body2()) ? joint->body2() : nullptr;
        if (!b1 && !b2) continue;
        ix.joints[j] = (int)liveJoints.size();
        liveJoints.push_back(j);
    }
    for (size_t i = 0; i < triggers.size(); ++i) ix.triggers[triggers[i]] = (int)i;

    const Vec2 start = stageToPx(character->getPosition());
    std::string xml = "<levelXML><info" + attr("v", "1.87") + attrN("x", start.x) + attrN("y", start.y) +
                      attrN("c", character->defaultCharacter()) + attrB("f", character->forceCharacter() != 0) +
                      attrB("h", character->hideVehicle()) + attrN("bg", info.background) +
                      attrN("bgc", info.backgroundColor & 0xffffff) + attr("e", "1") + attr("ow", "1") + "/>";
    int vertexId = 1;
    if (!shapes.empty())
    {
        xml += "<shapes>";
        for (Special* s : shapes) xml += shapeNode(s, false, nullptr, &vertexId);
        xml += "</shapes>";
    }
    if (!specials.empty())
    {
        xml += "<specials>";
        for (Special* s : specials) xml += specialNode(s);
        xml += "</specials>";
    }
    if (!groups.empty())
    {
        xml += "<groups>";
        for (GroupRef* g : groups)
        {
            g->recenter();
            const Vec2 c = stageToPx(g->getPosition());
            xml += "<g" + attrN("x", c.x) + attrN("y", c.y) + attrN("r", 0) + attrN("ox", -c.x) + attrN("oy", -c.y) +
                   attrB("s", g->sleeping) + attrB("f", g->foreground) + attrN("o", g->opacity) +
                   attrB("im", g->immovable) + attrB("fr", g->fixedRotation);
            if (g->vehicle)
                xml += attr("v", "t") + attrN("sb", g->spaceAction) + attrN("sh", g->shiftAction) +
                       attrN("ct", g->ctrlAction) + attrN("a", g->acceleration) + attrN("l", g->leaningStrength) +
                       attrN("cp", g->characterPose) + attrB("lo", g->lockJoints);
            xml += ">";
            Vector<Special*> live = g->liveMembers();
            for (Special* m : live)
                if (isShapeRef(m)) xml += shapeNode(m, g->vehicle, g, &vertexId);
            for (Special* m : live)
                if (!isShapeRef(m)) xml += specialNode(m);
            xml += "</g>";
        }
        xml += "</groups>";
    }
    if (!liveJoints.empty())
    {
        xml += "<joints>";
        for (Special* j : liveJoints)
        {
            JointRef* joint = static_cast<JointRef*>(j);
            Special* b1 = onStage(joint->body1()) ? joint->body1() : nullptr;
            Special* b2 = onStage(joint->body2()) ? joint->body2() : nullptr;
            if (!b1) std::swap(b1, b2);
            const Vec2 p = stageToPx(joint->getPosition());
            xml += "<j" + attrN("t", joint->prismatic() ? 1 : 0) + attrN("x", p.x) + attrN("y", p.y) +
                   attr("b1", bodyRef(b1, ix)) + attr("b2", bodyRef(b2, ix));
            if (joint->prismatic())
                xml += attrN("a", joint->axisAngle) + attrB("l", joint->limit) + attrN("ul", joint->upper) +
                       attrN("ll", joint->lower) + attrB("m", joint->motor) + attrN("fo", joint->torque) +
                       attrN("sp", joint->speed);
            else
                xml += attrB("l", joint->limit) + attrN("ua", joint->upper) + attrN("la", joint->lower) +
                       attrB("m", joint->motor) + attrN("tq", joint->torque) + attrN("sp", joint->speed);
            xml += attrB("c", joint->collideSelf);
            if (joint->vehicleAttached()) xml += attrB("v", joint->vehicleControlled);
            xml += "/>";
        }
        xml += "</joints>";
    }
    if (!triggers.empty())
    {
        xml += "<triggers>";
        for (Special* r : triggers)
        {
            TriggerRef* t = static_cast<TriggerRef*>(r);
            const Vec2 p = stageToPx(t->getPosition());
            xml += "<t" + attrN("x", p.x) + attrN("y", p.y) + attrN("w", t->widthPx()) + attrN("h", t->heightPx()) +
                   attrN("a", normalizedAngle(t->getRotation())) + attrN("b", t->triggeredBy) +
                   attrN("t", t->typeIndex) + attrN("r", t->repeatType) + attrB("sd", t->startDisabled);
            if (t->repeatType > 2) xml += attrN("i", t->repeatInterval);
            if (t->typeIndex == 1) xml += attrN("d", t->delay);
            else if (t->typeIndex == 2)
                xml += attrN("s", t->sound) + attrN("d", t->delay) + attrN("l", t->soundLocation) +
                       attrN("p", t->panning) + attrN("v", t->volume);
            std::string targets;
            if (t->hasTargets())
            {
                for (const TriggerTarget& target : t->targets())
                {
                    Special* ref = target.ref.get();
                    if (!onStage(ref)) continue;
                    const char* tag = nullptr;
                    int index = -1;
                    auto find = [&](const std::map<Special*, int>& m, const char* name) {
                        auto it = m.find(ref);
                        if (it != m.end() && !tag) { tag = name; index = it->second; }
                    };
                    find(ix.shapes, "sh");
                    find(ix.specials, "sp");
                    find(ix.groups, "g");
                    find(ix.joints, "j");
                    find(ix.triggers, "t");
                    if (!tag) continue;
                    targets += std::string("<") + tag + attrN("i", index) + ">";
                    if (t->hasActions())
                    {
                        const std::vector<ActionInfo>* list = actionsFor(ref);
                        for (const TriggerAction& a : target.actions)
                        {
                            targets += "<a" + attrN("i", a.index);
                            if (list && a.index >= 0 && a.index < (int)list->size())
                            {
                                const auto& keys = (*list)[a.index].params;
                                for (size_t k = 0; k < keys.size(); ++k)
                                    targets += attrN(("p" + std::to_string(k)).c_str(), k < a.params.size() ? a.params[k] : 0.0f);
                            }
                            targets += "/>";
                        }
                    }
                    targets += std::string("</") + tag + ">";
                }
            }
            xml += targets.empty() ? "/>" : ">" + targets + "</t>";
        }
        xml += "</triggers>";
    }
    xml += "</levelXML>";
    return xml;
}

// ---- reader ---------------------------------------------------------------------------------------------

namespace {

struct Reader
{
    EditorSpriteBatchNode* sbn = nullptr;
    float version = 1.87f;
    std::vector<Special*> shapes, specials, groups, joints, triggers;
    std::map<int, std::vector<std::string>> vertCache[2];   // shared <v id> lists (polygon, art)

    // A world transform for group children: world = g + R(r) (p + o).
    struct Xf
    {
        Vec2 origin;
        Vec2 offset;
        float angle = 0;
        bool group = false;
        Vec2 apply(const Vec2& p) const
        {
            if (!group) return p;
            const float a = angle * 3.14159265f / 180.0f;
            const Vec2 q = p + offset;
            // Flash rotation is clockwise with y down: the usual rotation matrix in Flash space.
            return origin + Vec2(q.x * std::cos(a) - q.y * std::sin(a), q.x * std::sin(a) + q.y * std::cos(a));
        }
    };

    void place(Special* ref, const Vec2& px, float angle)
    {
        const Vec2 s = pxToStage(px.x, px.y);
        ref->setPosition(s);
        ref->setX(s.x, s.y);
        if (ref->canRotate()) ref->setRotation(angle);
    }

    Special* readShape(const XMLElement* e, const Xf& xf)
    {
        int t = (int)attrF(e, "t", 0);
        const bool interactive = attrBool(e, "i", true);
        if (t == 3 && !interactive) t = 4;
        RefShape* shape = nullptr;
        switch (t)
        {
        case 0: shape = RectangleRefShape::create(); break;
        case 1: shape = CircleRefShape::create(); break;
        case 2: shape = TriangleRefShape::create(); break;
        case 3:
        case 4:
        {
            PolygonRefShape* poly = PolygonRefShape::create(t == 4);
            const XMLElement* v = e->FirstChildElement("v");
            if (!v || !poly) return nullptr;
            std::vector<std::string>& cache = vertCache[t == 4][(int)attrF(v, "id", 0)];
            if (v->Attribute("n"))
            {
                cache.clear();
                const int n = std::min(20000, (int)attrF(v, "n", 0));
                for (int i = 0; i < n; ++i)
                {
                    const char* s = v->Attribute(("v" + std::to_string(i)).c_str());
                    cache.push_back(s ? s : "");
                }
            }
            std::vector<online::geom::Pt> pts, hin, hout;
            for (const std::string& s : cache)
            {
                online::geom::Pt p, a, b;
                if (!online::geom::parseVertex(s.c_str(), version < 1.84f, &p, &a, &b)) continue;
                pts.push_back(p);
                hin.push_back(a);
                hout.push_back(b);
            }
            if (pts.size() < 2) return nullptr;
            const bool closed = attrBool(v, "f", true);
            // Flash EdgeShape scale = shapeWidth / default extent (0.1..10).
            const double dw = online::geom::flashDefaultExtent(pts, true), dh = online::geom::flashDefaultExtent(pts, false);
            const float w = attrF(e, "p2", 0), h = attrF(e, "p3", 0);
            const double sx = (w != 0 && dw > 0) ? std::max(0.1, std::min(10.0, w / dw)) : 1.0;
            const double sy = (h != 0 && dh > 0) ? std::max(0.1, std::min(10.0, h / dh)) : 1.0;
            // Kept as loaded (art bezier handles too); Flash's size scale is baked in.
            std::vector<Vec2> verts, handlesIn, handlesOut;
            for (size_t k = 0; k < pts.size(); ++k)
            {
                verts.push_back(Vec2((float)(pts[k].x * sx), (float)(pts[k].y * sy)));
                handlesIn.push_back(Vec2((float)(hin[k].x * sx), (float)(hin[k].y * sy)));
                handlesOut.push_back(Vec2((float)(hout[k].x * sx), (float)(hout[k].y * sy)));
            }
            poly->setClosed(closed);
            poly->setVertsPx(verts);
            if (t == 4) poly->setHandlesPx(handlesIn, handlesOut);
            shape = poly;
            break;
        }
        default: return nullptr;
        }
        if (!shape) return nullptr;
        if (t <= 2)
        {
            const float w = std::max(1.0f, std::fabs(attrF(e, "p2", 100))) / kPxPerMetre;
            const float h = std::max(1.0f, std::fabs(attrF(e, "p3", 100))) / kPxPerMetre;
            shape->setValueForKey(Value(w), "widthMeters");
            shape->setValueForKey(Value(t == 1 ? w : h), "heightMeters");
            if (t == 1) static_cast<CircleRefShape*>(shape)->setInnerCutout(attrF(e, "p12", 0));
        }
        shape->setValueForKey(Value(attrBool(e, "p5", false)), "fixed");
        shape->setValueForKey(Value(attrBool(e, "p6", false)), "sleeping");
        {
            // Flash keeps density "NaN" (such shapes behave as static); keep it.
            const char* d = e->Attribute("p7");
            shape->setValueForKey(Value(d && !strcmp(d, "NaN") ? std::nanf("") : attrF(e, "p7", 1)), "density");
        }
        shape->setColor((unsigned int)((int)attrF(e, "p8", 0) & 0xffffff));
        shape->setNoFill(attrF(e, "p8", 0) < 0.0f);  // EDITOR (browser features, PC addition)
        shape->setOutlineColor(attrF(e, "p9", -1));
        shape->setShapeOpacity((unsigned int)std::max(0.0f, std::min(100.0f, attrF(e, "p10", 100))));
        shape->setCollision((unsigned int)std::max(1.0f, std::min(7.0f, attrF(e, "p11", 1))));
        if (t <= 2 && !interactive) shape->setValueForKey(Value(false), "interactive");
        const Vec2 p = xf.apply(Vec2(attrF(e, "p0", 0), attrF(e, "p1", 0)));
        place(shape, p, attrF(e, "p4", 0) + (xf.group ? xf.angle : 0.0f));
        return shape;
    }

    Special* readSpecial(const XMLElement* e, const Xf& xf)
    {
        const int t = (int)attrF(e, "t", -1);
        Special* ref = nullptr;
        float angle = 0.0f;
        if (specialInfo(t))
        {
            FlashSpecialRef* f = FlashSpecialRef::create(t);
            f->readFlash(e);
            angle = f->getRotation();
            ref = f;
        }
        else if ((ref = EditorSettings::getInstance()->objectForKey((unsigned int)t)) != nullptr)
        {
            const std::vector<std::string> keys = ref->propertyKeys();
            for (size_t i = 0; i < keys.size(); ++i)
            {
                const std::string& key = keys[i];
                const char* v = e->Attribute(("p" + std::to_string(i)).c_str());
                if (!v || key == "xMeters" || key == "yMeters") continue;
                float f = parseFloat(v, 0.0f);
                if (key == "angle") { angle = f; continue; }
                if (isLengthKey(key) && t != 5001) f /= kPxPerMetre;
                ref->setValueForKey(Value(f), key);
            }
        }
        if (!ref) return nullptr;
        const Vec2 p = xf.apply(Vec2(attrF(e, "p0", 0), attrF(e, "p1", 0)));
        place(ref, p, angle + (xf.group ? xf.angle : 0.0f));
        if (!dynamic_cast<FlashSpecialRef*>(ref)) ref->createRef();
        return ref;
    }

    Special* bodyFor(const char* text)
    {
        if (!text || !*text || !strcmp(text, "-1")) return nullptr;
        auto at = [](std::vector<Special*>& list, int i) { return i >= 0 && i < (int)list.size() ? list[i] : nullptr; };
        if (text[0] == 's') return at(specials, std::atoi(text + 1));
        if (text[0] == 'g') return at(groups, std::atoi(text + 1));
        return at(shapes, std::atoi(text));
    }

    void add(Special* ref)
    {
        if (ref) sbn->addRef(ref);
    }
};

}  // namespace

bool readBrowserLevel(const std::string& xml, EditorSpriteBatchNode* sbn, LevelReaderDelegate* delegate, LevelInfo* info)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return false;
    const XMLElement* root = doc.RootElement();
    const XMLElement* inf = root ? root->FirstChildElement("info") : nullptr;
    if (!inf) return false;
    Reader r;
    r.sbn = sbn;
    r.version = attrF(inf, "v", 1.87f);
    info->background = (int)attrF(inf, "bg", 0);
    info->backgroundColor = (unsigned int)((int)attrF(inf, "bgc", 16777215) & 0xffffff);
    delegate->readerAddCharacter(attrF(inf, "x", 0) / kPxPerMetre, (kCanvasHeight - attrF(inf, "y", 0)) / kPxPerMetre,
                                 (int)attrF(inf, "c", 1), attrBool(inf, "f", false), attrBool(inf, "h", false));
    const Reader::Xf none;
    if (const XMLElement* list = root->FirstChildElement("shapes"))
        for (const XMLElement* e = list->FirstChildElement("sh"); e; e = e->NextSiblingElement("sh"))
        {
            Special* s = r.readShape(e, none);
            r.shapes.push_back(s);
            r.add(s);
        }
    if (const XMLElement* list = root->FirstChildElement("specials"))
        for (const XMLElement* e = list->FirstChildElement("sp"); e; e = e->NextSiblingElement("sp"))
        {
            Special* s = r.readSpecial(e, none);
            r.specials.push_back(s);
            r.add(s);
        }
    if (const XMLElement* list = root->FirstChildElement("groups"))
        for (const XMLElement* e = list->FirstChildElement("g"); e; e = e->NextSiblingElement("g"))
        {
            Reader::Xf xf;
            xf.group = true;
            xf.origin = Vec2(attrF(e, "x", 0), attrF(e, "y", 0));
            xf.offset = Vec2(attrF(e, "ox", 0), attrF(e, "oy", 0));
            xf.angle = attrF(e, "r", 0);
            GroupRef* g = GroupRef::create();
            Vector<Special*> members;
            std::vector<Special*> nonHandles;
            for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
            {
                Special* m = nullptr;
                if (!strcmp(c->Value(), "sh"))
                {
                    m = r.readShape(c, xf);
                    if (m && c->Attribute("h") && !attrBool(c, "h", true)) nonHandles.push_back(m);
                }
                else if (!strcmp(c->Value(), "sp"))
                    m = r.readSpecial(c, xf);
                if (!m) continue;
                r.add(m);
                members.pushBack(m);
            }
            g->sleeping = attrBool(e, "s", false);
            g->foreground = attrBool(e, "f", false);
            g->opacity = e->Attribute("o") ? attrF(e, "o", 100) : 100.0f;
            g->immovable = attrBool(e, "im", false);
            g->fixedRotation = attrBool(e, "fr", false);
            g->vehicle = attrBool(e, "v", false);
            g->spaceAction = (int)attrF(e, "sb", 0);
            g->shiftAction = (int)attrF(e, "sh", 0);
            g->ctrlAction = (int)attrF(e, "ct", 0);
            g->acceleration = attrF(e, "a", 1);
            g->leaningStrength = (int)attrF(e, "l", 0);
            g->characterPose = (int)attrF(e, "cp", 0);
            g->lockJoints = attrBool(e, "lo", false);
            for (Special* m : nonHandles) g->nonHandles.insert(m);
            r.add(g);
            g->setMembers(members);
            r.groups.push_back(g);
        }
    if (const XMLElement* list = root->FirstChildElement("joints"))
        for (const XMLElement* e = list->FirstChildElement("j"); e; e = e->NextSiblingElement("j"))
        {
            const bool pris = attrF(e, "t", 0) == 1.0f;
            JointRef* j = JointRef::create(pris);
            j->setBodies(r.bodyFor(e->Attribute("b1")), r.bodyFor(e->Attribute("b2")));
            j->limit = attrBool(e, "l", false);
            j->motor = attrBool(e, "m", false);
            j->collideSelf = attrBool(e, "c", false);
            j->vehicleControlled = !(e->Attribute("v") && !attrBool(e, "v", true));
            if (pris)
            {
                j->axisAngle = attrF(e, "a", 0);
                j->upper = attrF(e, "ul", 100);
                j->lower = attrF(e, "ll", -100);
                j->torque = attrF(e, "fo", 50);
            }
            else
            {
                j->upper = attrF(e, "ua", 90);
                j->lower = attrF(e, "la", -90);
                j->torque = attrF(e, "tq", 50);
            }
            j->speed = attrF(e, "sp", 3);
            r.place(j, Vec2(attrF(e, "x", 0), attrF(e, "y", 0)), 0);
            r.joints.push_back(j);
            r.add(j);
        }
    if (const XMLElement* list = root->FirstChildElement("triggers"))
    {
        std::vector<const XMLElement*> elements;
        for (const XMLElement* e = list->FirstChildElement("t"); e; e = e->NextSiblingElement("t"))
        {
            TriggerRef* t = TriggerRef::create();
            t->setValueForKey(Value(attrF(e, "w", 100)), "shapeWidth");
            t->setValueForKey(Value(attrF(e, "h", 100)), "shapeHeight");
            t->triggeredBy = std::max(1, std::min(6, (int)attrF(e, "b", 1)));
            t->typeIndex = std::max(1, std::min(3, (int)attrF(e, "t", 1)));
            t->repeatType = std::max(1, std::min(4, (int)attrF(e, "r", 1)));
            t->repeatInterval = attrF(e, "i", 1);
            t->delay = attrF(e, "d", 0);
            t->startDisabled = attrBool(e, "sd", false);
            t->sound = (int)attrF(e, "s", 0);
            t->soundLocation = (int)attrF(e, "l", 1);
            t->panning = attrF(e, "p", 0);
            t->volume = attrF(e, "v", 1);
            r.place(t, Vec2(attrF(e, "x", 0), attrF(e, "y", 0)), attrF(e, "a", 0));
            r.triggers.push_back(t);
            elements.push_back(e);
            r.add(t);
        }
        for (size_t i = 0; i < elements.size(); ++i)
        {
            TriggerRef* t = static_cast<TriggerRef*>(r.triggers[i]);
            if (!t->hasTargets()) continue;
            for (const XMLElement* c = elements[i]->FirstChildElement(); c; c = c->NextSiblingElement())
            {
                const int index = (int)attrF(c, "i", -1);
                std::vector<Special*>* list = nullptr;
                const std::string tag = c->Value();
                if (tag == "sh") list = &r.shapes;
                else if (tag == "sp") list = &r.specials;
                else if (tag == "g") list = &r.groups;
                else if (tag == "j") list = &r.joints;
                else list = &r.triggers;
                if (index < 0 || index >= (int)list->size() || !(*list)[index]) continue;
                Special* ref = (*list)[index];
                const int before = (int)t->targets().size();
                if (!t->addTarget(ref, false)) continue;
                TriggerTarget& target = t->targets()[before];
                target.actions.clear();
                const std::vector<ActionInfo>* actions = actionsFor(ref);
                if (!t->hasActions() || !actions || actions->empty()) continue;
                auto readAction = [&](const XMLElement* a, const char* indexKey) {
                    TriggerAction action;
                    action.index = std::max(0, std::min((int)actions->size() - 1, (int)attrF(a, indexKey, 0)));
                    action.params = TriggerRef::defaultParams(ref, action.index);
                    for (size_t k = 0; k < action.params.size(); ++k)
                        action.params[k] = attrF(a, ("p" + std::to_string(k)).c_str(), action.params[k]);
                    target.actions.push_back(action);
                };
                if (r.version < 1.87f)
                    readAction(c, "a");
                else
                    for (const XMLElement* a = c->FirstChildElement("a"); a; a = a->NextSiblingElement("a"))
                        readAction(a, "i");
                if (target.actions.empty())
                {
                    TriggerAction a;
                    a.params = TriggerRef::defaultParams(ref, 0);
                    target.actions.push_back(a);
                }
            }
        }
    }
    return true;
}

}  // namespace flashed
