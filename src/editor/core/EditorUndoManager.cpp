#include "EditorUndoManager.h"

#include <algorithm>

#include "cocos2d.h"
#include "UIKitCompat.h"

USING_NS_CC;

const char* const EditorUndoManager::kWillUndoChangeNotification = "NSUndoManagerWillUndoChangeNotification";
const char* const EditorUndoManager::kDidUndoChangeNotification = "NSUndoManagerDidUndoChangeNotification";
const char* const EditorUndoManager::kWillRedoChangeNotification = "NSUndoManagerWillRedoChangeNotification";
const char* const EditorUndoManager::kDidRedoChangeNotification = "NSUndoManagerDidRedoChangeNotification";

namespace {
const char* const kEventGroupKey = "EditorUndoManager.endEventGroup";
}

EditorUndoManager* EditorUndoManager::create()
{
    EditorUndoManager* manager = new (std::nothrow) EditorUndoManager();
    if (manager)
    {
        manager->autorelease();
    }
    return manager;
}

EditorUndoManager::EditorUndoManager()
{
}

EditorUndoManager::~EditorUndoManager()
{
    unscheduleEventGroupEnd();
}

// ---- registration ---------------------------------------------------------------------------

void EditorUndoManager::registerUndo(Ref* target, Action action)
{
    if (_disableCount > 0)
    {
        return;
    }
    if (_openGroups.empty())
    {
        if (!_groupsByEvent)
        {
            // Foundation raises NSInternalInconsistencyException here.
            CCLOG("EditorUndoManager: registerUndo with no open group and groupsByEvent off");
            return;
        }
        openGroup(true);
    }
    Entry entry;
    entry.target = target;
    entry.action = std::move(action);
    _openGroups.back().push_back(std::move(entry));
    if (!_isUndoing && !_isRedoing)
    {
        _redoStack.clear();
    }
}

// ---- grouping -------------------------------------------------------------------------------

void EditorUndoManager::openGroup(bool implicitEventGroup)
{
    _openGroups.emplace_back();
    if (implicitEventGroup)
    {
        _eventGroupOpen = true;
        scheduleEventGroupEnd();
    }
}

void EditorUndoManager::beginUndoGrouping()
{
    // With groupsByEvent an explicit group opened outside any group nests inside the run-loop
    // group (it is closed at the end of the event).
    if (_openGroups.empty() && _groupsByEvent && !_isUndoing && !_isRedoing)
    {
        openGroup(true);
    }
    openGroup(false);
}

void EditorUndoManager::endUndoGrouping()
{
    if (_openGroups.empty())
    {
        CCLOG("EditorUndoManager: endUndoGrouping without a matching begin");
        return;
    }
    Group group = std::move(_openGroups.back());
    _openGroups.pop_back();
    if (!_openGroups.empty())
    {
        Group& parent = _openGroups.back();
        for (auto& entry : group)
        {
            parent.push_back(std::move(entry));
        }
        return;
    }
    if (_eventGroupOpen)
    {
        _eventGroupOpen = false;
        unscheduleEventGroupEnd();
    }
    if (group.empty())
    {
        return;
    }
    if (_isUndoing)
    {
        pushGroup(_redoStack, std::move(group));
    }
    else
    {
        pushGroup(_undoStack, std::move(group));
    }
}

int EditorUndoManager::groupingLevel() const
{
    return (int)_openGroups.size();
}

bool EditorUndoManager::groupsByEvent() const
{
    return _groupsByEvent;
}

void EditorUndoManager::setGroupsByEvent(bool groupsByEvent)
{
    _groupsByEvent = groupsByEvent;
}

void EditorUndoManager::endEventGroup()
{
    if (_eventGroupOpen && _openGroups.size() == 1)
    {
        endUndoGrouping();
    }
}

void EditorUndoManager::scheduleEventGroupEnd()
{
    if (_eventGroupEndScheduled)
    {
        return;
    }
    _eventGroupEndScheduled = true;
    // Next scheduler tick == after the handlers of the current touch/key event have run.
    Director::getInstance()->getScheduler()->schedule(
        [this](float) {
            _eventGroupEndScheduled = false;
            endEventGroup();
        },
        this, 0.0f, 0, 0.0f, false, kEventGroupKey);
}

void EditorUndoManager::unscheduleEventGroupEnd()
{
    if (!_eventGroupEndScheduled)
    {
        return;
    }
    _eventGroupEndScheduled = false;
    Director::getInstance()->getScheduler()->unschedule(kEventGroupKey, this);
}

void EditorUndoManager::pushGroup(std::vector<Group>& stack, Group&& group)
{
    stack.push_back(std::move(group));
    if (_levelsOfUndo > 0)
    {
        while (stack.size() > _levelsOfUndo)
        {
            stack.erase(stack.begin());
        }
    }
}

void EditorUndoManager::runGroup(Group& group)
{
    // Actions run newest first. Each action keeps its target alive while it runs.
    for (auto it = group.rbegin(); it != group.rend(); ++it)
    {
        RefPtr<Ref> keep = it->target;
        if (it->action)
        {
            it->action();
        }
    }
}

// ---- undo / redo ----------------------------------------------------------------------------

bool EditorUndoManager::canUndo() const
{
    if (!_undoStack.empty())
    {
        return true;
    }
    for (const auto& group : _openGroups)
    {
        if (!group.empty())
        {
            return true;
        }
    }
    return false;
}

bool EditorUndoManager::canRedo() const
{
    return !_redoStack.empty();
}

void EditorUndoManager::undo()
{
    if (_eventGroupOpen && _openGroups.size() == 1)
    {
        endUndoGrouping();
    }
    if (!_openGroups.empty())
    {
        CCASSERT(false, "EditorUndoManager::undo with an explicit undo group open");
        return;
    }
    undoNestedGroup();
}

void EditorUndoManager::redo()
{
    if (_eventGroupOpen && _openGroups.size() == 1)
    {
        endUndoGrouping();
    }
    if (!_openGroups.empty())
    {
        CCASSERT(false, "EditorUndoManager::redo with an explicit undo group open");
        return;
    }
    redoNestedGroup();
}

void EditorUndoManager::undoNestedGroup()
{
    if (_undoStack.empty())
    {
        return;
    }
    RefPtr<EditorUndoManager> keepSelf(this);
    uikit::NotificationCenter::postNotification(kWillUndoChangeNotification, this);
    Group group = std::move(_undoStack.back());
    _undoStack.pop_back();
    _isUndoing = true;
    openGroup(false);
    runGroup(group);
    endUndoGrouping();  // -> redo stack
    _isUndoing = false;
    uikit::NotificationCenter::postNotification(kDidUndoChangeNotification, this);
}

void EditorUndoManager::redoNestedGroup()
{
    if (_redoStack.empty())
    {
        return;
    }
    RefPtr<EditorUndoManager> keepSelf(this);
    uikit::NotificationCenter::postNotification(kWillRedoChangeNotification, this);
    Group group = std::move(_redoStack.back());
    _redoStack.pop_back();
    _isRedoing = true;
    openGroup(false);
    runGroup(group);
    endUndoGrouping();  // -> undo stack (the redo stack is kept)
    _isRedoing = false;
    uikit::NotificationCenter::postNotification(kDidRedoChangeNotification, this);
}

bool EditorUndoManager::isUndoing() const
{
    return _isUndoing;
}

bool EditorUndoManager::isRedoing() const
{
    return _isRedoing;
}

// ---- housekeeping ---------------------------------------------------------------------------

unsigned int EditorUndoManager::levelsOfUndo() const
{
    return _levelsOfUndo;
}

void EditorUndoManager::setLevelsOfUndo(unsigned int levels)
{
    _levelsOfUndo = levels;
    if (levels > 0)
    {
        while (_undoStack.size() > levels)
        {
            _undoStack.erase(_undoStack.begin());
        }
        while (_redoStack.size() > levels)
        {
            _redoStack.erase(_redoStack.begin());
        }
    }
}

void EditorUndoManager::removeAllActions()
{
    _undoStack.clear();
    _redoStack.clear();
    for (auto& group : _openGroups)
    {
        group.clear();
    }
    _isUndoing = false;
    _isRedoing = false;
    _disableCount = 0;
}

void EditorUndoManager::removeAllActionsWithTarget(Ref* target)
{
    auto strip = [target](Group& group) {
        group.erase(std::remove_if(group.begin(), group.end(),
                                   [target](const Entry& e) { return e.target.get() == target; }),
                    group.end());
    };
    for (auto* stack : {&_undoStack, &_redoStack})
    {
        for (auto& group : *stack)
        {
            strip(group);
        }
        stack->erase(std::remove_if(stack->begin(), stack->end(),
                                    [](const Group& g) { return g.empty(); }),
                     stack->end());
    }
    for (auto& group : _openGroups)
    {
        strip(group);
    }
}

void EditorUndoManager::disableUndoRegistration()
{
    ++_disableCount;
}

void EditorUndoManager::enableUndoRegistration()
{
    if (_disableCount > 0)
    {
        --_disableCount;
    }
}

bool EditorUndoManager::isUndoRegistrationEnabled() const
{
    return _disableCount == 0;
}
