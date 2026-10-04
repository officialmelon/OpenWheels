// ONLINE (PC addition): see Grindable.h.
#include "online/items/Grindable.h"

#include <map>

namespace online {

namespace {
std::map<b2Body*, Grindable*>& registry()
{
    static std::map<b2Body*, Grindable*> r;
    return r;
}
}  // namespace

Grindable::~Grindable()
{
    auto& r = registry();
    for (auto it = r.begin(); it != r.end();) {
        if (it->second == this) it = r.erase(it);
        else ++it;
    }
}

Grindable* Grindable::forBody(b2Body* body)
{
    auto it = registry().find(body);
    return it == registry().end() ? nullptr : it->second;
}

void Grindable::registerGrindBody(b2Body* body)
{
    if (body) registry()[body] = this;
}

void Grindable::unregisterGrindBody(b2Body* body)
{
    auto it = registry().find(body);
    if (it != registry().end() && it->second == this) registry().erase(it);
}

}  // namespace online
