#include "qol/QoLMenu.h"

#include <algorithm>
#include <cmath>

#include "Globals.h"
#include "HWWindow.h"
#include "OptionsMenu.h"
#include "OptionsMenuItem.h"
#include "Settings.h"
#include "qol/QoL.h"
#include "qol/CharacterChoice.h"
#include "qol/QoLControlsMenu.h"
#include "qol/QoLPadMenu.h"  // PAD (PC addition)
#include "qol/QoLWidgets.h"
#include "online/FlashPhysics.h"

USING_NS_CC;

namespace {

const int kParticleSteps[] = {2000, 4000, 8000};
const float kZoomSteps[] = {1.0f, 0.8f, 0.65f, 0.5f};
const char* const kZoomNames[] = {"normal", "far", "farther", "farthest"};
// "" = auto (the original's choice), then the four asset tiers.
const char* const kTiers[] = {"", "large", "medium", "small", "tiny"};

int zoomIndex() {
    const float z = qol::cameraZoom();
    int best = 0;
    for (int i = 1; i < 4; ++i)
        if (std::fabs(kZoomSteps[i] - z) < std::fabs(kZoomSteps[best] - z)) best = i;
    return best;
}

const char* onOff(bool on) { return on ? "on" : "off"; }

std::string percent(float value) { return std::to_string((int)std::lround(value * 100.0f)) + "%"; }

Label* columnHeader(const std::string& text) {
    Label* label = Label::createWithTTF(text, "fonts/ClarendonLTStd-Bold.ttf", 80.0f);
    label->setColor(globals::colors::blue);
    return label;
}

// The tier the game would pick on its own is not known once a tier is forced: the row shows the
// running tier, and flags a choice that needs a restart.
std::string texturesLabel() {
    const std::string chosen = qol::textureTier();
    std::string text = "textures: " + (chosen.empty() ? "auto (" + qol::runningAssetTier() + ")" : chosen);
    if (!chosen.empty() && chosen != qol::runningAssetTier()) text += " - restart";
    return text;
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
    case RowController: return "controller";
    case RowChildGore:
        // The sheet is built from the player's browser-game SWF; without it the row says so.
        return std::string("child gore: ") + (qol::childGoreAvailable() ? onOff(qol::childGore()) : "no art");
    case RowTextures: return texturesLabel();
    case RowFrameRate: return "frame rate: " + std::to_string(qol::frameRate()) + " fps";
    case RowTouchControls: return std::string("touch controls: ") + qol::touchControlsName(qol::touchControls());
    case RowRegrabVehicle: return std::string("re-grab vehicle: ") + onOff(qol::regrabVehicleSetting());
    case RowAnyCharacter: return std::string("any character: ") + onOff(qol::anyCharacterOnForcedLevels());
    case RowBrowserPhysics: return std::string("browser physics (online levels): ") + onOff(online::browserPhysicsOption());
    default: return std::string();
    }
}

OptionsMenuItem* QoLMenu::makeRow(int row)
{
    return OptionsMenuItem::create(labelFor(row), row, CC_CALLBACK_1(QoLMenu::rowPressed, this),
                                   row == RowControls || row == RowController ? OptionsMenuItemAppearanceBlue
                                                                              : OptionsMenuItemAppearanceDefault);
}

void QoLMenu::addContent()
{
    const Size visibleSize = Director::getInstance()->getVisibleSize();

    Menu* visuals = Menu::create(makeRow(RowBlood), makeRow(RowParticles), makeRow(RowCamera),
                                 makeRow(RowTextures), makeRow(RowFrameRate), makeRow(RowFps), nullptr);
    Menu* game = Menu::create();
    game->addChild(QoLSliderItem::create([](float v) { return "sound effects: " + percent(v); }, qol::effectsVolume,
                                         qol::setEffectsVolume));
    game->addChild(QoLSliderItem::create([](float v) { return "music: " + percent(v); }, qol::musicVolume,
                                         qol::setMusicVolume));
    game->addChild(makeRow(RowUnlockLevels));
    game->addChild(makeRow(RowAnyCharacter));  // user / online levels that force a character
    game->addChild(makeRow(RowBrowserPhysics));  // the next browser level steps at 1/30 (FlashPhysics.h)
    game->addChild(makeRow(RowChildGore));
    game->addChild(makeRow(RowRegrabVehicle));
    if (qol::fullscreenSupported())
    {
        game->addChild(makeRow(RowFullscreen));
    }
    game->addChild(makeRow(RowTouchControls));  // PAD (PC addition): touch devices too (controllers)
    if (qol::desktopBuild())
    {
        game->addChild(makeRow(RowControls));  // the keyboard bridge is desktop-only
    }
    game->addChild(makeRow(RowController));  // PAD (PC addition): every platform
    const float padding = 35.0f;
    visuals->alignItemsVerticallyWithPadding(padding);
    game->alignItemsVerticallyWithPadding(padding);

    // Two 1500-wide columns of rows between the title and the back button; shrink them
    // on narrow (4:3-ish) or short screens.
    const float columnWidth = 1500.0f, gap = 160.0f, headerGap = 210.0f;
    const float rowHeight = visuals->getChildren().front()->getContentSize().height;
    const size_t rows = std::max(visuals->getChildren().size(), game->getChildren().size());
    const float columnHeight = headerGap + rows * rowHeight + (rows - 1) * padding;
    const float top = visibleSize.height - 380.0f;  // below the title
    const float bottom = 420.0f;                     // above the back button
    const float scale = std::min({1.0f, (visibleSize.width - 200.0f) / (2.0f * columnWidth + gap),
                                  (top - bottom) / columnHeight});
    const float half = (columnWidth + gap) * 0.5f * scale;
    // Column origin: the first row's centre sits headerGap + rowHeight / 2 below the header.
    const float firstRowY = top - (headerGap + 40.0f + rowHeight * 0.5f) * scale;

    auto placeColumn = [&](Menu* menu, const std::string& header, float x) {
        Node* column = Node::create();
        column->setScale(scale);
        const float firstY = menu->getChildren().front()->getPositionY();
        column->setPosition(Vec2(x, firstRowY));
        menu->setPosition(Vec2(0.0f, -firstY));
        column->addChild(menu);
        Label* label = columnHeader(header);
        label->setPosition(Vec2(0.0f, headerGap));
        column->addChild(label);
        addChild(column, 100);
    };
    placeColumn(visuals, "visuals", visibleSize.width * 0.5f - half);
    placeColumn(game, "game", visibleSize.width * 0.5f + half);
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
    case RowTextures:
    {
        int i = 0;
        while (i < 5 && qol::textureTier() != kTiers[i]) ++i;
        qol::setTextureTier(kTiers[(i + 1) % 5]);
        break;
    }
    case RowFrameRate:
        qol::setFrameRate(qol::frameRate() == 60 ? 30 : 60);
        break;
    case RowTouchControls:
        qol::setTouchControls((qol::TouchControls)(((int)qol::touchControls() + 1) % 3));
        break;
    case RowRegrabVehicle:
        qol::setRegrabVehicle(!qol::regrabVehicleSetting());
        break;
    case RowAnyCharacter:
        qol::setAnyCharacterOnForcedLevels(!qol::anyCharacterOnForcedLevels());
        break;
    case RowBrowserPhysics:
        online::setBrowserPhysicsOption(!online::browserPhysicsOption());
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
    {
        // Pushed, so the back button returns here.
        Director::getInstance()->pushScene(
            TransitionFade::create(globals::ui::menuFadeTime, QoLControlsMenu::createScene(), Color3B(0, 0, 0)));
        return;
    }
    case RowController:
    {
        Director::getInstance()->pushScene(
            TransitionFade::create(globals::ui::menuFadeTime, QoLPadMenu::createScene(), Color3B(0, 0, 0)));
        return;
    }
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
