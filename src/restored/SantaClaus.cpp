// RESTORED (PC addition): see SantaClaus.h.

#include "SantaClaus.h"

#include "Sleigh.h"

USING_NS_CC;

bool SantaClaus::init(Vec2 position, std::string name, std::string vehicleName, int groupIndex,
                      bool showGore)
{
    bool ok = CharacterB2D::init(position, name, "Santa", vehicleName, groupIndex, showGore, true);
    if (!ok) {
        return ok;
    }
    setMainCharacter(true);
    Sleigh* sleigh = Sleigh::create(position, vehicleName, groupIndex);
    _vehicle = sleigh;
    _vehicle->retain();
    sleigh->addSanta(this);

    // The second elf is the first one 110 Flash symbol px further ahead (the SWF draws both alike).
    const float elfOffset = 110.0f / 125.0f;
    const int groups[2] = {-2, -4};
    for (int i = 0; i < 2; i++) {
        CharacterB2D* elf = new CharacterB2D();
        elf->init(position + Vec2(i * elfOffset, 0.0f), "santa_elf", "Elf1", vehicleName, groups[i], showGore,
                  true);
        elf->autorelease();
        elf->retain();
        elf->setVehicle(sleigh);
        _elves[i] = elf;
        sleigh->addElf(elf, i);
    }
    return ok;
}

SantaClaus::~SantaClaus()
{
    for (CharacterB2D*& elf : _elves) {
        CC_SAFE_RELEASE(elf);
    }
    CC_SAFE_RELEASE(_vehicle);
}

void SantaClaus::setState(unsigned char state)
{
    CharacterB2D::setState(state);
    if (Sleigh* sleigh = dynamic_cast<Sleigh*>(_vehicle)) {
        sleigh->extraControls(state);
    }
    for (CharacterB2D* elf : _elves) {
        if (elf && elf->getEjected() && !elf->getDead()) {
            elf->setState(state & 0x1f);
        }
    }
}
