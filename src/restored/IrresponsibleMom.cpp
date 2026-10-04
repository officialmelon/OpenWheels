// RESTORED (PC addition): see IrresponsibleMom.h.

#include "IrresponsibleMom.h"

#include "MomBike.h"
#include "Restored.h"
#include "Session.h"

USING_NS_CC;

bool IrresponsibleMom::init(Vec2 position, std::string name, std::string vehicleName, int groupIndex,
                            bool showGore)
{
    bool ok = CharacterB2D::init(position, name, "Char4", vehicleName, groupIndex, showGore, true);
    if (!ok) {
        return ok;
    }
    setMainCharacter(true);
    MomBike* bike = MomBike::create(position, vehicleName, groupIndex);
    _vehicle = bike;
    _vehicle->retain();
    bike->addMom(this);

    bool kidGore = showGore && restored::childGore();
    _daughter = new CharacterB2D();
    _daughter->init(position, "irresponsible_mom_daughter", "Kid2", vehicleName, -2, kidGore, true);
    _daughter->autorelease();
    _daughter->retain();
    _daughter->setVehicle(bike);
    bike->addDaughter(_daughter);

    _son = new CharacterB2D();
    _son->init(position, "irresponsible_mom_son", "kid1", vehicleName, -3, kidGore, true);
    _son->autorelease();
    _son->retain();
    _son->setVehicle(bike);
    bike->addSon(_son);
    return ok;
}

IrresponsibleMom::~IrresponsibleMom()
{
    CC_SAFE_RELEASE(_son);
    CC_SAFE_RELEASE(_daughter);
    CC_SAFE_RELEASE(_vehicle);
}

void IrresponsibleMom::setState(unsigned char state)
{
    CharacterB2D::setState(state);
    MomBike* bike = static_cast<MomBike*>(_vehicle);
    if (bike) {
        bike->kidControls(state);
    }
    for (CharacterB2D* kid : {_daughter, _son}) {
        if (kid && kid->getEjected() && !kid->getDead()) {
            kid->setState(state);
        }
    }
}
