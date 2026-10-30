#pragma once
#include <nijigenerate/core/typed_format.hpp>

#include <cstdint>
#include <array>
#include <cstddef>
#include <concepts>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <common/resource_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::actions {
class Action;
class AsyncGroupAction;
using ActionPtr = std::shared_ptr<Action>;
using ActionText = compat::RetainedStringSlice;

template<class T> using ActionSlice = compat::Slice<T>;

class ConstActionSlice {
public:
    explicit ConstActionSlice(ActionSlice<ActionPtr> values) : values_(std::move(values)) {}
    std::size_t size() const { return values_.size(); }
    bool empty() const { return values_.empty(); }
    bool isNull() const { return values_.isNull(); }
    std::shared_ptr<const Action> get(std::size_t i) const { return values_.get(i); }
    std::shared_ptr<const Action> operator[](std::size_t i) const { return get(i); }
private:
    ActionSlice<ActionPtr> values_;
};

class Action {
public:
    virtual ~Action() = default;
    // Interface keys use their complete object's protocol, not the interface address.
    const void* objectIdentity() const noexcept { return dynamic_cast<const void*>(this); }
    // Native bridge for the Source interface-to-Object formatting conversion.
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(ActionPtr other);
    virtual int opCmp(ActionPtr other);
    virtual void rollback() = 0;
    virtual void redo() = 0;
    virtual ActionText describe() = 0;
    virtual ActionText describeUndo() = 0;
    virtual ActionText getName() = 0;
    virtual bool merge(ActionPtr other) = 0;
    virtual bool canMerge(ActionPtr other) = 0;
};
class LazyBoundAction : public virtual Action {
public:
    virtual void updateNewState() = 0;
    virtual void clear() = 0;
};
class GroupAction : public virtual Action, public std::enable_shared_from_this<GroupAction> {
public:
    ActionSlice<ActionPtr> actions;
    explicit GroupAction(ActionSlice<ActionPtr> actions = {});
    GroupAction(const GroupAction&) = delete;
    GroupAction& operator=(const GroupAction&) = delete;
    GroupAction(GroupAction&&) = delete;
    GroupAction& operator=(GroupAction&&) = delete;
    virtual void addAction(ActionPtr action);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr other) override;
    bool canMerge(ActionPtr other) override;
    virtual bool empty();
    compat::RetainedString toString() override;
};
using ClaimAsyncGroupActionHook = std::shared_ptr<AsyncGroupAction> (*)(ActionPtr);
extern ClaimAsyncGroupActionHook ngClaimAsyncGroupActionHook;
using AsyncActionCompletedHook = void (*)(std::shared_ptr<AsyncGroupAction>);
extern AsyncActionCompletedHook ngAsyncActionCompletedHook;
enum class AsyncGroupActionState : std::int32_t { Idle, Scheduled, Running, Completed, Failed, Undone };
enum class AsyncGroupActionEvent : std::int32_t { Scheduled, Running, Progressed, Completed, Failed, Canceled, Redone };

// Copies retain the same mutable capture context, as source delegate descriptors do.
// Callable construction is the native boundary; descriptor assignment never clones it.
template<class... Args> class RetainedActionDelegate {
    using Function = std::function<void(Args...)>;
    std::shared_ptr<Function> callable_;
public:
    RetainedActionDelegate() noexcept = default;
    RetainedActionDelegate(std::nullptr_t) noexcept {}
    template<class Callable> requires (
        !std::same_as<std::remove_cvref_t<Callable>, RetainedActionDelegate> &&
        std::constructible_from<Function, Callable>)
    RetainedActionDelegate(Callable&& callable) {
        Function function(std::forward<Callable>(callable));
        if (function) callable_ = std::allocate_shared<Function>(
            ::nicxlive::core::d_runtime::SourceAllocator<Function>{}, std::move(function));
    }
    RetainedActionDelegate(const RetainedActionDelegate&) noexcept = default;
    RetainedActionDelegate& operator=(const RetainedActionDelegate&) noexcept = default;
    RetainedActionDelegate(RetainedActionDelegate&& other) noexcept : callable_(other.callable_) {}
    RetainedActionDelegate& operator=(RetainedActionDelegate&& other) noexcept {
        callable_ = other.callable_;
        return *this;
    }
    explicit operator bool() const noexcept { return static_cast<bool>(callable_); }
    void operator()(Args... args) const {
        const auto callable = callable_;
        if (!callable) compat::ngFailNullAccess(false);
        (*callable)(std::move(args)...);
    }
};

struct AsyncActionToken {
    bool valid() const;
    bool active() const;
    bool canceled() const;
    bool acceptsCompletion() const;
    std::uint64_t generation() const;
private:
    friend class AsyncGroupAction;
    std::shared_ptr<AsyncGroupAction> owner_;
    std::uint64_t generation_{};
};
class AsyncGroupAction : public GroupAction {
public:
    // All Group-derived views share the actual GroupAction control block.
    std::shared_ptr<AsyncGroupAction> shared_from_this() {
        return std::dynamic_pointer_cast<AsyncGroupAction>(GroupAction::shared_from_this());
    }
    std::shared_ptr<const AsyncGroupAction> shared_from_this() const {
        return std::dynamic_pointer_cast<const AsyncGroupAction>(GroupAction::shared_from_this());
    }
    std::weak_ptr<AsyncGroupAction> weak_from_this() noexcept {
        return std::dynamic_pointer_cast<AsyncGroupAction>(GroupAction::weak_from_this().lock());
    }
    std::weak_ptr<const AsyncGroupAction> weak_from_this() const noexcept {
        return std::dynamic_pointer_cast<const AsyncGroupAction>(GroupAction::weak_from_this().lock());
    }
    using LifecycleHandler = void (*)(std::shared_ptr<AsyncGroupAction>);
    using MergeHandler = bool (*)(std::shared_ptr<AsyncGroupAction>, std::shared_ptr<AsyncGroupAction>);
    using Observer = RetainedActionDelegate<std::shared_ptr<AsyncGroupAction>, AsyncGroupActionEvent>;
    explicit AsyncGroupAction(ActionSlice<ActionPtr> actions = {});
    virtual void setLifecycleHandlers(LifecycleHandler cancel, LifecycleHandler beginUndo, LifecycleHandler endUndo,
        LifecycleHandler beginRedo, LifecycleHandler endRedo);
    virtual void setMergeHandler(MergeHandler handler);
    virtual std::uint64_t addObserver(Observer observer);
    virtual void removeObserver(std::uint64_t id);
    virtual std::uint64_t generation() const;
    virtual AsyncGroupActionState state() const;
    virtual bool isApplied() const;
    virtual bool replaysCompletedResults() const;
    bool empty() override;
    virtual void beginCheckpointRecording();
    virtual void endCheckpointRecording();
    void addAction(ActionPtr action) override;
    virtual void retainCompletedResults();
    virtual std::size_t pendingAsyncCount() const;
    virtual std::size_t totalAsyncCount() const;
    virtual std::size_t completedAsyncCount() const;
    virtual ConstActionSlice completedAsyncActions() const;
    virtual AsyncActionToken asyncToken();
    virtual void markAsyncScheduled(std::size_t count = 1);
    virtual void markAsyncRunning();
    virtual bool addCompletedAsyncAction(std::uint64_t generation, ActionPtr action, std::size_t completed = 1);
    virtual void markAsyncFinished(std::size_t completed = 1);
    virtual void markAsyncFailed();
    void rollback() override;
    void redo() override;
    bool merge(ActionPtr other) override;
    bool canMerge(ActionPtr other) override;
private:
    struct ObserverEntry { std::uint64_t id = 0; Observer observer; };
    class CompletionMap {
        struct Hash {
            std::size_t operator()(const ActionPtr& action) const {
                const auto object = action;
                return object ? object->toHash() : 0;
            }
        };
        struct Equal {
            bool operator()(const ActionPtr& query, const ActionPtr& stored) const;
        };
        using Storage = ::nicxlive::core::common::ResourceKeyMap<ActionPtr, bool, Hash, Equal>;
        Storage storage_;
    public:
        bool isNull() const noexcept { return storage_.isNull(); }
        bool contains(const ActionPtr& action) const {
            const auto storage = storage_;
            return storage.contains(action);
        }
        void insert(const ActionPtr& action) {
            // The canonical provider retains the actual value cell through assignment.
            storage_[action] = true;
        }
        void reset() noexcept { storage_ = Storage{}; }
    };
    ActionSlice<ActionPtr> derivedActions;
    LifecycleHandler cancelHandler{}, beginUndoHandler{}, endUndoHandler{}, beginRedoHandler{}, endRedoHandler{};
    MergeHandler mergeHandler{};
    AsyncGroupActionState currentState = AsyncGroupActionState::Idle;
    std::uint64_t currentGeneration = 1;
    std::size_t pendingCount{}, totalCount{}, completedCount{};
    bool applied = true;
    bool replayCompleted{};
    CompletionMap delegatedCompletions;
    bool recordsCompletionOrder{};
    ActionSlice<ActionPtr> completionOrder;
    static std::shared_ptr<AsyncGroupAction> checkpointRecorder;
    ActionSlice<ObserverEntry> observers;
    std::uint64_t nextObserverId = 1;
    void notifyObservers(AsyncGroupActionEvent event);
    std::size_t finishPending(std::size_t completed);
};
} // namespace nijigenerate::actions

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<actions::AsyncGroupActionState> {
    using Enum = actions::AsyncGroupActionState;
    inline static constexpr std::string_view name = "AsyncGroupActionState";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Idle, "Idle"},
        FormatEnumMember<Enum>{Enum::Scheduled, "Scheduled"},
        FormatEnumMember<Enum>{Enum::Running, "Running"},
        FormatEnumMember<Enum>{Enum::Completed, "Completed"},
        FormatEnumMember<Enum>{Enum::Failed, "Failed"},
        FormatEnumMember<Enum>{Enum::Undone, "Undone"}};
};
template<> struct FormatEnumTraits<actions::AsyncGroupActionEvent> {
    using Enum = actions::AsyncGroupActionEvent;
    inline static constexpr std::string_view name = "AsyncGroupActionEvent";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Scheduled, "Scheduled"},
        FormatEnumMember<Enum>{Enum::Running, "Running"},
        FormatEnumMember<Enum>{Enum::Progressed, "Progressed"},
        FormatEnumMember<Enum>{Enum::Completed, "Completed"},
        FormatEnumMember<Enum>{Enum::Failed, "Failed"},
        FormatEnumMember<Enum>{Enum::Canceled, "Canceled"},
        FormatEnumMember<Enum>{Enum::Redone, "Redone"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
#define NG_ACTION_PACKAGE_TYPE_ORIGIN(Type, Qualified, Unqualified, Object) \
    template<> struct SourceTypeSpellings<::nijigenerate::actions::Type> { \
        inline static constexpr bool isNativeObject=Object; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral("(" Qualified ")"), \
                RetainedString::fromStaticLiteral("(" Unqualified ")"), \
                RetainedString::fromStaticLiteral(Qualified),RetainedString::fromStaticLiteral(Unqualified)}; \
        } \
    }
NG_ACTION_PACKAGE_TYPE_ORIGIN(Action,"nijigenerate.actions.Action","Action",true);
NG_ACTION_PACKAGE_TYPE_ORIGIN(LazyBoundAction,"nijigenerate.actions.LazyBoundAction","LazyBoundAction",true);
NG_ACTION_PACKAGE_TYPE_ORIGIN(GroupAction,"nijigenerate.actions.GroupAction","GroupAction",true);
NG_ACTION_PACKAGE_TYPE_ORIGIN(AsyncGroupAction,"nijigenerate.actions.AsyncGroupAction","AsyncGroupAction",true);
NG_ACTION_PACKAGE_TYPE_ORIGIN(AsyncGroupActionState,
    "nijigenerate.actions.AsyncGroupActionState","AsyncGroupActionState",false);
NG_ACTION_PACKAGE_TYPE_ORIGIN(AsyncGroupActionEvent,
    "nijigenerate.actions.AsyncGroupActionEvent","AsyncGroupActionEvent",false);
NG_ACTION_PACKAGE_TYPE_ORIGIN(AsyncActionToken,"nijigenerate.actions.AsyncActionToken","AsyncActionToken",false);
NG_ACTION_PACKAGE_TYPE_ORIGIN(AsyncGroupAction::ObserverEntry,
    "nijigenerate.actions.AsyncGroupAction.ObserverEntry","ObserverEntry",false);
#undef NG_ACTION_PACKAGE_TYPE_ORIGIN

// Static interface origins are independent of dynamic object ClassInfo.
template<> struct SourceObjectTypeNames<::nijigenerate::actions::Action> {
    inline static constexpr char className[]="nijigenerate.actions.Action";
    inline static constexpr char templateArgumentName[]="(nijigenerate.actions.Action)";
};
template<> struct SourceObjectTypeNames<::nijigenerate::actions::LazyBoundAction> {
    inline static constexpr char className[]="nijigenerate.actions.LazyBoundAction";
    inline static constexpr char templateArgumentName[]="(nijigenerate.actions.LazyBoundAction)";
};

// Original actions/package.d: module nijigenerate.actions, non-template class.
template<> struct SourceObjectTypeNames<::nijigenerate::actions::GroupAction> {
    inline static constexpr char className[] = "nijigenerate.actions.GroupAction";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.actions.GroupAction)";
};
template<> struct SourceObjectTypeNames<::nijigenerate::actions::AsyncGroupAction> {
    inline static constexpr char className[] = "nijigenerate.actions.AsyncGroupAction";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.actions.AsyncGroupAction)";
};
}

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::Action>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::Action>> {};
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::LazyBoundAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::LazyBoundAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::GroupAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::GroupAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::AsyncGroupAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::AsyncGroupAction>> {};

} // namespace nijigenerate::core::translation
