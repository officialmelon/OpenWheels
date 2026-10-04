#pragma once
// ONLINE (PC addition): the player character without its vehicle, for browser levels that
// start with "hide vehicle" (Flash PlayableCharacterB2D). See BareCharacter.cpp.

class CharacterB2D;

namespace online {

// A main character of the given mobile character id, ejected, with no vehicle.
CharacterB2D* createBareCharacter(float x, float y, int characterId);

}  // namespace online
