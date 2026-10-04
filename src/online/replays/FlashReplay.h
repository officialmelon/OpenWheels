#pragma once
// ONLINE (PC addition): browser Happy Wheels replays (totaljerkface.com replay.hw). Not part of
// the 1:1 reconstruction. See docs/FLASH_LEVELS.md section 11.
//
// Format (decompiled Flash v1.87 ReplayData / CharacterB2D / Trigger / SessionReplay, confirmed on
// live replays): a replay stores INPUT ONLY - no positions, no keyframes. One byte per 30 Hz frame
// (Session._iteration), most significant bit first:
//     left (lean back), right (lean forward), up (accelerate), down (brake/reverse),
//     space (primary action), shift, ctrl (secondary actions), z (eject)
// optionally followed by a 0xFF separator and 4-byte big-endian mouse entries for click triggers:
//     uint16 iteration (+32768 = roll-out instead of click), uint16 trigger index (document order).
// The bytes are sent raw (base64 in the upload). ct = frames when the level was completed, else
// 6000 (Settings.maxReplayFrames, 200 s - the upload limit).
//
// Flash itself only promised exact playback on the same floating-point "architecture"
// (ArchitectureTest: the x of a Box2D test body after 30 steps; `ar` of each replay, the browser
// greys out replays with another `ar`). OpenWheels runs Box2D 2.3 at 60 Hz, so a browser replay
// is re-simulated from its inputs and labelled approximate; it can desync.

#include <cstdint>
#include <string>
#include <vector>

#include "online/OnlineLevel.h"

namespace online {
namespace replays {

const int kMaxReplayFrames = 6000;     // Settings.maxReplayFrames
const char* const kFlashVersion = "1.87";
// Our `ar`: not the browser's (HTML5: 40922988), so browser players see OpenWheels uploads as
// "not 100% accurate" - which they are, the physics differ.
const char* const kArchitecture = "00000000";

// One <rp> of replay.hw (ReplayDataObject).
struct ReplayInfo {
    int id = 0;               // id
    int levelId = 0;          // li
    int userId = 0;           // ui
    std::string userName;     // un
    float rating = 0.0f;      // rg (weighted)
    int votes = 0;            // vs
    int views = 0;            // vw
    std::string created;      // dc "YYYY-MM-DD"
    std::string comment;      // <uc>
    int character = 1;        // pc
    int frames = 0;           // ct (30 Hz frames; 6000 = not completed)
    std::string architecture; // ar
    std::string version;      // vr

    bool completed() const { return frames > 0 && frames < kMaxReplayFrames; }
    float seconds() const { return frames / 30.0f; }
    // ReplayDataObject.getAverageRating (Bayesian weighting undone, 0..5).
    float averageRating() const;
};

struct MouseEntry {
    int iteration = 0;
    int triggerIndex = 0;
    bool rollOut = false;
};

struct ReplayInput {
    std::vector<uint8_t> keys;          // Flash bytes, one per 30 Hz frame
    std::vector<MouseEntry> mouse;

    // ReplayData.byteArray / parseByteArray (including its quirk: a 0xFF at index 0 is not a
    // separator).
    static ReplayInput fromBytes(const std::string& bytes);
    std::string toBytes() const;
};

// Flash key byte <-> the mobile control byte (0x01 forward, 0x02 back, 0x04 lean forward,
// 0x08 lean back, 0x10 space, 0x20 shift, 0x40 ctrl, 0x80 eject).
uint8_t flashToMobile(uint8_t flashByte);
uint8_t mobileToFlash(uint8_t mobileByte);

// Parsers.
bool parseReplayList(const std::string& body, std::vector<ReplayInfo>& replays, int* page, int* perPage,
                     std::string* error);
// get_combined: <combined_data><rp .../><lv .../></combined_data>
bool parseCombined(const std::string& body, ReplayInfo& replay, OnlineLevelInfo& level, std::string* error);
// get_cmb_records: int32 big-endian replay length, the replay bytes, then the encrypted level record.
bool splitCombinedRecord(const std::string& body, std::string& replayBytes, std::string& levelRecord,
                         std::string* error);

// AS3 escape() (what SaveReplayMenu applies to the comment).
std::string as3Escape(const std::string& s);
// "34.13 s" / "1:02.40"
std::string formatTime(int frames);

// A run of the player saved on this PC (<writable>/online/replays/local/*.owreplay).
struct SavedRun {
    std::string file;         // full path ("" = not saved yet)
    int levelId = 0;
    std::string levelName;
    int levelAuthorId = 0;
    int character = 1;        // browser character id
    int frames = 0;           // 30 Hz frames recorded (completion frame when completed)
    bool completed = false;
    std::string date;         // "YYYY-MM-DD HH:MM"
    int uploadedId = 0;       // replay id on the site once uploaded
    ReplayInput input;
};
bool saveRun(SavedRun& run, std::string* error);
bool loadRun(const std::string& file, SavedRun& run);
bool deleteRun(const SavedRun& run);
std::vector<SavedRun> listSavedRuns(int levelId);   // 0 = all levels, newest first

}  // namespace replays
}  // namespace online
