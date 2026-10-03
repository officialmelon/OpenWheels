#pragma once

// TerrainShape: ShapeItem for a terrain (b2ChainShape loop on the level body). The terrain art is
// drawn once into the session's TerrainNode, so this item has no draw node and adds no members
// (sizeof 0x40). No virtuals of its own => no key function: its vtable and deleting destructor
// are emitted as COMDAT where it is constructed (LevelB2D.cpp).

#include "ShapeItem.h"

class TerrainShape : public ShapeItem
{
public:
    bool init(b2Fixture* fixture);  // ShapeItem::init(fixture, 1.0f, 1.0f, nullptr)
};
