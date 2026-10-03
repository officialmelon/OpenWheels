#include "VictoryAnimation.h"

#include <iomanip>
#include <sstream>

#include "Globals.h"

USING_NS_CC;

// _INIT_19: TU-level string @00ac65c8, never used.
static std::string s_victoryFontName = "ClarendonLTStd-Bold";

// @006400d8
VictoryAnimation::VictoryAnimation()
    : _bgWidth(0.0f)
{
    _placementString = "";
    _timeLabel = nullptr;
    _placementLabel = nullptr;
}

// @0064015c (D1), @0064019c (D0)
VictoryAnimation::~VictoryAnimation()
{
}

// @006401c0
bool VictoryAnimation::init(float time, std::string placementString, float bgWidth)
{
    _time = time;
    _placementString = placementString;
    _bgWidth = bgWidth;
    createAnimation();
    return true;
}

// @00640204
void VictoryAnimation::createAnimation()
{
    // banner
    Sprite* banner = Sprite::createWithSpriteFrameName("victory_banner.png");
    banner->setAnchorPoint(Vec2(0.5f, 0.0f));
    addChild(banner);
    banner->setScale(0.0f);
    banner->setPosition(Vec2(0.0f, 0.0f));
    FiniteTimeAction* delay = DelayTime::create(0.4f);
    banner->runAction(
        Sequence::create(delay, EaseExponentialOut::create(ScaleTo::create(0.5f, 1.0f)), nullptr));
    banner->runAction(Sequence::create(
        delay, EaseExponentialOut::create(MoveTo::create(0.5f, Vec2(0.0f, 0.0f))), nullptr));

    // placement ("your best time!", ...)
    _placementLabel = Label::createWithTTF(_placementString, "fonts/ClarendonLTStd-Bold.ttf", 73.0f,
                                           Size::ZERO, TextHAlignment::LEFT, TextVAlignment::TOP);
    _placementLabel->setAlignment(TextHAlignment::CENTER);
    _placementLabel->setColor(globals::colors::yellow);
    _placementLabel->setPosition(Vec2(0.0f, 200.0f));
    _placementLabel->setOpacity(0);
    _placementLabel->setScale(0.1f);
    addChild(_placementLabel, 100);
    delay = DelayTime::create(1.0f);
    FiniteTimeAction* scaleIn = Sequence::create(
        delay, EaseExponentialOut::create(ScaleTo::create(1.0f, 1.0f)), nullptr);
    FiniteTimeAction* fadeIn = Sequence::create(delay, FadeIn::create(1.0f), nullptr);
    _placementLabel->runAction(scaleIn);
    _placementLabel->runAction(fadeIn);

    // completion time
    if (_time > 0.0f)
    {
        std::stringstream timeText;
        timeText << std::fixed << std::setprecision(2) << _time;
        std::string text = timeText.str() + " seconds";
        _timeLabel = Label::createWithTTF(text, "fonts/ClarendonLTStd-Bold.ttf", 96.0f, Size::ZERO,
                                          TextHAlignment::LEFT, TextVAlignment::TOP);
        _timeLabel->setAlignment(TextHAlignment::CENTER);
        _timeLabel->setColor(globals::colors::blue);
        _timeLabel->setPosition(Vec2(0.0f, -110.0f));
        _timeLabel->setScale(0.01f);
        addChild(_timeLabel, 100);
        delay = DelayTime::create(0.65f);
        _timeLabel->runAction(Sequence::create(
            delay, EaseExponentialOut::create(ScaleTo::create(0.5f, 1.0f)), nullptr));
        _timeLabel->runAction(Sequence::create(
            delay, EaseExponentialOut::create(MoveTo::create(0.5f, Vec2(0.0f, 100.0f))), nullptr));
    }

    // decorations: 1/3 to the left, 2/4 to the right; 3/4 further out
    for (unsigned int i = 1; i != 5; i++)
    {
        Sprite* flower = Sprite::createWithSpriteFrameName("victory_flower.png");
        float direction = (i == 1 || i == 3) ? -1.0f : 1.0f;
        bool outer = i > 2;
        float xOffset = outer ? 270.0f : 130.0f;
        float y = outer ? -350.0f : -160.0f;
        int rotation = outer ? 45 : 15;
        float scale = outer ? 0.8f : 1.0f;
        float delayTime = outer ? 0.85f : 0.65f;
        flower->setPosition(Vec2(direction * (xOffset + _bgWidth * 0.5f), y));
        flower->setScaleX(direction * 0.01f);
        flower->setScaleY(0.01f);
        flower->setRotation(direction * -180.0f);
        addChild(flower);
        FiniteTimeAction* flowerDelay = DelayTime::create(delayTime);
        FiniteTimeAction* scaleAction = Sequence::create(
            flowerDelay,
            EaseExponentialOut::create(ScaleTo::create(0.5f, scale * direction, scale)), nullptr);
        FiniteTimeAction* rotateAction = Sequence::create(
            flowerDelay, EaseExponentialOut::create(RotateTo::create(0.5f, direction * rotation)),
            nullptr);
        flower->runAction(scaleAction);
        flower->runAction(rotateAction);
    }
}
