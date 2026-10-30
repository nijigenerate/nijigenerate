#pragma once
#include "simplephysics.hpp"
#include "detail/simplephysics_notification.hpp"
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/simple_physics_driver.hpp>
#include <array>
#include <functional>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::commands::node::simplephysics {
namespace detail {
inline compat::RetainedString retainSimplePhysicsDescription(std::string text) {
    try {
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class T, auto Getter, auto Setter, bool Rescan>
compat::RetainedString simplePhysicsActionTypeName(bool qualifiedArgument, bool module, bool classInfo) {
    static_assert(requires { Getter.sourceIdentifier.view(); Setter.sourceIdentifier.view(); },
        "Translate source function aliases with their defining declaration identifiers");
    using namespace nicxlive::core::d_runtime;
    const auto type = ngSourceTypeSpelling<T>(qualifiedArgument ?
        SourceTypeSpelling::QualifiedElement : SourceTypeSpelling::UnqualifiedElement);
    // Resolve genuine type metadata before the passive string-allocation boundary.
    try {
        std::string name = module ? "nijigenerate.commands.node.simplephysics." : "";
        name += "SimplePhysicsValueChangeAction!(";
        name.append(type.view());
        name += ", ";
        name.append(Getter.sourceIdentifier.view());
        name += ", ";
        name.append(Setter.sourceIdentifier.view());
        name += Rescan ? ", true)" : ", false)";
        if (classInfo) name += ".SimplePhysicsValueChangeAction";
        return compat::RetainedString(std::move(name));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}
}
template<class I, class... Args>
std::shared_ptr<I> ngCreateSimplePhysicsValueAction(Args&&... args);

namespace detail {
template<class> inline constexpr bool isSimplePhysicsActionArguments = false;
template<class Producer>
inline constexpr bool isSimplePhysicsActionArguments<SimplePhysicsActionArguments<Producer>> = true;
}

// Consume the genuine existing Source .init provider, including retained
// handles, declared enum defaults, recursive static arrays and vector NaNs.
// A translated type can still specialize this declaration-side adapter.
template<class T> struct SimplePhysicsValueInitialization {
    static T get() {
        return ::nijigenerate::commands::detail::CommandResultInitialValue<T>::get();
    }
};

template<> struct SimplePhysicsValueInitialization<nicxlive::core::nodes::SourcePhysicsModel> {
    static nicxlive::core::nodes::SourcePhysicsModel get() {
        return nicxlive::core::nodes::SourcePhysicsModel::Pendulum;
    }
};

template<> struct SimplePhysicsValueInitialization<nicxlive::core::nodes::SourceParamMapMode> {
    static nicxlive::core::nodes::SourceParamMapMode get() {
        return nicxlive::core::nodes::SourceParamMapMode::AngleLength;
    }
};

template<class T, auto Getter, auto Setter, bool Rescan>
class SimplePhysicsValueChangeAction : public actions::Action,
    public std::enable_shared_from_this<SimplePhysicsValueChangeAction<T, Getter, Setter, Rescan>> {
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    template<class I, class... Args>
    friend std::shared_ptr<I> ngCreateSimplePhysicsValueAction(Args&&...);
    using Node = std::shared_ptr<nicxlive::core::nodes::SimplePhysicsDriver>;
    void notify(Node node) {
        detail::notifySimplePhysics(node, Rescan);
    }
    template<class U> static U sourceIndex(compat::Slice<U> values, std::size_t index, std::size_t line) {
        if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/node/simplephysics.d"), line);
        return values[index];
    }
public:
    SimplePhysicsValueChangeAction(const SimplePhysicsValueChangeAction&) = delete;
    SimplePhysicsValueChangeAction& operator=(const SimplePhysicsValueChangeAction&) = delete;
    SimplePhysicsValueChangeAction(SimplePhysicsValueChangeAction&&) = delete;
    SimplePhysicsValueChangeAction& operator=(SimplePhysicsValueChangeAction&&) = delete;
    actions::ActionText propName;
    compat::Slice<Node> nodes;
    compat::Slice<T> oldValues;
    T newValue = SimplePhysicsValueInitialization<T>::get();
    compat::RetainedString toString() override {
        [[maybe_unused]] const auto receiverLifetime = this->weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    void rollback() override {
        [[maybe_unused]] const auto receiverLifetime = this->weak_from_this().lock();
        const auto range = nodes;
        for (std::size_t i=0; i<range.size(); ++i) {
            const auto node = range[i];
            const auto previous = sourceIndex(oldValues, i, 149);
            Setter(node, previous);
            notify(node);
        }
    }
    void redo() override {
        [[maybe_unused]] const auto receiverLifetime = this->weak_from_this().lock();
        const auto range = nodes;
        for (auto node : range) {
            Setter(node, newValue);
            notify(node);
        }
    }
    actions::ActionText describe() override {
        [[maybe_unused]] const auto receiverLifetime = this->weak_from_this().lock();
        const auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Changed SimplePhysics %s"));
        const std::array args{core::translation::ngFormatArgument(propName)};
        return detail::retainSimplePhysicsDescription(core::translation::ngFormatTyped(pattern.view(), args));
    }
    actions::ActionText describeUndo() override {
        [[maybe_unused]] const auto receiverLifetime = this->weak_from_this().lock();
        const auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Undo SimplePhysics %s change"));
        const std::array args{core::translation::ngFormatArgument(propName)};
        return detail::retainSimplePhysicsDescription(core::translation::ngFormatTyped(pattern.view(), args));
    }
    actions::ActionText getName() override {
        [[maybe_unused]] const auto receiverLifetime = this->weak_from_this().lock();
        try {
            return compat::RetainedString("SimplePhysics." + std::string(propName.view()));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        }
    }
    bool canMerge(actions::ActionPtr other) override {
        [[maybe_unused]] const auto receiverLifetime = this->weak_from_this().lock();
        if constexpr (std::same_as<T, std::shared_ptr<nicxlive::core::param::Parameter>>) return false;
        auto candidate = std::dynamic_pointer_cast<SimplePhysicsValueChangeAction>(other);
        if (!candidate || candidate->propName != propName || candidate->nodes.size() != nodes.size()) return false;
        const auto range = nodes;
        for (std::size_t i=0; i<range.size(); ++i) {
            const auto node = range[i];
            const auto leftUUID = compat::ngRequireObject(node)->getUUID();
            const auto rightNode = sourceIndex(candidate->nodes, i, 181);
            if (leftUUID != compat::ngRequireObject(rightNode)->getUUID()) return false;
        }
        return true;
    }
    bool merge(actions::ActionPtr other) override {
        [[maybe_unused]] const auto receiverLifetime = this->weak_from_this().lock();
        auto candidate = std::dynamic_pointer_cast<SimplePhysicsValueChangeAction>(other);
        if (!candidate || !canMerge(other)) return false;
        newValue = candidate->newValue;
        return true;
    }
protected:
    SimplePhysicsValueChangeAction() {
        static const bool registered = [] {
            using Self = SimplePhysicsValueChangeAction<T, Getter, Setter, Rescan>;
            (void)nicxlive::core::d_runtime::ngObjectTemplateArgumentName<Self>();
            const auto name = detail::simplePhysicsActionTypeName<T, Getter, Setter, Rescan>(true, true, true);
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Self), name);
            return true;
        }();
        (void)registered;
    }
    void initialize(actions::ActionText name, compat::Slice<Node> targets, T value) {
        propName = name;
        nodes = targets.duplicate();
        newValue = value;
        for (auto node : targets) oldValues.append(Getter(node));
        redo();
    }
};

// The genuine most-derived object and strong owner exist before the original
// constructor body invokes virtual redo, as they do for a D class constructor.
template<class I, class... Args>
std::shared_ptr<I> ngCreateSimplePhysicsValueAction(Args&&... args) {
    auto action = std::allocate_shared<I>(nicxlive::core::d_runtime::SourceAllocator<I>{});
    if constexpr (sizeof...(Args) == 1 &&
        (detail::isSimplePhysicsActionArguments<std::remove_cvref_t<Args>> && ...)) {
        auto&& producer = std::get<0>(std::forward_as_tuple(args...));
        auto arguments = std::invoke(std::forward<decltype(producer.produce)>(producer.produce));
        std::apply([&](auto&&... values) {
            action->initialize(std::forward<decltype(values)>(values)...);
        }, std::move(arguments));
    } else {
        action->initialize(std::forward<Args>(args)...);
    }
    return action;
}
} // namespace nijigenerate::commands::node::simplephysics

namespace nicxlive::core::d_runtime {
template<class T, auto Getter, auto Setter, bool Rescan>
struct SourceTypeSpellings<::nijigenerate::commands::node::simplephysics::
        SimplePhysicsValueChangeAction<T, Getter, Setter, Rescan>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::commands::node::simplephysics::detail;
        const auto qualified = source::simplePhysicsActionTypeName<T, Getter, Setter, Rescan>(false, true, false);
        const auto unqualified = source::simplePhysicsActionTypeName<T, Getter, Setter, Rescan>(false, false, false);
        try {
            return {RetainedString("(" + std::string(qualified.view()) + ")"),
                RetainedString("(" + std::string(unqualified.view()) + ")"), qualified, unqualified};
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        }
    }
};
} // namespace nicxlive::core::d_runtime
