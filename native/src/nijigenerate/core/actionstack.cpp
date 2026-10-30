#include "actionstack.hpp"
#include "settings.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <common/resource_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/actions/binding_value_action.hpp>
#include <nijigenerate/actions/binding_all_value_action.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <functional>
#include <new>
#include <source_location>
#include <string>
#include <utility>

namespace nijigenerate::core::actionstack {
using actions::ActionPtr;
using actions::ActionSlice;
using actions::AsyncGroupAction;
using actions::GroupAction;
namespace {
void prepareActionStackScopeClassInfo() {
    static const bool registered = [] {
        try {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ActionStackScope),
                compat::RetainedString::fromStaticLiteral("nijigenerate.core.actionstack.ActionStackScope"));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return true;
    }();
    (void)registered;
}
template<class T, class... Args> std::shared_ptr<T> allocateStackObject(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{}, std::forward<Args>(args)...);
}
struct ScopeUnitHash {
    std::size_t operator()(ActionStackScopeUnit value) const noexcept {
        return static_cast<std::size_t>(static_cast<std::int32_t>(value));
    }
};
template<class T> using ScopeMap = nicxlive::core::common::ResourceKeyMap<ActionStackScopeUnit,
    T, ScopeUnitHash, std::equal_to<ActionStackScopeUnit>>;
using ActionObjectPolicy = nicxlive::core::common::ObjectClassKeyPolicy<ActionPtr>;
using ActionObjectHash = ActionObjectPolicy::Hash;
using ActionObjectEqual = ActionObjectPolicy::Equal;
struct TypeNameHash {
    std::size_t operator()(const settings::SettingsString& name) const noexcept {
        return compat::ngPhobosStringHash(name.view());
    }
};
thread_local compat::Slice<ActionSlice<ActionPtr>> histories;
thread_local std::size_t currentLevel = 0;
thread_local compat::Slice<std::shared_ptr<GroupAction>> currentGroup;
thread_local compat::Slice<std::int32_t> groupCount;
thread_local compat::Slice<std::size_t> actionPointer, actionIndex, savedIndex;
thread_local compat::Slice<bool> savedStateValid;
thread_local std::size_t maxUndoHistory{};
thread_local ScopeMap<std::shared_ptr<ActionStackScope>> activeScopes;
thread_local compat::Slice<std::shared_ptr<ActionStackScope>> openScopes;
thread_local ScopeMap<void (*)(ActionStackScopeUnit)> scopeCloseHandlers;

void resizeLevels() {
    histories.resize(currentLevel + 1);
    actionPointer.resize(currentLevel + 1);
    actionIndex.resize(currentLevel + 1);
    currentGroup.resize(currentLevel + 1);
    groupCount.resize(currentLevel + 1);
    savedIndex.resize(currentLevel + 1);
    savedStateValid.resize(currentLevel + 1);
}
bool actionContains(ActionPtr entry, ActionPtr target) {
    if (entry == target) return true;
    auto group = std::dynamic_pointer_cast<GroupAction>(entry);
    if (!group) return false;
    auto slice = group->actions;
    for (std::size_t i = 0; i < slice.size(); ++i) if (actionContains(slice.get(i), target)) return true;
    return false;
}
void incActionAsyncCompletionChanged(std::shared_ptr<AsyncGroupAction> owner) {
    const auto capturedHistories = histories;
    const auto count = capturedHistories.size();
    for (std::size_t level = 0; level < count; ++level) {
        if (level >= savedStateValid.size() || !savedStateValid[level]) continue;
        auto history = capturedHistories.get(level);
        for (std::size_t index = 0; index < history.size(); ++index) {
            if (!actionContains(history.get(index), owner)) continue;
            if (level < savedIndex.size() && index < savedIndex[level]) savedStateValid[level] = false;
            break;
        }
    }
}
}

ActionStackScope::ActionStackScope(ConstructionKey, ActionStackScopeUnit requestedUnit) : unit(requestedUnit) {
    prepareActionStackScopeClassInfo();
    incActionPushStack();
    level = currentLevel;
    active = true;
}
std::shared_ptr<ActionStackScope> ActionStackScope::create(ActionStackScopeUnit unit) {
    auto scope = allocateStackObject<ActionStackScope>(ConstructionKey{}, unit);
    openScopes.append(scope);
    return scope;
}
void ActionStackScope::close() {
    if (!active) return;
    auto keepAlive = weak_from_this().lock();
    compat::Slice<ActionStackScopeUnit> closedUnits;
    compat::Slice<std::shared_ptr<ActionStackScope>> retainedScopes;
    const auto capturedOpenScopes = openScopes;
    for (auto openScope : capturedOpenScopes) {
        if (compat::ngRequireObject(openScope)->level < level) {
            retainedScopes.append(openScope);
            continue;
        }
        compat::ngRequireObject(openScope)->active = false;
        bool haveUnit = false;
        for (auto closedUnit : closedUnits) {
            if (closedUnit == compat::ngRequireObject(openScope)->unit) { haveUnit = true; break; }
        }
        if (!haveUnit) closedUnits.append(compat::ngRequireObject(openScope)->unit);
        if (compat::ngRequireObject(openScope)->unit != ActionStackScopeUnit::Manual) {
            if (auto found = activeScopes.find(compat::ngRequireObject(openScope)->unit); found != activeScopes.end()) {
                if (found->second == openScope) activeScopes.erase(compat::ngRequireObject(openScope)->unit);
            }
        }
    }
    openScopes = std::move(retainedScopes);
    while (currentLevel >= level && currentLevel > 0) {
        ngFlushActionStackGroups();
        incActionPopStack();
    }
    for (auto closedUnit : closedUnits) {
        if (auto found = scopeCloseHandlers.find(closedUnit); found != scopeCloseHandlers.end()) {
            if (!found->second) compat::ngFailNullAccess(false);
            found->second(closedUnit);
        }
    }
}
bool ActionStackScope::isActive() { return active; }
ActionStackScope::~ActionStackScope() { close(); }

// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: native/port/licenses/druntime-throwable.txt.
compat::RetainedString ActionStackScope::toString() {
    prepareActionStackScopeClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t ActionStackScope::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(this);
    return address ^ (address >> 4);
}
bool ActionStackScope::opEquals(std::shared_ptr<ActionStackScope> other) {
    return other.get() == this;
}
int ActionStackScope::opCmp(std::shared_ptr<ActionStackScope>) {
    prepareActionStackScopeClassInfo();
    compat::RetainedString message;
    try {
        auto bytes = std::string("need opCmp for class ");
        bytes += nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)).view();
        message = compat::RetainedString(std::move(bytes));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

void incActionInit() {
    maxUndoHistory = settings::incSettingsGet<std::size_t>("MaxUndoHistory", 100);
    resizeLevels();
    savedStateValid[currentLevel] = true;
    actions::ngAsyncActionCompletedHook = &incActionAsyncCompletionChanged;
}
void incActionPush(ActionPtr action) {
    if (actions::ngClaimAsyncGroupActionHook) {
        auto owner = actions::ngClaimAsyncGroupActionHook(action);
        if (owner) action = owner;
    }
    if (currentGroup[currentLevel]) {
        // A virtual addAction may clear or pop the stack containing this slot.
        auto receiver = currentGroup[currentLevel];
        compat::ngRequireObject(receiver)->addAction(action);
    } else {
        if (actionPointer[currentLevel] + 1 > incActionGetUndoHistoryLength()) {
            std::size_t toChop = (actionPointer[currentLevel] + 1) - incActionGetUndoHistoryLength();
            histories[currentLevel] = histories[currentLevel].slice(toChop, histories[currentLevel].size());
            actionPointer[currentLevel] -= toChop;
        }
        if (incActionTop() && compat::ngRequireObject(incActionTop())->canMerge(action)) {
            compat::ngRequireObject(incActionTop())->merge(action);
            incActionNotifyTopChanged();
        } else {
            auto slice = histories[currentLevel].slice(0, actionPointer[currentLevel]).duplicate();
            slice.append(action);
            histories[currentLevel] = std::move(slice);
            actionPointer[currentLevel]++;
        }
    }
}
void incActionUndo() {
    auto& pointer = actionPointer[currentLevel];
    --pointer;
    if (std::bit_cast<std::ptrdiff_t>(pointer) < 0) { pointer = 0; return; }
    compat::ngRequireObject(histories[currentLevel].get(pointer))->rollback();
}
void incActionRedo() {
    if (actionPointer[currentLevel] >= histories[currentLevel].size()) {
        actionPointer[currentLevel] = histories[currentLevel].size();
        return;
    }
    compat::ngRequireObject(histories[currentLevel].get(actionPointer[currentLevel]))->redo();
    actionPointer[currentLevel]++;
}
bool incActionCanUndo() { return actionPointer[currentLevel] > 0; }
bool incActionCanRedo() { return actionPointer[currentLevel] < histories[currentLevel].size(); }
ActionSlice<ActionPtr> incActionHistory() { return histories[currentLevel]; }
settings::SettingsJsonValue ngActionHistoryMemoryInfo() {
    using Json = settings::SettingsJsonValue;
    using String = settings::SettingsString;
    using Names = nicxlive::core::common::ResourceKeyMap<String, std::uint64_t,
        TypeNameHash, std::equal_to<String>>;
    using Seen = nicxlive::core::common::ResourceKeyMap<ActionPtr, bool,
        ActionObjectHash, ActionObjectEqual>;
    using Deformation = nicxlive::core::param::DeformSlot;
    using ValueAction = actions::binding::ParameterBindingValueChangeAction<Deformation,
        nicxlive::core::param::DeformationParameterBinding>;
    using ValuesAction = actions::binding::ParameterBindingAllValueChangeAction<Deformation>;
    std::uint64_t objects = 0, bindingBytes = 0, groups = 0, unclassified = 0;
    Seen seen;
    Names types;
    const auto visit = [&](auto&& self, ActionPtr action) -> void {
        if (!action || seen.contains(action)) return;
        seen[action] = true;
        ++objects;
        const auto name = nicxlive::core::d_runtime::ngFindObjectClassName(typeid(*action));
        if (name) {
            std::uint64_t& count = types[String(*name)];
            ++count;
        } else {
            ++unclassified;
        }
        if (auto group = std::dynamic_pointer_cast<GroupAction>(action)) {
            ++groups;
            const auto children = group->actions;
            for (auto child : children) self(self, child);
        }
        if (auto owner = std::dynamic_pointer_cast<AsyncGroupAction>(action)) {
            const auto children = owner->completedAsyncActions();
            for (std::size_t i = 0; i < children.size(); ++i)
                self(self, std::const_pointer_cast<actions::Action>(children.get(i)));
        }
        if (auto value = std::dynamic_pointer_cast<ValueAction>(action))
            bindingBytes += value->value.vertexOffsets.size() * std::size_t{2} * sizeof(float);
        if (auto values = std::dynamic_pointer_cast<ValuesAction>(action)) {
            const auto rows = values->values;
            for (auto row : rows)
                for (auto value : row)
                    bindingBytes += value.vertexOffsets.size() * std::size_t{2} * sizeof(float);
        }
    };
    const auto capturedHistories = histories;
    for (auto history : capturedHistories)
        for (auto action : history) visit(visit, action);
    const auto capturedGroups = currentGroup;
    for (auto group : capturedGroups) visit(visit, group);
    settings::SettingsJsonObject counts;
    for (auto [name, count] : types) counts[name] = Json(count);
    settings::SettingsJsonObject result;
    result[String("history_entries")] = Json(static_cast<std::uint64_t>(histories[currentLevel].size()));
    result[String("undo_index")] = Json(static_cast<std::uint64_t>(actionPointer[currentLevel]));
    result[String("action_count")] = Json(objects);
    result[String("groups")] = Json(groups);
    // Ordinary shared_ptr ownership exposes no containing-allocation size.
    result[String("action_object_allocation_bytes")] = Json{};
    result[String("binding_snapshot_bytes")] = Json(bindingBytes);
    result[String("types")] = Json(counts);
    result[String("unclassified_action_count")] = Json(unclassified);
    return Json(result);
}
std::size_t incActionIndex() { return actionPointer[currentLevel]; }
ActionPtr incActionTop() {
    auto pointer = actionPointer[currentLevel];
    return pointer > 0 && pointer <= histories[currentLevel].size() ? histories[currentLevel].get(pointer - 1) : nullptr;
}
ActionPtr incActionBacktrack(std::size_t backIndex) {
    if (actionPointer[currentLevel] > backIndex)
        return histories[currentLevel].get(actionPointer[currentLevel] - backIndex - 1);
    return {};
}
void incActionNotifyTopChanged() { histories[currentLevel].resize(actionPointer[currentLevel]); }
void incActionSetUndoHistoryLength(std::size_t length) {
    length = std::clamp<std::size_t>(length, 0, 1000);
    maxUndoHistory = length;
    settings::incSettingsSet("MaxUndoHistory", maxUndoHistory);
}
std::size_t incActionGetUndoHistoryLength() { return maxUndoHistory; }
void incActionSetIndex(std::size_t index) {
    if (index > histories[currentLevel].size()) index = histories[currentLevel].size();
    if (index == 0) while (incActionCanUndo()) incActionUndo();
    if (index < actionPointer[currentLevel]) {
        while (index < actionPointer[currentLevel]) incActionUndo();
    } else if (std::bit_cast<std::ptrdiff_t>(index) > std::bit_cast<std::ptrdiff_t>(actionPointer[currentLevel])) {
        while (std::bit_cast<std::ptrdiff_t>(index) > std::bit_cast<std::ptrdiff_t>(actionPointer[currentLevel])) incActionRedo();
    }
}
void incActionClearHistory(ActionStackClear target) {
    switch (target) {
    case ActionStackClear::All: {
        const auto capturedOpenScopes = openScopes;
        for (auto scope : capturedOpenScopes) compat::ngRequireObject(scope)->active = false;
        openScopes.clear();
        activeScopes.clear();
        currentLevel = 0;
        histories = {ActionSlice<ActionPtr>{}};
        actionPointer.resize(currentLevel + 1);
        actionIndex.resize(currentLevel + 1);
        currentGroup = {nullptr};
        groupCount.resize(currentLevel + 1);
        savedIndex.resize(currentLevel + 1);
        savedStateValid.resize(currentLevel + 1);
        histories[currentLevel].clear();
        actionPointer[currentLevel] = 0;
        currentGroup[currentLevel].reset();
        savedIndex[currentLevel] = 0;
        savedStateValid[currentLevel] = true;
        break;
    }
    case ActionStackClear::CurrentLevel:
        histories[currentLevel].clear();
        actionPointer[currentLevel] = 0;
        currentGroup[currentLevel] = groupCount[currentLevel] > 0 ? allocateStackObject<GroupAction>() : nullptr;
        break;
    default: break;
    }
}
void incActionPushGroup(std::shared_ptr<GroupAction> owner) {
    if (!currentGroup[currentLevel]) currentGroup[currentLevel] = owner ? owner : allocateStackObject<GroupAction>();
    auto& count = groupCount[currentLevel];
    count = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(count) + 1u);
}
void incActionPopGroup() {
    auto& count = groupCount[currentLevel];
    if (count <= 0) return;
    --count;
    if (count == 0 && currentGroup[currentLevel]) {
        auto group = currentGroup[currentLevel];
        currentGroup[currentLevel].reset();
        if (group && !group->empty()) incActionPush(group);
    }
}
std::size_t ngActionStackGroupDepth() {
    return groupCount[currentLevel] > 0 ? static_cast<std::size_t>(groupCount[currentLevel]) : 0;
}
void ngFlushActionStackGroups() {
    while (currentLevel < groupCount.size() && groupCount[currentLevel] > 0) incActionPopGroup();
}
void incActionPushStack() {
    ++currentLevel;
    resizeLevels();
    savedIndex[currentLevel] = 0;
    savedStateValid[currentLevel] = true;
}
std::shared_ptr<ActionStackScope> ngOpenActionStackScope(ActionStackScopeUnit unit) {
    if (unit != ActionStackScopeUnit::Manual) {
        if (auto found = activeScopes.find(unit); found != activeScopes.end()) {
            if (compat::ngRequireObject(found->second)->isActive()) return found->second;
            activeScopes.erase(unit);
        }
    }
    auto created = ActionStackScope::create(unit);
    if (unit != ActionStackScopeUnit::Manual) activeScopes[unit] = created;
    return created;
}
void ngCloseActionStackScope(ActionStackScopeUnit unit) {
    if (auto found = activeScopes.find(unit); found != activeScopes.end()) compat::ngRequireObject(found->second)->close();
}
void ngRegisterActionStackScopeCloseHandler(ActionStackScopeUnit unit, void (*handler)(ActionStackScopeUnit)) {
    scopeCloseHandlers[unit] = handler;
}
void ngGuardActionStackScopes(std::span<const ActionStackScopeUnit> allowedUnits) {
    auto isAllowed = [&](ActionStackScopeUnit unit) {
        for (auto allowed : allowedUnits) if (allowed == unit) return true;
        return false;
    };
    compat::Slice<std::shared_ptr<ActionStackScope>> scopesToClose;
    const auto capturedActiveScopes = activeScopes;
    for (auto [unit, scope] : capturedActiveScopes) if (!isAllowed(unit)) scopesToClose.append(scope);
    for (auto scope : scopesToClose) compat::ngRequireObject(scope)->close();
}
bool ngActionStackScopeActive(ActionStackScopeUnit unit) {
    if (auto found = activeScopes.find(unit); found != activeScopes.end()) return compat::ngRequireObject(found->second)->isActive();
    return false;
}
std::size_t ngActionStackLevel() { return currentLevel; }
void incActionPopStack() {
    if (currentLevel > 0) {
        histories[currentLevel].clear();
        currentGroup[currentLevel].reset();
        --currentLevel;
        resizeLevels();
    }
}
bool incIsActionStackEmpty() { return histories[currentLevel].empty(); }
void incActionMarkSaved() {
    if (savedIndex.size() <= currentLevel) savedIndex.resize(currentLevel + 1);
    if (savedStateValid.size() <= currentLevel) savedStateValid.resize(currentLevel + 1);
    savedIndex[currentLevel] = actionPointer[currentLevel];
    savedStateValid[currentLevel] = true;
}
bool incActionIsModified() {
    auto saved = savedIndex.size() > currentLevel ? savedIndex[currentLevel] : 0;
    bool valid = savedStateValid.size() > currentLevel && savedStateValid[currentLevel];
    return !valid || actionPointer[currentLevel] != saved;
}
void incActionInvalidateSavedState() {
    if (savedStateValid.size() <= currentLevel) savedStateValid.resize(currentLevel + 1);
    savedStateValid[currentLevel] = false;
}
} // namespace nijigenerate::core::actionstack
