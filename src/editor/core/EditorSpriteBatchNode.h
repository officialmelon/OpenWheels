#pragma once
// iOS EditorSpriteBatchNode : CCSpriteBatchNode (instanceSize 0x220) - the editor stage's item
// layer: owns every ref (Special) of the level (`refs`, index 0 = the CharacterRef), the
// selection, the marquee, drag / rotate handling with the move/rotate circles, edge panning,
// add / delete with undo, and the per-frame drawing of shape outlines (CCDrawNode) and overlays.
//
// Scene graph (built by EditorLayer): EditorLayer -> stage (CCLayerPanZoom) -> this (z 100)
//                                                                       \-> drawNode (z 99)
// so parent() is the stage and parent()->parent() the EditorLayer (rotCirc / moveCirc live there).
// Batch texture: "levelEditorObjects1<suffix>.png"; every ref sprite is a child of this node.
//
// Stage space: iPad points with the iOS iPad editor ptmRatio (Session mode 2 on iPad: 72), so the
// stage is 320 m x 160 m = 23040 x 11520 points (stageWidth / stageHeight). See EditorLayer.h.
//
// Touch handling: iOS targeted delegate, priority 0, swallows (registered in onEnter) ->
// EventListenerTouchOneByOne with fixed priority kTouchPriority; ccTouchBegan returns false while
// editing is disabled or edge panning.
//
// Interaction states (`state` ivar, iOS values): 0 idle, 1 dragging refs (touch began on a ref or
// the move circle), 2 rotating (rotate circle), 4 modify (unused: modifyRef is empty),
// 5 marquee selection.
//
// Undo (EditorUndoManager, 40 levels): selection changes, drags, rotations, adds and deletions
// register their inverse; after each registration "undo_stack_updated" is posted with the undo
// manager as object (EditorLayer::undoStackUpdated refreshes the undo/redo buttons).
//
// Notifications posted (uikit::NotificationCenter): "sel_ref_change" (setSelectedRefs: object =
// &_selectedRefs; setSelectedRef: object = the ref), "undo_stack_updated".
// Observed: EditorUndoManager did-undo / did-redo (-> updateSelectionRect), "sel_rect_changed"
// (EditParametersView, -> updateSelectionRect).

#include "2d/CCSpriteBatchNode.h"
#include "base/CCVector.h"
#include "renderer/CCCustomCommand.h"
#include "EditorGeometry.h"

namespace cocos2d {
class DrawNode;
class EventListenerTouchOneByOne;
class Touch;
class Event;
}

class CharacterRef;
class EditorUndoManager;
class Special;

// -[EditorSpriteBatchNode state] values.
enum EditorSpriteBatchNodeState
{
    EditorSBNStateIdle = 0,
    EditorSBNStateDrag = 1,
    EditorSBNStateRotate = 2,
    EditorSBNStateModify = 4,
    EditorSBNStateMarquee = 5,
};

class EditorSpriteBatchNode : public cocos2d::SpriteBatchNode
{
public:
    static const int kTouchPriority;

    // +[CCSpriteBatchNode batchNodeWithFile:] on this class (-> initWithTexture:capacity:).
    static EditorSpriteBatchNode* create(const std::string& fileImage,
                                         ssize_t capacity = 29);  // SpriteBatchNode::DEFAULT_CAPACITY (private)

    // Base init, then: selectionRect zero, stageWidth/Height = ptm * 320 / 160, enableEdit,
    // empty arrays, undo manager with levelsOfUndo 40, circNormalOpacity 63,
    // circPressedOpacity 153. (Hides SpriteBatchNode::initWithTexture, which is not virtual.)
    bool initWithTexture(cocos2d::Texture2D* tex, ssize_t capacity);       // @ios 1000bce60
    // dealloc                                                             // @ios 1000bcfd8
    void onEnter() override;                                               // @ios 1000bd058
    void onEnterTransitionDidFinish() override;                            // @ios 1000bd13c  scheduleUpdate
    void onExit() override;                                                // @ios 1000bd140

    // ---- touches ---------------------------------------------------------------------------
    bool ccTouchBegan(cocos2d::Touch* touch, cocos2d::Event* event);       // @ios 1000bd1b4
    void ccTouchMoved(cocos2d::Touch* touch, cocos2d::Event* event);       // @ios 1000bd8cc
    void ccTouchCancelled(cocos2d::Touch* touch, cocos2d::Event* event);   // @ios 1000bd910  -> ccTouchEnded
    void ccTouchEnded(cocos2d::Touch* touch, cocos2d::Event* event);       // @ios 1000bd914
    void updateMarquee(cocos2d::Touch* touch, cocos2d::Event* event);      // @ios 1000bda2c  updateMarquee:withEvent:
    void selectRefsInMarquee();                                            // @ios 1000bdc84
    void updateSelectionRect();                                            // @ios 1000bdd98
    void dragRef(cocos2d::Touch* touch, cocos2d::Event* event);            // @ios 1000bded8  dragRef:withEvent:
    void moveRefs(const cocos2d::Vector<Special*>& refs, const cg::Point& movement);  // @ios 1000be25c  moveRefs:movement:
    cg::Point limitPosForRef(Special* ref, const cg::Point& desiredPos);   // @ios 1000be510  limitPosForRef:desiredPos:
    void handleEdgePan(float dt);                                          // @ios 1000be5f0  (scheduled selector)
    void rotateRefs(cocos2d::Touch* touch, cocos2d::Event* event);         // @ios 1000be850  rotateRefs:withEvent:
    void modifyRef(cocos2d::Touch* touch, cocos2d::Event* event);          // @ios 1000bed30  (empty)

    // ---- adding / removing refs (with undo) -------------------------------------------------
    // Undo group {undoAddRefs:(refs), setSelectedRefs:(current selection)}, then add as children
    // and select them.
    void addRefs(const cocos2d::Vector<Special*>& refs);                   // @ios 1000bed34
    void undoAddRefs(const cocos2d::Vector<Special*>& refs);               // @ios 1000beebc
    // First ref ever added becomes characterRef (no undo); later ones register undoAddRef:.
    void addRef(Special* ref);                                             // @ios 1000befc4
    void undoAddRef(Special* ref);                                         // @ios 1000bf05c
    void undoDeletionOfRefs(const cocos2d::Vector<Special*>& refs);        // @ios 1000bf098
    // Removes the selection except the character, registers undoDeletionOfRefs:.
    void deleteSelectedRefs();                                             // @ios 1000bf1a8

    // ---- selection --------------------------------------------------------------------------
    // Replaces the selection (copying `refs`), updates the selection rect and the circle controls,
    // posts "sel_ref_change".
    void setSelectedRefs(const cocos2d::Vector<Special*>& refs);           // @ios 1000bf388
    void setSelectedRef(Special* ref);                                     // @ios 1000bf414  (legacy single-ref dots)

    // ---- drawing / controls -----------------------------------------------------------------
    cocos2d::DrawNode* drawNode();                                         // @ios 1000bf564  (created on demand, parent z 99)
    void showCircControls();                                               // @ios 1000bf5cc
    void positionMoveRotCircs();                                           // @ios 1000bf73c
    void hideCircControls();                                               // @ios 1000bf7c8
    void positionDots();                                                   // @ios 1000bf80c
    // drawNode clear + updateDrawingWithNode: on every ref that implements it.
    void update(float dt) override;                                        // @ios 1000bf8f4
    // Marquee (black 0.25 outline + 0.1 fill), selection rect, highlighted refs' boxes, wrecking
    // ball ropes (2 px, grey 0.345), then the batch. Immediate-mode GL in iOS -> a CustomCommand
    // with DrawPrimitives queued before SpriteBatchNode::draw.
    void draw(cocos2d::Renderer* renderer, const cocos2d::Mat4& transform,
              uint32_t flags) override;                                    // @ios 1000bfa48
    void setEnableEdit(bool enableEdit);                                   // @ios 1000bfe4c
    // Select all + delete (character stays), character back to (75 m, 50 m), clear undo.
    void reset();                                                          // @ios 1000bfed4
    void undo();                                                           // @ios 1000bff78
    void redo();                                                           // @ios 1000bffcc
    void undoManagerDidUndo(void* notification);                           // @ios 1000c0020
    void undoManagerDidRedo(void* notification);                           // @ios 1000c0024

    // ---- properties -------------------------------------------------------------------------
    Special* selectedRef();                                                // @ios 1000c0028
    bool enableEdit();                                                     // @ios 1000c0038
    const cocos2d::Vector<Special*>& refs();                               // @ios 1000c0048
    const cocos2d::Vector<Special*>& selectedRefs();                       // @ios 1000c0058
    bool snapToAngle();                                                    // @ios 1000c0068
    void setSnapToAngle(bool snapToAngle);                                 // @ios 1000c0078
    bool rotateRefsIndependently();                                        // @ios 1000c0088
    void setRotateRefsIndependently(bool rotateRefsIndependently);         // @ios 1000c0098
    bool lockToAxis();                                                     // @ios 1000c00a8
    void setLockToAxis(bool lockToAxis);                                   // @ios 1000c00b8
    void setDrawNode(cocos2d::DrawNode* drawNode);                         // @ios 1000c00c8
    EditorUndoManager* undoManager();                                      // @ios 1000c00d8

    // port: characterRef accessor (iOS reads refs[0]).
    CharacterRef* characterRef() { return _characterRef; }
    cg::Rect selectionRect() { return _selectionRect; }

protected:
    EditorSpriteBatchNode();
    ~EditorSpriteBatchNode() override;

    void onDrawOverlay(const cocos2d::Mat4& transform, uint32_t flags);   // port: draw's GL part
    // port: touch location -> this node's space (iOS convertToGL + convertToNodeSpace:).
    cg::Point touchLocationInNode(cocos2d::Touch* touch);
    cg::Point previousTouchLocationInNode(cocos2d::Touch* touch);

    // iOS ivars (offsets from the ObjC metadata).
    float stageWidth = 0.0f;                          // +0x158
    float stageHeight = 0.0f;                         // +0x15c
    cocos2d::Vector<Special*> _selectedRefs;          // +0x160  iOS "selectedRefs"
    cocos2d::Vector<Special*> highlightedRefs;        // +0x168  (marquee candidates)
    CharacterRef* _characterRef = nullptr;            // +0x170  iOS "characterRef" (not retained)
    cocos2d::Vector<Special*> _refs;                  // +0x178  iOS "refs"
    cocos2d::Touch* draggingTouch = nullptr;          // +0x180  (not retained)
    cocos2d::Sprite* rotateDot = nullptr;             // +0x188  "e_rotate.png"
    cocos2d::Sprite* modifyDot = nullptr;             // +0x190  "e_modify.png"
    Special* _selectedRef = nullptr;                  // +0x198  iOS "selectedRef"
    EditorSpriteBatchNodeState state = EditorSBNStateIdle;  // +0x1a0
    EditorUndoManager* _undoManager = nullptr;        // +0x1a8  iOS "undoManager" (retained)
    float rotateStartAngle = 0.0f;                    // +0x1b0
    bool _enableEdit = true;                          // +0x1b4  iOS "enableEdit"
    bool edgePanning = false;                         // +0x1b5
    bool _snapToAngle = false;                        // +0x1b6  iOS "snapToAngle"
    bool _rotateRefsIndependently = false;            // +0x1b7  iOS "rotateRefsIndependently"
    bool _lockToAxis = false;                         // +0x1b8  iOS "lockToAxis"
    cg::Point dragStartPos;                           // +0x1c0
    cg::Point marqueeStart;                           // +0x1d0
    cg::Point marqueeEnd;                             // +0x1e0
    cg::Rect _selectionRect;                          // +0x1f0  iOS "selectionRect"
    float circPressedOpacity = 0.0f;                  // +0x210
    float circNormalOpacity = 0.0f;                   // +0x214
    cocos2d::DrawNode* _drawNode = nullptr;           // +0x218  (not retained; child of the stage)

    // port
    cocos2d::EventListenerTouchOneByOne* _touchListener = nullptr;
    cocos2d::CustomCommand _overlayCommand;
};
