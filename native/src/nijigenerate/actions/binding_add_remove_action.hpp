#pragma once

#include "package.hpp"
#include "binding.hpp"
#include "binding_value_notification.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <param/parameter.hpp>
#include <param/binding_impl.hpp>
#include <array>
#include <memory>
#include <new>
#include <source_location>
#include <utility>

namespace nijigenerate::actions::binding {

template<class I, class... Args> std::shared_ptr<I> ngCreateBindingAddRemoveAction(Args&&... args);

template<class I,class Producer>
std::shared_ptr<I> ngCreateBindingAddRemoveActionDeferred(Producer&& produce) {
    return ngCreateBindingAddRemoveAction<I>(
        detail::BindingArgumentsProducer<Producer>{std::forward<Producer>(produce)});
}

template<bool added = true> class ParameterBindingAddRemoveAction : public Action,
    public std::enable_shared_from_this<ParameterBindingAddRemoveAction<added>> {
    template<class I, class... Args> friend std::shared_ptr<I> ngCreateBindingAddRemoveAction(Args&&...);
public:
    ParameterBindingAddRemoveAction(const ParameterBindingAddRemoveAction&) = delete;
    ParameterBindingAddRemoveAction& operator=(const ParameterBindingAddRemoveAction&) = delete;
    ParameterBindingAddRemoveAction(ParameterBindingAddRemoveAction&&) = delete;
    ParameterBindingAddRemoveAction& operator=(ParameterBindingAddRemoveAction&&) = delete;
    std::shared_ptr<nicxlive::core::param::Parameter> parent;
    std::shared_ptr<nicxlive::core::param::ParameterBinding> self;
    bool notifyDepthBone = false;

    virtual compat::RetainedString toString() {
        const auto owner = this->weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }

    virtual void notifyChange(std::shared_ptr<nicxlive::core::param::ParameterBinding> binding) {
        auto owner = this->weak_from_this().lock();
        if (auto node = compat::ngRequireObject(binding)->getTarget().target.lock())
            node->notifyChange(node, nicxlive::core::nodes::NotifyReason::StructureChanged);
        if (notifyDepthBone) depthboneinvalidation::ngNotifyDepthBoneBindingStructureChanged(binding);
    }
    void rollback() override {
        auto owner = this->weak_from_this().lock();
        if constexpr (!added) {
            const auto receiver = compat::ngRequireObject(parent);
            const auto binding = self;
            receiver->bindings.push_back(binding);
        } else {
            const auto receiver = compat::ngRequireObject(parent);
            const auto binding = self;
            receiver->removeBinding(binding);
        }
        notifyChange(self);
    }
    void redo() override {
        auto owner = this->weak_from_this().lock();
        if constexpr (added) {
            const auto receiver = compat::ngRequireObject(parent);
            const auto binding = self;
            receiver->bindings.push_back(binding);
        } else {
            const auto receiver = compat::ngRequireObject(parent);
            const auto binding = self;
            receiver->removeBinding(binding);
        }
        notifyChange(self);
    }
    ActionText describe() override {
        if constexpr (added) return describeWithKey("Added binding %s");
        else return describeWithKey("Removed binding %s");
    }
    ActionText describeUndo() override {
        if constexpr (added) return describeWithKey("Binding %s was removed");
        else return describeWithKey("Binding %s was added");
    }
    ActionText getName() override { return compat::RetainedString::fromStaticLiteral("this"); }
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }

protected:
    ParameterBindingAddRemoveAction() {
        const auto className=added
            ? compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.binding.ParameterBindingAddRemoveAction!true.ParameterBindingAddRemoveAction")
            : compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.binding.ParameterBindingAddRemoveAction!false.ParameterBindingAddRemoveAction");
        nicxlive::core::d_runtime::ngRegisterObjectTypeNames<ParameterBindingAddRemoveAction>(className,added
            ? compat::RetainedString::fromStaticLiteral(
                "(nijigenerate.actions.binding.ParameterBindingAddRemoveAction!true)")
            : compat::RetainedString::fromStaticLiteral(
                "(nijigenerate.actions.binding.ParameterBindingAddRemoveAction!false)"));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterBindingAddRemoveAction),className);
    }
    void initialize(std::shared_ptr<nicxlive::core::param::Parameter> parameter,
        std::shared_ptr<nicxlive::core::param::ParameterBinding> binding, bool notify = true) {
        parent = parameter;
        self = binding;
        notifyDepthBone = notify;
        notifyChange(binding);
    }
private:
    template<std::size_t N> ActionText describeWithKey(const char (&key)[N]) {
        auto owner = this->weak_from_this().lock();
        const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
        const auto receiver = compat::ngRequireObject(self);
        const std::array<core::translation::TypedFormatArgument, 1> arguments{
            core::translation::ngFormatArgument(receiver->getNameDescriptor())};
        auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
        if (formatted.empty()) return {};
        return detail::retainBindingText(std::move(formatted));
    }
};

using ParameterBindingAddAction = ParameterBindingAddRemoveAction<true>;
using ParameterBindingRemoveAction = ParameterBindingAddRemoveAction<false>;

template<class I, class... Args> std::shared_ptr<I> ngCreateBindingAddRemoveAction(Args&&... args) {
    nicxlive::core::d_runtime::SourceAllocator<I> allocator;
    auto* storage = allocator.allocate(1);
    try {
        ::new (static_cast<void*>(storage)) I();
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    auto action = std::shared_ptr<I>(storage, [allocator](I* object) mutable {
        std::destroy_at(object);
        allocator.deallocate(object, 1);
    }, allocator);
    detail::initializeBindingAction([&](auto&&... values) {
        action->initialize(std::forward<decltype(values)>(values)...);
    },std::forward<Args>(args)...);
    return action;
}

} // namespace nijigenerate::actions::binding

namespace nicxlive::core::d_runtime {
template<bool added> struct SourceObjectTypeNames<
        ::nijigenerate::actions::binding::ParameterBindingAddRemoveAction<added>> {
    static RetainedString className() {
        if constexpr (added) return RetainedString::fromStaticLiteral(
            "nijigenerate.actions.binding.ParameterBindingAddRemoveAction!true.ParameterBindingAddRemoveAction");
        else return RetainedString::fromStaticLiteral(
            "nijigenerate.actions.binding.ParameterBindingAddRemoveAction!false.ParameterBindingAddRemoveAction");
    }
    static RetainedString templateArgumentName() {
        if constexpr (added) return RetainedString::fromStaticLiteral(
            "(nijigenerate.actions.binding.ParameterBindingAddRemoveAction!true)");
        else return RetainedString::fromStaticLiteral(
            "(nijigenerate.actions.binding.ParameterBindingAddRemoveAction!false)");
    }
};

template<bool added> struct SourceTypeSpellings<
        ::nijigenerate::actions::binding::ParameterBindingAddRemoveAction<added>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        if constexpr (added) return {
            RetainedString::fromStaticLiteral("(nijigenerate.actions.binding.ParameterBindingAddRemoveAction!true)"),
            RetainedString::fromStaticLiteral("(ParameterBindingAddRemoveAction!true)"),
            RetainedString::fromStaticLiteral("nijigenerate.actions.binding.ParameterBindingAddRemoveAction!true"),
            RetainedString::fromStaticLiteral("ParameterBindingAddRemoveAction!true")};
        else return {
            RetainedString::fromStaticLiteral("(nijigenerate.actions.binding.ParameterBindingAddRemoveAction!false)"),
            RetainedString::fromStaticLiteral("(ParameterBindingAddRemoveAction!false)"),
            RetainedString::fromStaticLiteral("nijigenerate.actions.binding.ParameterBindingAddRemoveAction!false"),
            RetainedString::fromStaticLiteral("ParameterBindingAddRemoveAction!false")};
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<bool added> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::binding::ParameterBindingAddRemoveAction<added>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::binding::ParameterBindingAddRemoveAction<added>>> {};

} // namespace nijigenerate::core::translation
