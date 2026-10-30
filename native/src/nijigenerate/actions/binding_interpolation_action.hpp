#pragma once

#include "package.hpp"
#include "binding.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <param/binding.hpp>
#include <memory>
#include <utility>

namespace nijigenerate::actions::binding {

class ParameterBindingInterpolationChangeAction;
template<class... Args>
std::shared_ptr<ParameterBindingInterpolationChangeAction> ngCreateBindingInterpolationChangeAction(Args&&... args);

class ParameterBindingInterpolationChangeAction : public Action,
    public std::enable_shared_from_this<ParameterBindingInterpolationChangeAction> {
public:
    ParameterBindingInterpolationChangeAction(const ParameterBindingInterpolationChangeAction&) = delete;
    ParameterBindingInterpolationChangeAction& operator=(const ParameterBindingInterpolationChangeAction&) = delete;
    ParameterBindingInterpolationChangeAction(ParameterBindingInterpolationChangeAction&&) = delete;
    ParameterBindingInterpolationChangeAction& operator=(ParameterBindingInterpolationChangeAction&&) = delete;
    using InterpolateMode = nicxlive::core::common::InterpolateMode;
    std::shared_ptr<nicxlive::core::param::ParameterBinding> self;
    InterpolateMode oldMode{};
    InterpolateMode newMode{};
private:
    struct SourceConstructionKey {};
    explicit ParameterBindingInterpolationChangeAction(SourceConstructionKey) {}
    template<class T> friend struct ::nicxlive::core::d_runtime::SourceAllocator;
    template<class... Args>
    friend std::shared_ptr<ParameterBindingInterpolationChangeAction>
        ngCreateBindingInterpolationChangeAction(Args&&... args);

    void initialize(std::shared_ptr<nicxlive::core::param::ParameterBinding> binding,
        InterpolateMode previous, InterpolateMode next) {
        self = std::move(binding);
        oldMode = previous;
        newMode = next;
        nicxlive::core::d_runtime::ngRegisterObjectTypeNames<ParameterBindingInterpolationChangeAction>(
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.binding.ParameterBindingInterpolationChangeAction"),
            compat::RetainedString::fromStaticLiteral(
                "(nijigenerate.actions.binding.ParameterBindingInterpolationChangeAction)"));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterBindingInterpolationChangeAction),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.binding.ParameterBindingInterpolationChangeAction"));
    }
public:
    virtual compat::RetainedString toString() {
        const auto owner = weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }

    ParameterBindingInterpolationChangeAction(std::shared_ptr<nicxlive::core::param::ParameterBinding> binding,
        InterpolateMode previous, InterpolateMode next) {
        initialize(std::move(binding), previous, next);
    }

    void rollback() override {
        auto owner = weak_from_this().lock();
        const auto receiver = compat::ngRequireObject(self);
        receiver->setInterpolateMode(oldMode);
    }
    void redo() override {
        auto owner = weak_from_this().lock();
        const auto receiver = compat::ngRequireObject(self);
        receiver->setInterpolateMode(newMode);
    }
    ActionText describe() override {
        const auto owner = weak_from_this().lock();
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Changed binding interpolation"));
    }
    ActionText describeUndo() override {
        const auto owner = weak_from_this().lock();
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Binding interpolation change cancelled"));
    }
    ActionText getName() override { return compat::RetainedString::fromStaticLiteral("this"); }
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
};

template<class... Args>
std::shared_ptr<ParameterBindingInterpolationChangeAction> ngCreateBindingInterpolationChangeAction(Args&&... args) {
    using Action = ParameterBindingInterpolationChangeAction;
    auto result = std::allocate_shared<Action>(nicxlive::core::d_runtime::SourceAllocator<Action>{},
        Action::SourceConstructionKey{});
    detail::initializeBindingAction([&](auto&&... values) {
        result->initialize(std::forward<decltype(values)>(values)...);
    }, std::forward<Args>(args)...);
    return result;
}

} // namespace nijigenerate::actions::binding

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::actions::binding::ParameterBindingInterpolationChangeAction> {
    inline static constexpr char className[]=
        "nijigenerate.actions.binding.ParameterBindingInterpolationChangeAction";
    inline static constexpr char templateArgumentName[]=
        "(nijigenerate.actions.binding.ParameterBindingInterpolationChangeAction)";
};

template<> struct SourceTypeSpellings<::nijigenerate::actions::binding::ParameterBindingInterpolationChangeAction> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return {
            RetainedString::fromStaticLiteral("(nijigenerate.actions.binding.ParameterBindingInterpolationChangeAction)"),
            RetainedString::fromStaticLiteral("(ParameterBindingInterpolationChangeAction)"),
            RetainedString::fromStaticLiteral("nijigenerate.actions.binding.ParameterBindingInterpolationChangeAction"),
            RetainedString::fromStaticLiteral("ParameterBindingInterpolationChangeAction")};
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::binding::ParameterBindingInterpolationChangeAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::binding::ParameterBindingInterpolationChangeAction>> {};

} // namespace nijigenerate::core::translation
