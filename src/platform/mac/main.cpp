// macOS entry point (PC addition): everything lives in the POSIX desktop glue shared with Linux,
// src/platform/unix/DesktopMain.cpp (the twin of src/platform/win32/main.cpp). The app bundle's
// Info.plist is src/platform/mac/Info.plist.

#include "platform/unix/DesktopMain.h"

int main(int argc, char** argv)
{
    return openwheels::desktop::run(argc, argv);
}
