// RESTORED (PC addition): see Restored.h.

#include "restored/Restored.h"

#include <algorithm>

#include "cocos2d.h"

#include "GameplayBtn.h"
#include "LevelB2D.h"
#include "Session.h"
#include "Settings.h"
#include "ExplorerGuy.h"
#include "HelicopterMan.h"
#include "IrresponsibleMom.h"
#include "LawnMowerMan.h"
#include "SantaClaus.h"

USING_NS_CC;

namespace restored {

namespace {

const char* kRoot = "generated/restored/";
const char* kList = "Characters_restored.plist";

// Full path of Characters_restored.plist, or "" when the characters were not generated. Found
// through the search paths once addSearchPaths ran, else next to them / the resource root.
std::string listPath()
{
    FileUtils* fileUtils = FileUtils::getInstance();
    if (fileUtils->isFileExist(kList)) {
        return fileUtils->fullPathForFilename(kList);
    }
    std::vector<std::string> paths = fileUtils->getSearchPaths();
    paths.push_back(fileUtils->getDefaultResourceRootPath());
    // Win32: the resource root is <exe dir>/Resources/, generated/ sits next to the exe.
    paths.push_back(fileUtils->getDefaultResourceRootPath() + "../");
    paths.push_back("");
    for (const std::string& path : paths) {
        std::string candidate = path + kRoot + "shared/" + kList;
        if (fileUtils->isFileExist(candidate)) {
            return fileUtils->fullPathForFilename(candidate);
        }
    }
    return "";
}

bool available()
{
    return !listPath().empty();
}

// Ids listed in Characters_restored.plist (only the generated characters are there).
const std::vector<int>& generatedIds()
{
    static std::vector<int> ids;
    static bool loaded = false;
    if (!loaded) {
        std::string path = listPath();
        if (path.empty()) {
            return ids;  // try again later (the search paths may not be set up yet)
        }
        loaded = true;
        for (const Value& entry : FileUtils::getInstance()->getValueVectorFromFile(path)) {
            if (entry.getType() == Value::Type::MAP) {
                ids.push_back(entry.asValueMap().at("id").asInt());
            }
        }
    }
    return ids;
}

bool hasFrame(const std::string& frame)
{
    const std::string sheet = "controls/restored_controls.plist";
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    if (!cache->isSpriteFramesWithFileLoaded(sheet) && FileUtils::getInstance()->isFileExist(sheet)) {
        cache->addSpriteFramesWithFile(sheet);
    }
    return cache->getSpriteFrameByName(frame) != nullptr;
}

GameplayBtn* button(const std::string& frame, unsigned int bit, float userScale)
{
    GameplayBtn* btn = GameplayBtn::createWithSpriteFrameName(frame, bit, userScale);
    if (btn) {
        btn->setAdjustedScale(1.0f);
    }
    return btn;
}

// The main character of the running gameplay session, or nullptr.
CharacterB2D* mainCharacter()
{
    Session* session = Settings::getInstance()->getCurrentSession();
    LevelB2D* level = session ? session->getLevel() : nullptr;
    return level ? level->getCharacter() : nullptr;
}

// Flash PlayableCharacterB2D: the ragdoll alone. Only the Explorer needs a change from the
// riding set-up: PlayableCharacterB2D's helmetedChars (2, 3, 8, 9, 10, 11) leave him out, so his
// hat never flies off and his head smashes at the usual limit.
class BareRestoredCharacter : public CharacterB2D
{
public:
    void keepHatOn()
    {
        if (_helmetOn && _headFixture)
        {
            _helmetOn = false;
            _contactImpulseDict[_headFixture] = _headSmashLimit;
        }
    }
};

void place(GameplayBtn* btn, const Vec2& position)
{
    btn->setPosition(position);
    btn->setHitArea(Rect(Rect::ZERO));
    btn->nudgeBounds(36.0f, 36.0f, 36.0f, 36.0f);
}

}  // namespace

void addSearchPaths(const std::string& tier)
{
    // generated/restored/ sits next to the exe on Win32 (a search path of its own, not the
    // resource root) and in the APK's assets on Android: look it up under every search path.
    FileUtils* fileUtils = FileUtils::getInstance();
    std::string root = kRoot;
    std::vector<std::string> paths = fileUtils->getSearchPaths();
    paths.push_back(fileUtils->getDefaultResourceRootPath());
    for (const std::string& path : paths)
    {
        std::string candidate = path + kRoot;
        if (fileUtils->isFileExist(candidate + "shared/" + kList))
        {
            root = candidate;
            break;
        }
    }
    fileUtils->addSearchPath(root + "shared");
    fileUtils->addSearchPath(root + tier);
    fileUtils->addSearchPath(root + "sounds");
}

void appendCharacters(ValueVector& characters)
{
    std::string path = listPath();
    if (path.empty())
    {
        return;
    }
    ValueVector extra = FileUtils::getInstance()->getValueVectorFromFile(path);
    for (const Value& entry : extra)
    {
        if (entry.getType() != Value::Type::MAP)
        {
            continue;
        }
        int id = entry.asValueMap().at("id").asInt();
        bool duplicate = false;
        for (const Value& existing : characters)
        {
            duplicate = duplicate || existing.asValueMap().at("id").asInt() == id;
        }
        if (!duplicate)
        {
            characters.push_back(entry);
        }
    }
}

bool hasCharacter(int characterId)
{
    const std::vector<int>& ids = generatedIds();
    return std::find(ids.begin(), ids.end(), characterId) != ids.end();
}

CharacterB2D* createCharacter(float x, float y, int characterId, int groupIndex, bool showGore)
{
    if (!hasCharacter(characterId))
    {
        return nullptr;
    }
    switch (characterId)
    {
    case 6:
    {
        LawnMowerMan* character = new LawnMowerMan();
        character->init(Vec2(x, y), "lawnmower_man", "lawnmower", groupIndex, showGore);
        return character;
    }
    case 7:
    {
        // Flash MiddleAgedExplorer: x + 20, y - 40 Flash px (y down) from the start point.
        ExplorerGuy* character = new ExplorerGuy();
        character->init(Vec2(x + 20.0f * 0.016f, y + 40.0f * 0.016f), "explorer_guy", "mine_cart",
                        groupIndex, showGore);
        return character;
    }
    case 8:
    {
        SantaClaus* character = new SantaClaus();
        character->init(Vec2(x, y), "santa_claus", "sleigh", groupIndex, showGore);
        return character;
    }
    case 11:
    {
        HelicopterMan* character = new HelicopterMan();
        character->init(Vec2(x, y), "helicopter_man", "helicopter", groupIndex, showGore);
        return character;
    }
    case 10:
    {
        IrresponsibleMom* character = new IrresponsibleMom();
        character->init(Vec2(x, y), "irresponsible_mom", "mom_bike", groupIndex, showGore);
        return character;
    }
    default:
        return nullptr;
    }
}

CharacterB2D* createBareCharacter(float x, float y, int characterId, bool showGore)
{
    if (!hasCharacter(characterId))
    {
        return nullptr;
    }
    // Flash PlayableCharacterB2D.tags: the voice of each character index.
    const char* name = nullptr;
    const char* vehicle = nullptr;
    const char* vocals = nullptr;
    switch (characterId)
    {
    case 6: name = "lawnmower_man"; vehicle = "lawnmower"; vocals = "Char11"; break;
    case 7: name = "explorer_guy"; vehicle = "mine_cart"; vocals = "Char2"; break;
    case 8: name = "santa_claus"; vehicle = "sleigh"; vocals = "Santa"; break;
    case 10: name = "irresponsible_mom"; vehicle = "mom_bike"; vocals = "Char4"; break;
    case 11: name = "helicopter_man"; vehicle = "helicopter"; vocals = "Heli"; break;
    default: return nullptr;
    }
    // The body description is the riding one (<name>_<vehicle>.plist); Flash builds the bare
    // ragdoll from the same shape guide, at the start point itself (no per-vehicle offset).
    BareRestoredCharacter* character = new BareRestoredCharacter();
    character->init(Vec2(x, y), name, vocals, vehicle, -1, showGore, true);
    if (characterId == 7)
    {
        character->keepHatOn();
    }
    return character;
}

void loadIconFrames()
{
    const std::string sheet = "menus/character_select/restored_icons.plist";
    if (available() && FileUtils::getInstance()->isFileExist(sheet))
    {
        SpriteFrameCache::getInstance()->addSpriteFramesWithFile(sheet);
    }
}

bool controlsMeter(int type)
{
    if (type != ControlsTypeSanta)
    {
        return false;
    }
    // No sleigh in a "hide vehicle" start (createBareCharacter): no flight meter either.
    CharacterB2D* character = mainCharacter();
    return !character || dynamic_cast<SantaClaus*>(character) != nullptr;
}

bool childGore()
{
    return UserDefault::getInstance()->getBoolForKey("qol_child_gore", true);
}

void controlsSpecial(int type, int overrideSpecialPosition, std::string* frame, bool* onLeft)
{
    switch (type)
    {
    case ControlsTypeLawnMower:
        if (hasFrame("restored_btn_deck_lift.png"))
        {
            *frame = "restored_btn_deck_lift.png";
        }
        if (overrideSpecialPosition == 0)
        {
            *onLeft = true;
        }
        break;
    case ControlsTypeIrresponsibleMom:
        *frame = "controls_gameplay_btn_break.png";
        break;
    case ControlsTypeSanta:
        *frame = "controls_gameplay_btn_jet.png";  // flight (the Moped's boost button)
        break;
    case ControlsTypeExplorer:
        if (hasFrame("restored_btn_rail.png"))
        {
            *frame = "restored_btn_rail.png";
        }
        break;
    case ControlsTypeHelicopter:
        if (hasFrame("restored_btn_magnet.png"))
        {
            *frame = "restored_btn_magnet.png";
        }
        break;
    default:
        break;
    }
}

std::vector<GameplayBtn*> extraControls(int type, bool ejected, float userScale, const Vec2& leanBackPos,
                                        const Vec2& leanForwardPos, float leanHeight, const Vec2& grabPos,
                                        float grabHeight, float spacing)
{
    std::vector<GameplayBtn*> buttons;
    // A "hide vehicle" start (createBareCharacter) has no kids or elves to let go of, and Flash
    // PlayableCharacterB2D gives shift / ctrl nothing to do.
    if (ejected && (type == ControlsTypeIrresponsibleMom || type == ControlsTypeSanta))
    {
        CharacterB2D* character = mainCharacter();
        const bool bare = character && (type == ControlsTypeIrresponsibleMom
                                            ? dynamic_cast<IrresponsibleMom*>(character) == nullptr
                                            : dynamic_cast<SantaClaus*>(character) == nullptr);
        if (bare)
        {
            return buttons;
        }
    }
    // Two extra buttons (control bits 0x20 = Flash shift, 0x40 = Flash ctrl).
    const char* frames[2] = {nullptr, nullptr};
    switch (type)
    {
    case ControlsTypeIrresponsibleMom:
        frames[0] = "restored_btn_eject_son.png";       // shift: the son out of the basket
        frames[1] = "restored_btn_eject_daughter.png";  // ctrl: the daughter off her bike
        break;
    case ControlsTypeExplorer:
        if (ejected)
        {
            return buttons;  // (Flash: shift / ctrl do nothing once he is out)
        }
        frames[0] = "restored_btn_stand.png";   // shift: stand up in the cart
        frames[1] = "restored_btn_crouch.png";  // ctrl: duck
        break;
    case ControlsTypeHelicopter:
        if (ejected)
        {
            return buttons;  // (the rope is the copter's: nothing to reel once he is out)
        }
        frames[0] = "restored_btn_rope_up.png";    // shift: reel the magnet in
        frames[1] = "restored_btn_rope_down.png";  // ctrl: let it down
        break;
    case ControlsTypeSanta:
    {
        // shift while riding: let go of the elves that cannot run; once Santa is off, the eject
        // bit (Flash Z) releases both - one button, above the special / grab button.
        if (!hasFrame("restored_btn_release_elves.png"))
        {
            return buttons;
        }
        GameplayBtn* release = button("restored_btn_release_elves.png", ejected ? 0x80 : 0x20, userScale);
        if (!release)
        {
            return buttons;
        }
        Size size = release->getContentSize();
        if (!ejected)
        {
            float y = leanBackPos.y + leanHeight * 0.5f + spacing + size.height * 0.5f;
            place(release, Vec2(leanBackPos.x, y));
        }
        else
        {
            place(release, Vec2(grabPos.x, grabPos.y + grabHeight * 0.5f + spacing + size.height * 0.5f));
        }
        buttons.push_back(release);
        return buttons;
    }
    default:
        return buttons;
    }
    if (!hasFrame(frames[0]) || !hasFrame(frames[1]))
    {
        return buttons;
    }
    GameplayBtn* first = button(frames[0], 0x20, userScale);
    GameplayBtn* second = button(frames[1], 0x40, userScale);
    if (!first || !second)
    {
        return buttons;
    }
    Size size = first->getContentSize();
    if (!ejected)
    {
        // Where a left-hand special button goes (GameplayControls: above the lean buttons).
        float y = leanBackPos.y + leanHeight * 0.5f + spacing + size.height * 0.5f;
        place(first, Vec2(leanBackPos.x, y));
        place(second, Vec2(leanForwardPos.x, y));
    }
    else
    {
        // The ejected d-pad takes the left side: above the grab button instead.
        float y = grabPos.y + grabHeight * 0.5f + spacing + size.height * 0.5f;
        place(first, Vec2(grabPos.x, y));
        place(second, Vec2(grabPos.x - size.width - spacing * 0.72f, y));
    }
    buttons.push_back(first);
    buttons.push_back(second);
    return buttons;
}

}  // namespace restored
