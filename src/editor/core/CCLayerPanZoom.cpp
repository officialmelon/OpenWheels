#include "CCLayerPanZoom.h"

#include <cmath>
#include <limits>

#include "cocos2d.h"
#include "UIKitCompat.h"

USING_NS_CC;

const int CCLayerPanZoom::kTouchPriority = 4;

namespace {

// ccpDistance as compiled into the iOS app (@10010406c): sqrtf of the float-rounded square sum.
double pointDistance(const cg::Point& a, const cg::Point& b)
{
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return (double)std::sqrt((float)(dx * dx + dy * dy));
}

}  // namespace

CCLayerPanZoom::~CCLayerPanZoom()
{
    for (Touch* t : _touches)
    {
        t->release();
    }
    _touches.clear();
    if (_touchListener)
    {
        _eventDispatcher->removeEventListener(_touchListener);
        _touchListener = nullptr;
    }
    if (_mouseListener)
    {
        _eventDispatcher->removeEventListener(_mouseListener);
        _mouseListener = nullptr;
    }
}

// port: the iOS touch location in GL points == this layer's parent space (the EditorLayer, whose
// local unit is one iPad point).
static cg::Point glLocation(Node* layer, const Vec2& worldLocation)
{
    Node* parent = layer->getParent();
    return cg::Point(parent ? parent->convertToNodeSpace(worldLocation) : worldLocation);
}

static Vec2 glToWorld(Node* layer, const cg::Point& p)
{
    Node* parent = layer->getParent();
    return parent ? parent->convertToWorldSpace(p.toVec2()) : p.toVec2();
}

// @ios 1000bbb78
void CCLayerPanZoom::setMaxScale(double maxScale)
{
    _maxScale = maxScale;
    float scale = getScale();
    if (_maxScale <= (double)scale)
    {
        scale = (float)_maxScale;
    }
    setScale(scale);
}

// @ios 1000bbbbc
double CCLayerPanZoom::maxScale()
{
    return _maxScale;
}

// @ios 1000bbbcc
void CCLayerPanZoom::setMinScale(double minScale)
{
    _minScale = minScale;
    float scale = getScale();
    float newScale = (float)minScale;
    if (minScale <= (double)scale)
    {
        newScale = scale;
    }
    setScale(newScale);
}

// @ios 1000bbc18
double CCLayerPanZoom::minScale()
{
    return _minScale;
}

// @ios 1000bbc28
void CCLayerPanZoom::setIsTouchEnabled(bool enabled)
{
    // [super setIsTouchEnabled:] (de)registers the standard touch delegate.
    _isTouchEnabled = enabled;
    if (_running)
    {
        if (enabled && !_touchListener)
        {
            auto listener = EventListenerTouchAllAtOnce::create();
            listener->onTouchesBegan = [this](const std::vector<Touch*>& t, Event* e) { onTouchesBegan(t, e); };
            listener->onTouchesMoved = [this](const std::vector<Touch*>& t, Event* e) { onTouchesMoved(t, e); };
            listener->onTouchesEnded = [this](const std::vector<Touch*>& t, Event* e) { onTouchesEnded(t, e); };
            listener->onTouchesCancelled = [this](const std::vector<Touch*>& t, Event* e) { onTouchesCancelled(t, e); };
            _eventDispatcher->addEventListenerWithFixedPriority(listener, kTouchPriority);
            _touchListener = listener;
        }
        else if (!enabled && _touchListener)
        {
            _eventDispatcher->removeEventListener(_touchListener);
            _touchListener = nullptr;
        }
    }
    if (!enabled)
    {
        for (Touch* t : _touches)
        {
            t->release();
        }
        _touches.clear();
    }
}

// @ios 1000bbc7c
bool CCLayerPanZoom::init()
{
    if (!Layer::init())
    {
        return false;
    }
    setIgnoreAnchorPointForPosition(false);
    _isTouchEnabled = true;
    setMaxScale(3.0);
    setMinScale(0.5);
    _touches.clear();
    setPanBoundsRect(cg::RectNull);
    setTouchDistance(0.0);
    setMaxTouchDistanceToClick(15.0);
    setMode(kCCLayerPanZoomModeSheet);
    setMinSpeed(100.0);
    setMaxSpeed(1000.0);
    setTopFrameMargin(100.0);
    setBottomFrameMargin(100.0);
    setLeftFrameMargin(100.0);
    setRightFrameMargin(100.0);
    return true;
}

// @ios 1000bbdbc
void CCLayerPanZoom::onTouchesBegan(const std::vector<Touch*>& touches, Event* event)
{
    for (Touch* t : touches)
    {
        t->retain();
        _touches.push_back(t);
    }
    if (_touches.size() == 1)
    {
        _touchMoveBegan = false;
        _singleTouchTimestamp = utils::gettime();  // [NSDate timeIntervalSinceReferenceDate]
    }
    else
    {
        _singleTouchTimestamp = std::numeric_limits<double>::infinity();
    }
}

// @ios 1000bbf08
void CCLayerPanZoom::onTouchesMoved(const std::vector<Touch*>& touches, Event* event)
{
    if (_touches.empty())
    {
        return;
    }
    double touchDistance;
    Touch* touch0 = _touches[0];
    if (_touches.size() < 2)
    {
        cg::Point current = glLocation(this, touch0->getLocation());
        cg::Point previous = glLocation(this, touch0->getPreviousLocation());
        Vec2 position = getPosition();
        setPosition(Vec2((float)((current.x - previous.x) + position.x),
                         (float)((current.y - previous.y) + position.y)));
        touchDistance = _touchDistance + pointDistance(current, previous);
    }
    else
    {
        Touch* touch1 = _touches[1];
        cg::Point cur0 = glLocation(this, touch0->getLocation());
        cg::Point cur1 = glLocation(this, touch1->getLocation());
        cg::Point prev0 = glLocation(this, touch0->getPreviousLocation());
        cg::Point prev1 = glLocation(this, touch1->getPreviousLocation());
        cg::Point curMid((cur1.x + cur0.x) * 0.5, (cur1.y + cur0.y) * 0.5);
        cg::Point prevMid((prev1.x + prev0.x) * 0.5, (prev1.y + prev0.y) * 0.5);
        float prevScale = getScale();
        double curDistance = pointDistance(cur0, cur1);
        double prevDistance = pointDistance(prev0, prev1);
        setScale(getScale() * (float)(curDistance / prevDistance));
        if (getScale() != prevScale)
        {
            Vec2 midInLayer = convertToNodeSpace(glToWorld(this, curMid));
            Vec2 anchor = getAnchorPoint();
            Size size = getContentSize();
            float scaleX = getScale();
            float scaleY = getScale();
            Vec2 position = getPosition();
            setPosition(Vec2(
                (float)(position.x + ((double)scaleX - (double)prevScale) *
                                         ((double)size.width * anchor.x - midInLayer.x)),
                (float)(position.y + ((double)scaleY - (double)prevScale) *
                                         ((double)size.height * anchor.y - midInLayer.y))));
        }
        if (!(prevMid.x == curMid.x && prevMid.y == curMid.y))
        {
            Vec2 position = getPosition();
            setPosition(Vec2((float)((curMid.x - prevMid.x) + position.x),
                             (float)((curMid.y - prevMid.y) + position.y)));
        }
        touchDistance = std::numeric_limits<double>::infinity();
    }
    setTouchDistance(touchDistance);
}

// @ios 1000bc290
void CCLayerPanZoom::onTouchesEnded(const std::vector<Touch*>& touches, Event* event)
{
    if (_touchDistance < _maxTouchDistanceToClick && _delegate && _touches.size() == 1)
    {
        Touch* touch = _touches[0];
        Vec2 point = convertToNodeSpace(touch->getLocation());
        _delegate->layerPanZoom(this, point);
    }
    for (Touch* t : touches)
    {
        for (auto it = _touches.begin(); it != _touches.end(); ++it)
        {
            if (*it == t)
            {
                (*it)->release();
                _touches.erase(it);
                break;
            }
        }
    }
    if (_touches.empty())
    {
        setTouchDistance(0.0);
    }
}

// @ios 1000bc494
void CCLayerPanZoom::onTouchesCancelled(const std::vector<Touch*>& touches, Event* event)
{
    for (Touch* t : touches)
    {
        for (auto it = _touches.begin(); it != _touches.end(); ++it)
        {
            if (*it == t)
            {
                (*it)->release();
                _touches.erase(it);
                break;
            }
        }
    }
    if (_touches.empty())
    {
        setTouchDistance(0.0);
    }
}

// @ios 1000bc5b8
void CCLayerPanZoom::onEnter()
{
    Layer::onEnter();
    // CCLayer onEnter registers the touch delegate when touch is enabled.
    setIsTouchEnabled(_isTouchEnabled);

    // PC: mouse-wheel zoom around the cursor (a mouse cannot pinch). OpenWheels addition.
    if (!_mouseListener)
    {
        auto mouse = EventListenerMouse::create();
        mouse->onMouseScroll = [this](EventMouse* e) {
            float scrollY = e->getScrollY();
            if (scrollY == 0.0f || !isVisible())
            {
                return;
            }
            float prevScale = getScale();
            setScale(prevScale * std::pow(1.1f, -scrollY));
            if (getScale() != prevScale)
            {
                Vec2 cursorWorld(e->getCursorX(), e->getCursorY());
                Vec2 cursorInLayer = convertToNodeSpace(cursorWorld);
                // keep the point under the cursor fixed (same formula as the pinch branch)
                Vec2 anchor = getAnchorPoint();
                Size size = getContentSize();
                Vec2 position = getPosition();
                float delta = getScale() - prevScale;
                setPosition(Vec2(position.x + delta * (size.width * anchor.x - cursorInLayer.x),
                                 position.y + delta * (size.height * anchor.y - cursorInLayer.y)));
            }
        };
        _eventDispatcher->addEventListenerWithFixedPriority(mouse, kTouchPriority);
        _mouseListener = mouse;
    }
}

// @ios 1000bc5ec
void CCLayerPanZoom::onExit()
{
    unscheduleAllCallbacks();
    if (_touchListener)
    {
        _eventDispatcher->removeEventListener(_touchListener);
        _touchListener = nullptr;
    }
    if (_mouseListener)
    {
        _eventDispatcher->removeEventListener(_mouseListener);
        _mouseListener = nullptr;
    }
    Layer::onExit();
}

// @ios 1000bc644
void CCLayerPanZoom::setMode(CCLayerPanZoomMode mode)
{
    _mode = mode;
}

// @ios 1000bc654
CCLayerPanZoomMode CCLayerPanZoom::mode()
{
    return _mode;
}

// @ios 1000bc664
void CCLayerPanZoom::setPanBoundsRect(const cg::Rect& rect)
{
    _panBoundsRect = rect;
}

// @ios 1000bc67c
cg::Rect CCLayerPanZoom::panBoundsRect()
{
    return _panBoundsRect;
}

// @ios 1000bc694
// The editor's build of the extension clamps the layer so the bounds (scaled) never leave the
// window by more than `border`: x in [winW - (border + w * scale), border], same for y.
// port: cocos routes setPosition(const Vec2&) to the (x, y) overload, so the body lives there and
// only calls the base (x, y) overload (the Vec2 one would dispatch back here).
void CCLayerPanZoom::setPosition(const Vec2& position)
{
    setPosition(position.x, position.y);
}

void CCLayerPanZoom::setPosition(float newX, float newY)
{
    Layer::setPosition(newX, newY);
    if (cg::rectIsNull(_panBoundsRect))
    {
        return;
    }
    Size winSize = uikit::windowSize();
    double winW = winSize.width;
    double winH = winSize.height;
    double boundsW = _panBoundsRect.size.width;
    float scaleX = getScale();
    double boundsH = _panBoundsRect.size.height;
    float scaleY = getScale();
    double px = getPosition().x;
    double py = getPosition().y;
    float border = _border;

    float x;
    if ((double)getPosition().x > (double)border)
    {
        x = border;
    }
    else
    {
        x = (float)(winW - (double)(float)(border + (float)(boundsW * (double)scaleX)));
        if (!((double)getPosition().x < (double)x))
        {
            x = (float)px;
        }
    }
    double y;
    if ((double)getPosition().y > (double)border)
    {
        y = (double)border;
    }
    else
    {
        float minY = (float)(winH - (double)(float)(border + (float)(boundsH * (double)scaleY)));
        y = (double)minY;
        if (!((double)getPosition().y < (double)minY))
        {
            y = (double)(float)py;
        }
    }
    Layer::setPosition(x, (float)y);
}

// @ios 1000bc84c
void CCLayerPanZoom::setScale(float scale)
{
    double s = _minScale;
    if (s <= (double)scale)
    {
        s = (double)scale;
    }
    s = std::fmin(s, _maxScale);
    Layer::setScale((float)s);
}

// @ios 1000bc8b4
double CCLayerPanZoom::topEdgeDistance()
{
    Rect box = getBoundingBox();
    Vec2 position = getPosition();
    Vec2 anchor = getAnchorPoint();
    double d = ((_panBoundsRect.origin.y + _panBoundsRect.size.height) - position.y) +
               ((double)anchor.y * box.size.height - box.size.height);
    if (d <= 0.0)
    {
        d = 0.0;
    }
    return (double)(long long)d;
}

// @ios 1000bc930
double CCLayerPanZoom::leftEdgeDistance()
{
    Rect box = getBoundingBox();
    Vec2 position = getPosition();
    Vec2 anchor = getAnchorPoint();
    double d = position.x - (_panBoundsRect.origin.x + (double)box.size.width * anchor.x);
    if (d <= 0.0)
    {
        d = 0.0;
    }
    return (double)(long long)d;
}

// @ios 1000bc998
double CCLayerPanZoom::bottomEdgeDistance()
{
    Rect box = getBoundingBox();
    Vec2 position = getPosition();
    Vec2 anchor = getAnchorPoint();
    double d = position.y - (_panBoundsRect.origin.y + (double)box.size.height * anchor.y);
    if (d <= 0.0)
    {
        d = 0.0;
    }
    return (double)(long long)d;
}

// @ios 1000bca00
double CCLayerPanZoom::rightEdgeDistance()
{
    Rect box = getBoundingBox();
    Vec2 position = getPosition();
    Vec2 anchor = getAnchorPoint();
    double d = ((_panBoundsRect.origin.x + _panBoundsRect.size.width) - position.x) +
               ((double)anchor.x * box.size.width - box.size.width);
    if (d <= 0.0)
    {
        d = 0.0;
    }
    return (double)(long long)d;
}

// @ios 1000bca7c
double CCLayerPanZoom::minPossibleScale()
{
    return minScale();
}

// @ios 1000bcad4
double CCLayerPanZoom::maxTouchDistanceToClick()
{
    return _maxTouchDistanceToClick;
}

// @ios 1000bcae4
void CCLayerPanZoom::setMaxTouchDistanceToClick(double distance)
{
    _maxTouchDistanceToClick = distance;
}

// @ios 1000bcaf4
CCLayerPanZoomClickDelegate* CCLayerPanZoom::delegate()
{
    return _delegate;
}

// @ios 1000bcb04
void CCLayerPanZoom::setDelegate(CCLayerPanZoomClickDelegate* delegate)
{
    _delegate = delegate;
}

// @ios 1000bcb14
const std::vector<Touch*>& CCLayerPanZoom::touches()
{
    return _touches;
}

// @ios 1000bcb24
void CCLayerPanZoom::setTouches(const std::vector<Touch*>& touches)
{
    for (Touch* t : touches)
    {
        t->retain();
    }
    for (Touch* t : _touches)
    {
        t->release();
    }
    _touches = touches;
}

// @ios 1000bcb30
double CCLayerPanZoom::touchDistance()
{
    return _touchDistance;
}

// @ios 1000bcb40
void CCLayerPanZoom::setTouchDistance(double distance)
{
    _touchDistance = distance;
}

// @ios 1000bcb50
double CCLayerPanZoom::minSpeed()
{
    return _minSpeed;
}

// @ios 1000bcb60
void CCLayerPanZoom::setMinSpeed(double speed)
{
    _minSpeed = speed;
}

// @ios 1000bcb70
double CCLayerPanZoom::maxSpeed()
{
    return _maxSpeed;
}

// @ios 1000bcb80
void CCLayerPanZoom::setMaxSpeed(double speed)
{
    _maxSpeed = speed;
}

// @ios 1000bcb90
double CCLayerPanZoom::topFrameMargin()
{
    return _topFrameMargin;
}

// @ios 1000bcba0
void CCLayerPanZoom::setTopFrameMargin(double margin)
{
    _topFrameMargin = margin;
}

// @ios 1000bcbb0
double CCLayerPanZoom::bottomFrameMargin()
{
    return _bottomFrameMargin;
}

// @ios 1000bcbc0
void CCLayerPanZoom::setBottomFrameMargin(double margin)
{
    _bottomFrameMargin = margin;
}

// @ios 1000bcbd0
double CCLayerPanZoom::leftFrameMargin()
{
    return _leftFrameMargin;
}

// @ios 1000bcbe0
void CCLayerPanZoom::setLeftFrameMargin(double margin)
{
    _leftFrameMargin = margin;
}

// @ios 1000bcbf0
double CCLayerPanZoom::rightFrameMargin()
{
    return _rightFrameMargin;
}

// @ios 1000bcc00
void CCLayerPanZoom::setRightFrameMargin(double margin)
{
    _rightFrameMargin = margin;
}

// @ios 1000bcc10
float CCLayerPanZoom::border()
{
    return _border;
}

// @ios 1000bcc20
void CCLayerPanZoom::setBorder(float border)
{
    _border = border;
}
