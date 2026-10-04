#include "qol/QoLControlsMenu.h"

#include <algorithm>

#include "Globals.h"
#include "OptionsMenuItem.h"
#include "qol/KeyBindings.h"
#include "qol/QoLWidgets.h"

USING_NS_CC;

namespace {

const int kActions = (int)qol::KeyAction::Count;
const int kResetTag = 9999;

}  // namespace

Scene* QoLControlsMenu::createScene()
{
    Scene* scene = Scene::create();
    scene->addChild(QoLControlsMenu::create());
    return scene;
}

bool QoLControlsMenu::init()
{
    _title = "Controls";
    _popSceneOnExit = true;  // pushed from the QoL page
    if (!SecondaryMenu::init()) return false;
    // Ahead of the keyboard bridge (priority 1) and the fullscreen key (2) while a slot waits.
    _keys = EventListenerKeyboard::create();
    _keys->onKeyPressed = CC_CALLBACK_2(QoLControlsMenu::keyPressed, this);
    _eventDispatcher->addEventListenerWithFixedPriority(_keys, -10);
    return true;
}

void QoLControlsMenu::onExit()
{
    if (_keys)
    {
        _eventDispatcher->removeEventListener(_keys);
        _keys = nullptr;
    }
    SecondaryMenu::onExit();
}

void QoLControlsMenu::addContent()
{
    const Size visibleSize = Director::getInstance()->getVisibleSize();

    // A table: action name, then its two key slots (rows like the original Options rows, at
    // three widths), and "reset to defaults" below.
    const float nameWidth = 1500.0f, slotWidth = 1050.0f, gap = 60.0f, padding = 30.0f;
    const float tableWidth = nameWidth + 2.0f * (slotWidth + gap);
    Menu* menu = Menu::create();
    Node* table = Node::create();
    float rowHeight = 0.0f;
    for (int a = 0; a < kActions; a++)
    {
        for (int s = 0; s < qol::kKeySlots; s++)
        {
            // (A text gives the row its full height; refresh() writes the key.)
            QoLRowItem* slot = QoLRowItem::create("-", a * qol::kKeySlots + s, slotWidth,
                                                  CC_CALLBACK_1(QoLControlsMenu::slotPressed, this),
                                                  OptionsMenuItemAppearanceDefault);
            rowHeight = slot->getContentSize().height;
            _slots.push_back(slot);
        }
    }
    for (int a = 0; a < kActions; a++)
    {
        const float y = -a * (rowHeight + padding);
        Label* name = Label::createWithTTF(qol::keyActionName((qol::KeyAction)a), "fonts/ClarendonLTStd-Bold.ttf", 100.0f);
        name->setColor(globals::colors::blue);
        name->setAnchorPoint(Vec2(0.0f, 0.5f));
        name->setPosition(Vec2(-tableWidth * 0.5f, y - 10.0f));
        table->addChild(name);
        for (int s = 0; s < qol::kKeySlots; s++)
        {
            QoLRowItem* slot = _slots[a * qol::kKeySlots + s];
            const float x = -tableWidth * 0.5f + nameWidth + gap + s * (slotWidth + gap) + slotWidth * 0.5f;
            slot->setAnchorPoint(Vec2(0.5f, 0.5f));
            slot->setPosition(Vec2(x, y));
            menu->addChild(slot);
        }
    }
    OptionsMenuItem* reset = QoLRowItem::create("reset to defaults", kResetTag, nameWidth,
                                                CC_CALLBACK_1(QoLControlsMenu::resetPressed, this),
                                                OptionsMenuItemAppearanceRed);
    const float tableHeight = kActions * rowHeight + kActions * padding + rowHeight;
    reset->setPosition(Vec2(0.0f, -kActions * (rowHeight + padding) - padding));
    menu->addChild(reset);
    menu->setPosition(Vec2::ZERO);
    table->addChild(menu);

    const float top = visibleSize.height - 380.0f;  // below the title
    const float bottom = 120.0f;
    const float scale = std::min({0.7f, (visibleSize.width - 900.0f) / tableWidth, (top - bottom) / tableHeight});
    table->setScale(scale);
    // The back button sits bottom left: keep the table centred, slightly right on narrow screens.
    table->setPosition(Vec2(visibleSize.width * 0.5f + 150.0f * (1.0f - scale), top - rowHeight * 0.5f * scale));
    addChild(table, 100);

    Label* hint = Label::createWithTTF("click a key, then press the new one  -  esc cancels, backspace clears",
                                       "fonts/ClarendonLTStd-Bold.ttf", 55.0f);
    hint->setColor(Color3B(170, 170, 170));
    hint->setPosition(Vec2(visibleSize.width * 0.5f, top + 90.0f));
    addChild(hint, 100);
    refresh();
}

void QoLControlsMenu::refresh()
{
    for (int a = 0; a < kActions; a++)
    {
        const std::vector<EventKeyboard::KeyCode> keys = qol::keysFor((qol::KeyAction)a);
        for (int s = 0; s < qol::kKeySlots; s++)
        {
            const int index = a * qol::kKeySlots + s;
            QoLRowItem* slot = _slots[index];
            if (index == _capturing)
            {
                slot->setLabelText("press a key");
                slot->setTextColor(globals::colors::pink);
            }
            else
            {
                slot->setLabelText(qol::keyName(keys[s]));
                slot->setTextColor(keys[s] == EventKeyboard::KeyCode::KEY_NONE ? Color3B(120, 120, 120)
                                                                                : Color3B::WHITE);
            }
        }
    }
}

void QoLControlsMenu::slotPressed(Ref* sender)
{
    const int index = static_cast<Node*>(sender)->getTag();
    _capturing = _capturing == index ? -1 : index;  // a second click cancels
    refresh();
}

void QoLControlsMenu::resetPressed(Ref*)
{
    _capturing = -1;
    qol::resetKeyBindings();
    refresh();
}

void QoLControlsMenu::keyPressed(EventKeyboard::KeyCode key, Event* event)
{
    if (_capturing < 0)
    {
        return;
    }
    event->stopPropagation();
    const qol::KeyAction action = (qol::KeyAction)(_capturing / qol::kKeySlots);
    const int slot = _capturing % qol::kKeySlots;
    _capturing = -1;
    if (key == EventKeyboard::KeyCode::KEY_BACKSPACE || key == EventKeyboard::KeyCode::KEY_DELETE)
    {
        qol::setKey(action, slot, EventKeyboard::KeyCode::KEY_NONE);
    }
    else if (key != EventKeyboard::KeyCode::KEY_ESCAPE)
    {
        qol::setKey(action, slot, key);
    }
    refresh();
}

void QoLControlsMenu::backBtnPressed()
{
    Director::getInstance()->popScene();
}
