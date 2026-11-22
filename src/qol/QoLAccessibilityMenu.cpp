#include "qol/QoLAccessibilityMenu.h"

#include <algorithm>

#include "Globals.h"
#include "OptionsMenuItem.h"
#include "input/Gamepad.h"
#include "qol/QoL.h"
#include "qol/QoLWidgets.h"

USING_NS_CC;

namespace {

const char* onOff(bool on) { return on ? "on" : "off"; }

std::string percent(float value) { return std::to_string((int)std::lround(value * 100.0f)) + "%"; }

}  // namespace

Scene* QoLAccessibilityMenu::createScene()
{
    Scene* scene = Scene::create();
    scene->addChild(QoLAccessibilityMenu::create());
    return scene;
}

bool QoLAccessibilityMenu::init()
{
    _title = "Accessibility";
    _popSceneOnExit = true;  // pushed from the QoL page
    return SecondaryMenu::init();
}

std::string QoLAccessibilityMenu::labelFor(int row) const
{
    switch (row)
    {
    case RowColorFilter: return std::string("colour filter: ") + qol::colorFilterName(qol::colorFilter());
    case RowTouchControls: return std::string("touch controls: ") + qol::touchControlsName(qol::touchControls());
    case RowReduceShake: return std::string("reduce screen shake: ") + onOff(qol::reduceShake());
    case RowHideTimer: return std::string("timer: ") + (qol::hideTimer() ? "hidden" : "shown");
    case RowToughness: return std::string("character toughness: ") + qol::toughnessName(qol::characterToughness());
    case RowRumble: return std::string("controller rumble: ") + qol::rumbleLevelName(qol::rumbleLevel());
    case RowPhoneVibration: return std::string("phone vibration: ") + onOff(qol::phoneVibration());
    default: return std::string();
    }
}

OptionsMenuItem* QoLAccessibilityMenu::makeRow(int row)
{
    return OptionsMenuItem::create(labelFor(row), row, CC_CALLBACK_1(QoLAccessibilityMenu::rowPressed, this),
                                   OptionsMenuItemAppearanceDefault);
}

void QoLAccessibilityMenu::addContent()
{
    const Size visibleSize = Director::getInstance()->getVisibleSize();

    Menu* menu = Menu::create();
    menu->addChild(makeRow(RowColorFilter));
    menu->addChild(makeRow(RowTouchControls));
    menu->addChild(QoLSliderItem::create([](float v) { return "touch button opacity: " + percent(v); },
                                         qol::touchButtonOpacity, qol::setTouchButtonOpacity));
    menu->addChild(makeRow(RowReduceShake));
    menu->addChild(makeRow(RowHideTimer));
    menu->addChild(makeRow(RowToughness));
    menu->addChild(makeRow(RowRumble));
    if (qol::phoneVibrationSupported())
    {
        menu->addChild(makeRow(RowPhoneVibration));
    }
    const float padding = 35.0f;
    menu->alignItemsVerticallyWithPadding(padding);

    // Between the title and the back button; shrink on short screens.
    const float rowHeight = menu->getChildren().front()->getContentSize().height;
    const size_t rows = menu->getChildrenCount();
    const float height = rows * rowHeight + (rows - 1) * padding;
    const float top = visibleSize.height - 380.0f;
    const float bottom = 420.0f;
    const float scale = std::min(1.0f, (top - bottom) / height);
    menu->setScale(scale);
    menu->setPosition(Vec2(visibleSize.width * 0.5f, (top + bottom) * 0.5f));
    addChild(menu, 100);
}

void QoLAccessibilityMenu::rowPressed(Ref* sender)
{
    auto* item = static_cast<OptionsMenuItem*>(sender);
    const int row = item->getTag();
    switch (row)
    {
    case RowColorFilter:
        qol::setColorFilter((qol::ColorFilter)(((int)qol::colorFilter() + 1) % 5));
        break;
    case RowTouchControls:
        qol::setTouchControls((qol::TouchControls)(((int)qol::touchControls() + 1) % 3));
        break;
    case RowReduceShake:
        qol::setReduceShake(!qol::reduceShake());
        break;
    case RowHideTimer:
        qol::setHideTimer(!qol::hideTimer());
        break;
    case RowToughness:
        qol::setCharacterToughness((qol::Toughness)(((int)qol::characterToughness() + 1) % 4));
        break;
    case RowRumble:
        qol::setRumbleLevel((qol::RumbleLevel)(((int)qol::rumbleLevel() + 1) % 4));
        openwheels::pad::rumble(0.8f, 0.25f);  // a taste of the new strength
        break;
    case RowPhoneVibration:
        qol::setPhoneVibration(!qol::phoneVibration());
        if (qol::phoneVibration()) openwheels::pad::rumble(0.8f, 0.25f);
        break;
    default:
        return;
    }
    UserDefault::getInstance()->flush();
    item->setLabelText(labelFor(row));
}

void QoLAccessibilityMenu::backBtnPressed()
{
    Director::getInstance()->popScene();
}
