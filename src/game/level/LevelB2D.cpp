#include "LevelB2D.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "cocos2d.h"
#include "tinyxml2/tinyxml2.h"

#include "ArrowGun.h"
#include "BackgroundLayer.h"
#include "BladeWeapon.h"
#include "BoostPanel.h"
#include "Bottle.h"
#include "BusinessGuy.h"
#include "Chain.h"
#include "CharacterB2D.h"
#include "CircleShape.h"
#include "EffectiveShopper.h"
#include "FFDrawNode.h"
#include "Fan.h"
#include "FinishLine.h"
#include "GroupItem.h"
#include "HarpoonGun.h"
#include "HomingMine.h"
#include "IBeam.h"
#include "IrresponsibleDad.h"
#include "Jet.h"
#include "LevelDataElement.h"
#include "LevelItem.h"
#include "LevelXMLParser.h"
#include "Log.h"
#include "Mine.h"
#include "MopedCouple.h"
#include "Patch.h"
#include "PogostickGuy.h"
#include "PolygonShape.h"
#include "RectangleShape.h"
#include "Session.h"
#include "Settings.h"
#include "ShapeItem.h"
#include "Sign.h"
#include "SlowMotionPanel.h"
#include "SoccerBall.h"
#include "Spikes.h"
#include "SpringBox.h"
#include "TargetAction.h"
#include "TargetActionGroup.h"
#include "TargetActionPrisJoint.h"
#include "TargetActionRevJoint.h"
#include "TargetActionSpecial.h"
#include "TargetActionTrigger.h"
#include "TerrainNode.h"
#include "TerrainShape.h"
#include "Token.h"
#include "TriangleShape.h"
#include "Trigger.h"
#include "Van.h"
#include "WheelchairGuy.h"
#include "WreckingBall.h"
#include "online/BareCharacter.h"   // ONLINE (PC addition)
#include "online/FlashRuntime.h"   // ONLINE (PC addition)
#include "online/FlashPhysics.h"  // ONLINE (PC addition)
#include "online/items/FlashSpecials.h"  // ONLINE (PC addition)
#include "online/vehicles/UserVehicle.h"  // ONLINE (PC addition)
#include "qol/CharacterChoice.h"  // QOL (PC addition)
#include "restored/Restored.h"  // RESTORED (PC addition)

USING_NS_CC;

// @005cd484
LevelB2D::LevelB2D()
    : _version(-1.0f),
      _stageWidth(0.0f),
      _stageHeight(0.0f),
      _sourcePtmRatio(1.0f),
      _registration(false),
      _clockwise(false),
      _levelComplete(false),
      _frameActionTimer(0.0f)
{
    _ptmRatio = Settings::getInstance()->getCurrentSession()->getPtmRatio();
    _characters.clear();
    _specials.clear();
    _shapeItems.clear();
    _groupItems.clear();
    _foregroundGroupItems.clear();
    _triggers.clear();
    // ONLINE (PC addition): a new level is not a browser level until addInfo finds src="flash";
    // character select's level (no init, no info) must not run the browser hooks of the level
    // played before it.
    online::setFlashLevel(false, 0.0f);
}

// @005cd79c
LevelB2D::~LevelB2D()
{
    online::destroyUserVehicles(this);  // ONLINE (PC addition): browser user vehicles
    for (unsigned int i = 0; i < _characters.size(); i++)
    {
        if (_characters[i] != nullptr)
        {
            delete _characters[i];
        }
    }
    _characters.clear();

    for (auto it = _specials.begin(); it != _specials.end(); ++it)
    {
        (*it)->release();
    }
    _specials.clear();

    for (auto it = _shapeItems.begin(); it != _shapeItems.end(); ++it)
    {
        if (*it != nullptr)
        {
            delete *it;
        }
    }
    _shapeItems.clear();

    int groupCount = (int)_groupItems.size();
    int foregroundGroupCount = (int)_foregroundGroupItems.size();
    if (foregroundGroupCount > 0)
    {
        for (auto it = _foregroundGroupItems.begin(); it != _foregroundGroupItems.end(); ++it)
        {
            if (*it != nullptr)
            {
                delete *it;
            }
        }
    }
    _foregroundGroupItems.clear();
    if (groupCount > 0)
    {
        for (auto it = _groupItems.begin(); it != _groupItems.end(); ++it)
        {
            if (*it != nullptr)
            {
                delete *it;
            }
        }
    }
    _groupItems.clear();

    for (auto it = _triggers.begin(); it != _triggers.end(); ++it)
    {
        (*it)->release();
    }
    _triggers.clear();
}

// @005cdaf4
bool LevelB2D::init(std::string xml)
{
    _ptmRatio = Settings::getInstance()->getCurrentSession()->getPtmRatio();
    if (xml.length() == 0)
    {
        xml = Settings::getInstance()->getLevelXMLData();
    }
    LevelXMLParser* parser = new LevelXMLParser();
    bool result = parser->init(xml, this);
    delete parser;
    _levelComplete = false;
    return result;
}

// @005cdc14
void LevelB2D::die()
{
    for (auto it = _specials.begin(); it != _specials.end(); ++it)
    {
        (*it)->die();
    }
}

// @005cdc58
void LevelB2D::update(float dt)
{
    int groupCount = (int)_groupItems.size();
    int foregroundGroupCount = (int)_foregroundGroupItems.size();
    if (foregroundGroupCount > 0)
    {
        for (auto it = _foregroundGroupItems.begin(); it != _foregroundGroupItems.end(); ++it)
        {
            (*it)->update();
        }
    }
    if (groupCount > 0)
    {
        for (auto it = _groupItems.begin(); it != _groupItems.end(); ++it)
        {
            (*it)->update();
        }
    }

    if (online::flashLevel())
    {
        // ONLINE (PC addition): Flash LevelB2D.actions walks singleActionVector by index over the
        // length it had when the walk started; a trigger disabled by an earlier one is spliced
        // out (here: nulled, see removeFromSingleActions) and does not run.
        _onlineRunningSingleActions = true;
        const size_t count = _singleActionVector.size();
        for (size_t i = 0; i < count && i < _singleActionVector.size(); i++)
        {
            LevelItem* item = _singleActionVector[i];
            if (item != nullptr)
            {
                item->singleAction();
            }
        }
        _onlineRunningSingleActions = false;
        _singleActionVector.clear();
        // Flash pushes into actionsVector immediately, so everything queued by the single actions
        // (delayed triggers, fades, motor ramps) gets its first actions() call in this frame.
        for (auto it = _actionsToAdd.begin(); it != _actionsToAdd.end(); ++it)
        {
            if (std::find(_actionsVector.begin(), _actionsVector.end(), *it) == _actionsVector.end())
            {
                _actionsVector.push_back(*it);
            }
        }
        _actionsToAdd.clear();
        // actions() may queue more (pushed after Flash took the vector length: next frame).
        const size_t actionCount = _actionsVector.size();
        for (size_t i = 0; i < actionCount; i++)
        {
            _actionsVector[i]->actions();
        }
    }
    else
    {
    for (auto it = _singleActionVector.begin(); it != _singleActionVector.end(); ++it)
    {
        (*it)->singleAction();
    }
    _singleActionVector.clear();

    for (auto it = _actionsVector.begin(); it != _actionsVector.end(); ++it)
    {
        (*it)->actions();
    }
    }

    // Frame actions run at most at 60 Hz.
    if (_frameActionTimer >= 0.016666668f)
    {
        for (auto it = _frameActions.begin(); it != _frameActions.end(); ++it)
        {
            (*it)->frameAction();
        }
        _frameActionTimer = 0.0f;
    }
    _frameActionTimer += dt;

    for (auto it = _actionsToAdd.begin(); it != _actionsToAdd.end(); ++it)
    {
        if (std::find(_actionsVector.begin(), _actionsVector.end(), *it) == _actionsVector.end())
        {
            _actionsVector.push_back(*it);
        }
    }
    _actionsToAdd.clear();

    for (auto it = _actionsToRemove.begin(); it != _actionsToRemove.end(); ++it)
    {
        auto found = std::find(_actionsVector.begin(), _actionsVector.end(), *it);
        if (found != _actionsVector.end())
        {
            _actionsVector.erase(found);
        }
    }
    _actionsToRemove.clear();

    for (auto it = _frameActionsToRemove.begin(); it != _frameActionsToRemove.end(); ++it)
    {
        auto found = std::find(_frameActions.begin(), _frameActions.end(), *it);
        if (found != _frameActions.end())
        {
            _frameActions.erase(found);
        }
    }
    _frameActionsToRemove.clear();
}

// @005ce02c
void LevelB2D::paint()
{
    for (auto it = _paintBodies.begin(); it != _paintBodies.end(); ++it)
    {
        b2Body* body = *it;
        Node* node = static_cast<Node*>(body->GetUserData());
        const b2Vec2& position = body->GetPosition();
        node->setPosition(Vec2(position.x * _ptmRatio, position.y * _ptmRatio));
        node->setRotation(body->GetAngle() * -57.29578f);
    }
    for (auto it = _paintItems.begin(); it != _paintItems.end(); ++it)
    {
        (*it)->paint();
    }
}

// @005ce10c
bool LevelB2D::addToActions(LevelItem* item)
{
    if (std::find(_actionsToAdd.begin(), _actionsToAdd.end(), item) != _actionsToAdd.end())
    {
        return false;
    }
    _actionsToAdd.push_back(item);
    return true;
}

// @005ce2a8
void LevelB2D::addToSingleActions(LevelItem* item)
{
    if (std::find(_singleActionVector.begin(), _singleActionVector.end(), item) ==
        _singleActionVector.end())
    {
        _singleActionVector.push_back(item);
    }
}

// @005ce444
bool LevelB2D::singleActionsContainsLevelItem(LevelItem* item)
{
    return std::find(_singleActionVector.begin(), _singleActionVector.end(), item) !=
           _singleActionVector.end();
}

// @005ce478
bool LevelB2D::actionsContainsLevelItem(LevelItem* item)
{
    return std::find(_actionsVector.begin(), _actionsVector.end(), item) != _actionsVector.end();
}

// ONLINE (PC addition)
bool LevelB2D::onlineActionsContainsLevelItem(LevelItem* item)
{
    return actionsContainsLevelItem(item) ||
           std::find(_actionsToAdd.begin(), _actionsToAdd.end(), item) != _actionsToAdd.end();
}

// @005ce4ac
void LevelB2D::removeFromSingleActions(LevelItem* item)
{
    auto it = std::find(_singleActionVector.begin(), _singleActionVector.end(), item);
    if (it != _singleActionVector.end())
    {
        if (_onlineRunningSingleActions)
        {
            *it = nullptr;  // ONLINE (PC addition): update() is walking the vector
            return;
        }
        _singleActionVector.erase(it);
    }
}

// @005ce510
void LevelB2D::removeFromActions(LevelItem* item)
{
    if (std::find(_actionsToRemove.begin(), _actionsToRemove.end(), item) == _actionsToRemove.end())
    {
        _actionsToRemove.push_back(item);
    }
}

// @005ce6ac
void LevelB2D::addToPaintItem(LevelItem* item)
{
    if (std::find(_paintItems.begin(), _paintItems.end(), item) == _paintItems.end())
    {
        _paintItems.push_back(item);
    }
}

// @005ce848
void LevelB2D::removeFromPaintItem(LevelItem* item)
{
    // No "found" check in the original.
    _paintItems.erase(std::find(_paintItems.begin(), _paintItems.end(), item));
}

// @005ce8a4
void LevelB2D::addToPaintBody(b2Body* body)
{
    if (std::find(_paintBodies.begin(), _paintBodies.end(), body) == _paintBodies.end())
    {
        _paintBodies.push_back(body);
    }
}

// @005cea40
void LevelB2D::removeFromPaintBody(b2Body* body)
{
    // No "found" check in the original.
    _paintBodies.erase(std::find(_paintBodies.begin(), _paintBodies.end(), body));
}

// @005cea9c
void LevelB2D::addToFrameActions(LevelItem* item)
{
    if (std::find(_frameActions.begin(), _frameActions.end(), item) == _frameActions.end())
    {
        _frameActions.push_back(item);
    }
}

// @005cec38
void LevelB2D::removeFromFrameActions(LevelItem* item)
{
    if (std::find(_frameActionsToRemove.begin(), _frameActionsToRemove.end(), item) ==
        _frameActionsToRemove.end())
    {
        _frameActionsToRemove.push_back(item);
    }
}

// @005cedd4
float LevelB2D::getVersion()
{
    return _version;
}

// @005ceddc
float LevelB2D::getSourcePtmRatio()
{
    return _sourcePtmRatio;
}

// @005cede4
void LevelB2D::levelCompleted()
{
    if (!_levelComplete)
    {
        _levelComplete = true;
        Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("levelCompleted");
    }
}

// @005ceea8
void LevelB2D::setTimeStep(float timeStep)
{
    for (int i = 0; i < _characters.size(); i++)
    {
        _characters[i]->timeStepChanged();
    }
    LevelItem::setTimeStep(timeStep);
}

// @005cef08
bool LevelB2D::getLevelComplete()
{
    return _levelComplete;
}

// @005cef10
bool LevelB2D::getForcedChar()
{
    return _forcedChar;
}

// @005cef18
CharacterB2D* LevelB2D::addCharacter(float x, float y, CharacterId characterId,
                                     VehicleId vehicleId, bool hideVehicle, int groupIndex)
{
    CharacterB2D* character = createCharacter(x, y, characterId, vehicleId, false, groupIndex);
    _characters.push_back(character);
    return character;
}

// @005cf084
CharacterB2D* LevelB2D::createCharacter(float x, float y, CharacterId characterId,
                                        VehicleId vehicleId, bool hideVehicle, int groupIndex)
{
    // hideVehicle is not used.
    std::string characterName;
    std::string vehicleName;
    bool showGore = !UserDefault::getInstance()->getBoolForKey("gore_disabled");
    CharacterB2D* character = nullptr;
    bool bareCharacter = false;

    switch (characterId)
    {
    case CharacterIdWheelchairGuy:
        characterName = "wheelchair_guy";
        if (vehicleId != VehicleIdDefault)
        {
            bareCharacter = true;
            break;
        }
        vehicleName = "wheelchair";
        {
            WheelchairGuy* wheelchairGuy = new WheelchairGuy();
            wheelchairGuy->init(Vec2(x, y), characterName, vehicleName, groupIndex, showGore);
            character = wheelchairGuy;
        }
        break;
    case CharacterIdBusinessGuy:
        characterName = "business_guy";
        if (vehicleId != VehicleIdDefault)
        {
            bareCharacter = true;
            break;
        }
        vehicleName = "personal_transporter";
        {
            BusinessGuy* businessGuy = new BusinessGuy();
            businessGuy->init(Vec2(x, y), characterName, vehicleName, groupIndex, showGore);
            character = businessGuy;
        }
        break;
    case CharacterIdIrresponsibleDad:
        characterName = "irresponsible_dad";
        if (vehicleId != VehicleIdDefault)
        {
            bareCharacter = true;
            break;
        }
        vehicleName = "road_bike";
        {
            IrresponsibleDad* irresponsibleDad = new IrresponsibleDad();
            irresponsibleDad->init(Vec2(x, y), characterName, vehicleName, groupIndex, showGore);
            character = irresponsibleDad;
        }
        break;
    case CharacterIdEffectiveShopper:
        characterName = "effective_shopper";
        if (vehicleId != VehicleIdDefault)
        {
            bareCharacter = true;
            break;
        }
        vehicleName = "motor_cart";
        {
            EffectiveShopper* effectiveShopper = new EffectiveShopper();
            effectiveShopper->init(Vec2(x, y), characterName, vehicleName, groupIndex, showGore);
            character = effectiveShopper;
        }
        break;
    case CharacterIdMopedCouple:
        characterName = "moped_guy";
        if (vehicleId != VehicleIdDefault)
        {
            bareCharacter = true;
            break;
        }
        vehicleName = "moped";
        {
            MopedCouple* mopedCouple = new MopedCouple();
            mopedCouple->init(Vec2(x, y), characterName, vehicleName, groupIndex, showGore);
            character = mopedCouple;
        }
        break;
    case CharacterIdPogostickGuy:
        characterName = "pogo_stick_guy";
        if (vehicleId != VehicleIdDefault)
        {
            bareCharacter = true;
            break;
        }
        vehicleName = "pogo_stick";
        {
            PogostickGuy* pogostickGuy = new PogostickGuy();
            pogostickGuy->init(Vec2(x, y), characterName, vehicleName, groupIndex, showGore);
            character = pogostickGuy;
        }
        break;
    default:
        // RESTORED (PC addition): browser characters rebuilt by tools/assets/extract_character.py.
        if (vehicleId == VehicleIdDefault)
        {
            character = restored::createCharacter(x, y, characterId, groupIndex, showGore);
        }
        break;
    }

    if (bareCharacter)
    {
        // Character without its vehicle class (vehicleName stays empty). Never reached on
        // Android: addInfo always passes VehicleIdDefault.
        character = new CharacterB2D();
        character->init(Vec2(x, y), characterName, "Char1", vehicleName, groupIndex, true, true);
    }
    return character;
}

// @005cf880
CharacterB2D* LevelB2D::getCharacter()
{
    if (_characters.size() != 0)
    {
        return _characters[0];
    }
    return nullptr;
}

// @005cf89c
float LevelB2D::convertYMetersToLevelMeters(float y)
{
    return y;
}

// @005cf8a0
void LevelB2D::addInfo(LevelDataElement* info)
{
    _sourcePtmRatio = 1.0f;
    _registration = false;
    _clockwise = false;
    _stageWidth = 320.0f;
    _stageHeight = 160.0f;
    info->boolAttribute("cw", &_clockwise);
    info->boolAttribute("r", &_registration);
    info->floatAttribute("ptm", &_sourcePtmRatio);
    info->floatAttribute("sw", &_stageWidth);
    info->floatAttribute("sh", &_stageHeight);
    _stageWidth = _stageWidth / _sourcePtmRatio;
    _stageHeight = _stageHeight / _sourcePtmRatio;

    float version = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    info->floatAttribute("v", &version);
    info->floatAttribute("x", &x);
    info->floatAttribute("y", &y);
    {
        // ONLINE (PC addition): converted browser levels carry src="flash" (and the browser
        // version in "fv"); every other level switches the browser-only behaviour off.
        const char* source = info->stringAttribute("src");
        float browserVersion = version;
        info->floatAttribute("fv", &browserVersion);
        online::setFlashLevel(source != nullptr && std::string(source) == "flash", browserVersion);
    }

    _forcedChar = false;
    bool hideVehicle = false;
    int vehicle = 0;
    int characterId = 1;
    _version = version;
    // EDITOR (iOS port): iOS flips y only for registration (r="1") levels; editor levels (no r) are y-up metres.
    y = _registration ? _stageHeight - y / _sourcePtmRatio : y / _sourcePtmRatio;
    x = x / _sourcePtmRatio;
    info->boolAttribute("f", &_forcedChar);
    info->boolAttribute("h", &hideVehicle);
    info->intAttribute("c", &characterId);
    info->intAttribute("ve", &vehicle);  // read but unused

    Settings* settings = Settings::getInstance();
    if (_forcedChar)
    {
        // QOL (PC addition): keep the player's own choice for later; a character picked for this
        // user level ("any character") replaces the level's (qol/CharacterChoice.h).
        qol::rememberPlayerCharacter();
        characterId = qol::forcedLevelCharacter(characterId);
        settings->setSelectedCharacterId(characterId);
    }
    else
    {
        characterId = settings->getSelectedCharacterId();
    }
    if (online::flashLevel() && hideVehicle)
    {
        // ONLINE (PC addition): browser "hide vehicle" levels start with the bare character.
        _characters.push_back(online::createBareCharacter(x, y, characterId));
    }
    else
    addCharacter(x, y, (CharacterId)characterId, VehicleIdDefault, hideVehicle, -1);

    // ONLINE (PC addition): browser levels step once per Flash frame (1/30 s) with the browser
    // physics profile; the characters exist now, so they rescale with the step (FlashPhysics.h).
    online::beginLevelTimeStep(Settings::getInstance()->getCurrentSession());

    int background = 0;
    int backgroundColor = 0;
    info->intAttribute("bg", &background);
    info->intAttribute("bgc", &backgroundColor);

    float stageWidth = 20000.0f;
    float stageHeight = 10000.0f;
    if (info->floatAttribute("sw", &stageWidth) && info->floatAttribute("sh", &stageHeight))
    {
        _stageWidth = stageWidth / _sourcePtmRatio;
        _stageHeight = stageHeight / _sourcePtmRatio;
    }

    BackgroundLayer* backgroundLayer =
        Settings::getInstance()->getCurrentSession()->getBackgroundLayer();
    if (backgroundLayer != nullptr)
    {
        backgroundLayer->init(Size(stageWidth * _ptmRatio, _ptmRatio * stageHeight),
                              (backgrounds)background, (long)backgroundColor, _ptmRatio, info);
    }
}

// @005cfec8
void LevelB2D::convertLengthData(float* length)
{
    *length = *length / _sourcePtmRatio;
}

// @005cfedc
void LevelB2D::convertYMeterPositionData(float* y)
{
    // EDITOR (iOS port): iOS flips y only for registration (r="1") levels; editor levels (no r) are y-up metres.
    if (_registration)
    {
        *y = _stageHeight - *y;
    }
}

// @005cfef0
void LevelB2D::addSpecial(LevelDataElement* special, int index)
{
    addSpecial(special, index, nullptr, b2Vec2(0.0f, 0.0f));
}

// @005cff00
LevelItem* LevelB2D::addSpecial(LevelDataElement* special, int index, b2Body* groupBody,
                                b2Vec2 groupOffset)
{
    LevelItem* item = nullptr;
    bool autoreleased = false;  // created by a static create() (init + autorelease done)

    int type = special->getType();
    // ONLINE (PC addition): in converted browser levels a few mobile classes are replaced by
    // full ports of their browser versions (e.g. the Android Chain/Token are stubs).
    if (online::flashLevel())
    {
        item = online::createFlashSpecialOverride(type, groupBody != nullptr);
    }
    if (item == nullptr)
    switch (type)
    {
    case 0:
        item = new (std::nothrow) Van();
        break;
    case 2:
        item = new (std::nothrow) Mine();
        break;
    case 3:
        item = IBeam::create(special, groupBody, groupOffset);
        autoreleased = true;
        break;
    case 4:
        item = new (std::nothrow) Log();
        break;
    case 5:
        item = new (std::nothrow) SpringBox();
        break;
    case 6:
        item = Spikes::create(special, groupBody, groupOffset);
        autoreleased = true;
        break;
    case 7:
        item = new (std::nothrow) WreckingBall();
        break;
    case 8:
        item = new (std::nothrow) Fan();
        break;
    case 9:
        item = FinishLine::create(special, groupBody, groupOffset);
        autoreleased = true;
        break;
    case 10:
        item = new (std::nothrow) SoccerBall();
        break;
    case 12:
        item = new (std::nothrow) BoostPanel();
        break;
    case 15:
        item = HarpoonGun::create(special, groupBody, groupOffset);
        autoreleased = true;
        break;
    case 20:
        item = new (std::nothrow) Bottle();
        break;
    case 23:
        item = new (std::nothrow) Sign();
        break;
    case 25:
        item = new (std::nothrow) HomingMine();
        break;
    case 28:
        item = new (std::nothrow) Jet();
        break;
    case 29:
        item = new (std::nothrow) ArrowGun();
        break;
    case 30:
        item = new (std::nothrow) Chain();
        break;
    case 31:
        item = new (std::nothrow) Token();
        break;
    case 34:
        item = new (std::nothrow) BladeWeapon();
        break;
    case 1:
    case 11:
    case 13:
    case 14:
    case 16:
    case 17:
    case 18:
    case 19:
    case 21:
    case 22:
    case 24:
    case 26:
    case 27:
    case 32:
    case 33:
        // ONLINE (PC addition): browser-only items (text boxes, NPCs, furniture...) exist in
        // converted browser levels; mobile levels never reach the factory.
        if (online::flashLevel() && (item = online::createFlashSpecial(type)) != nullptr)
        {
            break;
        }
        return nullptr;
    default:
        if (online::flashLevel() && (item = online::createFlashSpecial(type)) != nullptr)
        {
            break;  // ONLINE (PC addition): see above
        }
        if (type != 5001)
        {
            return nullptr;
        }
        item = new (std::nothrow) SlowMotionPanel();
        break;
    }

    if (item == nullptr)
    {
        return nullptr;
    }
    if (!autoreleased)
    {
        if (!item->init(special, groupBody, groupOffset))
        {
            delete item;
            return nullptr;
        }
        item->autorelease();
    }
    item->retain();
    item->setIndex(index);
    _specials.push_back(item);
    return item;
}

// @005d0b18
void LevelB2D::setShapeFilter(int collision, bool immovable, b2FixtureDef* fixtureDef)
{
    fixtureDef->friction = 1.0f;
    fixtureDef->restitution = 0.1f;
    switch (collision)
    {
    case 2:
        fixtureDef->filter.categoryBits = 0x8;
        fixtureDef->filter.maskBits = 0x8;
        if (_version > 1.84 && immovable)
        {
            fixtureDef->filter.categoryBits = 0x18;
            fixtureDef->filter.maskBits = 0x38;
        }
        break;
    case 3:
        if (_version >= 1.85f)
        {
            fixtureDef->filter.categoryBits = 0;
            fixtureDef->filter.maskBits = 0;
        }
        else
        {
            fixtureDef->filter.categoryBits = 0x1;
            fixtureDef->filter.maskBits = 0x1;
            fixtureDef->filter.groupIndex = -10;
            fixtureDef->isSensor = true;
        }
        break;
    case 4:
        fixtureDef->filter.groupIndex = -321;
        fixtureDef->filter.categoryBits = 0x8;
        if (_version > 1.84f && immovable)
        {
            fixtureDef->filter.categoryBits = 0x18;
        }
        break;
    case 5:
        fixtureDef->filter.categoryBits = 0x10;
        fixtureDef->filter.maskBits = 0x10;
        fixtureDef->filter.groupIndex = -322;
        if (_version > 1.84f && immovable)
        {
            fixtureDef->filter.maskBits = 0x30;
            fixtureDef->filter.groupIndex = 0;
        }
        break;
    case 6:
        if (_version > 1.84f)
        {
            fixtureDef->filter.maskBits = 0x30;
            fixtureDef->filter.categoryBits = immovable ? 0x30 : 0x20;
        }
        else
        {
            fixtureDef->filter.categoryBits = 0x10;
            fixtureDef->filter.maskBits = 0x10;
        }
        break;
    case 7:
        fixtureDef->filter.categoryBits = 0xf;
        fixtureDef->filter.maskBits = 0xf00;
        break;
    default:
        if (immovable)
        {
            fixtureDef->filter.categoryBits = 0x18;
            fixtureDef->filter.groupIndex = -10;
        }
        else
        {
            fixtureDef->filter.categoryBits = 0x8;
        }
        break;
    }
}

// @005d0c98
void LevelB2D::addShape(LevelDataElement* shape, int index)
{
    addShape(shape, nullptr, Vec2::ZERO, index, false);
}

namespace {

// Affine point transform in float arithmetic, written out at every use in the original (cocos2d's
// PointApplyAffineTransform works in double and is not used here).
inline b2Vec2 transformPoint(const AffineTransform& t, float x, float y)
{
    return b2Vec2(t.a * x + t.c * y + t.tx, t.b * x + t.d * y + t.ty);
}

// ONLINE (PC addition): puts an FFDrawNode back to the original (campaign) shape drawing when
// LevelB2D::addShape returns.
struct OnlineShapeStyleScope
{
    FFDrawNode* node = nullptr;
    ~OnlineShapeStyleScope()
    {
        if (node != nullptr)
        {
            node->onlineClearShapeStyle();
        }
    }
};

// ONLINE (PC addition): stroke width (points) of browser-level shape outlines. Flash strokes
// shapes with a 1 px line at the browser game's 1:1 view; the gameplay view shows as many Flash
// px as the browser stage (2000 points tall, 4 points per Flash px), so that is one Flash px
// here, kept at least ~1.25 framebuffer pixels thick when the view is small or zoomed out.
float onlineOutlineWidth(Node* drawNode, float ptmRatio, float sourcePtmRatio)
{
    const float width = sourcePtmRatio > 0.0f ? ptmRatio / sourcePtmRatio : 1.0f;
    float scale = 1.0f;
    for (Node* node = drawNode; node != nullptr; node = node->getParent())
    {
        scale *= std::fabs(node->getScaleX());
    }
    GLView* view = Director::getInstance()->getOpenGLView();
    const float pixelsPerPoint = (view != nullptr ? view->getScaleX() : 1.0f) * scale;
    if (pixelsPerPoint > 0.0f)
    {
        return std::max(width, 1.25f / pixelsPerPoint);
    }
    return width;
}

}  // namespace

// @005d0cb4
// Shape "t": 0 rectangle, 1 circle, 2 triangle, 3 polygon, 4 art polygon (no physics), 5 terrain.
// Attributes: p0/p1 position, p2/p3 size, p4 rotation (degrees, clockwise), p5 immovable,
// i interactive (has a fixture), p6 sleeping, p7 density, p8 fill RGB, p9 outline RGB (-1: none),
// p10 opacity (%), p11 collision filter. Immovable (and free non-interactive) shapes are fixtures of
// the level body; other shapes get their own dynamic body, or join the group's body.
// Returns the ShapeItem when it is owned by the level (_shapeItems); group shapes return nullptr.
ShapeItem* LevelB2D::addShape(LevelDataElement* shape, GroupItem* groupItem, Vec2 groupOffset,
                              unsigned int index, bool foreground)
{
    Session* session = Settings::getInstance()->getCurrentSession();
    b2Body* levelBody = session->getLevelBody();
    b2World* world = session->getWorld();

    int type = 0;
    shape->intAttribute("t", &type);
    FFDrawNode* drawNode =
        foreground ? session->getForegroundShapesNode() : session->getShapesNode();

    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float rotation = 0.0f;
    float density = 0.0f;
    float opacity = 1.0f;
    bool immovable = false;
    bool interactive = true;
    bool sleeping = false;
    int fillRGB = 0;
    int outlineRGB = -1;
    int collision = 0;
    shape->floatAttribute("p0", &x);
    shape->floatAttribute("p1", &y);
    shape->floatAttribute("p2", &width);
    shape->floatAttribute("p3", &height);
    shape->floatAttribute("p4", &rotation);
    x = x / _sourcePtmRatio;
    // EDITOR (iOS port): iOS flips y only for registration (r="1") levels; editor levels (no r) are y-up metres.
    y = _registration ? _stageHeight - y / _sourcePtmRatio : y / _sourcePtmRatio;
    width = width / _sourcePtmRatio;
    height = height / _sourcePtmRatio;
    shape->floatAttribute("p7", &density);
    shape->floatAttribute("p10", &opacity);
    opacity = opacity / 100.0f;  // absent => 0.01
    shape->boolAttribute("p5", &immovable);
    shape->boolAttribute("i", &interactive);
    shape->boolAttribute("p6", &sleeping);
    if (!interactive)
    {
        immovable = false;
    }
    shape->intAttribute("p8", &fillRGB);
    shape->intAttribute("p9", &outlineRGB);
    shape->intAttribute("p11", &collision);

    Color4F fillColor = ccColorFromRGB(fillRGB);
    fillColor.a = opacity;
    Color4F outlineColor;
    if (outlineRGB < 0)
    {
        outlineColor = Color4F::WHITE;
    }
    else
    {
        outlineColor = ccColorFromRGB(outlineRGB);
    }
    float borderWidth = (outlineRGB >= 0) ? 1.0f : 0.0f;

    // ONLINE (PC addition): browser levels draw Flash's outlines (p9, which the original ignores),
    // outline-only shapes (p8 -1 from FlashLevelConverter) and circle cutouts (p12, % of the
    // radius); see FFDrawNode::onlineSetShapeStyle.
    OnlineShapeStyleScope onlineStyle;
    if (online::flashLevel() && type != 5)
    {
        float innerCutout = 0.0f;
        if (type == 1)
        {
            shape->floatAttribute("p12", &innerCutout);
        }
        float outlineWidth = 0.0f;
        if (outlineRGB >= 0)
        {
            outlineWidth = onlineOutlineWidth(drawNode, _ptmRatio, _sourcePtmRatio);
        }
        drawNode->onlineSetShapeStyle(fillRGB >= 0, outlineWidth, innerCutout / 100.0f);
        onlineStyle.node = drawNode;
    }

    b2FixtureDef fixtureDef;
    if (groupItem == nullptr)
    {
        setShapeFilter(collision, immovable, &fixtureDef);
    }
    else
    {
        bool filterImmovable = groupItem->getImmovable();
        bool shapeImmovable = false;
        if (online::flashLevel() && shape->boolAttribute("fim", &shapeImmovable))
        {
            filterImmovable = shapeImmovable;  // ONLINE (PC addition): Flash <= 1.84 group shapes
        }
        setShapeFilter(collision, filterImmovable, &fixtureDef);
    }
    float angle = rotation * -0.017453292f;

    ShapeItem* shapeItem = nullptr;
    if (immovable || (groupItem == nullptr && !interactive))
    {
        // Static shape: a fixture of the level body.
        fixtureDef.friction = 1.0f;
        fixtureDef.restitution = 0.1f;
        fixtureDef.density = density;
        switch (type)
        {
        case 0:
        {
            b2Fixture* fixture = nullptr;
            if (interactive)
            {
                b2PolygonShape box;
                box.SetAsBox(width * 0.5f, height * 0.5f, b2Vec2(x, y), angle);
                fixtureDef.shape = &box;
                fixture = levelBody->CreateFixture(&fixtureDef);
            }
            RectangleShape* rectangle = new (std::nothrow) RectangleShape();
            rectangle->init(Vec2(x * _ptmRatio, _ptmRatio * y), angle, AffineTransformIdentity,
                            fillColor, outlineColor, opacity, borderWidth, _ptmRatio * width,
                            _ptmRatio * height, drawNode, false);
            rectangle->setFixtureRef(fixture);
            rectangle->setDelegate(this);
            rectangle->setIndex(index);
            _shapeItems.push_back(rectangle);
            shapeItem = rectangle;
            break;
        }
        case 1:
        {
            float radius = width * 0.5f;
            b2Fixture* fixture = nullptr;
            if (interactive)
            {
                b2CircleShape circle;
                circle.m_radius = radius;
                circle.m_p.Set(x, y);
                fixtureDef.shape = &circle;
                fixture = levelBody->CreateFixture(&fixtureDef);
            }
            CircleShape* circleShape = new (std::nothrow) CircleShape();
            circleShape->init(Vec2(x * _ptmRatio, _ptmRatio * y), Vec2::ZERO, radius * _ptmRatio,
                              fillColor, outlineColor, opacity, borderWidth, drawNode, false);
            circleShape->setIndex(index);
            circleShape->setDelegate(this);
            circleShape->setFixtureRef(fixture);
            _shapeItems.push_back(circleShape);
            shapeItem = circleShape;
            break;
        }
        case 2:
        {
            b2Fixture* fixture = nullptr;
            if (interactive)
            {
                // Base centred on the position, apex up; the position is the centroid.
                float thirdHeight = height / 3.0f;
                float halfWidth = width * 0.5f;
                float twoThirdsHeight = thirdHeight + thirdHeight;
                AffineTransform transform = AffineTransformRotate(
                    AffineTransformTranslate(AffineTransformIdentity, x, y), angle);
                b2PolygonShape triangle;
                b2Vec2 verts[3];
                verts[0] = transformPoint(transform, -halfWidth, -thirdHeight);
                verts[1] = transformPoint(transform, halfWidth, -thirdHeight);
                verts[2] = transformPoint(transform, 0.0f, twoThirdsHeight);
                triangle.Set(verts, 3);
                fixtureDef.shape = &triangle;
                fixture = levelBody->CreateFixture(&fixtureDef);
            }
            TriangleShape* triangleShape = new (std::nothrow) TriangleShape();
            triangleShape->init(Vec2(x * _ptmRatio, _ptmRatio * y), angle, AffineTransformIdentity,
                                fillColor, outlineColor, opacity, borderWidth, _ptmRatio * width,
                                _ptmRatio * height, drawNode, false);
            triangleShape->setIndex(index);
            triangleShape->setFixtureRef(fixture);
            triangleShape->setDelegate(this);
            _shapeItems.push_back(triangleShape);
            shapeItem = triangleShape;
            break;
        }
        case 3:
        {
            // <v n="count" id="cache id" v0="x_y" ...>; a later shape may give only the id.
            LevelDataElement* vertsElement = new (std::nothrow) LevelDataElement;
            vertsElement->init(shape->getData()->FirstChildElement());
            int count = 0;
            bool hasCount = vertsElement->intAttribute("n", &count);
            int id = -1;
            vertsElement->intAttribute("id", &id);
            Vec2 verts[10];
            if (!hasCount)
            {
                std::vector<Vec2> cached = _polygonVerts[id];
                count = (int)cached.size();
                for (int i = 0; i < count; i++)
                {
                    verts[i] = cached[i];
                }
            }
            else
            {
                if (count <= 10)
                {
                    for (int i = 0; i < count; i++)
                    {
                        verts[i] =
                            stringToVec(vertsElement->stringAttribute("v" + std::to_string(i)));
                    }
                }
                convertVerts(verts, count);
                std::vector<Vec2> list;
                for (int i = 0; i < count; i++)
                {
                    list.push_back(verts[i]);
                }
                _polygonVerts[id] = list;
            }
            delete vertsElement;

            // The vertices are stretched to the shape's width/height.
            float maxX = 0.0f;
            float minX = 0.0f;
            float maxY = 0.0f;
            float minY = 0.0f;
            for (unsigned int i = 0; i < count; i++)
            {
                if (verts[i].x > maxX)
                {
                    maxX = verts[i].x;
                }
                else if (verts[i].x < minX)
                {
                    minX = verts[i].x;
                }
                if (verts[i].y > maxY)
                {
                    maxY = verts[i].y;
                }
                else if (verts[i].y < minY)
                {
                    minY = verts[i].y;
                }
            }
            float scaleX = (width == 0.0f) ? 1.0f : width / (maxX - minX);
            float scaleY = (height == 0.0f) ? 1.0f : height / (maxY - minY);

            AffineTransform transform = AffineTransformTranslate(AffineTransformIdentity, x, y);
            b2Fixture* fixture = nullptr;
            if (interactive)
            {
                transform = AffineTransformRotate(transform, angle);
                b2PolygonShape polygon;
                b2Vec2 physicsVerts[10];
                for (unsigned int i = 0; i < count; i++)
                {
                    physicsVerts[i] =
                        transformPoint(transform, verts[i].x * scaleX, verts[i].y * scaleY);
                }
                polygon.Set(physicsVerts, count);
                fixtureDef.shape = &polygon;
                fixture = levelBody->CreateFixture(&fixtureDef);
            }
            for (unsigned int i = 0; i < count; i++)
            {
                verts[i].x *= scaleX * _ptmRatio;
                verts[i].y *= scaleY * _ptmRatio;
            }
            AffineTransform artTransform = AffineTransformRotate(
                AffineTransformTranslate(AffineTransformIdentity, x * _ptmRatio, _ptmRatio * y),
                angle);
            // ONLINE (PC addition): drawn from the browser polygon's own outline when it has one.
            Vec2 onlineVerts[100];
            int onlineCount =
                online::flashLevel() ? onlinePolygonArtVerts(shape, scaleX, scaleY, onlineVerts) : 0;
            PolygonShape* polygonShape = new (std::nothrow) PolygonShape();
            // FIX (reconstruction): a static polygon's vertices are local to `pos`, which
            // PolygonShape::init already applies as the art transform; passing artTransform as
            // the initial transform too (as this line did) placed it twice as far from the
            // origin, i.e. off-screen - every static polygon and art shape of a level was
            // invisible. Rectangles and triangles pass identity here.
            (void)artTransform;
            polygonShape->init(Vec2(x * _ptmRatio, _ptmRatio * y), angle,
                               onlineCount > 0 ? onlineVerts : verts,
                               onlineCount > 0 ? onlineCount : count, AffineTransformIdentity,
                               fillColor, outlineColor, opacity, borderWidth, drawNode, false);
            polygonShape->setIndex(index);
            polygonShape->setDelegate(this);
            polygonShape->setFixtureRef(fixture);
            _shapeItems.push_back(polygonShape);
            shapeItem = polygonShape;
            break;
        }
        case 4:
        {
            // Art polygon: like case 3 (up to 100 vertices, own cache) but never has a fixture.
            LevelDataElement* vertsElement = new (std::nothrow) LevelDataElement;
            vertsElement->init(shape->getData()->FirstChildElement());
            int count = 0;
            bool hasCount = vertsElement->intAttribute("n", &count);
            int id = -1;
            vertsElement->intAttribute("id", &id);
            Vec2 verts[100];
            if (!hasCount)
            {
                std::vector<Vec2> cached = _artVerts[id];
                count = (int)cached.size();
                for (int i = 0; i < count; i++)
                {
                    verts[i] = cached[i];
                }
            }
            else
            {
                if (count <= 100)
                {
                    for (int i = 0; i < count; i++)
                    {
                        verts[i] =
                            stringToVec(vertsElement->stringAttribute("v" + std::to_string(i)));
                    }
                }
                convertVerts(verts, count);
                std::vector<Vec2> list;
                for (int i = 0; i < count; i++)
                {
                    list.push_back(verts[i]);
                }
                _artVerts[id] = list;
            }
            delete vertsElement;

            float maxX = 0.0f;
            float minX = 0.0f;
            float maxY = 0.0f;
            float minY = 0.0f;
            for (unsigned int i = 0; i < count; i++)
            {
                if (verts[i].x > maxX)
                {
                    maxX = verts[i].x;
                }
                else if (verts[i].x < minX)
                {
                    minX = verts[i].x;
                }
                if (verts[i].y > maxY)
                {
                    maxY = verts[i].y;
                }
                else if (verts[i].y < minY)
                {
                    minY = verts[i].y;
                }
            }
            float scaleX = (width == 0.0f) ? 1.0f : width / (maxX - minX);
            float scaleY = (height == 0.0f) ? 1.0f : height / (maxY - minY);
            for (unsigned int i = 0; i < count; i++)
            {
                verts[i].x *= scaleX * _ptmRatio;
                verts[i].y *= scaleY * _ptmRatio;
            }
            AffineTransform artTransform = AffineTransformRotate(
                AffineTransformTranslate(AffineTransformIdentity, x * _ptmRatio, _ptmRatio * y),
                angle);
            PolygonShape* polygonShape = new (std::nothrow) PolygonShape();
            // FIX (reconstruction): identity initial transform, see the polygon case above.
            (void)artTransform;
            polygonShape->init(Vec2(x * _ptmRatio, _ptmRatio * y), angle, verts, count,
                               AffineTransformIdentity, fillColor, outlineColor, opacity,
                               borderWidth, drawNode, false);
            polygonShape->setIndex(index);
            polygonShape->setDelegate(this);
            polygonShape->setFixtureRef(nullptr);
            _shapeItems.push_back(polygonShape);
            shapeItem = polygonShape;
            break;
        }
        case 5:
        {
            // Terrain: closed outline of straight and cubic bezier segments, a b2ChainShape loop
            // on the level body, drawn into the session's TerrainNode.
            LevelDataElement* vertsElement = new (std::nothrow) LevelDataElement;
            vertsElement->init(shape->getData()->FirstChildElement());
            int count = 0;
            bool hasCount = vertsElement->intAttribute("n", &count);
            int id = -1;
            vertsElement->intAttribute("id", &id);
            TerrainVert verts[2000];
            if (!hasCount)
            {
                std::vector<TerrainVert> cached = _terrainVerts[id];
                count = (int)cached.size();
                for (int i = 0; i < count; i++)
                {
                    verts[i] = cached[i];
                }
            }
            else
            {
                if (count <= 2000)
                {
                    for (int i = 0; i < count; i++)
                    {
                        verts[i] = stringToTerrainVert(
                            vertsElement->stringAttribute("v" + std::to_string(i)));
                    }
                }
                convertTerrainVerts(verts, count);
                std::vector<TerrainVert> list;
                for (int i = 0; i < count; i++)
                {
                    list.push_back(verts[i]);
                }
                _terrainVerts[id] = list;
            }
            delete vertsElement;

            b2ChainShape chain;
            b2Vec2 chainVerts[2000];
            unsigned int chainCount = 0;
            for (unsigned int i = 0; i < count; i++)
            {
                TerrainVert& vert = verts[i];
                TerrainVert& next = (i == count - 1) ? verts[0] : verts[i + 1];
                if (vert.active)
                {
                    chainVerts[chainCount++] = b2Vec2(vert.point.x + x, vert.point.y + y);
                    if (vert.controlPointOut.x != 0.0f || vert.controlPointOut.y != 0.0f ||
                        next.controlPointIn.x != 0.0f || next.controlPointIn.y != 0.0f)
                    {
                        for (unsigned int j = 1; j < vert.segments; j++)
                        {
                            float t = (float)j / (float)vert.segments;
                            float bezierX = calculateBezier(
                                t, vert.point.x, vert.point.x + vert.controlPointOut.x,
                                next.point.x + next.controlPointIn.x, next.point.x);
                            float bezierY = calculateBezier(
                                t, vert.point.y, vert.point.y + vert.controlPointOut.y,
                                next.point.y + next.controlPointIn.y, next.point.y);
                            chainVerts[chainCount++] = b2Vec2(x + bezierX, y + bezierY);
                        }
                    }
                }
            }
            // Drop points closer than b2_linearSlop to their predecessor (CreateLoop's limit).
            unsigned int removed = 0;
            if (chainCount >= 2)
            {
                for (unsigned int k = 0; k < chainCount - 1; k++)
                {
                    b2Vec2 point = chainVerts[k + 1];
                    if (b2DistanceSquared(chainVerts[(int)(k - removed)], point) <
                        b2_linearSlop * b2_linearSlop)
                    {
                        removed++;
                    }
                    chainVerts[(int)(k - removed + 1)] = point;
                }
            }
            unsigned int loopCount = chainCount - removed;
            chain.CreateLoop(chainVerts, loopCount);
            fixtureDef.shape = &chain;
            b2Fixture* fixture = levelBody->CreateFixture(&fixtureDef);
            levelBody->ResetMassData();

            Vec2 artVerts[2000];
            for (unsigned int i = 0; i < loopCount; i++)
            {
                artVerts[i] = Vec2(chainVerts[i].x * _ptmRatio, chainVerts[i].y * _ptmRatio);
            }
            session->getTerrainNode()->drawPolyWithVerts(artVerts, loopCount, fillColor, 0.0,
                                                         Color4F(0.0f, 0.0f, 0.0f, 0.0f));
            TerrainShape* terrainShape = new (std::nothrow) TerrainShape();
            terrainShape->init(fixture);
            terrainShape->setIndex(index);
            terrainShape->setDelegate(this);
            _shapeItems.push_back(terrainShape);
            shapeItem = terrainShape;
            break;
        }
        default:
            break;
        }
        return shapeItem;
    }

    // Dynamic shape: its own body, or the group's body.
    b2Body* body;
    if (groupItem == nullptr)
    {
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position.Set(x, y);
        bodyDef.angle = angle;
        bodyDef.awake = !sleeping;
        bool nanMass = false;
        if (online::flashLevel() && shape->boolAttribute("nm", &nanMass) && nanMass)
        {
            bodyDef.type = b2_staticBody;  // ONLINE (PC addition): see onlineNanMassBodies
        }
        body = world->CreateBody(&bodyDef);
        if (nanMass)
        {
            onlineNanMassBodies.insert(body);
        }
    }
    else
    {
        body = groupItem->getBody();
    }
    fixtureDef.friction = 1.0f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = density;

    switch (type)
    {
    case 0:
    {
        b2PolygonShape box;
        if (groupItem == nullptr)
        {
            box.SetAsBox(width * 0.5f, height * 0.5f);
        }
        else
        {
            box.SetAsBox(width * 0.5f, height * 0.5f,
                         b2Vec2(groupOffset.x + x, groupOffset.y + y), angle);
        }
        fixtureDef.shape = &box;
        b2Fixture* fixture = nullptr;
        if (interactive)
        {
            fixture = body->CreateFixture(&fixtureDef);
        }
        if (groupItem == nullptr)
        {
            RectangleShape* rectangle = new (std::nothrow) RectangleShape();
            rectangle->init(fixture, _ptmRatio, fillColor, outlineColor, opacity, borderWidth,
                            _ptmRatio * width, _ptmRatio * height, drawNode);
            rectangle->setDelegate(this);
            rectangle->setIndex(index);
            _shapeItems.push_back(rectangle);
            return rectangle;
        }
        AffineTransform artTransform =
            AffineTransformRotate(AffineTransformTranslate(AffineTransformIdentity,
                                                           (groupOffset.x + x) * _ptmRatio,
                                                           _ptmRatio * (groupOffset.y + y)),
                                  angle);
        RectangleShape* rectangle = new (std::nothrow) RectangleShape();
        rectangle->init(Vec2::ZERO, angle, artTransform, fillColor, outlineColor, opacity,
                        borderWidth, width * _ptmRatio, _ptmRatio * height, drawNode, true);
        rectangle->setIndex(index);
        groupItem->addShapeItem(rectangle);
        break;
    }
    case 1:
    {
        float radius = width * 0.5f;
        b2Fixture* fixture = nullptr;
        if (interactive)
        {
            b2CircleShape circle;
            if (groupItem != nullptr)
            {
                circle.m_p.Set(groupOffset.x + x, groupOffset.y + y);
            }
            circle.m_radius = radius;
            fixtureDef.shape = &circle;
            fixture = body->CreateFixture(&fixtureDef);
        }
        if (groupItem == nullptr)
        {
            CircleShape* circleShape = new (std::nothrow) CircleShape();
            circleShape->init(fixture, _ptmRatio, fillColor, outlineColor, opacity, borderWidth,
                              radius * _ptmRatio, drawNode);
            circleShape->setDelegate(this);
            circleShape->setIndex(index);
            _shapeItems.push_back(circleShape);
            return circleShape;
        }
        CircleShape* circleShape = new (std::nothrow) CircleShape();
        circleShape->init(Vec2::ZERO,
                          Vec2(x * _ptmRatio + groupOffset.x * _ptmRatio,
                               y * _ptmRatio + groupOffset.y * _ptmRatio),
                          radius * _ptmRatio, fillColor, outlineColor, opacity, borderWidth,
                          drawNode, true);
        circleShape->setIndex(index);
        groupItem->addShapeItem(circleShape);
        break;
    }
    case 2:
    {
        float thirdHeight = height / 3.0f;
        float halfWidth = width * 0.5f;
        b2PolygonShape triangle;
        b2Vec2 verts[3];
        verts[0].Set(-halfWidth, -thirdHeight);
        verts[1].Set(halfWidth, -thirdHeight);
        verts[2].Set(0.0f, thirdHeight + thirdHeight);
        if (groupItem != nullptr)
        {
            AffineTransform transform =
                AffineTransformRotate(AffineTransformIdentity, rotation * -0.017453292f);
            for (int i = 0; i < 3; i++)
            {
                b2Vec2 point = transformPoint(transform, verts[i].x, verts[i].y);
                verts[i].Set(point.x + x + groupOffset.x, point.y + y + groupOffset.y);
            }
        }
        triangle.Set(verts, 3);
        fixtureDef.shape = &triangle;
        b2Fixture* fixture = nullptr;
        if (interactive)
        {
            fixture = body->CreateFixture(&fixtureDef);
        }
        if (groupItem == nullptr)
        {
            TriangleShape* triangleShape = new (std::nothrow) TriangleShape();
            triangleShape->init(fixture, _ptmRatio, fillColor, outlineColor, opacity, borderWidth,
                                _ptmRatio * width, _ptmRatio * height, drawNode);
            triangleShape->setDelegate(this);
            triangleShape->setIndex(index);
            _shapeItems.push_back(triangleShape);
            return triangleShape;
        }
        AffineTransform artTransform =
            AffineTransformRotate(AffineTransformTranslate(AffineTransformIdentity,
                                                           (groupOffset.x + x) * _ptmRatio,
                                                           _ptmRatio * (groupOffset.y + y)),
                                  angle);
        TriangleShape* triangleShape = new (std::nothrow) TriangleShape();
        // Unlike the rectangle, the triangle passes rotation 0 here.
        triangleShape->init(Vec2::ZERO, 0.0f, artTransform, fillColor, outlineColor, opacity,
                            borderWidth, width * _ptmRatio, _ptmRatio * height, drawNode, true);
        triangleShape->setIndex(index);
        groupItem->addShapeItem(triangleShape);
        break;
    }
    case 3:
    {
        LevelDataElement* vertsElement = new (std::nothrow) LevelDataElement;
        vertsElement->init(shape->getData()->FirstChildElement());
        AffineTransform transform = AffineTransformIdentity;
        if (groupItem != nullptr)
        {
            transform = AffineTransformRotate(
                AffineTransformTranslate(AffineTransformIdentity, groupOffset.x + x,
                                         groupOffset.y + y),
                angle);
        }
        b2PolygonShape polygon;
        int count = 0;
        Vec2 verts[10];
        bool hasCount = vertsElement->intAttribute("n", &count);
        int id = -1;
        vertsElement->intAttribute("id", &id);
        if (!hasCount)
        {
            std::vector<Vec2> cached = _polygonVerts[id];
            count = (int)cached.size();
            for (int i = 0; i < count; i++)
            {
                verts[i] = cached[i];
            }
        }
        else
        {
            if (count <= 10)
            {
                for (int i = 0; i < count; i++)
                {
                    verts[i] = stringToVec(vertsElement->stringAttribute("v" + std::to_string(i)));
                }
            }
            convertVerts(verts, count);
            std::vector<Vec2> list;
            for (int i = 0; i < count; i++)
            {
                list.push_back(verts[i]);
            }
            _polygonVerts[id] = list;
        }
        delete vertsElement;

        float maxX = 0.0f;
        float minX = 0.0f;
        float maxY = 0.0f;
        float minY = 0.0f;
        for (unsigned int i = 0; i < count; i++)
        {
            if (verts[i].x > maxX)
            {
                maxX = verts[i].x;
            }
            else if (verts[i].x < minX)
            {
                minX = verts[i].x;
            }
            if (verts[i].y > maxY)
            {
                maxY = verts[i].y;
            }
            else if (verts[i].y < minY)
            {
                minY = verts[i].y;
            }
        }
        float scaleX = 1.0f;
        if (width != 0.0f)
        {
            scaleX = width / (maxX - minX);
        }
        float scaleY = (height == 0.0f) ? 1.0f : height / (maxY - minY);

        b2Vec2 physicsVerts[10];
        for (unsigned int i = 0; i < count; i++)
        {
            verts[i].x = verts[i].x * scaleX;
            verts[i].y = verts[i].y * scaleY;
            physicsVerts[i] = transformPoint(transform, verts[i].x, verts[i].y);
            verts[i].x = verts[i].x * _ptmRatio;
            verts[i].y = verts[i].y * _ptmRatio;
        }
        b2Fixture* fixture = nullptr;
        if (interactive)
        {
            polygon.Set(physicsVerts, count);
            fixtureDef.shape = &polygon;
            fixture = body->CreateFixture(&fixtureDef);
        }
        // ONLINE (PC addition): browser polygons are drawn from their own outline (the converter's
        // <av>, else the vertices above), not from the fixture's convex hull.
        Vec2 onlineVerts[100];
        int onlineCount =
            online::flashLevel() ? onlinePolygonArtVerts(shape, scaleX, scaleY, onlineVerts) : 0;
        Vec2* drawVerts = onlineCount > 0 ? onlineVerts : verts;
        int drawCount = onlineCount > 0 ? onlineCount : count;
        if (groupItem == nullptr)
        {
            PolygonShape* polygonShape = new (std::nothrow) PolygonShape();
            if (online::flashLevel())
            {
                polygonShape->onlineInit(fixture, _ptmRatio, fillColor, outlineColor, opacity,
                                         borderWidth, drawVerts, drawCount, drawNode);
            }
            else
            {
                polygonShape->init(fixture, _ptmRatio, fillColor, outlineColor, opacity,
                                   borderWidth, drawNode);
            }
            polygonShape->setDelegate(this);
            polygonShape->setIndex(index);
            _shapeItems.push_back(polygonShape);
            return polygonShape;
        }
        AffineTransform artTransform =
            AffineTransformRotate(AffineTransformTranslate(AffineTransformMakeIdentity(),
                                                           (groupOffset.x + x) * _ptmRatio,
                                                           _ptmRatio * (groupOffset.y + y)),
                                  angle);
        PolygonShape* polygonShape = new (std::nothrow) PolygonShape();
        polygonShape->init(Vec2::ZERO, 0.0f, drawVerts, drawCount, artTransform, fillColor,
                           outlineColor, opacity, borderWidth, drawNode, true);
        polygonShape->setIndex(index);
        groupItem->addShapeItem(polygonShape);
        break;
    }
    case 4:
    {
        // RE-TODO(@005d0cb4): only the group path exists here; a non-interactive art shape always
        // takes the static branch, an interactive one without a group would crash as in the
        // original (groupItem->addShapeItem on nullptr).
        LevelDataElement* vertsElement = new (std::nothrow) LevelDataElement;
        vertsElement->init(shape->getData()->FirstChildElement());
        int count = 0;
        bool hasCount = vertsElement->intAttribute("n", &count);
        int id = -1;
        vertsElement->intAttribute("id", &id);
        Vec2 verts[100];
        if (!hasCount)
        {
            std::vector<Vec2> cached = _artVerts[id];
            count = (int)cached.size();
            for (int i = 0; i < count; i++)
            {
                verts[i] = cached[i];
            }
        }
        else
        {
            if (count <= 100)
            {
                for (int i = 0; i < count; i++)
                {
                    verts[i] = stringToVec(vertsElement->stringAttribute("v" + std::to_string(i)));
                }
            }
            convertVerts(verts, count);
            std::vector<Vec2> list;
            for (int i = 0; i < count; i++)
            {
                list.push_back(verts[i]);
            }
            _artVerts[id] = list;
        }
        delete vertsElement;

        float maxX = 0.0f;
        float minX = 0.0f;
        float maxY = 0.0f;
        float minY = 0.0f;
        for (unsigned int i = 0; i < count; i++)
        {
            if (verts[i].x > maxX)
            {
                maxX = verts[i].x;
            }
            else if (verts[i].x < minX)
            {
                minX = verts[i].x;
            }
            if (verts[i].y > maxY)
            {
                maxY = verts[i].y;
            }
            else if (verts[i].y < minY)
            {
                minY = verts[i].y;
            }
        }
        float scaleX = (width == 0.0f) ? 1.0f : width / (maxX - minX);
        float scaleY = (height == 0.0f) ? 1.0f : height / (maxY - minY);
        for (unsigned int i = 0; i < count; i++)
        {
            verts[i].x *= scaleX * _ptmRatio;
            verts[i].y *= scaleY * _ptmRatio;
        }
        AffineTransform artTransform =
            AffineTransformRotate(AffineTransformTranslate(AffineTransformIdentity,
                                                           (groupOffset.x + x) * _ptmRatio,
                                                           _ptmRatio * (groupOffset.y + y)),
                                  angle);
        PolygonShape* polygonShape = new (std::nothrow) PolygonShape();
        polygonShape->init(Vec2::ZERO, 0.0f, verts, count, artTransform, fillColor, outlineColor,
                           opacity, borderWidth, drawNode, true);
        polygonShape->setIndex(index);
        groupItem->addShapeItem(polygonShape);
        break;
    }
    default:
    {
        // Built and discarded (presumably a stripped log message).
        int unknownType = type + 6000;
        std::string message = "Unknown Shape Item ID: " + patch::to_string(unknownType);
        break;
    }
    }
    return nullptr;
}

// @005d5d70
void LevelB2D::addShapeItem(ShapeItem* shapeItem)
{
    _shapeItems.push_back(shapeItem);
}

// @005d5ee4
void LevelB2D::convertPositionAndRotationData(float* x, float* y, float* rotation)
{
    *x = *x / _sourcePtmRatio;
    *y = *y / _sourcePtmRatio;
    // EDITOR (iOS port): iOS flips y only for registration (r="1") levels; editor levels (no r) are y-up metres.
    if (_registration)
    {
        *y = _stageHeight - *y;
    }
}

// @005d5f14
Color4F LevelB2D::ccColorFromRGB(long rgb)
{
    return Color4F(((rgb >> 16) & 0xff) / 255.0f, ((rgb >> 8) & 0xff) / 255.0f,
                   (rgb & 0xff) / 255.0f, 1.0f);
}

// @005d62e8
Vec2 LevelB2D::stringToVec(const char* str)
{
    std::stringstream stream(str);
    std::string part;
    std::vector<std::string> parts;
    // Levels older than v1.84 separate the coordinates with '.', newer ones with '_'.
    char separator = (_version >= 1.84) ? '_' : '.';
    while (std::getline(stream, part, separator))
    {
        parts.push_back(part);
    }
    return Vec2(atoi(parts[0].c_str()), atoi(parts[1].c_str()));
}

// @005d65d0
void LevelB2D::convertVerts(Vec2* verts, int count)
{
    for (int i = 0; i < count; i++)
    {
        verts[i].x = verts[i].x / _sourcePtmRatio;
        verts[i].y = verts[i].y / _sourcePtmRatio;
        if (_clockwise)
        {
            verts[i].y = -verts[i].y;
        }
    }
}

// ONLINE (PC addition): see LevelB2D.h. <av n v0..> is written by FlashLevelConverter with the
// same integer scale as the shape's <v>, so the shape's stretch applies to it unchanged.
int LevelB2D::onlinePolygonArtVerts(LevelDataElement* shape, float scaleX, float scaleY,
                                    Vec2* verts)
{
    tinyxml2::XMLElement* art = shape->getData()->FirstChildElement("av");
    if (art == nullptr)
    {
        return 0;
    }
    int count = 0;
    art->QueryIntAttribute("n", &count);
    if (count < 3 || count > 100)
    {
        return 0;
    }
    const char separator = (_version >= 1.84) ? '_' : '.';
    for (int i = 0; i < count; i++)
    {
        const char* text = art->Attribute(("v" + std::to_string(i)).c_str());
        if (text == nullptr || strchr(text, separator) == nullptr)
        {
            return 0;
        }
        verts[i] = stringToVec(text);
    }
    convertVerts(verts, count);
    for (int i = 0; i < count; i++)
    {
        verts[i].x *= scaleX * _ptmRatio;
        verts[i].y *= scaleY * _ptmRatio;
    }
    return count;
}

// @005d6698
TerrainVert LevelB2D::stringToTerrainVert(const char* str)
{
    // "x_y_<t|f>_segments[_inX_inY_outX_outY]"
    std::stringstream stream(str);
    std::string part;
    std::vector<std::string> parts;
    while (std::getline(stream, part, '_'))
    {
        parts.push_back(part);
    }
    int x = atoi(parts[0].c_str());
    int y = atoi(parts[1].c_str());
    int segments = atoi(parts[3].c_str());
    float inX = 0.0f;
    float inY = 0.0f;
    float outX = 0.0f;
    float outY = 0.0f;
    if (parts.size() > 4)
    {
        inX = atoi(parts[4].c_str());
        inY = atoi(parts[5].c_str());
        outX = atoi(parts[6].c_str());
        outY = atoi(parts[7].c_str());
    }
    if (segments > 19)
    {
        segments = 20;
    }
    TerrainVert vert = {b2Vec2(x, y), b2Vec2(inX, inY), b2Vec2(outX, outY),
                        (unsigned int)segments, true};
    return vert;
}

// @005d6a50
void LevelB2D::convertTerrainVerts(TerrainVert* verts, int count)
{
    for (int i = 0; i < count; i++)
    {
        verts[i].point.x = verts[i].point.x / _sourcePtmRatio;
        verts[i].point.y = verts[i].point.y / _sourcePtmRatio;
        verts[i].controlPointIn.x = verts[i].controlPointIn.x / _sourcePtmRatio;
        verts[i].controlPointIn.y = verts[i].controlPointIn.y / _sourcePtmRatio;
        verts[i].controlPointOut.x = verts[i].controlPointOut.x / _sourcePtmRatio;
        verts[i].controlPointOut.y = verts[i].controlPointOut.y / _sourcePtmRatio;
        if (_clockwise)
        {
            verts[i].point.y = -verts[i].point.y;
            verts[i].controlPointIn.y = -verts[i].controlPointIn.y;
            verts[i].controlPointOut.y = -verts[i].controlPointOut.y;
        }
    }
}

// @005d6af0
float LevelB2D::calculateBezier(float t, float value0, float value1, float value2, float value3)
{
    // Cubic bezier in Horner form. The original (clang, -ffp-contract=on) fuses 8 of these
    // multiply-adds (fmadd); spelled out with std::fma so every compiler rounds the same way.
    float c1 = std::fma(3.0f, value1, -3.0f * value0);                         // 3v1 - 3v0
    float c2 = std::fma(3.0f, value0, std::fma(3.0f, value2, -6.0f * value1));  // 3v2 - 6v1 + 3v0
    float c3 = std::fma(3.0f, value1, std::fma(-3.0f, value2, value3)) - value0;  // v3 - 3v2 + 3v1 - v0
    return std::fma(std::fma(std::fma(c3, t, c2), t, c1), t, value0);
}

// @005d6b2c
void LevelB2D::addJoint(LevelDataElement* joint, int index)
{
    // Joint element: "t" 0 = revolute, 1 = prismatic; anchor "x"/"y"; bodies "b1"/"b2":
    // "s<n>" special n (LevelItem::getJointBody at the anchor), "g<n>" group n's body,
    // "<n>" shape n's body, "-1" (any negative) the level body.
    Session* session = Settings::getInstance()->getCurrentSession();
    b2Body* levelBody = session->getLevelBody();
    b2World* world = session->getWorld();

    int type = -1;
    joint->intAttribute("t", &type);
    float x = 0.0f;
    float y = 0.0f;
    joint->floatAttribute("x", &x);
    joint->floatAttribute("y", &y);
    convertPositionData(&x, &y);
    const char* body1Attribute = joint->stringAttribute("b1");
    const char* body2Attribute = joint->stringAttribute("b2");
    std::string body1Name(body1Attribute);
    std::string body2Name(body2Attribute);
    b2Vec2 anchor(x, y);
    std::string body1Kind = body1Name.substr(0, 1);
    std::string body2Kind = body2Name.substr(0, 1);

    b2Body* bodyA = nullptr;
    if (body1Kind == "s")
    {
        int specialIndex = std::stoi(body1Name.substr(1));
        for (auto it = _specials.begin(); it != _specials.end(); ++it)
        {
            if ((*it)->getIndex() == specialIndex)
            {
                bodyA = (*it)->getJointBody(anchor);
                break;
            }
        }
    }
    else if (body1Kind == "g")
    {
        // No null check: an unknown group id is not handled.
        bodyA = groupItemWithId(std::stoi(body1Name.substr(1)))->getBody();
    }
    else
    {
        int shapeIndex = std::stoi(body1Name);
        bodyA = levelBody;
        if (shapeIndex >= 0)
        {
            bodyA = nullptr;
            for (auto it = _shapeItems.begin(); it != _shapeItems.end(); ++it)
            {
                if ((*it)->getIndex() == shapeIndex)
                {
                    bodyA = (*it)->getFixtureRef()->GetBody();
                    break;
                }
            }
        }
    }

    b2Body* bodyB = nullptr;
    if (body2Kind == "s")
    {
        int specialIndex = std::stoi(body2Name.substr(1));
        for (auto it = _specials.begin(); it != _specials.end(); ++it)
        {
            if ((*it)->getIndex() == specialIndex)
            {
                bodyB = (*it)->getJointBody(anchor);
                break;
            }
        }
    }
    else if (body2Kind == "g")
    {
        bodyB = groupItemWithId(std::stoi(body2Name.substr(1)))->getBody();
    }
    else
    {
        int shapeIndex = std::stoi(body2Name);
        bodyB = levelBody;
        if (shapeIndex >= 0)
        {
            bodyB = nullptr;
            for (auto it = _shapeItems.begin(); it != _shapeItems.end(); ++it)
            {
                if ((*it)->getIndex() == shapeIndex)
                {
                    bodyB = (*it)->getFixtureRef()->GetBody();
                    break;
                }
            }
        }
    }

    if (type == 1)
    {
        bool enableLimit = false;
        bool enableMotor = false;
        bool collideConnected = false;
        float motorSpeed = 0.0f;
        float axisAngle = 0.0f;
        float upperTranslation = 0.0f;
        float lowerTranslation = 0.0f;
        float maxMotorForce = 0.0f;
        joint->boolAttribute("l", &enableLimit);
        joint->boolAttribute("m", &enableMotor);
        joint->boolAttribute("c", &collideConnected);
        joint->floatAttribute("sp", &motorSpeed);
        joint->floatAttribute("a", &axisAngle);
        joint->floatAttribute("ul", &upperTranslation);
        joint->floatAttribute("ll", &lowerTranslation);
        joint->floatAttribute("fo", &maxMotorForce);
        convertRotationData(&axisAngle);
        convertLengthData(&upperTranslation);
        convertLengthData(&lowerTranslation);
        float axisRadians = axisAngle * 0.017453292f;
        b2Vec2 axis(cosf(axisRadians), sinf(axisRadians));

        b2PrismaticJointDef jointDef;
        jointDef.Initialize(bodyA, bodyB, anchor, axis);
        if (enableLimit)
        {
            jointDef.enableLimit = true;
            jointDef.lowerTranslation = lowerTranslation;
            jointDef.upperTranslation = upperTranslation;
        }
        if (enableMotor)
        {
            jointDef.enableMotor = true;
        }
        jointDef.maxMotorForce = maxMotorForce;
        jointDef.motorSpeed = motorSpeed;
        if (collideConnected)
        {
            jointDef.collideConnected = true;
        }
        _joints.push_back(world->CreateJoint(&jointDef));
        if (online::flashLevel())
        {
            online::userVehicleJointCreated(this, joint, _joints.back());  // ONLINE (PC addition)
        }
    }
    else if (type == 0)
    {
        bool enableLimit = false;
        bool enableMotor = false;
        bool collideConnected = false;
        float motorSpeed = 0.0f;
        float upperAngle = 0.0f;
        float lowerAngle = 0.0f;
        float maxMotorTorque = 0.0f;
        joint->boolAttribute("l", &enableLimit);
        joint->boolAttribute("m", &enableMotor);
        joint->boolAttribute("c", &collideConnected);
        joint->floatAttribute("sp", &motorSpeed);
        joint->floatAttribute("ua", &upperAngle);
        joint->floatAttribute("la", &lowerAngle);
        convertRevJointData(&motorSpeed, &lowerAngle, &upperAngle);
        joint->floatAttribute("tq", &maxMotorTorque);

        b2RevoluteJointDef jointDef;
        jointDef.Initialize(bodyA, bodyB, anchor);
        if (enableLimit)
        {
            jointDef.enableLimit = true;
            jointDef.lowerAngle = lowerAngle * 0.017453292f;
            jointDef.upperAngle = upperAngle * 0.017453292f;
        }
        if (enableMotor)
        {
            jointDef.enableMotor = true;
        }
        jointDef.motorSpeed = motorSpeed;
        jointDef.maxMotorTorque = maxMotorTorque;
        // ONLINE (PC addition): Flash <= 1.84 gives a joint without motor the torque 50 (a
        // trigger's "change motor speed" switches the motor on with it).
        if (online::flashLevel() && online::flashVersion() <= 1.84f && !enableMotor)
        {
            jointDef.maxMotorTorque = 50.0f;
        }
        if (collideConnected)
        {
            jointDef.collideConnected = true;
        }
        _joints.push_back(world->CreateJoint(&jointDef));
        if (online::flashLevel())
        {
            online::userVehicleJointCreated(this, joint, _joints.back());  // ONLINE (PC addition)
        }
    }
}

// @005d7c38
GroupItem* LevelB2D::groupItemWithId(int index)
{
    for (auto it = _groupItems.begin(); it != _groupItems.end(); ++it)
    {
        if ((*it)->getIndex() == index)
        {
            return *it;
        }
    }
    for (auto it = _foregroundGroupItems.begin(); it != _foregroundGroupItems.end(); ++it)
    {
        if ((*it)->getIndex() == index)
        {
            return *it;
        }
    }
    return nullptr;
}

// @005d7cc8
void LevelB2D::convertRevJointData(float* motorSpeed, float* lowerAngle, float* upperAngle)
{
    if (_clockwise)
    {
        *motorSpeed = -*motorSpeed;
        float upper = *upperAngle;
        *upperAngle = -*lowerAngle;
        *lowerAngle = -upper;
    }
}

// @005d7cf8
void LevelB2D::convertRotationData(float* rotation)
{
    if (_clockwise)
    {
        *rotation = -*rotation;
    }
}

// @005d7d10
void LevelB2D::addTrigger(LevelDataElement* trigger, int index)
{
    // Triggers are built in addTriggersComplete, once every other element exists.
    trigger->retain();
    _triggerElements.push_back(trigger);
}

// @005d7e8c
void LevelB2D::addTriggersComplete()
{
    // Pass 1: create every Trigger (index = position in the level's trigger list).
    int triggerIndex = 0;
    for (auto it = _triggerElements.begin(); it != _triggerElements.end(); ++it)
    {
        LevelDataElement* element = *it;
        // RE-TODO(@005d7f6c): the original passes an uninitialised b2Vec2 offset (s0/s1 are
        // never set before the virtual init call); Trigger::init does not need it.
        b2Vec2 offset;
        Trigger* trigger = new (std::nothrow) Trigger();
        if (trigger != nullptr)
        {
            if (trigger->init(element, nullptr, offset))
            {
                trigger->autorelease();
            }
            else
            {
                delete trigger;
                trigger = nullptr;
            }
        }
        bool disabled = false;
        element->boolAttribute("sd", &disabled);
        trigger->setDisabled(disabled);
        trigger->setIndex(triggerIndex);
        trigger->retain();
        _triggers.push_back(trigger);
        triggerIndex++;
    }

    // Pass 2: wire up the targets of "targets" triggers and the activation bodies of
    // triggers activated by specific bodies. Target children: <t> triggers, <sh> shapes,
    // <j> joints, <g> groups, <sp> specials, each with "i" (target index), "a" (action) and
    // action properties "p0", "p1", ...
    for (int i = 0; i < _triggerElements.size(); i++)
    {
        LevelDataElement* element = _triggerElements[i];
        Trigger* trigger = _triggers[i];
        int type = -1;
        element->intAttribute("t", &type);
        int triggeredBy = -1;
        element->intAttribute("b", &triggeredBy);
        if (type != TriggerTypeTargets && triggeredBy != TriggerTriggeredByBodies)
        {
            continue;
        }
        if (online::flashLevel())
        {
            onlineAddTriggerTargets(element, trigger, type, triggeredBy);  // ONLINE (PC addition)
            continue;
        }

        for (tinyxml2::XMLElement* target = element->getData()->FirstChildElement("t");
             target != nullptr; target = target->NextSiblingElement("t"))
        {
            int targetIndex = -1;
            int action = -1;
            target->QueryIntAttribute("i", &targetIndex);
            target->QueryIntAttribute("a", &action);
            trigger->addTargetActionTrigger(nullptr, trigger, _triggers[targetIndex], action,
                                            std::vector<float>());
        }

        tinyxml2::XMLElement* target = element->getData()->FirstChildElement("sh");
        while (target != nullptr)
        {
            std::vector<float> properties;
            int action = -1;
            int targetIndex = -1;
            target->QueryIntAttribute("i", &targetIndex);
            target->QueryIntAttribute("a", &action);
            float value = 0.0f;
            std::string key = "p0";
            int count = 0;
            while (target->QueryFloatAttribute(key.c_str(), &value) != tinyxml2::XML_NO_ATTRIBUTE)
            {
                properties.push_back(value);
                count++;
                key = "p" + std::to_string(count);
            }

            // No null check on the shape item.
            ShapeItem* shapeItem = getShapeItem(targetIndex);
            b2Fixture* fixture = shapeItem->getFixtureRef();
            TargetAction* targetAction =
                trigger->addTargetActionShapeItem(shapeItem, fixture, nullptr, action, properties);
            _targetActions[targetIndex].push_back(targetAction);
            if (fixture != nullptr && triggeredBy == TriggerTriggeredByBodies &&
                fixture->GetBody()->GetMass() > 0.0f && !fixture->IsSensor())
            {
                std::vector<b2Body*> bodies = {fixture->GetBody()};
                trigger->addActivationBodies(bodies);
            }
            target = target->NextSiblingElement("sh");
        }

        target = element->getData()->FirstChildElement("j");
        while (target != nullptr)
        {
            std::vector<float> properties;
            int action = -1;
            int targetIndex = -1;
            target->QueryIntAttribute("i", &targetIndex);
            target->QueryIntAttribute("a", &action);
            float value = 0.0f;
            std::string key = "p0";
            int count = 0;
            while (target->QueryFloatAttribute(key.c_str(), &value) != tinyxml2::XML_NO_ATTRIBUTE)
            {
                properties.push_back(value);
                count++;
                key = "p" + std::to_string(count);
            }

            // Joints are addressed by creation order (no bounds check).
            b2Joint* targetJoint = _joints[targetIndex];
            if (targetJoint->GetType() == e_revoluteJoint)
            {
                TargetActionRevJoint* targetAction = trigger->addTargetActionRevJoint(
                    static_cast<b2RevoluteJoint*>(targetJoint), action, properties);
                targetAction->setIndex(targetIndex + 10000);
                _targetActions[targetIndex + 10000].push_back(targetAction);
            }
            else if (targetJoint->GetType() == e_prismaticJoint)
            {
                TargetActionPrisJoint* targetAction = trigger->addTargetActionPrisJoint(
                    static_cast<b2PrismaticJoint*>(targetJoint), action, properties);
                targetAction->setIndex(targetIndex + 10000);
                _targetActions[targetIndex + 10000].push_back(targetAction);
            }
            target = target->NextSiblingElement("j");
        }

        target = element->getData()->FirstChildElement("g");
        while (target != nullptr)
        {
            std::vector<float> properties;
            int action = -1;
            int targetIndex = -1;
            target->QueryIntAttribute("i", &targetIndex);
            target->QueryIntAttribute("a", &action);
            float value = 0.0f;
            std::string key = "p0";
            int count = 0;
            while (target->QueryFloatAttribute(key.c_str(), &value) != tinyxml2::XML_NO_ATTRIBUTE)
            {
                properties.push_back(value);
                count++;
                key = "p" + std::to_string(count);
            }

            GroupItem* groupItem = groupItemWithId(targetIndex);
            TargetActionGroup* targetAction =
                trigger->addTargetActionGroup(groupItem, nullptr, action, properties);
            targetAction->setIndex(targetIndex + 20000);
            _targetActions[targetIndex + 20000].push_back(targetAction);
            if (triggeredBy == TriggerTriggeredByBodies && groupItem->getBody() != nullptr)
            {
                // Only a group with mass and at least one non-sensor fixture activates.
                bool sensorsOnly = true;
                for (b2Fixture* fixture = groupItem->getBody()->GetFixtureList();
                     fixture != nullptr; fixture = fixture->GetNext())
                {
                    sensorsOnly = fixture->IsSensor();
                    if (!sensorsOnly)
                    {
                        break;
                    }
                }
                if (!(groupItem->getBody()->GetMass() > 0.0f))
                {
                    sensorsOnly = true;
                }
                if (!sensorsOnly)
                {
                    std::vector<b2Body*> bodies = {groupItem->getBody()};
                    trigger->addActivationBodies(bodies);
                }
            }
            target = target->NextSiblingElement("g");
        }

        target = element->getData()->FirstChildElement("sp");
        while (target != nullptr)
        {
            std::vector<float> properties;
            int action = -1;
            int targetIndex = -1;
            target->QueryIntAttribute("i", &targetIndex);
            target->QueryIntAttribute("a", &action);
            float value = 0.0f;
            std::string key = "p0";
            int count = 0;
            while (target->QueryFloatAttribute(key.c_str(), &value) != tinyxml2::XML_NO_ATTRIBUTE)
            {
                properties.push_back(value);
                count++;
                key = "p" + std::to_string(count);
            }

            // No null check on the special.
            LevelItem* special = getSpecial(targetIndex);
            special->prepareForTrigger();
            TargetActionSpecial* targetAction =
                trigger->addTargetItemSpecial(special, nullptr, nullptr, action, properties);
            targetAction->setIndex(targetIndex + 30000);
            // The original registers the special itself (not its TargetActionSpecial) here.
            _targetActions[targetIndex + 30000].push_back(special);
            if (triggeredBy == TriggerTriggeredByBodies)
            {
                std::vector<b2Body*> bodies = special->getBodyList();
                if (bodies.size() != 0)
                {
                    trigger->addActivationBodies(bodies);
                }
            }
            target = target->NextSiblingElement("sp");
        }
    }

    for (auto it = _triggerElements.begin(); it != _triggerElements.end(); ++it)
    {
        (*it)->release();
    }
    _triggerElements.clear();

    if (online::flashLevel())
    {
        online::installClickTriggers(this);  // ONLINE (PC addition): b = 6 click triggers
    }
}

// ONLINE (PC addition): Flash UserLevel.createTriggers. Targets are wired in the order the level
// lists them (Flash keeps the editor's target order; the mobile loader above goes by kind: t, sh,
// j, g, sp), so a trigger whose targets are "set to non fixed" then "activate trigger X" (X
// applying an impulse) acts in that order. Actions only exist on "activate object" triggers, and
// specials get prepareForTrigger() only from those; "triggered by specific bodies" takes the
// activation bodies from every target. Missing targets are skipped instead of dereferenced.
void LevelB2D::onlineAddTriggerTargets(LevelDataElement* element, Trigger* trigger, int type,
                                       int triggeredBy)
{
    const bool runs = type == TriggerTypeTargets;
    const bool bodies = triggeredBy == TriggerTriggeredByBodies;
    for (tinyxml2::XMLElement* target = element->getData()->FirstChildElement(); target != nullptr;
         target = target->NextSiblingElement())
    {
        const std::string kind = target->Value() ? target->Value() : "";
        int targetIndex = -1;
        int action = -1;
        target->QueryIntAttribute("i", &targetIndex);
        target->QueryIntAttribute("a", &action);
        std::vector<float> properties;
        float value = 0.0f;
        for (int count = 0;; count++)
        {
            const std::string key = "p" + std::to_string(count);
            if (target->QueryFloatAttribute(key.c_str(), &value) == tinyxml2::XML_NO_ATTRIBUTE)
            {
                break;
            }
            properties.push_back(value);
        }

        if (kind == "t")
        {
            if (runs && action >= 0 && targetIndex >= 0 && targetIndex < (int)_triggers.size())
            {
                trigger->addTargetActionTrigger(nullptr, trigger, _triggers[targetIndex], action,
                                                properties);
            }
        }
        else if (kind == "sh")
        {
            ShapeItem* shapeItem = getShapeItem(targetIndex);
            if (shapeItem == nullptr)
            {
                continue;
            }
            b2Fixture* fixture = shapeItem->getFixtureRef();
            if (runs && action >= 0)
            {
                TargetAction* targetAction = trigger->addTargetActionShapeItem(
                    shapeItem, fixture, nullptr, action, properties);
                _targetActions[targetIndex].push_back(targetAction);
            }
            if (bodies && fixture != nullptr && fixture->GetBody()->GetMass() > 0.0f &&
                !fixture->IsSensor())
            {
                trigger->addActivationBodies(std::vector<b2Body*>{fixture->GetBody()});
            }
        }
        else if (kind == "j")
        {
            if (!runs || action < 0 || targetIndex < 0 || targetIndex >= (int)_joints.size() ||
                _joints[targetIndex] == nullptr)
            {
                continue;
            }
            b2Joint* targetJoint = _joints[targetIndex];
            if (targetJoint->GetType() == e_revoluteJoint)
            {
                TargetActionRevJoint* targetAction = trigger->addTargetActionRevJoint(
                    static_cast<b2RevoluteJoint*>(targetJoint), action, properties);
                targetAction->setIndex(targetIndex + 10000);
                _targetActions[targetIndex + 10000].push_back(targetAction);
            }
            else if (targetJoint->GetType() == e_prismaticJoint)
            {
                TargetActionPrisJoint* targetAction = trigger->addTargetActionPrisJoint(
                    static_cast<b2PrismaticJoint*>(targetJoint), action, properties);
                targetAction->setIndex(targetIndex + 10000);
                _targetActions[targetIndex + 10000].push_back(targetAction);
            }
        }
        else if (kind == "g")
        {
            GroupItem* groupItem = groupItemWithId(targetIndex);
            if (groupItem == nullptr)
            {
                continue;
            }
            if (runs && action >= 0)
            {
                TargetActionGroup* targetAction =
                    trigger->addTargetActionGroup(groupItem, nullptr, action, properties);
                targetAction->setIndex(targetIndex + 20000);
                _targetActions[targetIndex + 20000].push_back(targetAction);
            }
            if (bodies && groupItem->getBody() != nullptr)
            {
                trigger->addActivationBodies(std::vector<b2Body*>{groupItem->getBody()});
            }
        }
        else if (kind == "sp")
        {
            LevelItem* special = getSpecial(targetIndex);
            if (special == nullptr)
            {
                continue;
            }
            if (runs)
            {
                special->prepareForTrigger();
                if (action >= 0)
                {
                    TargetActionSpecial* targetAction =
                        trigger->addTargetItemSpecial(special, nullptr, nullptr, action, properties);
                    targetAction->setIndex(targetIndex + 30000);
                    _targetActions[targetIndex + 30000].push_back(special);
                }
            }
            if (bodies)
            {
                std::vector<b2Body*> list = special->getBodyList();
                if (!list.empty())
                {
                    trigger->addActivationBodies(list);
                }
            }
        }
    }
}

// @005da388
ShapeItem* LevelB2D::getShapeItem(int index)
{
    for (int i = 0; i < _shapeItems.size(); i++)
    {
        ShapeItem* shapeItem = _shapeItems[i];
        if (shapeItem->getIndex() == index)
        {
            return shapeItem;
        }
    }
    return nullptr;
}

// @005da3f0
LevelItem* LevelB2D::getSpecial(unsigned int index)
{
    for (unsigned int i = 0; i < _specials.size(); i++)
    {
        LevelItem* special = _specials[i];
        if (special->getIndex() == index)
        {
            return special;
        }
    }
    return nullptr;
}

// @005da458
void LevelB2D::addGroup(LevelDataElement* group, int index)
{
    b2World* world = Settings::getInstance()->getCurrentSession()->getWorld();

    float x = 0.0f;
    float y = 0.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float opacity = 0.0f;
    float rotation = 0.0f;
    bool sleeping = false;
    bool immovable = false;
    bool fixedRotation = false;
    bool foreground = false;
    group->floatAttribute("x", &x);
    group->floatAttribute("y", &y);
    group->floatAttribute("ox", &offsetX);
    group->floatAttribute("oy", &offsetY);
    group->floatAttribute("o", &opacity);
    opacity = opacity / 100.0f;
    group->floatAttribute("r", &rotation);
    group->boolAttribute("s", &sleeping);
    group->boolAttribute("im", &immovable);
    group->boolAttribute("fr", &fixedRotation);
    group->boolAttribute("f", &foreground);

    x = x / _sourcePtmRatio;
    y = _stageHeight - y / _sourcePtmRatio;
    // Offset that turns the converted (stage-flipped) shape positions into group-local ones.
    offsetX = offsetX / _sourcePtmRatio;
    offsetY = -_stageHeight - offsetY / _sourcePtmRatio;

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position.Set(x, y);
    bodyDef.angle = rotation * -0.017453292f;
    bodyDef.awake = !sleeping;
    bodyDef.fixedRotation = fixedRotation;
    b2Body* body = world->CreateBody(&bodyDef);

    GroupItem* groupItem = new (std::nothrow) GroupItem();
    groupItem->init(body, _ptmRatio);
    groupItem->setIndex(index);
    groupItem->setBody(body);
    groupItem->setImmovable(immovable);

    // ONLINE (PC addition): a browser group saved as a vehicle (v="t") drives like Flash's
    // level/groups/Vehicle (src/online/vehicles/UserVehicle.cpp).
    online::UserVehicle* userVehicle =
        online::flashLevel() ? online::UserVehicle::createForGroup(this, group, body, index) : nullptr;

    for (tinyxml2::XMLElement* element = group->getData()->FirstChildElement("sh");
         element != nullptr; element = element->NextSiblingElement("sh"))
    {
        LevelDataElement* shape = new LevelDataElement;
        shape->init(element);
        b2Fixture* previousFirstFixture = body->GetFixtureList();  // ONLINE (PC addition)
        addShape(shape, groupItem, Vec2(offsetX, offsetY), 0, foreground);
        if (userVehicle)
        {
            userVehicle->shapeAdded(shape, body, previousFirstFixture);  // ONLINE (PC addition)
        }
        delete shape;
    }
    if (online::flashLevel())
    {
        // ONLINE (PC addition): remember the shape fixtures (see GroupItem::onlineShapeFixtures).
        for (b2Fixture* fixture = body->GetFixtureList(); fixture != nullptr;
             fixture = fixture->GetNext())
        {
            groupItem->onlineShapeFixtures.push_back(fixture);
        }
    }

    for (tinyxml2::XMLElement* element = group->getData()->FirstChildElement("sp");
         element != nullptr; element = element->NextSiblingElement("sp"))
    {
        LevelDataElement* specialElement = new LevelDataElement;
        specialElement->init(element);
        LevelItem* special = addSpecial(specialElement, 0, body, b2Vec2(offsetX, offsetY));
        if (userVehicle && special)
        {
            userVehicle->groupSpecialAdded(special);  // ONLINE (PC addition)
        }
        float specialX = 0.0f;
        float specialY = 0.0f;
        float specialRotation = 0.0f;
        int specialType = -1;
        specialElement->floatAttribute("p0", &specialX);
        specialElement->floatAttribute("p1", &specialY);
        specialElement->floatAttribute("p2", &specialRotation);
        specialElement->intAttribute("t", &specialType);
        specialX = specialX / _sourcePtmRatio;
        specialY = _stageHeight - specialY / _sourcePtmRatio;
        if (specialType == 3)
        {
            // IBeam keeps its rotation in p4.
            float beamRotation = 0.0f;
            specialElement->floatAttribute("p4", &beamRotation);
            specialRotation = beamRotation;
        }
        GroupSpecial groupSpecial;
        groupSpecial.special = special;
        groupSpecial.rotation = specialRotation;
        groupSpecial.offset = Vec2(_ptmRatio * offsetX + _ptmRatio * specialX,
                                   _ptmRatio * offsetY + _ptmRatio * specialY);
        groupItem->addSpecial(groupSpecial);
        delete specialElement;
    }

    if (body->GetFixtureList() == nullptr)
    {
        world->DestroyBody(body);
        groupItem->setBody(nullptr);
    }
    else
    {
        body->ResetMassData();
        bool nanMass = false;
        if (online::flashLevel() && group->boolAttribute("nm", &nanMass) && nanMass)
        {
            body->SetType(b2_staticBody);  // ONLINE (PC addition): see onlineNanMassBodies
            onlineNanMassBodies.insert(body);
        }
    }
    if (userVehicle)
    {
        userVehicle->groupFinished(groupItem->getBody());  // ONLINE (PC addition)
    }
    groupItem->setOpacity(opacity);

    if (foreground)
    {
        _foregroundGroupItems.push_back(groupItem);
    }
    else
    {
        _groupItems.push_back(groupItem);
    }
}

// @005dae94
void LevelB2D::removeGroupItem(GroupItem* groupItem)
{
    auto it = std::find(_groupItems.begin(), _groupItems.end(), groupItem);
    if (it != _groupItems.end())
    {
        _groupItems.erase(it);
    }
    else
    {
        it = std::find(_foregroundGroupItems.begin(), _foregroundGroupItems.end(), groupItem);
        if (it == _foregroundGroupItems.end())
        {
            return;
        }
        _foregroundGroupItems.erase(it);
    }
    if (groupItem != nullptr)
    {
        delete groupItem;
    }
}

// @005db248
GroupItem* LevelB2D::getGroupItem(unsigned int index)
{
    for (unsigned int i = 0; i < _groupItems.size(); i++)
    {
        GroupItem* groupItem = _groupItems[i];
        if (groupItem->getIndex() == index)
        {
            return groupItem;
        }
    }
    for (unsigned int i = 0; i < _foregroundGroupItems.size(); i++)
    {
        GroupItem* groupItem = _foregroundGroupItems[i];
        if (groupItem->getIndex() == index)
        {
            return groupItem;
        }
    }
    return nullptr;
}

// @005db2e8 (non-virtual thunk @005db380)
void LevelB2D::removeShapeItem(ShapeItem* shapeItem, bool deleteItem)
{
    auto it = std::find(_shapeItems.begin(), _shapeItems.end(), shapeItem);
    if (it != _shapeItems.end())
    {
        ShapeItem* found = *it;
        _shapeItems.erase(it);
        if (deleteItem && found != nullptr)
        {
            delete found;
        }
    }
}

// @005db418
void LevelB2D::removeSpecial(LevelItem* special)
{
    auto it = std::find(_specials.begin(), _specials.end(), special);
    if (it != _specials.end())
    {
        _specials.erase(it);
        special->release();
    }
}

// @005db49c
void LevelB2D::convertPositionData(float* x, float* y)
{
    *x = *x / _sourcePtmRatio;
    *y = *y / _sourcePtmRatio;
    // EDITOR (iOS port): iOS flips y only for registration (r="1") levels; editor levels (no r) are y-up metres.
    if (_registration)
    {
        *y = _stageHeight - *y;
    }
}

// @005db4cc
void LevelB2D::convertDirectionIfNecessaryBasedOnRegistration(float* value)
{
    if (_registration)
    {
        *value = -*value;
    }
}

// @005db4e4
std::vector<LevelItem*> LevelB2D::getActionsVector()
{
    return _actionsVector;
}

// @005db5dc
std::vector<CharacterB2D*> LevelB2D::getCharacters()
{
    return _characters;
}

// @005db6d4
void LevelB2D::updateTargetActionRevJoint(unsigned int index, b2Joint* joint,
                                          TargetActionRevJoint* caller)
{
    std::vector<LevelItem*> actions = _targetActions[(int)index];
    for (unsigned int i = 0; i < actions.size(); i++)
    {
        TargetActionRevJoint* action = static_cast<TargetActionRevJoint*>(actions[i]);
        if (action != caller)
        {
            action->updateTargetActionForJoint(joint);
        }
    }
}

// @005db938
void LevelB2D::updateTargetActionPrisJoint(unsigned int index, b2Joint* joint,
                                           TargetActionPrisJoint* caller)
{
    std::vector<LevelItem*> actions = _targetActions[(int)index];
    for (unsigned int i = 0; i < actions.size(); i++)
    {
        TargetActionPrisJoint* action = static_cast<TargetActionPrisJoint*>(actions[i]);
        if (action != caller)
        {
            action->updateTargetActionForJoint(joint);
        }
    }
}

// @005dbb9c
void LevelB2D::updateTargetActionsFor(unsigned int index, ShapeItem* shapeItem,
                                      b2Fixture* currentShape, b2Fixture* newShape,
                                      TargetAction* caller)
{
    std::vector<LevelItem*> actions = _targetActions[(int)index];
    for (unsigned int i = 0; i < actions.size(); i++)
    {
        TargetAction* action = static_cast<TargetAction*>(actions[i]);
        if (action != caller)
        {
            action->updateTargetActionsForShapeItem(shapeItem, currentShape, newShape);
        }
    }
}

// @005dbe1c
void LevelB2D::updateTargetActionGroupsFor(unsigned int index, GroupItem* groupItem,
                                           TargetActionGroup* caller)
{
    std::vector<LevelItem*> actions = _targetActions[(int)index];
    for (unsigned int i = 0; i < actions.size(); i++)
    {
        TargetActionGroup* action = static_cast<TargetActionGroup*>(actions[i]);
        if (action != caller)
        {
            action->updateTargetActionsForGroupItem(groupItem);
        }
    }
}

// @005dc080
void LevelB2D::addFixtureMaterial(b2Fixture* fixture, int material)
{
    _fixtureMaterials[fixture] = material;
}

// @005dc148
void LevelB2D::removeFixtureMaterial(b2Fixture* fixture)
{
    _fixtureMaterials.erase(fixture);
}

// @005dc20c
int LevelB2D::getFixtureMaterial(b2Fixture* fixture)
{
    auto it = _fixtureMaterials.find(fixture);
    if (it != _fixtureMaterials.end())
    {
        return it->second;
    }
    return 0;
}
