#include "GameplayTimer.h"

#include <cmath>

#include "Patch.h"
#include "Session.h"
#include "Settings.h"

USING_NS_CC;

// @005bde9c (D2), @005bdf28 (D0)
GameplayTimer::~GameplayTimer()
{
}

// @005bd744
bool GameplayTimer::init()
{
    _session = Settings::getInstance()->getCurrentSession();
    _label = Label::createWithBMFont("fonts/timer.fnt", "0.00", TextHAlignment::LEFT, 0, Vec2::ZERO);
    _label->setAnchorPoint(Vec2(0.0f, 0.5f));
    addChild(_label);
    _time = 0.0f;
    _realignedForMinutes = false;
    updateLabel();
    return true;
}

// @005bd898
void GameplayTimer::updateLabel()
{
    float time = _time;
    unsigned int minutes = floorf(time / 60.0f);
    unsigned int seconds = floorf(fmodf(time, 60.0f));
    unsigned int hundredths = floorf(fmodf(time, 1.0f) * 100.0f);

    if (minutes == 0)
    {
        _minutesString = "";
    }
    else
    {
        _minutesString = patch::to_string(minutes) + ":";
    }

    if (seconds < 10)
    {
        _secondsString = "0" + patch::to_string(seconds);
    }
    else
    {
        _secondsString = patch::to_string(seconds);
    }

    if (minutes < 10)
    {
        if (hundredths < 10)
        {
            _hundredthsString = ".0" + patch::to_string(hundredths);
        }
        else
        {
            _hundredthsString = "." + patch::to_string(hundredths);
        }
    }
    else
    {
        _hundredthsString = "";
    }

    _timeString = _minutesString + _secondsString + _hundredthsString;
    _label->setString(_timeString);

    if (minutes != 0)
    {
        if (minutes >= 10 && !_realignedForTenMinutes)
        {
            _realignedForTenMinutes = true;
            alignText();
        }
        else if (!_realignedForMinutes)
        {
            _realignedForMinutes = true;
            alignText();
        }
    }
}

// @005bdd44
void GameplayTimer::setTimeLimit(float timeLimit)
{
    _timeLimit = timeLimit;
}

// @005bdd4c
void GameplayTimer::setBackgroundSprite(Sprite* sprite)
{
    _backgroundSprite = sprite;
    alignText();
}

// @005bdd54
void GameplayTimer::alignText()
{
    if (!_label)
    {
        return;
    }
    // both fetched but not used
    Size winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    _label->setPosition(_backgroundSprite->getPosition().x + _label->getContentSize().width * -0.5f,
                        _backgroundSprite->getPosition().y);
}

// @005bde30
float GameplayTimer::getTime()
{
    return _time;
}

// @005bde38
void GameplayTimer::reset(float timeLimit)
{
    _timeLimit = timeLimit;
    _time = 0.0f;
    _realignedForMinutes = false;
    _realignedForTenMinutes = false;
    updateLabel();
}

// @005bde48
bool GameplayTimer::update()
{
    _time = _session->getTimeStep() + _time;
    updateLabel();
    return true;
}

// @005bde84
void GameplayTimer::setHidden(bool hidden)
{
    _label->setVisible(!hidden);
}
