// Object protocol adapted from independent object.d, Digital Mars 2000-2011.
// Boost Software License 1.0; see native/port/licenses/druntime-throwable.txt.
#include "package.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>

#include <algorithm>
#include <new>
#include <source_location>
#include <string>
#include <utility>

namespace nijigenerate::actions {
ClaimAsyncGroupActionHook ngClaimAsyncGroupActionHook{};
AsyncActionCompletedHook ngAsyncActionCompletedHook{};
std::shared_ptr<AsyncGroupAction> AsyncGroupAction::checkpointRecorder;

namespace {
// Only module-owned backing operations enter this boundary. Actual action,
// metadata and observer callbacks are evaluated before invoking these lambdas.
template<class Allocate> decltype(auto) allocateActionBacking(Allocate&& allocate) {
    try {
        return std::forward<Allocate>(allocate)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
ActionPtr groupMergeActionAt(ActionSlice<ActionPtr> values, std::size_t index) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
        index, values.size(), compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/actions/package.d"), 149);
    return values[index];
}
template<class T> void prepareActionTypeOrigin() {
    using namespace nicxlive::core::d_runtime;
    using Names=SourceObjectTypeNames<T>;
    ngPrepareSourceTypeSpellings<T>();
    ngRegisterObjectTypeNames<T>(compat::RetainedString::fromStaticLiteral(Names::className),
        compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
}

void prepareGroupActionClassInfo() {
    static const bool registered = [] {
        prepareActionTypeOrigin<Action>();
        prepareActionTypeOrigin<LazyBoundAction>();
        prepareActionTypeOrigin<GroupAction>();
        prepareActionTypeOrigin<AsyncGroupAction>();
        using nicxlive::core::d_runtime::ngRegisterObjectClassName;
        ngRegisterObjectClassName(typeid(GroupAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.GroupAction"));
        ngRegisterObjectClassName(typeid(AsyncGroupAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.AsyncGroupAction"));
        return true;
    }();
    (void)registered;
}
[[maybe_unused]] const bool groupActionClassInfo = [] {
    prepareGroupActionClassInfo();
    return true;
}();
}
compat::RetainedString GroupAction::toString() {
    prepareGroupActionClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}

compat::RetainedString Action::toString() {
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Action::toHash() {
    const auto address = reinterpret_cast<std::size_t>(objectIdentity());
    return address ^ (address >> 4);
}
bool Action::opEquals(ActionPtr other) {
    return other && objectIdentity() == other->objectIdentity();
}
int Action::opCmp(ActionPtr) {
    using namespace ::nicxlive::core::d_runtime;
    ngRegisterObjectClassName(typeid(GroupAction),
        RetainedString::fromStaticLiteral("nijigenerate.actions.GroupAction"));
    ngRegisterObjectClassName(typeid(AsyncGroupAction),
        RetainedString::fromStaticLiteral("nijigenerate.actions.AsyncGroupAction"));
    const auto className = ngObjectClassName(typeid(*this));
    const auto message = allocateActionBacking([&] {
        std::string text = "need opCmp for class ";
        text += className.view();
        return compat::RetainedString(std::move(text));
    });
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool AsyncGroupAction::CompletionMap::Equal::operator()(const ActionPtr& query,
    const ActionPtr& stored) const {
    // TypeInfo_Interface.equals adjusts both handles before Object comparison.
    const auto left = query;
    const auto right = stored;
    if (!left || !right) compat::ngFailNullAccess(false);
    if (left->objectIdentity() == right->objectIdentity()) return true;
    if (left->opEquals(right)) {
        if (typeid(*left) == typeid(*right)) return true;
        const auto leftName = ::nicxlive::core::d_runtime::ngFindObjectClassName(typeid(*left));
        const auto rightName = ::nicxlive::core::d_runtime::ngFindObjectClassName(typeid(*right));
        if (leftName && rightName && leftName->view() == rightName->view()) return true;
        if (right->opEquals(left)) return true;
    }
    return left->opCmp(right) == 0;
}

GroupAction::GroupAction(ActionSlice<ActionPtr> initialActions) : actions(std::move(initialActions)) {}
void GroupAction::addAction(ActionPtr action) {
    const auto receiver = weak_from_this().lock();
    actions.append(std::move(action));
}
void GroupAction::rollback() {
    const auto receiver = weak_from_this().lock();
    auto slice = actions;
    for (auto i = slice.size(); i > 0; --i) compat::ngRequireObject(slice.get(i - 1))->rollback();
}
void GroupAction::redo() {
    const auto receiver = weak_from_this().lock();
    auto slice = actions;
    for (std::size_t i = 0; i < slice.size(); ++i) compat::ngRequireObject(slice.get(i))->redo();
}
ActionText GroupAction::describe() {
    const auto receiver = weak_from_this().lock();
    std::string result;
    auto slice = actions;
    for (std::size_t i = 0; i < slice.size(); ++i) {
        const auto text = compat::ngRequireObject(slice.get(i))->describe();
        allocateActionBacking([&] { result.append(text.view()); });
    }
    if (result.empty()) return {};
    return allocateActionBacking([&] { return compat::RetainedString(std::move(result)); });
}
ActionText GroupAction::describeUndo() {
    const auto receiver = weak_from_this().lock();
    std::string result;
    auto slice = actions;
    for (auto i = slice.size(); i > 0; --i) {
        const auto text = compat::ngRequireObject(slice.get(i - 1))->describeUndo();
        allocateActionBacking([&] { result.append(text.view()); });
    }
    if (result.empty()) return {};
    return allocateActionBacking([&] { return compat::RetainedString(std::move(result)); });
}
ActionText GroupAction::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}
bool GroupAction::merge(ActionPtr other) {
    const auto receiver = weak_from_this().lock();
    bool result = canMerge(other);
    if (!result) return false;
    auto group = std::dynamic_pointer_cast<GroupAction>(other);
    const auto length = actions.size();
    for (std::size_t i = 0; i < length; ++i) {
        const auto action = groupMergeActionAt(actions, i);
        const auto receiver = compat::ngRequireObject(action);
        const auto argument = groupMergeActionAt(compat::ngRequireObject(group)->actions, i);
        result &= receiver->merge(argument);
    }
    return result;
}
bool GroupAction::canMerge(ActionPtr other) {
    const auto receiver = weak_from_this().lock();
    if (auto group = std::dynamic_pointer_cast<GroupAction>(other)) {
        if (actions.size() != group->actions.size()) return false;
        auto left = actions;
        auto right = group->actions;
        for (std::size_t i = 0; i < left.size(); ++i) {
            const auto action = left.get(i);
            const auto argument = right.get(i);
            if (!compat::ngRequireObject(action)->canMerge(argument)) return false;
        }
        return true;
    }
    return false;
}
bool GroupAction::empty() { return actions.empty(); }
bool AsyncActionToken::valid() const { return owner_ && generation_ != 0; }
bool AsyncActionToken::active() const {
    if (!valid()) return false;
    const auto appliedOwner = owner_;
    if (!compat::ngRequireObject(appliedOwner)->isApplied()) return false;
    const auto generationOwner = owner_;
    const auto currentGeneration = compat::ngRequireObject(generationOwner)->generation();
    if (currentGeneration != generation_) return false;
    const auto stateOwner = owner_;
    return compat::ngRequireObject(stateOwner)->state() != AsyncGroupActionState::Failed;
}
bool AsyncActionToken::canceled() const { return valid() && !active(); }
bool AsyncActionToken::acceptsCompletion() const { return !valid() || active(); }
std::uint64_t AsyncActionToken::generation() const { return generation_; }
AsyncGroupAction::AsyncGroupAction(ActionSlice<ActionPtr> initialActions) : GroupAction(std::move(initialActions)) {}
void AsyncGroupAction::notifyObservers(AsyncGroupActionEvent event) {
    const auto lifetime = weak_from_this().lock();
    const auto source = observers;
    auto snapshot = source.duplicate();
    for (auto entry : snapshot) if (entry.observer) entry.observer(shared_from_this(), event);
}
std::size_t AsyncGroupAction::finishPending(std::size_t completed) {
    auto finished = std::min(completed, pendingCount);
    pendingCount -= finished;
    completedCount += finished;
    return finished;
}
void AsyncGroupAction::setLifecycleHandlers(LifecycleHandler cancel, LifecycleHandler beginUndo,
    LifecycleHandler endUndo, LifecycleHandler beginRedo, LifecycleHandler endRedo) {
    cancelHandler = cancel;
    beginUndoHandler = beginUndo;
    endUndoHandler = endUndo;
    beginRedoHandler = beginRedo;
    endRedoHandler = endRedo;
}
void AsyncGroupAction::setMergeHandler(MergeHandler handler) { mergeHandler = handler; }
std::uint64_t AsyncGroupAction::addObserver(Observer observer) {
    if (!observer) return 0;
    const auto lifetime = weak_from_this().lock();
    auto id = nextObserverId++;
    if (nextObserverId == 0) nextObserverId = 1;
    observers.append(ObserverEntry{id, std::move(observer)});
    return id;
}
void AsyncGroupAction::removeObserver(std::uint64_t id) {
    if (id == 0) return;
    const auto lifetime = weak_from_this().lock();
    ActionSlice<ObserverEntry> retained;
    const auto entries = observers;
    for (auto entry : entries) if (entry.id != id) retained.append(std::move(entry));
    observers = std::move(retained);
}
std::uint64_t AsyncGroupAction::generation() const { return currentGeneration; }
AsyncGroupActionState AsyncGroupAction::state() const { return currentState; }
bool AsyncGroupAction::isApplied() const { return applied; }
bool AsyncGroupAction::replaysCompletedResults() const { return replayCompleted; }
bool AsyncGroupAction::empty() { return actions.empty() && derivedActions.empty(); }
void AsyncGroupAction::beginCheckpointRecording() {
    const auto lifetime = weak_from_this().lock();
    if (checkpointRecorder) {
        const auto message = allocateActionBacking([] {
            return compat::RetainedString(std::string("An asynchronous checkpoint is already recording"));
        });
        throw compat::ThrowableException(message,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/package.d"), 316);
    }
    recordsCompletionOrder = true;
    const auto primary = actions;
    completionOrder = primary.duplicate();
    checkpointRecorder = shared_from_this();
}
void AsyncGroupAction::endCheckpointRecording() {
    const auto lifetime = weak_from_this().lock();
    if (checkpointRecorder.get() == this) checkpointRecorder.reset();
}
void AsyncGroupAction::addAction(ActionPtr action) {
    const auto lifetime = weak_from_this().lock();
    GroupAction::addAction(action);
    if (recordsCompletionOrder) completionOrder.append(std::move(action));
}
void AsyncGroupAction::retainCompletedResults() {
    const auto lifetime = weak_from_this().lock();
    if (pendingCount != 0) {
        const auto message = allocateActionBacking([] {
            return compat::RetainedString(std::string("Cannot retain results of unfinished asynchronous work"));
        });
        throw compat::ThrowableException(message,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/package.d"), 331);
    }
    const auto retain = [](auto&& recurse, ActionPtr child) -> void {
        if (auto owner = std::dynamic_pointer_cast<AsyncGroupAction>(child)) owner->retainCompletedResults();
        else if (auto group = std::dynamic_pointer_cast<GroupAction>(child)) {
            auto slice = group->actions;
            for (std::size_t i = 0; i < slice.size(); ++i) recurse(recurse, slice.get(i));
        }
    };
    auto primary = actions;
    for (std::size_t i = 0; i < primary.size(); ++i) retain(retain, primary.get(i));
    auto derived = derivedActions;
    for (std::size_t i = 0; i < derived.size(); ++i) retain(retain, derived.get(i));
    if (!recordsCompletionOrder) {
        const auto currentPrimary = actions;
        const auto currentDerived = derivedActions;
        auto combined = currentPrimary.duplicate();
        combined.append(currentDerived);
        completionOrder = std::move(combined);
    }
    replayCompleted = true;
}
std::size_t AsyncGroupAction::pendingAsyncCount() const { return pendingCount; }
std::size_t AsyncGroupAction::totalAsyncCount() const { return totalCount; }
std::size_t AsyncGroupAction::completedAsyncCount() const { return completedCount; }
ConstActionSlice AsyncGroupAction::completedAsyncActions() const {
    return ConstActionSlice(derivedActions);
}
AsyncActionToken AsyncGroupAction::asyncToken() {
    AsyncActionToken result;
    result.owner_ = shared_from_this();
    result.generation_ = currentGeneration;
    return result;
}
void AsyncGroupAction::markAsyncScheduled(std::size_t count) {
    const auto lifetime = weak_from_this().lock();
    if (!applied || currentState == AsyncGroupActionState::Failed || count == 0) return;
    pendingCount += count;
    totalCount += count;
    currentState = AsyncGroupActionState::Scheduled;
    notifyObservers(AsyncGroupActionEvent::Scheduled);
}
void AsyncGroupAction::markAsyncRunning() {
    const auto lifetime = weak_from_this().lock();
    if (!applied || currentState == AsyncGroupActionState::Failed) return;
    currentState = AsyncGroupActionState::Running;
    notifyObservers(AsyncGroupActionEvent::Running);
}
bool AsyncGroupAction::addCompletedAsyncAction(std::uint64_t generationValue, ActionPtr action, std::size_t completed) {
    const auto lifetime = weak_from_this().lock();
    if (!applied || currentState == AsyncGroupActionState::Failed || generationValue != currentGeneration || !action)
        return false;
    derivedActions.append(action);
    if (checkpointRecorder) {
        checkpointRecorder->completionOrder.append(action);
        if (checkpointRecorder.get() != this) delegatedCompletions.insert(action);
    }
    if (ngAsyncActionCompletedHook) ngAsyncActionCompletedHook(shared_from_this());
    finishPending(completed);
    currentState = pendingCount == 0 ? AsyncGroupActionState::Completed : AsyncGroupActionState::Running;
    notifyObservers(pendingCount == 0 ? AsyncGroupActionEvent::Completed : AsyncGroupActionEvent::Progressed);
    return true;
}
void AsyncGroupAction::markAsyncFinished(std::size_t completed) {
    const auto lifetime = weak_from_this().lock();
    if (!applied || currentState == AsyncGroupActionState::Failed) return;
    finishPending(completed);
    if (pendingCount == 0) {
        currentState = AsyncGroupActionState::Completed;
        notifyObservers(AsyncGroupActionEvent::Completed);
    } else notifyObservers(AsyncGroupActionEvent::Progressed);
}
void AsyncGroupAction::markAsyncFailed() {
    const auto lifetime = weak_from_this().lock();
    if (!applied || currentState == AsyncGroupActionState::Failed) return;
    currentGeneration++;
    pendingCount = 0;
    currentState = AsyncGroupActionState::Failed;
    notifyObservers(AsyncGroupActionEvent::Failed);
}
void AsyncGroupAction::rollback() {
    const auto lifetime = weak_from_this().lock();
    if (!applied) return;
    if (cancelHandler) cancelHandler(shared_from_this());
    currentGeneration++;
    pendingCount = 0;
    totalCount = 0;
    completedCount = 0;
    if (!replayCompleted) {
        auto slice = derivedActions;
        for (auto i = slice.size(); i > 0; --i) {
            auto action = slice.get(i - 1);
            if (!delegatedCompletions.contains(action)) compat::ngRequireObject(action)->rollback();
        }
    }
    if (beginUndoHandler) beginUndoHandler(shared_from_this());
    compat::ngRunWithScopeExit([&] {
        if (replayCompleted) {
            auto slice = completionOrder;
            for (auto i = slice.size(); i > 0; --i) {
                auto action = slice.get(i - 1);
                if (!delegatedCompletions.contains(action)) compat::ngRequireObject(action)->rollback();
            }
        } else {
            derivedActions.clear();
            delegatedCompletions.reset();
            GroupAction::rollback();
        }
        applied = false;
        currentState = AsyncGroupActionState::Undone;
        notifyObservers(AsyncGroupActionEvent::Canceled);
    }, [&] {
        if (endUndoHandler) endUndoHandler(shared_from_this());
    });
}
void AsyncGroupAction::redo() {
    const auto lifetime = weak_from_this().lock();
    if (applied) return;
    currentGeneration++;
    pendingCount = 0;
    totalCount = 0;
    completedCount = 0;
    if (!replayCompleted) derivedActions.clear();
    applied = true;
    currentState = AsyncGroupActionState::Idle;
    notifyObservers(AsyncGroupActionEvent::Redone);
    if (beginRedoHandler) beginRedoHandler(shared_from_this());
    compat::ngRunWithScopeExit([&] {
        if (replayCompleted) {
            auto slice = completionOrder;
            for (std::size_t i = 0; i < slice.size(); ++i) {
                auto action = slice.get(i);
                if (!delegatedCompletions.contains(action)) compat::ngRequireObject(action)->redo();
            }
            currentState = AsyncGroupActionState::Completed;
        } else GroupAction::redo();
    }, [&] {
        if (endRedoHandler) endRedoHandler(shared_from_this());
    });
}
bool AsyncGroupAction::merge(ActionPtr other) {
    const auto lifetime = weak_from_this().lock();
    auto incoming = std::dynamic_pointer_cast<AsyncGroupAction>(other);
    if (!canMerge(incoming)) return false;
    if (mergeHandler && !mergeHandler(shared_from_this(), incoming)) return false;
    if (!GroupAction::merge(incoming)) return false;
    derivedActions.append(incoming->derivedActions);
    pendingCount = incoming->pendingCount;
    totalCount = incoming->totalCount;
    completedCount = incoming->completedCount;
    if (pendingCount > 0) {
        currentState = incoming->currentState == AsyncGroupActionState::Running
            ? AsyncGroupActionState::Running : AsyncGroupActionState::Scheduled;
        notifyObservers(currentState == AsyncGroupActionState::Running
            ? AsyncGroupActionEvent::Running : AsyncGroupActionEvent::Scheduled);
    } else if (incoming->currentState == AsyncGroupActionState::Failed) {
        currentState = AsyncGroupActionState::Failed;
        notifyObservers(AsyncGroupActionEvent::Failed);
    } else if (!derivedActions.empty()) {
        currentState = AsyncGroupActionState::Completed;
        notifyObservers(AsyncGroupActionEvent::Completed);
    }
    return true;
}
bool AsyncGroupAction::canMerge(ActionPtr other) {
    const auto lifetime = weak_from_this().lock();
    auto incoming = std::dynamic_pointer_cast<AsyncGroupAction>(other);
    return incoming && !replayCompleted && !incoming->replayCompleted &&
        !recordsCompletionOrder && !incoming->recordsCompletionOrder && applied && incoming->applied &&
        mergeHandler == incoming->mergeHandler && GroupAction::canMerge(incoming);
}
} // namespace nijigenerate::actions
