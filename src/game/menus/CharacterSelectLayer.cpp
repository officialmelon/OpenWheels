#include "CharacterSelectLayer.h"

#include "CharacterB2D.h"
#include "Gameplay.h"
#include "Globals.h"
#include "LevelB2D.h"
#include "MainMenu.h"
#include "MenuHelper.h"
#include "Session.h"
#include "Settings.h"
#include "SoundController.h"
#include "Tracker.h"
#include "Vehicle.h"

#include "Box2D/Box2D.h"

#include <cmath>

USING_NS_CC;

// Tracker category of the character menu (_INIT_6).
static std::string s_trackerCategory = "character_menu";

// @005a2328
CharacterSelectLayer::CharacterSelectLayer()
    : _spotlight1(nullptr)
    , _spotlight2(nullptr)
    , _scaleInc1(0.0f)
    , _rotInc1(0.0f)
    , _scaleInc2(0.0f)
    , _rotInc2(0.0f)
    , _unk348(0)
    , _lastBtn(nullptr)
    , _currentID(-1)
    , _selectedCharacterName()
    , _confirmed(false)
    , _characterBtnPressedThisFrame(false)
{
    _selectedCharacterName = "";
}

// @005a23c0 (D1), @005a2400 (D0)
CharacterSelectLayer::~CharacterSelectLayer()
{
}

// @005a2424
Scene* CharacterSelectLayer::createScene(int unused1, unsigned long unused2)
{
    Scene* scene = Scene::create();
    CharacterSelectLayer* layer = new (std::nothrow) CharacterSelectLayer();
    layer->init(unused1, unused2);
    layer->autorelease();
    scene->addChild(layer);
    return scene;
}

// @005a24a0
bool CharacterSelectLayer::init(int unused1, unsigned long unused2)
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA8888);
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("menus/character_select/character_select.plist");
    MenuHelper::addBg(this, 0);

    _spotlight1 = Sprite::createWithSpriteFrameName("spotlight.png");
    _spotlight1->setAnchorPoint(Vec2(0.5f, -0.1f));
    _spotlight1->setPosition(Vec2(origin.x + visibleSize.width * 0.5f - 750.0f, origin.y - 300.0f));
    _spotlight1->setOpacity(135);
    _spotlight1->setBlendFunc(BlendFunc{GL_ONE, GL_ONE});

    _spotlight2 = Sprite::createWithSpriteFrameName("spotlight.png");
    _spotlight2->setAnchorPoint(Vec2(0.5f, -0.1f));
    _spotlight2->setPosition(Vec2(origin.x + visibleSize.width * 0.5f + 750.0f, origin.y - 300.0f));
    _spotlight2->setOpacity(135);
    _spotlight2->setBlendFunc(BlendFunc{GL_ONE, GL_ONE});

    addChild(_spotlight1);
    addChild(_spotlight2);

    Sprite* platform = Sprite::createWithSpriteFrameName("charSel_platform.png");
    platform->setAnchorPoint(Vec2(0.5f, 0.0f));
    platform->setPosition(Vec2(visibleSize.width * 0.5f, -375.0f));
    addChild(platform);

    Settings::getInstance()->getSoundController()->playSound("DrumLong");

    addCharacterMenu();
    MenuHelper::addBackBtn(this, 500, CC_CALLBACK_0(CharacterSelectLayer::handleBackButtonReleased, this));
    MenuHelper::addConfirmBtn(this, 501, CC_CALLBACK_0(CharacterSelectLayer::handleConfirmButtonReleased, this));

    _scaleInc1 = 0.0f;
    _rotInc1 = 0.0f;
    _scaleInc2 = 3.14f;
    _rotInc2 = 3.14f;

    // Not restored: everything loaded afterwards defaults to RGBA4444.
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA4444);
    scheduleUpdate();
    return true;
}

// @005a29a8
void CharacterSelectLayer::onEnterTransitionDidFinish()
{
    Layer::onEnterTransitionDidFinish();
}

// @005a29ac
void CharacterSelectLayer::onEnter()
{
    SpriteFrameCache::getInstance()->removeUnusedSpriteFrames();
    int index = Settings::getInstance()->getSelectedCharacterIndex();
    MenuItem* btn = static_cast<MenuItem*>(_vehicleMenu->getChildByTag(index));
    btn->selected();
    _lastBtn = btn;
    _glowFrame->setPosition(btn->getPosition());
    selectCharacter(index, false);
    Layer::onEnter();
}

// @005a2a48
void CharacterSelectLayer::selectCharacter(int index, bool trackPreview)
{
    if (_currentID == index)
    {
        return;
    }

    Settings* settings = Settings::getInstance();
    settings->killSession();
    Session* session = Session::create(1.0f, settings->getSoundController(), SessionModeCharacterSelect);
    settings->setCurrentSession(session);
    session->createWorld();
    addChild(session, 1);
    createPlatform();

    _currentID = index;
    Settings::getInstance()->setSelectedCharacterIndex(_currentID);
    session->setupLevelForCharacterSelect();

    ValueMap characterData = Settings::getInstance()->getSelectedCharacterData();
    if (trackPreview)
    {
        _selectedCharacterName = characterData["name"].asString();
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "character_previewed",
                                                            _selectedCharacterName, -1);
    }

    std::string offsetString = characterData["vehicles"].asValueVector()[0].asValueMap()["offset"].asString();
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    Vec2 position = PointFromString(offsetString) +
                    Vec2(origin.x + visibleSize.width * 0.5f / session->getPtmRatio(),
                         origin.y + visibleSize.height * 0.35f / session->getPtmRatio());
    CharacterId characterId = (CharacterId)Settings::getInstance()->getSelectedCharacterId();
    LevelB2D* level = session->getLevel();
    CharacterB2D* character = level->addCharacter(position.x, position.y, characterId, VehicleIdDefault, false, -1);
    if (character != nullptr)
    {
        Vehicle* vehicle = character->getVehicle();
        if (vehicle != nullptr)
        {
            vehicle->lockWheels();
        }
    }
}

// @005a2f3c
void CharacterSelectLayer::onExitTransitionDidStart()
{
    Layer::onExitTransitionDidStart();
}

// @005a2f40
void CharacterSelectLayer::addCharacterMenu()
{
    ValueVector characters = Settings::getInstance()->getAllCharactersData();
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    ValueMap characterData;

    Node* framesNode = Node::create();
    addChild(framesNode, 1000);

    _vehicleMenu = Menu::create();
    _vehicleMenu->setPosition(origin + Vec2::ZERO);
    addChild(_vehicleMenu);

    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA8888);
    _glowFrame = Sprite::createWithSpriteFrameName("charSel_frame_highlight.png");
    _glowFrame->setBlendFunc(BlendFunc{GL_SRC_ALPHA, GL_ONE});
    addChild(_glowFrame);
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA4444);

    // 13 slots: rows of 7, 4 (+ gap) and 2 (+ gap), 350 px apart.
    float y = visibleSize.height - 125.0f - 100.0f;
    int startX = (int)(visibleSize.width * 0.5f - 1175.0f + 125.0f);
    int x = startX;
    for (int i = 0; i < 13; i++)
    {
        if (i < characters.size())
        {
            characterData = characters[i].asValueMap();
            Sprite* frame = Sprite::createWithSpriteFrameName("charSel_frame.png");
            std::string key = characterData["key"].asString();
            MenuItemSprite* btn = createBtn(key, i);
            _vehicleMenu->addChild(btn);
            btn->setPosition((float)x, y);
            frame->setPosition((float)x, y);
            framesNode->addChild(frame);
        }
        else
        {
            Sprite* frame = Sprite::createWithSpriteFrameName("charSel_frame.png");
            frame->setPosition(Vec2((float)x, y));
            framesNode->addChild(frame);
        }

        x = (int)((float)x + 350.0f);
        switch (i)
        {
        case 6:
        case 10:
            y += -350.0f;
            x = startX;
            break;
        case 8:
            x = (int)((float)x + 1050.0f);
            break;
        case 11:
            x = (int)((float)x + 1750.0f);
            break;
        }
    }
}

// @005a3508
void CharacterSelectLayer::handleBackButtonReleased()
{
    Settings::getInstance()->killSession();
    removeUnusedTexturesAndSpriteFrames();
    Scene* scene = MainMenu::createScene(MenuModeLevelSelect, nullptr);
    Director::getInstance()->replaceScene(TransitionFade::create(globals::ui::menuFadeTime, scene, Color3B(0, 0, 0)));
}

// @005a35c8
void CharacterSelectLayer::handleConfirmButtonReleased()
{
    if (_characterBtnPressedThisFrame)
    {
        return;
    }

    _confirmed = true;
    unscheduleUpdate();
    Settings::getInstance()->killSession();
    removeUnusedTexturesAndSpriteFrames();
    std::string levelPath = Settings::getInstance()->getSelectedLevelFilePath();
    Director::getInstance()->replaceScene(TransitionFade::create(
        globals::ui::menuFadeTime, Gameplay::createScene(levelPath, nullptr), Color3B(0, 0, 0)));
    Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "character_selected",
                                                        _selectedCharacterName, -1);
}

// @005a381c
void CharacterSelectLayer::removeUnusedTexturesAndSpriteFrames()
{
    SpriteFrameCache::getInstance()->removeUnusedSpriteFrames();
    Director::getInstance()->getTextureCache()->removeUnusedTextures();
}

// @005a383c
void CharacterSelectLayer::update(float dt)
{
    adjustSpotlights();

    Session* session = Settings::getInstance()->getCurrentSession();
    if (session != nullptr)
    {
        session->update(dt);
        std::vector<CharacterB2D*> characters = session->getLevel()->getCharacters();
        for (unsigned int i = 0; i < characters.size(); i++)
        {
            characters[i]->setState(0);
        }
    }
    _characterBtnPressedThisFrame = false;
}

// @005a3a3c
void CharacterSelectLayer::adjustSpotlights()
{
    _spotlight1->setScaleY(sinf(_scaleInc1) * 0.25f + 1.25f);
    _scaleInc1 += 0.01f;
    _spotlight1->setRotation(sinf(_rotInc1) * 15.0f + 10.0f);
    _rotInc1 += 0.02f;
    _spotlight2->setScaleY(cosf(_scaleInc2) * 0.25f + 1.25f);
    _scaleInc2 += -0.012f;
    _spotlight2->setRotation(cosf(_rotInc2) * 15.0f + -10.0f);
    _rotInc2 += -0.02f;
}

// @005a3b5c
MenuItemSprite* CharacterSelectLayer::createBtn(std::string key, int tag)
{
    if (key.length() == 0)
    {
        key = "biz";
    }
    std::string normalFrame = key + "_icon_bw.png";
    std::string selectedFrame = key + "_icon.png";

    MenuItemImage* btn =
        MenuItemImage::create("", "", CC_CALLBACK_1(CharacterSelectLayer::characterBtnPressed, this));
    Sprite* normal = Sprite::createWithSpriteFrameName(normalFrame);
    Sprite* selected = Sprite::createWithSpriteFrameName(selectedFrame);
    btn->setNormalImage(normal);
    btn->setSelectedImage(selected);
    btn->setTag(tag);
    return btn;
}

// @005a3f88
void CharacterSelectLayer::characterBtnPressed(Ref* sender)
{
    if (_confirmed)
    {
        return;
    }

    _characterBtnPressedThisFrame = true;
    if (_lastBtn != nullptr)
    {
        _lastBtn->unselected();
    }
    MenuItem* btn = static_cast<MenuItem*>(sender);
    int index = btn->getTag();
    _lastBtn = btn;
    _lastBtn->selected();
    _glowFrame->setPosition(btn->getPosition());
    _glowFrame->stopAllActions();
    _glowFrame->setOpacity(0);
    _glowFrame->runAction(FadeIn::create(0.25f));
    Settings::getInstance()->getSoundController()->playSound("SelectCharacter");
    selectCharacter(index, true);
}

// @005a411c
void CharacterSelectLayer::killCharacter()
{
}

// @005a4120
void CharacterSelectLayer::createPlatform()
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    (void)origin;
    float ptm = Settings::getInstance()->getCurrentSession()->getPtmRatio();

    b2BodyDef bodyDef;
    bodyDef.position.Set(visibleSize.width * 0.5f / ptm, 388.0f / ptm);

    b2PolygonShape box;
    box.SetAsBox(810.0f / ptm, 125.0f / ptm);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &box;
    fixtureDef.friction = 0.2f;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xffff;
    fixtureDef.filter.groupIndex = -10;

    b2Body* body = Settings::getInstance()->getCurrentSession()->getWorld()->CreateBody(&bodyDef);
    body->CreateFixture(&fixtureDef);
    body->ResetMassData();
}
