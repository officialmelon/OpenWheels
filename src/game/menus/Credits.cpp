#include "Credits.h"

#include "Globals.h"
#include "Settings.h"

USING_NS_CC;

// @005a6a3c
Credits::Credits()
: _container(nullptr)
, _scrollSpeed(0.0f)
, _showEnding(false)
, _touching(false)
, _maxScrollY(0.0f)
, _touchListener(nullptr)
{
}

// @005a6a80
Credits* Credits::create(bool showEnding)
{
    // Plain (throwing) new; no null check and init()'s result is ignored (as in the original).
    Credits* credits = new Credits();
    credits->init(showEnding);
    credits->autorelease();
    return credits;
}

// @005a6ad8
bool Credits::init(bool showEnding)
{
    _showEnding = showEnding;

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();  // unused

    _container = Node::create();
    _container->setPosition(Vec2(visibleSize.width * 0.5f, 0.0f));
    addChild(_container);

    std::string fullPath = FileUtils::getInstance()->fullPathForFilename("credits.plist");
    ValueMap plist = FileUtils::getInstance()->getValueMapFromFile(fullPath.c_str());
    ValueVector sections = plist["credits"].asValueVector();

    // PC addition: OpenWheels' own credits ahead of the original game's (which come from the
    // player's own credits.plist).
    {
        ValueMap openWheels;
        openWheels["category"] = Value("OpenWheels");
        openWheels["items"] = Value(ValueVector{Value("@officialmelon"), Value("github.com/officialmelon/OpenWheels")});
        sections.insert(sections.begin(), Value(openWheels));
    }

    Settings::getInstance();  // result unused

    _scrollSpeed = 2.5f;

    float y;
    if (!_showEnding)
    {
        y = 0.0f;
    }
    else
    {
        std::string endingHeader = plist["ending_header"].asString();
        std::string endingMessage = plist["ending_message"].asString();

        Label* headerLabel = Label::createWithTTF(endingHeader, "fonts/ClarendonLTStd.ttf", 150.0f);
        headerLabel->setAnchorPoint(Vec2(0.5f, 1.0f));
        headerLabel->setPosition(Vec2(0.0f, 0.0f));
        headerLabel->setColor(Color3B::WHITE);
        headerLabel->setAlignment(TextHAlignment::CENTER, headerLabel->getVerticalAlignment());
        _container->addChild(headerLabel);
        float headerHeight = headerLabel->getContentSize().height;

        Label* messageLabel = Label::createWithTTF(endingMessage, "fonts/ClarendonLTStd.ttf", 75.0f);
        messageLabel->setAnchorPoint(Vec2(0.5f, 1.0f));
        y = 0.0f - (headerHeight + 50.0f);
        messageLabel->setPosition(Vec2(0.0f, y));
        messageLabel->setColor(Color3B::WHITE);
        messageLabel->setAlignment(TextHAlignment::CENTER, messageLabel->getVerticalAlignment());
        messageLabel->setAdditionalKerning(3.0f);
        _container->addChild(messageLabel);
        float messageHeight = messageLabel->getContentSize().height;

        y = y - (messageHeight + 250.0f);
    }

    Label* lastLabel = nullptr;
    for (size_t i = 0; i < sections.size(); i++)
    {
        if (i != 0)
        {
            y += -100.0f;
        }

        ValueMap section = sections[i].asValueMap();
        std::string category = section["category"].asString();

        Label* categoryLabel = Label::createWithTTF(category, "fonts/ClarendonLTStd.ttf", 75.0f);
        categoryLabel->setAnchorPoint(Vec2(0.5f, 1.0f));
        categoryLabel->setPosition(Vec2(0.0f, y));
        categoryLabel->setColor(globals::colors::blue);
        categoryLabel->setAlignment(TextHAlignment::CENTER, categoryLabel->getVerticalAlignment());
        _container->addChild(categoryLabel);
        float categoryHeight = categoryLabel->getContentSize().height;

        ValueVector items = section["items"].asValueVector();
        y = y - (categoryHeight + 50.0f);

        for (size_t j = 0; j < items.size(); j++)
        {
            std::string item = items[j].asString();
            lastLabel = Label::createWithTTF(item, "fonts/ClarendonLTStd.ttf", 75.0f);
            lastLabel->setColor(Color3B::WHITE);
            lastLabel->setPosition(Vec2(0.0f, y));
            lastLabel->setAlignment(TextHAlignment::CENTER, lastLabel->getVerticalAlignment());
            lastLabel->setAnchorPoint(Vec2(0.5f, 1.0f));
            _container->addChild(lastLabel);
            float itemHeight = lastLabel->getContentSize().height;
            float spacing = (j == items.size() - 1) ? -100.0f : -50.0f;
            y = (y - itemHeight) + spacing;
        }
    }

    // The container scrolls up until the last label has passed the top of the screen.
    float lastY = lastLabel->getPosition().y;
    _maxScrollY = (lastLabel->getContentSize().height - lastY) + visibleSize.height;

    scheduleUpdate();
    addTouchListener();

    return true;
}

// @005a774c (D1), @005a77e4 (D0)
Credits::~Credits()
{
    removeTouchListener();
}

// @005a77a4
void Credits::removeTouchListener()
{
    if (_touchListener)
    {
        Director::getInstance()->getEventDispatcher()->removeEventListener(_touchListener);
        _touchListener->release();
        _touchListener = nullptr;
    }
}

// @005a7930
void Credits::addTouchListener()
{
    if (_touchListener)
    {
        return;
    }

    _touchListener = EventListenerTouchOneByOne::create();
    _touchListener->retain();
    _touchListener->setSwallowTouches(true);
    // @005a7d84  Credits::addTouchListener()::$_0
    _touchListener->onTouchBegan = [this](Touch* touch, Event* event) { return touchBegan(touch, event); };
    // @005a7f80  Credits::addTouchListener()::$_1
    _touchListener->onTouchMoved = [this](Touch* touch, Event* event) { touchMoved(touch, event); };
    // @005a8220  Credits::addTouchListener()::$_2
    _touchListener->onTouchEnded = [this](Touch* touch, Event* event) { touchEnded(touch, event); };
    // @005a82ac  Credits::addTouchListener()::$_3
    _touchListener->onTouchCancelled = [this](Touch* touch, Event* event) { touchCancelled(touch, event); };
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(_touchListener, 1);
}

// @005a7b00
void Credits::update(float dt)
{
    Vec2 position = _container->getPosition();
    if (!_touching)
    {
        _container->setPosition(Vec2(position.x, position.y + _scrollSpeed));
    }

    // Wrap around (tested against the position before this frame's scroll step).
    if (position.y > _maxScrollY)
    {
        _container->setPosition(Vec2(position.x, 0.0f));
    }
    else if (position.y < 0.0f)
    {
        _container->setPosition(Vec2(position.x, _maxScrollY));
    }
}

// @005a7bc4
bool Credits::touchBegan(Touch* touch, Event* event)
{
    _touching = true;
    return true;
}

// @005a7bd8
void Credits::touchMoved(Touch* touch, Event* event)
{
    Vec2 location = touch->getLocation();
    Vec2 previousLocation = touch->getPreviousLocation();
    float deltaY = location.y - previousLocation.y;
    float x = _container->getPosition().x;
    _container->setPosition(Vec2(x, deltaY + _container->getPosition().y));
}

// @005a7c94
void Credits::touchEnded(Touch* touch, Event* event)
{
    _touching = false;
}

// @005a7c9c
void Credits::touchCancelled(Touch* touch, Event* event)
{
    _touching = false;
}
