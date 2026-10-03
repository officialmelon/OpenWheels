#include "FFHelper.h"

#include <cmath>

#include "cocos2d.h"

USING_NS_CC;

// @005b3300
bool FFHelper::isTablet()
{
    Director* director = Director::getInstance();
    float dpi = (float)Device::getDPI();
    float widthInches = director->getWinSize().width / dpi;
    float heightInches = director->getWinSize().height / dpi;
    // Diagonal in inches, rounded to two decimals (all float math, roundf = frinta).
    float diagonal = roundf(sqrtf(widthInches * widthInches + heightInches * heightInches) * 100.0f) /
                     100.0f;
    return diagonal >= 7.0f;
}

// @005b3380
bool FFHelper::isFourInch()
{
    return Director::getInstance()->getWinSize().width > 480.0f;
}
