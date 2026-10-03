#include "LevelUIHelpers.h"

#include "HWWindow.h"
#include "HWWindowDelegate.h"

#include <cctype>

USING_NS_CC;

namespace levelui {

namespace {

// HWWindow name = button layout of the UIAlertView it stands for.
const char* const kAlertOneButton = "uialert1";
const char* const kAlertTwoButtons = "uialert2";
const char* const kAlertThreeButtons = "uialert3";

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

}  // namespace

std::string capitalized(const std::string& text)
{
    std::string result = text;
    bool wordStart = true;
    for (char& c : result)
    {
        unsigned char u = static_cast<unsigned char>(c);
        if (isSpace(c))
        {
            wordStart = true;
            continue;
        }
        if (u < 0x80 && std::isalpha(u))
        {
            c = static_cast<char>(wordStart ? std::toupper(u) : std::tolower(u));
        }
        wordStart = false;
    }
    return result;
}

std::string trimmed(const std::string& text)
{
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && isSpace(text[begin]))
    {
        begin++;
    }
    while (end > begin && isSpace(text[end - 1]))
    {
        end--;
    }
    return text.substr(begin, end - begin);
}

HWWindow* showAlert(int tag, const std::string& title, const std::string& message,
                    const std::string& cancelTitle, const std::string& otherTitle,
                    HWWindowDelegate* delegate, const std::string& otherTitle2)
{
    Scene* runningScene = Director::getInstance()->getRunningScene();
    if (!runningScene)
    {
        return nullptr;
    }
    bool threeButtons = !otherTitle2.empty();
    HWWindow* window = HWWindow::create(HWWindowAppearanceAlert, delegate, threeButtons, false);
    window->setTag(tag);
    if (threeButtons)
    {
        window->setName(kAlertThreeButtons);
        window->showAlertMessage(title, message, otherTitle, otherTitle2, true);
    }
    else if (otherTitle.empty())
    {
        window->setName(kAlertOneButton);
        window->showAlertMessage(title, message, cancelTitle, "", true);
    }
    else
    {
        window->setName(kAlertTwoButtons);
        window->showAlertMessage(title, message, otherTitle, cancelTitle, true);
    }
    runningScene->addChild(window, uikit::kWindowZOrder + 1);
    return window;
}

long buttonIndex(int buttonTag, HWWindow* window)
{
    const std::string& layout = window ? window->getName() : std::string();
    if (layout == kAlertThreeButtons)
    {
        if (buttonTag == 1)
        {
            return 1;
        }
        if (buttonTag == 0)
        {
            return 2;
        }
        return 0;
    }
    if (layout == kAlertTwoButtons)
    {
        return buttonTag == 1 ? 1 : 0;
    }
    return 0;
}

ui::Button* makeButton(const std::string& title, float pointSize,
                       const std::function<void(Ref*)>& action)
{
    ui::Button* button = ui::Button::create();
    button->setScale9Enabled(true);
    button->setTitleText(title);
    button->setTitleFontSize(pointSize * uikit::pointsToDesign());
    if (action)
    {
        button->addClickEventListener(action);
    }
    return button;
}

void setButtonTitle(ui::Button* button, const std::string& title)
{
    if (button)
    {
        button->setTitleText(title);
    }
}

void setButtonEnabled(ui::Button* button, bool enabled)
{
    if (button)
    {
        button->setEnabled(enabled);
        button->setBright(enabled);
    }
}

ui::Text* makeLabel(const std::string& text, float pointSize, bool bold, int alignment,
                    const Color4B& color)
{
    ui::Text* label = uikit::makeLabel(text, bold ? "Helvetica-Bold" : "Helvetica", pointSize);
    label->ignoreContentAdaptWithSize(false);
    label->setTextHorizontalAlignment(alignment == 1   ? TextHAlignment::CENTER
                                      : alignment == 2 ? TextHAlignment::RIGHT
                                                       : TextHAlignment::LEFT);
    label->setTextVerticalAlignment(TextVAlignment::CENTER);
    label->setTextColor(color);
    return label;
}

}  // namespace levelui
