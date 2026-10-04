// RESTORED (PC addition): see ExplorerGuy.h.

#include "ExplorerGuy.h"

#include "MineCart.h"

USING_NS_CC;

bool ExplorerGuy::init(Vec2 position, std::string name, std::string vehicleName, int groupIndex,
                       bool showGore)
{
    bool ok = CharacterB2D::init(position, name, "Char2", vehicleName, groupIndex, showGore, true);
    if (!ok) {
        return ok;
    }
    setMainCharacter(true);
    // Flash hatSmashLimit (CharacterB2D::createDictionaries gives a helmet 2).
    if (_helmetOn && _headFixture) {
        _contactImpulseDict[_headFixture] = 0.75f;
    }
    // Flash createMovieClips: lowerLeg1MC / lowerLeg2MC.visible = false (inside the cart).
    if (_lowerLeg1Sprite) {
        _lowerLeg1Sprite->setVisible(false);
    }
    if (_lowerLeg2Sprite) {
        _lowerLeg2Sprite->setVisible(false);
    }
    _vehicle = MineCart::create(position, vehicleName, groupIndex);
    _vehicle->retain();
    _vehicle->addCharacter(this);
    return ok;
}

ExplorerGuy::~ExplorerGuy()
{
    CC_SAFE_RELEASE(_vehicle);
}

void ExplorerGuy::setState(unsigned char state)
{
    CharacterB2D::setState(state);
    if (MineCart* cart = dynamic_cast<MineCart*>(_vehicle)) {
        cart->extraControls(state);
    }
}

void ExplorerGuy::showLowerLeg(int leg)
{
    if ((leg == 0 || leg == 1) && _lowerLeg1Sprite) {
        _lowerLeg1Sprite->setVisible(true);
    }
    if ((leg == 0 || leg == 2) && _lowerLeg2Sprite) {
        _lowerLeg2Sprite->setVisible(true);
    }
}
