#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include "selection.hpp"
#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/typed_format_vec2.hpp>
#include <nijigenerate/core/typed_format_vec3.hpp>
#include <nijigenerate/core/typed_format_shared_array.hpp>
#include <nijigenerate/core/typed_format_inspector_enums.hpp>
#include <nijigenerate/project.hpp>
#include <common/object_key_policy.hpp>
#include <compat/source_allocator.hpp>
#include <param/parameter.hpp>
#include <resource.hpp>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::panels::inspector {
using Node = nicxlive::core::nodes::Node;
using Parameter = nicxlive::core::param::Parameter;
using Cursor = compat::Vec2u;
using ModelEditSubMode = project::ModelEditSubMode;

extern thread_local ImVec4 CategoryTextColor;

void incModelModeHeader(const std::shared_ptr<Node>& node);
void incCommonNonEditHeader(const std::shared_ptr<Node>& node);
void ngMultiEditHeader(Selection<Node> nodes);
bool ngInspectorDeformFloatDragVal(compat::RetainedString name, float* result, float adjustSpeed, bool rotation = false);
bool ngInspectorDeformInputFloat(compat::RetainedString name, float* result, float step, float stepFast);
bool ngInspectorDeformSliderFloat(compat::RetainedString name, float* result, float min, float max);
bool ngInspectorDeformDragFloat(compat::RetainedString name, float* result, float speed, float min, float max,
    const char* format);
float incInspectorDeformGetValue(const std::shared_ptr<Node>& node, const std::shared_ptr<Parameter>& parameter,
    compat::RetainedString name, Cursor cursor);
void incInspectorDeformSetValue(const std::shared_ptr<Node>& node, const std::shared_ptr<Parameter>& parameter,
    compat::RetainedString name, Cursor cursor, float value);
void incInspectorDeformSetValue(Selection<Node> nodes, const std::shared_ptr<Parameter>& parameter,
    compat::RetainedString name, Cursor cursor, float value);

class Inspector : public std::enable_shared_from_this<Inspector> {
public:
    virtual ~Inspector() = default;
    virtual void inspect(std::shared_ptr<Parameter> parameter = {}, Cursor cursor = {}) = 0;
    virtual ModelEditSubMode subMode() = 0;
    virtual void subMode(ModelEditSubMode value) = 0;
    // Native interface bridge to the actual implementing Source class's Object.
    virtual compat::RetainedString toString() = 0;
    virtual std::size_t toHash() = 0;
    virtual bool opEquals(std::shared_ptr<Inspector> other) = 0;
    virtual int opCmp(std::shared_ptr<Inspector> other) = 0;
protected:
    std::shared_ptr<Inspector> retainSelf() { return weak_from_this().lock(); }
};

template<class I, class T> std::shared_ptr<I> ngCreateInspector(Selection<T> nodes, ModelEditSubMode mode);
template<class I, class T, class NodesProducer, class ModeProducer>
std::shared_ptr<I> ngCreateInspectorDeferred(NodesProducer&& nodes, ModeProducer&& mode);
namespace detail {
template<class I> std::shared_ptr<I> allocateInspectorReceiver();
}
template<class Owner> struct InspectorTargetAccess;

template<class T> class TypedInspector : public Inspector {
public:
    using Target = T;
    virtual void capture(Selection<T> nodes) = 0;
    virtual bool acceptable(Selection<T> nodes) = 0;
    compat::RetainedString toString() override;
    std::size_t toHash() override;
    bool opEquals(std::shared_ptr<Inspector> other) override;
    int opCmp(std::shared_ptr<Inspector> other) override;
protected:
    virtual void prepareSourceClassInfo();
};

namespace detail {
// Implemented once by common.cpp against the real policy and category providers.
compat::RetainedString formatInspectorText(std::string_view pattern,
    std::span<const core::translation::TypedFormatArgument> arguments);
void inspectWithPolicy(const std::function<void()>& contents);
void inspectSelectionHeader(Selection<Node> targets, ModelEditSubMode mode);
void drawProjectedHeader(std::size_t length,
    const std::function<std::shared_ptr<Node>(std::size_t)>& read);

template<class T> void captureCompatible(Selection<T>& targets, Selection<Node> nodes) {
    targets.resize(0);
    for (auto node : nodes) {
        if (auto target = std::dynamic_pointer_cast<T>(node)) targets.append(std::move(target));
    }
}
template<class T> bool allCompatible(Selection<Node> nodes) {
    for (auto node : nodes) if (!std::dynamic_pointer_cast<T>(node)) return false;
    return true;
}
} // namespace detail

template<ModelEditSubMode targetMode, class T> requires std::derived_from<T, Node>
class BaseInspector;

template<class T> requires std::derived_from<T, Node>
class BaseInspector<ModelEditSubMode::Layout, T> : public TypedInspector<Node> {
    template<class I> friend std::shared_ptr<I> detail::allocateInspectorReceiver();
    template<class I, class U> friend std::shared_ptr<I> ngCreateInspector(Selection<U>, ModelEditSubMode);
    template<class I, class U, class N, class M>
    friend std::shared_ptr<I> ngCreateInspectorDeferred(N&&, M&&);
    template<class Owner> friend struct InspectorTargetAccess;
protected:
    void prepareSourceClassInfo() override;
    Selection<T> targets;
    ModelEditSubMode mode{};
    // ngCreateInspector invokes the logical source constructor after allocating
    // the shared owner, allowing virtual capture to create retained accessors.
    void initialize(Selection<Node> nodes, ModelEditSubMode value) {
        capture(std::move(nodes));
        mode = value;
    }
public:
    using InspectorTargetType = T;
    void inspect(std::shared_ptr<Parameter> = {}, Cursor = {}) override {
        auto owner = this->retainSelf();
        if (mode == ModelEditSubMode::Layout) run();
    }
    virtual void run() = 0;
    Selection<T> getTargets() const { return targets; }
    void capture(Selection<Node> nodes) override {
        auto owner = this->retainSelf();
        detail::captureCompatible<T>(targets, std::move(nodes));
    }
    bool acceptable(Selection<Node> nodes) override { return detail::allCompatible<T>(std::move(nodes)); }
    ModelEditSubMode subMode() override { return mode; }
    void subMode(ModelEditSubMode value) override { mode = value; }
};

template<class T> requires std::derived_from<T, Node>
class BaseInspector<ModelEditSubMode::Deform, T> : public TypedInspector<Node> {
    template<class I> friend std::shared_ptr<I> detail::allocateInspectorReceiver();
    template<class I, class U> friend std::shared_ptr<I> ngCreateInspector(Selection<U>, ModelEditSubMode);
    template<class I, class U, class N, class M>
    friend std::shared_ptr<I> ngCreateInspectorDeferred(N&&, M&&);
    template<class Owner> friend struct InspectorTargetAccess;
protected:
    void prepareSourceClassInfo() override;
    Selection<T> targets;
    ModelEditSubMode mode{};
    void initialize(Selection<Node> nodes, ModelEditSubMode value) {
        capture(std::move(nodes));
        mode = value;
    }
public:
    using InspectorTargetType = T;
    void inspect(std::shared_ptr<Parameter> parameter = {}, Cursor cursor = {}) override {
        auto owner = this->retainSelf();
        if (mode == ModelEditSubMode::Deform) run(std::move(parameter), cursor);
    }
    virtual void run(std::shared_ptr<Parameter> parameter, Cursor cursor) = 0;
    Selection<T> getTargets() const { return targets; }
    void capture(Selection<Node> nodes) override {
        auto owner = this->retainSelf();
        detail::captureCompatible<T>(targets, std::move(nodes));
    }
    bool acceptable(Selection<Node> nodes) override { return detail::allCompatible<T>(std::move(nodes)); }
    ModelEditSubMode subMode() override { return mode; }
    void subMode(ModelEditSubMode value) override { mode = value; }
};

template<class T>
class InspectorHolder : public TypedInspector<T> {
    template<class I> friend std::shared_ptr<I> detail::allocateInspectorReceiver();
    template<class I, class U> friend std::shared_ptr<I> ngCreateInspector(Selection<U>, ModelEditSubMode);
    template<class I, class U, class N, class M>
    friend std::shared_ptr<I> ngCreateInspectorDeferred(N&&, M&&);
    template<class Owner> friend struct InspectorTargetAccess;
protected:
    void prepareSourceClassInfo() override;
    compat::Slice<std::shared_ptr<TypedInspector<T>>> inspectors;
    Selection<T> targets;
    ModelEditSubMode mode{};
    InspectorHolder() = default;
    void initialize(Selection<T> nodes, ModelEditSubMode value) {
        capture(std::move(nodes));
        mode = value;
    }
public:
    using InspectorTargetType = T;
    InspectorHolder(Selection<T> nodes, ModelEditSubMode value) {
        initialize(std::move(nodes), value);
    }
    void inspect(std::shared_ptr<Parameter> parameter = {}, Cursor cursor = {}) override {
        detail::inspectWithPolicy([this, owner = this->retainSelf(), parameter = std::move(parameter), cursor] {
            inspectContents(parameter, cursor);
        });
    }
    virtual void setInspectors(compat::Slice<std::shared_ptr<TypedInspector<T>>> values) {
        auto owner = this->retainSelf();
        inspectors = std::move(values);
        auto snapshot = inspectors;
        for (auto inspector : snapshot) compat::ngRequireObject(inspector)->capture(targets);
    }
    void capture(Selection<T> nodes) override {
        auto owner = this->retainSelf();
        targets.resize(0);
        for (auto node : nodes) if (static_cast<bool>(node)) targets.append(node);
        auto snapshot = inspectors;
        for (auto inspector : snapshot) compat::ngRequireObject(inspector)->capture(targets);
    }
    bool acceptable(Selection<T> nodes) override {
        auto owner = this->retainSelf();
        auto snapshot = inspectors;
        for (auto inspector : snapshot) if (!compat::ngRequireObject(inspector)->acceptable(nodes)) return false;
        return true;
    }
    ModelEditSubMode subMode() override { return mode; }
    void subMode(ModelEditSubMode value) override { mode = value; }
    virtual Selection<T> getTargets() { return targets; }
    template<class I> std::shared_ptr<I> getInspector() {
        for (auto& inspector : inspectors) {
            if (auto result = std::dynamic_pointer_cast<I>(inspector)) return result;
        }
        return {};
    }
    virtual compat::Slice<std::shared_ptr<TypedInspector<T>>> getAll() { return inspectors; }
private:
    void inspectContents(std::shared_ptr<Parameter> parameter, Cursor cursor) {
        const auto currentMode = project::ngModelEditSubMode();
        if constexpr (std::derived_from<T, Node>)
            detail::inspectSelectionHeader(Selection<Node>(targets), currentMode);
        else if (targets.size() > 1) {
            const auto headerTargets = targets;
            detail::drawProjectedHeader(InspectorNodeProjection<T>::size(headerTargets),
                [headerTargets](std::size_t index) { return InspectorNodeProjection<T>::get(headerTargets, index); });
        }
        auto snapshot = inspectors;
        for (auto inspector : snapshot) {
            compat::ngRequireObject(inspector)->subMode(currentMode);
            if (compat::ngRequireObject(inspector)->acceptable(targets))
                compat::ngRequireObject(inspector)->inspect(parameter, cursor);
        }
    }
};

namespace detail {
template<class I> std::shared_ptr<I> allocateInspectorReceiver() {
    static_assert(std::derived_from<I, Inspector> && !std::is_abstract_v<I>);
    std::allocator<I> allocator;
    I* storage;
    try {
        storage = allocator.allocate(1);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    try {
        ::new (static_cast<void*>(storage)) I();
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    std::shared_ptr<I> owner;
    try {
        owner = std::shared_ptr<I>(storage, [](I* object) noexcept {
            std::destroy_at(object);
            std::allocator<I>{}.deallocate(object, 1);
        });
    } catch (const std::bad_alloc&) {
        // The control-block constructor invokes the typed deleter on failure.
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return owner;
}
} // namespace detail

template<class I, class T> std::shared_ptr<I> ngCreateInspector(Selection<T> nodes, ModelEditSubMode mode) {
    auto owner = detail::allocateInspectorReceiver<I>();
    if constexpr (std::is_same_v<T, typename I::Target>) owner->initialize(std::move(nodes), mode);
    else {
        static_assert(std::derived_from<T, typename I::Target>);
        Selection<typename I::Target> captured;
        if constexpr (std::derived_from<T, Node> && std::derived_from<typename I::Target, Node>) {
            captured = Selection<typename I::Target>(nodes);
        } else {
            for (auto node : nodes) captured.append(std::move(node));
        }
        owner->initialize(std::move(captured), mode);
    }
    return owner;
}

template<class I, class T, class NodesProducer, class ModeProducer>
std::shared_ptr<I> ngCreateInspectorDeferred(NodesProducer&& produceNodes, ModeProducer&& produceMode) {
    auto owner = detail::allocateInspectorReceiver<I>();
    Selection<T> nodes = std::invoke(std::forward<NodesProducer>(produceNodes));
    const ModelEditSubMode mode = std::invoke(std::forward<ModeProducer>(produceMode));
    if constexpr (std::is_same_v<T, typename I::Target>) owner->initialize(std::move(nodes), mode);
    else {
        static_assert(std::derived_from<T, typename I::Target>);
        Selection<typename I::Target> captured;
        if constexpr (std::derived_from<T, Node> && std::derived_from<typename I::Target, Node>) {
            captured = Selection<typename I::Target>(nodes);
        } else {
            for (auto node : nodes) captured.append(std::move(node));
        }
        owner->initialize(std::move(captured), mode);
    }
    return owner;
}

// Typed replacement for the source's generated per-property classes. Descriptors
// provide the real getter/setter or batch setter; no property string evaluation.
template<class Owner> struct InspectorTargetAccess {
    static auto get(Owner& owner) {
        // The original mixin reads the lexical field, not a virtual getTargets.
        // Standalone native owners may supply the same storage through their
        // explicit target-access protocol or specialize this adapter.
        if constexpr (requires { owner.targets; }) return owner.targets;
        else return owner.getTargets();
    }
};
template<std::size_t N> struct PropertyId {
    char text[N];
    constexpr PropertyId(const char (&value)[N]) { std::copy_n(value, N, text); }
    constexpr std::string_view view() const { return {text, N - 1}; }
};

template<class Descriptor, PropertyId prefix>
compat::RetainedString ngInspectorPropertyLabel() {
    struct Label {
        char text[prefix.view().size() + Descriptor::id.view().size() + 1]{};
    };
    static constexpr auto label = [] {
        Label result;
        std::size_t position = 0;
        for (const char value : prefix.view()) result.text[position++] = value;
        for (const char value : Descriptor::id.view()) result.text[position++] = value;
        return result;
    }();
    return compat::RetainedString::fromStaticLiteral(label.text);
}
template<class Member> struct InspectorMemberType;
template<class Target, class Value> struct InspectorMemberType<Value Target::*> { using Owner = Target; };
template<class Owner, class DeclaringTarget, class = void> struct InspectorMemberTarget {
    using Type = DeclaringTarget;
};
template<class Owner, class DeclaringTarget>
struct InspectorMemberTarget<Owner, DeclaringTarget, std::void_t<typename Owner::InspectorTargetType>> {
    using Type = typename Owner::InspectorTargetType;
};
template<class FieldValue, PropertyId name, auto member,
    class SourceTarget = typename InspectorMemberType<std::remove_cv_t<decltype(member)>>::Owner>
    requires std::is_member_object_pointer_v<decltype(member)>
struct InspectorMemberAttributeDescriptor {
    using Target = SourceTarget;
    using Value = FieldValue;
    inline static constexpr auto id = name;
    static Value readTarget(const InspectorTargetReference<Target>& target) {
        if constexpr (std::same_as<InspectorTargetReference<Target>, std::shared_ptr<Target>>)
            return Value(std::invoke(member, compat::ngRequireObject(target)));
        else return Value(std::invoke(member, target));
    }
    static void writeTarget(InspectorTargetReference<Target>& target, const Value& value) {
        const auto assign = [&](auto& slot) {
            if constexpr (std::same_as<std::remove_cvref_t<decltype(slot)>, nicxlive::core::common::NullableString>
                && requires { value.toNullableString(); }) slot = value.toNullableString();
            else slot = value;
        };
        if constexpr (std::same_as<InspectorTargetReference<Target>, std::shared_ptr<Target>>)
            assign(std::invoke(member, compat::ngRequireObject(target)));
        else assign(std::invoke(member, target));
    }
};

template<class V> struct InspectorValueInitialization {};
template<class V> struct InspectorStaticArray : std::false_type {};
template<class V, std::size_t N> struct InspectorStaticArray<std::array<V, N>> : std::true_type {
    using Element = V;
    static constexpr std::size_t size = N;
};
template<class V> struct InspectorSequence : InspectorStaticArray<V> {};
template<class V> struct InspectorSequence<compat::Slice<V>> : std::true_type {};

template<class V> struct InspectorSequence<compat::ConstSlice<V>> : std::true_type {};
template<class V> struct InspectorSequence<nicxlive::core::common::SharedArray<V>> : std::true_type {};
template<class V> struct InspectorResourceValue : std::false_type {};
template<class V> struct InspectorResourceValue<std::shared_ptr<V>>
    : std::bool_constant<std::derived_from<V, nicxlive::core::Resource>> {};
template<class V> struct InspectorHandleValue : std::false_type {};
template<class V> struct InspectorHandleValue<std::shared_ptr<V>> : std::true_type {};
template<class V> struct InspectorComparisonSnapshot : std::bool_constant<
    std::is_scalar_v<V> || InspectorHandleValue<V>::value ||
    (InspectorSequence<V>::value && !InspectorStaticArray<V>::value) ||
    std::is_same_v<V, compat::RetainedString> || std::is_same_v<V, compat::RetainedStringSlice> ||
    std::is_same_v<V, std::string> || std::is_same_v<V, std::string_view>> {};

template<class V> V ngInspectorInitialValue() {
    if constexpr (requires { InspectorValueInitialization<V>::get(); })
        return InspectorValueInitialization<V>::get();
    else if constexpr (std::is_floating_point_v<V>) return std::numeric_limits<V>::quiet_NaN();
    else if constexpr (std::is_same_v<V, char> || std::is_same_v<V, char8_t>) return static_cast<V>(0xff);
    else if constexpr (std::is_same_v<V, char16_t>) return static_cast<V>(0xffff);
    else if constexpr (std::is_same_v<V, char32_t>) return static_cast<V>(0xffff);
    else if constexpr (std::is_same_v<V, nicxlive::core::math::Vec2>) {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan};
    } else if constexpr (std::is_same_v<V, nicxlive::core::math::Vec3>) {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan, nan};
    } else if constexpr (std::is_same_v<V, nicxlive::core::math::Vec4>) {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan, nan, nan};
    }
    else if constexpr (InspectorStaticArray<V>::value) {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return V{(static_cast<void>(I), ngInspectorInitialValue<typename InspectorStaticArray<V>::Element>())...};
        }(std::make_index_sequence<InspectorStaticArray<V>::size>{});
    } else return V{};
}

template<class V> bool ngInspectorValuesDiffer(const V& left, const V& right) {
    if constexpr (InspectorSequence<V>::value) {
        if (left.size() != right.size()) return true;
        for (std::size_t i = 0; i < left.size(); ++i)
            if (ngInspectorValuesDiffer(left[i], right[i])) return true;
        return false;
    } else if constexpr (InspectorResourceValue<V>::value)
        return !nicxlive::core::ngResourceObjectEquals(left, right);
    else if constexpr (InspectorHandleValue<V>::value) {
        if constexpr (std::is_polymorphic_v<typename V::element_type> && requires(V object) {
            { object->opEquals(object) } -> std::convertible_to<bool>;
        }) return !nicxlive::core::common::ngClassObjectEquals(left, right);
        else return left != right;
    } else if constexpr (std::is_same_v<V, nicxlive::core::math::Vec2>)
        return left.x != right.x || left.y != right.y;
    else if constexpr (std::is_same_v<V, nicxlive::core::math::Vec3>)
        return left.x != right.x || left.y != right.y || left.z != right.z;
    else if constexpr (std::is_same_v<V, nicxlive::core::math::Vec4>)
        return left.x != right.x || left.y != right.y || left.z != right.z || left.w != right.w;
    else return left != right;
}

template<class Owner, class Descriptor, bool deformation = false> class InspectorProperty;
template<class Descriptor> struct InspectorFloatDeformationDescriptor {
    using Target = typename Descriptor::Target;
    using Value = float;
    inline static constexpr auto id = Descriptor::id;
    template<class Owner> static float read(Owner& owner, const InspectorTargetReference<Target>& target)
        requires requires { Descriptor::read(owner, target); } {
        return Descriptor::read(owner, target);
    }
    template<class State> static float readState(State& state, const InspectorTargetReference<Target>& target)
        requires requires { Descriptor::readState(state, target); } {
        return Descriptor::readState(state, target);
    }
    template<class Owner> static void writeBatch(Owner& owner, Selection<Target> nodes, float value)
        requires requires { Descriptor::writeBatch(owner, nodes, value); } {
        Descriptor::writeBatch(owner, std::move(nodes), value);
    }
    template<class State> static void writeState(State& state, Selection<Target> nodes, float value)
        requires requires { Descriptor::writeState(state, nodes, value); } {
        Descriptor::writeState(state, std::move(nodes), value);
    }
};
template<class Descriptor, class = void> struct InspectorDeformationType {
    using Type = InspectorFloatDeformationDescriptor<Descriptor>;
};
template<class Descriptor> struct InspectorDeformationType<Descriptor, std::void_t<typename Descriptor::Value>> {
    using Type = Descriptor;
};
template<class T> struct InspectorSharedValueData {
    InspectorSharedValueData() = default;
    explicit InspectorSharedValueData(T initial) : value(std::move(initial)) {}
    template<class Initialize>
    InspectorSharedValueData(std::in_place_t, Initialize&& initialize)
        : value(std::invoke(std::forward<Initialize>(initialize))) {}
    bool isShared = false;
    T value = ngInspectorInitialValue<T>();
};

// A cached State's self-parent edge is held by its escaped State/outer lease,
// rather than a permanent Owner -> State -> Owner cycle. A copied parent value
// is a normal strong handle; a different explicitly assigned parent is strong.
template<class Owner> class InspectorParent {
public:
    InspectorParent(std::shared_ptr<Owner> outer, std::shared_ptr<Owner> value) : outer_(outer) {
        operator=(std::move(value));
    }
    InspectorParent(const InspectorParent& other) : value_(other.get()) {}
    InspectorParent& operator=(const InspectorParent& other) { return operator=(other.get()); }
    InspectorParent& operator=(std::shared_ptr<Owner> value) {
        const auto outer = outer_.lock();
        self_ = outer && value == outer;
        value_ = self_ ? std::shared_ptr<Owner>{} : std::move(value);
        return *this;
    }
    InspectorParent& operator=(std::nullptr_t) { return operator=(std::shared_ptr<Owner>{}); }
    std::shared_ptr<Owner> get() const { return self_ ? outer_.lock() : value_; }
    operator std::shared_ptr<Owner>() const { return get(); }
    explicit operator bool() const { return bool(get()); }
    std::shared_ptr<Owner> operator->() const { return compat::ngRequireObject(get()); }
    friend bool operator==(const InspectorParent& left, const std::shared_ptr<Owner>& right) {
        return left.get() == right;
    }
    friend bool operator==(const InspectorParent& left, const InspectorParent& right) {
        return left.get() == right.get();
    }
    friend bool operator==(const InspectorParent& left, std::nullptr_t) { return !left; }
private:
    std::weak_ptr<Owner> outer_;
    std::shared_ptr<Owner> value_;
    bool self_ = false;
};

template<class State, class Descriptor, bool deformation> class InspectorStateSetter;
template<class State, class Descriptor> class InspectorStateSetter<State, Descriptor, false> {
public:
    virtual ~InspectorStateSetter() = default;
    virtual void set(InspectorTargetReference<typename Descriptor::Target> node, typename Descriptor::Value value) {
        static_cast<State&>(*this).writeAttribute(node, value);
    }
};
template<class State, class Descriptor> class InspectorStateSetter<State, Descriptor, true> {
public:
    virtual ~InspectorStateSetter() = default;
    virtual void set(Selection<typename Descriptor::Target> nodes, typename Descriptor::Value value) {
        static_cast<State&>(*this).writeDeformation(nodes, value);
    }
};

template<class Owner, class State> struct InspectorStateLifetime {
    std::shared_ptr<Owner> outer;
    std::shared_ptr<State> state;
};
template<class Owner, class State> std::shared_ptr<State> ngRetainInspectorState(
    std::shared_ptr<Owner> outer, std::shared_ptr<State> state) {
    if (!state) return {};
    InspectorStateLifetime<Owner, State> retained{std::move(outer), std::move(state)};
    auto* pointer = retained.state.get();
    using Lifetime = InspectorStateLifetime<Owner, State>;
    auto lifetime = std::allocate_shared<Lifetime>(nicxlive::core::d_runtime::SourceAllocator<Lifetime>{}, retained);
    return std::shared_ptr<State>(std::move(lifetime), pointer);
}
// The first argument is the source nested class's implicit lexical outer; the
// second is its independent, explicitly supplied public constructor parameter.
template<class State, class Owner, class... Args> std::shared_ptr<State> ngCreateInspectorState(
    std::shared_ptr<Owner> outer, std::shared_ptr<Owner> parent, Args&&... args) {
    auto state = std::allocate_shared<State>(nicxlive::core::d_runtime::SourceAllocator<State>{},
        outer, std::move(parent), std::forward<Args>(args)...);
    return ngRetainInspectorState(outer, state);
}
template<class Owner> class InspectorLexicalLifetime {
protected:
    explicit InspectorLexicalLifetime(const std::shared_ptr<Owner>& owner) : lexicalOuter_(owner) {}
    std::weak_ptr<Owner> lexicalOuter_;
    std::shared_ptr<Owner> ownedOuter_;
};

template<class Owner, class Descriptor, bool deformation> class InspectorSharedValue
    : private InspectorLexicalLifetime<Owner>,
      public InspectorStateSetter<InspectorSharedValue<Owner, Descriptor, deformation>, Descriptor, deformation>,
      public InspectorSharedValueData<typename Descriptor::Value>,
      public std::enable_shared_from_this<InspectorSharedValue<Owner, Descriptor, deformation>> {
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    struct CachedConstruction {};
public:
    using Value = typename Descriptor::Value;
    using Target = typename Descriptor::Target;
    using TargetReference = InspectorTargetReference<Target>;
    using Data = InspectorSharedValueData<Value>;
    using Data::isShared;
    using Data::value;
    InspectorSharedValue(std::shared_ptr<Owner> outer, std::shared_ptr<Owner> parentValue)
        : InspectorSharedValue(CachedConstruction{}, outer, std::move(parentValue)) {
        // Independently constructed source nested objects own their lexical
        // outer even when they are not obtained through a property getter.
        this->ownedOuter_ = std::move(outer);
    }
    virtual ~InspectorSharedValue() = default;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<InspectorSharedValue> other);
    virtual int opCmp(std::shared_ptr<InspectorSharedValue> other);
    InspectorSharedValue(const InspectorSharedValue&) = delete;
    InspectorSharedValue& operator=(const InspectorSharedValue&) = delete;
    InspectorSharedValue(InspectorSharedValue&&) = delete;
    InspectorSharedValue& operator=(InspectorSharedValue&&) = delete;
    InspectorParent<Owner> parent;

    std::shared_ptr<Owner> outer() const { return this->lexicalOuter_.lock(); }
    virtual Value get(TargetReference node) {
        if constexpr (requires { Descriptor::readState(*this, node); })
            return Descriptor::readState(*this, node);
        else if constexpr (!deformation && requires { Descriptor::readTarget(node); })
            return Descriptor::readTarget(node);
        else if constexpr (deformation)
            return Descriptor::read(*compat::ngRequireObject(parent.get()), node);
        else return Descriptor::read(*compat::ngRequireObject(outer()), node);
    }
    virtual bool capture() {
        if constexpr (deformation) if (!compat::ngRequireObject(outer())->currParam) return false;
        if (InspectorTargetAccess<Owner>::get(*compat::ngRequireObject(outer())).empty()) {
            isShared = false; return false;
        }
        isShared = true;
        value = get(InspectorTargetAccess<Owner>::get(*compat::ngRequireObject(parent.get()))[0]);
        auto remaining = InspectorTargetAccess<Owner>::get(*compat::ngRequireObject(parent.get()));
        remaining = remaining.slice(1, remaining.size());
        for (auto node : remaining) {
            const auto differs = [&](const Value& previous, const Value& next) {
                if constexpr (requires { Descriptor::valuesDiffer(previous, next); })
                    return Descriptor::valuesDiffer(previous, next);
                else return ngInspectorValuesDiffer(previous, next);
            };
            bool different;
            if constexpr (InspectorComparisonSnapshot<Value>::value) {
                // Scalar/reference and dynamic-array operands are values.
                const Value previous = value;
                const Value next = get(node);
                different = differs(previous, next);
            } else {
                // A struct receiver or static array keeps its actual storage
                // while the right operand's virtual getter is evaluated.
                const Value& previous = value;
                const Value next = get(node);
                different = differs(previous, next);
            }
            if (different) { isShared = false; return false; }
        }
        return true;
    }
    virtual void apply() {
        if constexpr (deformation) {
            if (!compat::ngRequireObject(outer())->currParam) return;
            auto nodes = InspectorTargetAccess<Owner>::get(*compat::ngRequireObject(parent.get()));
            const Value next = value;
            this->set(std::move(nodes), next);
        } else {
            const auto nodes = InspectorTargetAccess<Owner>::get(*compat::ngRequireObject(parent.get()));
            for (auto node : nodes) this->set(node, value);
        }
    }
private:
    friend class InspectorProperty<Owner, Descriptor, deformation>;
    friend class InspectorStateSetter<InspectorSharedValue, Descriptor, deformation>;
    using std::enable_shared_from_this<InspectorSharedValue>::shared_from_this;
    using std::enable_shared_from_this<InspectorSharedValue>::weak_from_this;
    InspectorSharedValue(CachedConstruction, std::shared_ptr<Owner> outer, std::shared_ptr<Owner> parentValue)
        : InspectorLexicalLifetime<Owner>(outer), Data(std::in_place, [] {
            if constexpr (requires { Descriptor::initialValue(); }) return Descriptor::initialValue();
            else return ngInspectorInitialValue<Value>();
        }), parent(outer, std::move(parentValue)) {}
protected:
    virtual void prepareSourceClassInfo();
private:
    void writeAttribute(TargetReference& node, Value& next) requires (!deformation) {
        if constexpr (requires { Descriptor::writeState(*this, node, next); })
            Descriptor::writeState(*this, node, next);
        else if constexpr (requires { Descriptor::writeTarget(node, next); })
            Descriptor::writeTarget(node, next);
        else Descriptor::write(*compat::ngRequireObject(outer()), node, next);
        isShared = true;
        if constexpr (std::derived_from<Target, Node>) compat::ngRequireObject(node)->notifyChange(node);
    }
    void writeDeformation(Selection<Target>& nodes, Value& next) requires deformation {
        if constexpr (requires { Descriptor::writeState(*this, nodes, next); })
            Descriptor::writeState(*this, nodes, next);
        else Descriptor::writeBatch(*compat::ngRequireObject(parent.get()), nodes, next);
        isShared = true;
    }
};

template<class Owner, class Descriptor> concept AttributeDescriptor = requires {
    { Descriptor::id.view() } -> std::same_as<std::string_view>;
} && (requires(Owner& owner, InspectorTargetReference<typename Descriptor::Target> target) {
    { Descriptor::read(owner, target) } -> std::same_as<typename Descriptor::Value>;
} || requires(InspectorSharedValue<Owner, Descriptor, false>& state,
    InspectorTargetReference<typename Descriptor::Target> target) {
    { Descriptor::readState(state, target) } -> std::same_as<typename Descriptor::Value>;
} || requires(InspectorTargetReference<typename Descriptor::Target> target) {
    { Descriptor::readTarget(target) } -> std::same_as<typename Descriptor::Value>;
}) && (requires(Owner& owner, InspectorTargetReference<typename Descriptor::Target> target,
    typename Descriptor::Value value) {
    { Descriptor::write(owner, target, value) } -> std::same_as<void>;
} || requires(InspectorSharedValue<Owner, Descriptor, false>& state,
    InspectorTargetReference<typename Descriptor::Target> target, typename Descriptor::Value value) {
    { Descriptor::writeState(state, target, value) } -> std::same_as<void>;
} || requires(InspectorTargetReference<typename Descriptor::Target> target, typename Descriptor::Value value) {
    { Descriptor::writeTarget(target, value) } -> std::same_as<void>;
});
template<class Owner, class Descriptor> concept DeformationDescriptor = requires {
    { Descriptor::id.view() } -> std::same_as<std::string_view>;
} && (requires(Owner& owner, InspectorTargetReference<typename Descriptor::Target> target) {
    { Descriptor::read(owner, target) } -> std::same_as<typename Descriptor::Value>;
} || requires(InspectorSharedValue<Owner, Descriptor, true>& state,
    InspectorTargetReference<typename Descriptor::Target> target) {
    { Descriptor::readState(state, target) } -> std::same_as<typename Descriptor::Value>;
}) && (requires(Owner& owner, Selection<typename Descriptor::Target> targets,
    typename Descriptor::Value value) {
    { Descriptor::writeBatch(owner, targets, value) } -> std::same_as<void>;
} || requires(InspectorSharedValue<Owner, Descriptor, true>& state,
    Selection<typename Descriptor::Target> targets, typename Descriptor::Value value) {
    { Descriptor::writeState(state, targets, value) } -> std::same_as<void>;
});

template<class Owner, class Descriptor, bool deformation>
class InspectorProperty {
public:
    static_assert((!deformation && AttributeDescriptor<Owner, Descriptor>) ||
        (deformation && DeformationDescriptor<Owner, Descriptor>), "Invalid typed inspector descriptor");
    using State = InspectorSharedValue<Owner, Descriptor, deformation>;
    std::shared_ptr<State> get(std::shared_ptr<Owner> parent) {
        if (!value_) value_ = std::allocate_shared<State>(nicxlive::core::d_runtime::SourceAllocator<State>{},
            typename State::CachedConstruction{}, parent, parent);
        const auto state = value_;
        return ngRetainInspectorState(state->outer(), state);
    }
    std::shared_ptr<State> operator()(std::shared_ptr<Owner> owner) { return get(std::move(owner)); }
    // Native spelling of the source's public nullable _name cache field.
    // Normalize a self-outer alias to its genuine State allocation so the
    // cache does not retain its own Owner through an escaped handle's lease.
    void replace(std::shared_ptr<Owner> owner, std::shared_ptr<State> state) {
        if (!state) { value_.reset(); return; }
        if (state->outer() == owner) value_ = state->shared_from_this();
        else value_ = ngRetainInspectorState(state->outer(), state->shared_from_this());
    }
    std::shared_ptr<State> cached([[maybe_unused]] std::shared_ptr<Owner> owner) const {
        // The by-value owner pins the cache's containing object through a
        // possible lease-allocation failure; the returned lease uses the
        // State's own immutable lexical outer, including foreign cache values.
        if (!value_) return {};
        const auto state = value_;
        return ngRetainInspectorState(state->outer(), state);
    }
private:
    std::shared_ptr<State> value_;
};

namespace detail {
bool beginSharedCombo(compat::RetainedStringSlice id);
bool selectSharedValue(compat::RetainedStringSlice label);
void endSharedCombo();
void beginSharedGroup();
void endSharedGroup();
void setSharedWidth();
float sharedWidth();
void setSharedWidth(float width);
void pushInspectorActionGroup();
void popInspectorActionGroup();
bool editInspectorColor(float (&rgb)[3]);
float inspectorTextWidth(compat::RetainedStringSlice value);
void drawInspectorColorRow(std::shared_ptr<Node> node, const float (&rgb)[3], float maxWidth, bool& selected);
bool beginInspectorColorCombo();
} // namespace detail

// Owner supplies getTargets(), capture(), and shared_from_this(). Keeping this
// state on each concrete Inspector preserves lazy identity across frames.
template<class Handle> Handle ngInspectorWritableState(Handle state) {
    if (!state) compat::ngFailNullAccess(true);
    return state;
}
template<class Owner> class MultiEdit {
public:
    virtual ~MultiEdit() = default;
    template<class Value> using SharedValue = InspectorSharedValueData<Value>;
    template<class Descriptor> using Attribute = InspectorProperty<Owner, Descriptor, false>;
    template<class Descriptor> using Deformation =
        InspectorProperty<Owner, typename InspectorDeformationType<Descriptor>::Type, true>;
    template<class Descriptor> static Attribute<Descriptor> attribute() { return {}; }
    template<class Value, PropertyId name, auto member> static auto attribute() {
        using DeclaringTarget = typename InspectorMemberType<std::remove_cv_t<decltype(member)>>::Owner;
        using Target = typename InspectorMemberTarget<Owner, DeclaringTarget>::Type;
        return Attribute<InspectorMemberAttributeDescriptor<Value, name, member, Target>>{};
    }
    template<class Descriptor> static Deformation<Descriptor> deformation() { return {}; }
    std::shared_ptr<Parameter> currParam;
    Cursor currCursor{};

    template<class Descriptor, class StateAccessor, class Edit> requires std::invocable<StateAccessor&>
    bool shared(StateAccessor&& state, Edit&& edit) {
        auto& owner = static_cast<Owner&>(*this);
        const auto keepAlive = owner.shared_from_this();
        const compat::SharedDelegate<bool()> retainedEdit(std::forward<Edit>(edit));
        if (InspectorTargetAccess<Owner>::get(owner).size() == 1) return retainedEdit();
        bool changed = false;
        detail::beginSharedGroup();
        const float width = detail::sharedWidth();
        if (!compat::ngRequireObject(std::invoke(state))->isShared) {
            detail::setSharedWidth(width);
            if (detail::beginSharedCombo(ngInspectorPropertyLabel<Descriptor, "##combo_">())) {
                for (auto target : InspectorTargetAccess<Owner>::get(owner)) {
                    const auto next = compat::ngRequireObject(std::invoke(state))->get(target);
                    const std::array<core::translation::TypedFormatArgument, 2> arguments{
                        core::translation::ngFormatArgument(compat::ngRequireObject(target)->getName()),
                        core::translation::ngFormatArgument(next)};
                    const auto label = detail::formatInspectorText("%s: %s", arguments);
                    if (detail::selectSharedValue(label)) {
                        ngInspectorWritableState(std::invoke(state))->value = next;
                        changed = true;
                        break;
                    }
                }
                detail::endSharedCombo();
            }
        } else {
            detail::setSharedWidth(width);
            changed |= retainedEdit();
        }
        detail::endSharedGroup();
        return changed;
    }
    virtual void updateDeform(std::shared_ptr<Parameter> parameter, Cursor cursor) {
        auto& owner = static_cast<Owner&>(*this);
        const auto keepAlive = owner.shared_from_this();
        if (!nicxlive::core::ngResourceObjectEquals(currParam, parameter)
                || currCursor.x != cursor.x || currCursor.y != cursor.y) {
            currParam = parameter;
            currCursor = cursor;
            owner.capture(InspectorTargetAccess<Owner>::get(owner));
        }
    }
    template<class Descriptor, class StateAccessor, class Edit> requires std::invocable<StateAccessor&>
    void deform(StateAccessor&& state, Edit&& edit) {
        const auto keepAlive = static_cast<Owner&>(*this).shared_from_this();
        using ValuePointer = decltype(&std::declval<std::invoke_result_t<StateAccessor&>>()->value);
        const compat::SharedDelegate<bool(compat::RetainedString, ValuePointer)> retainedEdit(
            std::forward<Edit>(edit));
        if (shared<Descriptor>(state, [&] {
            const auto id = ngInspectorPropertyLabel<Descriptor, "###">();
            const auto current = ngInspectorWritableState(std::invoke(state));
            return retainedEdit(id, &current->value);
        })) compat::ngRequireObject(std::invoke(state))->apply();
    }
    template<class R, class G, class B>
        requires (std::invocable<R&> && std::invocable<G&> && std::invocable<B&>)
    void deformRGB(R&& red, G&& green, B&& blue) {
        auto& owner = static_cast<Owner&>(*this);
        const auto keepAlive = owner.shared_from_this();
        if (InspectorTargetAccess<Owner>::get(owner).size() < 2 || (compat::ngRequireObject(std::invoke(red))->isShared
                && compat::ngRequireObject(std::invoke(green))->isShared
                && compat::ngRequireObject(std::invoke(blue))->isShared)) {
            float rgb[3] = {compat::ngRequireObject(std::invoke(red))->value,
                compat::ngRequireObject(std::invoke(green))->value, compat::ngRequireObject(std::invoke(blue))->value};
            if (detail::editInspectorColor(rgb)) applyRGB(red, green, blue, rgb);
        } else if (!compat::ngRequireObject(std::invoke(red))->isShared
                || !compat::ngRequireObject(std::invoke(green))->isShared
                || !compat::ngRequireObject(std::invoke(blue))->isShared) {
            float width = 0;
            for (auto node : InspectorTargetAccess<Owner>::get(owner)) {
                const compat::RetainedString name(compat::ngRequireObject(node)->getName());
                width = std::max(width, detail::inspectorTextWidth(name));
            }
            detail::setSharedWidth();
            if (detail::beginInspectorColorCombo()) {
                for (auto node : InspectorTargetAccess<Owner>::get(owner)) {
                    float rgb[3] = {compat::ngRequireObject(std::invoke(red))->get(node),
                        compat::ngRequireObject(std::invoke(green))->get(node),
                        compat::ngRequireObject(std::invoke(blue))->get(node)};
                    bool selected = false;
                    detail::drawInspectorColorRow(node, rgb, width, selected);
                    if (selected) { applyRGB(red, green, blue, rgb); break; }
                }
                detail::endSharedCombo();
            }
        }
    }
private:
    template<class R, class G, class B> void applyRGB(R& red, G& green, B& blue, const float (&rgb)[3]) {
        detail::pushInspectorActionGroup();
        ngInspectorWritableState(std::invoke(red))->value = rgb[0];
        compat::ngRequireObject(std::invoke(red))->apply();
        ngInspectorWritableState(std::invoke(green))->value = rgb[1];
        compat::ngRequireObject(std::invoke(green))->apply();
        ngInspectorWritableState(std::invoke(blue))->value = rgb[2];
        compat::ngRequireObject(std::invoke(blue))->apply();
        detail::popInspectorActionGroup();
    }
};
} // namespace nijigenerate::panels::inspector

#include "common_object.hpp"
