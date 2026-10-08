// Linux entry point (PC addition): everything lives in the POSIX desktop glue shared with macOS,
// src/platform/unix/DesktopMain.cpp (the twin of src/platform/win32/main.cpp).

#include "platform/unix/DesktopMain.h"

int main(int argc, char** argv)
{
    return openwheels::desktop::run(argc, argv);
}
