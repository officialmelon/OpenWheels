#include "IrresponsibleDad.h"

#include "RoadBike.h"
#include "Session.h"
#include "Settings.h"
#include "qol/QoL.h"  // QOL (PC addition)

USING_NS_CC;

// @005cb20c
bool IrresponsibleDad::init(Vec2 position, std::string name, std::string vehicleName,
                            int groupIndex, bool showGore)
{
    bool ok = CharacterB2D::init(position, name, "Char3", vehicleName, groupIndex, showGore, true);
    if (ok)
    {
        setMainCharacter(true);
        _vehicle = RoadBike::create(position, vehicleName, groupIndex);
        RoadBike* bike = static_cast<RoadBike*>(_vehicle);
        bike->addDad(this);
        _vehicle->retain();

        _kid = new CharacterB2D();
        bool kidGore = false;
        if (showGore)
        {
            kidGore = !Settings::getInstance()->getSelectedCharacterData()["suppress_gore"].asBool();
            // QOL (PC addition): "child gore" - the kid's gore frames rebuilt from the browser
            // game (tools/assets/extract_kid_gore.py). Gameplay only: character select uses its
            // own character_select_ sheet, which has no gore frames.
            if (!kidGore && qol::childGore() && getSession()->getMode() == SessionModeGameplay)
            {
                kidGore = qol::loadChildGoreSprites();
            }
        }
        _kid->init(position, "irresponsible_dad_kid", "kid1", vehicleName, -2, kidGore, kidGore);
        _kid->autorelease();
        _kid->retain();
        _kid->setVehicle(_vehicle);
        _kid->setMourner(this);
        bike->addKid(_kid);
    }
    return ok;
}

// @005cb8c0
IrresponsibleDad::IrresponsibleDad()
{
}

// @005cb9f8
IrresponsibleDad::~IrresponsibleDad()
{
    CC_SAFE_RELEASE(_kid);
    CC_SAFE_RELEASE(_vehicle);
}

// @005cba78
void IrresponsibleDad::startGrab()
{
    if (_kid)
    {
        _kid->startGrab();
    }
    CharacterB2D::startGrab();
}

// @005cbaac
void IrresponsibleDad::endGrab()
{
    if (_kid)
    {
        _kid->endGrab();
    }
    CharacterB2D::endGrab();
}

// @005cbae0
void IrresponsibleDad::mourn()
{
    addVocalsWithName("Damnit", VocalPriority1);
}

// @005cbb88
void IrresponsibleDad::setCurrentPose(CharacterPose pose)
{
    if (_kid)
    {
        _kid->setCurrentPose(pose);
    }
    CharacterB2D::setCurrentPose(pose);
}

// @005cbbc8
void IrresponsibleDad::taperBodies()
{
    CharacterB2D::taperBodies();
    taperBody(_chestBody, 0.85f, true);
    _chestBody->ResetMassData();
}
