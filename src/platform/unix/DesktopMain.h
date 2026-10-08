#pragma once
// PC platform glue for Linux and macOS (not part of the original game): the POSIX twin of
// src/platform/win32/main.cpp. Parses the same command line, finds the player's own game files,
// creates the GLFW window with the "device" frame size, installs the keyboard bridge and the
// crash handler, then runs the cocos2d-x main loop. See src/platform/win32/main.cpp for the
// meaning of every option; docs/DESKTOP.md for building and running.

namespace openwheels {
namespace desktop {

// Runs the game; returns the process exit code. `appName` titles the window and message boxes.
int run(int argc, char** argv);

}  // namespace desktop
}  // namespace openwheels
