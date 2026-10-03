#include "BusinessGuy.h"

#include "PersonalTransporter.h"

USING_NS_CC;

// @00588e74
BusinessGuy::BusinessGuy()
{
}

// @00588fb4
BusinessGuy::~BusinessGuy()
{
    CC_SAFE_RELEASE_NULL(_vehicle);
}

// @00589024
bool BusinessGuy::init(Vec2 position, std::string name, std::string vehicleName, int groupIndex,
                       bool showGore)
{
    bool ok = CharacterB2D::init(position, name, "Char2", vehicleName, groupIndex, showGore, true);
    if (ok)
    {
        setMainCharacter(true);
        _vehicle = PersonalTransporter::create(position, vehicleName, groupIndex);
        _vehicle->addCharacter(this);
        _vehicle->retain();
    }
    return ok;
}

// @00589410
void BusinessGuy::taperBodies()
{
    CharacterB2D::taperBodies();
    taperBody(_chestBody, 0.75f, true);
    _chestBody->ResetMassData();
}
