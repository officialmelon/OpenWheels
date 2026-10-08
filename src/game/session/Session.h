#pragma once

// Session: owns the Box2D world, the level, the camera, the draw/particle layers and the
// contact/destruction listeners of one gameplay (or character-select) scene.
// cocos2d::Node subclass, sizeof 0x410 (arm64).

#include <string>

#include "2d/CCNode.h"
#include "Box2D/Box2D.h"
#include "math/Vec2.h"

namespace cocos2d {
class DrawNode;
}

class B2DebugDrawLayer;
class BackgroundLayer;
class ContactListener;
class DestructionListener;
class EmitterNode;
class FFDrawNode;
class GameplayControls;
class GLESDebugDraw;
class LevelB2D;
class LevelItemsDrawNode;
class SoundController;
class StageCamera;
class TerrainNode;

// Session::_mode. Selects the pixels-per-meter ratio: {250, 500, 250}.
enum SessionMode
{
    SessionModeGameplay = 0,         // Gameplay, DebugScene
    SessionModeCharacterSelect = 1,  // CharacterSelectLayer ("character_select_" sprites)
    SessionModeUnknown2 = 2,         // RE-TODO: never created on Android; name unknown
};

class Session : public cocos2d::Node
{
public:
    Session();
    virtual ~Session();

    void die();
    static Session* create(float version, SoundController* soundController, SessionMode mode);
    bool init(float version, SoundController* soundController, SessionMode mode);

    void setMode(SessionMode mode);
    cocos2d::Node* getGameplayContainer();  // returns this
    void setVersion(float version);
    b2World* getWorld();
    void createWorld();                      // createWorld(b2Vec2(0, -10))
    void createWorld(b2Vec2 gravity);
    SessionMode getMode();
    void setGravity(cocos2d::Vec2 gravity);
    b2Body* getLevelBody();
    cocos2d::Node* getLevelItemsNode();
    cocos2d::Node* getLabelAtlasNode();
    cocos2d::Node* getCharacterBackground();
    cocos2d::Node* getCharacterMidground();
    cocos2d::Node* getCharacterForeground();
    TerrainNode* getTerrainNode();
    StageCamera* getCamera();
    LevelItemsDrawNode* getBackgroundDrawNode();
    cocos2d::Node* getVehicleBackground();
    cocos2d::Node* getVehicleForeground();
    LevelItemsDrawNode* getMidgroundDrawNode();
    FFDrawNode* getShapesNode();
    FFDrawNode* getForegroundShapesNode();
    void setupLevelForCharacterSelect();
    void setupLevel(std::string levelFile, bool characterSelect);
    LevelB2D* getLevel();
    float getPtmRatio();
    void setTimeStep(float timeStep);
    float getTimeStep();
    void slowMotion(float factor);  // empty in 1.1.3
    void slowMotionStop();          // empty in 1.1.3
    ContactListener* getContactListener();
    DestructionListener* getDestructionListener();
    GLESDebugDraw* getDebugDraw();
    bool debugDrawVisible();
    void setDebugDrawVisible(bool visible);
    bool getDrag();
    void setDrag(bool drag);
    void setIsReplay(bool isReplay);
    bool getIsReplay();
    // ONLINE (PC addition): whether update(dt) will step the world. With the browser physics
    // profile (1/30 step, online/FlashPhysics.h) not every display frame does; Gameplay::update
    // then runs the controls and the timer only on frames that step, once per step as at 1/60.
    bool onlineWillStep(float dt) { return _timeAccumulator + dt >= getTimeStep(); }

    // Fixed-step accumulator: at most one b2World::Step(step, 8, 3) per call.
    virtual void update(float dt) override;

    EmitterNode* getParticlesForeground();
    EmitterNode* getParticlesMidground();
    void pauseEmitters();
    void resumeEmitters();
    int getMaxParticles();
    bool canAddEmitter(int particleCount);  // total of the three EmitterNodes + count < 2000
    EmitterNode* getParticlesBackground();
    void setBackgroundLayer(BackgroundLayer* backgroundLayer);
    BackgroundLayer* getBackgroundLayer();
    void setControls(GameplayControls* controls);
    GameplayControls* getControls();
    void userHasDisabledGoreDuringSession();
    bool getUserHasDisabledGoreDuringSession();

protected:
    b2World* _world;                              // +0x2f8
    b2Body* _levelBody;                           // +0x300 static body: stage walls, shapes, sensors
    StageCamera* _camera;                         // +0x308 (retained)
    LevelB2D* _level;                             // +0x310
    float _version;                               // +0x318 level format version
    float _ptmRatio;                              // +0x31c pixels per meter (per SessionMode)
    float _unk0x320;                              // +0x320 RE-TODO: ctor 0.0f, never read

public:
    cocos2d::Vec2 _gravity;                       // +0x324 read directly by Burst/FlowEmitter

protected:
    SessionMode _mode;                            // +0x32c
    float _unk0x330;                              // +0x330 RE-TODO: ctor 1.0f, never read
    void* _unk0x338;                              // +0x338 RE-TODO: ctor 0, never read
    int _unk0x340;                                // +0x340 RE-TODO: ctor 0, never read
    float _timeAccumulator;                       // +0x344 not initialised by ctor/init (sic)
    bool _drag;                                   // +0x348
    bool _isReplay;                               // +0x349
    bool _userHasDisabledGoreDuringSession;       // +0x34a
    ContactListener* _contactListener;            // +0x350 owned (deleted in die())
    DestructionListener* _destructionListener;    // +0x358 owned
    GLESDebugDraw* _debugDraw;                    // +0x360 owned, never created in 1.1.3
    B2DebugDrawLayer* _debugDrawLayer;            // +0x368 child while debug draw is visible
    cocos2d::Node* _characterBackground;          // +0x370 z 6
    cocos2d::Node* _characterMidground;           // +0x378 z 8
    cocos2d::Node* _characterForeground;          // +0x380 z 11
    cocos2d::Node* _vehicleBackground;            // +0x388 z 7
    cocos2d::Node* _vehicleForeground;            // +0x390 z 12
    cocos2d::Node* _levelItemsNode;               // +0x398 z 2
    cocos2d::Node* _labelAtlasNode;               // +0x3a0 z 3
    TerrainNode* _terrainNode;                    // +0x3a8 z 13
    LevelItemsDrawNode* _backgroundDrawNode;      // +0x3b0 z 4
    LevelItemsDrawNode* _midgroundDrawNode;       // +0x3b8 z 9
    FFDrawNode* _shapesNode;                      // +0x3c0 z 1
    FFDrawNode* _foregroundShapesNode;            // +0x3c8 z 14
    SoundController* _soundController;            // +0x3d0
    EmitterNode* _particlesForeground;            // +0x3d8 z 15 (null if particles_disabled)
    EmitterNode* _particlesMidground;             // +0x3e0 z 10
    EmitterNode* _particlesBackground;            // +0x3e8 z 5
    BackgroundLayer* _backgroundLayer;            // +0x3f0
    GameplayControls* _controls;                  // +0x3f8
    cocos2d::DrawNode* _drawNode;                 // +0x400 DrawNode(2.0) at z 1000
    void* _unk0x408;                              // +0x408 RE-TODO: sizeof is 0x410, never accessed
};
