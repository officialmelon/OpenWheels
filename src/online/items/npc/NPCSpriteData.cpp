// ONLINE (PC addition): see NPCSpriteData.h.
#include "online/items/npc/NPCSpriteData.h"

#include <algorithm>

namespace online {
namespace npc {

namespace {

const SpriteData kSprites[kSpriteCount] = {
#include "online/items/npc/NPCSpriteData.inc"
};

}  // namespace

const SpriteData& spriteData(int charIndex)
{
    return kSprites[std::max(1, std::min(kSpriteCount, charIndex)) - 1];
}

}  // namespace npc
}  // namespace online
