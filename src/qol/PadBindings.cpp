// PAD (PC addition): remappable game controller bindings, see PadBindings.h.
#include "qol/PadBindings.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

#include "cocos2d.h"

#include "input/Gamepad.h"

USING_NS_CC;

namespace qol {
namespace {

namespace pad = openwheels::pad;

const int kActionCount = (int)PadAction::Count;

struct Slots
{
    int code[kPadSlots];
};

// Sticks and d-pad drive like the arrow keys, the triggers accelerate / brake, A is the primary
// action (space), Y ejects, X / B and the bumpers are the restored characters' extra actions,
// Start pauses and Back restarts.
Slots defaults(PadAction action)
{
    const int n = pad::kNoInput;
    switch (action)
    {
    case PadAction::Accelerate: return {{pad::kLUp, pad::kDUp, pad::kRT}};
    case PadAction::Reverse: return {{pad::kLDown, pad::kDDown, pad::kLT}};
    case PadAction::LeanBack: return {{pad::kLLeft, pad::kDLeft, n}};
    case PadAction::LeanForward: return {{pad::kLRight, pad::kDRight, n}};
    case PadAction::Special: return {{pad::kA, n, n}};
    case PadAction::Shift: return {{pad::kX, pad::kLB, n}};
    case PadAction::Ctrl: return {{pad::kB, pad::kRB, n}};
    case PadAction::Eject: return {{pad::kY, n, n}};
    case PadAction::Pause: return {{pad::kStart, n, n}};
    case PadAction::Restart: return {{pad::kBack, n, n}};
    case PadAction::MenuSelect: return {{pad::kA, n, n}};
    case PadAction::MenuBack: return {{pad::kB, n, n}};
    default: return {{n, n, n}};
    }
}

std::string storeKey(int action) { return "qol_pad_" + std::to_string(action); }

Slots* table()
{
    static Slots slots[kActionCount];
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        UserDefault* store = UserDefault::getInstance();
        for (int a = 0; a < kActionCount; a++)
        {
            slots[a] = defaults((PadAction)a);
            const std::string saved = store->getStringForKey(storeKey(a).c_str(), "");
            if (saved.empty()) continue;
            std::istringstream in(saved);
            std::string item;
            for (int s = 0; s < kPadSlots; s++)
            {
                slots[a].code[s] = pad::kNoInput;
                if (std::getline(in, item, ',')) slots[a].code[s] = std::atoi(item.c_str());
            }
        }
    }
    return slots;
}

void save()
{
    UserDefault* store = UserDefault::getInstance();
    Slots* slots = table();
    for (int a = 0; a < kActionCount; a++)
    {
        bool isDefault = true;
        const Slots d = defaults((PadAction)a);
        std::string text;
        for (int s = 0; s < kPadSlots; s++)
        {
            isDefault = isDefault && slots[a].code[s] == d.code[s];
            text += (s ? "," : "") + std::to_string(slots[a].code[s]);
        }
        if (isDefault)
        {
            store->deleteValueForKey(storeKey(a).c_str());
        }
        else
        {
            store->setStringForKey(storeKey(a).c_str(), text);
        }
    }
    store->flush();
}

}  // namespace

bool padActionIsMenu(PadAction action) { return action == PadAction::MenuSelect || action == PadAction::MenuBack; }

const char* padActionName(PadAction action)
{
    switch (action)
    {
    case PadAction::Accelerate: return "accelerate";
    case PadAction::Reverse: return "brake / reverse";
    case PadAction::LeanBack: return "lean back";
    case PadAction::LeanForward: return "lean forward";
    case PadAction::Special: return "primary action";
    case PadAction::Shift: return "extra action 1";
    case PadAction::Ctrl: return "extra action 2";
    case PadAction::Eject: return "eject";
    case PadAction::Pause: return "pause";
    case PadAction::Restart: return "restart";
    case PadAction::MenuSelect: return "menu: select";
    case PadAction::MenuBack: return "menu: back";
    default: return "";
    }
}

std::vector<int> padInputsFor(PadAction action)
{
    const Slots& s = table()[(int)action];
    return std::vector<int>(s.code, s.code + kPadSlots);
}

void setPadInput(PadAction action, int slot, int code)
{
    if (slot < 0 || slot >= kPadSlots) return;
    Slots* slots = table();
    if (code != pad::kNoInput)
    {
        for (int a = 0; a < kActionCount; a++)
        {
            if (padActionIsMenu((PadAction)a) != padActionIsMenu(action)) continue;
            for (int s = 0; s < kPadSlots; s++)
            {
                if (slots[a].code[s] == code) slots[a].code[s] = pad::kNoInput;
            }
        }
    }
    slots[(int)action].code[slot] = code;
    save();
}

void resetPadBindings()
{
    Slots* slots = table();
    for (int a = 0; a < kActionCount; a++) slots[a] = defaults((PadAction)a);
    save();
}

bool padActionHeld(PadAction action)
{
    for (int code : table()[(int)action].code)
    {
        if (code != pad::kNoInput && pad::held(code)) return true;
    }
    return false;
}

bool padActionPressed(PadAction action)
{
    for (int code : table()[(int)action].code)
    {
        if (code != pad::kNoInput && pad::pressed(code)) return true;
    }
    return false;
}

std::string padInputsText(PadAction action)
{
    std::string text;
    for (int code : padInputsFor(action))
    {
        if (code == pad::kNoInput) continue;
        text += (text.empty() ? "" : " / ") + pad::inputName(code);
    }
    return text.empty() ? "-" : text;
}

std::string padHint(PadAction action)
{
    std::string text = padInputsText(action);
    for (char& c : text) c = (char)toupper((unsigned char)c);
    return text;
}

}  // namespace qol
