#pragma once
// Online (browser Happy Wheels) user levels - not part of the 1:1 reconstruction.
// See docs/FLASH_LEVELS.md for the protocol and the Flash level format.

#include <string>

namespace online {

// One <lv> of a get_level.hw response.
struct OnlineLevelInfo {
    int id = 0;                 // id
    std::string name;           // ln
    int authorId = 0;           // ui (also the record's decryption key suffix)
    std::string authorName;     // un
    float rating = 0.0f;        // rg, weighted 0..5
    int votes = 0;              // vs
    int plays = 0;              // ps
    std::string published;      // dp, "YYYY-MM-DD"
    int character = 0;          // pc, the level's forced character (0 = player's choice)
    std::string comment;        // <uc> author comment (may be empty)
};

}  // namespace online
