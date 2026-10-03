#include "MopedCouple.h"

#include "Moped.h"

USING_NS_CC;

// @005f3e8c
bool MopedCouple::init(Vec2 position, std::string name, std::string vehicleName, int groupIndex,
                       bool showGore)
{
    bool ok = CharacterB2D::init(position, name, "Char8", vehicleName, groupIndex, showGore, true);
    if (ok)
    {
        setMainCharacter(true);
        Moped* moped = Moped::create(position, vehicleName, groupIndex);
        moped->addDriver(this);
        setVehicle(moped);

        _girl = new CharacterB2D();
        _girl->init(position, "moped_girl", "Char9", vehicleName, -2, showGore, true);
        _girl->autorelease();
        _girl->retain();
        _girl->setVehicle(_vehicle);
        moped->addPassenger(_girl);
        setMourner(_girl);
    }
    return ok;
}

// @005f444c
MopedCouple::MopedCouple()
{
}

// @005f4584
MopedCouple::~MopedCouple()
{
    _girl->release();
    _vehicle->release();
}

// @005f45fc
void MopedCouple::startGrab()
{
    _girl->startGrab();
    CharacterB2D::startGrab();
}

// @005f462c
void MopedCouple::endGrab()
{
    _girl->endGrab();
    CharacterB2D::endGrab();
}

// @005f465c
void MopedCouple::setCurrentPose(CharacterPose pose)
{
    _girl->setCurrentPose(pose);
    CharacterB2D::setCurrentPose(pose);
}
