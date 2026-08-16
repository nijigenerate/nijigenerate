/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.actions;
public import nijigenerate.actions.node;
public import nijigenerate.actions.camera;
public import nijigenerate.actions.parameter;
public import nijigenerate.actions.binding;
public import nijigenerate.actions.mesheditor;
public import nijigenerate.actions.drawable;
public import nijigenerate.actions.mesh;
public import nijigenerate.actions.deformable;
public import nijigenerate.actions.vertex;
public import nijigenerate.actions.depth;
public import nijigenerate.actions.depthbone;

import std.algorithm;
import std.range;

/**
    An undo/redo-able action
*/
interface Action {
    /**
        Roll back the action that was done
    */
    void rollback();

    /**
        Redo the action that was done
    */
    void redo();

    /**
        Describes the action
    */
    string describe();

    /**
        Describes the action
    */
    string describeUndo();

    /**
        Gets the name of the action
    */
    string getName();

    /**
        Merge action with other action (if possible)

        returns true if merge was successful
    */
    bool merge(Action other);

    /**
        Gets whether this action can merge with an other
    */
    bool canMerge(Action other);
}

/**
   Special case of actions which captures the status of the target to implement undo/redo.
   Action is instantiated before executing any change to the target. status is captured by
   Action implementation. Later, updateState is called after change is applied to the target.
   New status is captured by Action implementation then.
*/
interface LazyBoundAction : Action {
    /** 
     * Confirm 'redo' state from the current status of the target.
     */
    void updateNewState();
    void clear();
}



/**
    Grouping several actions into one undo/redo action.
*/
class GroupAction : Action {
public:
    Action[] actions;

    this(Action[] actions = []) {
        this.actions = actions;
    }

    void addAction(Action action) {
        this.actions ~= action;
    }

    /**
        Rollback
    */
    void rollback() {
        foreach_reverse (action; actions) {
            action.rollback();
        }
    }

    /**
        Redo
    */
    void redo() {
        foreach (action; actions) {
            action.redo();
        }
    }

    /**
        Describe the action
    */
    string describe() {
        string result;
        foreach (action; actions) {
            result ~= action.describe();
        }
        return result;
    }

    /**
        Describe the action
    */
    string describeUndo() {
        string result;
        foreach_reverse (action; actions) {
            result ~= action.describeUndo();
        }
        return result;
    }

    /**
        Gets name of this action
    */
    string getName() {
        return this.stringof;
    }
    
    bool merge(Action other) { 
        bool result = canMerge(other);
        if (!result) return false;
        auto group = cast(GroupAction)other;
        foreach (i; 0..actions.length) {
            result &= actions[i].merge(group.actions[i]);
        } 
        return result;
    }

    bool canMerge(Action other) { 
        if (auto group = cast(GroupAction)other) {
            if (actions.length != group.actions.length) return false;
            return zip(actions, group.actions).all!((t)=>t[0].canMerge(t[1]));
        }
        return false;
    }

    bool empty() { return actions.length == 0; }
}

/** Lets asynchronous subsystems replace a newly pushed action with an owner group. */
alias ClaimAsyncGroupActionHook = AsyncGroupAction function(Action action);
__gshared ClaimAsyncGroupActionHook ngClaimAsyncGroupActionHook;

/** Notifies the history owner when an applied async action gains derived state. */
alias AsyncActionCompletedHook = void function(AsyncGroupAction action);
__gshared AsyncActionCompletedHook ngAsyncActionCompletedHook;

enum AsyncGroupActionState {
    Idle,
    Scheduled,
    Running,
    Completed,
    Undone,
}

enum AsyncGroupActionEvent {
    Scheduled,
    Running,
    Progressed,
    Completed,
    Canceled,
    Redone,
}

/**
 * Identifies one applied generation of an AsyncGroupAction.
 *
 * Schedulers and progress registries keep this value instead of implementing
 * their own undo observers. A token becomes canceled automatically when its
 * owner is undone or advances to another generation on redo.
 */
struct AsyncActionToken {
private:
    AsyncGroupAction owner_;
    ulong generation_;

public:
    bool valid() const { return owner_ !is null && generation_ != 0; }
    bool active() const {
        return valid && owner_.isApplied && owner_.generation == generation_;
    }
    bool canceled() const { return valid && !active; }
    bool acceptsCompletion() const { return !valid || active; }
    ulong generation() const { return generation_; }
}

/**
    A group whose derived work is completed asynchronously.

    Primary actions stay in GroupAction.actions. Actions produced by the
    asynchronous work are kept separately so undo can cancel pending work,
    roll back completed output, and then roll back the operation which caused
    that work. Redo deliberately does not replay the old derived actions; it
    redoes the primary operation and lets the owner schedule fresh work.
*/
class AsyncGroupAction : GroupAction {
public:
    alias LifecycleHandler = void function(AsyncGroupAction action);
    alias MergeHandler = bool function(AsyncGroupAction current, AsyncGroupAction incoming);
    alias Observer = void delegate(AsyncGroupAction action, AsyncGroupActionEvent event);

private:
    struct ObserverEntry {
        ulong id;
        Observer observer;
    }

    Action[] derivedActions;
    LifecycleHandler cancelHandler;
    LifecycleHandler beginUndoHandler;
    LifecycleHandler endUndoHandler;
    LifecycleHandler beginRedoHandler;
    LifecycleHandler endRedoHandler;
    MergeHandler mergeHandler;
    AsyncGroupActionState currentState = AsyncGroupActionState.Idle;
    ulong currentGeneration = 1;
    size_t pendingCount;
    size_t totalCount;
    size_t completedCount;
    bool applied = true;
    ObserverEntry[] observers;
    ulong nextObserverId = 1;

    void notifyObservers(AsyncGroupActionEvent event) {
        auto snapshot = observers.dup;
        foreach (entry; snapshot) {
            if (entry.observer !is null) entry.observer(this, event);
        }
    }

    size_t finishPending(size_t completed) {
        auto finished = min(completed, pendingCount);
        pendingCount -= finished;
        completedCount += finished;
        return finished;
    }

public:
    this(Action[] actions = []) {
        super(actions);
    }

    void setLifecycleHandlers(
        LifecycleHandler cancel,
        LifecycleHandler beginUndo,
        LifecycleHandler endUndo,
        LifecycleHandler beginRedo,
        LifecycleHandler endRedo,
    ) {
        cancelHandler = cancel;
        beginUndoHandler = beginUndo;
        endUndoHandler = endUndo;
        beginRedoHandler = beginRedo;
        endRedoHandler = endRedo;
    }

    void setMergeHandler(MergeHandler handler) {
        mergeHandler = handler;
    }

    ulong addObserver(Observer observer) {
        if (observer is null) return 0;
        auto id = nextObserverId++;
        if (nextObserverId == 0) nextObserverId = 1;
        observers ~= ObserverEntry(id, observer);
        return id;
    }

    void removeObserver(ulong id) {
        if (id == 0) return;
        observers = observers.filter!(entry => entry.id != id).array;
    }

    ulong generation() const { return currentGeneration; }
    AsyncGroupActionState state() const { return currentState; }
    bool isApplied() const { return applied; }
    size_t pendingAsyncCount() const { return pendingCount; }
    size_t totalAsyncCount() const { return totalCount; }
    size_t completedAsyncCount() const { return completedCount; }
    const(Action)[] completedAsyncActions() const { return derivedActions; }
    AsyncActionToken asyncToken() {
        return AsyncActionToken(this, currentGeneration);
    }

    void markAsyncScheduled(size_t count = 1) {
        if (!applied || count == 0) return;
        pendingCount += count;
        totalCount += count;
        currentState = AsyncGroupActionState.Scheduled;
        notifyObservers(AsyncGroupActionEvent.Scheduled);
    }

    void markAsyncRunning() {
        if (!applied) return;
        currentState = AsyncGroupActionState.Running;
        notifyObservers(AsyncGroupActionEvent.Running);
    }

    bool addCompletedAsyncAction(ulong generation, Action action, size_t completed = 1) {
        if (!applied || generation != currentGeneration || action is null) return false;
        derivedActions ~= action;
        if (ngAsyncActionCompletedHook !is null) ngAsyncActionCompletedHook(this);
        finishPending(completed);
        currentState = pendingCount == 0
            ? AsyncGroupActionState.Completed
            : AsyncGroupActionState.Running;
        notifyObservers(pendingCount == 0
            ? AsyncGroupActionEvent.Completed
            : AsyncGroupActionEvent.Progressed);
        return true;
    }

    void markAsyncFinished(size_t completed = 1) {
        if (!applied) return;
        finishPending(completed);
        if (pendingCount == 0) {
            currentState = AsyncGroupActionState.Completed;
            notifyObservers(AsyncGroupActionEvent.Completed);
        } else {
            notifyObservers(AsyncGroupActionEvent.Progressed);
        }
    }

    override void rollback() {
        if (!applied) return;
        if (cancelHandler !is null) cancelHandler(this);
        currentGeneration++;
        pendingCount = 0;
        totalCount = 0;
        completedCount = 0;
        foreach_reverse (action; derivedActions) action.rollback();
        derivedActions.length = 0;
        if (beginUndoHandler !is null) beginUndoHandler(this);
        scope(exit) {
            if (endUndoHandler !is null) endUndoHandler(this);
        }
        super.rollback();
        applied = false;
        currentState = AsyncGroupActionState.Undone;
        notifyObservers(AsyncGroupActionEvent.Canceled);
    }

    override void redo() {
        if (applied) return;
        currentGeneration++;
        pendingCount = 0;
        totalCount = 0;
        completedCount = 0;
        derivedActions.length = 0;
        applied = true;
        currentState = AsyncGroupActionState.Idle;
        notifyObservers(AsyncGroupActionEvent.Redone);
        if (beginRedoHandler !is null) beginRedoHandler(this);
        scope(exit) {
            if (endRedoHandler !is null) endRedoHandler(this);
        }
        super.redo();
    }

    override bool merge(Action other) {
        auto incoming = cast(AsyncGroupAction)other;
        if (!canMerge(incoming)) return false;
        if (mergeHandler !is null && !mergeHandler(this, incoming)) return false;
        if (!super.merge(incoming)) return false;
        derivedActions ~= incoming.derivedActions;
        pendingCount = incoming.pendingCount;
        totalCount = incoming.totalCount;
        completedCount = incoming.completedCount;
        if (pendingCount > 0) {
            currentState = incoming.currentState == AsyncGroupActionState.Running
                ? AsyncGroupActionState.Running
                : AsyncGroupActionState.Scheduled;
            notifyObservers(currentState == AsyncGroupActionState.Running
                ? AsyncGroupActionEvent.Running
                : AsyncGroupActionEvent.Scheduled);
        } else if (derivedActions.length > 0) {
            currentState = AsyncGroupActionState.Completed;
            notifyObservers(AsyncGroupActionEvent.Completed);
        }
        return true;
    }

    override bool canMerge(Action other) {
        auto incoming = cast(AsyncGroupAction)other;
        return incoming !is null &&
            applied && incoming.applied &&
            mergeHandler is incoming.mergeHandler &&
            super.canMerge(incoming);
    }
}
