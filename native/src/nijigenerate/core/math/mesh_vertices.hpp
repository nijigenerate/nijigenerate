#pragma once
#include "mesh_declarations.hpp"
#include "nijigenerate/compat/slice.hpp"
#include <math/types.hpp>
#include <cstdint>
#include <limits>
#include <memory>
#include <initializer_list>
#include <utility>

namespace nijigenerate::core::math::mesh {
namespace detail { struct MeshVertexStorage; }
class MeshVertexSlot;
class MeshVertexHandle {
public:
    MeshVertexHandle() = default;
    MeshVertexHandle(const MeshVertexHandle&) = default;
    MeshVertexHandle& operator=(const MeshVertexHandle&) = default;
    MeshVertexHandle(MeshVertexHandle&& other) noexcept
        : pointer_(other.pointer_), owner_(other.owner_) {}
    MeshVertexHandle& operator=(MeshVertexHandle&& other) noexcept {
        pointer_ = other.pointer_;
        owner_ = other.owner_;
        return *this;
    }
    MeshVertex* get() const noexcept { return pointer_; }
    MeshVertex* operator->() const noexcept { return pointer_; }
    MeshVertex& operator*() const { return *pointer_; }
    explicit operator bool() const noexcept { return pointer_ != nullptr; }
private:
    friend MeshVertexHandle ngRetainMeshVertex(MeshVertex*);
    friend MeshVertexHandle ngTryRetainMeshVertex(MeshVertex*);
    friend MeshVertexHandle ngCreateMeshVertex(nicxlive::core::math::Vec2, std::uint32_t, std::size_t);
    MeshVertex* pointer_ = nullptr;
    std::shared_ptr<detail::MeshVertexStorage> owner_;
};

// Copies retain the pointer backing and actual known graph owners. Unmanaged raw
// ingress is borrowed from its caller. Embedded graph edges use weak ownership;
// copying that descriptor outward promotes its real graph owner.
class MeshVertexArray {
public:
    MeshVertexArray() = default;
    MeshVertexArray(std::initializer_list<MeshVertex*> values) { for (auto* value : values) append(value); }
    MeshVertexArray(const MeshVertexArray&);
    MeshVertexArray(MeshVertexArray&&);
    MeshVertexArray& operator=(const MeshVertexArray&);
    MeshVertexArray& operator=(MeshVertexArray&&);
    std::size_t size() const { return values_.size(); }
    bool empty() const { return values_.empty(); }
    bool isNull() const { return values_.isNull(); }
    explicit operator bool() const { return !isNull(); }
    MeshVertex* operator[](std::size_t index) const { return values_[index]; }
    MeshVertex* get(std::size_t index) const { return values_[index]; }
    MeshVertexSlot mutableSlot(std::size_t index);
    MeshVertex* const* begin() const { return values_.begin(); }
    MeshVertex* const* end() const { return values_.end(); }
    MeshVertex* const* data() const { return values_.data(); }
    void append(MeshVertex* value);
    void append(const MeshVertexHandle& value) { append(value.get()); }
    void append(const MeshVertexArray& values);
    void set(std::size_t index, MeshVertex* value);
    void resize(std::size_t size);
    void clear();
    MeshVertexArray duplicate() const;
    MeshVertexArray slice(std::size_t first, std::size_t last) const;
private:
    friend struct MeshVertex;
    friend struct detail::MeshVertexStorage;
    friend MeshVertexHandle ngCreateMeshVertex(nicxlive::core::math::Vec2, std::uint32_t, std::size_t);
    explicit MeshVertexArray(bool embedded) : embedded_(embedded) {}
    std::shared_ptr<detail::MeshVertexStorage> owner() const;
    void retain(MeshVertex*);
    void assignOwner(std::shared_ptr<detail::MeshVertexStorage>);
    compat::Slice<MeshVertex*> values_;
    std::shared_ptr<detail::MeshVertexStorage> retained_;
    std::weak_ptr<detail::MeshVertexStorage> weak_;
    bool embedded_ = false;
};

// A mutable reference to the original pointer cell, with its backing and actual
// graph owner retained. Assignment updates the cell directly, not a temporary.
class MeshVertexSlot {
public:
    MeshVertexSlot(const MeshVertexSlot&) = default;
    MeshVertexSlot(MeshVertexSlot&&) = default;
    MeshVertex* get() const { return array_.get(index_); }
    operator MeshVertex*() const { return get(); }
    const void* identity() const { return array_.data() + index_; }
    MeshVertexSlot& operator=(MeshVertex* value) {
        array_.set(index_, value);
        return *this;
    }
    MeshVertexSlot& operator=(const MeshVertexSlot& other) { return *this = other.get(); }
private:
    friend class MeshVertexArray;
    MeshVertexSlot(MeshVertexArray array, std::size_t index)
        : array_(std::move(array)), index_(index) {}
    MeshVertexArray array_;
    std::size_t index_;
};

struct MeshVertex {
    nicxlive::core::math::Vec2 position{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    MeshVertexArray connections;
    std::uint32_t groupId = 1;
    std::size_t originalIndex = std::numeric_limits<std::size_t>::max();
    MeshVertex() = default;
    MeshVertex(const MeshVertex& other) : position(other.position), connections(other.connections),
        groupId(other.groupId), originalIndex(other.originalIndex) {}
    MeshVertex& operator=(const MeshVertex& other) {
        if (this != &other) {
            position = other.position; connections = other.connections;
            groupId = other.groupId; originalIndex = other.originalIndex;
        }
        return *this;
    }
private:
    friend struct detail::MeshVertexStorage;
    friend class MeshVertexArray;
    friend MeshVertexHandle ngRetainMeshVertex(MeshVertex*);
    friend MeshVertexHandle ngTryRetainMeshVertex(MeshVertex*);
    friend MeshVertexHandle ngCreateMeshVertex(nicxlive::core::math::Vec2, std::uint32_t, std::size_t);
    std::weak_ptr<detail::MeshVertexStorage> owner_;
};
MeshVertexHandle ngRetainMeshVertex(MeshVertex*);
// An empty result never claims ownership of an unmanaged pointer.
MeshVertexHandle ngTryRetainMeshVertex(MeshVertex*);
MeshVertexHandle ngCreateMeshVertex(nicxlive::core::math::Vec2 position = {
    std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()},
    std::uint32_t groupId = 1, std::size_t originalIndex = std::numeric_limits<std::size_t>::max());
template<class Position> MeshVertexHandle ngCreateMeshVertexSource(Position&& position) {
    auto result = ngCreateMeshVertex();
    result->position = std::forward<Position>(position)();
    return result;
}
}
