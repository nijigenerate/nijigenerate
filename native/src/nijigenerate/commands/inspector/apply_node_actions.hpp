#pragma once

#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/actions/depthbone_publishers.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/panels/inspector/common.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <concepts>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nijigenerate::commands::inspector::apply_node {

namespace detail {
template<class Property> struct InspectorPropertyInfo;
template<class Owner, class Descriptor, bool deformation>
struct InspectorPropertyInfo<panels::inspector::InspectorProperty<Owner, Descriptor, deformation>> {
    using Parent = Owner;
    using Attribute = Descriptor;
};
} // namespace detail

// Bind to the real generated property member, retaining its actual parent/state.
// The descriptor identity comes from that member's type, never from a second
// hand-written property string or a global inspector resolver.
template<class I, auto member> struct InspectorAttributeAccess {
    static_assert(std::derived_from<I, panels::inspector::Inspector>);
    using Property = std::remove_cvref_t<decltype(std::declval<I&>().*member)>;
    using Info = detail::InspectorPropertyInfo<Property>;
    using Descriptor = typename Info::Attribute;
    using Target = typename Descriptor::Target;
    using Value = typename Descriptor::Value;
    static constexpr auto id = Descriptor::id;

    static auto get(std::shared_ptr<I> inspector) {
        std::shared_ptr<typename Info::Parent> parent = inspector;
        return (compat::ngRequireObject(inspector).get()->*member).get(parent);
    }
};

namespace detail {
template<class I, class Access> compat::RetainedString inspectorPropertySourceArguments(bool qualified) {
    const auto inspector = ::nijigenerate::commands::detail::commandResultSourceElement<I>(qualified);
    const auto property = nicxlive::core::d_runtime::ngSourceStringValueArgument(Access::id.view());
    try {
        std::string arguments = "(";
        arguments.append(inspector.view());
        arguments += ", ";
        arguments.append(property.view());
        arguments += ')';
        return compat::RetainedString(std::move(arguments));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class T> void prepareInspectorTypeSpellings() {
    // Obtain genuine suppliers before translating only cache backing allocation.
    const auto names = nicxlive::core::d_runtime::SourceTypeSpellings<T>::get();
    try {
        nicxlive::core::d_runtime::ngRegisterSourceTypeSpellings<T>(names);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class T> T actionValueAt(compat::Slice<T> values, std::size_t index, std::size_t line) {
    if (index >= values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/inspector/apply_node.d"), line);
    // Retain the reached real backing until the value copy is complete.
    return values[index];
}

template<class Access> void notifyDepthBoneInspectorMutation(
    std::shared_ptr<nicxlive::core::nodes::Node> node) {
    constexpr auto property = Access::id.view();
    if constexpr (property == "translationX" || property == "translationY" || property == "translationZ" ||
        property == "rotationX" || property == "rotationY" || property == "rotationZ" ||
        property == "scaleX" || property == "scaleY" || property == "lockToRoot") {
        actions::depthboneinvalidation::ngNotifyDepthBoneTargetChanged(std::move(node),
            actions::depthboneinvalidation::DepthBoneMutationKind::TargetTransform,
            compat::RetainedString::fromStaticLiteral("Target Transform"));
    }
}

inline compat::RetainedString retainInspectorText(std::string formatted) {
    if (formatted.empty()) return {};
    try {
        return compat::RetainedString(std::move(formatted));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class Access, std::size_t N> actions::ActionText describeInspectorMutation(const char (&key)[N]) {
    const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(compat::RetainedString::fromStaticLiteral(Access::id.text))};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return retainInspectorText(std::move(formatted));
}
} // namespace detail

// Native mapping of ApplyInspectorPropCommand.run's source-local _AttrAction.
template<class I, class Access> class InspectorAttributeAction : public actions::Action,
    public std::enable_shared_from_this<InspectorAttributeAction<I, Access>> {
public:
    InspectorAttributeAction(const InspectorAttributeAction&) = delete;
    InspectorAttributeAction& operator=(const InspectorAttributeAction&) = delete;
    InspectorAttributeAction(InspectorAttributeAction&&) = delete;
    InspectorAttributeAction& operator=(InspectorAttributeAction&&) = delete;
    using NodeT = typename Access::Target;
    using ValT = typename Access::Value;
    using Nodes = compat::Slice<std::shared_ptr<NodeT>>;
    using Values = compat::Slice<ValT>;

    std::shared_ptr<I> ni;
    Nodes nodes;
    Values oldVals;
    ValT newVal = panels::inspector::ngInspectorInitialValue<ValT>();

    InspectorAttributeAction(std::shared_ptr<I> inspector, Nodes targets, Values previous, ValT next)
        : ni(std::move(inspector)), nodes(std::move(targets)), oldVals(std::move(previous)), newVal(std::move(next)) {
        auto snapshot = nodes;
        for (auto node : snapshot) detail::notifyDepthBoneInspectorMutation<Access>(std::move(node));
    }
    void rollback() override {
        auto owner = this->weak_from_this().lock();
        auto snapshot = nodes;
        for (std::size_t i = 0; i < snapshot.size(); ++i) {
            auto node = snapshot[i];
            Access::get(ni)->set(node, detail::actionValueAt(oldVals, i, 111));
            compat::ngRequireObject(node)->notifyChange(node, nicxlive::core::nodes::NotifyReason::AttributeChanged);
            detail::notifyDepthBoneInspectorMutation<Access>(std::move(node));
        }
    }
    void redo() override {
        auto owner = this->weak_from_this().lock();
        auto snapshot = nodes;
        for (auto node : snapshot) {
            Access::get(ni)->set(node, newVal);
            compat::ngRequireObject(node)->notifyChange(node, nicxlive::core::nodes::NotifyReason::AttributeChanged);
            detail::notifyDepthBoneInspectorMutation<Access>(std::move(node));
        }
    }
    actions::ActionText describe() override {
        auto owner = this->weak_from_this().lock();
        return detail::describeInspectorMutation<Access>("Changed %s");
    }
    actions::ActionText describeUndo() override {
        auto owner = this->weak_from_this().lock();
        return detail::describeInspectorMutation<Access>("Undo change %s");
    }
    actions::ActionText getName() override { return compat::RetainedString::fromStaticLiteral("this"); }
    // The concrete Source class inherits Object in addition to the Action interface.
    virtual compat::RetainedString toString();
    int opCmp(actions::ActionPtr other) override;
    bool canMerge(actions::ActionPtr other) override {
        auto owner = this->weak_from_this().lock();
        auto change = std::dynamic_pointer_cast<InspectorAttributeAction>(std::move(other));
        if (!change || change->ni != ni || change->nodes.size() != nodes.size()) return false;
        const auto count = nodes.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto leftUuid = [&] {
                const auto node = detail::actionValueAt(nodes, i, 133);
                return compat::ngRequireObject(node)->getUUID();
            }();
            // The left virtual call may replace either live node array.
            const auto rightUuid = [&] {
                const auto node = detail::actionValueAt(change->nodes, i, 133);
                return compat::ngRequireObject(node)->getUUID();
            }();
            if (leftUuid != rightUuid) return false;
        }
        return true;
    }
    bool merge(actions::ActionPtr other) override {
        auto owner = this->weak_from_this().lock();
        auto change = std::dynamic_pointer_cast<InspectorAttributeAction>(std::move(other));
        if (!change) return false;
        // The source intentionally does not call canMerge here.
        newVal = change->newVal;
        return true;
    }
private:
    static void prepareSourceClassInfo();
};

// Native mapping of ToggleInspectorPropCommand.run's _ToggleAttrAction.
template<class I, class Access> class InspectorToggleAttributeAction : public actions::Action,
    public std::enable_shared_from_this<InspectorToggleAttributeAction<I, Access>> {
public:
    InspectorToggleAttributeAction(const InspectorToggleAttributeAction&) = delete;
    InspectorToggleAttributeAction& operator=(const InspectorToggleAttributeAction&) = delete;
    InspectorToggleAttributeAction(InspectorToggleAttributeAction&&) = delete;
    InspectorToggleAttributeAction& operator=(InspectorToggleAttributeAction&&) = delete;
    using NodeT = typename Access::Target;
    using ValT = typename Access::Value;
    static_assert(std::same_as<ValT, bool>, "Toggle command only works for boolean properties");
    using Nodes = compat::Slice<std::shared_ptr<NodeT>>;
    using Values = compat::Slice<bool>;

    std::shared_ptr<I> ni;
    Nodes nodes;
    Values oldVals;

    InspectorToggleAttributeAction(std::shared_ptr<I> inspector, Nodes targets, Values previous)
        : ni(std::move(inspector)), nodes(std::move(targets)), oldVals(std::move(previous)) {
        auto snapshot = nodes;
        for (auto node : snapshot) detail::notifyDepthBoneInspectorMutation<Access>(std::move(node));
    }
    void rollback() override {
        auto owner = this->weak_from_this().lock();
        auto snapshot = nodes;
        for (std::size_t i = 0; i < snapshot.size(); ++i) {
            auto node = snapshot[i];
            Access::get(ni)->set(node, detail::actionValueAt(oldVals, i, 213));
            compat::ngRequireObject(node)->notifyChange(node, nicxlive::core::nodes::NotifyReason::AttributeChanged);
            detail::notifyDepthBoneInspectorMutation<Access>(std::move(node));
        }
    }
    void redo() override {
        auto owner = this->weak_from_this().lock();
        bool allSame = true;
        if (oldVals.size() > 1) {
            const auto count = oldVals.size();
            for (std::size_t i = 1; i < count; ++i) {
                const auto value = detail::actionValueAt(oldVals, i, 224);
                const auto first = detail::actionValueAt(oldVals, 0, 224);
                if (value != first) { allSame = false; break; }
            }
        }
        bool next = true;
        if (allSame) next = !detail::actionValueAt(oldVals, 0, 233);
        auto snapshot = nodes;
        for (auto node : snapshot) {
            Access::get(ni)->set(node, next);
            compat::ngRequireObject(node)->notifyChange(node, nicxlive::core::nodes::NotifyReason::AttributeChanged);
            detail::notifyDepthBoneInspectorMutation<Access>(std::move(node));
        }
    }
    actions::ActionText describe() override {
        auto owner = this->weak_from_this().lock();
        return detail::describeInspectorMutation<Access>("Toggled %s");
    }
    actions::ActionText describeUndo() override {
        auto owner = this->weak_from_this().lock();
        return detail::describeInspectorMutation<Access>("Undo toggle %s");
    }
    actions::ActionText getName() override { return compat::RetainedString::fromStaticLiteral("this"); }
    virtual compat::RetainedString toString();
    int opCmp(actions::ActionPtr other) override;
    bool canMerge(actions::ActionPtr) override { return false; }
    bool merge(actions::ActionPtr) override { return false; }
private:
    static void prepareSourceClassInfo();
};

} // namespace nijigenerate::commands::inspector::apply_node

namespace nijigenerate::commands::inspector::apply_node::detail {

template<class Access> compat::RetainedString inspectorActionSourceArguments(bool qualified) {
    const auto node = ::nijigenerate::commands::detail::
        commandResultSourceElement<typename Access::Target>(qualified);
    const auto value = ::nijigenerate::commands::detail::
        commandResultSourceElement<typename Access::Value>(qualified);
    try {
        std::string arguments = "(";
        arguments.append(node.view());
        arguments += ", ";
        arguments.append(value.view());
        arguments += ')';
        return compat::RetainedString(std::move(arguments));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class I, class Access> compat::RetainedString inspectorActionSourceName(
    bool toggle, bool classInfo, bool qualified) {
    compat::RetainedString scope;
    if (qualified) {
        const auto outerArguments = inspectorPropertySourceArguments<I, Access>(classInfo);
        const auto outer = ::nijigenerate::commands::detail::composeCommandTemplateName(
            "nijigenerate.commands.inspector.apply_node",
            toggle ? "ToggleInspectorPropCommand" : "ApplyInspectorPropCommand", outerArguments, true);
        try {
            std::string text(outer.view());
            text += ".run";
            scope = compat::RetainedString(std::move(text));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    const auto arguments = inspectorActionSourceArguments<Access>(classInfo);
    return ::nijigenerate::commands::detail::composeCommandTemplateName(
        scope.view(), toggle ? "_ToggleAttrAction" : "_AttrAction", arguments, classInfo);
}

template<class I, class Access, bool Toggle> struct InspectorActionSourceNames {
    static compat::RetainedString className() {
        return inspectorActionSourceName<I, Access>(Toggle, true, true);
    }
    static compat::RetainedString templateArgumentName() {
        return ::nijigenerate::commands::detail::commandTypeArgumentGroup(
            inspectorActionSourceName<I, Access>(Toggle, false, true));
    }
    static nicxlive::core::d_runtime::SourceTypeSpellingNames get() {
        const auto qualified = inspectorActionSourceName<I, Access>(Toggle, false, true);
        const auto unqualified = inspectorActionSourceName<I, Access>(Toggle, false, false);
        return {::nijigenerate::commands::detail::commandTypeArgumentGroup(qualified),
            ::nijigenerate::commands::detail::commandTypeArgumentGroup(unqualified), qualified, unqualified};
    }
};

} // namespace nijigenerate::commands::inspector::apply_node::detail

namespace nicxlive::core::d_runtime {

template<class I, class Access>
struct SourceObjectTypeNames<::nijigenerate::commands::inspector::apply_node::InspectorAttributeAction<I, Access>>
    : ::nijigenerate::commands::inspector::apply_node::detail::InspectorActionSourceNames<I, Access, false> {};

template<class I, class Access>
struct SourceTypeSpellings<::nijigenerate::commands::inspector::apply_node::InspectorAttributeAction<I, Access>>
    : ::nijigenerate::commands::inspector::apply_node::detail::InspectorActionSourceNames<I, Access, false> {
    inline static constexpr bool isNativeObject = true;
};

template<class I, class Access>
struct SourceObjectTypeNames<
        ::nijigenerate::commands::inspector::apply_node::InspectorToggleAttributeAction<I, Access>>
    : ::nijigenerate::commands::inspector::apply_node::detail::InspectorActionSourceNames<I, Access, true> {};

template<class I, class Access>
struct SourceTypeSpellings<
        ::nijigenerate::commands::inspector::apply_node::InspectorToggleAttributeAction<I, Access>>
    : ::nijigenerate::commands::inspector::apply_node::detail::InspectorActionSourceNames<I, Access, true> {
    inline static constexpr bool isNativeObject = true;
};

} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands::inspector::apply_node {

template<class I, class Access> void InspectorAttributeAction<I, Access>::prepareSourceClassInfo() {
    using Self = InspectorAttributeAction<I, Access>;
    static const bool prepared = [] {
        ::nijigenerate::commands::detail::prepareCommandResultClass<Self>();
        detail::prepareInspectorTypeSpellings<Self>();
        return true;
    }();
    (void)prepared;
}

template<class I, class Access> compat::RetainedString InspectorAttributeAction<I, Access>::toString() {
    const auto owner = this->weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}

template<class I, class Access> int InspectorAttributeAction<I, Access>::opCmp(actions::ActionPtr other) {
    const auto owner = this->weak_from_this().lock();
    prepareSourceClassInfo();
    return actions::Action::opCmp(std::move(other));
}

template<class I, class Access> void InspectorToggleAttributeAction<I, Access>::prepareSourceClassInfo() {
    using Self = InspectorToggleAttributeAction<I, Access>;
    static const bool prepared = [] {
        ::nijigenerate::commands::detail::prepareCommandResultClass<Self>();
        detail::prepareInspectorTypeSpellings<Self>();
        return true;
    }();
    (void)prepared;
}

template<class I, class Access> compat::RetainedString InspectorToggleAttributeAction<I, Access>::toString() {
    const auto owner = this->weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}

template<class I, class Access> int InspectorToggleAttributeAction<I, Access>::opCmp(actions::ActionPtr other) {
    const auto owner = this->weak_from_this().lock();
    prepareSourceClassInfo();
    return actions::Action::opCmp(std::move(other));
}

} // namespace nijigenerate::commands::inspector::apply_node
