// RESTORED (PC addition): see LawnMowerMan.h.

#include "LawnMowerMan.h"

#include "LawnMower.h"

USING_NS_CC;

bool LawnMowerMan::init(Vec2 position, std::string name, std::string vehicleName, int groupIndex,
                        bool showGore)
{
    bool ok = CharacterB2D::init(position, name, "Char11", vehicleName, groupIndex, showGore, true);
    if (ok)
    {
        setMainCharacter(true);
        _vehicle = LawnMower::create(position, vehicleName, groupIndex);
        _vehicle->addCharacter(this);
        _vehicle->retain();
    }
    return ok;
}

LawnMowerMan::~LawnMowerMan()
{
    CC_SAFE_RELEASE(_vehicle);
}
