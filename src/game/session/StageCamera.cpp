#include "StageCamera.h"

#include <cmath>
#include <cstdlib>

#include "2d/CCDrawNode.h"
#include "2d/CCNode.h"
#include "Box2D/Box2D.h"
#include "base/CCDirector.h"
#include "base/CCEventCustom.h"
#include "base/CCEventDispatcher.h"
#include "base/CCEventListenerCustom.h"
#include "base/ccMacros.h"
#include "Globals.h"
#include "online/FlashRuntime.h"  // ONLINE (PC addition)

USING_NS_CC;

// @00633410
StageCamera::StageCamera()
    : _yParticleLimit(0.0f)
    , _midScreen()
    , _leftLimitPixels(0.0f)
    , _rightLimitPixels(0.0f)
    , _topLimitPixels(0.0f)
    , _bottomLimitPixels(0.0f)
    , _shakeAmount(0.0f)
    , _halfShakeRange(0.0f)
{
    // Every other member (including _systemTriggerListener) is left uninitialised, as in the
    // original; init() assigns them.
}

// @00633454
StageCamera::~StageCamera()
{
    if (_systemTriggerListener != nullptr)
    {
        Director::getInstance()->getEventDispatcher()->removeEventListener(_systemTriggerListener);
        _systemTriggerListener->release();
        _systemTriggerListener = nullptr;
    }
}

// @006334d0
StageCamera* StageCamera::create(Node* stage, b2Body* focus, float ptmRatio)
{
    StageCamera* camera = new (std::nothrow) StageCamera();
    if (camera != nullptr)
    {
        // The result of init() is ignored by the original.
        camera->init(stage, focus, ptmRatio);
        camera->autorelease();
    }
    return camera;
}

// @00633560
bool StageCamera::init(Node* stage, b2Body* focus, float ptmRatio)
{
    _drawNode = nullptr;
    _containerObj = stage;
    _focus = focus;
    _ptmRatio = ptmRatio;
    _scale = 1.0f;

    Size winSize = Director::getInstance()->getWinSize();

    _moveIncrementPixels = 2.5f;
    _upInc = 1.0f;
    _leadRight = true;

    _xFastForwardLeftSide = winSize.width * 0.11f;
    _xFastForwardRightSide = winSize.width * 0.22f;
    _xFastBackwardsLeftSide = winSize.width * 0.78f;
    _xFastBackwardsRightSide = winSize.width * 0.89f;
    _xNormalLeftSide = winSize.width * 0.29f;
    _xNormalRightSide = winSize.width * 0.4f;
    _xNormalLeftSideLeadLeft = winSize.width * 0.6f;
    _xNormalRightSideLeadLeft = winSize.width * 0.71f;
    _yFastTopSide = winSize.height * 0.8f;
    _yFastBottomSide = winSize.height * 0.7f;
    _yNormalTopSide = winSize.height * 0.6f;
    _yNormalBottomSide = winSize.height * 0.5f;

    _leftBorderPixels = _xNormalLeftSide;
    _rightBorderPixels = _xNormalRightSide;
    _topBorderPixels = _yNormalTopSide;
    _bottomBorderPixels = _yNormalBottomSide;

    _midXPoints = (winSize.width * 0.5f) / _scale;
    _midYPoints = (winSize.height * 0.5f) / _scale;

    // @00633f7c (lambda body; handleSystemTrigger inlined)
    _systemTriggerListener = EventListenerCustom::create("system_trigger", [this](EventCustom* event) {
        handleSystemTrigger(event->getUserData());
    });
    _systemTriggerListener->retain();
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(
        _systemTriggerListener, 60);
    return true;
}

// @00633750
void StageCamera::reset()
{
    _leadRight = true;
    _topBorderPixels = _yNormalTopSide;
    _bottomBorderPixels = _yNormalBottomSide;
    _leftBorderPixels = _xNormalLeftSide;
    _rightBorderPixels = _xNormalRightSide;
}

// @0063376c
void StageCamera::setFocus(b2Body* focus)
{
    _focus = focus;
}

// @00633774
b2Body* StageCamera::getFocus()
{
    return _focus;
}

// @0063377c
Vec2 StageCamera::getMidScreen()
{
    return _midScreen;
}

// @00633784
float StageCamera::getYParticleLimit()
{
    return _yParticleLimit;
}

// @0063378c
void StageCamera::handleSystemTrigger(void* data)
{
    switch (*static_cast<int*>(data))
    {
    case 6:
        _leadRight = true;
        break;
    case 7:
        _leadRight = false;
        break;
    case 8:
        _shakeAmount = 1.0f;
        _halfShakeRange = 20.0f;
        break;
    case 9:
        _shakeAmount = 0.0f;
        _halfShakeRange = 0.0f;
        break;
    }
}

// @006337e0
void StageCamera::shakeStage()
{
    _shakeAmount = 1.0f;
    _halfShakeRange = 20.0f;
}

// @006337f0
void StageCamera::endShakeStage()
{
    _shakeAmount = 0.0f;
    _halfShakeRange = 0.0f;
}

// @006337f8
void StageCamera::setLimits(Size limits)
{
    Size winSizeInPixels = Director::getInstance()->getWinSizeInPixels();
    Size scaledLimits(limits.width * _scale, _scale * limits.height);
    _rightLimitPixels = 0.0f;
    _topLimitPixels = 0.0f;
    _leftLimitPixels = winSizeInPixels.width - scaledLimits.width;
    _bottomLimitPixels = winSizeInPixels.height - scaledLimits.height;
}

// @00633888
// Eases the four borders towards the targets picked from the focus velocity. Increases are
// clamped to the target (fminf); decreases of the right and bottom borders, and of the left border
// in the fast cases, are NOT clamped (as in the original).
void StageCamera::setBorders()
{
    if (_focus == nullptr)
    {
        return;
    }
    const b2Vec2& velocity = _focus->GetLinearVelocity();
    const float velocityY = velocity.y;

    if (velocity.x > 3.0f)
    {
        if (_leftBorderPixels < _xFastForwardLeftSide)
        {
            _leftBorderPixels = fminf(_leftBorderPixels + _moveIncrementPixels, _xFastForwardLeftSide);
        }
        else if (_leftBorderPixels > _xFastForwardLeftSide)
        {
            _leftBorderPixels = _leftBorderPixels - _moveIncrementPixels;
        }
        if (_rightBorderPixels < _xFastForwardRightSide)
        {
            _rightBorderPixels = fminf(_rightBorderPixels + _moveIncrementPixels, _xFastForwardRightSide);
        }
        else if (_rightBorderPixels != _xFastForwardRightSide)
        {
            _rightBorderPixels = _rightBorderPixels - _moveIncrementPixels;
        }
    }
    else if (velocity.x < -3.0f)
    {
        if (_leftBorderPixels < _xFastBackwardsLeftSide)
        {
            _leftBorderPixels = fminf(_leftBorderPixels + _moveIncrementPixels, _xFastBackwardsLeftSide);
        }
        else if (_leftBorderPixels > _xFastBackwardsLeftSide)
        {
            _leftBorderPixels = _leftBorderPixels - _moveIncrementPixels;
        }
        if (_rightBorderPixels < _xFastBackwardsRightSide)
        {
            _rightBorderPixels = fminf(_rightBorderPixels + _moveIncrementPixels, _xFastBackwardsRightSide);
        }
        else if (_rightBorderPixels != _xFastBackwardsRightSide)
        {
            _rightBorderPixels = _rightBorderPixels - _moveIncrementPixels;
        }
    }
    else if (_leadRight)
    {
        if (_leftBorderPixels < _xNormalLeftSide)
        {
            _leftBorderPixels = fminf(_leftBorderPixels + _moveIncrementPixels, _xNormalLeftSide);
        }
        else if (_leftBorderPixels > _xNormalLeftSide)
        {
            float left = _leftBorderPixels - _moveIncrementPixels;
            if (left <= _xNormalLeftSide)
            {
                left = _xNormalLeftSide;
            }
            _leftBorderPixels = left;
        }
        if (_rightBorderPixels < _xNormalRightSide)
        {
            _rightBorderPixels = fminf(_rightBorderPixels + _moveIncrementPixels, _xNormalRightSide);
        }
        else if (_rightBorderPixels != _xNormalRightSide)
        {
            _rightBorderPixels = _rightBorderPixels - _moveIncrementPixels;
        }
    }
    else
    {
        if (_leftBorderPixels < _xNormalLeftSideLeadLeft)
        {
            _leftBorderPixels =
                fminf(_leftBorderPixels + _moveIncrementPixels, _xNormalLeftSideLeadLeft);
        }
        else if (_leftBorderPixels > _xNormalLeftSideLeadLeft)
        {
            float left = _leftBorderPixels - _moveIncrementPixels;
            if (left <= _xNormalLeftSideLeadLeft)
            {
                left = _xNormalLeftSideLeadLeft;
            }
            _leftBorderPixels = left;
        }
        if (_rightBorderPixels < _xNormalRightSideLeadLeft)
        {
            _rightBorderPixels =
                fminf(_rightBorderPixels + _moveIncrementPixels, _xNormalRightSideLeadLeft);
        }
        else if (_rightBorderPixels != _xNormalRightSideLeadLeft)
        {
            _rightBorderPixels = _rightBorderPixels - _moveIncrementPixels;
        }
    }

    if (velocityY < -3.0f)
    {
        if (_topBorderPixels < _yFastTopSide)
        {
            _topBorderPixels = fminf(_topBorderPixels + _upInc, _yFastTopSide);
        }
        else if (_topBorderPixels > _yFastTopSide)
        {
            float top = _topBorderPixels - _moveIncrementPixels;
            if (top <= _yFastTopSide)
            {
                top = _yFastTopSide;
            }
            _topBorderPixels = top;
        }
        if (_bottomBorderPixels < _yFastBottomSide)
        {
            _bottomBorderPixels = fminf(_bottomBorderPixels + _upInc, _yFastBottomSide);
        }
        else if (_bottomBorderPixels != _yFastBottomSide)
        {
            _bottomBorderPixels = _bottomBorderPixels - _moveIncrementPixels;
        }
    }
    else
    {
        if (_topBorderPixels < _yNormalTopSide)
        {
            _topBorderPixels = fminf(_topBorderPixels + _upInc, _yNormalTopSide);
        }
        else if (_topBorderPixels > _yNormalTopSide)
        {
            float top = _topBorderPixels - _moveIncrementPixels;
            if (top <= _yNormalTopSide)
            {
                top = _yNormalTopSide;
            }
            _topBorderPixels = top;
        }
        if (_bottomBorderPixels < _yNormalBottomSide)
        {
            _bottomBorderPixels = fminf(_bottomBorderPixels + _upInc, _yNormalBottomSide);
        }
        else if (_bottomBorderPixels != _yNormalBottomSide)
        {
            _bottomBorderPixels = _bottomBorderPixels - _moveIncrementPixels;
        }
    }
}

// @00633aac
void StageCamera::debugDraw()
{
    if (_drawNode == nullptr)
    {
        DrawNode* existing = static_cast<DrawNode*>(_containerObj->getParent()->getChildByTag(9999));
        if (existing == nullptr)
        {
            _drawNode = DrawNode::create(2.0f);
            _containerObj->getParent()->addChild(_drawNode, 9999);
            _drawNode->setTag(9999);
        }
        else
        {
            _drawNode = existing;
        }
    }
    _drawNode->clear();

    const float winWidth = Director::getInstance()->getWinSize().width;
    const float winHeight = Director::getInstance()->getWinSize().height;

    float x = _leftBorderPixels * 0.5f;
    _drawNode->drawSegment(Vec2(x, 0.0f), Vec2(x, winHeight), 1.0f, Color4F(1.0f, 0.0f, 0.0f, 1.0f));
    x = _rightBorderPixels * 0.5f;
    _drawNode->drawSegment(Vec2(x, 0.0f), Vec2(x, winHeight), 1.0f, Color4F(0.0f, 1.0f, 0.0f, 1.0f));
    float y = _topBorderPixels * 0.5f;
    _drawNode->drawSegment(Vec2(0.0f, y), Vec2(winWidth, y), 1.0f, Color4F(1.0f, 0.0f, 0.0f, 1.0f));
    y = _bottomBorderPixels * 0.5f;
    _drawNode->drawSegment(Vec2(0.0f, y), Vec2(winWidth, y), 1.0f, Color4F(0.0f, 1.0f, 0.0f, 1.0f));
}

// @00633cc8
void StageCamera::center()
{
    if (_focus == nullptr)
    {
        return;
    }
    setBorders();

    b2Vec2 focusPosition = _focus->GetPosition();
    // ONLINE (PC addition): a browser-level focus body frozen by a NaN-density shape (Box2D 2.0
    // NaN, online::flashNanBody) reads as Flash's stage origin, its top-left corner: Flash's
    // StageCamera then pushes the view to the stage's top-left limits (CLICK PARKOUR 3).
    if (online::flashLevel() &&
        (online::flashNanBody(_focus) || !std::isfinite(focusPosition.x) || !std::isfinite(focusPosition.y)))
    {
        focusPosition.Set(0.0f, globals::flash::stageSizeMeters.height);
    }
    const Vec2 focusWorld =
        _containerObj->convertToWorldSpace(Vec2(focusPosition.x * _ptmRatio, focusPosition.y * _ptmRatio));
    const float focusX = focusWorld.x * _scale;
    const float focusY = focusWorld.y * _scale;

    float x = _containerObj->getPosition().x * _scale;
    float y = _containerObj->getPosition().y * _scale;

    if (focusX > _rightBorderPixels)
    {
        x -= focusX - _rightBorderPixels;
        if (x < _leftLimitPixels)
        {
            x = _leftLimitPixels;
        }
    }
    else if (focusX < _leftBorderPixels)
    {
        x += _leftBorderPixels - focusX;
        if (x > _rightLimitPixels)
        {
            x = _rightLimitPixels;
        }
    }

    if (focusY < _bottomBorderPixels)
    {
        y += _bottomBorderPixels - focusY;
        if (y > _topLimitPixels)
        {
            y = _topLimitPixels;
        }
    }
    else if (focusY > _topBorderPixels)
    {
        y -= focusY - _topBorderPixels;
        if (y < _bottomLimitPixels)
        {
            y = _bottomLimitPixels;
        }
    }

    // ONLINE (PC addition): setLimits measures the window in pixels and the stage in points, so
    // below the large asset tier the view can pass the stage's right and top edges by the
    // difference. Flash's StageCamera stops exactly at its cameraBounds (the stage): browser
    // levels clamp to the window size in points as well (CLICK PARKOUR 3 builds its screens
    // against the stage's top-left corner).
    if (online::flashLevel())
    {
        const Size winSize = Director::getInstance()->getWinSize();
        const Size winSizeInPixels = Director::getInstance()->getWinSizeInPixels();
        x = std::fmax(x, _leftLimitPixels + winSize.width * _scale - winSizeInPixels.width);
        y = std::fmax(y, _bottomLimitPixels + winSize.height * _scale - winSizeInPixels.height);
    }

    const Vec2 position(x / _scale, y / _scale);
    if (_shakeAmount == 0.0f)
    {
        _containerObj->setPosition(position);
    }
    else
    {
        // CCRANDOM_MINUS1_1() is 2 * (rand() / RAND_MAX) - 1 (the division becomes * 2^-31).
        const float randomX = CCRANDOM_MINUS1_1();
        const float randomY = CCRANDOM_MINUS1_1();
        // Separate statement: the offset is not contracted into the add (fmul + fadd, no fma).
        const Vec2 shake(randomX * _halfShakeRange * _shakeAmount, randomY * _halfShakeRange * _shakeAmount);
        _containerObj->setPosition(position + shake);
        _shakeAmount = fmaxf(_shakeAmount - 0.016666668f, 0.0f);
    }

    _midScreen = Vec2((_midXPoints - position.x) / _ptmRatio, (_midYPoints - position.y) / _ptmRatio);
    _yParticleLimit = fabsf(_containerObj->getPosition().y);
}
