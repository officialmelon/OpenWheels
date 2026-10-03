#include "DebugScene.h"

#include "CharacterB2D.h"
#include "GameText.h"
#include "HWWindow.h"
#include "LevelB2D.h"
#include "Session.h"
#include "Settings.h"
#include "Vehicle.h"

USING_NS_CC;

// @005aacc4
Scene* DebugScene::createScene()
{
    Scene* scene = Scene::create();
    DebugScene* layer = DebugScene::create();
    scene->addChild(layer);
    return scene;
}

// @005aad6c
DebugScene::DebugScene()
    : _unk0x32c(0.0f)
    , _touchCount(0.0f)
    , _unk0x334(0.0f)
    , _unk0x338(0.0f)
    , _alertWindow(nullptr)
{
    // Both results are unused.
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    (void)visibleSize;
    (void)origin;

    addChild(LayerColor::create(Color4B::GRAY));
    scheduleUpdate();
}

// @005aae4c
void DebugScene::onEnter()
{
    Layer::onEnter();
}

// @005aae50
void DebugScene::addTouchInteractivity()
{
    _touchListener = EventListenerTouchOneByOne::create();
    _touchListener->retain();
    _touchListener->setSwallowTouches(true);
    // @005abc2c (lambda; touch() inlined)
    _touchListener->onTouchBegan = [this](Touch* touch, Event* event) {
        _touchCount += 1.0f;
        return true;
    };
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(_touchListener, 100);
}

// @005aaf1c
void DebugScene::addAlertWindowTest()
{
    _alertWindow = HWWindow::createAlertWindow("here's the NEW title", "and the body", "yes", "no", true, false, true);
    _alertWindow->addDelegate(this);
}

// @005ab0dc
void DebugScene::createEmitterTest()
{
    Settings* settings = Settings::getInstance();
    settings->killSession();
    Session* session = Session::create(1.0f, settings->getSoundController(), SessionModeGameplay);
    settings->setCurrentSession(session);
    session->createWorld();
    addChild(session, 1);

    Settings::getInstance()->setSelectedCharacterIndex(0);
    // A path, although Session::setupLevel / LevelB2D::init expect XML text in 1.1.3 (dead code).
    session->setupLevel("levels/debug/debug.xml", false);

    ValueMap characterData = Settings::getInstance()->getSelectedCharacterData();
    std::string offsetString = characterData["vehicles"].asValueVector()[0].asValueMap()["offset"].asString();

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    Vec2 offset = PointFromString(offsetString);
    Vec2 position = origin + Vec2(visibleSize.width * 0.5f / session->getPtmRatio(),
                                  visibleSize.height * 0.35f / session->getPtmRatio())
                    + offset;
    CharacterB2D* character = session->getLevel()->addCharacter(
        position.x, position.y, (CharacterId)Settings::getInstance()->getSelectedCharacterId(), VehicleIdDefault,
        false, -1);
    if (character)
    {
        Vehicle* vehicle = character->getVehicle();
        if (vehicle)
        {
            vehicle->lockWheels();
        }
    }

    session->setDebugDrawVisible(true);
    scheduleUpdate();
}

// @005ab474
void DebugScene::touch(Vec2 location)
{
    _touchCount += 1.0f;
}

// @005ab488
void DebugScene::update(float dt)
{
    if (_alertWindow)
    {
        return;
    }
    addAlertWindowTest();
}

// @005ab498
void DebugScene::createSlider()
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    ui::Slider* slider = ui::Slider::create();
    slider->loadBarTexture("cocosui/sliderTrack.png");
    slider->loadSlidBallTextures("cocosui/sliderThumb.png", "cocosui/sliderThumb.png", "");
    slider->loadProgressBarTexture("cocosui/sliderProgress.png");
    slider->setMaxPercent(10000);
    slider->setPosition(Vec2(origin.x + visibleSize.width * 0.5f, origin.y + visibleSize.height * 0.5f));
    slider->addEventListener(CC_CALLBACK_2(DebugScene::sliderEvent, this));
    addChild(slider);
}

// @005ab7c0
void DebugScene::sliderEvent(Ref* sender, ui::Slider::EventType type)
{
    if (type == ui::Slider::EventType::ON_PERCENTAGE_CHANGED)
    {
        ui::Slider* slider = dynamic_cast<ui::Slider*>(sender);
        // Both values are unused.
        int percent = slider->getPercent();
        int maxPercent = slider->getMaxPercent();
        (void)percent;
        (void)maxPercent;
    }
}

// @005ab80c (D1), @005ab850 (D0)
DebugScene::~DebugScene()
{
    _touchListener->release();
}

// @005ab874
bool DebugScene::init()
{
    return true;
}

// @005ab87c (thunk @005abbcc)
void DebugScene::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    if (buttonTag == 1)
    {
        window->showAlertMessage("new alert message 1", "bing bong", "yes", "cancel", false);
    }
    else
    {
        window->showAlertMessage("new alert message 2", OW_GAMETEXT(debugscene_oingo_boingo, 0x003f5d80), "uh huh",
                                 "nope", false);
    }
}

// @005abbd0 (thunk @005abbd4)
void DebugScene::hwWindowWasDismissed(HWWindow* window)
{
}
