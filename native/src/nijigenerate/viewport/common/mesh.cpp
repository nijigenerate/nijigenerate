// Copyright 2022 Inochi2D Project, 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "mesh.hpp"
#include <nijigenerate/core/math/triangulate.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <common/resource_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <functional>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <typeinfo>
#if defined(NG_DEBUG_DELAUNAY)
#include <nijigenerate/compat/phobos_stdio.hpp>
#endif

namespace nijigenerate::viewport::common::mesh {
namespace {
void prepareMeshClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(IncMesh),
            compat::RetainedString::fromStaticLiteral("nijigenerate.viewport.common.mesh.IncMesh"));
        return true;
    }();
    (void)registered;
}
struct MeshPointerHash {
    std::size_t operator()(MeshVertex* pointer) const noexcept {
        const auto address = reinterpret_cast<std::size_t>(pointer);
        return address ^ (address >> 4);
    }
};
template<class Value> using MeshPointerMap = nicxlive::core::common::ResourceKeyMap<
    MeshVertex*, Value, MeshPointerHash, std::equal_to<MeshVertex*>>;
using core::math::mesh::connect;
using core::math::mesh::disconnectAll;
using core::math::mesh::isConnectedTo;
using core::math::mesh::ngCreateMeshVertex;
Vec3 point3(Vec2 point) { return {point.x, point.y, 0}; }
// Actual independent nijilive/math/package.d segment-intersection provider.
bool areLineSegmentsIntersecting(Vec2 p1, Vec2 p2, Vec2 p3, Vec2 p4) {
    constexpr float epsilon = 0.00001f;
    const float denominator = (p4.y - p3.y) * (p2.x - p1.x) - (p4.x - p3.x) * (p2.y - p1.y);
    if (denominator == 0) return false;
    const float uA = ((p4.x - p3.x) * (p1.y - p3.y) - (p4.y - p3.y) * (p1.x - p3.x)) / denominator;
    const float uB = ((p2.x - p1.x) * (p1.y - p3.y) - (p2.y - p1.y) * (p1.x - p3.x)) / denominator;
    return uA > epsilon && uA < 1 - epsilon && uB > epsilon && uB < 1 - epsilon;
}
template<class Range> bool contains(const Range& values, MeshVertex* vertex) {
    return std::find(values.begin(), values.end(), vertex) != values.end();
}
}

bool isGrid(Vec2Array vertices, GridAxes& gridAxes) {
    // Initialize the source out argument before materializing the input.
    gridAxes.clear();
    return isGrid(vertices.toArray(), gridAxes);
}

std::shared_ptr<IncMesh> IncMesh::create(MeshData& mesh) {
    return create(mesh, {});
}
std::shared_ptr<IncMesh> IncMesh::create(MeshData& mesh, std::shared_ptr<MeshData> owner) {
    auto result = std::allocate_shared<IncMesh>(
        nicxlive::core::d_runtime::SourceAllocator<IncMesh>{}, SourceFieldsOnly{});
    if (owner.get() == &mesh) result->ownedData_ = owner;
    result->constructMeshOwned(mesh);
    return result;
}
std::shared_ptr<IncMesh> IncMesh::create(std::shared_ptr<MeshData> mesh) {
    auto result = std::allocate_shared<IncMesh>(
        nicxlive::core::d_runtime::SourceAllocator<IncMesh>{}, SourceFieldsOnly{});
    result->ownedData_ = mesh;
    result->constructMeshOwned(*compat::ngRequireObject(mesh));
    return result;
}
std::shared_ptr<IncMesh> IncMesh::create(std::shared_ptr<IncMesh> source) {
    auto result = std::allocate_shared<IncMesh>(
        nicxlive::core::d_runtime::SourceAllocator<IncMesh>{}, SourceFieldsOnly{});
    result->constructMeshOwned(source);
    return result;
}
void IncMesh::constructMeshOwned(MeshData& mesh) { import_(mesh); }
void IncMesh::constructMeshOwned(std::shared_ptr<IncMesh> source) {
    const auto receiver = compat::ngRequireObject(source);
    ownedData_ = receiver->ownedData_;
    import_(*receiver->data_);
    selectRadius = receiver->selectRadius;
    maxGroupId = receiver->maxGroupId;
}
// Inherited object.d defaults (Digital Mars 2000-2011, BSL-1.0).
// Retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
void IncMesh::prepareSourceClassInfo() { prepareMeshClassInfo(); }
compat::RetainedString IncMesh::toString() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t IncMesh::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
int IncMesh::opCmp(std::shared_ptr<IncMesh>) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        std::string text = "need opCmp for class ";
        text += className.view();
        message = compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool IncMesh::opEquals(std::shared_ptr<IncMesh> other) {
    return nicxlive::core::common::ngClassObjectIdentity(other) == dynamic_cast<const void*>(this);
}
bool ngIncMeshObjectEquals(std::shared_ptr<IncMesh> left, std::shared_ptr<IncMesh> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right), &prepareMeshClassInfo);
}
void IncMesh::import_(MeshData& mesh) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (ownedData_.get() != &mesh) ownedData_.reset();
    data_ = &mesh;
    mImport(mesh, true);
}
bool IncMesh::isBasedOn(MeshData& mesh) const { return data_ == &mesh; }
void IncMesh::merge_(MeshData& mesh, Mat4 matrix) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    mImport(mesh, false, matrix);
}
MeshData IncMesh::export_() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return mExport();
}

void IncMesh::mImport(MeshData& data, bool reset, Mat4 matrix) {
    if (reset) vertices.resize(0);
    else maxGroupId = 2;
    eOrigin_ = data.origin;
    MeshVertexArray imported;
    imported.resize(data.vertices.size());
    const auto sourceVertices = data.vertices;
    for (std::size_t i = 0; i < sourceVertices.size(); ++i) {
        const auto point = core::math::ngTransformHomogeneousPoint(matrix, point3(sourceVertices[i]));
        const auto vertex = ngCreateMeshVertex({point.x, point.y}, reset ? 1 : 2,
            reset ? i : std::numeric_limits<std::size_t>::max());
        imported.set(i, vertex.get());
    }
    const auto triangleCount = data.indices.size() / 3;
    for (std::size_t i = 0; i < triangleCount; ++i) {
        auto* a = imported[data.indices[i * 3]];
        auto* b = imported[data.indices[i * 3 + 1]];
        auto* c = imported[data.indices[i * 3 + 2]];
        if (!isConnectedTo(a, b)) connect(a, b);
        if (!isConnectedTo(b, c)) connect(b, c);
        if (!isConnectedTo(c, a)) connect(c, a);
    }
    for (auto* vertex : imported) {
        // Preserve the source's local printConnections traversal (it does not print).
        compat::Slice<std::uint16_t> connections;
        Vec2Array coordinates;
        for (auto* connection : vertex->connections) {
            for (std::size_t i = 0; i < imported.size(); ++i) {
                if (imported[i] == connection) {
                    connections.append(static_cast<std::uint16_t>(i));
                    coordinates.push_back(imported[i]->position);
                    break;
                }
            }
        }
        vertices.append(vertex);
    }
    axes.clear();
    GridAxes gridAxes;
    if (reset && isGrid(data.vertices, gridAxes)) {
        for (const auto& axis : gridAxes) axes.push_back(axis.duplicate());
    }
    refresh();
}

MeshData IncMesh::mExport() {
    auto newData = std::allocate_shared<MeshData>(
        nicxlive::core::d_runtime::SourceAllocator<MeshData>{});
    newData->origin = eOrigin_;
    MeshPointerMap<std::uint16_t> indices;
    std::uint16_t index = 0;
    for (auto* vertex : vertices) {
        newData->vertices.push_back(vertex->position);
        newData->uvs.push_back(vertex->position);
        indices[vertex] = index++;
    }
    const auto goesBackToRoot = [](MeshVertex* root, MeshVertex* vertex) {
        return contains(vertex->connections, root);
    };
    const auto hasSequence = [&](std::uint16_t a, std::uint16_t b, std::uint16_t c) {
        for (std::size_t i = 0; i < newData->indices.size() / 3; ++i) {
            int score = 0;
            for (std::size_t j = 0; j < 3; ++j) {
                const auto value = newData->indices[i * 3 + j];
                if (value == a || value == b || value == c) ++score;
            }
            if (score == 3) return true;
        }
        return false;
    };
    const auto intersectsEdges = [](const std::array<Vec2, 3>& a, const std::array<Vec2, 3>& b) {
        for (std::size_t i = 0; i < 3; ++i) for (std::size_t j = 0; j < 3; ++j)
            if (areLineSegmentsIntersecting(a[i], a[(i + 1) % 3], b[j], b[(j + 1) % 3])) return true;
        return false;
    };
    const auto intersectsTriangles = [&](const std::array<Vec2, 3>& triangle) {
        for (std::size_t i = 0; i < newData->indices.size() / 3; ++i) {
            // The current independent source repeats the first index three times.
            const std::array<Vec2, 3> points{
                newData->vertices[newData->indices[i * 3]],
                newData->vertices[newData->indices[i * 3]],
                newData->vertices[newData->indices[i * 3]]};
            if (intersectsEdges(triangle, points)) return true;
        }
        return false;
    };
    MeshVertexArray visited;
    const auto visit = [&](auto&& self, MeshVertex* vertex) -> void {
        visited.append(vertex);
        [[maybe_unused]] const auto findFreeIndice = [&]() -> MeshVertex* {
            for (auto* key : indices.keys()) {
                if (indices.at(key) != compat::ngArrayAt(newData->indices, newData->indices.size() - 1) &&
                    indices.at(key) != compat::ngArrayAt(newData->indices, newData->indices.size() - 2) &&
                    indices.at(key) != compat::ngArrayAt(newData->indices, newData->indices.size() - 3) &&
                    !contains(visited, key)) return key;
            }
            return nullptr;
        };
        for (auto* connection : vertex->connections) {
            if (connection == vertex) continue;
            for (auto* second : connection->connections) {
                if (goesBackToRoot(vertex, second)) {
                    const auto a = indices.at(vertex), b = indices.at(connection), c = indices.at(second);
                    if (hasSequence(a, b, c)) continue;
                    if (intersectsTriangles({vertex->position, connection->position, second->position})) continue;
                    newData->indices.push_back(a);
                    newData->indices.push_back(b);
                    newData->indices.push_back(c);
                    break;
                }
            }
        }
        for (auto* connection : vertex->connections) if (!contains(visited, connection)) self(self, connection);
    };
    for (auto* vertex : vertices) if (!contains(visited, vertex)) visit(visit, vertex);
    if (axes.size() >= 2) newData->gridAxes = axes;
    newData->clearGridIfDirty();
    ownedData_ = newData;
    data_ = newData.get();
    reset();
    return *newData;
}
void IncMesh::regen() {
    points_.resize(0);
    for (auto* vertex : vertices) points_.push_back(point3(vertex->position));
}
void IncMesh::regenConnections() {
    lines_.resize(0);
    wlines_.resize(0);
    MeshPointerMap<bool> visited;
    MeshVertexArray stack;
    [[maybe_unused]] const auto recurseLines = [&](auto&& self, MeshVertex* current) -> void {
        visited[current] = true;
        for (auto* connection : current->connections) if (!visited.contains(connection)) {
            lines_.push_back(point3(current->position));
            lines_.push_back(point3(connection->position));
        }
        for (auto* connection : current->connections)
            if (!visited.contains(connection)) self(self, connection);
    };
    for (std::size_t i = vertices.size(); i > 0; --i)
        if (!visited.contains(vertices[i - 1])) stack.append(vertices[i - 1]);
    while (!stack.empty()) {
        auto* current = stack[stack.size() - 1];
        const auto retained = core::math::mesh::ngTryRetainMeshVertex(current);
        stack.resize(stack.size() - 1);
        visited[current] = true;
        for (auto* connection : current->connections) {
            if (!visited.contains(connection)) {
                lines_.push_back(point3(current->position));
                lines_.push_back(point3(connection->position));
            }
        }
        for (std::size_t i = current->connections.size(); i > 0; --i) {
            auto* connection = current->connections[i - 1];
            if (!visited.contains(connection)) stack.append(connection);
        }
    }
}
std::size_t IncMesh::getEdgeCount() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (lines_.empty()) regenConnections();
    return lines_.size();
}
bool IncMesh::hasInvalidGeometry() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    for (auto* vertex : vertices) {
        if (vertex->connections.size() < 2) return true;
        bool valid = false;
        for (auto* a : vertex->connections) for (auto* b : vertex->connections) {
            if (a != b && isConnectedTo(a, b)) valid = true;
        }
        if (!valid) return true;
    }
    return false;
}
std::size_t IncMesh::getTriCount() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (vertices.empty()) return 0;
    std::size_t triangles = 0, vertexIndex = 0;
    MeshVertexArray visited;
    auto* vertex = vertices[0];
    while (vertexIndex + 1 < vertices.size()) {
        if (vertex->connections.size() > 1) {
            compat::Slice<std::array<MeshVertex*, 2>> mutual;
            for (auto* a : vertex->connections) for (auto* b : vertex->connections) {
                if (a == b) continue;
                bool repeated = false;
                for (const auto& pair : mutual) {
                    if ((pair[0] == a || pair[0] == b) && (pair[1] == a || pair[1] == b)) {
                        repeated = true;
                        break;
                    }
                }
                if (repeated) continue;
                if (isConnectedTo(a, b) && !contains(visited, a) && !contains(visited, b)) {
                    mutual.append({a, b});
                    ++triangles;
                }
            }
        }
        vertex = vertices[++vertexIndex];
        visited.append(vertex);
    }
    return triangles;
}
std::size_t IncMesh::getVertexCount() { return vertices.size(); }
void IncMesh::reset() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    mImport(*data_, true);
    refresh();
    changed = true;
}
void IncMesh::clear() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    vertices.resize(0);
    refresh();
    changed = true;
}
void IncMesh::refresh() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    regen();
    regenConnections();
}
bool IncMesh::isPointOverVertex(Vec2 point, float zoomRate) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return core::math::vertex::isPointOverVertex(vertices, point, zoomRate);
}
void IncMesh::removeVertexAt(Vec2 point, float zoomRate) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    core::math::vertex::removeVertexAt(vertices, point, zoomRate, [this](MeshVertex* vertex) { remove(vertex); });
}
std::uint64_t IncMesh::getVertexFromPoint(Vec2 point, float zoomRate) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return core::math::vertex::getVertexFromPoint(vertices, point, zoomRate);
}
compat::Slice<float> IncMesh::getVerticesInBrush(Vec2 point, std::shared_ptr<Brush> brush) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return core::math::vertex::getVerticesInBrush(vertices, point, brush);
}
void IncMesh::remove(MeshVertex* vertex) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    [[maybe_unused]] const auto vertexLifetime = core::math::mesh::ngTryRetainMeshVertex(vertex);
    for (std::size_t i = 0; i < vertices.size(); ++i) if (vertices[i] == vertex) {
        disconnectAll(vertex);
        for (auto j = i; j + 1 < vertices.size(); ++j) vertices.set(j, vertices[j + 1]);
        vertices.resize(vertices.size() - 1);
        break;
    }
    changed = true;
}
Vec2Array IncMesh::getOffsets() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Vec2Array offsets(vertices.size());
    if (data_->vertices.size() >= vertices.size()) for (std::size_t i = 0; i < vertices.size(); ++i) {
        offsets[i] = Vec2{vertices[i]->position.x - data_->vertices[i].x,
                         vertices[i]->position.y - data_->vertices[i].y};
    }
    return offsets;
}
void IncMesh::applyOffsets(Vec2Array offsets) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const auto value = offsets[i].toVector();
        vertices[i]->position.x += value.x;
        vertices[i]->position.y += value.y;
    }
    regen(); regenConnections(); changed = true;
}
void IncMesh::flipHorz() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    for (auto* v : vertices) v->position.x *= -1;
    refresh();
    changed = true;
}
void IncMesh::flipVert() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    for (auto* v : vertices) v->position.y *= -1;
    refresh();
    changed = true;
}
void IncMesh::getBounds(Vec2& minimum, Vec2& maximum) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    core::math::vertex::getBounds(vertices, minimum, maximum);
}
nicxlive::core::common::SharedArray<std::uint64_t> IncMesh::getInRect(Vec2 minimum, Vec2 maximum,
    std::uint32_t groupId) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return core::math::vertex::getInRect(vertices, minimum, maximum, groupId);
}
void IncMesh::importVertsAndTris(Vec2Array positions, compat::Slice<compat::Vec3u> triangles) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    for (std::size_t i = 0; i < positions.size(); ++i) vertices.append(ngCreateMeshVertex(positions[i].toVector()));
    for (const auto triangle : triangles) {
        connect(vertices[triangle.x], vertices[triangle.y]);
        connect(vertices[triangle.y], vertices[triangle.z]);
        connect(vertices[triangle.z], vertices[triangle.x]);
    }
}
std::shared_ptr<IncMesh> IncMesh::autoTriangulate() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
#if defined(NG_DEBUG_DELAUNAY)
    providers::mcp::stdio::standardOutput().writeln("==== autoTriangulate ====");
#endif
    if (vertices.size() < 3) return ownedData_ ? create(ownedData_) : create(*data_);
    Vec2 minimum, maximum;
    getBounds(minimum, maximum);
    auto [positions, triangles] = core::math::triangle::triangulate(vertices,
        Vec4{minimum.x, minimum.y, maximum.x, maximum.y});
    auto result = ownedData_ ? create(ownedData_) : create(*data_);
    result->changed = true;
    result->vertices.resize(0);
    result->importVertsAndTris(positions, triangles);
    nicxlive::core::common::SharedArray<bool> used(vertices.size(), false);
    for (auto* vertex : result->vertices) for (std::size_t i = 0; i < vertices.size(); ++i) {
        auto* original = vertices[i];
        if (!used[i] && vertex->position.x == original->position.x && vertex->position.y == original->position.y) {
            vertex->originalIndex = original->originalIndex;
            used[i] = true;
            break;
        }
    }
    result->refresh();
#if defined(NG_DEBUG_DELAUNAY)
    providers::mcp::stdio::standardOutput().writeln("==== autoTriangulate done ====");
#endif
    return result;
}
void IncMesh::copyFromMeshData(MeshData data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    mImport(data, true);
}
std::shared_ptr<IncMesh> ngCreateIncMesh(Vec2Array positions) {
    auto data = std::allocate_shared<MeshData>(nicxlive::core::d_runtime::SourceAllocator<MeshData>{});
    GridAxes axes;
    if (isGrid(positions, axes) && axes[0].size() >= 2 && axes[1].size() >= 2) {
        data->gridAxes.resize(2);
        data->gridAxes = axes;
        data->regenerateGrid();
    } else {
        data->vertices = positions.dup();
    }
    return IncMesh::create(data);
}
Vec2Array ngMeshPositions(std::shared_ptr<IncMesh> mesh) {
    Vec2Array positions(compat::ngRequireObject(mesh)->vertices.size());
    const auto vertices = compat::ngRequireObject(mesh)->vertices;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        auto* vertex = vertices[i];
        if (vertex) positions[i] = vertex->position;
    }
    return positions;
}
MeshVertexArray ngMeshVerticesFromPositions(std::span<const Vec2> positions) {
    MeshVertexArray vertices;
    for (const auto position : positions) vertices.append(ngCreateMeshVertex(position));
    return vertices;
}
MeshVertexArray ngMeshVerticesFromPositions(Vec2Array positions) {
    auto values = positions.toArray();
    if (values.empty()) return ngMeshVerticesFromPositions(compat::ConstSlice<Vec2>{});
    const compat::Slice<Vec2> retained(std::move(values));
    return ngMeshVerticesFromPositions(compat::ConstSlice<Vec2>(retained));
}
MeshVertexArray ngMeshVerticesFromPositions(compat::ConstSlice<Vec2> positions) {
    return ngMeshVerticesFromPositions(positions.view());
}
}
