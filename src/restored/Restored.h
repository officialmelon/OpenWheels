#pragma once

// RESTORED (PC addition): the browser game's player characters the mobile port left out
// (Lawnmower Man, Irresponsible Mom), rebuilt from the player's own browser-game SWFs by
// tools/assets/extract_character.py into generated/restored/ next to the exe (Win32) or in the
// APK's assets (Android):
//   generated/restored/shared/   Characters_restored.plist, characters/ and vehicles/ bodies
//   generated/restored/<tier>/   sprite sheets, character-select icons, control buttons
//   generated/restored/sounds/   sounds the mobile build lacks (MowerLoop, GrindLoop2, Kid2...)
// Without those files nothing changes: the character list stays the original six.

#include <string>
#include <vector>

#include "base/CCValue.h"
#include "math/Vec2.h"

class CharacterB2D;
class GameplayBtn;

namespace restored {

// Control sets of the restored characters (Characters_restored.plist "controls"). The original
// sets are 1..5 (GameplayControls.h); these are the default vehicle set plus their own buttons.
enum ControlsType
{
    ControlsTypeLawnMower = 106,         // special = deck lift (own icon), on the left
    ControlsTypeExplorer = 107,          // special = rail clamp; + stand / crouch (bits 0x20, 0x40)
    ControlsTypeSanta = 108,             // special = flight (+ boost meter); + release elves
    ControlsTypeIrresponsibleMom = 110,  // special = brake; + son / daughter eject (bits 0x20, 0x40)
};

// Appends generated/restored/{shared,<tier>,sounds} to the search paths (after the original
// ones, so original files always win). Called by AppDelegate once the tier is known.
void addSearchPaths(const std::string& tier);

// Appends the restored characters' entries to the list read from Characters.plist.
void appendCharacters(cocos2d::ValueVector& characters);

// True when the restored character `characterId` (browser id, e.g. 6 = Lawnmower Man) has been
// generated and can be played (src/online/FlashLevelConverter uses it to keep forced characters).
// Works before addSearchPaths too (tools such as --convert-flash): the generated folder is then
// looked up next to the search paths / the resource root.
bool hasCharacter(int characterId);

// LevelB2D::createCharacter for the restored ids; nullptr for any other id.
CharacterB2D* createCharacter(float x, float y, int characterId, int groupIndex, bool showGore);

// Adds the <key>_icon(_bw).png frames of the restored characters (character select).
void loadIconFrames();

// The kids' gore (Irresponsible Mom's daughter and son) follows the QoL "child gore" option.
bool childGore();

// GameplayControls::addControls hooks. controlsSpecial: the special button's frame and side for
// a restored control set (nothing changes for the original ones; the side only when the player
// has not overridden it). extraControls: the additional buttons of a restored control set,
// positioned for the vehicle layout (above the lean buttons) or the ejected one (above the grab
// button at `grabPos`); the caller adds them as children and to its button list.
void controlsSpecial(int type, int overrideSpecialPosition, std::string* frame, bool* onLeft);
// True when a restored control set shows the boost meter (as the Moped's), fed by the vehicle
// through GameplayControls::setMeterPercentage.
bool controlsMeter(int type);
std::vector<GameplayBtn*> extraControls(int type, bool ejected, float userScale,
                                        const cocos2d::Vec2& leanBackPos,
                                        const cocos2d::Vec2& leanForwardPos, float leanHeight,
                                        const cocos2d::Vec2& grabPos, float grabHeight,
                                        float spacing);

}  // namespace restored
