#pragma once
#include <nijigenerate/core/math/mesh.hpp>
#include <nijigenerate/core/math/vertex.hpp>
#include <nijigenerate/compat/vec3u.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <common/resource_key_map.hpp>
#include <common/float_key_policy.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nodes/drawable.hpp>
#include <nodes/grid_axis_sort.hpp>
#include <bit>
#include <math/mat4.hpp>
#include <math/veca.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace nijigenerate::viewport::common::mesh {
using nicxlive::core::nodes::MeshData;
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Vec4;
using nicxlive::core::math::Mat4;
using nicxlive::core::math::Vec2Array;
using nicxlive::core::math::Vec3Array;
using core::math::mesh::MeshVertex;
using core::math::mesh::MeshVertexArray;
using core::math::mesh::connect;
using core::math::mesh::disconnect;
using core::math::mesh::disconnectAll;
using core::math::mesh::isConnectedTo;
using core::math::mesh::applyMeshToTarget;
using core::math::mesh::applyMeshToTargetNoRecord;
using FloatArray = nicxlive::core::common::SharedArray<float>;
using GridAxes = nicxlive::core::common::SharedArray<FloatArray>;
using Marker = std::pair<compat::Slice<std::ptrdiff_t>, Vec4>;
using Markers = compat::Slice<Marker>;
using Brush = mesheditor::brushes::base::Brush;

template<class Vertices>
bool isGrid(Vertices vertices, GridAxes& gridAxes) {
    gridAxes.clear();
    if (vertices.size() < 4) return false;
    using Counts = nicxlive::core::common::PolicyKeyMap<float, std::int32_t,
        nicxlive::core::common::Float32KeyPolicy>;
    Counts yPoints, xPoints;
    const auto vertexCount = vertices.size();
    for (std::size_t i = 0; i < vertexCount; ++i) {
        auto&& vertex = vertices[i];
        const auto position = core::math::vertex::position(vertex);
        auto yCell = yPoints[position.y];
        auto& yCount = static_cast<std::int32_t&>(yCell);
        yCount = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(yCount) + 1u);
        auto xCell = xPoints[position.x];
        auto& xCount = static_cast<std::int32_t&>(xCell);
        xCount = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(xCount) + 1u);
    }
    for (auto key : yPoints.keys()) {
        const auto count = yPoints.at(key);
        const auto oppositeKeys = xPoints.keys();
        if (count != oppositeKeys.size()) return false;
    }
    for (auto key : xPoints.keys()) {
        const auto count = xPoints.at(key);
        const auto oppositeKeys = yPoints.keys();
        if (count != oppositeKeys.size()) return false;
    }
    gridAxes.resize(2);
    auto yKeys = yPoints.keys();
    nicxlive::core::nodes::grid_axis_detail::sort({yKeys.data(), yKeys.size()});
    gridAxes[0] = yKeys.duplicate();
    auto xKeys = xPoints.keys();
    nicxlive::core::nodes::grid_axis_detail::sort({xKeys.data(), xKeys.size()});
    gridAxes[1] = xKeys.duplicate();
    return true;
}
bool isGrid(Vec2Array vertices, GridAxes& gridAxes);

class IncMesh : public std::enable_shared_from_this<IncMesh> {
private:
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    MeshData* data_ = nullptr;
    std::shared_ptr<MeshData> ownedData_;
    Vec2 eOrigin_{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    Vec3Array points_, lines_, wlines_;
    void mImport(MeshData& data, bool reset, Mat4 matrix = Mat4::identity());
    MeshData mExport();
    void regen();
    void regenConnections();
public:
    float selectRadius = 16.0f;
    MeshVertexArray vertices;
    GridAxes axes;
    bool changed = false;
    std::uint32_t maxGroupId = 1;
    IncMesh(const IncMesh&) = delete;
    IncMesh& operator=(const IncMesh&) = delete;
    static std::shared_ptr<IncMesh> create(MeshData& mesh);
    static std::shared_ptr<IncMesh> create(MeshData& mesh, std::shared_ptr<MeshData> owner);
    static std::shared_ptr<IncMesh> create(std::shared_ptr<MeshData> mesh);
    // The producer must retain the actual referenced MeshData cell, not a copy.
    template<class Producer>
    static std::shared_ptr<IncMesh> createSourceDeferred(Producer&& produceMeshCell) {
        auto receiver = std::allocate_shared<IncMesh>(
            nicxlive::core::d_runtime::SourceAllocator<IncMesh>{}, SourceFieldsOnly{});
        std::shared_ptr<MeshData> mesh = std::invoke(std::forward<Producer>(produceMeshCell));
        receiver->ownedData_ = mesh;
        receiver->constructMeshOwned(*compat::ngRequireObject(mesh));
        return receiver;
    }
    static std::shared_ptr<IncMesh> create(std::shared_ptr<IncMesh> source);
    static std::shared_ptr<IncMesh> create(std::nullptr_t) {
        return create(std::shared_ptr<IncMesh>{});
    }
    virtual ~IncMesh() = default;
    virtual void import_(MeshData& mesh) final;
    virtual bool isBasedOn(MeshData& mesh) const;
    virtual void merge_(MeshData& mesh, Mat4 matrix) final;
    virtual MeshData export_() final;
    virtual std::size_t getEdgeCount() final;
    virtual bool hasInvalidGeometry() final;
    virtual std::size_t getTriCount() final;
    virtual std::size_t getVertexCount() final;
    virtual void reset();
    virtual void clear();
    virtual void refresh();
    virtual void drawLines(Mat4 transform = Mat4::identity(), Vec4 color = {0.7f, 0.7f, 0.7f, 1});
    virtual void drawPoints(Mat4 transform = Mat4::identity(), Vec4 color = {1, 1, 1, 1}, Markers markers = {});
    virtual void drawPointSubset(MeshVertexArray subset, Vec4 color, Mat4 transform = Mat4::identity(), float size = 6);
    virtual void drawPoint(Vec2 point, Vec4 color, Mat4 transform = Mat4::identity(), float size = 6);
    virtual void draw(Mat4 transform = Mat4::identity(), Vec4 vertexColor = {1, 1, 1, 1},
        Vec4 edgeColor = {0.7f, 0.7f, 0.7f, 1}, Markers markers = {});
    virtual bool isPointOverVertex(Vec2 point, float zoomRate);
    virtual void removeVertexAt(Vec2 point, float zoomRate);
    virtual std::uint64_t getVertexFromPoint(Vec2 point, float zoomRate);
    virtual compat::Slice<float> getVerticesInBrush(Vec2 point, std::shared_ptr<Brush> brush);
    virtual void remove(MeshVertex* vertex);
    virtual Vec2Array getOffsets();
    virtual void applyOffsets(Vec2Array offsets);
    virtual void flipHorz();
    virtual void flipVert();
    virtual void getBounds(Vec2& minimum, Vec2& maximum);
    virtual nicxlive::core::common::SharedArray<std::uint64_t> getInRect(
        Vec2 minimum, Vec2 maximum, std::uint32_t groupId = 0);
    virtual void importVertsAndTris(Vec2Array positions, compat::Slice<compat::Vec3u> triangles);
    virtual std::shared_ptr<IncMesh> autoTriangulate();
    virtual void copyFromMeshData(MeshData data);
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual int opCmp(std::shared_ptr<IncMesh> other);
    virtual bool opEquals(std::shared_ptr<IncMesh> other);
protected:
    virtual void prepareSourceClassInfo();
    struct SourceFieldsOnly {};
    explicit IncMesh(SourceFieldsOnly) noexcept {}
    void constructMeshOwned(MeshData& mesh);
    void constructMeshOwned(std::shared_ptr<IncMesh> source);
};
bool ngIncMeshObjectEquals(std::shared_ptr<IncMesh> left, std::shared_ptr<IncMesh> right);
std::shared_ptr<IncMesh> ngCreateIncMesh(Vec2Array positions);
Vec2Array ngMeshPositions(std::shared_ptr<IncMesh> mesh);
MeshVertexArray ngMeshVerticesFromPositions(std::span<const Vec2> positions);
MeshVertexArray ngMeshVerticesFromPositions(compat::ConstSlice<Vec2> positions);
MeshVertexArray ngMeshVerticesFromPositions(Vec2Array positions);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::common::mesh::IncMesh> {
    inline static constexpr char className[] = "nijigenerate.viewport.common.mesh.IncMesh";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.common.mesh.IncMesh)";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesh::IncMesh> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.common.mesh.IncMesh)";
    inline static constexpr char unqualifiedArgument[] = "(IncMesh)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.common.mesh.IncMesh";
    inline static constexpr char unqualifiedElement[] = "IncMesh";
};
}
