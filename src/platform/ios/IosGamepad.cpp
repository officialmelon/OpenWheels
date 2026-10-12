// PAD (PC addition): game controllers and tilt steering on iOS (see src/input/Gamepad.h).
//
// Controllers: cocos2d-x's Controller (GCController: MFi, Xbox, PlayStation and other pads iOS
// supports) reports the extended gamepad with the standard controls; it is polled every frame.
// The pause / menu button only sends a press-and-release, held here for two frames so a poll
// sees it. No rumble (GCController haptics are not wrapped by cocos2d-x 3.17).
// Tilt: cocos2d-x's accelerometer (already rotated to the interface orientation).
#include <cmath>

#include "cocos2d.h"
#include "base/CCController.h"
#include "base/CCEventListenerController.h"

#include "input/Gamepad.h"
#include "input/Tilt.h"

USING_NS_CC;

namespace openwheels {
namespace pad {
namespace {

int g_pauseFrames = 0;

float positive(float v) { return v > 0.0f ? std::fmin(1.0f, v) : 0.0f; }
float negative(float v) { return v < 0.0f ? std::fmin(1.0f, -v) : 0.0f; }

void poll(std::vector<State>& out)
{
    for (Controller* controller : Controller::getAllController())
    {
        if (!controller || !controller->isConnected()) continue;
        State s;
        s.id = controller->getDeviceId();
        s.name = controller->getDeviceName().empty() ? "controller" : controller->getDeviceName();
        auto button = [controller](int key) {
            const Controller::KeyStatus& k = controller->getKeyStatus(key);
            return k.isPressed ? std::fmax(0.5f, k.value) : 0.0f;
        };
        auto axis = [controller](int key) { return controller->getKeyStatus(key).value; };
        float* c = s.control;
        c[kA] = button(Controller::Key::BUTTON_A);
        c[kB] = button(Controller::Key::BUTTON_B);
        c[kX] = button(Controller::Key::BUTTON_X);
        c[kY] = button(Controller::Key::BUTTON_Y);
        c[kLB] = button(Controller::Key::BUTTON_LEFT_SHOULDER);
        c[kRB] = button(Controller::Key::BUTTON_RIGHT_SHOULDER);
        c[kLT] = std::fmax(0.0f, axis(Controller::Key::AXIS_LEFT_TRIGGER));
        c[kRT] = std::fmax(0.0f, axis(Controller::Key::AXIS_RIGHT_TRIGGER));
        c[kBack] = button(Controller::Key::BUTTON_SELECT);
        c[kStart] = std::fmax(button(Controller::Key::BUTTON_START), g_pauseFrames > 0 ? 1.0f : 0.0f);
        c[kLS] = button(Controller::Key::BUTTON_LEFT_THUMBSTICK);
        c[kRS] = button(Controller::Key::BUTTON_RIGHT_THUMBSTICK);
        c[kDUp] = button(Controller::Key::BUTTON_DPAD_UP);
        c[kDDown] = button(Controller::Key::BUTTON_DPAD_DOWN);
        c[kDLeft] = button(Controller::Key::BUTTON_DPAD_LEFT);
        c[kDRight] = button(Controller::Key::BUTTON_DPAD_RIGHT);
        // cocos2d-x flips the y axes: down = positive.
        const float lx = axis(Controller::Key::JOYSTICK_LEFT_X), ly = axis(Controller::Key::JOYSTICK_LEFT_Y);
        const float rx = axis(Controller::Key::JOYSTICK_RIGHT_X), ry = axis(Controller::Key::JOYSTICK_RIGHT_Y);
        c[kLUp] = negative(ly);
        c[kLDown] = positive(ly);
        c[kLLeft] = negative(lx);
        c[kLRight] = positive(lx);
        c[kRUp] = negative(ry);
        c[kRDown] = positive(ry);
        c[kRLeft] = negative(rx);
        c[kRRight] = positive(rx);
        out.push_back(s);
    }
    if (g_pauseFrames > 0) g_pauseFrames--;
}

}  // namespace

void installPlatformProvider()
{
    Controller::startDiscoveryController();
    auto listener = EventListenerController::create();
    listener->onKeyDown = [](Controller*, int key, Event*) {
        if (key == Controller::Key::BUTTON_PAUSE) g_pauseFrames = 2;
    };
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(listener, 1);
    setPoll(poll);
}

}  // namespace pad

namespace tilt {

void enableSensor(bool on)
{
    static EventListenerAcceleration* s_listener = nullptr;
    if (on && !s_listener)
    {
        // Held level the reading points down the screen (y = -1); turned counter-clockwise it
        // swings to -x: roll = atan2(-x, -y).
        s_listener = EventListenerAcceleration::create([](Acceleration* a, Event*) {
            setRoll((float)std::atan2(-a->x, -a->y));
        });
        Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(s_listener, 1);
        Device::setAccelerometerInterval(1.0f / 60.0f);
    }
    else if (!on && s_listener)
    {
        Director::getInstance()->getEventDispatcher()->removeEventListener(s_listener);
        s_listener = nullptr;
    }
    Device::setAccelerometerEnabled(on);
}

}  // namespace tilt
}  // namespace openwheels
