#include "SoundList.h"

#include "cocos2d.h"

#include <cstdlib>
#include <iostream>
#include <sstream>

// @006158ec
// The original constructor is ~100 KB of inline inserts (326 sounds): for every sound id it appends
// the sound name to _sfxArray and stores name -> file base name in _sfxDictionary; _sfxLookup is left
// empty. That table is game data, so it is not reproduced here: tools/re/extract_soundlist.py runs
// the original constructor (arm64 emulator, player's own libMyGame.so) at build time and writes
// soundlist.tsv ("<id>\t<sound name>\t<file base name>" per line, id order), which is read back here.
SoundList::SoundList()
{
    std::string data = cocos2d::FileUtils::getInstance()->getStringFromFile("soundlist.tsv");
    if (data.empty())
    {
        cocos2d::log("SoundList: soundlist.tsv missing - run the build (tools/re/extract_soundlist.py)");
        return;
    }

    std::istringstream in(data);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
        {
            line.erase(line.size() - 1);
        }
        size_t nameStart = line.find('\t');
        if (nameStart == std::string::npos)
        {
            continue;
        }
        size_t fileStart = line.find('\t', nameStart + 1);
        if (fileStart == std::string::npos)
        {
            continue;
        }
        size_t soundId = strtoul(line.substr(0, nameStart).c_str(), nullptr, 10);
        std::string soundName = line.substr(nameStart + 1, fileStart - nameStart - 1);
        std::string fileName = line.substr(fileStart + 1);

        if (soundId >= _sfxArray.size())
        {
            _sfxArray.resize(soundId + 1);
        }
        _sfxArray[soundId] = soundName;
        _sfxDictionary[soundName] = fileName;
    }
}

// @0062e6d4
SoundList::~SoundList()
{
}

// @0062e7d8
std::string SoundList::soundFileNameForId(unsigned int soundId)
{
    std::string fileName;
    std::string soundName = _sfxArray[soundId];
    fileName = _sfxDictionary[soundName];
    std::cout << soundName << " : " << fileName << std::endl;
    return fileName;
}
