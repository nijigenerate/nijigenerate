#pragma once

#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>

#include <cstddef>
#include <memory>
#include <span>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::core::actionstack {

enum class ActionStackScopeUnit { Manual, VertexEdit, DepthEdit, OneTimeDeform };
enum class ActionStackClear { All, CurrentLevel };

class ActionStackScope final : public std::enable_shared_from_this<ActionStackScope> {
    class ConstructionKey {
        friend class ActionStackScope;
        ConstructionKey() = default;
    public:
        ConstructionKey(const ConstructionKey&) = default;
    };
public:
    // Registration retains this object, so creation must already have an owner.
    static std::shared_ptr<ActionStackScope> create(ActionStackScopeUnit unit = ActionStackScopeUnit::Manual);
    // Only create() can supply the key; allocation precedes the real constructor.
    explicit ActionStackScope(ConstructionKey, ActionStackScopeUnit unit);
    ActionStackScope(const ActionStackScope&) = delete;
    ActionStackScope& operator=(const ActionStackScope&) = delete;
    ActionStackScope(ActionStackScope&&) = delete;
    ActionStackScope& operator=(ActionStackScope&&) = delete;
    void close();
    bool isActive();
    virtual ~ActionStackScope();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<ActionStackScope> other);
    virtual int opCmp(std::shared_ptr<ActionStackScope> other);
private:
    friend void incActionClearHistory(ActionStackClear);
    bool active{};
    std::size_t level{};
    ActionStackScopeUnit unit{};
};

void incActionInit();
void incActionPush(actions::ActionPtr action);
void incActionUndo();
void incActionRedo();
bool incActionCanUndo();
bool incActionCanRedo();
actions::ActionSlice<actions::ActionPtr> incActionHistory();
settings::SettingsJsonValue ngActionHistoryMemoryInfo();
std::size_t incActionIndex();
actions::ActionPtr incActionTop();
actions::ActionPtr incActionBacktrack(std::size_t backIndex);
template<class T> std::shared_ptr<T> incActionFindLast(int maxSteps) {
    for (int i = 0; i < maxSteps; ++i) {
        auto lastActionGroup = std::dynamic_pointer_cast<actions::GroupAction>(incActionBacktrack(i));
        if (!lastActionGroup) return {};
        if (lastActionGroup->actions.empty()) continue;
        auto slice = lastActionGroup->actions;
        for (auto j = slice.size(); j > 0; --j) {
            auto action = std::dynamic_pointer_cast<T>(slice.get(j - 1));
            if (action) return action;
        }
    }
    return {};
}
void incActionNotifyTopChanged();
void incActionSetUndoHistoryLength(std::size_t length);
std::size_t incActionGetUndoHistoryLength();
void incActionSetIndex(std::size_t index);
void incActionClearHistory(ActionStackClear target = ActionStackClear::All);
void incActionPushGroup(std::shared_ptr<actions::GroupAction> owner = {});
void incActionPopGroup();
std::size_t ngActionStackGroupDepth();
void ngFlushActionStackGroups();
void incActionPushStack();
std::shared_ptr<ActionStackScope> ngOpenActionStackScope(ActionStackScopeUnit unit = ActionStackScopeUnit::Manual);
void ngCloseActionStackScope(ActionStackScopeUnit unit);
void ngRegisterActionStackScopeCloseHandler(ActionStackScopeUnit unit, void (*handler)(ActionStackScopeUnit));
void ngGuardActionStackScopes(std::span<const ActionStackScopeUnit> allowedUnits = {});
bool ngActionStackScopeActive(ActionStackScopeUnit unit);
std::size_t ngActionStackLevel();
void incActionPopStack();
bool incIsActionStackEmpty();
void incActionMarkSaved();
bool incActionIsModified();
void incActionInvalidateSavedState();

} // namespace nijigenerate::core::actionstack

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::core::actionstack::ActionStackScopeUnit> {
    using Enum = ::nijigenerate::core::actionstack::ActionStackScopeUnit;
    inline static constexpr std::string_view name = "ActionStackScopeUnit";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Manual, "Manual"},
        FormatEnumMember<Enum>{Enum::VertexEdit, "VertexEdit"},
        FormatEnumMember<Enum>{Enum::DepthEdit, "DepthEdit"},
        FormatEnumMember<Enum>{Enum::OneTimeDeform, "OneTimeDeform"}};
};
template<> struct FormatEnumTraits<::nijigenerate::core::actionstack::ActionStackClear> {
    using Enum = ::nijigenerate::core::actionstack::ActionStackClear;
    inline static constexpr std::string_view name = "ActionStackClear";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::All, "All"},
        FormatEnumMember<Enum>{Enum::CurrentLevel, "CurrentLevel"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::actionstack::ActionStackScopeUnit> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.actionstack.ActionStackScopeUnit)";
    inline static constexpr char unqualifiedArgument[] = "(ActionStackScopeUnit)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.actionstack.ActionStackScopeUnit";
    inline static constexpr char unqualifiedElement[] = "ActionStackScopeUnit";
};
template<> struct SourceTypeSpellings<::nijigenerate::core::actionstack::ActionStackClear> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.actionstack.ActionStackClear)";
    inline static constexpr char unqualifiedArgument[] = "(ActionStackClear)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.actionstack.ActionStackClear";
    inline static constexpr char unqualifiedElement[] = "ActionStackClear";
};
} // namespace nicxlive::core::d_runtime

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::actionstack::ActionStackScope> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.actionstack.ActionStackScope)";
    inline static constexpr char unqualifiedArgument[] = "(ActionStackScope)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.actionstack.ActionStackScope";
    inline static constexpr char unqualifiedElement[] = "ActionStackScope";
};
template<> struct SourceObjectTypeNames<::nijigenerate::core::actionstack::ActionStackScope> {
    inline static constexpr char className[] = "nijigenerate.core.actionstack.ActionStackScope";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.core.actionstack.ActionStackScope)";
};
}
