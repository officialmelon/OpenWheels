#pragma once

#include <map>
#include <string>
#include <vector>

// Sound id -> sound name -> file base name table (arm64 sizeof 0x48, owned by SoundController).
//
// The original constructor (@006158ec, ~100 KB, 326 sounds) is one long run of map/vector inserts that
// Ghidra cannot decompile. The table is not transcribed into this repository: at build time
// tools/re/extract_soundlist.py runs that constructor in an arm64 emulator against the player's own
// libMyGame.so and writes soundlist.tsv (UTF-8, one line per id in id order:
// "<id>\t<sound name>\t<file base name>"). OpenWheels' constructor reads that file through
// cocos2d::FileUtils::getStringFromFile("soundlist.tsv") and fills _sfxDictionary / _sfxArray.
//
// Member names follow the iOS ObjC SoundList ivars (_sfxDictionary, _sfxArray, _sfxLookup).
class SoundList
{
public:
    SoundList();                                                               // @006158ec
    ~SoundList();                                                              // @0062e6d4

    // _sfxDictionary[_sfxArray[soundId]] (operator[]: inserts "" for unknown names). Also prints
    // "<sound name> : <file name>" + std::endl to std::cout.
    std::string soundFileNameForId(unsigned int soundId);                      // @0062e7d8

private:
    std::map<std::string, std::string> _sfxDictionary;   // +0x00 sound name -> file base name
    std::vector<std::string> _sfxArray;                  // +0x18 sound id -> sound name
    // Zeroed by the constructor and never filled or read in 1.1.3 (verified by running the original
    // constructor in the emulator: it stays empty); only the destructor touches it.
    std::vector<std::vector<std::string>> _sfxLookup;    // +0x30
};
