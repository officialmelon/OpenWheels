#pragma once

#include "cocos2d.h"

// Game-wide constants. All of these are exported data symbols in libMyGame.so (they are read through
// the GOT, never constant-folded), so the original header declared them extern and one source file
// defined them. Definitions: src/game/Globals.cpp.
//   .rodata (const):   globals::advertising::*, globals::ui::{postDeath..., menuFadeTime,
//                      alertWindowDepth}, globals::flash::{ptmRatio, fps}
//   .data (non-const): globals::ui::editor::* (editor layout values, unused by the 1.1.3 Android game)
//   .bss + dynamic init (_INIT_8 @005beae8): globals::flash::stageSizeMeters, globals::colors::*
//
// Note: the four 8-byte "statics" {0,0} {0,0} {0.1f,0.5f} 0.5f that every TU including cocos2d.h
// initialises are cocos2d's PHYSICSSHAPE_MATERIAL_DEFAULT / PHYSICSBODY_MATERIAL_DEFAULT
// (cocos/physics/CCPhysicsShape.h, CCPhysicsBody.h), not game data.
namespace globals
{
namespace colors
{
extern const cocos2d::Color3B yellow;                                   // @00ac61b4 (255, 255, 102)
extern const cocos2d::Color3B blue;                                     // @00ac61b7 ( 61, 139, 199)
extern const cocos2d::Color3B pink;                                     // @00ac61ba (253, 129, 129)
} // namespace colors

namespace ui
{
extern const float postDeathDelayToAllowAdToLoadOrRegisterImpression;   // @0041c0dc 3.0f
extern const float menuFadeTime;                                        // @0041c0e0 0.4f
extern const int alertWindowDepth;                                      // @0041c0e4 10000 (z order)

namespace editor
{
extern float labelFontSize;                                             // @00abb5fc 100.0f
extern float menuButtonSize;                                            // @00abb600 225.0f
extern float menuButtonSpace;                                           // @00abb604 60.0f
extern float menuInsetFromScreenEdge;                                   // @00abb608 150.0f
extern float editorLabelFontSize;                                       // @00abb60c 60.0f
extern float windowDefaultWidth;                                        // @00abb610 2000.0f
extern GLubyte moveRotCircDefaultOpacity;                               // @00abb614 64
extern GLubyte moveRotCircPressedOpacity;                               // @00abb615 128

namespace windows
{
extern float inputObjectLeftSpace;                                      // @00abb618 100.0f
extern float inputObjectRightSpace;                                     // @00abb61c 200.0f
extern float inputObjectTopSpace;                                       // @00abb620 35.0f
extern float inputObjectBottomSpace;                                    // @00abb624 35.0f
extern float inputObjectLabelBottomSpace;                               // @00abb628 25.0f
} // namespace windows
} // namespace editor
} // namespace ui

namespace flash
{
// Pixels per Box2D metre of the original Flash game's level coordinates (level XML -> metres).
extern const float ptmRatio;                                            // @0041c0e8 62.5f
extern const int fps;                                                   // @0041c0ec 30
extern const cocos2d::Size stageSizeMeters;                             // @00ac61ac (320, 160)
} // namespace flash

namespace advertising
{
// Seconds the internal (house) ad is shown (Gameplay).
extern const int internalAdDisplayTime;                                 // @0041c0d0 10
extern const float firstInterstitialIntervalSeconds;                    // @0041c0d4 60.0f
extern const float subsequentInterstitialIntervalSeconds;               // @0041c0d8 180.0f
} // namespace advertising
} // namespace globals
