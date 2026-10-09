// PAD (PC addition): QoL "controller" page, see QoLPadMenu.h.
#include "qol/QoLPadMenu.h"

#include <algorithm>

#include "Globals.h"
#include "OptionsMenuItem.h"
#include "input/Gamepad.h"
#include "qol/PadBindings.h"
#include "qol/QoL.h"
#include "qol/QoLWidgets.h"

USING_NS_CC;

namespace pad = openwheels::pad;

namespace {

const int kActions = (int)qol::PadAction::Count;
const float kCaptureTimeout = 6.0f;

enum Option
{
    OptionRumble = 9000,
    OptionTilt,
    OptionPhoneVibration,
    OptionReset,
};

}  // namespace

Scene* QoLPadMenu::createScene()
{
    Scene* scene = Scene::create();
    scene->addChild(QoLPadMenu::create());
    return scene;
}

bool QoLPadMenu::init()
{
    _title = "Controller";
    _popSceneOnExit = true;  // pushed from the QoL page
    if (!SecondaryMenu::init()) return false;
    // Esc cancels a waiting slot (ahead of the keyboard bridge, like the keyboard page).
    _keys = EventListenerKeyboard::create();
    _keys->onKeyPressed = CC_CALLBACK_2(QoLPadMenu::keyPressed, this);
    _eventDispatcher->addEventListenerWithFixedPriority(_keys, -10);
    scheduleUpdate();
    return true;
}

void QoLPadMenu::onExit()
{
    stopCapture();
    if (_keys)
    {
        _eventDispatcher->removeEventListener(_keys);
        _keys = nullptr;
    }
    SecondaryMenu::onExit();
}

std::string QoLPadMenu::optionLabel(int tag) const
{
    switch (tag)
    {
    case OptionRumble:
        return std::string("rumble: ") + qol::rumbleLevelName(qol::rumbleLevel());
    case OptionTilt:
        return std::string("tilt steering: ") + qol::tiltSteeringName(qol::tiltSteering());
    case OptionPhoneVibration:
        return std::string("phone vibration: ") + (qol::phoneVibration() ? "on" : "off");
    case OptionReset:
        return "reset to defaults";
    default:
        return "";
    }
}

void QoLPadMenu::addContent()
{
    const Size visibleSize = Director::getInstance()->getVisibleSize();

    // A table like the keyboard page's: action name, then its three slots; the options below.
    const float nameWidth = 1250.0f, slotWidth = 900.0f, gap = 50.0f, padding = 26.0f;
    const float tableWidth = nameWidth + qol::kPadSlots * (slotWidth + gap);
    Menu* menu = Menu::create();
    Node* table = Node::create();
    float rowHeight = 0.0f;
    for (int a = 0; a < kActions; a++)
    {
        for (int s = 0; s < qol::kPadSlots; s++)
        {
            QoLRowItem* slot = QoLRowItem::create("-", a * qol::kPadSlots + s, slotWidth,
                                                  CC_CALLBACK_1(QoLPadMenu::slotPressed, this),
                                                  OptionsMenuItemAppearanceDefault);
            rowHeight = slot->getContentSize().height;
            _slots.push_back(slot);
        }
    }
    for (int a = 0; a < kActions; a++)
    {
        // The menu actions get a little gap above them.
        const float extra = qol::padActionIsMenu((qol::PadAction)a) ? rowHeight * 0.5f : 0.0f;
        const float y = -a * (rowHeight + padding) - extra;
        Label* name = Label::createWithTTF(qol::padActionName((qol::PadAction)a), "fonts/ClarendonLTStd-Bold.ttf", 90.0f);
        name->setColor(globals::colors::blue);
        name->setAnchorPoint(Vec2(0.0f, 0.5f));
        name->setPosition(Vec2(-tableWidth * 0.5f, y - 10.0f));
        table->addChild(name);
        for (int s = 0; s < qol::kPadSlots; s++)
        {
            QoLRowItem* slot = _slots[a * qol::kPadSlots + s];
            const float x = -tableWidth * 0.5f + nameWidth + gap + s * (slotWidth + gap) + slotWidth * 0.5f;
            slot->setAnchorPoint(Vec2(0.5f, 0.5f));
            slot->setPosition(Vec2(x, y));
            menu->addChild(slot);
        }
    }

    // Options row below the table.
    std::vector<int> options{OptionRumble};
    if (qol::tiltSteeringSupported()) options.push_back(OptionTilt);
    if (qol::phoneVibrationSupported()) options.push_back(OptionPhoneVibration);
    options.push_back(OptionReset);
    const float optionWidth = (tableWidth - gap * (options.size() - 1)) / options.size();
    const float optionsY = -kActions * (rowHeight + padding) - rowHeight * 0.5f - padding * 2.0f;
    for (size_t i = 0; i < options.size(); i++)
    {
        const int tag = options[i];
        QoLRowItem* item = QoLRowItem::create(optionLabel(tag), tag, optionWidth,
                                              CC_CALLBACK_1(QoLPadMenu::optionPressed, this),
                                              tag == OptionReset ? OptionsMenuItemAppearanceRed
                                                                 : OptionsMenuItemAppearanceDefault);
        item->setAnchorPoint(Vec2(0.5f, 0.5f));
        item->setPosition(Vec2(-tableWidth * 0.5f + optionWidth * 0.5f + i * (optionWidth + gap), optionsY));
        menu->addChild(item);
        _options.push_back(item);
    }
    menu->setPosition(Vec2::ZERO);
    table->addChild(menu);

    const float tableHeight = -optionsY + rowHeight;
    const float top = visibleSize.height - 420.0f;  // below the title and the status line
    const float bottom = 90.0f;
    const float scale = std::min({0.62f, (visibleSize.width - 900.0f) / tableWidth, (top - bottom) / tableHeight});
    table->setScale(scale);
    // The back button sits bottom left: keep the table centred, slightly right on narrow screens.
    table->setPosition(Vec2(visibleSize.width * 0.5f + 150.0f * (1.0f - scale), top - rowHeight * 0.5f * scale));
    addChild(table, 100);

    _status = Label::createWithTTF("", "fonts/ClarendonLTStd-Bold.ttf", 50.0f);
    _status->setColor(Color3B(170, 170, 170));
    _status->setPosition(Vec2(visibleSize.width * 0.5f, top + 110.0f));
    addChild(_status, 100);
    refresh();
}

void QoLPadMenu::refresh()
{
    for (int a = 0; a < kActions; a++)
    {
        const std::vector<int> codes = qol::padInputsFor((qol::PadAction)a);
        for (int s = 0; s < qol::kPadSlots; s++)
        {
            const int index = a * qol::kPadSlots + s;
            QoLRowItem* slot = _slots[index];
            if (index == _capturing)
            {
                slot->setLabelText("press...");
                slot->setTextColor(globals::colors::pink);
            }
            else
            {
                slot->setLabelText(pad::inputName(codes[s]));
                slot->setTextColor(codes[s] == pad::kNoInput ? Color3B(120, 120, 120) : Color3B::WHITE);
            }
        }
    }
    for (OptionsMenuItem* item : _options) item->setLabelText(optionLabel(item->getTag()));
}

void QoLPadMenu::update(float dt)
{
    SecondaryMenu::update(dt);
    if (_capturing >= 0)
    {
        _captureTime += dt;
        if (_captureTime > kCaptureTimeout) stopCapture();
    }
    // The status line: what is connected, or how to bind.
    std::string text;
    if (_capturing >= 0)
    {
        text = "press the controller button, trigger or stick direction to use  -  select the slot again to clear it";
    }
    else
    {
        const std::vector<std::string> names = pad::connectedNames();
        if (names.empty())
        {
            text = "no controller connected  -  select a slot, then press an input to bind it";
        }
        else
        {
            text = "connected: ";
            for (size_t i = 0; i < names.size(); i++) text += (i ? ", " : "") + names[i];
            text += "  -  R3: free pointer";
        }
    }
    if (text != _statusText && _status)
    {
        _statusText = text;
        _status->setString(text);
    }
}

void QoLPadMenu::startCapture(int index)
{
    _capturing = index;
    _captureTime = 0.0f;
    pad::beginCapture([this](int code) {
        const int index = _capturing;
        _capturing = -1;
        if (index >= 0 && code != pad::kNoInput)
        {
            qol::setPadInput((qol::PadAction)(index / qol::kPadSlots), index % qol::kPadSlots, code);
        }
        refresh();
    });
    refresh();
}

void QoLPadMenu::stopCapture()
{
    if (_capturing < 0) return;
    pad::cancelCapture();  // calls the capture callback with kNoInput
    _capturing = -1;
    if (!_slots.empty()) refresh();
}

void QoLPadMenu::slotPressed(Ref* sender)
{
    const int index = static_cast<Node*>(sender)->getTag();
    if (_capturing == index)
    {
        // Selected again while waiting: clear the slot.
        stopCapture();
        qol::setPadInput((qol::PadAction)(index / qol::kPadSlots), index % qol::kPadSlots, pad::kNoInput);
        refresh();
        return;
    }
    stopCapture();
    startCapture(index);
}

void QoLPadMenu::optionPressed(Ref* sender)
{
    stopCapture();
    switch (static_cast<Node*>(sender)->getTag())
    {
    case OptionRumble:
    {
        const qol::RumbleLevel next = (qol::RumbleLevel)(((int)qol::rumbleLevel() + 1) % 4);
        qol::setRumbleLevel(next);
        pad::rumble(0.8f, 0.25f);  // a taste of the new strength
        break;
    }
    case OptionTilt:
        qol::setTiltSteering((qol::TiltSteering)(((int)qol::tiltSteering() + 1) % 4));
        break;
    case OptionPhoneVibration:
        qol::setPhoneVibration(!qol::phoneVibration());
        if (qol::phoneVibration()) pad::rumble(0.8f, 0.25f);
        break;
    case OptionReset:
        qol::resetPadBindings();
        break;
    default:
        return;
    }
    UserDefault::getInstance()->flush();
    refresh();
}

void QoLPadMenu::keyPressed(EventKeyboard::KeyCode key, Event* event)
{
    if (_capturing < 0 || key != EventKeyboard::KeyCode::KEY_ESCAPE || pad::syntheticInput()) return;
    event->stopPropagation();
    stopCapture();
}

void QoLPadMenu::backBtnPressed()
{
    stopCapture();
    Director::getInstance()->popScene();
}
