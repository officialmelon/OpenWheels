#pragma once

#include <string>

// Empty helper (arm64 sizeof 1) created by the Settings constructor, which calls init() once.
// init() builds and discards a 512-digit random string, i.e. it consumes 512 rand() values at startup.
// That side effect on the C library random sequence must be kept.
class Obfuscation
{
public:
    Obfuscation();                                                             // @0064d18c
    ~Obfuscation();                                                            // @0064d190

    // createBlob(512); returns true.
    bool init();                                                               // @0064d194
    // length random decimal digits: (int)((float)rand() * 4.656613e-10f * 9.0f) each.
    std::string createBlob(int length);                                        // @0064d1f4
};
