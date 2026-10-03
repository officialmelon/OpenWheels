#pragma once
// Long user-facing text (option descriptions, warnings, help paragraphs) is NOT transcribed into
// this repository. Where the original uses such a string, write
//
//     OW_GAMETEXT(someUniqueKey, 0x<address>)   // key, Ghidra address of the original string
//
// which evaluates to the text (const std::string&). tools/re/extract_gametext.py scans src/game for
// these uses, reads each string from the player's own copy of libMyGame.so at build time and writes
// gametext.tsv next to the executable; GameText loads it on first use. Keys must be unique.
// Short labels ("Back", "Options"), keys and asset names stay ordinary literals.

#include <string>

#define OW_GAMETEXT(key, addr) (::GameText::get(#key))
// Same for text hard-coded in the iOS build (level editor port): addr is the iOS Mach-O address
// of the C string or of the ObjC @"..." constant (__cfstring) that references it.
#define OW_IOSTEXT(key, addr) (::GameText::get(#key))

namespace GameText {

// Text for `key`, or an empty string (and a log line) if the generated table lacks it.
const std::string& get(const char* key);

}  // namespace GameText
