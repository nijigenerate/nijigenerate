#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nodes/node.hpp>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace nijigenerate::panels::inspector {

using SelectionNode = nicxlive::core::nodes::Node;

// Source classes are reference values; scalar and struct targets are values.
// A ported class with a non-polymorphic native facade can explicitly specialize
// this representation without turning every generic T into a shared_ptr<T>.
template<class T> struct InspectorTargetRepresentation {
    using Type = std::conditional_t<std::is_polymorphic_v<T>, std::shared_ptr<T>, T>;
};
template<class T> using InspectorTargetReference = typename InspectorTargetRepresentation<T>::Type;

template<class T, bool nodeType = std::derived_from<T, SelectionNode>> class Selection;

// Non-Node property targets keep the existing generic retained-slice surface.
template<class T> class Selection<T, false> : public compat::Slice<InspectorTargetReference<T>> {
    using Base = compat::Slice<InspectorTargetReference<T>>;
public:
    using Base::Base;
    Selection() = default;
    Selection(Base values) : Base(std::move(values)) {}
    Selection slice(std::size_t first, std::size_t last) const { return Base::slice(first, last); }
    Selection duplicate() const { return Base::duplicate(); }
};

// Every typed Node view uses the same actual Node-handle slots. Upcasts copy
// only the descriptor, retaining its backing, offset and length without casting
// shared_ptr arrays or creating a second array of handles.
template<class T> class Selection<T, true> {
    template<class, bool> friend class Selection;
    using NodePtr = std::shared_ptr<SelectionNode>;
    using Slots = compat::Slice<NodePtr>;
    struct ViewTag {};
    Slots slots_;
    static void reserveSlots(std::vector<NodePtr>& nodes,std::size_t length) {
        try {
            nodes.reserve(length);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    Selection(Slots slots, ViewTag) : slots_(std::move(slots)) {}
public:
    using value_type = std::shared_ptr<T>;
    Selection() = default;
    Selection(std::initializer_list<value_type> values) {
        std::vector<NodePtr> nodes;
        reserveSlots(nodes,values.size());
        for (const auto& value : values) nodes.push_back(value);
        slots_ = Slots(std::move(nodes));
    }
    explicit Selection(std::vector<value_type> values) {
        std::vector<NodePtr> nodes;
        reserveSlots(nodes,values.size());
        for (auto& value : values) nodes.push_back(std::move(value));
        slots_ = Slots(std::move(nodes));
    }
    Selection(Slots slots) requires std::same_as<T, SelectionNode> : slots_(std::move(slots)) {}
    operator Slots() const requires std::same_as<T, SelectionNode> { return slots_; }

    template<class U> requires std::derived_from<U, T>
    Selection(const Selection<U, true>& other) : slots_(other.slots_) {}

    std::size_t size() const { return slots_.size(); }
    bool empty() const { return slots_.empty(); }
    bool isNull() const { return slots_.isNull(); }
    // Addresses identify the common slot carrier, never a shared_ptr<T> array.
    const NodePtr* data() const { return slots_.data(); }
    value_type get(std::size_t index) const {
        if constexpr (std::same_as<T, SelectionNode>) return slots_.get(index);
        else return std::dynamic_pointer_cast<T>(slots_.get(index));
    }
    // A value read must not turn an 'auto node' into a writable proxy. Mutation
    // uses set explicitly, including a base view writing a compatible subtype.
    const value_type operator[](std::size_t index) const { return get(index); }
    void set(std::size_t index, value_type value) { slots_.set(index, std::move(value)); }
    void append(value_type value) { slots_.append(std::move(value)); }
    void append(Selection values) {
        const auto count = values.size();
        for (std::size_t i = 0; i < count; ++i) append(values.get(i));
    }
    void resize(std::size_t count, value_type initial = {}) { slots_.resize(count, std::move(initial)); }
    void clear() { slots_.clear(); }
    Selection duplicate() const { return Selection(slots_.duplicate(), ViewTag{}); }
    Selection slice(std::size_t first, std::size_t last) const {
        return Selection(slots_.slice(first, last), ViewTag{});
    }

    class Iterator {
        Slots slots_;
        std::size_t index_ = 0;
    public:
        using value_type = std::shared_ptr<T>;
        using difference_type = std::ptrdiff_t;
        using reference = value_type;
        using pointer = void;
        using iterator_category = std::forward_iterator_tag;
        Iterator() = default;
        Iterator(Slots slots, std::size_t index) : slots_(std::move(slots)), index_(index) {}
        value_type operator*() const {
            if constexpr (std::same_as<T, SelectionNode>) return slots_.get(index_);
            else return std::dynamic_pointer_cast<T>(slots_.get(index_));
        }
        Iterator& operator++() { ++index_; return *this; }
        Iterator operator++(int) { auto previous = *this; ++*this; return previous; }
        bool operator==(const Iterator& other) const {
            return slots_.data() == other.slots_.data() && index_ == other.index_;
        }
    };
    Iterator begin() const { return {slots_, 0}; }
    Iterator end() const { return {slots_, slots_.size()}; }
};

// Typed native spelling of the source's cast(Node[]) view. Reference-class
// targets project each live original cell, retaining the actual Node owner.
// Other representations may specialize this protocol with their real source
// cast/ownership provider; no raw layout cast or fabricated Node is used here.
template<class T> struct InspectorNodeProjection {
    static std::size_t size(const Selection<T>& values) { return values.size(); }
    static std::shared_ptr<SelectionNode> get(const Selection<T>& values, std::size_t index)
        requires (std::same_as<InspectorTargetReference<T>, std::shared_ptr<T>> && std::is_polymorphic_v<T>) {
        return std::dynamic_pointer_cast<SelectionNode>(values[index]);
    }
};

} // namespace nijigenerate::panels::inspector
