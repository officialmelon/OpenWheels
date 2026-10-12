#include "qol/KeyBindings.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

#include "cocos2d.h"

#include "Globals.h"
#include "input/Gamepad.h"  // PAD (PC addition)
#include "qol/PadBindings.h"  // PAD (PC addition)
#include "qol/QoL.h"

USING_NS_CC;

namespace qol {
namespace {

using K = EventKeyboard::KeyCode;

const int kActionCount = (int)KeyAction::Count;

struct Slots {
    K key[kKeySlots];
};

// The keyboard bridge's original keys.
Slots defaults(KeyAction action)
{
    switch (action) {
    case KeyAction::Accelerate: return {{K::KEY_UP_ARROW, K::KEY_W}};
    case KeyAction::Reverse: return {{K::KEY_DOWN_ARROW, K::KEY_S}};
    case KeyAction::LeanBack: return {{K::KEY_LEFT_ARROW, K::KEY_A}};
    case KeyAction::LeanForward: return {{K::KEY_RIGHT_ARROW, K::KEY_D}};
    case KeyAction::Special: return {{K::KEY_SPACE, K::KEY_NONE}};
    case KeyAction::Shift: return {{K::KEY_LEFT_SHIFT, K::KEY_RIGHT_SHIFT}};
    case KeyAction::Ctrl: return {{K::KEY_LEFT_CTRL, K::KEY_RIGHT_CTRL}};
    case KeyAction::Eject: return {{K::KEY_Z, K::KEY_NONE}};
    case KeyAction::Pause: return {{K::KEY_ESCAPE, K::KEY_P}};
    case KeyAction::Restart: return {{K::KEY_R, K::KEY_NONE}};
    case KeyAction::Fullscreen: return {{K::KEY_F11, K::KEY_NONE}};
    default: return {{K::KEY_NONE, K::KEY_NONE}};
    }
}

std::string storeKey(int action) { return "qol_keys_" + std::to_string(action); }

Slots* table()
{
    static Slots slots[kActionCount];
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        UserDefault* store = UserDefault::getInstance();
        for (int a = 0; a < kActionCount; a++) {
            slots[a] = defaults((KeyAction)a);
            const std::string saved = store->getStringForKey(storeKey(a).c_str(), "");
            if (saved.empty()) continue;
            std::istringstream in(saved);
            std::string item;
            for (int s = 0; s < kKeySlots; s++) {
                slots[a].key[s] = K::KEY_NONE;
                if (std::getline(in, item, ',')) slots[a].key[s] = (K)std::atoi(item.c_str());
            }
        }
    }
    return slots;
}

void save()
{
    UserDefault* store = UserDefault::getInstance();
    Slots* slots = table();
    for (int a = 0; a < kActionCount; a++) {
        bool isDefault = true;
        const Slots d = defaults((KeyAction)a);
        std::string text;
        for (int s = 0; s < kKeySlots; s++) {
            isDefault = isDefault && slots[a].key[s] == d.key[s];
            text += (s ? "," : "") + std::to_string((int)slots[a].key[s]);
        }
        if (isDefault) {
            store->deleteValueForKey(storeKey(a).c_str());
        } else {
            store->setStringForKey(storeKey(a).c_str(), text);
        }
    }
    store->flush();
}

}  // namespace

const char* keyActionName(KeyAction action)
{
    switch (action) {
    case KeyAction::Accelerate: return "accelerate";
    case KeyAction::Reverse: return "brake / reverse";
    case KeyAction::LeanBack: return "lean back";
    case KeyAction::LeanForward: return "lean forward";
    case KeyAction::Special: return "primary action";
    case KeyAction::Shift: return "extra action 1";
    case KeyAction::Ctrl: return "extra action 2";
    case KeyAction::Eject: return "eject";
    case KeyAction::Pause: return "pause";
    case KeyAction::Restart: return "restart";
    case KeyAction::Fullscreen: return "fullscreen";
    default: return "";
    }
}

std::vector<K> keysFor(KeyAction action)
{
    const Slots& s = table()[(int)action];
    return std::vector<K>(s.key, s.key + kKeySlots);
}

void setKey(KeyAction action, int slot, K key)
{
    if (slot < 0 || slot >= kKeySlots) return;
    Slots* slots = table();
    if (key != K::KEY_NONE) {
        for (int a = 0; a < kActionCount; a++) {
            for (int s = 0; s < kKeySlots; s++) {
                if (slots[a].key[s] == key) slots[a].key[s] = K::KEY_NONE;
            }
        }
    }
    slots[(int)action].key[slot] = key;
    save();
}

void resetKeyBindings()
{
    Slots* slots = table();
    for (int a = 0; a < kActionCount; a++) slots[a] = defaults((KeyAction)a);
    save();
}

bool actionForKey(K key, KeyAction* action)
{
    if (key == K::KEY_NONE) return false;
    Slots* slots = table();
    for (int a = 0; a < kActionCount; a++) {
        for (int s = 0; s < kKeySlots; s++) {
            if (slots[a].key[s] == key) {
                *action = (KeyAction)a;
                return true;
            }
        }
    }
    return false;
}

bool keyIs(K key, KeyAction action)
{
    KeyAction bound;
    return actionForKey(key, &bound) && bound == action;
}

std::string keyName(K key)
{
    const int k = (int)key;
    if (k >= (int)K::KEY_A && k <= (int)K::KEY_Z) return std::string(1, (char)('a' + k - (int)K::KEY_A));
    if (k >= (int)K::KEY_CAPITAL_A && k <= (int)K::KEY_CAPITAL_Z)
        return std::string(1, (char)('a' + k - (int)K::KEY_CAPITAL_A));
    if (k >= (int)K::KEY_0 && k <= (int)K::KEY_9) return std::string(1, (char)('0' + k - (int)K::KEY_0));
    if (k >= (int)K::KEY_F1 && k <= (int)K::KEY_F12) return "f" + std::to_string(1 + k - (int)K::KEY_F1);
    switch (key) {
    case K::KEY_NONE: return "-";
    case K::KEY_UP_ARROW: return "up";
    case K::KEY_DOWN_ARROW: return "down";
    case K::KEY_LEFT_ARROW: return "left";
    case K::KEY_RIGHT_ARROW: return "right";
    case K::KEY_SPACE: return "space";
    case K::KEY_LEFT_SHIFT: return "left shift";
    case K::KEY_RIGHT_SHIFT: return "right shift";
    case K::KEY_LEFT_CTRL: return "left ctrl";
    case K::KEY_RIGHT_CTRL: return "right ctrl";
    case K::KEY_LEFT_ALT: return "left alt";
    case K::KEY_RIGHT_ALT: return "right alt";
    case K::KEY_ESCAPE: return "esc";
    case K::KEY_ENTER: case K::KEY_RETURN: case K::KEY_KP_ENTER: return "enter";
    case K::KEY_TAB: return "tab";
    case K::KEY_BACKSPACE: return "backspace";
    case K::KEY_CAPS_LOCK: return "caps lock";
    case K::KEY_INSERT: return "insert";
    case K::KEY_DELETE: return "delete";
    case K::KEY_HOME: return "home";
    case K::KEY_END: return "end";
    case K::KEY_PG_UP: return "page up";
    case K::KEY_PG_DOWN: return "page down";
    case K::KEY_COMMA: return ",";
    case K::KEY_PERIOD: return ".";
    case K::KEY_SLASH: return "/";
    case K::KEY_SEMICOLON: return ";";
    case K::KEY_APOSTROPHE: return "'";
    case K::KEY_MINUS: return "minus";
    case K::KEY_EQUAL: return "=";
    case K::KEY_LEFT_BRACKET: return "[";
    case K::KEY_RIGHT_BRACKET: return "]";
    case K::KEY_BACK_SLASH: return "\\";
    case K::KEY_GRAVE: return "`";
    case K::KEY_KP_PLUS: return "num +";
    case K::KEY_KP_MINUS: return "num -";
    case K::KEY_KP_MULTIPLY: return "num *";
    case K::KEY_KP_DIVIDE: return "num /";
    case K::KEY_KP_UP: return "num 8";
    case K::KEY_KP_DOWN: return "num 2";
    case K::KEY_KP_LEFT: return "num 4";
    case K::KEY_KP_RIGHT: return "num 6";
    case K::KEY_KP_FIVE: return "num 5";
    case K::KEY_KP_HOME: return "num 7";
    case K::KEY_KP_PG_UP: return "num 9";
    case K::KEY_KP_END: return "num 1";
    case K::KEY_KP_PG_DOWN: return "num 3";
    case K::KEY_KP_INSERT: return "num 0";
    case K::KEY_KP_DELETE: return "num .";
    case K::KEY_PAUSE: return "pause";
    case K::KEY_SCROLL_LOCK: return "scroll lock";
    case K::KEY_PRINT: return "print";
    case K::KEY_MENU: return "menu";
    default: return "key " + std::to_string(k);
    }
}

std::string keysText(KeyAction action)
{
    std::string text;
    for (K key : keysFor(action)) {
        if (key == K::KEY_NONE) continue;
        text += (text.empty() ? "" : " / ") + keyName(key);
    }
    return text.empty() ? "-" : text;
}

std::string keyHint(KeyAction action)
{
    // PAD (PC addition): the controller's buttons when it is what drives the game (the first ten
    // key actions and the controller actions are in the same order).
    const bool padDrives = openwheels::pad::lastInputWasPad() || (!desktopBuild() && openwheels::pad::anyConnected());
    if (padDrives && (int)action < (int)KeyAction::Fullscreen) return padHint((PadAction)(int)action);
    std::string text = keysText(action);
    for (char& c : text) c = (char)toupper((unsigned char)c);
    return text;
}

Label* createKeyHintLabel(KeyAction action, float fontSize)
{
    Label* label = Label::createWithTTF(keyHint(action), "fonts/ClarendonLTStd-Bold.ttf", fontSize);
    label->setColor(globals::colors::yellow);
    label->enableOutline(Color4B(0, 0, 0, 255), (int)(fontSize * 0.08f) + 1);
    return label;
}

}  // namespace qol
