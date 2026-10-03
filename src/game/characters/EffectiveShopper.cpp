#include "EffectiveShopper.h"

#include "MotorCart.h"

USING_NS_CC;

// @005acb38
bool EffectiveShopper::init(Vec2 position, std::string name, std::string vehicleName,
                            int groupIndex, bool showGore)
{
    bool ok = CharacterB2D::init(position, name, "Char4", vehicleName, groupIndex, showGore, true);
    if (ok)
    {
        setMainCharacter(true);
        _vehicle = MotorCart::create(position, vehicleName, groupIndex);
        _vehicle->addCharacter(this);
        _vehicle->retain();
    }
    return ok;
}

// @005ace64
EffectiveShopper::EffectiveShopper()
{
}

// @005acfa4
EffectiveShopper::~EffectiveShopper()
{
    _vehicle->release();
}
