#include "EditorSettings.h"

#include "platform/common/Localization.h"

#include "ArrowGunRef.h"
#include "BladeWeaponRef.h"
#include "BoostPanelRef.h"
#include "BottleRef.h"
#include "CharacterRef.h"
#include "CircleRefShape.h"
#include "DecorationRef.h"
#include "FanRef.h"
#include "FinishLineRef.h"
#include "HarpoonGunRef.h"
#include "HomingMineRef.h"
#include "IBeamRef.h"
#include "JetRef.h"
#include "LogRef.h"
#include "MineRef.h"
#include "RectangleRefShape.h"
#include "SlowMotionPanelRef.h"
#include "SoccerBallRef.h"
#include "SpikesRef.h"
#include "SpringBoxRef.h"
#include "TokenRef.h"
#include "TriangleRefShape.h"
#include "VanRef.h"
#include "WreckingBallRef.h"
// EDITOR (browser features, PC addition)
#include "FlashCatalog.h"
#include "FlashSpecialRef.h"
#include "GroupRef.h"
#include "JointRef.h"
#include "PolygonRefShape.h"
#include "TriggerRef.h"

namespace {

template <class T>
EditorSettings::RefFactory factory()
{
    return []() -> Special* { return T::create(); };
}

}  // namespace

EditorSettings* EditorSettings::getInstance()
{
    static EditorSettings* instance = new EditorSettings();
    return instance;
}

// @ios 1000c0820
EditorSettings::EditorSettings()
    : _scrollViewYOffset(0.0f)
{
    // sectionNames (localized once, as iOS does in -init)
    for (const char* key : {"SHAPES", "BUILDING BLOCKS", "HAZARDS", "MOVEMENT", "OBJECTIVES",
                            "MISCELLANEOUS"})
    {
        _sectionNames.push_back(Localization::get(key));
    }
    _sectionedLevelItemIDs = {
        {6000, 6001, 6002},
        {3, 4},
        {29, 6, 15, 2, 25, 7},
        {5, 8, 12, 5001, 28},
        {9},
        {34, 20, 0, 10},
    };
    // EDITOR (browser features, PC addition): the browser game's items, logic and drawing tools
    // (src/editor/flash/). Names are the browser editor's, not iOS text.
    _sectionedLevelItemIDs[0].insert(_sectionedLevelItemIDs[0].end(),
                                     {PolygonRefShape::kPolygonLevelItemID, PolygonRefShape::kArtLevelItemID});
    _sectionedLevelItemIDs[4].push_back(31);  // OBJECTIVES (before LOGIC is inserted)
    // Logic (triggers, joints) right after the shapes; the browser items after the iOS ones.
    _sectionNames.insert(_sectionNames.begin() + 1, Localization::get("LOGIC"));
    _sectionedLevelItemIDs.insert(_sectionedLevelItemIDs.begin() + 1,
                                  {TriggerRef::kLevelItemID, JointRef::kPinLevelItemID, JointRef::kSlideLevelItemID});
    for (const char* name : {"CHARACTERS & TEXT", "FURNITURE", "MORE ITEMS", "BUILDINGS"})
    {
        _sectionNames.push_back(Localization::get(name));
    }
    _sectionedLevelItemIDs.push_back({17, 16, 23});
    _sectionedLevelItemIDs.push_back({1, 19, 21, 22, 24, 26, 32, 18});
    _sectionedLevelItemIDs.push_back({11, 33, 35, 30, 27});
    _sectionedLevelItemIDs.push_back({13, 14});

    // levelItems (id -> class name) and levelItemNames (id -> Localizable key), in iOS order.
    struct Item
    {
        int id;
        const char* className;
        const char* nameKey;
    };
    static const Item kItems[] = {
        {3, "IBeamRef", "IBEAM"},
        {4, "LogRef", "LOG"},
        {6, "SpikesRef", "SPIKES"},
        {15, "HarpoonGunRef", "HARPOONGUN"},
        {2, "MineRef", "MINE"},
        {7, "WreckingBallRef", "WRECKINGBALL"},
        {5, "SpringBoxRef", "SPRINGBOX"},
        {8, "FanRef", "FAN"},
        {12, "BoostPanelRef", "BOOSTPANEL"},
        {5001, "SlowMotionPanelRef", "SLOWMOTIONPANEL"},
        {28, "JetRef", "JET"},
        {5000, "CharacterRef", "CHARACTER"},
        {9, "FinishLineRef", "FINISHLINE"},
        {0, "VanRef", "VAN"},
        {6000, "RectangleRefShape", "RECT"},
        {6001, "CircleRefShape", "CIRC"},
        {6002, "TriangleRefShape", "TRIANGLE"},
        {23, "SignRef", "SIGN"},  // no such class in 1.2.7 (NSClassFromString -> nil)
        {20, "BottleRef", "BOTTLE"},
        {25, "HomingMineRef", "HOMINGMINE"},
        {29, "ArrowGunRef", "ARROWGUN"},
        {34, "BladeWeaponRef", "BLADEWEAPON"},
        {10, "SoccerBallRef", "SOCCERBALL"},
    };
    for (const Item& item : kItems)
    {
        _levelItems[item.id] = item.className;
        _levelItemNames[item.id] = item.nameKey;
    }
    // EDITOR (browser features, PC addition)
    for (int type : flashed::flashSpecialTypes())
    {
        const std::string className = "FlashSpecialRef" + std::to_string(type);
        _levelItems[type] = className;
        _levelItemNames[type] = flashed::specialInfo(type)->name;
        _factories[className] = [type]() -> Special* { return FlashSpecialRef::create(type); };
        _icons[type] = flashed::specialInfo(type)->art0;
    }
    _icons[17] = "ed_npc_1";
    _icons[23] = "ed_sign_1";
    _icons[16] = "ic_text";
    _icons[13] = "b1_win_front";
    _icons[14] = "b2_roof";
    _icons[18] = "";
    _icons[30] = "chain_l0_1";
    struct Extra
    {
        int id;
        const char* className;
        const char* name;
        const char* icon;
        RefFactory make;
    };
    const Extra extras[] = {
        {PolygonRefShape::kPolygonLevelItemID, "PolygonRefShape", "polygon", "ic_polygon", []() -> Special* { return PolygonRefShape::create(false); }},
        {PolygonRefShape::kArtLevelItemID, "ArtRefShape", "art shape", "ic_art", []() -> Special* { return PolygonRefShape::create(true); }},
        {TriggerRef::kLevelItemID, "TriggerRef", "trigger", "ic_trigger", []() -> Special* { return TriggerRef::create(); }},
        {JointRef::kPinLevelItemID, "PinJointRef", "pin joint", "ic_joint", []() -> Special* { return JointRef::create(false); }},
        {JointRef::kSlideLevelItemID, "SlideJointRef", "sliding joint", "ic_slide", []() -> Special* { return JointRef::create(true); }},
        {GroupRef::kLevelItemID, "GroupRef", "group", "", []() -> Special* { return GroupRef::create(); }},
    };
    for (const Extra& e : extras)
    {
        _levelItems[e.id] = e.className;
        _levelItemNames[e.id] = e.name;
        _factories[e.className] = e.make;
        _icons[e.id] = e.icon;
    }

    // port: NSClassFromString for every ref class in the binary (registered or not).
    _factories.insert({  // EDITOR (browser features, PC addition): insert, keeping the browser refs registered above
        {"ArrowGunRef", factory<ArrowGunRef>()},
        {"BladeWeaponRef", factory<BladeWeaponRef>()},
        {"BoostPanelRef", factory<BoostPanelRef>()},
        {"BottleRef", factory<BottleRef>()},
        {"CharacterRef", factory<CharacterRef>()},
        {"CircleRefShape", factory<CircleRefShape>()},
        {"DecorationRef", factory<DecorationRef>()},
        {"FanRef", factory<FanRef>()},
        {"FinishLineRef", factory<FinishLineRef>()},
        {"HarpoonGunRef", factory<HarpoonGunRef>()},
        {"HomingMineRef", factory<HomingMineRef>()},
        {"IBeamRef", factory<IBeamRef>()},
        {"JetRef", factory<JetRef>()},
        {"LogRef", factory<LogRef>()},
        {"MineRef", factory<MineRef>()},
        {"RectangleRefShape", factory<RectangleRefShape>()},
        {"SlowMotionPanelRef", factory<SlowMotionPanelRef>()},
        {"SoccerBallRef", factory<SoccerBallRef>()},
        {"SpikesRef", factory<SpikesRef>()},
        {"SpringBoxRef", factory<SpringBoxRef>()},
        {"TokenRef", factory<TokenRef>()},
        {"TriangleRefShape", factory<TriangleRefShape>()},
        {"VanRef", factory<VanRef>()},
        {"WreckingBallRef", factory<WreckingBallRef>()},
    });
}

// @ios 1000c12c8
Special* EditorSettings::objectForKey(unsigned int levelItemID)
{
    RefFactory make = levelItemRefClass(levelItemID);
    return make ? make() : nullptr;
}

// @ios 1000c1308
EditorSettings::RefFactory EditorSettings::levelItemRefClass(unsigned int levelItemID)
{
    auto item = _levelItems.find((int)levelItemID);
    if (item == _levelItems.end())
    {
        return RefFactory();
    }
    auto it = _factories.find(item->second);
    return it == _factories.end() ? RefFactory() : it->second;
}

// @ios 1000c133c
std::string EditorSettings::levelItemClass(unsigned int levelItemID)
{
    auto item = _levelItems.find((int)levelItemID);
    if (item == _levelItems.end())
    {
        return "";
    }
    // [[[name componentsSeparatedByString:@"Ref"][0] componentsSeparatedByString:@"Shape"][0]
    std::string name = item->second.substr(0, item->second.find("Ref"));
    return name.substr(0, name.find("Shape"));
}

// @ios 1000c1398
std::string EditorSettings::nameForLevelItem(unsigned int levelItemID)
{
    auto it = _levelItemNames.find((int)levelItemID);
    return it == _levelItemNames.end() ? "" : Localization::get(it->second);
}

// @ios 1000c13c8
std::string EditorSettings::keyForLevelItem(unsigned int levelItemID)
{
    auto it = _levelItems.find((int)levelItemID);
    return it == _levelItems.end() ? "" : it->second;
}

// EDITOR (browser features, PC addition)
std::string EditorSettings::iconForLevelItem(unsigned int levelItemID)
{
    auto it = _icons.find((int)levelItemID);
    return it == _icons.end() ? "" : it->second;
}

// @ios 1000c13f8
const std::map<int, std::string>& EditorSettings::levelItems()
{
    return _levelItems;
}

// @ios 1000c1408
const std::vector<std::vector<int>>& EditorSettings::sectionedLevelItemIDs()
{
    return _sectionedLevelItemIDs;
}

// @ios 1000c1410
const std::vector<std::string>& EditorSettings::sectionNames()
{
    return _sectionNames;
}

// @ios 1000c1418
float EditorSettings::scrollViewYOffset()
{
    return _scrollViewYOffset;
}

// @ios 1000c1420
void EditorSettings::setScrollViewYOffset(float scrollViewYOffset)
{
    _scrollViewYOffset = scrollViewYOffset;
}
