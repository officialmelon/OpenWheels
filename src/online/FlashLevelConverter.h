#pragma once
// Browser (Flash 1.x / HTML5 2.x) level XML -> the mobile level XML LevelB2D loads.
// The formats share elements, type ids and units; see docs/FLASH_LEVELS.md section 5/6 for
// what has to change (info attributes, unsupported items, multi-action triggers, characters).

#include <string>
#include <vector>

namespace online {

struct ConversionReport {
    bool ok = false;
    std::string error;                 // set when ok == false
    std::vector<std::string> warnings; // player-facing, short ("2 chairs replaced by blocks")
    bool hasUserVehicle = false;       // <g v="t"> custom vehicles (not playable as vehicles here)
    int droppedItems = 0;              // items removed (decoration with no mobile equivalent)
    int substitutedItems = 0;          // items replaced by placeholder shapes
    int character = 0;                 // mobile character id the level uses (after fallback)
    bool forceCharacter = false;       // info f, possibly cleared by the character fallback
};

class FlashLevelConverter {
public:
    // Returns the mobile XML ("" when report->ok is false).
    static std::string toMobile(const std::string& flashXml, ConversionReport* report);
};

}  // namespace online
