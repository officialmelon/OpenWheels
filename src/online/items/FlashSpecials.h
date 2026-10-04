#pragma once
// ONLINE (PC addition): browser-game specials that the Android game doesn't have (or only has as
// stubs), ported from the Flash v1.87 classes (com.totaljerkface.game.level.userspecials.*).
// Only created for converted browser levels (online::flashLevel()); see FlashRuntime.h.
//
// Each port registers itself with a FLASH_SPECIAL registration in its own .cpp, so adding an item
// needs no edit here:
//
//     static online::FlashSpecialRegistration reg(18, [] { return (LevelItem*)new online::Glass(); });
//
// The converter asks the registry which ids are implemented (flashSpecialImplemented) and only
// falls back to placeholder blocks for the others.

#include <string>
#include <vector>

#include "LevelItem.h"

class LevelDataElement;

namespace online {

enum class FlashSpecialUse {
    Missing,   // the mobile game has no class for this id: always use the port
    Override,  // replace the mobile class (it is a stub or behaves differently)
    InGroup,   // replace the mobile class only inside groups (the mobile one ignores groups)
};

using FlashSpecialFactory = LevelItem* (*)();

struct FlashSpecialRegistration {
    FlashSpecialRegistration(int type, FlashSpecialFactory factory,
                             FlashSpecialUse use = FlashSpecialUse::Missing, bool groupable = false);
};

// LevelB2D::addSpecial hooks: a new, not yet initialised item (init(element, groupBody, offset)
// follows), or nullptr.
LevelItem* createFlashSpecial(int type);
LevelItem* createFlashSpecialOverride(int type, bool inGroup);

// Converter queries: whether the id has a port, and whether that port supports groups.
bool flashSpecialImplemented(int type);
bool flashSpecialGroupable(int type);
FlashSpecialUse flashSpecialUse(int type);

// Common base of the ports. Flash semantics the mobile trigger code needs to know about.
class FlashItem : public LevelItem
{
public:
    // Flash TargetActionSpecial._instant: false for actions that run over several frames
    // (they get triggerRepeatActivation every step until it returns true).
    virtual bool isInstantAction(int action) { return true; }

protected:
    // Attribute helpers with Flash's defaults.
    static float num(LevelDataElement* e, const char* key, float def);
    static int inum(LevelDataElement* e, const char* key, int def);
    static bool flag(LevelDataElement* e, const char* key, bool def);
    static std::string text(LevelDataElement* e, const char* key);

    // Flash frame counter (30 fps) for the seconds TargetActionSpecial passes to
    // triggerRepeatActivation.
    static int flashFrames(float seconds);
};

}  // namespace online
