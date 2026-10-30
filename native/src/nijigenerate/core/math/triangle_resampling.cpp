#include "triangle_resampling.hpp"
#include "detail/grid_locate_interval.hpp"
#include "detail/grid_bilinear.hpp"
#include "detail/triangle_mesh_resampling.hpp"
#include <nodes/path_deformer.hpp>
#include <param/binding_impl.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/grid_axis_sort.hpp>
#include <algorithm>
#include <vector>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace nijigenerate::core::math::triangle {
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    ::nicxlive::core::param::DeformationParameterBinding* binding,
    ::nicxlive::core::param::DeformationParameterBinding* sourceBinding,
    ::nicxlive::core::param::Vec2u index, bool flipHorz) {
    using namespace ::nicxlive::core::nodes;
    if (!binding || !sourceBinding) return {};
    [[maybe_unused]] const auto bindingLifetime = binding->weak_from_this().lock();
    [[maybe_unused]] const auto sourceBindingLifetime = sourceBinding->weak_from_this().lock();
    if (auto drawable = std::dynamic_pointer_cast<Drawable>(binding->getTarget().target.lock())) {
        if (auto sourceDrawable = std::dynamic_pointer_cast<Drawable>(sourceBinding->getTarget().target.lock())) {
            auto& sourceMesh = drawable->nicxlive::core::nodes::Drawable::getMesh();
            const auto sourceMeshOwner = drawable->mesh;
            auto mesh = ::nijigenerate::viewport::common::mesh::IncMesh::create(sourceMesh, sourceMeshOwner);
            const Deformation deformation = sourceBinding->getValue(index);
            const auto vertices = mesh->vertices;
            return deformByDeformationBinding(vertices, sourceDrawable.get(), deformation, flipHorz);
        }
    } else if (auto grid = std::dynamic_pointer_cast<GridDeformer>(binding->getTarget().target.lock())) {
        if (auto sourceGrid = std::dynamic_pointer_cast<GridDeformer>(sourceBinding->getTarget().target.lock())) {
            const Deformation deformation = sourceBinding->getValue(index);
            const auto vertices = grid->getVertices();
            return deformByDeformationBinding(vertices, sourceGrid.get(), deformation, flipHorz);
        }
    } else if (auto path = std::dynamic_pointer_cast<PathDeformer>(binding->getTarget().target.lock())) {
        if (auto sourcePath = std::dynamic_pointer_cast<PathDeformer>(sourceBinding->getTarget().target.lock())) {
            const Deformation deformation = sourceBinding->getValue(index);
            const auto vertices = path->getVertices();
            return deformByDeformationBinding(vertices, sourcePath.get(), deformation, flipHorz);
        }
    }
    return {};
}
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    ::nicxlive::core::math::Vec2Array vertices,
    ::nicxlive::core::param::DeformationParameterBinding* binding,
    ::nicxlive::core::param::Vec2u index, bool flipHorz) {
    using namespace ::nicxlive::core::nodes;
    if (!binding) return {};
    [[maybe_unused]] const auto bindingLifetime = binding->weak_from_this().lock();
    if (auto drawable = std::dynamic_pointer_cast<Drawable>(binding->getTarget().target.lock())) {
        const Deformation deformation = binding->getValue(index);
        return deformByDeformationBinding(vertices, drawable.get(), deformation, flipHorz);
    }
    if (auto grid = std::dynamic_pointer_cast<GridDeformer>(binding->getTarget().target.lock())) {
        const Deformation deformation = binding->getValue(index);
        return deformByDeformationBinding(vertices, grid.get(), deformation, flipHorz);
    }
    if (auto path = std::dynamic_pointer_cast<PathDeformer>(binding->getTarget().target.lock())) {
        const Deformation deformation = binding->getValue(index);
        return deformByDeformationBinding(vertices, path.get(), deformation, flipHorz);
    }
    return {};
}
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    ::nicxlive::core::math::Vec2Array vertices, ::nicxlive::core::nodes::Drawable* drawable,
    ::nicxlive::core::nodes::Deformation deform, bool flipHorz) {
    using namespace detail::resampling;
    [[maybe_unused]] const auto receiverLifetime = drawable ? drawable->weak_from_this().lock() :
        std::shared_ptr<::nicxlive::core::nodes::Node>{};
    if (deform.vertexOffsets.size() < 3 || vertices.size() < 3 ||
        compat::ngRequireObject(drawable)->nicxlive::core::nodes::Drawable::getMesh().vertices.size() < 3) {
        ::nicxlive::core::math::Vec2Array offsets;
        for (std::int32_t i = 0; static_cast<std::size_t>(i) < vertices.size(); i = searchAdd(i, 1))
            offsets.push_back(Vec2{0, 0});
        return detail::resampling::allocateDeformation(Deformation{offsets});
    }
    auto original = vertices.dup();
    auto mesh = compat::ngRequireObject(drawable)->nicxlive::core::nodes::Drawable::getMesh();
    auto result = detail::resampling::allocateDeformation();
    const auto transformed = transformMesh(mesh, deform);
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        Vec2 point = vertices[i];
        if (flipHorz) point.x = -point.x;
        auto triangle = findSurroundingTriangle(point, mesh, deform);
        if (triangle.size() == 0) {
            std::int32_t j = 0;
            compat::Slice<std::int32_t> candidate{0, 1, 2};
            float nearestDistance = -1;
            std::int32_t nearestIndex = -1;
            while (static_cast<std::size_t>(searchAdd(j, 2)) < mesh.indices.size()) {
                candidate[0] = mesh.indices[static_cast<std::size_t>(j)];
                candidate[1] = mesh.indices[static_cast<std::size_t>(searchAdd(j, 1))];
                candidate[2] = mesh.indices[static_cast<std::size_t>(searchAdd(j, 2))];
                if (!usableTriangle(mesh, candidate, deform)) {
                    j = searchAdd(j, 3);
                    continue;
                }
                const auto distance = [&](std::int32_t index) {
                    const auto p = meshVertex(mesh, index);
                    const float x = point.x - p.x, y = point.y - p.y;
                    long double squared = 0;
                    squared += x * x;
                    squared += y * y;
                    return squared;
                };
                const auto d1 = distance(candidate[0]), d2 = distance(candidate[1]), d3 = distance(candidate[2]);
                const auto d12 = d2 < d1 ? d2 : d1;
                const auto minimum = d3 < d12 ? d3 : d12;
                if (nearestDistance < 0 || minimum < nearestDistance) {
                    nearestDistance = static_cast<float>(minimum); nearestIndex = j;
                }
                j = searchAdd(j, 3);
            }
            if (nearestIndex >= 0) {
                compat::Slice<std::int32_t> nearest;
                try {
                    nearest = compat::Slice<std::int32_t>{std::vector<std::int32_t>(3)};
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
                nearest[0] = mesh.indices[static_cast<std::size_t>(nearestIndex)];
                nearest[1] = mesh.indices[static_cast<std::size_t>(searchAdd(nearestIndex, 1))];
                nearest[2] = mesh.indices[static_cast<std::size_t>(searchAdd(nearestIndex, 2))];
                triangle = nearest;
            }
        }
        if (triangle.size() == 0)
            throw compat::ThrowableException{detail::resampling::enforcementMessage(
                "No finite nondegenerate triangle available for deformation resampling"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/triangle.d"), 209};
        const auto offset = calcOffsetInTriangleCoords(point, mesh, triangle);
        auto position = transformPointInTriangleCoords(point, offset, transformed, triangle);
        if (flipHorz) position.x = -position.x;
        const Vec2 old = original[i];
        result->vertexOffsets.push_back({position.x - old.x, position.y - old.y});
    }
    return result;
}

std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    ::nicxlive::core::math::Vec2Array vertices, ::nicxlive::core::nodes::PathDeformer* path,
    ::nicxlive::core::nodes::Deformation deform, bool flipHorz) {
    using namespace ::nicxlive::core::nodes;
    [[maybe_unused]] const auto receiverLifetime = path ? path->weak_from_this().lock() : std::shared_ptr<Node>{};
    if (deform.vertexOffsets.size() < 2 || vertices.size() < 2 ||
        compat::ngRequireObject(path)->getVertices().size() < 2) {
        ::nicxlive::core::math::Vec2Array offsets;
        for (std::int32_t i = 0; static_cast<std::size_t>(i) < vertices.size();
                i = detail::resampling::searchAdd(i, 1))
            offsets.push_back(Vec2{0, 0});
        return detail::resampling::allocateDeformation(Deformation{offsets});
    }
    auto original = compat::ngRequireObject(path)->getVertices().dup();
    auto deformed = compat::ngRequireObject(path)->getVertices().dup();
    for (std::size_t i = 0; i < original.size(); ++i) {
        auto posed = deformed[i];
        const Vec2 offset = deform.vertexOffsets[i];
        posed = {posed.x + offset.x, posed.y + offset.y};
        if (flipHorz) {
            original[i].x *= -1;
            deformed[i].x *= -1;
        }
    }
    const auto originalCurve = compat::ngRequireObject(path)->createCurve(original);
    const auto deformedCurve = compat::ngRequireObject(path)->createCurve(deformed);
    auto result = detail::resampling::allocateDeformation();
    const auto normalized = [](Vec2 v) {
        long double squared = 0;
        squared += v.x * v.x;
        squared += v.y * v.y;
        const long double length = std::sqrt(squared);
        if (length == 0) return v;
        return Vec2{static_cast<float>(static_cast<long double>(v.x) / length),
            static_cast<float>(static_cast<long double>(v.y) / length)};
    };
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const Vec2 point = vertices[i];
        const float t = compat::ngRequireObject(originalCurve)->closestPoint(point);
        const auto closest = compat::ngRequireObject(originalCurve)->point(t);
        const auto tangent = normalized(compat::ngRequireObject(originalCurve)->derivative(t));
        const Vec2 normal{-tangent.y, tangent.x};
        const Vec2 delta{point.x - closest.x, point.y - closest.y};
        const auto dot = [](Vec2 a, Vec2 b) {
            float sum = 0;
            sum += a.x * b.x;
            sum += a.y * b.y;
            return sum;
        };
        const float normalDistance = dot(delta, normal);
        const float tangentDistance = dot(delta, tangent);
        const auto posed = compat::ngRequireObject(deformedCurve)->point(t);
        const auto posedTangent = normalized(compat::ngRequireObject(deformedCurve)->derivative(t));
        const Vec2 posedNormal{-posedTangent.y, posedTangent.x};
        result->vertexOffsets.push_back({
            (posed.x + posedNormal.x * normalDistance) + posedTangent.x * tangentDistance - point.x,
            (posed.y + posedNormal.y * normalDistance) + posedTangent.y * tangentDistance - point.y});
    }
    return result;
}
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    compat::Slice<::nicxlive::core::math::Vec2> vertices, ::nicxlive::core::nodes::GridDeformer* deformable,
    ::nicxlive::core::nodes::Deformation deform, bool flipHorz) {
    return deformByDeformationBinding(toVec2Array(vertices), deformable, deform, flipHorz);
}

std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    ::nicxlive::core::math::Vec2Array vertices, ::nicxlive::core::nodes::GridDeformer* deformable,
    ::nicxlive::core::nodes::Deformation deform, bool flipHorz) {
    using ::nicxlive::core::math::Vec2;
    using ::nicxlive::core::math::Vec2Array;
    using ::nicxlive::core::nodes::Deformation;
    [[maybe_unused]] const auto receiverLifetime = deformable ? deformable->weak_from_this().lock() :
        std::shared_ptr<::nicxlive::core::nodes::Node>{};
    auto grid = compat::ngRequireObject(deformable)->getVertices().dup();
    auto offsets = deform.vertexOffsets.dup();
    const auto zeros = [&] {
        Vec2Array zeroOffsets(vertices.size());
        for (std::size_t i = 0; i < zeroOffsets.size(); ++i) zeroOffsets[i] = Vec2{0, 0};
        auto result = detail::resampling::allocateDeformation();
        result->vertexOffsets = std::move(zeroOffsets);
        return result;
    };
    if (grid.size() < 4 || offsets.size() != grid.size()) return zeros();
    compat::Slice<float> xsRaw, ysRaw;
    xsRaw.resize(grid.size(), std::numeric_limits<float>::quiet_NaN());
    ysRaw.resize(grid.size(), std::numeric_limits<float>::quiet_NaN());
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const Vec2 point = grid[i];
        xsRaw[i] = point.x;
        ysRaw[i] = point.y;
    }
    ::nicxlive::core::nodes::grid_axis_detail::sort({xsRaw.data(), xsRaw.size()});
    ::nicxlive::core::nodes::grid_axis_detail::sort({ysRaw.data(), ysRaw.size()});
    const auto uniqueArray = [](compat::Slice<float> sorted) {
        compat::Slice<float> result;
        std::size_t i = 0;
        while (i < sorted.size()) {
            const auto last = sorted[i];
            result.append(last);
            do { ++i; } while (i < sorted.size() && last == sorted[i]);
        }
        return result;
    };
    const auto xs = uniqueArray(xsRaw);
    const auto ys = uniqueArray(ysRaw);
    const auto columns = xs.size(), rows = ys.size();
    if (columns < 2 || rows < 2 || columns * rows != grid.size()) return zeros();
    Vec2Array samplePositions(vertices.size());
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        Vec2 point = vertices[i];
        if (flipHorz) point.x = -point.x;
        samplePositions[i] = point;
    }
    auto result = detail::resampling::allocateDeformation();
    result->vertexOffsets.setLength(vertices.size());
    for (std::size_t i = 0; i < samplePositions.size(); ++i) {
        const Vec2 query = samplePositions[i];
        std::size_t cellX, cellY;
        float u, v;
        if (!detail::grid_deformation::locateInterval({xs.data(), xs.size()}, query.x, cellX, u) ||
            !detail::grid_deformation::locateInterval({ys.data(), ys.size()}, query.y, cellY, v)) {
            result->vertexOffsets[i] = Vec2{0, 0};
            continue;
        }
        const auto idx00 = cellY * columns + cellX, idx10 = cellY * columns + cellX + 1;
        const auto idx01 = (cellY + 1) * columns + cellX, idx11 = (cellY + 1) * columns + cellX + 1;
        const auto posed = [&](std::size_t index) {
            const Vec2 point = grid[index], offset = offsets[index];
            return Vec2{point.x + offset.x, point.y + offset.y};
        };
        const auto def00 = posed(idx00), def10 = posed(idx10), def01 = posed(idx01), def11 = posed(idx11);
        auto defPos = detail::grid_deformation::bilinear(def00, def10, def01, def11, u, v);
        if (flipHorz) defPos.x = -defPos.x;
        const Vec2 original = vertices[i];
        result->vertexOffsets[i] = Vec2{defPos.x - original.x, defPos.y - original.y};
    }
    return result;
}
} // namespace nijigenerate::core::math::triangle
