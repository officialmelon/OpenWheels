#include "Session.h"

#include "qol/QoL.h"  // QOL (PC addition)

#include "B2DebugDrawLayer.h"
#include "CharacterB2D.h"
#include "ContactListener.h"
#include "DestructionListener.h"
#include "EmitterNode.h"
#include "FFDrawNode.h"
#include "GLESDebugDraw.h"
#include "Globals.h"
#include "LevelB2D.h"
#include "LevelItemsDrawNode.h"
#include "SoundController.h"
#include "StageCamera.h"
#include "TerrainNode.h"
#include "online/FlashRuntime.h"  // ONLINE (PC addition)
#include "online/replays/ReplayRuntime.h"  // ONLINE (PC addition)

USING_NS_CC;

namespace {

// Pixels per metre for each SessionMode (.rodata @0041dd48).
const float kSessionPtmRatios[3] = {250.0f, 500.0f, 250.0f};

// Physics step of the session (.data @00abb640, file-local in the original: no symbol).
// Written by Session::setTimeStep, read by Session::update / Session::getTimeStep.
float s_sessionTimeStep = 1.0f / 60.0f;

}  // namespace

// @0060f2fc
Session::Session()
    : _world(nullptr)
    , _levelBody(nullptr)
    , _camera(nullptr)
    , _level(nullptr)
    , _version(0.0f)
    , _ptmRatio(1.0f)
    , _unk0x320(0.0f)
    , _mode(SessionModeGameplay)
    , _unk0x330(1.0f)
    , _unk0x338(nullptr)
    , _unk0x340(0)
    // RE-NOTE(@0060f2fc): the original never initialises the step accumulator (fresh heap memory
    // is zero in practice; the emulator oracle also runs with 0). Zeroed here for determinism.
    , _timeAccumulator(0.0f)
    , _drag(false)
    , _isReplay(false)
    , _userHasDisabledGoreDuringSession(false)
    , _contactListener(nullptr)
    , _destructionListener(nullptr)
    , _debugDraw(nullptr)
    , _debugDrawLayer(nullptr)
    , _characterBackground(nullptr)
    , _characterMidground(nullptr)
    , _characterForeground(nullptr)
    , _vehicleBackground(nullptr)
    , _vehicleForeground(nullptr)
    , _levelItemsNode(nullptr)
    , _labelAtlasNode(nullptr)
    , _backgroundDrawNode(nullptr)
    , _midgroundDrawNode(nullptr)
    , _soundController(nullptr)
    , _particlesForeground(nullptr)
    , _particlesMidground(nullptr)
    , _particlesBackground(nullptr)
    , _backgroundLayer(nullptr)
    , _controls(nullptr)
    , _drawNode(nullptr)
{
    // _terrainNode, _shapesNode, _foregroundShapesNode and _unk0x408 are left uninitialised, as in
    // the original (init() assigns the three nodes).
}

// @0060f394
Session::~Session()
{
    die();
}

// @0060f3cc
void Session::die()
{
    if (_world != nullptr)
    {
        delete _world;
        _world = nullptr;
    }
    if (_contactListener != nullptr)
    {
        delete _contactListener;
        _contactListener = nullptr;
    }
    if (_destructionListener != nullptr)
    {
        delete _destructionListener;
        _destructionListener = nullptr;
    }
    if (_level != nullptr)
    {
        _level->die();
        delete _level;
        _level = nullptr;
    }
    if (_debugDraw != nullptr)
    {
        delete _debugDraw;
        _debugDraw = nullptr;
    }
    if (_camera != nullptr)
    {
        _camera->release();
        _camera = nullptr;
    }
}

// @0060f4a4
Session* Session::create(float version, SoundController* soundController, SessionMode mode)
{
    Session* session = new (std::nothrow) Session();
    if (session != nullptr)
    {
        // The result of init() is ignored by the original.
        session->init(version, soundController, mode);
        session->autorelease();
    }
    return session;
}

// @0060f534
bool Session::init(float version, SoundController* soundController, SessionMode mode)
{
    _level = nullptr;
    _debugDraw = nullptr;
    _mode = mode;
    if (mode < 3)
    {
        _ptmRatio = kSessionPtmRatios[mode];
    }
    _version = version;
    _soundController = soundController;
    _drag = true;

    _levelItemsNode = Node::create();
    _labelAtlasNode = Node::create();
    _characterBackground = Node::create();
    _characterMidground = Node::create();
    _characterForeground = Node::create();
    _vehicleBackground = Node::create();
    _vehicleForeground = Node::create();
    _backgroundDrawNode = LevelItemsDrawNode::create(_ptmRatio);
    _midgroundDrawNode = LevelItemsDrawNode::create(_ptmRatio);
    _shapesNode = FFDrawNode::create(2.0f);
    _foregroundShapesNode = FFDrawNode::create(2.0f);

    UserDefault* userDefault = UserDefault::getInstance();
    if (userDefault->getBoolForKey("gore_disabled"))
    {
        _userHasDisabledGoreDuringSession = true;
    }
    if (!userDefault->getBoolForKey("particles_disabled"))
    {
        _particlesForeground = EmitterNode::create();
        _particlesMidground = EmitterNode::create();
        _particlesBackground = EmitterNode::create();
        addChild(_particlesForeground, 15);
        addChild(_particlesMidground, 10);
        addChild(_particlesBackground, 5);
    }

    _terrainNode = TerrainNode::create(2.0f);
    addChild(_levelItemsNode, 2);
    addChild(_labelAtlasNode, 3);
    addChild(_characterBackground, 6);
    addChild(_characterMidground, 8);
    addChild(_characterForeground, 11);
    addChild(_terrainNode, 13);
    addChild(_vehicleBackground, 7);
    addChild(_vehicleForeground, 12);
    addChild(_backgroundDrawNode, 4);
    addChild(_midgroundDrawNode, 9);
    addChild(_shapesNode, 1);
    addChild(_foregroundShapesNode, 14);

    if (_drawNode == nullptr)
    {
        _drawNode = DrawNode::create(2.0f);
        addChild(_drawNode, 1000);
    }

    const float ptmRatio = _ptmRatio;
    const float stageWidth = globals::flash::stageSizeMeters.width;
    const float stageHeight = globals::flash::stageSizeMeters.height;
    _camera = StageCamera::create(this, nullptr, ptmRatio);
    _camera->retain();
    _camera->setLimits(Size(stageWidth * ptmRatio, ptmRatio * stageHeight));
    // QOL (PC addition): camera zoom-out for gameplay. The stage node is scaled about its origin and
    // the camera's position limits take the scaled stage size; the camera already works in world
    // (screen) space through convertToWorldSpace.
    const float zoom = qol::cameraZoom();
    if (mode == SessionModeGameplay && zoom < 1.0f)
    {
        setScale(zoom);
        _camera->setLimits(Size(stageWidth * ptmRatio * zoom, ptmRatio * stageHeight * zoom));
    }
    return true;
}

// @0060f980
void Session::setMode(SessionMode mode)
{
    _mode = mode;
    if (mode < 3)
    {
        _ptmRatio = kSessionPtmRatios[mode];
    }
}

// @0060f9a0
Node* Session::getGameplayContainer()
{
    return this;
}

// @0060f9a4
void Session::setVersion(float version)
{
    _version = version;
}

// @0060f9ac
b2World* Session::getWorld()
{
    return _world;
}

// @0060f9b4
void Session::createWorld()
{
    createWorld(b2Vec2(0.0f, -10.0f));
}

// @0060f9c0
void Session::createWorld(b2Vec2 gravity)
{
    _world = new b2World(gravity);
    _contactListener = new ContactListener();
    _world->SetContactListener(_contactListener);
    _destructionListener = new DestructionListener();
    _world->SetDestructionListener(_destructionListener);
    _world->SetAutoClearForces(true);
    _world->SetAllowSleeping(true);

    b2BodyDef levelBodyDef;
    _levelBody = _world->CreateBody(&levelBodyDef);

    // Two vertical walls at the left and right edges of the stage.
    const float stageWidth = globals::flash::stageSizeMeters.width;
    const float stageHeight = globals::flash::stageSizeMeters.height;
    const float bottom = stageHeight * -2.0f;
    const float top = stageHeight * 3.0f;

    b2EdgeShape wall;
    b2Vec2 v1(0.0f, bottom);
    b2Vec2 v2(0.0f, top);
    wall.Set(v1, v2);

    b2FixtureDef wallFixtureDef;
    wallFixtureDef.shape = &wall;
    wallFixtureDef.filter.categoryBits = 4;
    _levelBody->CreateFixture(&wallFixtureDef);

    v2.Set(stageWidth, top);
    v1.Set(stageWidth, bottom);
    wall.Set(v1, v2);
    _levelBody->CreateFixture(&wallFixtureDef);
}

// @0060fbd8
SessionMode Session::getMode()
{
    return _mode;
}

// @0060fbe0
void Session::setGravity(Vec2 gravity)
{
    _gravity = gravity;
    if (_world != nullptr)
    {
        _world->SetGravity(b2Vec2(gravity.x, gravity.y));
    }
}

// @0060fc00
b2Body* Session::getLevelBody()
{
    return _levelBody;
}

// @0060fc08
Node* Session::getLevelItemsNode()
{
    return _levelItemsNode;
}

// @0060fc10
Node* Session::getLabelAtlasNode()
{
    return _labelAtlasNode;
}

// @0060fc18
Node* Session::getCharacterBackground()
{
    return _characterBackground;
}

// @0060fc20
Node* Session::getCharacterMidground()
{
    return _characterMidground;
}

// @0060fc28
Node* Session::getCharacterForeground()
{
    return _characterForeground;
}

// @0060fc30
TerrainNode* Session::getTerrainNode()
{
    return _terrainNode;
}

// @0060fc38
StageCamera* Session::getCamera()
{
    return _camera;
}

// @0060fc40
LevelItemsDrawNode* Session::getBackgroundDrawNode()
{
    return _backgroundDrawNode;
}

// @0060fc48
Node* Session::getVehicleBackground()
{
    return _vehicleBackground;
}

// @0060fc50
Node* Session::getVehicleForeground()
{
    return _vehicleForeground;
}

// @0060fc58
LevelItemsDrawNode* Session::getMidgroundDrawNode()
{
    return _midgroundDrawNode;
}

// @0060fc60
FFDrawNode* Session::getShapesNode()
{
    return _shapesNode;
}

// @0060fc68
FFDrawNode* Session::getForegroundShapesNode()
{
    return _foregroundShapesNode;
}

// @0060fc70
void Session::setupLevelForCharacterSelect()
{
    setupLevel(std::string(), true);
}

// @0060fcfc
void Session::setupLevel(std::string levelFile, bool characterSelect)
{
    _level = new LevelB2D();
    if (!characterSelect)
    {
        _level->init(levelFile);
    }
    _version = _level->getVersion();

    CharacterB2D* character = _level->getCharacter();
    if (character != nullptr)
    {
        b2Body* focus = character->getFocus();
        if (focus != nullptr)
        {
            _camera->setFocus(focus);
        }
    }
}

// @0060fdfc
LevelB2D* Session::getLevel()
{
    return _level;
}

// @0060fe04
float Session::getPtmRatio()
{
    return _ptmRatio;
}

// @0060fe0c
void Session::setTimeStep(float timeStep)
{
    s_sessionTimeStep = timeStep;
    if (_level != nullptr)
    {
        _level->setTimeStep(timeStep);
    }
}

// @0060fe24
float Session::getTimeStep()
{
    return s_sessionTimeStep;
}

// @0060fe30
void Session::slowMotion(float factor)
{
}

// @0060fe34
void Session::slowMotionStop()
{
}

// @0060fe38
ContactListener* Session::getContactListener()
{
    return _contactListener;
}

// @0060fe40
DestructionListener* Session::getDestructionListener()
{
    return _destructionListener;
}

// @0060fe48
GLESDebugDraw* Session::getDebugDraw()
{
    return _debugDraw;
}

// @0060fe50
bool Session::debugDrawVisible()
{
    return _debugDrawLayer != nullptr;
}

// @0060fe60
void Session::setDebugDrawVisible(bool visible)
{
    if (_world == nullptr)
    {
        return;
    }
    if (visible)
    {
        if (_debugDrawLayer == nullptr)
        {
            _debugDrawLayer = B2DebugDrawLayer::create(_world, _ptmRatio);
            addChild(_debugDrawLayer, 16);
        }
    }
    else if (_debugDrawLayer != nullptr)
    {
        removeChild(_debugDrawLayer, true);
        _debugDrawLayer = nullptr;
    }
}

// @0060fedc
bool Session::getDrag()
{
    return _drag;
}

// @0060fee4
void Session::setDrag(bool drag)
{
    _drag = drag;
}

// @0060feec
void Session::setIsReplay(bool isReplay)
{
    _isReplay = isReplay;
}

// @0060fef4
bool Session::getIsReplay()
{
    return _isReplay;
}

// @0060fefc
void Session::update(float dt)
{
    const float timeStep = s_sessionTimeStep;
    _timeAccumulator += dt;
    if (_timeAccumulator < timeStep)
    {
        return;
    }
    _timeAccumulator -= timeStep;

    if (online::flashLevel())
    {
        online::replays::physicsStep();  // ONLINE (PC addition): browser replays, per world step
        online::flashPreStep(_world);  // ONLINE (PC addition): Flash Box2D 2.0 contact rules
    }
    _world->Step(timeStep, 8, 3);
    if (online::flashLevel())
    {
        online::flashPostStep(_world);  // ONLINE (PC addition)
    }
    if (_level != nullptr)
    {
        _level->update(timeStep);
        _level->paint();
    }
    _shapesNode->updateVerts();
    _foregroundShapesNode->updateVerts();
    _midgroundDrawNode->update();
    _backgroundDrawNode->update();
    _camera->center();
    _soundController->update(_camera->getMidScreen());
}

// @0060ffb8
EmitterNode* Session::getParticlesForeground()
{
    return _particlesForeground;
}

// @0060ffc0
EmitterNode* Session::getParticlesMidground()
{
    return _particlesMidground;
}

// @0060ffc8
void Session::pauseEmitters()
{
    if (_particlesBackground != nullptr)
    {
        _particlesBackground->pauseEmitters();
        _particlesMidground->pauseEmitters();
        _particlesForeground->pauseEmitters();
    }
}

// @00610008
void Session::resumeEmitters()
{
    if (_particlesBackground != nullptr)
    {
        _particlesBackground->resumeEmitters();
        _particlesMidground->resumeEmitters();
        _particlesForeground->resumeEmitters();
    }
}

// @00610048
int Session::getMaxParticles()
{
    int total = 0;
    if (_particlesBackground != nullptr)
    {
        const int background = _particlesBackground->getMaxParticles();
        const int midground = _particlesMidground->getMaxParticles();
        const int foreground = _particlesForeground->getMaxParticles();
        total = midground + background + foreground;
    }
    return total;
}

// @00610090
bool Session::canAddEmitter(int particleCount)
{
    int total;
    if (_particlesBackground == nullptr)
    {
        total = 0;
    }
    else
    {
        const int background = _particlesBackground->getMaxParticles();
        const int midground = _particlesMidground->getMaxParticles();
        const int foreground = _particlesForeground->getMaxParticles();
        total = midground + background + foreground;
    }
    // QOL (PC addition): the budget is qol::maxParticles() (original and default: 2000).
    return total + particleCount < qol::maxParticles();
}

// @006100f8
EmitterNode* Session::getParticlesBackground()
{
    return _particlesBackground;
}

// @00610100
void Session::setBackgroundLayer(BackgroundLayer* backgroundLayer)
{
    _backgroundLayer = backgroundLayer;
}

// @00610108
BackgroundLayer* Session::getBackgroundLayer()
{
    return _backgroundLayer;
}

// @00610110
void Session::setControls(GameplayControls* controls)
{
    _controls = controls;
}

// @00610118
GameplayControls* Session::getControls()
{
    return _controls;
}

// @00610120
void Session::userHasDisabledGoreDuringSession()
{
    _userHasDisabledGoreDuringSession = true;
}

// @0061012c
bool Session::getUserHasDisabledGoreDuringSession()
{
    return _userHasDisabledGoreDuringSession;
}
