#pragma once
// patch::to_string - the well-known Android NDK workaround for the missing std::to_string,
// used throughout the original game (instantiated for unsigned int, int, float, ...).
// Same implementation as the original: format through a std::ostringstream.

#include <sstream>
#include <string>

namespace patch {

template <typename T>
std::string to_string(const T& n) {
    std::ostringstream stm;
    stm << n;
    return stm.str();
}

}  // namespace patch
