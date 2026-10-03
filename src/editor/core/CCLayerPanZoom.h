#pragma once
// iOS CCLayerPanZoom : CCLayer (instanceSize 0x1f1) - the pan/pinch-zoom container of the editor
// stage (EditorLayer "stage" ivar). It is the cocos2d-iphone-extensions class compiled into the
// iOS app; ported here from the iOS binary's behaviour (not from the extension's source).
//
// The editor uses: setPanBoundsRect:, setBorder:30, setMinScale:, setMaxScale:3, setScale:,
// setAnchorPoint:(0,0), setPosition:, setIsTouchEnabled: (togglePan:), mode Sheet (default).
// setPosition:/setScale: clamp the layer to panBoundsRect (+ border); touches: one finger pans,
// two fingers pinch-zoom around their midpoint (multi-touch standard delegate -> a cocos2d-x
// EventListenerTouchAllAtOnce, see EditorLayer.h "Touch order").
// PC: a single mouse cannot pinch; Phase I adds mouse-wheel zoom around the cursor as a marked
// OpenWheels addition ("// PC:") using the same scale clamp.

#include "2d/CCLayer.h"
#include "EditorGeometry.h"

class CCLayerPanZoom;

// iOS protocol CCLayerPanZoomClickDelegate (the editor never sets a delegate).
class CCLayerPanZoomClickDelegate
{
public:
    virtual ~CCLayerPanZoomClickDelegate() = default;
    virtual void layerPanZoom(CCLayerPanZoom* sender, const cocos2d::Vec2& clickedAtPoint) = 0;
    virtual void layerPanZoomTouchPositionUpdated(CCLayerPanZoom* sender,
                                                  const cocos2d::Vec2& newPos) {}
};

enum CCLayerPanZoomMode
{
    kCCLayerPanZoomModeSheet = 0,
    kCCLayerPanZoomModeFrame = 1,
};

class CCLayerPanZoom : public cocos2d::Layer
{
public:
    CREATE_FUNC(CCLayerPanZoom);  // +[CCNode node]

    void setMaxScale(double maxScale);                                     // @ios 1000bbb78
    double maxScale();                                                     // @ios 1000bbbbc
    void setMinScale(double minScale);                                     // @ios 1000bbbcc
    double minScale();                                                     // @ios 1000bbc18
    // Touch methods: iOS ccTouches*:withEvent: -> Layer::onTouches* (cocos2d-x makes the cc* names final).
    void setIsTouchEnabled(bool enabled);                                  // @ios 1000bbc28
    bool init() override;                                                  // @ios 1000bbc7c
    void onTouchesBegan(const std::vector<cocos2d::Touch*>& touches, cocos2d::Event* event) override;  // @ios 1000bbdbc
    void onTouchesMoved(const std::vector<cocos2d::Touch*>& touches, cocos2d::Event* event) override;  // @ios 1000bbf08
    void onTouchesEnded(const std::vector<cocos2d::Touch*>& touches, cocos2d::Event* event) override;  // @ios 1000bc290
    void onTouchesCancelled(const std::vector<cocos2d::Touch*>& touches, cocos2d::Event* event) override;  // @ios 1000bc494
    void onEnter() override;                                               // @ios 1000bc5b8
    void onExit() override;                                                // @ios 1000bc5ec
    void setMode(CCLayerPanZoomMode mode);                                 // @ios 1000bc644
    CCLayerPanZoomMode mode();                                             // @ios 1000bc654
    void setPanBoundsRect(const cg::Rect& rect);                           // @ios 1000bc664
    cg::Rect panBoundsRect();                                              // @ios 1000bc67c
    void setPosition(const cocos2d::Vec2& position) override;              // @ios 1000bc694
    void setPosition(float x, float y) override;  // port: holds the body (cocos routes Vec2 here)
    using cocos2d::Layer::setScale;
    using cocos2d::Layer::setPosition;
    void setScale(float scale) override;                                   // @ios 1000bc84c
    double topEdgeDistance();                                              // @ios 1000bc8b4
    double leftEdgeDistance();                                             // @ios 1000bc930
    double bottomEdgeDistance();                                           // @ios 1000bc998
    double rightEdgeDistance();                                            // @ios 1000bca00
    double minPossibleScale();                                             // @ios 1000bca7c
    // dealloc                                                             // @ios 1000bca80
    double maxTouchDistanceToClick();                                      // @ios 1000bcad4
    void setMaxTouchDistanceToClick(double distance);                      // @ios 1000bcae4
    CCLayerPanZoomClickDelegate* delegate();                               // @ios 1000bcaf4
    void setDelegate(CCLayerPanZoomClickDelegate* delegate);               // @ios 1000bcb04
    const std::vector<cocos2d::Touch*>& touches();                         // @ios 1000bcb14
    void setTouches(const std::vector<cocos2d::Touch*>& touches);          // @ios 1000bcb24
    double touchDistance();                                                // @ios 1000bcb30
    void setTouchDistance(double distance);                                // @ios 1000bcb40
    double minSpeed();                                                     // @ios 1000bcb50
    void setMinSpeed(double speed);                                        // @ios 1000bcb60
    double maxSpeed();                                                     // @ios 1000bcb70
    void setMaxSpeed(double speed);                                        // @ios 1000bcb80
    double topFrameMargin();                                               // @ios 1000bcb90
    void setTopFrameMargin(double margin);                                 // @ios 1000bcba0
    double bottomFrameMargin();                                            // @ios 1000bcbb0
    void setBottomFrameMargin(double margin);                              // @ios 1000bcbc0
    double leftFrameMargin();                                              // @ios 1000bcbd0
    void setLeftFrameMargin(double margin);                                // @ios 1000bcbe0
    double rightFrameMargin();                                             // @ios 1000bcbf0
    void setRightFrameMargin(double margin);                               // @ios 1000bcc00
    float border();                                                        // @ios 1000bcc10
    void setBorder(float border);                                          // @ios 1000bcc20

    // Fixed priority of the all-at-once listener (after the targeted editor listeners).
    static const int kTouchPriority;

protected:
    CCLayerPanZoom() = default;
    ~CCLayerPanZoom() override;

    float _border = 0.0f;                          // +0x14c  iOS "border"
    double _maxScale = 0.0;                        // +0x150
    double _minScale = 0.0;                        // +0x158
    std::vector<cocos2d::Touch*> _touches;         // +0x160  (retained)
    cg::Rect _panBoundsRect;                       // +0x168
    double _touchDistance = 0.0;                   // +0x188
    double _maxTouchDistanceToClick = 0.0;         // +0x190
    CCLayerPanZoomClickDelegate* _delegate = nullptr;  // +0x198
    CCLayerPanZoomMode _mode = kCCLayerPanZoomModeSheet;  // +0x1a0
    double _minSpeed = 0.0;                        // +0x1a8
    double _maxSpeed = 0.0;                        // +0x1b0
    double _topFrameMargin = 0.0;                  // +0x1b8
    double _bottomFrameMargin = 0.0;               // +0x1c0
    double _leftFrameMargin = 0.0;                 // +0x1c8
    double _rightFrameMargin = 0.0;                // +0x1d0
    cg::Point _prevSingleTouchPositionInLayer;     // +0x1d8
    double _singleTouchTimestamp = 0.0;            // +0x1e8
    bool _touchMoveBegan = false;                  // +0x1f0

    // port: the standard-delegate registration (+ PC mouse-wheel zoom).
    bool _isTouchEnabled = false;                  // CCLayer isTouchEnabled
    cocos2d::EventListener* _touchListener = nullptr;
    cocos2d::EventListener* _mouseListener = nullptr;  // PC
};
