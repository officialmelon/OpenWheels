// ONLINE (PC addition): browser levels' "hide vehicle" start (info h="t").
//
// Flash Session.setupCharacter: with Settings.hideVehicle the player is a PlayableCharacterB2D,
// the selected character's ragdoll without its vehicle, controlled like an ejected rider (arrows
// pick the four poses, space grabs). PlayableCharacterB2D adds nothing else that matters here
// (no per-character chest taper, no kid / moped girl), so this is the mobile CharacterB2D built
// from the character's usual body description, ejected before the first step.
#include "online/BareCharacter.h"

#include "cocos2d.h"

#include "CharacterB2D.h"
#include "LevelB2D.h"

USING_NS_CC;

namespace online {

CharacterB2D* createBareCharacter(float x, float y, int characterId)
{
    std::string name, vehicle, vocals;
    switch (characterId) {
    case CharacterIdBusinessGuy: name = "business_guy"; vehicle = "personal_transporter"; vocals = "Char2"; break;
    case CharacterIdIrresponsibleDad: name = "irresponsible_dad"; vehicle = "road_bike"; vocals = "Char3"; break;
    case CharacterIdEffectiveShopper: name = "effective_shopper"; vehicle = "motor_cart"; vocals = "Char4"; break;
    case CharacterIdMopedCouple: name = "moped_guy"; vehicle = "moped"; vocals = "Char8"; break;
    case CharacterIdPogostickGuy: name = "pogo_stick_guy"; vehicle = "pogo_stick"; vocals = "Char12"; break;
    default: name = "wheelchair_guy"; vehicle = "wheelchair"; vocals = "Char1"; break;
    }
    const bool showGore = !UserDefault::getInstance()->getBoolForKey("gore_disabled");
    CharacterB2D* character = new CharacterB2D();
    character->init(Vec2(x, y), name, vocals, vehicle, -1, showGore, true);
    // Ejected from the start. eject() only announces it for the main character, and the
    // gameplay controls don't exist yet while the level loads: announce it on the next frame.
    character->eject();
    character->setMainCharacter(true);
    Director::getInstance()->getScheduler()->schedule(
        [](float) { Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("characterEjected"); },
        character, 0.0f, 0, 0.0f, false, "online_bare_character_ejected");
    return character;
}

}  // namespace online
