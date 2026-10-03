#include "TerrainShape.h"

// ~TerrainShape() @005dc27c (D0 only): implicit destructor; with no key function its vtable and
// deleting destructor are COMDAT (emitted in LevelB2D.cpp, where TerrainShape is constructed).

// @00636aec
bool TerrainShape::init(b2Fixture* fixture)
{
    return ShapeItem::init(fixture, 1.0f, 1.0f, nullptr);
}
