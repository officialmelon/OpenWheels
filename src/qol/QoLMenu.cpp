#include "qol/QoLMenu.h"

#include <algorithm>

#include "Globals.h"
#include "HWWindow.h"
#include "OptionsMenu.h"
#include "OptionsMenuItem.h"
#include "Settings.h"
#include "qol/QoL.h"

USING_NS_CC;

namespace {

const int kParticleSteps[] = {2000, 4000, 8000};
const float kZoomSteps[] = {1.0f, 0.8f, 0.65f, 0.5f};
const char* const kZoomNames[] = {"normal", "far", "farther", "farthest"};

int zoomIndex() {
    const float z = qol::cameraZoom();
    int best = 0;
    for (int i = 1; i < 4; ++i)
        if (std::fabs(kZoomSteps[i] - z) < std::fabs(kZoomSteps[best] - z)) best = i;
    return best;
}

const char* onOff(bool on) { return on ? "on" : "off"; }

Label* columnHeader(const std::string& text) {
    Label* label = Label::createWithTTF(text, "fonts/ClarendonLTStd-Bold.ttf", 80.0f);
    label->setColor(globals::colors::blue);
    return label;
}

}  // namespace

Scene* QoLMenu::createScene()
{
    Scene* scene = Scene::create();
    scene->addChild(QoLMenu::create());
    return scene;
}

bool QoLMenu::init()
{
    _title = "Quality of Life";
    _popSceneOnExit = false;  // SecondaryMenu leaves it uninitialised (the original zero-fills)
    return SecondaryMenu::init();
}

std::string QoLMenu::labelFor(int row) const
{
    switch (row)
    {
    case RowBlood: return std::string("blood: ") + qol::bloodStyleName(qol::bloodStyle());
    case RowParticles: return "max particles: " + std::to_string(qol::maxParticles());
    case RowCamera: return std::string("camera: ") + kZoomNames[zoomIndex()];
    case RowFps: return std::string("fps counter: ") + onOff(qol::showFps());
    case RowFullscreen: return std::string("fullscreen: ") + onOff(qol::fullscreen());
    case RowUnlockLevels: return std::string("unlock all levels: ") + onOff(qol::unlockAllLevels());
    case RowControls: return "keyboard controls";
    case RowChildGore:
        // The sheet is built from the player's browser-game SWF; without it the row says so.
        return std::string("child gore: ") + (qol::childGoreAvailable() ? onOff(qol::childGore()) : "no art");
    default: return std::string();
    }
}

OptionsMenuItem* QoLMenu::makeRow(int row)
{
    return OptionsMenuItem::create(labelFor(row), row, CC_CALLBACK_1(QoLMenu::rowPressed, this),
                                   row == RowControls ? OptionsMenuItemAppearanceBlue : OptionsMenuItemAppearanceDefault);
}

void QoLMenu::addContent()
{
    const Size visibleSize = Director::getInstance()->getVisibleSize();

    Menu* visuals = Menu::create(makeRow(RowBlood), makeRow(RowParticles), makeRow(RowCamera), makeRow(RowFps), nullptr);
    Menu* game = Menu::create(makeRow(RowUnlockLevels), makeRow(RowChildGore), nullptr);
    if (qol::fullscreenSupported())
    {
        game->addChild(makeRow(RowFullscreen));
        game->addChild(makeRow(RowControls));
    }
    visuals->alignItemsVerticallyWithPadding(35.0f);
    game->alignItemsVerticallyWithPadding(35.0f);

    // Two 1500-wide columns; shrink them on narrow (4:3-ish) screens.
    const float columnWidth = 1500.0f, gap = 160.0f;
    const float scale = std::min(1.0f, (visibleSize.width - 200.0f) / (2.0f * columnWidth + gap));
    const float half = (columnWidth + gap) * 0.5f * scale;
    const float rowsY = visibleSize.height * 0.5f - 60.0f;

    Node* left = Node::create();
    left->setPosition(Vec2(visibleSize.width * 0.5f - half, rowsY));
    left->setScale(scale);
    visuals->setPosition(Vec2::ZERO);
    left->addChild(visuals);
    Label* leftHeader = columnHeader("visuals");
    leftHeader->setPosition(Vec2(0.0f, visuals->getChildren().front()->getPositionY() + 210.0f));
    left->addChild(leftHeader);
    addChild(left, 100);

    Node* right = Node::create();
    right->setPosition(Vec2(visibleSize.width * 0.5f + half, rowsY));
    right->setScale(scale);
    // Top-align the shorter column with the left one.
    const float topLeft = visuals->getChildren().front()->getPositionY();
    const float topRight = game->getChildren().front()->getPositionY();
    game->setPosition(Vec2(0.0f, topLeft - topRight));
    right->addChild(game);
    Label* rightHeader = columnHeader("game");
    rightHeader->setPosition(Vec2(0.0f, topLeft + 210.0f));
    right->addChild(rightHeader);
    addChild(right, 100);
}

void QoLMenu::rowPressed(Ref* sender)
{
    auto* item = static_cast<OptionsMenuItem*>(sender);
    const int row = item->getTag();
    switch (row)
    {
    case RowBlood:
        qol::setBloodStyle((qol::BloodStyle)(((int)qol::bloodStyle() + 1) % 4));
        break;
    case RowParticles:
    {
        int i = 0;
        while (i < 3 && kParticleSteps[i] != qol::maxParticles()) ++i;
        qol::setMaxParticles(kParticleSteps[(i + 1) % 3]);
        break;
    }
    case RowCamera:
        qol::setCameraZoom(kZoomSteps[(zoomIndex() + 1) % 4]);
        break;
    case RowFps:
        qol::setShowFps(!qol::showFps());
        break;
    case RowFullscreen:
        qol::setFullscreen(!qol::fullscreen());
        break;
    case RowUnlockLevels:
        qol::setUnlockAllLevels(!qol::unlockAllLevels());
        break;
    case RowChildGore:
        if (!qol::childGoreAvailable())
        {
            HWWindow::createAlertWindow(
                "Child gore",
                "The kid's gore art is rebuilt from the browser game: put the decrypted game SWF at\n"
                "binary/flash/swf/ (with FFDec and Java) and rebuild - see\n"
                "tools/assets/extract_kid_gore.py.",
                "Ok", "", true, false, false);
            return;
        }
        qol::setChildGore(!qol::childGore());
        break;
    case RowControls:
        HWWindow::createAlertWindow(
            "Keyboard controls",
            "Up / W: accelerate\nDown / S: reverse\nLeft / A: lean back\nRight / D: lean forward\n"
            "Space: primary action\nShift / Ctrl: extra actions (restored characters)\nZ: eject\nEsc / P: pause\nR: restart level\nF11: fullscreen",
            "Ok", "", true, false, false);
        return;
    default:
        return;
    }
    UserDefault::getInstance()->flush();
    item->setLabelText(labelFor(row));
}

void QoLMenu::backBtnPressed()
{
    if (!_popSceneOnExit)
    {
        Director::getInstance()->pushScene(
            TransitionFade::create(globals::ui::menuFadeTime, OptionsMenu::createScene(), Color3B(0, 0, 0)));
    }
    else
    {
        Director::getInstance()->popScene();
    }
}
