#pragma once
// PC layer for Linux and macOS (not part of the original game): on a fatal signal (SIGSEGV,
// SIGABRT, SIGFPE, SIGILL, SIGBUS), write the signal and a stack trace to openwheels_crash.txt in
// `directory` (the log folder), then let the default handler end the process.

#include <string>

namespace openwheels {
namespace pc {

void installCrashHandler(const std::string& directory);

}  // namespace pc
}  // namespace openwheels
