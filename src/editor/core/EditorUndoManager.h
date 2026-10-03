#pragma once
// C++ replacement for the NSUndoManager the iOS editor uses (EDITOR_PORT.md rule 3).
// Not an iOS class: it reproduces the Foundation behaviour the editor code depends on.
//
// Who uses it (iOS 1.2.7):
//   * EditorSpriteBatchNode owns one (`undoManager` ivar, `setLevelsOfUndo:40`) and registers
//     its own inverse operations: setSelectedRefs:, undoAddRefs:, undoAddRef:,
//     undoDeletionOfRefs: (target = the batch node), setX:y: / setRotation: (target = a ref).
//   * EditParametersView gets it via setUndoManager: and registers setValue:forKey: on refs
//     (E4), wrapped in begin/endUndoGrouping.
//   * EditorLayer reads canUndo/canRedo (button state, unsavedChanges), calls removeAllActions
//     after loading a level, and undo/redo through the batch node.
//
// Semantics reproduced (Foundation NSUndoManager; GNUstep's implementation is the reference for
// the details Apple does not document):
//   * Registration: registerUndo(target, action) == [[um prepareWithInvocationTarget:target] ...].
//     The target is retained by the action until the action is dropped. While neither undoing nor
//     redoing, a registration clears the redo stack.
//   * Grouping: begin/endUndoGrouping nest; a nested group's actions are appended to its parent
//     (flattened, order kept). Closing a top-level group pushes it onto the undo stack (or the
//     redo stack while undoing) unless it is empty. Stacks are capped at levelsOfUndo groups
//     (0 = unlimited); the oldest group is dropped.
//   * groupsByEvent (default true): a registration or beginUndoGrouping with no group open first
//     opens an implicit top-level "event" group, closed at the end of the current event:
//     endEventGroup() — called from a Scheduler callback scheduled when the group opens (next
//     frame, i.e. after the current touch/key handlers), and also by EditorLayer before each new
//     touch event is dispatched, so one touch phase == one run-loop pass, as on iOS.
//   * canUndo: undo stack not empty, or the open top-level group already has actions (so the
//     "undo_stack_updated" handlers that run right after a registration see YES, as on iOS).
//   * undo(): closes the implicit event group if it is the only open group (groupingLevel 1),
//     then undoNestedGroup(): pops the newest group, runs its actions newest-first with
//     isUndoing() true; their registrations form one group pushed onto the redo stack; posts
//     kDidUndoChangeNotification. redo() mirrors it (registrations go to the undo stack without
//     clearing the redo stack; posts kDidRedoChangeNotification). Calling undo/redo with a nested
//     explicit group open is a programming error (Foundation raises) -> CCASSERT, then ignored.
//   * removeAllActions(): clears both stacks and the actions of any open group (grouping level is
//     kept so pending endUndoGrouping calls stay balanced).
//     RE-TODO(Foundation): Apple's handling of an open group here is undocumented; this choice keeps
//     -[EditorLayer addLevelItems] (addRef: registrations, then removeAllActions) from leaving an
//     undoable "load" step behind.
//
// Notifications go through uikit::NotificationCenter (Director EventDispatcher, EventCustom) with
// object = this manager, like NSUndoManagerDidUndoChangeNotification / ...DidRedo....

#include <functional>
#include <vector>

#include "base/CCRef.h"
#include "base/CCRefPtr.h"

class EditorUndoManager : public cocos2d::Ref
{
public:
    // Same strings as Foundation's notification names.
    static const char* const kWillUndoChangeNotification;  // "NSUndoManagerWillUndoChangeNotification"
    static const char* const kDidUndoChangeNotification;   // "NSUndoManagerDidUndoChangeNotification"
    static const char* const kWillRedoChangeNotification;  // "NSUndoManagerWillRedoChangeNotification"
    static const char* const kDidRedoChangeNotification;   // "NSUndoManagerDidRedoChangeNotification"

    using Action = std::function<void()>;

    static EditorUndoManager* create();  // [[NSUndoManager alloc] init], autoreleased

    // ---- registration ----------------------------------------------------------------------
    // [[um prepareWithInvocationTarget:target] <message>]: `action` replays the message. `target`
    // (may be nullptr) is retained for as long as the action is stored. Values the message needs
    // must be captured by value (cocos2d::Vector copies retain their refs, like NSArray).
    void registerUndo(cocos2d::Ref* target, Action action);
    // Typed convenience: prepareWithInvocationTarget(ref, [](Special* s) { s->setX(x, y); }).
    template <class T, class F>
    void prepareWithInvocationTarget(T* target, F&& message)
    {
        cocos2d::RefPtr<T> keep(target);
        registerUndo(target, [keep, message]() { message(keep.get()); });
    }

    // ---- grouping --------------------------------------------------------------------------
    void beginUndoGrouping();
    void endUndoGrouping();
    int groupingLevel() const;
    bool groupsByEvent() const;
    void setGroupsByEvent(bool groupsByEvent);
    // Closes the implicit event group if it is open and is the outermost group (no-op otherwise).
    void endEventGroup();

    // ---- undo / redo -----------------------------------------------------------------------
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    void undoNestedGroup();
    void redoNestedGroup();  // (Foundation has no public redoNestedGroup; used by redo())
    bool isUndoing() const;
    bool isRedoing() const;

    // ---- housekeeping ----------------------------------------------------------------------
    unsigned int levelsOfUndo() const;
    void setLevelsOfUndo(unsigned int levels);  // trims both stacks immediately
    void removeAllActions();
    void removeAllActionsWithTarget(cocos2d::Ref* target);
    void disableUndoRegistration();
    void enableUndoRegistration();
    bool isUndoRegistrationEnabled() const;

protected:
    EditorUndoManager();
    ~EditorUndoManager() override;

    struct Entry
    {
        cocos2d::RefPtr<cocos2d::Ref> target;
        Action action;
    };
    using Group = std::vector<Entry>;

    void openGroup(bool implicitEventGroup);
    void pushGroup(std::vector<Group>& stack, Group&& group);
    void runGroup(Group& group);
    void scheduleEventGroupEnd();
    void unscheduleEventGroupEnd();

    std::vector<Group> _undoStack;      // oldest first
    std::vector<Group> _redoStack;      // oldest first
    std::vector<Group> _openGroups;     // innermost last
    bool _eventGroupOpen = false;       // _openGroups[0] is the implicit event group
    bool _eventGroupEndScheduled = false;
    bool _groupsByEvent = true;
    bool _isUndoing = false;
    bool _isRedoing = false;
    int _disableCount = 0;
    unsigned int _levelsOfUndo = 0;
};
