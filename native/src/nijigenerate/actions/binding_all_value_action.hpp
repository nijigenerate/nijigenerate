#pragma once

#include "binding_value_storage.hpp"
#include "binding_value_notification.hpp"
#include "depthbone_publishers.hpp"
#include "package.hpp"
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <param/binding_impl.hpp>
#include <array>
#include <memory>
#include <new>
#include <source_location>
#include <utility>

namespace nijigenerate::actions::binding {

// Copy the delegate handle, retaining the same callable and captured context.
using BindingUpdate = compat::SharedDelegate<void()>;

template<class F> BindingUpdate ngCreateBindingUpdate(F&& callback) {
    return BindingUpdate(std::forward<F>(callback));
}

template<class I, class... Args>
std::shared_ptr<I> ngCreateBindingAllValueAction(Args&&... args);

template<class I,class Producer>
std::shared_ptr<I> ngCreateBindingAllValueActionDeferred(Producer&& produce) {
    return ngCreateBindingAllValueAction<I>(
        detail::BindingArgumentsProducer<Producer>{std::forward<Producer>(produce)});
}

template<class T>
class ParameterBindingAllValueChangeAction : public LazyBoundAction,
        public std::enable_shared_from_this<ParameterBindingAllValueChangeAction<T>> {
    template<class I, class... Args>
    friend std::shared_ptr<I> ngCreateBindingAllValueAction(Args&&...);
public:
    using TSelf = ParameterBindingAllValueChangeAction<T>;
    ParameterBindingAllValueChangeAction(const ParameterBindingAllValueChangeAction&) = delete;
    ParameterBindingAllValueChangeAction& operator=(const ParameterBindingAllValueChangeAction&) = delete;
    ParameterBindingAllValueChangeAction(ParameterBindingAllValueChangeAction&&) = delete;
    ParameterBindingAllValueChangeAction& operator=(ParameterBindingAllValueChangeAction&&) = delete;
    using TBinding = nicxlive::core::param::ParameterBindingImpl<T>;
    template<class V> using Matrix = nicxlive::core::common::SharedArray<nicxlive::core::common::SharedArray<V>>;

    compat::RetainedStringSlice name;
    std::shared_ptr<TBinding> self;
    Matrix<T> values;
    Matrix<bool> isSet;
    bool undoable = true;
    bool notifyDepthBone = false;

    virtual compat::RetainedString toString() {
        const auto owner = this->weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }

    void updateNewState() override {
        auto owner = this->weak_from_this().lock();
        if (notifyDepthBone) depthboneinvalidation::ngNotifyDepthBoneBindingAllValuesChanged(self);
    }
    void clear() override {}
    void rollback() override {
        auto owner = this->weak_from_this().lock();
        if (undoable) {
            std::swap(values, compat::ngRequireObject(self)->values);
            std::swap(isSet, compat::ngRequireObject(self)->TBinding::getIsSet());
            undoable = false;
            if (notifyDepthBone) depthboneinvalidation::ngNotifyDepthBoneBindingAllValuesChanged(self);
        }
    }
    void redo() override {
        auto owner = this->weak_from_this().lock();
        if (!undoable) {
            std::swap(values, compat::ngRequireObject(self)->values);
            std::swap(isSet, compat::ngRequireObject(self)->TBinding::getIsSet());
            undoable = true;
            if (notifyDepthBone) depthboneinvalidation::ngNotifyDepthBoneBindingAllValuesChanged(self);
        }
    }
    ActionText describe() override { return describeWithKey("%s->%s changed"); }
    ActionText describeUndo() override { return describeWithKey("%s->%s change cancelled"); }
    ActionText getName() override {
        return compat::RetainedString::fromStaticLiteral("this");
    }
    bool merge(ActionPtr other) override {
        auto owner = this->weak_from_this().lock();
        return canMerge(std::move(other));
    }
    bool canMerge(ActionPtr other) override {
        const auto change = std::dynamic_pointer_cast<TSelf>(std::move(other));
        return change && name == change->name;
    }
protected:
    ParameterBindingAllValueChangeAction() {
        detail::prepareBindingActionClass<TSelf, T>("ParameterBindingAllValueChangeAction");
    }
    void initialize(compat::RetainedStringSlice label, std::shared_ptr<TBinding> binding,
                    BindingUpdate update = {}, bool notify = true) {
        name = std::move(label);
        self = binding;
        notifyDepthBone = notify;
        values = detail::duplicate(compat::ngRequireObject(binding)->values);
        // Source reads the actual base field; suppress a derived virtual accessor.
        isSet = detail::duplicate(compat::ngRequireObject(binding)->TBinding::getIsSet());
        if (update) {
            update();
            updateNewState();
        }
    }
private:
    template<std::size_t N> ActionText describeWithKey(const char (&key)[N]) {
        auto owner = this->weak_from_this().lock();
        const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
        const auto receiver = compat::ngRequireObject(self);
        auto bindingName = receiver->getNameDescriptor();
        const std::array<core::translation::TypedFormatArgument, 2> arguments{
            core::translation::ngFormatArgument(std::move(bindingName)),
            core::translation::ngFormatArgument(name)};
        auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
        if (formatted.empty()) return {};
        return detail::retainBindingText(std::move(formatted));
    }
};

// Run the logical D constructor only after the most-derived object and owner
// exist: constructor-time virtual updateNewState must reach a derived override.
template<class I, class... Args>
std::shared_ptr<I> ngCreateBindingAllValueAction(Args&&... args) {
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
template<class T> struct SourceObjectTypeNames<
        ::nijigenerate::actions::binding::ParameterBindingAllValueChangeAction<T>> {
    static RetainedString className() {
        return ::nijigenerate::actions::binding::detail::bindingActionClassName<T>(
            "ParameterBindingAllValueChangeAction");
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::actions::binding::detail::bindingActionTypeSpellings<T>(
            "ParameterBindingAllValueChangeAction").qualifiedArgument;
    }
};

template<class T> struct SourceTypeSpellings<
        ::nijigenerate::actions::binding::ParameterBindingAllValueChangeAction<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::actions::binding::detail::bindingActionTypeSpellings<T>(
            "ParameterBindingAllValueChangeAction");
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<class T> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::binding::ParameterBindingAllValueChangeAction<T>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::binding::ParameterBindingAllValueChangeAction<T>>> {};

} // namespace nijigenerate::core::translation
