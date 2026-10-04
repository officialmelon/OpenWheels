// ONLINE (PC addition): see FlashSpecials.h.
#include "online/items/FlashSpecials.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>

#include "LevelDataElement.h"

namespace online {

namespace {

struct Entry {
    FlashSpecialFactory factory = nullptr;
    FlashSpecialUse use = FlashSpecialUse::Missing;
    bool groupable = false;
};

std::map<int, Entry>& registry()
{
    static std::map<int, Entry> r;  // function-local: safe from static-init order
    return r;
}

}  // namespace

FlashSpecialRegistration::FlashSpecialRegistration(int type, FlashSpecialFactory factory,
                                                   FlashSpecialUse use, bool groupable)
{
    Entry e;
    e.factory = factory;
    e.use = use;
    e.groupable = groupable;
    registry()[type] = e;
}

LevelItem* createFlashSpecial(int type)
{
    auto it = registry().find(type);
    if (it == registry().end() || !it->second.factory) return nullptr;
    return it->second.factory();
}

LevelItem* createFlashSpecialOverride(int type, bool inGroup)
{
    auto it = registry().find(type);
    if (it == registry().end() || !it->second.factory) return nullptr;
    switch (it->second.use) {
    case FlashSpecialUse::Override: return it->second.factory();
    case FlashSpecialUse::InGroup: return inGroup ? it->second.factory() : nullptr;
    default: return nullptr;  // Missing: reached through createFlashSpecial
    }
}

bool flashSpecialImplemented(int type) { return registry().count(type) != 0; }

FlashSpecialUse flashSpecialUse(int type)
{
    auto it = registry().find(type);
    return it != registry().end() ? it->second.use : FlashSpecialUse::Missing;
}

bool flashSpecialGroupable(int type)
{
    auto it = registry().find(type);
    return it != registry().end() && it->second.groupable;
}

float FlashItem::num(LevelDataElement* e, const char* key, float def)
{
    float v = def;
    if (!e->floatAttribute(key, &v) || !std::isfinite(v)) return def;
    return v;
}

int FlashItem::inum(LevelDataElement* e, const char* key, int def)
{
    float v = 0.0f;
    if (!e->floatAttribute(key, &v) || !std::isfinite(v) || std::fabs(v) > 2.0e9f) return def;
    return (int)std::trunc(v);  // AS3 int()
}

bool FlashItem::flag(LevelDataElement* e, const char* key, bool def)
{
    const char* s = e->stringAttribute(key);
    if (!s) return def;
    if (!strcmp(s, "t") || !strcmp(s, "1") || !strcmp(s, "true")) return true;
    if (!strcmp(s, "f") || !strcmp(s, "0") || !strcmp(s, "false")) return false;
    return def;
}

std::string FlashItem::text(LevelDataElement* e, const char* key)
{
    const char* s = e->stringAttribute(key);
    return s ? std::string(s) : std::string();
}

int FlashItem::flashFrames(float seconds) { return (int)std::lround(seconds * 30.0f); }

}  // namespace online
