#include "Globals.h"

// Definitions of the exported globals:: data (see Globals.h). Values were read from the original
// libMyGame.so (.rodata / .data); the three Color3B and the Size are dynamically initialised, in this
// order, by the original's static initialiser (_INIT_8 @005beae8).

namespace globals
{
namespace advertising
{
const int internalAdDisplayTime = 10;                                   // @0041c0d0
const float firstInterstitialIntervalSeconds = 60.0f;                   // @0041c0d4
const float subsequentInterstitialIntervalSeconds = 180.0f;             // @0041c0d8
} // namespace advertising

namespace ui
{
const float postDeathDelayToAllowAdToLoadOrRegisterImpression = 3.0f;   // @0041c0dc
const float menuFadeTime = 0.4f;                                        // @0041c0e0
const int alertWindowDepth = 10000;                                     // @0041c0e4

namespace editor
{
float labelFontSize = 100.0f;                                           // @00abb5fc
float menuButtonSize = 225.0f;                                          // @00abb600
float menuButtonSpace = 60.0f;                                          // @00abb604
float menuInsetFromScreenEdge = 150.0f;                                 // @00abb608
float editorLabelFontSize = 60.0f;                                      // @00abb60c
float windowDefaultWidth = 2000.0f;                                     // @00abb610
GLubyte moveRotCircDefaultOpacity = 64;                                 // @00abb614
GLubyte moveRotCircPressedOpacity = 128;                                // @00abb615

namespace windows
{
float inputObjectLeftSpace = 100.0f;                                    // @00abb618
float inputObjectRightSpace = 200.0f;                                   // @00abb61c
float inputObjectTopSpace = 35.0f;                                      // @00abb620
float inputObjectBottomSpace = 35.0f;                                   // @00abb624
float inputObjectLabelBottomSpace = 25.0f;                              // @00abb628
} // namespace windows
} // namespace editor
} // namespace ui

namespace flash
{
const float ptmRatio = 62.5f;                                           // @0041c0e8
const int fps = 30;                                                     // @0041c0ec
const cocos2d::Size stageSizeMeters = cocos2d::Size(320.0f, 160.0f);    // @00ac61ac
} // namespace flash

namespace colors
{
const cocos2d::Color3B yellow = cocos2d::Color3B(255, 255, 102);        // @00ac61b4
const cocos2d::Color3B blue = cocos2d::Color3B(61, 139, 199);           // @00ac61b7
const cocos2d::Color3B pink = cocos2d::Color3B(253, 129, 129);          // @00ac61ba
} // namespace colors
} // namespace globals
