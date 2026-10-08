#pragma once
// Android (bionic) libc random numbers on Windows, Linux, macOS and iOS. The original game calls rand()/srand() directly
// (blood, sparks, voices, camera shake, ...). bionic's rand() is BSD random() (additive feedback,
// x^31 + x^3 + 1, RAND_MAX 2^31-1, unseeded state == srandom(1)); MSVC's is a 15-bit LCG. This
// header is force-included into the game's translation units off Android (see CMakeLists.txt) so
// the reconstructed code gets the same generator, range and sequences as on a device.
// On Android builds (tools/check_tu.sh) it does nothing.

#if !defined(__ANDROID__)
#include <cstdlib>

extern "C" int ow_bionic_rand(void);
extern "C" void ow_bionic_srand(unsigned int seed);
// `std::rand()` (game code, and cocos2d's inline ccRandom.h helpers such as CCRANDOM_0_1) must map
// to the same generator - on Android std::rand is bionic's rand.
namespace std {
using ::ow_bionic_rand;
using ::ow_bionic_srand;
}

#undef RAND_MAX
#define RAND_MAX 0x7fffffff
#define rand() ow_bionic_rand()
#define srand(seed) ow_bionic_srand(seed)
#endif
