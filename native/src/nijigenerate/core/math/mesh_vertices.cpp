#include "mesh_vertices.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/compat/array_access.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include "nijigenerate/viewport/common/mesheditor/operations/impl.hpp"
#include <compat/source_allocator.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace nijigenerate::core::math::mesh {
namespace detail {
nicxlive::core::math::Vec2Array toVertices(MeshVertexArray vertices) {
    return viewport::common::mesheditor::operations::impl::toVertices(vertices);
}

struct MeshVertexStorage : std::enable_shared_from_this<MeshVertexStorage> {
    std::shared_ptr<MeshVertexStorage> parent;
    std::vector<std::unique_ptr<MeshVertex>,
        nicxlive::core::d_runtime::SourceAllocator<std::unique_ptr<MeshVertex>>> vertices;
    static std::shared_ptr<MeshVertexStorage> root(std::shared_ptr<MeshVertexStorage> value) {
        while (value && value->parent) value = value->parent;
        return value;
    }
    static std::shared_ptr<MeshVertexStorage> join(std::shared_ptr<MeshVertexStorage> a,
        std::shared_ptr<MeshVertexStorage> b) {
        a = root(std::move(a)); b = root(std::move(b));
        if (!a) return b;
        if (!b || a == b) return a;
        // Complete the sole throwing storage operation before changing either graph.
        if (b->vertices.size() > a->vertices.max_size() - a->vertices.size())
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        try {
            a->vertices.reserve(a->vertices.size() + b->vertices.size());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        for (auto& vertex : b->vertices) {
            vertex->owner_ = a;
            vertex->connections.assignOwner(a);
            a->vertices.push_back(std::move(vertex));
        }
        b->vertices.clear();
        b->parent = a;
        return a;
    }
};
}
MeshVertexHandle ngCreateMeshVertex(nicxlive::core::math::Vec2 position, std::uint32_t groupId,
    std::size_t originalIndex) {
    try {
        MeshVertexHandle result;
        result.owner_ = std::allocate_shared<detail::MeshVertexStorage>(
            nicxlive::core::d_runtime::SourceAllocator<detail::MeshVertexStorage>{});
        std::unique_ptr<MeshVertex> vertex(new MeshVertex);
        vertex->position = position; vertex->groupId = groupId; vertex->originalIndex = originalIndex;
        vertex->owner_ = result.owner_;
        vertex->connections.embedded_ = true;
        vertex->connections.assignOwner(result.owner_);
        result.pointer_ = vertex.get();
        result.owner_->vertices.push_back(std::move(vertex));
        return result;
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
MeshVertexHandle ngRetainMeshVertex(MeshVertex* vertex) {
    auto result = ngTryRetainMeshVertex(vertex);
    if (vertex && !result) throw std::logic_error("MeshVertex has no live owning handle");
    return result;
}
MeshVertexHandle ngTryRetainMeshVertex(MeshVertex* vertex) {
    MeshVertexHandle result;
    if (!vertex) return result;
    result.owner_ = detail::MeshVertexStorage::root(vertex->owner_.lock());
    if (!result.owner_) return {};
    result.pointer_ = vertex;
    return result;
}
std::shared_ptr<detail::MeshVertexStorage> MeshVertexArray::owner() const {
    return detail::MeshVertexStorage::root(embedded_ ? weak_.lock() : retained_);
}
void MeshVertexArray::assignOwner(std::shared_ptr<detail::MeshVertexStorage> value) {
    if (embedded_) { weak_ = value; retained_.reset(); }
    else { retained_ = std::move(value); weak_.reset(); }
}
MeshVertexArray::MeshVertexArray(const MeshVertexArray& other) : values_(other.values_), retained_(other.owner()) {}
MeshVertexArray::MeshVertexArray(MeshVertexArray&& other) : MeshVertexArray(static_cast<const MeshVertexArray&>(other)) {}
MeshVertexArray& MeshVertexArray::operator=(const MeshVertexArray& other) {
    if (this != &other) {
        auto ownership = other.owner();
        if (embedded_) ownership = detail::MeshVertexStorage::join(owner(), std::move(ownership));
        values_ = other.values_; assignOwner(std::move(ownership));
    }
    return *this;
}
MeshVertexArray& MeshVertexArray::operator=(MeshVertexArray&& other) {
    return operator=(static_cast<const MeshVertexArray&>(other));
}
void MeshVertexArray::retain(MeshVertex* value) {
    auto retained = ngTryRetainMeshVertex(value);
    auto graph = owner();
    // Resolve ownership through the handle before union changes either root.
    if (value) {
        // MeshVertexArray is a descriptor owner, never an owning graph edge.
        auto pointerOwner = value->owner_.lock();
        graph = detail::MeshVertexStorage::join(std::move(graph), std::move(pointerOwner));
    }
    if (!graph) {
        // An array of null/borrowed pointers needs a shared ownership group for
        // later writes through retained references to its actual pointer cells.
        graph = std::allocate_shared<detail::MeshVertexStorage>(
            nicxlive::core::d_runtime::SourceAllocator<detail::MeshVertexStorage>{});
    }
    assignOwner(std::move(graph));
}
MeshVertexSlot MeshVertexArray::mutableSlot(std::size_t index) {
    (void)compat::ngArrayAt(values_, index);
    return MeshVertexSlot(*this, index);
}
void MeshVertexArray::append(MeshVertex* value) { retain(value); values_.append(value); }
void MeshVertexArray::append(const MeshVertexArray& values) {
    auto snapshot = values;
    for (auto* vertex : snapshot) append(vertex);
}
void MeshVertexArray::set(std::size_t index, MeshVertex* value) { retain(value); values_[index] = value; }
void MeshVertexArray::resize(std::size_t size) {
    if (size && !owner()) assignOwner(std::allocate_shared<detail::MeshVertexStorage>(
        nicxlive::core::d_runtime::SourceAllocator<detail::MeshVertexStorage>{}));
    values_.resize(size, nullptr);
}
void MeshVertexArray::clear() { values_.clear(); if (!embedded_) assignOwner({}); }
MeshVertexArray MeshVertexArray::duplicate() const {
    MeshVertexArray result(*this); result.values_ = values_.duplicate(); return result;
}
MeshVertexArray MeshVertexArray::slice(std::size_t first, std::size_t last) const {
    MeshVertexArray result(*this); result.values_ = values_.slice(first, last); return result;
}
void connect(MeshVertex* self, MeshVertex* other) {
    if (isConnectedTo(self, other)) return;
    auto selfLifetime = ngTryRetainMeshVertex(compat::ngRequireObject(self));
    self->connections.append(other);
    compat::ngRequireObject(other)->connections.append(self);
}
void disconnect(MeshVertex* self, MeshVertex* other) {
    auto otherLifetime = ngTryRetainMeshVertex(compat::ngRequireObject(other));
    auto selfLifetime = ngTryRetainMeshVertex(self);
    const auto remove = [](MeshVertexArray& vertices, MeshVertex* value) {
        for (std::size_t i = 0; i < vertices.size(); ++i) if (vertices[i] == value) {
            for (std::size_t j = i + 1; j < vertices.size(); ++j) vertices.set(j - 1, vertices[j]);
            vertices.resize(vertices.size() - 1);
            return;
        }
    };
    remove(other->connections, self);
    remove(compat::ngRequireObject(self)->connections, other);
}
void disconnectAll(MeshVertex* self) {
    auto lifetime = ngTryRetainMeshVertex(compat::ngRequireObject(self));
    while (!self->connections.empty()) disconnect(self, self->connections[0]);
}
bool isConnectedTo(MeshVertex* self, MeshVertex* other) {
    if (!other) return false;
    const auto connections = other->connections;
    for (auto* connection : connections) if (connection == self) return true;
    return false;
}
}
