#include "WheelchairGuy.h"

#include "Wheelchair.h"

USING_NS_CC;

// @00646df8
WheelchairGuy::WheelchairGuy()
{
}

// @00646f38
WheelchairGuy::~WheelchairGuy()
{
    CC_SAFE_RELEASE(_vehicle);
}

// @00646fa8
bool WheelchairGuy::init(Vec2 position, std::string name, std::string vehicleName, int groupIndex,
                         bool showGore)
{
    bool ok = CharacterB2D::init(position, name, "Char1", vehicleName, groupIndex, showGore, true);
    if (ok)
    {
        setMainCharacter(true);
        _vehicle = Wheelchair::create(position, vehicleName, groupIndex);
        _vehicle->addCharacter(this);
        _vehicle->retain();
    }
    return ok;
}

// @006472d4
void WheelchairGuy::taperBodies()
{
    CharacterB2D::taperBodies();
    taperBody(_chestBody, 0.8f, false);
    taperBody(_chestBody, 0.7f, true);
    _chestBody->ResetMassData();
}
