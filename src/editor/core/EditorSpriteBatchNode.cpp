#include "EditorSpriteBatchNode.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "cocos2d.h"
#include "CharacterRef.h"
#include "EditorLayer.h"
#include "EditorUndoManager.h"
#include "Special.h"
#include "UIKitCompat.h"
#include "WreckingBallRef.h"

USING_NS_CC;

const int EditorSpriteBatchNode::kTouchPriority = 2;

namespace {

const float kPi = 3.1415927f;
const char* const kUndoStackUpdated = "undo_stack_updated";
const char* const kSelRefChange = "sel_ref_change";
const char* const kSelRectChanged = "sel_rect_changed";

// atan(dy/dx) in (-pi, pi] the way the iOS code builds it: atanf, minus pi when dx < 0.
float angleTo(float dx, float dy)
{
    float a = std::atan(dy / dx);
    return (0.0f <= dx) ? a : a - kPi;
}

EditorLayer* editorLayerOf(Node* sbn)
{
    Node* stage = sbn->getParent();
    return stage ? static_cast<EditorLayer*>(stage->getParent()) : nullptr;
}

}  // namespace

EditorSpriteBatchNode* EditorSpriteBatchNode::create(const std::string& fileImage, ssize_t capacity)
{
    EditorSpriteBatchNode* batchNode = new (std::nothrow) EditorSpriteBatchNode();
    if (batchNode)
    {
        Texture2D* texture = Director::getInstance()->getTextureCache()->addImage(fileImage);
        if (texture && batchNode->initWithTexture(texture, capacity))
        {
            batchNode->autorelease();
            return batchNode;
        }
    }
    delete batchNode;
    return nullptr;
}

EditorSpriteBatchNode::EditorSpriteBatchNode()
{
}

// @ios 1000bcfd8 (dealloc)
EditorSpriteBatchNode::~EditorSpriteBatchNode()
{
    if (_touchListener)
    {
        _eventDispatcher->removeEventListener(_touchListener);
        _touchListener = nullptr;
    }
    uikit::NotificationCenter::removeObserver(this);
    CC_SAFE_RELEASE(_undoManager);
}

// @ios 1000bce60
bool EditorSpriteBatchNode::initWithTexture(Texture2D* tex, ssize_t capacity)
{
    if (!SpriteBatchNode::initWithTexture(tex, capacity))
    {
        return false;
    }
    _selectionRect = cg::Rect(0.0, 0.0, 0.0, 0.0);
    float ptm = Special::sessionPtmRatio();  // [Session sharedSession].ptmRatio (editor mode, stage units)
    stageWidth = ptm * 320.0f;
    stageHeight = ptm * 160.0f;
    _enableEdit = true;
    edgePanning = false;
    _snapToAngle = false;
    _lockToAxis = false;
    _rotateRefsIndependently = false;
    _refs.clear();
    _selectedRefs.clear();
    highlightedRefs.clear();
    _undoManager = EditorUndoManager::create();
    _undoManager->retain();
    _undoManager->setLevelsOfUndo(0x28);
    circNormalOpacity = 63.0f;
    circPressedOpacity = 153.0f;
    return true;
}

// @ios 1000bd058
void EditorSpriteBatchNode::onEnter()
{
    if (_touchListener)
    {
        _eventDispatcher->removeEventListener(_touchListener);
    }
    _touchListener = EventListenerTouchOneByOne::create();
    _touchListener->setSwallowTouches(true);
    _touchListener->onTouchBegan = [this](Touch* t, Event* e) { return ccTouchBegan(t, e); };
    _touchListener->onTouchMoved = [this](Touch* t, Event* e) { ccTouchMoved(t, e); };
    _touchListener->onTouchEnded = [this](Touch* t, Event* e) { ccTouchEnded(t, e); };
    _touchListener->onTouchCancelled = [this](Touch* t, Event* e) { ccTouchCancelled(t, e); };
    _eventDispatcher->addEventListenerWithFixedPriority(_touchListener, kTouchPriority);

    uikit::NotificationCenter::addObserver(this, EditorUndoManager::kDidUndoChangeNotification, _undoManager,
                                           [this](void* object, void*) { undoManagerDidUndo(object); });
    uikit::NotificationCenter::addObserver(this, EditorUndoManager::kDidRedoChangeNotification, _undoManager,
                                           [this](void* object, void*) { undoManagerDidRedo(object); });
    uikit::NotificationCenter::addObserver(this, kSelRectChanged, nullptr,
                                           [this](void*, void*) { updateSelectionRect(); });
    SpriteBatchNode::onEnter();
}

// @ios 1000bd13c
void EditorSpriteBatchNode::onEnterTransitionDidFinish()
{
    // iOS only schedules the update (no [super onEnterTransitionDidFinish]).
    scheduleUpdate();
}

// @ios 1000bd140
void EditorSpriteBatchNode::onExit()
{
    uikit::NotificationCenter::removeObserver(this);
    unscheduleUpdate();
    if (_touchListener)
    {
        _eventDispatcher->removeEventListener(_touchListener);
        _touchListener = nullptr;
    }
    SpriteBatchNode::onExit();
}

// ---- coordinate helpers (port) ----------------------------------------------------------------
// iOS "world" space is the window in points, which is the EditorLayer's local space here.

cg::Point EditorSpriteBatchNode::touchLocationInNode(Touch* touch)
{
    return cg::Point(convertToNodeSpace(touch->getLocation()));
}

cg::Point EditorSpriteBatchNode::previousTouchLocationInNode(Touch* touch)
{
    return cg::Point(convertToNodeSpace(touch->getPreviousLocation()));
}

static cg::Point nodeToEditorWorld(Node* node, const cg::Point& p)
{
    Vec2 world = node->convertToWorldSpace(p.toVec2());
    EditorLayer* layer = editorLayerOf(node);
    return cg::Point(layer ? layer->convertToNodeSpace(world) : world);
}

static cg::Point editorWorldToNode(Node* node, const cg::Point& p)
{
    EditorLayer* layer = editorLayerOf(node);
    Vec2 world = layer ? layer->convertToWorldSpace(p.toVec2()) : p.toVec2();
    return cg::Point(node->convertToNodeSpace(world));
}

static cg::Point touchLocationInEditor(Node* node, Touch* touch)
{
    EditorLayer* layer = editorLayerOf(node);
    return cg::Point(layer ? layer->convertToNodeSpace(touch->getLocation()) : touch->getLocation());
}

// iOS textureRect.size.width in points (the circle sprites carry the atlas-to-point scale).
static double textureWidthInPoints(Sprite* sprite)
{
    return (double)(sprite->getTextureRect().size.width * sprite->getScaleX());
}

// ---- touches ----------------------------------------------------------------------------------

// @ios 1000bd1b4
bool EditorSpriteBatchNode::ccTouchBegan(Touch* touch, Event* event)
{
    if (!_enableEdit || edgePanning)
    {
        return false;
    }
    cg::Point location = touchLocationInNode(touch);
    EditorLayer* layer = editorLayerOf(this);
    Sprite* rotCirc = layer->rotCirc();
    Sprite* moveCirc = layer->moveCirc();
    cg::Point world = nodeToEditorWorld(this, location);

    bool onCircles = false;
    if (moveCirc->isVisible())
    {
        cg::Rect box(rotCirc->getBoundingBox());
        if (cg::rectContainsPoint(box, world))
        {
            Vec2 c = rotCirc->getPosition();
            double dx = (double)c.x - world.x;
            double dy = (double)c.y - world.y;
            double distance = (double)(float)std::sqrt(dy * dy + dx * dx);
            if (textureWidthInPoints(rotCirc) * 0.5 > distance)
            {
                if (textureWidthInPoints(moveCirc) * 0.5 <= distance)
                {
                    if (rotCirc->isVisible())
                    {
                        onCircles = true;
                        // rotate
                        rotCirc->setOpacity((GLubyte)(int)circPressedOpacity);
                        state = EditorSBNStateRotate;
                        _undoManager->beginUndoGrouping();
                        for (Special* ref : _selectedRefs)
                        {
                            float rotation = ref->getRotation();
                            _undoManager->prepareWithInvocationTarget(
                                ref, [rotation](Special* s) { s->setRotation(rotation); });
                            Vec2 pos = ref->getPosition();
                            float x = pos.x;
                            float y = pos.y;
                            _undoManager->prepareWithInvocationTarget(ref, [x, y](Special* s) { s->setX(x, y); });
                        }
                        _undoManager->endUndoGrouping();
                        uikit::NotificationCenter::postNotification(kUndoStackUpdated, _undoManager);
                        cg::Point center = editorWorldToNode(this, cg::Point(rotCirc->getPosition()));
                        float fdx = (float)(location.x - center.x);
                        rotateStartAngle = angleTo(fdx, (float)(location.y - center.y));
                    }
                }
                else
                {
                    onCircles = true;
                    // move
                    moveCirc->setOpacity((GLubyte)(int)circPressedOpacity);
                    state = EditorSBNStateDrag;
                    draggingTouch = touch;
                    dragStartPos = location;
                    _undoManager->beginUndoGrouping();
                    for (Special* ref : _selectedRefs)
                    {
                        ref->setStartPos(cg::Point(ref->getPosition()));
                        Vec2 pos = ref->getPosition();
                        float x = pos.x;
                        float y = pos.y;
                        _undoManager->prepareWithInvocationTarget(ref, [x, y](Special* s) { s->setX(x, y); });
                    }
                    _undoManager->endUndoGrouping();
                    uikit::NotificationCenter::postNotification(kUndoStackUpdated, _undoManager);
                }
            }
        }
    }
    if (onCircles)
    {
        return true;
    }

    // Touch on a ref (topmost by zOrder) or start a marquee.
    std::vector<Special*> hits;
    for (Special* ref : _refs)
    {
        if (ref->containsPoint(location) && !ref->locked())
        {
            hits.push_back(ref);
        }
    }
    if (hits.empty())
    {
        marqueeStart = location;
        state = EditorSBNStateMarquee;
        return true;
    }
    std::stable_sort(hits.begin(), hits.end(), [](Special* a, Special* b) {
        return a->getLocalZOrder() < b->getLocalZOrder();
    });
    Special* top = hits.back();
    state = EditorSBNStateDrag;
    draggingTouch = touch;
    top->setStartPos(cg::Point(top->getPosition()));
    if (_selectedRefs.size() != 1 || _selectedRefs.at(0) != top)
    {
        _undoManager->beginUndoGrouping();
        Vector<Special*> previousSelection = _selectedRefs;
        _undoManager->prepareWithInvocationTarget(
            this, [previousSelection](EditorSpriteBatchNode* sbn) { sbn->setSelectedRefs(previousSelection); });
        Vec2 pos = top->getPosition();
        float x = pos.x;
        float y = pos.y;
        _undoManager->prepareWithInvocationTarget(top, [x, y](Special* s) { s->setX(x, y); });
        _undoManager->endUndoGrouping();
        uikit::NotificationCenter::postNotification(kUndoStackUpdated, _undoManager);
        Vector<Special*> selection;
        selection.pushBack(top);
        setSelectedRefs(selection);
    }
    return true;
}

// @ios 1000bd8cc
void EditorSpriteBatchNode::ccTouchMoved(Touch* touch, Event* event)
{
    switch (state)
    {
    case EditorSBNStateDrag:
        dragRef(touch, event);
        return;
    case EditorSBNStateRotate:
        rotateRefs(touch, event);
        return;
    case EditorSBNStateModify:
        modifyRef(touch, event);
        return;
    case EditorSBNStateMarquee:
        updateMarquee(touch, event);
        return;
    default:
        return;
    }
}

// @ios 1000bd910
void EditorSpriteBatchNode::ccTouchCancelled(Touch* touch, Event* event)
{
    ccTouchEnded(touch, event);
}

// @ios 1000bd914
void EditorSpriteBatchNode::ccTouchEnded(Touch* touch, Event* event)
{
    touchLocationInNode(touch);  // computed and unused on iOS
    if ((unsigned int)state - 1u < 2u || state == EditorSBNStateModify)
    {
        positionMoveRotCircs();
        EditorLayer* layer = editorLayerOf(this);
        GLubyte opacity = (GLubyte)(int)circNormalOpacity;
        layer->moveCirc()->setOpacity(opacity);
        layer->rotCirc()->setOpacity(opacity);
        draggingTouch = nullptr;
        state = EditorSBNStateIdle;
        if (edgePanning)
        {
            edgePanning = false;
            unschedule(CC_SCHEDULE_SELECTOR(EditorSpriteBatchNode::handleEdgePan));
        }
    }
    else if (state == EditorSBNStateMarquee)
    {
        selectRefsInMarquee();
        state = EditorSBNStateIdle;
    }
}

// @ios 1000bda2c
void EditorSpriteBatchNode::updateMarquee(Touch* touch, Event* event)
{
    cg::Point location = touchLocationInNode(touch);
    marqueeEnd = location;
    double startX = marqueeStart.x;
    double startY = marqueeStart.y;
    highlightedRefs.clear();
    double height = (location.y <= startY) ? startY - location.y : location.y - startY;
    double width = (location.x <= startX) ? startX - location.x : location.x - startX;
    cg::Rect marquee((double)(float)std::fmin(location.x, startX), (double)(float)std::fmin(location.y, startY),
                     (double)(float)width, (double)(float)height);
    for (Special* ref : _refs)
    {
        cg::Rect box(ref->refBoundingBox());
        if ((cg::rectContainsRect(marquee, box) || cg::rectIntersectsRect(marquee, box)) && !ref->locked())
        {
            highlightedRefs.pushBack(ref);
        }
    }
}

// @ios 1000bdc84
void EditorSpriteBatchNode::selectRefsInMarquee()
{
    marqueeEnd = cg::PointZero;
    if (_selectedRefs.empty() && highlightedRefs.empty())
    {
        return;
    }
    bool sameSet = true;
    for (Special* ref : _selectedRefs)
    {
        if (!highlightedRefs.contains(ref))
        {
            sameSet = false;
            break;
        }
    }
    if (sameSet)
    {
        for (Special* ref : highlightedRefs)
        {
            if (!_selectedRefs.contains(ref))
            {
                sameSet = false;
                break;
            }
        }
    }
    if (!sameSet)
    {
        Vector<Special*> previousSelection = _selectedRefs;
        _undoManager->prepareWithInvocationTarget(
            this, [previousSelection](EditorSpriteBatchNode* sbn) { sbn->setSelectedRefs(previousSelection); });
        uikit::NotificationCenter::postNotification(kUndoStackUpdated, _undoManager);
        setSelectedRefs(highlightedRefs);
    }
    highlightedRefs.clear();
}

// @ios 1000bdd98
void EditorSpriteBatchNode::updateSelectionRect()
{
    if (_selectedRefs.empty())
    {
        _selectionRect = cg::Rect(0.0, 0.0, 0.0, 0.0);
        return;
    }
    cg::Rect first(_selectedRefs.at(0)->refBoundingBox());
    double maxX = first.origin.x + first.size.width;
    double maxY = first.origin.y + first.size.height;
    float minXf = (float)first.origin.x;
    float maxXf = (float)maxX;
    float minYf = (float)first.origin.y;
    float maxYf = (float)maxY;
    for (ssize_t i = 1; i < _selectedRefs.size(); i++)
    {
        cg::Rect box(_selectedRefs.at(i)->refBoundingBox());
        maxX = box.origin.x + box.size.width;
        if (maxX <= (double)maxXf)
        {
            maxX = (double)maxXf;
        }
        maxY = box.origin.y + box.size.height;
        if (maxY <= (double)maxYf)
        {
            maxY = (double)maxYf;
        }
        if (box.origin.x <= (double)minXf)
        {
            minXf = (float)box.origin.x;
        }
        maxXf = (float)maxX;
        if (box.origin.y <= (double)minYf)
        {
            minYf = (float)box.origin.y;
        }
        maxYf = (float)maxY;
    }
    _selectionRect = cg::Rect((double)minXf, (double)minYf, (double)(maxXf - minXf), (double)(maxYf - minYf));
}

// @ios 1000bded8
void EditorSpriteBatchNode::dragRef(Touch* touch, Event* event)
{
    cg::Point gl = touchLocationInEditor(this, touch);
    cg::Point location = touchLocationInNode(touch);
    cg::Point previous = previousTouchLocationInNode(touch);
    Size winSize = uikit::windowSize();
    double right = (double)winSize.width + -48.0;
    double top = (double)winSize.height + -48.0;
    bool inside = 48.0 <= gl.x && gl.x <= right && 48.0 <= gl.y && gl.y <= top;
    if (inside)
    {
        if (edgePanning)
        {
            edgePanning = false;
            unschedule(CC_SCHEDULE_SELECTOR(EditorSpriteBatchNode::handleEdgePan));
        }
    }
    else if (!edgePanning)
    {
        edgePanning = true;
        schedule(CC_SCHEDULE_SELECTOR(EditorSpriteBatchNode::handleEdgePan));
    }

    if (!_lockToAxis)
    {
        cg::Point movement((double)(float)(location.x - previous.x), (double)(float)(location.y - previous.y));
        moveRefs(_selectedRefs, movement);
    }
    else
    {
        float dx = (float)(location.x - dragStartPos.x);
        float dy = (float)(location.y - dragStartPos.y);
        if (std::fabs(dx) <= std::fabs(dy))
        {
            for (Special* ref : _selectedRefs)
            {
                cg::Point start = ref->startPos();
                ref->setX((float)start.x, (float)(start.y + (double)dy));
            }
        }
        else
        {
            for (Special* ref : _selectedRefs)
            {
                cg::Point start = ref->startPos();
                ref->setX((float)(start.x + (double)dx), (float)start.y);
            }
        }
    }
    updateSelectionRect();
    positionMoveRotCircs();
}

// @ios 1000be25c
void EditorSpriteBatchNode::moveRefs(const Vector<Special*>& refs, const cg::Point& movement)
{
    // (sic) both loops walk the selection, not `refs`.
    (void)refs;
    float mx = (float)movement.x;
    float my = (float)movement.y;
    double moveX;
    double moveY;
    if (_selectedRefs.empty())
    {
        moveX = std::numeric_limits<double>::infinity();
        moveY = std::numeric_limits<double>::infinity();
    }
    else
    {
        bool canMoveX = true;
        bool canMoveY = true;
        float bestX = std::numeric_limits<float>::infinity();
        float bestY = std::numeric_limits<float>::infinity();
        Vector<Special*> selection = _selectedRefs;
        for (Special* ref : selection)
        {
            Vec2 pos = ref->getPosition();
            cg::Point desired((double)pos.x + (double)mx, (double)pos.y + (double)my);
            cg::Point limited = limitPosForRef(ref, desired);
            canMoveX = ((double)pos.x != limited.x || mx == 0.0f) && canMoveX;
            canMoveY = ((double)pos.y != limited.y || my == 0.0f) && canMoveY;
            if (!canMoveY && !canMoveX)
            {
                updateSelectionRect();
                positionMoveRotCircs();
                return;
            }
            float dx = (float)(limited.x - (double)pos.x);
            float dy = (float)(limited.y - (double)pos.y);
            if (std::fabs(bestX) <= std::fabs(dx))
            {
                dx = bestX;
            }
            bestX = dx;
            if (std::fabs(bestY) <= std::fabs(dy))
            {
                dy = bestY;
            }
            bestY = dy;
        }
        moveX = (double)bestX;
        moveY = (double)bestY;
    }
    Vector<Special*> selection = _selectedRefs;
    for (Special* ref : selection)
    {
        Vec2 pos = ref->getPosition();
        ref->setX((float)((double)pos.x + moveX), (float)((double)pos.y + moveY));
    }
}

// @ios 1000be510
cg::Point EditorSpriteBatchNode::limitPosForRef(Special* ref, const cg::Point& desiredPos)
{
    Vec2 pos = ref->getPosition();
    cg::Rect box(ref->refBoundingBox());
    float minX = (float)((double)pos.x - box.origin.x);
    float maxXOffset = (float)((double)minX - box.size.width);
    float minY = (float)((double)pos.y - box.origin.y);
    float maxYOffset = (float)((double)minY - box.size.height);
    float x = std::fmin(std::fmax(minX, (float)desiredPos.x), stageWidth + maxXOffset);
    float y = std::fmin(std::fmax(minY, (float)desiredPos.y), stageHeight + maxYOffset);
    return cg::Point((double)x, (double)y);
}

// @ios 1000be5f0
void EditorSpriteBatchNode::handleEdgePan(float dt)
{
    if (!draggingTouch)
    {
        return;
    }
    cg::Point gl = touchLocationInEditor(this, draggingTouch);
    Size winSize = uikit::windowSize();
    double x = gl.x;
    double y = gl.y;
    float panX;
    if (48.0 <= x)
    {
        panX = 0.0f;
        if ((double)winSize.width + -48.0 < x)
        {
            float f = (float)((x - (double)winSize.width) * 0.020833333333333332 + 1.0);
            panX = f * f * -10.0f;
        }
    }
    else
    {
        float f = 1.0f;
        if (x != 0.0)
        {
            f = (float)(x * -0.020833333333333332 + 1.0);
        }
        panX = f * f * 10.0f;
    }
    float panY;
    if (48.0 <= y)
    {
        panY = 0.0f;
        if (!(y <= (double)winSize.height + -48.0))
        {
            float f = (float)((y - (double)winSize.height) * 0.020833333333333332 + 1.0);
            panY = f * f * -10.0f;
        }
    }
    else
    {
        float f = 1.0f;
        if (y != 0.0)
        {
            f = (float)(y * -0.020833333333333332 + 1.0);
        }
        panY = f * f * 10.0f;
    }
    if (panY == 0.0f && panX == 0.0f)
    {
        return;
    }
    Node* stage = getParent();
    Vec2 before = stage->getPosition();
    stage->setPosition(Vec2((float)((double)before.x + (double)panX), (float)((double)before.y + (double)panY)));
    Vec2 after = stage->getPosition();
    if (after.x == before.x && after.y == before.y)
    {
        return;
    }
    float scale = stage->getScale();
    cg::Point movement((double)((1.0f / scale) * -panX), (double)((1.0f / scale) * -panY));
    moveRefs(_selectedRefs, movement);
    updateSelectionRect();
    positionMoveRotCircs();
}

// @ios 1000be850
void EditorSpriteBatchNode::rotateRefs(Touch* touch, Event* event)
{
    cg::Point location = touchLocationInNode(touch);
    cg::Point previous = previousTouchLocationInNode(touch);
    EditorLayer* layer = editorLayerOf(this);
    cg::Point center = editorWorldToNode(this, cg::Point(layer->rotCirc()->getPosition()));
    float current = angleTo((float)(location.x - center.x), (float)(location.y - center.y));
    float before = angleTo((float)(previous.x - center.x), (float)(previous.y - center.y));
    float delta = current - before;
    float degrees = delta * 57.29578f;
    Sprite* rotCirc = layer->rotCirc();
    rotCirc->setRotation(rotCirc->getRotation() - degrees);

    if (_selectedRefs.size() == 1)
    {
        Special* ref = _selectedRefs.at(0);
        float angle;
        if (!_snapToAngle)
        {
            float r = std::fmod(ref->getRotation() - degrees, 360.0f);
            angle = (0.0f <= r) ? r : r + 360.0f;
        }
        else
        {
            float radians = ref->angle() * 0.017453292f - delta;
            float snapped = (float)(int)(radians * 3.8197186f) * 0.2617994f;
            if (0.08726647f <= std::fabs(snapped - radians))
            {
                snapped = radians;
            }
            angle = snapped * 57.29578f;
        }
        ref->setAngle(Value(angle));
    }
    else if (!_rotateRefsIndependently)
    {
        Vector<Special*> selection = _selectedRefs;
        for (Special* ref : selection)
        {
            Vec2 pos = ref->getPosition();
            float dx = (float)((double)pos.x - center.x);
            float dy = (float)((double)pos.y - center.y);
            float distance = std::sqrt(dy * dy + dx * dx);
            float offsetX = 0.0f;
            float offsetY = 0.0f;
            if (distance != 0.0f)
            {
                float a = angleTo(dx, dy) + delta;
                offsetY = std::sin(a) * distance;
                offsetX = std::cos(a) * distance;
            }
            cg::Point desired(center.x + (double)offsetX, center.y + (double)offsetY);
            cg::Point limited = limitPosForRef(ref, desired);
            ref->setPosition((float)limited.x, (float)limited.y);
            float r = std::fmod(ref->getRotation() - degrees, 360.0f);
            float angle = (0.0f <= r) ? r : r + 360.0f;
            ref->setAngle(Value(angle));
        }
    }
    else
    {
        Vector<Special*> selection = _selectedRefs;
        for (Special* ref : selection)
        {
            ref->setAngle(Value(std::fmod(ref->angle() - degrees, 360.0f)));
        }
    }
    updateSelectionRect();
}

// @ios 1000bed30
void EditorSpriteBatchNode::modifyRef(Touch* touch, Event* event)
{
}

// ---- adding / removing refs ----------------------------------------------------------------------

// @ios 1000bed34
void EditorSpriteBatchNode::addRefs(const Vector<Special*>& refs)
{
    Vector<Special*> added = refs;
    _undoManager->beginUndoGrouping();
    _undoManager->prepareWithInvocationTarget(
        this, [added](EditorSpriteBatchNode* sbn) { sbn->undoAddRefs(added); });
    Vector<Special*> previousSelection = _selectedRefs;
    _undoManager->prepareWithInvocationTarget(
        this, [previousSelection](EditorSpriteBatchNode* sbn) { sbn->setSelectedRefs(previousSelection); });
    _undoManager->endUndoGrouping();
    _refs.pushBack(added);
    for (Special* ref : added)
    {
        addChild(ref);
    }
    setSelectedRefs(added);
}

// @ios 1000beebc
void EditorSpriteBatchNode::undoAddRefs(const Vector<Special*>& refs)
{
    Vector<Special*> removed = refs;
    for (Special* ref : removed)
    {
        _refs.eraseObject(ref, true);
    }
    for (Special* ref : removed)
    {
        removeChild(ref, false);
    }
}

// @ios 1000befc4
void EditorSpriteBatchNode::addRef(Special* ref)
{
    if (_refs.empty())
    {
        _characterRef = static_cast<CharacterRef*>(ref);
    }
    else
    {
        _undoManager->prepareWithInvocationTarget(
            this, [ref](EditorSpriteBatchNode* sbn) { sbn->undoAddRef(ref); });
        uikit::NotificationCenter::postNotification(kUndoStackUpdated, _undoManager);
    }
    _refs.pushBack(ref);
    addChild(ref);
}

// @ios 1000bf05c
void EditorSpriteBatchNode::undoAddRef(Special* ref)
{
    RefPtr<Special> keep(ref);
    _refs.eraseObject(ref, true);
    removeChild(ref, false);
}

// @ios 1000bf098
void EditorSpriteBatchNode::undoDeletionOfRefs(const Vector<Special*>& refs)
{
    Vector<Special*> restored = refs;
    for (Special* ref : restored)
    {
        addRef(ref);
    }
    setSelectedRefs(restored);
}

// @ios 1000bf1a8
void EditorSpriteBatchNode::deleteSelectedRefs()
{
    Vector<Special*> deleted(_selectedRefs.size());
    Vector<Special*> selection = _selectedRefs;
    for (Special* ref : selection)
    {
        if (ref != _characterRef)
        {
            deleted.pushBack(ref);
            removeChild(ref, false);
            _refs.eraseObject(ref, true);
        }
    }
    _undoManager->prepareWithInvocationTarget(
        this, [deleted](EditorSpriteBatchNode* sbn) { sbn->undoDeletionOfRefs(deleted); });
    uikit::NotificationCenter::postNotification(kUndoStackUpdated, _undoManager);
    _selectedRefs.clear();
    updateSelectionRect();
    hideCircControls();
}

// ---- selection ----------------------------------------------------------------------------------

// @ios 1000bf388
void EditorSpriteBatchNode::setSelectedRefs(const Vector<Special*>& refs)
{
    Vector<Special*> selection = refs;  // `refs` may be _selectedRefs itself
    _selectedRefs.clear();
    if (selection.empty())
    {
        updateSelectionRect();
        hideCircControls();
    }
    else
    {
        _selectedRefs.pushBack(selection);
        updateSelectionRect();
        showCircControls();
    }
    uikit::NotificationCenter::postNotification(kSelRefChange, &_selectedRefs);
}

// @ios 1000bf414
void EditorSpriteBatchNode::setSelectedRef(Special* ref)
{
    if (rotateDot)
    {
        rotateDot->removeFromParentAndCleanup(false);
        rotateDot = nullptr;
    }
    if (modifyDot)
    {
        modifyDot->removeFromParentAndCleanup(false);
        modifyDot = nullptr;
    }
    _selectedRef = ref;
    if (ref->canRotate())
    {
        rotateDot = Sprite::createWithSpriteFrameName("e_rotate.png");
        addChild(rotateDot, 5000);
        rotateDot->setScale(1.0f / getParent()->getScale());
        rotateDot->setRotation(_selectedRef->getRotation());
    }
    if (_selectedRef->canDragModify())
    {
        modifyDot = Sprite::createWithSpriteFrameName("e_modify.png");
        addChild(modifyDot, 5000);
        modifyDot->setScale(1.0f / getParent()->getScale());
        modifyDot->setRotation(_selectedRef->getRotation());
    }
    positionDots();
    uikit::NotificationCenter::postNotification(kSelRefChange, _selectedRef);
}

// ---- drawing / controls ---------------------------------------------------------------------------

// @ios 1000bf564
DrawNode* EditorSpriteBatchNode::drawNode()
{
    if (_drawNode)
    {
        return _drawNode;
    }
    _drawNode = DrawNode::create();
    getParent()->addChild(_drawNode, 99);
    return _drawNode;
}

// @ios 1000bf5cc
void EditorSpriteBatchNode::showCircControls()
{
    EditorLayer* layer = editorLayerOf(this);
    Sprite* rotCirc = layer->rotCirc();
    Sprite* moveCirc = layer->moveCirc();
    rotCirc->setVisible(false);
    moveCirc->setVisible(true);
    positionMoveRotCircs();
    bool allRotate = true;
    for (Special* ref : _selectedRefs)
    {
        if (!ref->canRotate())
        {
            allRotate = false;
            break;
        }
    }
    if (allRotate)
    {
        rotCirc->setVisible(true);
    }
    float rotation = 0.0f;
    if (_selectedRefs.size() == 1)
    {
        rotation = _selectedRefs.at(0)->getRotation();
    }
    rotCirc->setRotation(rotation);
}

// @ios 1000bf73c
void EditorSpriteBatchNode::positionMoveRotCircs()
{
    EditorLayer* layer = editorLayerOf(this);
    if (!layer)
    {
        return;
    }
    cg::Point center(_selectionRect.origin.x + _selectionRect.size.width * 0.5,
                     _selectionRect.origin.y + _selectionRect.size.height * 0.5);
    Vec2 world = nodeToEditorWorld(this, center).toVec2();
    layer->rotCirc()->setPosition(world);
    layer->moveCirc()->setPosition(world);
}

// @ios 1000bf7c8
void EditorSpriteBatchNode::hideCircControls()
{
    EditorLayer* layer = editorLayerOf(this);
    if (!layer)
    {
        return;
    }
    layer->moveCirc()->setVisible(false);
    layer->rotCirc()->setVisible(false);
}

// @ios 1000bf80c
void EditorSpriteBatchNode::positionDots()
{
    if (_selectedRefs.empty())
    {
        return;  // iOS objectAtIndex:0 would throw
    }
    Special* ref = _selectedRefs.at(0);
    cg::Rect rect(ref->refRect());
    if (rotateDot)
    {
        Vec2 world = ref->convertToWorldSpace(
            Vec2((float)(rect.origin.x + rect.size.width + 10.0), (float)(rect.size.height + 10.0 + rect.origin.y)));
        rotateDot->setPosition(convertToNodeSpace(world));
    }
    if (modifyDot)
    {
        Vec2 world = ref->convertToWorldSpace(
            Vec2((float)(rect.origin.x + rect.size.width + 10.0), (float)(rect.origin.y + -10.0)));
        modifyDot->setPosition(convertToNodeSpace(world));
    }
}

// @ios 1000bf8f4
void EditorSpriteBatchNode::update(float dt)
{
    drawNode()->clear();
    Vector<Special*> refs = _refs;
    for (Special* ref : refs)
    {
        // respondsToSelector:@selector(updateDrawingWithNode:) - Special's default is a no-op.
        ref->updateDrawingWithNode(drawNode());
    }
}

// @ios 1000bfa48
void EditorSpriteBatchNode::draw(Renderer* renderer, const Mat4& transform, uint32_t flags)
{
    _overlayCommand.init(_globalZOrder, transform, flags);
    _overlayCommand.func = std::bind(&EditorSpriteBatchNode::onDrawOverlay, this, transform, flags);
    renderer->addCommand(&_overlayCommand);
    SpriteBatchNode::draw(renderer, transform, flags);
}

void EditorSpriteBatchNode::onDrawOverlay(const Mat4& transform, uint32_t flags)
{
    Director* director = Director::getInstance();
    director->pushMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
    director->loadMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW, transform);

    glLineWidth(1.0f);
    if (marqueeStart.x != 0.0 && marqueeStart.y != 0.0 && marqueeEnd.x != 0.0 && marqueeEnd.y != 0.0)
    {
        DrawPrimitives::setDrawColor4F(0.0f, 0.0f, 0.0f, 0.25f);
        DrawPrimitives::drawRect(marqueeStart.toVec2(), marqueeEnd.toVec2());
        DrawPrimitives::drawSolidRect(marqueeStart.toVec2(), marqueeEnd.toVec2(), Color4F(0.0f, 0.0f, 0.0f, 0.1f));
    }
    if (_selectionRect.size.width != 0.0 && _selectionRect.size.height != 0.0)
    {
        DrawPrimitives::setDrawColor4F(0.0f, 0.0f, 0.0f, 0.25f);
        const cg::Rect& r = _selectionRect;
        DrawPrimitives::drawRect(
            Vec2((float)(int)(r.origin.x + -0.5), (float)(int)(r.origin.y + -0.5)),
            Vec2((float)(int)(r.origin.x + 0.5 + r.size.width), (float)(int)(r.origin.y + 0.5 + r.size.height)));
    }
    for (Special* ref : highlightedRefs)
    {
        cg::Rect r(ref->refBoundingBox());
        DrawPrimitives::setDrawColor4F(0.0f, 0.0f, 0.0f, 0.25f);
        DrawPrimitives::drawRect(
            Vec2((float)(int)(r.origin.x + -0.5), (float)(int)(r.origin.y + -0.5)),
            Vec2((float)(int)(r.origin.x + 0.5 + r.size.width), (float)(int)(r.origin.y + 0.5 + r.size.height)));
    }
    glLineWidth(2.0f);
    DrawPrimitives::setDrawColor4F(0.17254902f, 0.17254902f, 0.17254902f, 1.0f);
    for (Special* ref : _refs)
    {
        WreckingBallRef* wreckingBall = dynamic_cast<WreckingBallRef*>(ref);
        if (wreckingBall)
        {
            Vec2 pos = ref->getPosition();
            DrawPrimitives::drawLine(pos, Vec2(pos.x, (float)((double)pos.y - (double)wreckingBall->scaledRopeLength())));
        }
    }
    DrawPrimitives::setDrawColor4F(0.0f, 0.0f, 0.0f, 1.0f);
    glLineWidth(1.0f);  // port: restore GL state for the rest of the frame

    director->popMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
}

// @ios 1000bfe4c
void EditorSpriteBatchNode::setEnableEdit(bool enableEdit)
{
    EditorLayer* layer = editorLayerOf(this);
    if (!enableEdit)
    {
        layer->moveCirc()->setVisible(false);
        layer->rotCirc()->setVisible(false);
    }
    else if (!_selectedRefs.empty())
    {
        showCircControls();
    }
    _enableEdit = enableEdit;
}

// @ios 1000bfed4
void EditorSpriteBatchNode::reset()
{
    setSelectedRefs(_refs);
    deleteSelectedRefs();
    Special* character = _refs.at(0);
    character->setXMeters(Value(75));
    character->setYMeters(Value(50));
    updateSelectionRect();
    positionMoveRotCircs();
    _undoManager->removeAllActions();
}

// @ios 1000bff78
void EditorSpriteBatchNode::undo()
{
    _undoManager->undo();
    uikit::NotificationCenter::postNotification(kUndoStackUpdated, _undoManager);
    updateSelectionRect();
    positionMoveRotCircs();
}

// @ios 1000bffcc
void EditorSpriteBatchNode::redo()
{
    _undoManager->redo();
    uikit::NotificationCenter::postNotification(kUndoStackUpdated, _undoManager);
    updateSelectionRect();
    positionMoveRotCircs();
}

// @ios 1000c0020
void EditorSpriteBatchNode::undoManagerDidUndo(void* notification)
{
    updateSelectionRect();
}

// @ios 1000c0024
void EditorSpriteBatchNode::undoManagerDidRedo(void* notification)
{
    updateSelectionRect();
}

// ---- properties ---------------------------------------------------------------------------------

// @ios 1000c0028
Special* EditorSpriteBatchNode::selectedRef()
{
    return _selectedRef;
}

// @ios 1000c0038
bool EditorSpriteBatchNode::enableEdit()
{
    return _enableEdit;
}

// @ios 1000c0048
const Vector<Special*>& EditorSpriteBatchNode::refs()
{
    return _refs;
}

// @ios 1000c0058
const Vector<Special*>& EditorSpriteBatchNode::selectedRefs()
{
    return _selectedRefs;
}

// @ios 1000c0068
bool EditorSpriteBatchNode::snapToAngle()
{
    return _snapToAngle;
}

// @ios 1000c0078
void EditorSpriteBatchNode::setSnapToAngle(bool snapToAngle)
{
    _snapToAngle = snapToAngle;
}

// @ios 1000c0088
bool EditorSpriteBatchNode::rotateRefsIndependently()
{
    return _rotateRefsIndependently;
}

// @ios 1000c0098
void EditorSpriteBatchNode::setRotateRefsIndependently(bool rotateRefsIndependently)
{
    _rotateRefsIndependently = rotateRefsIndependently;
}

// @ios 1000c00a8
bool EditorSpriteBatchNode::lockToAxis()
{
    return _lockToAxis;
}

// @ios 1000c00b8
void EditorSpriteBatchNode::setLockToAxis(bool lockToAxis)
{
    _lockToAxis = lockToAxis;
}

// @ios 1000c00c8
void EditorSpriteBatchNode::setDrawNode(DrawNode* drawNode)
{
    _drawNode = drawNode;
}

// @ios 1000c00d8
EditorUndoManager* EditorSpriteBatchNode::undoManager()
{
    return _undoManager;
}
