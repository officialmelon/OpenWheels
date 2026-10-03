#include "Obfuscation.h"

#include "Patch.h"

#include "cocos2d.h"

#include <cmath>

// @0064d18c
Obfuscation::Obfuscation()
{
}

// @0064d190
Obfuscation::~Obfuscation()
{
}

// @0064d194
bool Obfuscation::init()
{
    // The blob is thrown away; the only effect is consuming 512 values of the C rand() sequence.
    createBlob(512);
    return true;
}

// @0064d1f4
std::string Obfuscation::createBlob(int length)
{
    std::string blob;
    for (int i = 0; i < length; i++)
    {
        int digit = roundf(CCRANDOM_0_1() * 9.0f);
        blob = blob.append(patch::to_string(digit));
    }
    return blob;
}
