#include "PogostickGuy.h"

#include "PogoStick.h"

USING_NS_CC;

// @006083c4
PogostickGuy::PogostickGuy()
{
}

// @00608504
PogostickGuy::~PogostickGuy()
{
    _vehicle->release();
}

// @00608570
bool PogostickGuy::init(Vec2 position, std::string name, std::string vehicleName, int groupIndex,
                        bool showGore)
{
    bool ok = CharacterB2D::init(position, name, "Char12", vehicleName, groupIndex, showGore, true);
    if (ok)
    {
        setMainCharacter(true);
        _vehicle = PogoStick::create(position, vehicleName, groupIndex);
        _vehicle->addCharacter(this);
        _vehicle->retain();
    }
    return ok;
}
