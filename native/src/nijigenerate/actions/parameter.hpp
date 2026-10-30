#pragma once
#include "package.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include "parameter_arguments.hpp"
#include <compat/source_allocator.hpp>
#include "binding_actions.hpp"
#include "nijigenerate/ext/package.hpp"
#include "nijigenerate/project.hpp"
#include "nijigenerate/core/typed_format_shared_array.hpp"
#include "nijigenerate/core/typed_format_vec3.hpp"
#include "nijigenerate/core/typed_format_vec2.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nodes/simple_physics_driver.hpp>
#include <resource.hpp>
#include <compat/object_metadata.hpp>
#include <algorithm>
#include <array>
#include <concepts>
#include <functional>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <utility>
#include <limits>

namespace nijigenerate::actions::parameter {
namespace runtime = nicxlive::core::param;
using ParameterRef = std::shared_ptr<runtime::Parameter>;
using BindingRef = std::shared_ptr<runtime::ParameterBinding>;
using Bindings = nicxlive::core::common::SharedArray<BindingRef>;
using GroupRef = std::shared_ptr<ext::ExParameterGroup>;
template<class T> using Matrix = nicxlive::core::common::SharedArray<nicxlive::core::common::SharedArray<T>>;

namespace detail {
inline ActionText retainFormatted(std::string value) {
    try {
        return compat::RetainedString(std::move(value));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class... Args> class BindingMapper {
public:
    using SourceFunction=ActionPtr (*)(BindingRef&,Args&...);
    using NativeFunction=ActionPtr (*)(BindingRef,Args...);
    BindingMapper() = default;
    BindingMapper(std::nullptr_t) noexcept {}
    BindingMapper(NativeFunction function) noexcept : native_(function) {}
    template<class Callable> requires std::is_convertible_v<Callable,NativeFunction>
    BindingMapper(Callable&& function)
        noexcept(noexcept(static_cast<NativeFunction>(std::forward<Callable>(function))))
        : native_(static_cast<NativeFunction>(std::forward<Callable>(function))) {}
    static BindingMapper fromSource(SourceFunction function) noexcept {
        BindingMapper result;
        result.source_=function;
        return result;
    }
    ActionPtr invoke(BindingRef binding,Args&... args) const {
        // Existing C++ by-value pointer intake is a native compatibility path.
        if(native_)return native_(binding,args...);
        const auto function=source_;
        ActionPtr result;
        withParameterArguments<BindingRef,Args...>([&](BindingRef& value,Args&... arguments) {
            if(!function)compat::ngFailNullAccess(false);
            result=function(value,arguments...);
        },binding,args...);
        return result;
    }
private:
    SourceFunction source_ = nullptr;
    NativeFunction native_ = nullptr;
};

ActionPtr sourceBindingAddMapper(BindingRef&,ParameterRef&);
ActionPtr sourceBindingRemoveMapper(BindingRef&,ParameterRef&);
ActionPtr sourceBindingChangeMapper(BindingRef&);
ActionPtr sourceBindingValueChangeMapper(BindingRef&,int&,int&,bool&);

template<class NameGetter> ActionText describe(compat::RetainedString key, NameGetter getName) {
    const auto pattern = core::translation::ngTranslateRetained(key);
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}
template<class Range, class T> std::ptrdiff_t indexOf(Range range, T value) {
    for (std::size_t i = 0; i < range.size(); ++i)
        if (nicxlive::core::ngResourceObjectEquals(range[i], value)) return static_cast<std::ptrdiff_t>(i);
    return -1;
}
inline compat::Slice<BindingRef> bindingRange(Bindings values) {
    return compat::Slice<BindingRef>::retainedView(values.retentionOwner(), values.data(), values.size());
}
inline compat::Slice<BindingRef> bindingRange(compat::Slice<BindingRef> values) { return values; }
inline ActionText thisName() { return compat::RetainedString::fromStaticLiteral("this"); }

template<class Class, class... Types> void prepareParameterActionClass(const char* declaration) {
    static const bool registered=[declaration] {
        using namespace nicxlive::core::d_runtime;
        constexpr auto context=sizeof...(Types)==1 ? SourceTypeSpelling::QualifiedArgument :
            SourceTypeSpelling::QualifiedElement;
        // Genuine declaration providers run before the passive text allocation.
        const std::array<compat::RetainedString,sizeof...(Types)> arguments{
            ngSourceTypeSpelling<Types>(context)...};
        const auto name=[&] {
            try {
                std::string text="nijigenerate.actions.parameter.";
                text+=declaration;
                text+='!';
                if constexpr(sizeof...(Types)!=1)text+='(';
                for(std::size_t i=0;i<arguments.size();++i) {
                    if(i)text+=", ";
                    text.append(arguments[i].view());
                }
                if constexpr(sizeof...(Types)!=1)text+=')';
                text+='.';
                text+=declaration;
                return compat::RetainedString(std::move(text));
            } catch(const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch(const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }();
        ngRegisterObjectClassName(typeid(Class),name);
        return true;
    }();
    (void)registered;
}

template<class... Types> nicxlive::core::d_runtime::SourceTypeSpellingNames parameterActionTypeSpellings(
        const char* declaration) {
    using namespace nicxlive::core::d_runtime;
    // TypeClass's eponymous parent prints its own type arguments without
    // qualification, independently of ClassInfo's qualified parent arguments.
    constexpr auto context=sizeof...(Types)==1 ? SourceTypeSpelling::UnqualifiedArgument :
        SourceTypeSpelling::UnqualifiedElement;
    const std::array<compat::RetainedString,sizeof...(Types)> arguments{
        ngSourceTypeSpelling<Types>(context)...};
    try {
        std::string unqualified=declaration;
        unqualified+='!';
        if constexpr(sizeof...(Types)!=1)unqualified+='(';
        for(std::size_t i=0;i<arguments.size();++i) {
            if(i)unqualified+=", ";
            unqualified.append(arguments[i].view());
        }
        if constexpr(sizeof...(Types)!=1)unqualified+=')';
        auto qualified=std::string("nijigenerate.actions.parameter.")+unqualified;
        return {compat::RetainedString("("+qualified+")"),compat::RetainedString("("+unqualified+")"),
            compat::RetainedString(std::move(qualified)),compat::RetainedString(std::move(unqualified))};
    } catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

template<bool added = true> class ParameterAddRemoveAction : public Action,
    public std::enable_shared_from_this<ParameterAddRemoveAction<added>> {
    struct SourceConstructionKey {};
    template<class> friend struct ::nicxlive::core::d_runtime::SourceAllocator;
    explicit ParameterAddRemoveAction(SourceConstructionKey) {}
public:
    ParameterAddRemoveAction(const ParameterAddRemoveAction&) = delete;
    ParameterAddRemoveAction& operator=(const ParameterAddRemoveAction&) = delete;
    ParameterAddRemoveAction(ParameterAddRemoveAction&&) = delete;
    ParameterAddRemoveAction& operator=(ParameterAddRemoveAction&&) = delete;
    ParameterRef self;
    std::shared_ptr<nicxlive::core::Puppet> puppet;
    compat::Slice<std::shared_ptr<nicxlive::core::nodes::Driver>> drivers;
    GroupRef originalParent;
    std::int64_t indexInGroup = 0;
    virtual compat::RetainedString toString() {
        const auto receiver=this->weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    template<class... Producers>
    static std::shared_ptr<ParameterAddRemoveAction> createSourceDeferred(Producers&&... producers) {
        using Self = ParameterAddRemoveAction;
        auto result = std::allocate_shared<Self>(
            nicxlive::core::d_runtime::SourceAllocator<Self>{}, SourceConstructionKey{});
        std::tuple<std::decay_t<std::invoke_result_t<Producers&&>>...> arguments{
            std::decay_t<std::invoke_result_t<Producers&&>>(
                std::invoke(std::forward<Producers>(producers)))...};
        std::apply([&](auto&&... args) {
            result->initialize(std::forward<decltype(args)>(args)...);
        }, std::move(arguments));
        return result;
    }
    ParameterAddRemoveAction(ParameterRef value,
        nicxlive::core::common::SharedArray<ParameterRef>*)
        : ParameterAddRemoveAction(value, project::incActivePuppet()) {}
    explicit ParameterAddRemoveAction(ParameterRef value)
        : ParameterAddRemoveAction(value, project::incActivePuppet()) {}
    ParameterAddRemoveAction(ParameterRef value, std::shared_ptr<nicxlive::core::Puppet> owner) {
        initialize(value, owner);
    }
private:
    void initialize(const ParameterRef& value) {
        initialize(value, project::incActivePuppet());
    }
    void initialize(const ParameterRef& value,
            nicxlive::core::common::SharedArray<ParameterRef>*) {
        // The authored pointer overload ignores its argument and performs a
        // separate current-Puppet lookup after all new-expression arguments.
        initialize(value, project::incActivePuppet());
    }
    void initialize(const ParameterRef& value, const std::shared_ptr<nicxlive::core::Puppet>& owner) {
        self = value;
        puppet = owner;
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterAddRemoveAction<added>), added
            ? compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.parameter.ParameterAddRemoveAction!true.ParameterAddRemoveAction")
            : compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.parameter.ParameterAddRemoveAction!false.ParameterAddRemoveAction"));
        const auto extended = std::dynamic_pointer_cast<ext::ExParameter>(value);
        originalParent = extended ? extended->getParent() : nullptr;
        indexInGroup = -1;
        {
            const auto source = compat::ngRequireObject(owner)->getDrivers();
            for (const auto& driver : source) {
                if (const auto physics =
                        std::dynamic_pointer_cast<nicxlive::core::nodes::SimplePhysicsDriver>(driver)) {
                    if (physics->getParam() &&
                            compat::ngRequireObject(physics->getParam())->getUUID() ==
                                compat::ngRequireObject(value)->getUUID()) {
                        drivers.append(driver);
                    }
                }
            }
        }
        {
            const auto selectedDrivers = drivers;
            for (const auto& driver : selectedDrivers) {
                if (const auto physics =
                        std::dynamic_pointer_cast<nicxlive::core::nodes::SimplePhysicsDriver>(driver)) {
                    physics->setParam({});
                }
            }
        }
        notifyParameterResourceChanged();
    }
public:
    void rollback() override {
        [[maybe_unused]] const auto actionOwner = this->weak_from_this().lock();
        change(!added, true);
    }
    void redo() override {
        [[maybe_unused]] const auto actionOwner = this->weak_from_this().lock();
        change(added, false);
    }
    ActionText describe() override {
        [[maybe_unused]] const auto actionOwner = this->weak_from_this().lock();
        const auto pattern = core::translation::ngTranslateRetained(added
            ? compat::RetainedString::fromStaticLiteral("Added parameter %s")
            : compat::RetainedString::fromStaticLiteral("Removed parameter %s"));
        const std::array<core::translation::TypedFormatArgument, 1> arguments{
            core::translation::ngFormatArgument(compat::ngRequireObject(self)->getName())};
        return detail::retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
    }
    ActionText describeUndo() override {
        [[maybe_unused]] const auto actionOwner = this->weak_from_this().lock();
        const auto pattern = core::translation::ngTranslateRetained(added
            ? compat::RetainedString::fromStaticLiteral("Parameter %s was removed")
            : compat::RetainedString::fromStaticLiteral("Parameter %s was added"));
        const std::array<core::translation::TypedFormatArgument, 1> arguments{
            core::translation::ngFormatArgument(compat::ngRequireObject(self)->getName())};
        return detail::retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
    }
    ActionText getName() override { return detail::thisName(); }
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
private:
    void change(bool insert, bool restoreDrivers) {
        const auto newParent = originalParent;
        const auto newIndex = indexInGroup;
        (void)newIndex;
        const auto extended = std::dynamic_pointer_cast<ext::ExParameter>(self);
        if (extended) {
            originalParent = extended->getParent();
            std::ptrdiff_t index = -1;
            if (originalParent) {
                const auto parent = originalParent;
                const auto siblings = parent->children;
                for (std::size_t i = 0; i < siblings.size(); ++i) {
                    if (nicxlive::core::ngResourceObjectEquals(siblings[i], extended)) {
                        index = static_cast<std::ptrdiff_t>(i);
                        break;
                    }
                }
            }
            indexInGroup = index;
        }
        if (insert) {
            compat::ngRequireObject(puppet)->parameters.push_back(self);
            if (extended) extended->setParent(newParent);
        } else {
            const auto receiver = compat::ngRequireObject(puppet);
            const auto parameter = self;
            receiver->removeParameter(parameter);
        }
        {
            const auto selectedDrivers = drivers;
            for (const auto& driver : selectedDrivers) {
                if (const auto physics =
                        std::dynamic_pointer_cast<nicxlive::core::nodes::SimplePhysicsDriver>(driver)) {
                    physics->setParam(restoreDrivers ? self : ParameterRef{});
                }
            }
        }
        notifyParameterResourceChanged();
    }
    void notifyParameterResourceChanged() {
        if (puppet && puppet->root) {
            const auto receiver = puppet->root;
            const auto target = puppet->root;
            receiver->notifyChange(target, nicxlive::core::nodes::NotifyReason::StructureChanged);
        }
    }
};
using ParameterAddAction = ParameterAddRemoveAction<true>;
using ParameterRemoveAction = ParameterAddRemoveAction<false>;

template<class I, class... Args> std::shared_ptr<I> ngCreateParameterValueAction(Args&&... args);
template<class I, class... Producers>
std::shared_ptr<I> ngCreateParameterValueActionDeferred(Producers&&... producers);
template<class T> class ParameterValueChangeAction : public LazyBoundAction,
    public std::enable_shared_from_this<ParameterValueChangeAction<T>> {
    template<class I, class... Args> friend std::shared_ptr<I> ngCreateParameterValueAction(Args&&...);
    template<class I, class... Producers>
    friend std::shared_ptr<I> ngCreateParameterValueActionDeferred(Producers&&...);
public:
    using TSelf = ParameterValueChangeAction<T>;
    ParameterValueChangeAction(const ParameterValueChangeAction&) = delete;
    ParameterValueChangeAction& operator=(const ParameterValueChangeAction&) = delete;
    ActionText name;
    ParameterRef self;
    T oldValue = initialValue();
    T newValue = initialValue();
    // An aliasing shared pointer retains the allocation containing the D pointer target.
    std::shared_ptr<T> valuePtr;
    virtual compat::RetainedString toString() {
        const auto receiver=this->weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    void updateNewState() override {
        const auto owner = this->weak_from_this().lock();
        newValue = *compat::ngRequireObject(valuePtr);
    }
    void clear() override {}
    void rollback() override {
        const auto owner = this->weak_from_this().lock();
        const auto pointer = valuePtr;
        if (!pointer) compat::ngFailNullAccess(true);
        *pointer = oldValue;
    }
    void redo() override {
        const auto owner = this->weak_from_this().lock();
        const auto pointer = valuePtr;
        if (!pointer) compat::ngFailNullAccess(true);
        *pointer = newValue;
    }
    ActionText describe() override { return describeChange(false); }
    ActionText describeUndo() override { return describeChange(true); }
    ActionText getName() override { return name; }
    bool merge(ActionPtr other) override {
        const auto owner = this->weak_from_this().lock();
        if (!canMerge(other)) return false;
        newValue = compat::ngRequireObject(std::dynamic_pointer_cast<TSelf>(other))->newValue;
        return true;
    }
    bool canMerge(ActionPtr other) override {
        const auto change = std::dynamic_pointer_cast<TSelf>(other);
        return change && name == change->name;
    }
protected:
    ParameterValueChangeAction() {
        detail::prepareParameterActionClass<ParameterValueChangeAction<T>,T>("ParameterValueChangeAction");
    }
    template<class Before,class After>
    void initialize(ActionText label,ParameterRef parameter,Before&& before,After&& after,std::shared_ptr<T> pointer) {
        detail::withParameterArguments<T,T>([&](T& previous,T& next) {
            name=label;self=parameter;
            oldValue=previous;newValue=next;valuePtr=pointer;
        },std::forward<Before>(before),std::forward<After>(after));
    }
    void initialize(ActionText label, ParameterRef parameter, std::shared_ptr<T> pointer,
                    binding::BindingUpdate update = {}) {
        name = label; self = parameter; valuePtr = pointer;
        oldValue = *compat::ngRequireObject(pointer);
        if (update) { update(); updateNewState(); }
    }
private:
    ActionText describeChange(bool undo) {
        const auto owner = this->weak_from_this().lock();
        const bool axisPoints = name.view() == "axis points";
        const auto key = axisPoints
            ? (undo ? compat::RetainedString::fromStaticLiteral("%s->%s change cancelled")
                    : compat::RetainedString::fromStaticLiteral("%s->%s changed"))
            : (undo ? compat::RetainedString::fromStaticLiteral("%s->%s changed from %s")
                    : compat::RetainedString::fromStaticLiteral("%s->%s changed to %s"));
        const auto pattern = core::translation::ngTranslateRetained(key);
        const auto parameterName = compat::ngRequireObject(self)->getName();
        if (axisPoints) {
            const std::array<core::translation::TypedFormatArgument, 2> arguments{
                core::translation::ngFormatArgument(parameterName), core::translation::ngFormatArgument(name)};
            return detail::retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
        }
        const std::array<core::translation::TypedFormatArgument, 3> arguments{
            core::translation::ngFormatArgument(parameterName), core::translation::ngFormatArgument(name),
            core::translation::ngFormatArgument(undo ? oldValue : newValue)};
        return detail::retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
    }
    static T initialValue() {
        if constexpr (std::is_floating_point_v<T>) return std::numeric_limits<T>::quiet_NaN();
        else if constexpr (std::same_as<T, char> || std::same_as<T, char8_t>) return static_cast<T>(0xff);
        else if constexpr (std::same_as<T, char16_t> || std::same_as<T, char32_t> || std::same_as<T, wchar_t>)
            return static_cast<T>(0xffff);
        else if constexpr (std::is_enum_v<T> && requires { core::translation::FormatEnumTraits<T>::members; }) {
            static_assert(std::size(core::translation::FormatEnumTraits<T>::members)>0);
            return core::translation::FormatEnumTraits<T>::members[0].value;
        }
        else if constexpr (std::same_as<T, nicxlive::core::math::Vec2>) {
            const float nan = std::numeric_limits<float>::quiet_NaN();
            return {nan, nan};
        } else if constexpr (std::same_as<T, nicxlive::core::math::Vec3>) {
            const float nan = std::numeric_limits<float>::quiet_NaN();
            return {nan, nan, nan};
        } else if constexpr (std::same_as<T, nicxlive::core::math::Vec4>) {
            const float nan = std::numeric_limits<float>::quiet_NaN();
            return {nan, nan, nan, nan};
        } else return T{};
    }
};
template<class I, class... Args> std::shared_ptr<I> ngCreateParameterValueAction(Args&&... args) {
    using Allocator = nicxlive::core::d_runtime::SourceAllocator<I>;
    auto* storage = Allocator{}.allocate(1);
    I* instance;
    try {
        instance = ::new (static_cast<void*>(storage)) I();
    } catch (...) {
        Allocator{}.deallocate(storage, 1);
        throw;
    }
    auto result = std::shared_ptr<I>(instance, [](I* object) noexcept {
        std::destroy_at(object);
        Allocator{}.deallocate(object, 1);
    }, Allocator{});
    result->initialize(std::forward<Args>(args)...);
    return result;
}
template<class I, class... Producers>
std::shared_ptr<I> ngCreateParameterValueActionDeferred(Producers&&... producers) {
    using Allocator = nicxlive::core::d_runtime::SourceAllocator<I>;
    auto* storage = Allocator{}.allocate(1);
    I* instance;
    try {
        instance = ::new (static_cast<void*>(storage)) I();
    } catch (...) {
        Allocator{}.deallocate(storage, 1);
        throw;
    }
    auto result = std::shared_ptr<I>(instance, [](I* object) noexcept {
        std::destroy_at(object);
        Allocator{}.deallocate(object, 1);
    }, Allocator{});
    std::tuple<std::decay_t<std::invoke_result_t<Producers&&>>...> arguments{
        std::decay_t<std::invoke_result_t<Producers&&>>(
            std::invoke(std::forward<Producers>(producers)))...};
    std::apply([&](auto&&... args) {
        result->initialize(std::forward<decltype(args)>(args)...);
    }, std::move(arguments));
    return result;
}
class ParameterNameChangeAction : public ParameterValueChangeAction<nicxlive::core::common::NullableString> {
public:
    using TSelf = ParameterNameChangeAction;
    ParameterNameChangeAction(ParameterRef self, nicxlive::core::common::NullableString oldValue,
                              nicxlive::core::common::NullableString newValue);
    void rollback() override;
    void redo() override;
    bool canMerge(ActionPtr other) override;
};

template<bool added = true> class ParameterGroupAddRemoveAction : public Action,
    public std::enable_shared_from_this<ParameterGroupAddRemoveAction<added>> {
public:
    ParameterGroupAddRemoveAction(const ParameterGroupAddRemoveAction&) = delete;
    ParameterGroupAddRemoveAction& operator=(const ParameterGroupAddRemoveAction&) = delete;
    ParameterGroupAddRemoveAction(ParameterGroupAddRemoveAction&&) = delete;
    ParameterGroupAddRemoveAction& operator=(ParameterGroupAddRemoveAction&&) = delete;
    std::shared_ptr<ext::ExPuppet> puppet;
    GroupRef group;
    std::size_t index = 0;
    compat::Slice<std::shared_ptr<ext::ExParameter>> children;
    virtual compat::RetainedString toString() {
        const auto receiver=this->weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    ParameterGroupAddRemoveAction(std::shared_ptr<ext::ExPuppet> owner, GroupRef value,
        std::size_t position = std::numeric_limits<std::size_t>::max()) : puppet(owner), group(value) {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterGroupAddRemoveAction<added>), added
            ? compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.parameter.ParameterGroupAddRemoveAction!true.ParameterGroupAddRemoveAction")
            : compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.parameter.ParameterGroupAddRemoveAction!false.ParameterGroupAddRemoveAction"));
        index = position == std::numeric_limits<std::size_t>::max()
            ? static_cast<std::size_t>(detail::indexOf(compat::ngRequireObject(owner)->groups, value)) : position;
        if (index == std::numeric_limits<std::size_t>::max()) index = compat::ngRequireObject(owner)->groups.size();
        const auto source = compat::ngRequireObject(value)->children;
        for (const auto& child : source)
            if (auto extended = std::dynamic_pointer_cast<ext::ExParameter>(child)) children.append(extended);
    }
    void rollback() override {
        [[maybe_unused]] const auto actionOwner = this->weak_from_this().lock();
        if constexpr (added) removeGroup(); else insertGroup();
    }
    void redo() override {
        [[maybe_unused]] const auto actionOwner = this->weak_from_this().lock();
        if constexpr (added) insertGroup(); else removeGroup();
    }
    ActionText describe() override {
        [[maybe_unused]] const auto actionOwner = this->weak_from_this().lock();
        return detail::describe(added
            ? compat::RetainedString::fromStaticLiteral("Added parameter group %s")
            : compat::RetainedString::fromStaticLiteral("Removed parameter group %s"),
            [this] { return compat::ngRequireObject(group)->getName(); });
    }
    ActionText describeUndo() override {
        [[maybe_unused]] const auto actionOwner = this->weak_from_this().lock();
        return detail::describe(added
            ? compat::RetainedString::fromStaticLiteral("Parameter group %s was removed")
            : compat::RetainedString::fromStaticLiteral("Parameter group %s was added"),
            [this] { return compat::ngRequireObject(group)->getName(); });
    }
    ActionText getName() override { return detail::thisName(); }
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
private:
    void insertGroup() {
        if (detail::indexOf(compat::ngRequireObject(puppet)->groups, group) >= 0) return;
        const auto at = std::min(index, compat::ngRequireObject(puppet)->groups.size());
        compat::Slice<GroupRef> result;
        for (std::size_t i = 0; i < at; ++i) result.append(compat::ngRequireObject(puppet)->groups[i]);
        result.append(group);
        for (auto i = at; i < compat::ngRequireObject(puppet)->groups.size(); ++i)
            result.append(compat::ngRequireObject(puppet)->groups[i]);
        compat::ngRequireObject(puppet)->groups = result;
        const auto source = children;
        for (const auto& child : source) compat::ngRequireObject(child)->setParent(group);
    }
    void removeGroup() {
        const auto source = compat::ngRequireObject(group)->children.duplicate();
        for (const auto& child : source)
            if (auto extended = std::dynamic_pointer_cast<ext::ExParameter>(child)) extended->setParent(nullptr);
        const auto found = detail::indexOf(compat::ngRequireObject(puppet)->groups, group);
        if (found >= 0) {
            const auto receiver = compat::ngRequireObject(puppet);
            for (auto i = static_cast<std::size_t>(found); i + 1 < receiver->groups.size(); ++i)
                receiver->groups[i] = receiver->groups[i + 1];
            receiver->groups = receiver->groups.slice(0, receiver->groups.size() - 1);
        }
    }
};
using ParameterGroupAddAction = ParameterGroupAddRemoveAction<true>;
using ParameterGroupRemoveAction = ParameterGroupAddRemoveAction<false>;

class ParameterMoveAction : public Action, public std::enable_shared_from_this<ParameterMoveAction> {
    struct SourceConstructionKey {};
    template<class> friend struct ::nicxlive::core::d_runtime::SourceAllocator;
    explicit ParameterMoveAction(SourceConstructionKey) {}
public:
    ParameterMoveAction(const ParameterMoveAction&) = delete;
    ParameterMoveAction& operator=(const ParameterMoveAction&) = delete;
    ParameterMoveAction(ParameterMoveAction&&) = delete;
    ParameterMoveAction& operator=(ParameterMoveAction&&) = delete;
    std::shared_ptr<ext::ExParameter> param;
    GroupRef oldParent, newParent;
    ParameterMoveAction(ParameterRef param, GroupRef oldParent, GroupRef newParent);
    template<class... Producers>
    static std::shared_ptr<ParameterMoveAction> createSourceDeferred(Producers&&... producers) {
        auto result = std::allocate_shared<ParameterMoveAction>(
            nicxlive::core::d_runtime::SourceAllocator<ParameterMoveAction>{}, SourceConstructionKey{});
        std::tuple<std::decay_t<std::invoke_result_t<Producers&&>>...> arguments{
            std::decay_t<std::invoke_result_t<Producers&&>>(
                std::invoke(std::forward<Producers>(producers)))...};
        std::apply([&](auto&&... args) {
            result->initialize(std::forward<decltype(args)>(args)...);
        }, std::move(arguments));
        return result;
    }
    virtual compat::RetainedString toString();
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override { return detail::thisName(); }
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
private:
    void initialize(const ParameterRef& param, const GroupRef& oldParent, const GroupRef& newParent);
};

class ParameterShapeChangeAction : public LazyBoundAction,
    public std::enable_shared_from_this<ParameterShapeChangeAction> {
public:
    ParameterShapeChangeAction(const ParameterShapeChangeAction&) = delete;
    ParameterShapeChangeAction& operator=(const ParameterShapeChangeAction&) = delete;
    ParameterShapeChangeAction(ParameterShapeChangeAction&&) = delete;
    ParameterShapeChangeAction& operator=(ParameterShapeChangeAction&&) = delete;
    struct ValueBindingState { std::shared_ptr<runtime::ValueParameterBinding> binding; Matrix<float> values; Matrix<bool> isSet; };
    struct ParameterBindingState { std::shared_ptr<runtime::ParameterParameterBinding> binding; Matrix<float> values; Matrix<bool> isSet; };
    struct DeformationBindingState { std::shared_ptr<runtime::DeformationParameterBinding> binding; Matrix<runtime::DeformSlot> values; Matrix<bool> isSet; };
    struct State {
        runtime::Vec2 min{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
        runtime::Vec2 max{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
        Matrix<float> axisPoints;
        compat::Slice<ValueBindingState> valueBindings;
        compat::Slice<ParameterBindingState> parameterBindings;
        compat::Slice<DeformationBindingState> deformationBindings;
    };
    ActionText name;
    ParameterRef self;
    State oldState, newState;
    ParameterShapeChangeAction(ActionText name, ParameterRef self);
    virtual compat::RetainedString toString();
    void updateNewState() override;
    void clear() override {}
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override { return name; }
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
private:
    static Matrix<float> dupFloatMatrix(Matrix<float> source);
    static Matrix<bool> dupBoolMatrix(Matrix<bool> source);
    static runtime::DeformSlot dupDeformation(runtime::DeformSlot source);
    static Matrix<runtime::DeformSlot> dupDeformationMatrix(Matrix<runtime::DeformSlot> source);
    State captureState();
    void applyState(State& state);
    void notifyBindingValuesChanged();
};

template<class I, class... ConstructionArgs>
std::shared_ptr<I> ngCreateParameterBindingsAction(ConstructionArgs&&... args);

namespace detail {
// The producer is borrowed only for the synchronous factory invocation. Its
// actual argument expressions run after the complete receiver has an owner.
template<class Producer> struct ParameterArgumentsProducer { Producer&& produce; };
template<class T> inline constexpr bool isParameterArgumentsProducer=false;
template<class Producer>
inline constexpr bool isParameterArgumentsProducer<ParameterArgumentsProducer<Producer>> = true;
}

template<class I,class Producer>
std::shared_ptr<I> ngCreateParameterBindingsActionDeferred(Producer&& produce) {
    return ngCreateParameterBindingsAction<I>(
        detail::ParameterArgumentsProducer<Producer>{std::forward<Producer>(produce)});
}

template<class... Args> class AbstractParameterChangeBindingsAction : public GroupAction, public LazyBoundAction {
    template<class I, class... ConstructionArgs>
    friend std::shared_ptr<I> ngCreateParameterBindingsAction(ConstructionArgs&&...);
public:
    AbstractParameterChangeBindingsAction(const AbstractParameterChangeBindingsAction&) = delete;
    AbstractParameterChangeBindingsAction& operator=(const AbstractParameterChangeBindingsAction&) = delete;
    AbstractParameterChangeBindingsAction(AbstractParameterChangeBindingsAction&&) = delete;
    AbstractParameterChangeBindingsAction& operator=(AbstractParameterChangeBindingsAction&&) = delete;
    using TSelf = AbstractParameterChangeBindingsAction<Args...>;
    ActionText name;
    ParameterRef self;
    // Native compatibility entry; source construction uses the retained factory.
    AbstractParameterChangeBindingsAction(ActionText label, ParameterRef parameter, Bindings bindings,
        detail::BindingMapper<Args...> mapper, Args... args) : GroupAction() {
        detail::prepareParameterActionClass<TSelf,Args...>("AbstractParameterChangeBindingsAction");
        initialize(label, parameter, bindings, mapper, args...);
    }
protected:
    AbstractParameterChangeBindingsAction() : GroupAction() {
        detail::prepareParameterActionClass<TSelf,Args...>("AbstractParameterChangeBindingsAction");
    }
    template<class Range,class... Values>
    void initialize(ActionText label,ParameterRef parameter,Range bindings,
        detail::BindingMapper<Args...> mapper,Values&&... values) {
        detail::withParameterArguments<Args...>([&](Args&... args) {
            name=label;
            self=parameter;
            const auto appendActions=[&](auto source) {
                for(auto bindingValue:source) {
                    const auto action=mapper.invoke(bindingValue,args...);
                    if(action)addAction(action);
                }
            };
            if(bindings.isNull())
                appendActions(detail::bindingRange(compat::ngRequireObject(parameter)->bindings));
            else
                appendActions(bindings);
        },std::forward<Values>(values)...);
    }
public:
    void updateNewState() override {
        const auto receiver = this->weak_from_this().lock();
        const auto source = actions;
        for (const auto& action : source)
            if (auto lazy = std::dynamic_pointer_cast<LazyBoundAction>(action)) lazy->updateNewState();
    }
    void clear() override {}
    ActionText describe() override {
        const auto receiver = this->weak_from_this().lock();
        const auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("%s->%s changed"));
        const std::array<core::translation::TypedFormatArgument, 2> arguments{
            core::translation::ngFormatArgument(compat::ngRequireObject(self)->getName()),
            core::translation::ngFormatArgument(name)};
        return detail::retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
    }
    ActionText describeUndo() override {
        const auto receiver = this->weak_from_this().lock();
        const auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("%s->%s change cancelled"));
        const std::array<core::translation::TypedFormatArgument, 2> arguments{
            core::translation::ngFormatArgument(compat::ngRequireObject(self)->getName()),
            core::translation::ngFormatArgument(name)};
        return detail::retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
    }
    ActionText getName() override { return name; }
};
ActionPtr ngBindingAddMapper(BindingRef binding, ParameterRef parent);
ActionPtr ngBindingRemoveMapper(BindingRef binding, ParameterRef parent);
ActionPtr ngBindingChangeMapper(BindingRef binding);
ActionPtr ngBindingValueChangeMapper(BindingRef binding, int pointx, int pointy, bool notifyDepthBone);
class ParameterAddBindingsAction : public AbstractParameterChangeBindingsAction<ParameterRef> {
    template<class I, class... ConstructionArgs>
    friend std::shared_ptr<I> ngCreateParameterBindingsAction(ConstructionArgs&&...);
    using Base = AbstractParameterChangeBindingsAction<ParameterRef>;
public:
    ParameterAddBindingsAction(ActionText name, ParameterRef self, Bindings bindings)
        : Base(name, self, bindings, &ngBindingAddMapper, self) { registerSourceClass(); }
protected:
    ParameterAddBindingsAction() = default;
    template<class Range>
    void initialize(ActionText name, ParameterRef self, Range bindings) {
        registerSourceClass();
        Base::initialize(name,self,bindings,
            detail::BindingMapper<ParameterRef>::fromSource(&detail::sourceBindingAddMapper),self);
    }
private:
    static void registerSourceClass() {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterAddBindingsAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.parameter.ParameterAddBindingsAction"));
    }
};
class ParameterRemoveBindingsAction : public AbstractParameterChangeBindingsAction<ParameterRef> {
    template<class I, class... ConstructionArgs>
    friend std::shared_ptr<I> ngCreateParameterBindingsAction(ConstructionArgs&&...);
    using Base = AbstractParameterChangeBindingsAction<ParameterRef>;
public:
    ParameterRemoveBindingsAction(ActionText name, ParameterRef self, Bindings bindings)
        : Base(name, self, bindings, &ngBindingRemoveMapper, self) { registerSourceClass(); }
protected:
    ParameterRemoveBindingsAction() = default;
    template<class Range>
    void initialize(ActionText name, ParameterRef self, Range bindings) {
        registerSourceClass();
        Base::initialize(name,self,bindings,
            detail::BindingMapper<ParameterRef>::fromSource(&detail::sourceBindingRemoveMapper),self);
    }
private:
    static void registerSourceClass() {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterRemoveBindingsAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.parameter.ParameterRemoveBindingsAction"));
    }
};
class ParameterChangeBindingsAction : public AbstractParameterChangeBindingsAction<> {
    template<class I, class... ConstructionArgs>
    friend std::shared_ptr<I> ngCreateParameterBindingsAction(ConstructionArgs&&...);
    using Base = AbstractParameterChangeBindingsAction<>;
public:
    ParameterChangeBindingsAction(ActionText name, ParameterRef self, Bindings bindings)
        : Base(name, self, bindings, &ngBindingChangeMapper) { registerSourceClass(); }
protected:
    ParameterChangeBindingsAction() = default;
    template<class Range>
    void initialize(ActionText name, ParameterRef self, Range bindings) {
        registerSourceClass();
        Base::initialize(name,self,bindings,
            detail::BindingMapper<>::fromSource(&detail::sourceBindingChangeMapper));
    }
private:
    static void registerSourceClass() {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterChangeBindingsAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.parameter.ParameterChangeBindingsAction"));
    }
};
class ParameterChangeBindingsValueAction : public AbstractParameterChangeBindingsAction<int, int, bool> {
    template<class I, class... ConstructionArgs>
    friend std::shared_ptr<I> ngCreateParameterBindingsAction(ConstructionArgs&&...);
    using Base = AbstractParameterChangeBindingsAction<int, int, bool>;
public:
    ParameterChangeBindingsValueAction(ActionText name, ParameterRef self, Bindings bindings,
        int pointx, int pointy, bool notifyDepthBone = true)
        : Base(name, self, bindings, &ngBindingValueChangeMapper, pointx, pointy, notifyDepthBone) { registerSourceClass(); }
protected:
    ParameterChangeBindingsValueAction() = default;
    template<class Range>
    void initialize(ActionText name, ParameterRef self, Range bindings,
        int pointx, int pointy, bool notifyDepthBone = true) {
        registerSourceClass();
        Base::initialize(name,self,bindings,
            detail::BindingMapper<int,int,bool>::fromSource(&detail::sourceBindingValueChangeMapper),
            pointx,pointy,notifyDepthBone);
    }
private:
    static void registerSourceClass() {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ParameterChangeBindingsValueAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.parameter.ParameterChangeBindingsValueAction"));
    }
};
template<class I, class... ConstructionArgs>
std::shared_ptr<I> ngCreateParameterBindingsAction(ConstructionArgs&&... args) {
    static_assert(std::is_base_of_v<GroupAction, I> && std::is_base_of_v<LazyBoundAction, I>);
    nicxlive::core::d_runtime::SourceAllocator<I> allocator;
    auto* storage = allocator.allocate(1);
    try {
        ::new (static_cast<void*>(storage)) I;
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    auto receiver = std::shared_ptr<I>(storage, [allocator](I* object) mutable {
        std::destroy_at(object);
        allocator.deallocate(object, 1);
    }, allocator);
    if constexpr(sizeof...(ConstructionArgs)==1 &&
        (detail::isParameterArgumentsProducer<std::remove_cvref_t<ConstructionArgs>> && ...)) {
        auto&& producer=std::get<0>(std::forward_as_tuple(args...));
        auto arguments=std::invoke(std::forward<decltype(producer.produce)>(producer.produce));
        std::apply([&](auto&&... values) {
            receiver->initialize(std::forward<decltype(values)>(values)...);
        },std::move(arguments));
    } else {
        receiver->initialize(std::forward<ConstructionArgs>(args)...);
    }
    return receiver;
}

} // namespace nijigenerate::actions::parameter

namespace nicxlive::core::d_runtime {
template<class T> struct SourceTypeSpellings<
        ::nijigenerate::actions::parameter::ParameterValueChangeAction<T>> {
    inline static constexpr bool isNativeObject=true;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::actions::parameter::detail::parameterActionTypeSpellings<T>(
            "ParameterValueChangeAction");
    }
};
template<class... Args> struct SourceTypeSpellings<
        ::nijigenerate::actions::parameter::AbstractParameterChangeBindingsAction<Args...>> {
    inline static constexpr bool isNativeObject=true;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::actions::parameter::detail::parameterActionTypeSpellings<Args...>(
            "AbstractParameterChangeBindingsAction");
    }
};

#define NG_PARAMETER_ACTION_TYPE_ORIGIN(Type, Qualified, Unqualified, Object) \
    template<> struct SourceTypeSpellings<::nijigenerate::actions::parameter::Type> { \
        inline static constexpr bool isNativeObject=Object; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral("(" Qualified ")"), \
                RetainedString::fromStaticLiteral("(" Unqualified ")"), \
                RetainedString::fromStaticLiteral(Qualified),RetainedString::fromStaticLiteral(Unqualified)}; \
        } \
    }
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterAddRemoveAction<true>,
    "nijigenerate.actions.parameter.ParameterAddRemoveAction!true","ParameterAddRemoveAction!true",true);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterAddRemoveAction<false>,
    "nijigenerate.actions.parameter.ParameterAddRemoveAction!false","ParameterAddRemoveAction!false",true);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterGroupAddRemoveAction<true>,
    "nijigenerate.actions.parameter.ParameterGroupAddRemoveAction!true","ParameterGroupAddRemoveAction!true",true);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterGroupAddRemoveAction<false>,
    "nijigenerate.actions.parameter.ParameterGroupAddRemoveAction!false","ParameterGroupAddRemoveAction!false",true);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterNameChangeAction,
    "nijigenerate.actions.parameter.ParameterNameChangeAction","ParameterNameChangeAction",true);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterMoveAction,
    "nijigenerate.actions.parameter.ParameterMoveAction","ParameterMoveAction",true);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterShapeChangeAction,
    "nijigenerate.actions.parameter.ParameterShapeChangeAction","ParameterShapeChangeAction",true);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterAddBindingsAction,
    "nijigenerate.actions.parameter.ParameterAddBindingsAction","ParameterAddBindingsAction",true);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterRemoveBindingsAction,
    "nijigenerate.actions.parameter.ParameterRemoveBindingsAction","ParameterRemoveBindingsAction",true);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterChangeBindingsAction,
    "nijigenerate.actions.parameter.ParameterChangeBindingsAction","ParameterChangeBindingsAction",true);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterChangeBindingsValueAction,
    "nijigenerate.actions.parameter.ParameterChangeBindingsValueAction","ParameterChangeBindingsValueAction",true);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterShapeChangeAction::ValueBindingState,
    "nijigenerate.actions.parameter.ParameterShapeChangeAction.ValueBindingState","ValueBindingState",false);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterShapeChangeAction::ParameterBindingState,
    "nijigenerate.actions.parameter.ParameterShapeChangeAction.ParameterBindingState","ParameterBindingState",false);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterShapeChangeAction::DeformationBindingState,
    "nijigenerate.actions.parameter.ParameterShapeChangeAction.DeformationBindingState","DeformationBindingState",false);
NG_PARAMETER_ACTION_TYPE_ORIGIN(ParameterShapeChangeAction::State,
    "nijigenerate.actions.parameter.ParameterShapeChangeAction.State","State",false);
#undef NG_PARAMETER_ACTION_TYPE_ORIGIN
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

#define NG_PARAMETER_ACTION_FORMAT(Type) \
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::parameter::Type>> \
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::parameter::Type>> {};
NG_PARAMETER_ACTION_FORMAT(ParameterNameChangeAction)
NG_PARAMETER_ACTION_FORMAT(ParameterMoveAction)
NG_PARAMETER_ACTION_FORMAT(ParameterShapeChangeAction)
NG_PARAMETER_ACTION_FORMAT(ParameterAddBindingsAction)
NG_PARAMETER_ACTION_FORMAT(ParameterRemoveBindingsAction)
NG_PARAMETER_ACTION_FORMAT(ParameterChangeBindingsAction)
NG_PARAMETER_ACTION_FORMAT(ParameterChangeBindingsValueAction)
#undef NG_PARAMETER_ACTION_FORMAT

template<bool added> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::parameter::ParameterAddRemoveAction<added>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::parameter::ParameterAddRemoveAction<added>>> {};
template<bool added> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::parameter::ParameterGroupAddRemoveAction<added>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::parameter::ParameterGroupAddRemoveAction<added>>> {};
template<class T> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::parameter::ParameterValueChangeAction<T>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::parameter::ParameterValueChangeAction<T>>> {};
template<class... Args> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::parameter::AbstractParameterChangeBindingsAction<Args...>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::parameter::AbstractParameterChangeBindingsAction<Args...>>> {};

} // namespace nijigenerate::core::translation
