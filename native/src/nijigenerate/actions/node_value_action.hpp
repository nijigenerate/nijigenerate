#pragma once

#include "package.hpp"
#include "depthbone_publishers.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/typed_format_node.hpp>
#include <nijigenerate/core/typed_format_shared_array.hpp>
#include <common/shared_array.hpp>
#include <compat/source_allocator.hpp>
#include <nodes/node.hpp>
#include <math/types.hpp>
#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <memory>
#include <iterator>
#include <limits>
#include <new>
#include <source_location>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>

namespace nijigenerate::actions::node {

namespace detail {
// Allocate before evaluating Source constructor arguments. The callback creates
// the same real class; its failures are rethrown without allocation conversion.
template<class Class, class Construct> std::shared_ptr<Class> createNodeAction(Construct&& construct) {
    nicxlive::core::d_runtime::SourceAllocator<Class> allocator;
    auto* storage = allocator.allocate(1);
    try {
        std::forward<Construct>(construct)(storage);
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    return std::shared_ptr<Class>(storage, [allocator](Class* object) mutable {
        std::destroy_at(object);
        allocator.deallocate(object, 1);
    }, allocator);
}

template<class Class> void prepareNodeActionClass(const char* name) {
    static const bool registered = [name] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Class),
            compat::RetainedString::fromBorrowedView(name));
        nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<Class>();
        return true;
    }();
    (void)registered;
}

// Both template parameters have independent qualified and unqualified raw
// element origins; a grouped single argument is never substituted for either.
template<class T> struct NodeActionTypeArgument {
    static compat::RetainedString get(nicxlive::core::d_runtime::SourceTypeSpelling context =
            nicxlive::core::d_runtime::SourceTypeSpelling::QualifiedElement) {
        return nicxlive::core::d_runtime::ngSourceTypeSpelling<T>(
            context);
    }
};
template<> struct NodeActionTypeArgument<compat::RetainedStringSlice> {
    static compat::RetainedString get(nicxlive::core::d_runtime::SourceTypeSpelling context =
            nicxlive::core::d_runtime::SourceTypeSpelling::QualifiedElement) {
        const auto name = compat::RetainedString::fromStaticLiteral("string");
        nicxlive::core::d_runtime::ngRegisterSourceTypeSpellings<compat::RetainedStringSlice>(
            {name, name, name, name});
        return nicxlive::core::d_runtime::ngSourceTypeSpelling<compat::RetainedStringSlice>(
            context);
    }
};
template<class T> struct NodeActionTypeArgument<compat::Slice<T>> {
    static compat::RetainedString get(nicxlive::core::d_runtime::SourceTypeSpelling context =
            nicxlive::core::d_runtime::SourceTypeSpelling::QualifiedElement) {
        (void)NodeActionTypeArgument<T>::get();
        nicxlive::core::d_runtime::ngRegisterSourceTypeSpellings<compat::Slice<T>>(
            nicxlive::core::d_runtime::ngSourceDynamicArrayTypeSpellings<T>());
        return nicxlive::core::d_runtime::ngSourceTypeSpelling<compat::Slice<T>>(
            context);
    }
};
template<class T> struct NodeActionTypeArgument<nicxlive::core::common::SharedArray<T>> {
    static compat::RetainedString get(nicxlive::core::d_runtime::SourceTypeSpelling context =
            nicxlive::core::d_runtime::SourceTypeSpelling::QualifiedElement) {
        (void)NodeActionTypeArgument<T>::get();
        return nicxlive::core::d_runtime::ngSourceTypeSpelling<nicxlive::core::common::SharedArray<T>>(
            context);
    }
};
template<class Operation> auto nodeActionStringAllocation(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class TNode, class TValue>
compat::RetainedString nodeValueSourceName(bool qualified, bool grouped, bool classInfo = false) {
    using nicxlive::core::d_runtime::SourceTypeSpelling;
    const auto context = classInfo ? SourceTypeSpelling::QualifiedElement : SourceTypeSpelling::UnqualifiedElement;
    const auto node = NodeActionTypeArgument<TNode>::get(context);
    const auto value = NodeActionTypeArgument<TValue>::get(context);
    return nodeActionStringAllocation([&] {
        std::string text;
        if (grouped) text.push_back('(');
        if (qualified) text.append("nijigenerate.actions.node.");
        text.append("NodeValueChangeAction!(");
        text.append(node.view());
        text.append(", ");
        text.append(value.view());
        text.push_back(')');
        if (classInfo) text.append(".NodeValueChangeAction");
        if (grouped) text.push_back(')');
        return compat::RetainedString(std::move(text));
    });
}
template<class Class, class TNode, class TValue> void prepareNodeValueActionClass() {
    static const bool registered = [] {
        const auto name = nodeValueSourceName<TNode, TValue>(true, false, true);
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Class), name);
        nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<Class>();
        return true;
    }();
    (void)registered;
}

// Bridge to the existing private implementation in node.cpp.
bool isTransformActionName(std::string_view name) noexcept;
void notifyDepthBoneNodeTransform(std::shared_ptr<nicxlive::core::nodes::Node> node,
    compat::RetainedStringSlice reason = compat::RetainedString::fromStaticLiteral("Target Transform"));

// Defined in node.cpp: dispatch a native access fault without a C++ null
// dereference. Fault-handler parity remains part of grouped runtime acceptance.
[[noreturn]] void failActionValuePointer();
[[noreturn]] void failActionObjectReference();

template<class Handle> Handle requireActionObject(Handle value) {
    if (!value) failActionObjectReference();
    return value;
}

template<class Array> decltype(auto) actionArrayValue(Array& values, std::size_t index,
    std::source_location source = std::source_location::current()) {
    return compat::ngArrayAt(values, index, source);
}
template<class Array> decltype(auto) actionArrayValue(Array& values, std::size_t index,
    std::size_t sourceLine) {
    const auto length = values.size();
    if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/node.d"), sourceLine);
    return values[index];
}
template<class T> void writeActionValue(std::shared_ptr<T> pointer, const T& value) {
    if (!pointer) failActionValuePointer();
    *pointer = value;
}
template<class Value> Value initialActionValue() {
    if constexpr (std::floating_point<Value>) return std::numeric_limits<Value>::quiet_NaN();
    else if constexpr (std::same_as<Value, char> || std::same_as<Value, char8_t>) return static_cast<Value>(0xff);
    else if constexpr (std::same_as<Value, char16_t> || std::same_as<Value, char32_t> ||
        std::same_as<Value, wchar_t>)
        return static_cast<Value>(0xffff);
    else if constexpr (std::is_enum_v<Value> && requires { core::translation::FormatEnumTraits<Value>::members; }) {
        static_assert(std::size(core::translation::FormatEnumTraits<Value>::members) > 0);
        return core::translation::FormatEnumTraits<Value>::members[0].value;
    } else if constexpr (std::same_as<Value, nicxlive::core::math::Vec2>) {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan};
    } else if constexpr (std::same_as<Value, nicxlive::core::math::Vec3>) {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan, nan};
    } else if constexpr (std::same_as<Value, nicxlive::core::math::Vec4>) {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan, nan, nan};
    } else return Value{};
}
inline void notifyNodeTransform(std::shared_ptr<nicxlive::core::nodes::Node> value) {
    notifyDepthBoneNodeTransform(std::move(value));
}
} // namespace detail

template<class TNode, class TValue> class NodeValueChangeAction;

template<class TNode, class TValue> requires std::derived_from<TNode, nicxlive::core::nodes::Node>
class NodeValueChangeAction<TNode, TValue> : public Action,
    public std::enable_shared_from_this<NodeValueChangeAction<TNode, TValue>> {
protected:
    virtual void prepareSourceClassInfo();
public:
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    using TSelf = NodeValueChangeAction;
    NodeValueChangeAction(const NodeValueChangeAction&) = delete;
    NodeValueChangeAction& operator=(const NodeValueChangeAction&) = delete;
    NodeValueChangeAction(NodeValueChangeAction&&) = delete;
    NodeValueChangeAction& operator=(NodeValueChangeAction&&) = delete;
    std::shared_ptr<TNode> node;
    TValue oldValue = detail::initialActionValue<TValue>();
    TValue newValue = detail::initialActionValue<TValue>();
    // An aliasing pointer owns the actual allocation, not an assumed Node owner.
    // SharedArray::retainPointer supplies this without copying a target value.
    std::shared_ptr<TValue> valuePtr;
    compat::RetainedStringSlice name;

    NodeValueChangeAction(compat::RetainedStringSlice label, std::shared_ptr<TNode> target,
        TValue previous, TValue next, std::shared_ptr<TValue> pointer) {
        prepareSourceClassInfo();
        name = label;
        node = target;
        oldValue = previous;
        newValue = next;
        valuePtr = pointer;
        // Constructor parameters shadow the public fields in the source. A
        // callback can replace those fields without replacing these arguments.
        const auto receiver = detail::requireActionObject(target);
        receiver->notifyChange(target, nicxlive::core::nodes::NotifyReason::AttributeChanged);
        if (detail::isTransformActionName(label.view())) detail::notifyNodeTransform(target);
    }
    void rollback() override {
        auto owner = this->weak_from_this().lock();
        detail::writeActionValue(valuePtr, oldValue);
        notifyCurrent();
    }
    void redo() override {
        auto owner = this->weak_from_this().lock();
        detail::writeActionValue(valuePtr, newValue);
        notifyCurrent();
    }

    ActionText describe() override {
        return describeValue(compat::RetainedString::fromStaticLiteral("%s->%s changed to %s"), true);
    }
    ActionText describeUndo() override {
        return describeValue(compat::RetainedString::fromStaticLiteral("%s->%s changed from %s"), false);
    }
    ActionText getName() override { return name; }
    bool merge(ActionPtr other) override {
        auto owner = this->weak_from_this().lock();
        if (canMerge(other)) {
            auto change = detail::requireActionObject(std::dynamic_pointer_cast<TSelf>(std::move(other)));
            newValue = change->newValue;
            return true;
        }
        return false;
    }
    bool canMerge(ActionPtr other) override {
        auto owner = this->weak_from_this().lock();
        auto change = std::dynamic_pointer_cast<TSelf>(std::move(other));
        if (!change) return false;
        const auto otherUuid = detail::requireActionObject(change->node)->getUUID();
        const auto thisUuid = detail::requireActionObject(node)->getUUID();
        if (otherUuid != thisUuid) return false;
        const auto otherName = change->getName();
        const auto thisName = getName();
        return otherName == thisName;
    }
private:
    ActionText describeValue(compat::RetainedString key, bool forward) {
        auto owner = this->weak_from_this().lock();
        const auto pattern = core::translation::ngTranslateRetained(key);
        const auto receiver = detail::requireActionObject(node);
        const std::array<core::translation::TypedFormatArgument, 3> arguments{
            core::translation::ngFormatArgument(receiver->getName()),
            core::translation::ngFormatArgument(name),
            forward ? core::translation::ngFormatArgument(newValue) : core::translation::ngFormatArgument(oldValue)};
        auto text = core::translation::ngFormatTyped(pattern.view(), arguments);
        return detail::nodeActionStringAllocation([&] {
            return text.empty() ? ActionText{} : ActionText{compat::RetainedString(std::move(text))};
        });
    }
    void notifyCurrent() {
        const auto receiver = detail::requireActionObject(node);
        receiver->notifyChange(receiver, nicxlive::core::nodes::NotifyReason::AttributeChanged);
        // Callbacks may replace the public node/name fields; read them again.
        if (detail::isTransformActionName(name.view())) detail::notifyNodeTransform(node);
    }
};

// The source array-pattern constraint does not impose Node-derived U. Keep that
// distinction: required operations are checked when actual methods instantiate.
template<class Handle, class TValue>
class NodeValueChangeAction<compat::Slice<Handle>, TValue> : public Action,
    public std::enable_shared_from_this<NodeValueChangeAction<compat::Slice<Handle>, TValue>> {
protected:
    virtual void prepareSourceClassInfo();
public:
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    using TSelf = NodeValueChangeAction;
    using TNode = compat::Slice<Handle>;
    using Values = nicxlive::core::common::SharedArray<TValue>;
    using Pointers = compat::Slice<std::shared_ptr<TValue>>;
    NodeValueChangeAction(const NodeValueChangeAction&) = delete;
    NodeValueChangeAction& operator=(const NodeValueChangeAction&) = delete;
    NodeValueChangeAction(NodeValueChangeAction&&) = delete;
    NodeValueChangeAction& operator=(NodeValueChangeAction&&) = delete;
    TNode node;
    Values oldValue;
    Values newValue;
    Pointers valuePtr;
    compat::RetainedStringSlice name;

    NodeValueChangeAction(compat::RetainedStringSlice label, TNode targets, Values previous, Values next,
        Pointers pointers) {
        prepareSourceClassInfo();
        name = label;
        node = targets;
        oldValue = previous;
        newValue = next;
        valuePtr = pointers;
        for (auto target : targets) {
            auto receiver = detail::requireActionObject(target);
            receiver->notifyChange(receiver, nicxlive::core::nodes::NotifyReason::AttributeChanged);
            if (detail::isTransformActionName(label.view())) detail::notifyNodeTransform(target);
        }
    }
    void rollback() override {
        auto owner = this->weak_from_this().lock();
        const auto count = node.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto pointer = detail::actionArrayValue(valuePtr, i, std::size_t{1404});
            const auto values = oldValue;
            const auto& value = detail::actionArrayValue(values, i, std::size_t{1404});
            detail::writeActionValue(pointer, value);
            notifyIndex(i, 1405, 1407);
        }
    }
    void redo() override {
        auto owner = this->weak_from_this().lock();
        const auto count = node.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto pointer = detail::actionArrayValue(valuePtr, i, std::size_t{1416});
            const auto values = newValue;
            const auto& value = detail::actionArrayValue(values, i, std::size_t{1416});
            detail::writeActionValue(pointer, value);
            notifyIndex(i, 1417, 1419);
        }
    }
    ActionText describe() override {
        auto owner = this->weak_from_this().lock();
        auto nodes = node;
        auto values = newValue;
        return describeValues(compat::RetainedString::fromStaticLiteral("%s->%s changed to %s"),
            std::move(nodes), std::move(values));
    }
    ActionText describeUndo() override {
        auto owner = this->weak_from_this().lock();
        auto nodes = node;
        auto values = oldValue;
        return describeValues(compat::RetainedString::fromStaticLiteral("%s->%s changed from %s"),
            std::move(nodes), std::move(values));
    }
    ActionText getName() override { return name; }
    bool merge(ActionPtr other) override {
        auto owner = this->weak_from_this().lock();
        if (canMerge(other)) {
            auto change = detail::requireActionObject(std::dynamic_pointer_cast<TSelf>(std::move(other)));
            newValue = change->newValue;
            return true;
        }
        return false;
    }
    bool canMerge(ActionPtr other) override {
        auto owner = this->weak_from_this().lock();
        auto change = std::dynamic_pointer_cast<TSelf>(std::move(other));
        if (!change) return false;
        const auto otherName = change->getName();
        const auto thisName = getName();
        return otherName == thisName;
    }
private:
    ActionText describeValues(compat::RetainedString key, TNode nodes, Values values) {
        const auto count = std::min(nodes.size(), values.size());
        std::string joined;
        for (std::size_t i = 0; i < count; ++i) {
            if (i) detail::nodeActionStringAllocation([&] { joined.push_back(','); });
            // The source zip/map retains its initial descriptors and supplies
            // each current pair before translating/formatting that element.
            const auto target = detail::actionArrayValue(nodes, i);
            const auto value = detail::actionArrayValue(values, i);
            const auto pattern = core::translation::ngTranslateRetained(key);
            const auto receiver = detail::requireActionObject(target);
            const std::array<core::translation::TypedFormatArgument, 3> arguments{
                core::translation::ngFormatArgument(receiver->getName()),
                core::translation::ngFormatArgument(name),
                core::translation::ngFormatArgument(value)};
            const auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
            detail::nodeActionStringAllocation([&] { joined.append(formatted); });
        }
        if (joined.empty()) return {};
        return detail::nodeActionStringAllocation([&] {
            return ActionText{compat::RetainedString(std::move(joined))};
        });
    }
    void notifyIndex(std::size_t index, std::size_t notifyLine, std::size_t transformLine) {
        auto receiver = detail::requireActionObject(detail::actionArrayValue(node, index, notifyLine));
        receiver->notifyChange(receiver, nicxlive::core::nodes::NotifyReason::AttributeChanged);
        if (detail::isTransformActionName(name.view()))
            detail::notifyNodeTransform(detail::actionArrayValue(node, index, transformLine));
    }
};

} // namespace nijigenerate::actions::node


namespace nicxlive::core::d_runtime {
template<class TNode, class TValue>
struct SourceObjectTypeNames<::nijigenerate::actions::node::NodeValueChangeAction<TNode, TValue>> {
    static RetainedString className() {
        return ::nijigenerate::actions::node::detail::nodeValueSourceName<TNode, TValue>(true, false, true);
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::actions::node::detail::nodeValueSourceName<TNode, TValue>(true, true);
    }
};
template<class TNode, class TValue>
struct SourceTypeSpellings<::nijigenerate::actions::node::NodeValueChangeAction<TNode, TValue>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using ::nijigenerate::actions::node::detail::nodeValueSourceName;
        return {nodeValueSourceName<TNode, TValue>(true, true),
            nodeValueSourceName<TNode, TValue>(false, true),
            nodeValueSourceName<TNode, TValue>(true, false),
            nodeValueSourceName<TNode, TValue>(false, false)};
    }
};
}
namespace nijigenerate::actions::node {
template<class TNode, class TValue> requires std::derived_from<TNode, nicxlive::core::nodes::Node>
void NodeValueChangeAction<TNode, TValue>::prepareSourceClassInfo() {
    detail::prepareNodeValueActionClass<NodeValueChangeAction, TNode, TValue>();
}
template<class Handle, class TValue>
void NodeValueChangeAction<compat::Slice<Handle>, TValue>::prepareSourceClassInfo() {
    detail::prepareNodeValueActionClass<NodeValueChangeAction, compat::Slice<Handle>, TValue>();
}
}

namespace nijigenerate::core::translation {

template<class TNode, class TValue> requires std::derived_from<TNode, nicxlive::core::nodes::Node>
struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::node::NodeValueChangeAction<TNode, TValue>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::node::NodeValueChangeAction<TNode, TValue>>> {};
template<class Handle, class TValue>
struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::node::NodeValueChangeAction<compat::Slice<Handle>, TValue>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::node::NodeValueChangeAction<compat::Slice<Handle>, TValue>>> {};

} // namespace nijigenerate::core::translation
