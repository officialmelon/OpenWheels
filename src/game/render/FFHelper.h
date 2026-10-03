#pragma once

// FFHelper: device-class helpers (static only).

class FFHelper
{
public:
    static bool isTablet();    // screen diagonal (winSize / DPI, rounded to 1/100) >= 7 inches
    static bool isFourInch();  // winSize.width > 480
};
