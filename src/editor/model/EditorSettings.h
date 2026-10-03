#pragma once
// EditorSettings — the level-editor half of the iOS `Settings` class (reached on iOS as
// Session.sharedSession.settings): the editor item registry. Named EditorSettings because the
// game already has an (Android) `Settings`; the game-side members of iOS Settings (version,
// bdIndex, deviceType, architecture) stay with the game and are not ported here.
//
// Registry (iOS -[Settings init] @1000c0820), item id -> ref class / name key:
//   3 IBeamRef IBEAM          4 LogRef LOG              6 SpikesRef SPIKES
//  15 HarpoonGunRef HARPOONGUN 2 MineRef MINE           7 WreckingBallRef WRECKINGBALL
//   5 SpringBoxRef SPRINGBOX   8 FanRef FAN            12 BoostPanelRef BOOSTPANEL
// 5001 SlowMotionPanelRef SLOWMOTIONPANEL 28 JetRef JET 5000 CharacterRef CHARACTER
//   9 FinishLineRef FINISHLINE 0 VanRef VAN          6000 RectangleRefShape RECT
// 6001 CircleRefShape CIRC  6002 TriangleRefShape TRIANGLE  23 SignRef SIGN (no such class
//  on iOS: NSClassFromString -> nil)  20 BottleRef BOTTLE  25 HomingMineRef HOMINGMINE
//  29 ArrowGunRef ARROWGUN  34 BladeWeaponRef BLADEWEAPON  10 SoccerBallRef SOCCERBALL
// (TokenRef 31 and DecorationRef 5004 exist but are not registered.)
// Sections (AddSpecialItemUIView), names are Localizable keys:
//   SHAPES {6000,6001,6002}  BUILDING BLOCKS {3,4}  HAZARDS {29,6,15,2,25,7}
//   MOVEMENT {5,8,12,5001,28}  OBJECTIVES {9}  MISCELLANEOUS {34,20,0,10}

#include <functional>
#include <map>
#include <string>
#include <vector>

class Special;

class EditorSettings
{
public:
    // `[[Class alloc] init]` for a registered class: returns T::create(); empty function for
    // an id whose class does not exist (SignRef) or is not registered.
    using RefFactory = std::function<Special*()>;

    static EditorSettings* getInstance();   // port: iOS Session.sharedSession.settings

    // NSClassFromString(levelItems[id]) alloc/init — a new (autoreleased) ref or nullptr.
    Special* objectForKey(unsigned int levelItemID);                         // @ios 1000c12c8
    RefFactory levelItemRefClass(unsigned int levelItemID);                  // @ios 1000c1308
    // Game class name: levelItems[id] cut at "Ref" then at "Shape" ("IBeamRef" -> "IBeam",
    // "CircleRefShape" -> "Circle"). Used by the iOS game's LevelB2D only.
    std::string levelItemClass(unsigned int levelItemID);                    // @ios 1000c133c
    // Localized display name (Localization::get(levelItemNames key)); "" if unregistered.
    std::string nameForLevelItem(unsigned int levelItemID);                  // @ios 1000c1398
    // Ref class name, e.g. "IBeamRef" (AddSpecialItemUIView builds "<key>_cellIcon.png").
    std::string keyForLevelItem(unsigned int levelItemID);                   // @ios 1000c13c8
    const std::map<int, std::string>& levelItems();                          // @ios 1000c13f8
    const std::vector<std::vector<int>>& sectionedLevelItemIDs();            // @ios 1000c1408
    const std::vector<std::string>& sectionNames();   // localized          // @ios 1000c1410
    float scrollViewYOffset();                                               // @ios 1000c1418
    void setScrollViewYOffset(float scrollViewYOffset);                      // @ios 1000c1420

private:
    EditorSettings();                                                        // @ios 1000c0820 (init)

    std::map<int, std::string> _levelItems;           // levelItems       id -> class name
    std::map<int, std::string> _levelItemNames;       // levelItemNames   id -> Localizable key
    std::vector<std::vector<int>> _sectionedLevelItemIDs;  // sectionedLevelItemIDs
    std::vector<std::string> _sectionNames;           // sectionNames     localized
    float _scrollViewYOffset;                         // scrollViewYOffset
    std::map<std::string, RefFactory> _factories;     // port: NSClassFromString
};
