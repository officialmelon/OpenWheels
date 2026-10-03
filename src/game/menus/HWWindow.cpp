#include "HWWindow.h"

#include "GameText.h"
#include "Mascot.h"
#include "Settings.h"
#include "ui/UIScrollView.h"
#include "ui/UIText.h"

#include <algorithm>

USING_NS_CC;

// @005c5838
HWWindow* HWWindow::create(HWWindowAppearance appearance, HWWindowDelegate* delegate, bool addCloseBtn,
                           bool addMascot)
{
    HWWindow* window = new (std::nothrow) HWWindow();
    if (window)
    {
        // The result of init() is not checked.
        window->init(appearance, delegate, addCloseBtn, addMascot);
        window->autorelease();
    }
    return window;
}

// @005c58d0
bool HWWindow::init(HWWindowAppearance appearance, HWWindowDelegate* delegate, bool addCloseBtn, bool addMascot)
{
    _appearance = appearance;

    // The first assignment is immediately overwritten (as in the original).
    _windowSizeRatio = Size(0.66f, 0.8f);
    float sizeRatio = (appearance == HWWindowAppearanceLarge) ? 0.8f : 1.0f;
    _windowSizeRatio = Size(sizeRatio, sizeRatio);

    if (delegate)
    {
        addDelegate(delegate);
    }

    _windowSize.width = 2100.0f;

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    _bgLayer = LayerColor::create(Color4B(0, 0, 0, 255));
    _bgLayer->setOpacity(0);
    FadeTo* fadeIn = FadeTo::create(0.25f, 210);
    addChild(_bgLayer, _bgZOrder);
    _bgLayer->runAction(fadeIn);

    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("menus/alert/menu_alert.plist");

    _windowNode = Node::create();
    addChild(_windowNode, _windowZOrder);

    _windowFrame = Sprite::createWithSpriteFrameName("window_frame.png");
    _windowNode->addChild(_windowFrame);

    _contentNode = Node::create();
    _windowNode->addChild(_contentNode);

    Vec2 center(origin.x + visibleSize.width / 2, origin.y + visibleSize.height / 2);

    _windowFrame->setCenterRectNormalized(Rect(0.1f, 0.62f, 0.8f, 0.28f));
    _windowFrame->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    _windowFrame->setContentSize(Size(visibleSize.width * _windowSizeRatio.width,
                                      visibleSize.height * _windowSizeRatio.height));

    if (addCloseBtn)
    {
        _closeBtn = createCloseBtn(-1);
        _windowNode->addChild(Menu::create(_closeBtn, nullptr));
    }

    if (addMascot)
    {
        _mascot = Mascot::create();
        _windowNode->addChild(_mascot);
    }

    _windowNode->setPosition(center);

    // Swallow every touch while the window is up.
    _touchListener = EventListenerTouchOneByOne::create();
    _touchListener->retain();
    _touchListener->setSwallowTouches(true);
    // @005c7fe0  HWWindow::init(...)::$_0
    _touchListener->onTouchBegan = [](Touch* touch, Event* event) { return true; };
    Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(_touchListener, this);

    return true;
}

// @005c5d70
HWWindow* HWWindow::createAlertWindow(std::string title, std::string message, std::string confirmLabel,
                                      std::string cancelLabel, bool animated, bool addCloseBtn, bool addMascot)
{
    HWWindow* window =
        Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, addCloseBtn, addMascot);
    window->showAlertMessage(title, message, confirmLabel, cancelLabel, animated);
    return window;
}

// @005c5f54
void HWWindow::showAlertMessage(std::string title, std::string message, std::string confirmLabel,
                                std::string cancelLabel, bool animated)
{
    if (title.empty() && message.empty() && confirmLabel.empty() && cancelLabel.empty())
    {
        return;
    }

    if (_contentNode)
    {
        _contentNode->removeFromParent();
        _contentNode = Node::create();
        _windowNode->addChild(_contentNode);
    }

    Size frameSize = _windowFrame->getContentSize();

    float y = addHeader(title) - 100;

    if (!message.empty())
    {
        ui::Text* text = ui::Text::create(message, "fonts/Arial.ttf", (float)_textFontSize);
        text->setTextAreaSize(Size(_windowSize.width - _textSideMargin * 2.0f, 0.0f));
        text->setTextHorizontalAlignment(TextHAlignment::CENTER);
        text->setAnchorPoint(Vec2(0.5f, 1.0f));
        text->setColor(Color3B::WHITE);
        text->setPosition(Vec2(0.0f, y));
        _contentNode->addChild(text);
        y -= text->getContentSize().height;
    }

    MenuItemSprite* confirmBtn = btnWithLabel(confirmLabel, 1, 1, HWWindowButtonTypeMedium);
    Menu* menu;
    if (cancelLabel.empty())
    {
        menu = Menu::create(confirmBtn, nullptr);
    }
    else
    {
        MenuItemSprite* cancelBtn = btnWithLabel(cancelLabel, 0, 0, HWWindowButtonTypeMedium);
        menu = Menu::create(cancelBtn, confirmBtn, nullptr);
    }

    y = y - confirmBtn->getContentSize().height / 2 - 100;
    menu->alignItemsHorizontallyWithPadding(50.0f);
    menu->setPosition(Vec2(0.0f, y));
    _contentNode->addChild(menu);

    _windowSize.height = confirmBtn->getContentSize().height - y;
    resizeWindowFrameToFitContent();

    if (animated)
    {
        animateInWindow();
    }
    else
    {
        layoutContent();
    }
}

// @005c63a8
HWWindow::HWWindow()
: _bgZOrder(10000)
, _windowZOrder(10001)
{
    // Assigned in the body: the binary stores these after both Size members are constructed.
    // _touchListener is left uninitialised (as in the original).
    _mascot = nullptr;
    _closeBtn = nullptr;
    _bgLayer = nullptr;
    _headerFontSize = 120;
    _textFontSize = 80;
    _headerTopMargin = 100.0f;
    _headerBottomMargin = 100.0f;
    _titleBarHeight = 60.0f;
    _frameBorder = 8.0f;
    _unk380 = 16.0f;
    _textSideMargin = 100.0f;
    _buttonSpacing = 50.0f;
    _dismissed = false;
    _dismissUponButtonPress = true;
    _windowNode = nullptr;
    _windowFrame = nullptr;
    _headerLabel = nullptr;
    _contentNode = nullptr;
}

// @005c6480 (D1), @005c64c8 (D0)
HWWindow::~HWWindow()
{
}

// @005c64ec
void HWWindow::setDismissUponButtonPress(bool dismissUponButtonPress)
{
    _dismissUponButtonPress = dismissUponButtonPress;
}

// @005c64f4
void HWWindow::addDelegate(HWWindowDelegate* delegate)
{
    if (std::find(_delegates.begin(), _delegates.end(), delegate) == _delegates.end())
    {
        _delegates.push_back(delegate);
    }
}

// @005c6698
MenuItemSprite* HWWindow::createCloseBtn(int tag)
{
    std::string frameName = "window_btn_close.png";

    MenuItemImage* btn = MenuItemImage::create("", "", CC_CALLBACK_1(HWWindow::btnPressed, this));
    Sprite* normalSprite = Sprite::createWithSpriteFrameName(frameName);
    Sprite* selectedSprite = Sprite::createWithSpriteFrameName(frameName);
    selectedSprite->setOpacity(125);
    btn->setNormalImage(normalSprite);
    btn->setSelectedImage(selectedSprite);
    btn->setTag(tag);
    return btn;
}

// @005c68bc
Mascot* HWWindow::getMascot()
{
    return _mascot;
}

// @005c68c4
void HWWindow::animateInWindow()
{
    layoutContent();

    if (_appearance == HWWindowAppearanceFullScreen || _appearance == HWWindowAppearanceLarge)
    {
        Size visibleSize = Director::getInstance()->getVisibleSize();
        Vec2 origin = Director::getInstance()->getVisibleOrigin();

        // Start below the screen, slide up to the centre.
        _windowNode->setPosition(Vec2(visibleSize.width / 2, -visibleSize.height / 2));
        MoveTo* moveIn = MoveTo::create(0.5f, Vec2(visibleSize.width / 2, visibleSize.height / 2));
        _windowNode->runAction(EaseExponentialOut::create(moveIn));
    }
    else if (_appearance == HWWindowAppearanceAlert)
    {
        _windowNode->setScale(1.25f);
        FadeIn* fadeIn = FadeIn::create(0.3f);
        EaseExponentialOut* scaleIn = EaseExponentialOut::create(ScaleTo::create(0.5f, 1.0f));
        _windowNode->runAction(Spawn::create(fadeIn, scaleIn, nullptr));
    }
}

// @005c6a1c
void HWWindow::layoutContent()
{
    Size contentSize = _contentNode->getBoundingBox().size;
    _contentNode->setPosition(Vec2(0.0f, _windowSize.height / 2));

    Size frameSize = _windowFrame->getContentSize();

    if (_mascot)
    {
        if (_appearance == HWWindowAppearanceFullScreen)
        {
            Size mascotSize = _mascot->getContentSize();
            _mascot->setPosition(Vec2(155 - frameSize.width / 2, frameSize.height / 2 - 215));
        }
        else
        {
            _mascot->setPosition(Vec2(40 - frameSize.width / 2, frameSize.height / 2));
        }
    }

    if (_closeBtn)
    {
        Node* closeMenu = _closeBtn->getParent();
        if (_appearance == HWWindowAppearanceFullScreen)
        {
            closeMenu->setPosition(Vec2(frameSize.width / 2 - 125, frameSize.height / 2 - 175));
        }
        else
        {
            closeMenu->setPosition(Vec2(frameSize.width / 2, frameSize.height / 2 - _titleBarHeight / 2));
        }
    }
}

// @005c6bd4
void HWWindow::resizeWindowFrameToFitContent()
{
    if (_appearance == HWWindowAppearanceAlert)
    {
        _windowFrame->setContentSize(_windowSize);
    }
}

// @005c6bf8
void HWWindow::dismissWindow(bool animated)
{
    if (_dismissed)
    {
        return;
    }
    _dismissed = true;

    if (_touchListener)
    {
        Director::getInstance()->getEventDispatcher()->removeEventListener(_touchListener);
        _touchListener->release();
        _touchListener = nullptr;
    }

    if (_windowNode)
    {
        _windowNode->removeFromParent();
        _windowNode = nullptr;
    }

    if (_bgLayer)
    {
        if (animated)
        {
            FadeTo* fadeOut = FadeTo::create(0.4f, 0);
            CallFunc* done = CallFunc::create(CC_CALLBACK_0(HWWindow::dismissAnimationComplete, this));
            _bgLayer->runAction(Sequence::create(fadeOut, done, nullptr));
        }
        else
        {
            dismissAnimationComplete();
        }
    }
}

// @005c6e14
void HWWindow::dismissAnimationComplete()
{
    onWindowDismissed();
    for (HWWindowDelegate* delegate : _delegates)
    {
        if (delegate)
        {
            delegate->hwWindowWasDismissed(this);
        }
    }
    removeFromParentAndCleanup(true);
}

// @005c6e88
float HWWindow::addHeader(std::string title)
{
    float y = -_titleBarHeight - _headerTopMargin;

    _headerLabel = Label::createWithTTF(title, "fonts/ClarendonLTStd-Bold.ttf", (float)_headerFontSize);
    _headerLabel->setColor(Color3B::WHITE);
    _headerLabel->setAnchorPoint(Vec2(0.5f, 1.0f));
    _headerLabel->setPosition(0.0f, y);
    _contentNode->addChild(_headerLabel);

    return y - _headerLabel->getContentSize().height / 2;
}

// @005c700c
MenuItemSprite* HWWindow::btnWithLabel(std::string label, int color, int tag, HWWindowButtonType type)
{
    float fontSize = (type == HWWindowButtonTypeLarge) ? 120.0f : 80.0f;
    float width = (type == HWWindowButtonTypeLarge) ? 1400.0f : 700.0f;
    float height = (type == HWWindowButtonTypeLarge) ? 320.0f : 160.0f;

    std::string frameName = "window_btn_pink.png";
    Color3B textColor(255, 255, 255);
    switch (color)
    {
    case 0:
        frameName = "window_btn_pink.png";
        break;
    case 1:
        frameName = "window_btn_blue.png";
        break;
    case 2:
        textColor = Color3B(70, 70, 70);
        frameName = "window_btn_yellow.png";
        break;
    }

    Label* labelNode = Label::createWithTTF(label, "fonts/ClarendonLTStd-Bold.ttf", fontSize);
    labelNode->setColor(textColor);
    Size labelSize = labelNode->getContentSize();
    labelNode->setPosition(Vec2(width / 2, height / 2 - 7));

    MenuItemImage* btn = MenuItemImage::create("", "", CC_CALLBACK_1(HWWindow::btnPressed, this));
    btn->addChild(labelNode, 9999);

    Sprite* normalSprite = Sprite::createWithSpriteFrameName(frameName);
    Sprite* selectedSprite = nullptr;
    if (!frameName.empty())
    {
        selectedSprite = Sprite::createWithSpriteFrameName(frameName);
    }

    Rect centerRect(0.29f, 0.29f, 0.42f, 0.42f);
    normalSprite->setCenterRectNormalized(centerRect);
    normalSprite->setContentSize(Size(width, height));
    if (selectedSprite)
    {
        selectedSprite->setCenterRectNormalized(centerRect);
        selectedSprite->setContentSize(Size(width, height));
        selectedSprite->setOpacity(125);
        btn->setSelectedImage(selectedSprite);
    }
    btn->setNormalImage(normalSprite);
    btn->setTag(tag);
    return btn;
}

// @005c74d0
void HWWindow::showPrivacyPolicyMessage()
{
    Size frameSize = _windowFrame->getContentSize();

    std::string title = "Important";
    std::string message = OW_GAMETEXT(hwWindowPrivacyPolicyMessage, 0x3fbc91);
    // Built and never used (as in the original).
    std::string reviewNote = OW_GAMETEXT(hwWindowPrivacyPolicyReviewNote, 0x40ad49);

    float y = addHeader(title);

    ui::Text* text = ui::Text::create(message, "fonts/Arial.ttf", (float)_textFontSize);
    text->setTextAreaSize(Size(_windowSize.width - _textSideMargin * 2.0f, 0.0f));
    text->setTextHorizontalAlignment(TextHAlignment::CENTER);
    text->setAnchorPoint(Vec2(0.5f, 1.0f));
    text->setColor(Color3B::WHITE);
    y = y - 100;
    text->setPosition(Vec2(0.0f, y));
    _contentNode->addChild(text);
    float textHeight = text->getContentSize().height;

    MenuItemSprite* acceptBtn = btnWithLabel("ACCEPT", 1, 1, HWWindowButtonTypeMedium);
    MenuItemSprite* privacyPolicyBtn = btnWithLabel("Privacy Policy", 0, 0, HWWindowButtonTypeMedium);
    Menu* menu = Menu::create(privacyPolicyBtn, acceptBtn, nullptr);

    y = y - textHeight - acceptBtn->getContentSize().height / 2 - 100;
    menu->alignItemsHorizontallyWithPadding(50.0f);
    menu->setPosition(Vec2(0.0f, y));
    _contentNode->addChild(menu);

    _windowSize.height = -(y - acceptBtn->getContentSize().height / 2 - 50 - 50);
    resizeWindowFrameToFitContent();

    animateInWindow();
}

// @005c7988
void HWWindow::showScrollableText(std::string title, std::string text, std::string confirmLabel,
                                  std::string cancelLabel)
{
    Size frameSize = _windowFrame->getContentSize();

    float y = addHeader(title);

    float width = _windowSize.width - _textSideMargin * 2.0f;
    float height = frameSize.height - _textSideMargin * 2.0f;

    ui::ScrollView* scrollView = ui::ScrollView::create();
    scrollView->setContentSize(Size(width, height));
    _contentNode->addChild(scrollView);

    Label* label = Label::createWithTTF(text, "fonts/Arial Bold.ttf", 50.0f);
    label->setDimensions(width, 0.0f);
    label->setAnchorPoint(Vec2(0.0f, 1.0f));
    label->setTextColor(Color4B::BLACK);
    scrollView->setInnerContainerSize(Size(width, label->getContentSize().height));
    scrollView->setAnchorPoint(Vec2(0.5f, 0.5f));
    scrollView->addChild(label);
    float labelHeight = label->getContentSize().height;

    int confirmTag = (_delegates.size() > 1) ? 1 : -1;
    MenuItemSprite* confirmBtn = btnWithLabel(confirmLabel, 1, confirmTag, HWWindowButtonTypeMedium);
    Menu* menu;
    if (cancelLabel.empty())
    {
        menu = Menu::create(confirmBtn, nullptr);
    }
    else
    {
        MenuItemSprite* cancelBtn = btnWithLabel(cancelLabel, 0, 0, HWWindowButtonTypeMedium);
        menu = Menu::create(cancelBtn, confirmBtn, nullptr);
    }

    y = y - 100 - labelHeight - confirmBtn->getContentSize().height / 2 - 100;
    menu->alignItemsHorizontallyWithPadding(50.0f);
    menu->setPosition(Vec2(0.0f, y));
    _contentNode->addChild(menu);

    _windowSize.height = confirmBtn->getContentSize().height - y;

    animateInWindow();
}

// @005c7d80
void HWWindow::removeDelegate(HWWindowDelegate* delegate)
{
    auto it = std::find(_delegates.begin(), _delegates.end(), delegate);
    if (it != _delegates.end())
    {
        _delegates.erase(it);
    }
}

// @005c7de4
void HWWindow::removeAllDelegates()
{
    _delegates.clear();
}

// @005c7df0
void HWWindow::addButtons()
{
    MenuItemSprite* closeBtn = btnWithLabel("CLOSE", 0, 0, HWWindowButtonTypeMedium);
    Menu* menu = Menu::create(closeBtn, nullptr);
    menu->setPosition(Vec2::ZERO);
    _windowNode->addChild(menu);
}

// @005c7ee4
void HWWindow::btnPressed(Ref* sender)
{
    if (_dismissed)
    {
        return;
    }

    int tag = static_cast<Node*>(sender)->getTag();
    onWindowButtonPressed(tag);
    for (HWWindowDelegate* delegate : _delegates)
    {
        if (delegate)
        {
            delegate->hwWindowButtonPressed(tag, this);
        }
    }

    if (_dismissUponButtonPress)
    {
        dismissWindow(true);
    }
}

// @005c7f90
void HWWindow::update(float dt)
{
}

// @005c7f94
void HWWindow::onWindowButtonPressed(int buttonTag)
{
}

// @005c7f98
void HWWindow::onWindowDismissed()
{
}
