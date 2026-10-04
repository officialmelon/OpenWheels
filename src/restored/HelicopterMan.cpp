// RESTORED (PC addition): see HelicopterMan.h.

#include "HelicopterMan.h"

#include "Helicopter.h"

USING_NS_CC;

bool HelicopterMan::init(Vec2 position, std::string name, std::string vehicleName, int groupIndex,
                         bool showGore)
{
    bool ok = CharacterB2D::init(position, name, "Heli", vehicleName, groupIndex, showGore, true);
    if (!ok) {
        return ok;
    }
    setMainCharacter(true);
    _vehicle = Helicopter::create(position, vehicleName, groupIndex);
    _vehicle->retain();
    _vehicle->addCharacter(this);
    return ok;
}

HelicopterMan::~HelicopterMan()
{
    CC_SAFE_RELEASE(_vehicle);
}

void HelicopterMan::setState(unsigned char state)
{
    CharacterB2D::setState(state);
    if (Helicopter* heli = dynamic_cast<Helicopter*>(_vehicle)) {
        heli->extraControls(state);
    }
}
