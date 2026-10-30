#include "triangulate.hpp"
#include "detail/triangulation_split_edges.hpp"
#include "vertex.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <new>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace nijigenerate::core::math::triangle {
namespace {
template<class VertexReader>
Triangulation triangulateVertices(std::size_t vertexCount, const VertexReader& readVertex,
    ::nicxlive::core::math::Vec4 bounds) {
    using ::nicxlive::core::math::Vec2;
    using ::nicxlive::core::math::Vec2Array;
    using namespace detail::triangulation;
    const auto tupleText = [](auto v) {
        std::string text = "[" + std::to_string(v.x) + ", " + std::to_string(v.y);
        if constexpr (requires { v.z; }) text += ", " + std::to_string(v.z);
        return text + "]";
    };
    const auto logTriangle = [&](const char* prefix, std::uint32_t index, auto tri, auto edges) {
        if constexpr (defaultDelaunayDiagnostics) diagnostics::writeLine(std::string(prefix) +
            std::to_string(index) + ": " + tupleText(tri) + " Edges: " + tupleText(edges));
    };
    const auto logEdge = [&](const char* prefix, std::uint32_t index, auto triangles) {
        if constexpr (defaultDelaunayDiagnostics) diagnostics::writeLine(std::string(prefix) +
            std::to_string(index) + ": Tris " + tupleText(triangles));
    };
    Vec2 low{bounds.x, bounds.y}, high{bounds.z, bounds.w};
    const Vec2 range{high.x - low.x, high.y - low.y};
    const Vec2 lowPad{(range.x + range.y) + 0.123f, (range.y + range.x) + 0.125f};
    const Vec2 highPad{(range.x + range.y) + 0.127f, (range.y + range.x) + 0.129f};
    low = {low.x - lowPad.x, low.y - lowPad.y};
    high = {high.x + highPad.x, high.y + highPad.y};
    compat::Slice<compat::Vec3u> triangles{{0, 1, 3}, {1, 2, 3}};
    compat::Slice<compat::Vec3u> triangleEdges{{0, 1, 2}, {3, 4, 1}};
    compat::Slice<compat::Vec2u> edgeTriangles{{0, 0}, {0, 1}, {0, 0}, {1, 1}, {1, 1}};
    Vec2Array points{{low.x, low.y}, {low.x, high.y}, {high.x, high.y}, {high.x, low.y}};
    for (int repeat = 0; repeat < 4; ++repeat) splitEdges(triangles, triangleEdges, edgeTriangles, points);
    const auto dropVertices = static_cast<std::uint32_t>(points.size());
    const auto readPoint = [&](std::uint32_t index) {
        return static_cast<Vec2>(points[index]);
    };
    for (std::size_t originalIndex = 0; originalIndex < vertexCount; ++originalIndex) {
        auto sourceVertex = readVertex(originalIndex);
        const auto index = static_cast<std::uint32_t>(originalIndex) + dropVertices;
        if constexpr (defaultDelaunayDiagnostics) {
            const auto logPosition = vertex::position(sourceVertex);
            diagnostics::writeLine("Add @" + std::to_string(index) + ": " + diagnostics::vectorText(logPosition));
        }
        points.push_back(vertex::position(sourceVertex));
        bool found = false;
        compat::Slice<std::uint32_t> affectedEdges;
        const auto searchTriangles = triangles;
        for (std::size_t triangleIndex = 0; triangleIndex < searchTriangles.size(); ++triangleIndex) {
            const auto triangle = searchTriangles[triangleIndex];
            if (!contains(triangle, vertex::position(sourceVertex), points)) continue;
            const auto a = static_cast<std::uint32_t>(triangleIndex);
            const auto b = static_cast<std::uint32_t>(triangles.size()), c = b + std::uint32_t{1};
            triangles[a] = {triangle.x, triangle.y, index};
            triangles.append(compat::Vec3u{triangle.y, triangle.z, index});
            triangles.append(compat::Vec3u{triangle.z, triangle.x, index});
            if constexpr (defaultDelaunayDiagnostics)
                logTriangle("*** Tri ", a, triangles[a], triangleEdges[a]);
            const auto p = static_cast<std::uint32_t>(edgeTriangles.size());
            const auto q = p + std::uint32_t{1}, r = q + std::uint32_t{1};
            const auto x = triangleEdges[a].x, y = triangleEdges[a].y, z = triangleEdges[a].z;
            triangleEdges[a] = {x, q, p};
            triangleEdges.append(compat::Vec3u{y, r, q});
            triangleEdges.append(compat::Vec3u{z, p, r});
            if constexpr (defaultDelaunayDiagnostics) {
                logTriangle("  * Tri a ", a, triangles[a], triangleEdges[a]);
                logTriangle("  + Tri b ", b, triangles[b], triangleEdges[b]);
                logTriangle("  + Tri c ", c, triangles[c], triangleEdges[c]);
            }
            edgeTriangles.append(compat::Vec2u{c, a});
            edgeTriangles.append(compat::Vec2u{a, b});
            edgeTriangles.append(compat::Vec2u{b, c});
            if constexpr (defaultDelaunayDiagnostics) {
                logEdge("  + Edg p ", p, edgeTriangles[p]);
                logEdge("  + Edg q ", q, edgeTriangles[q]);
                logEdge("  + Edg r ", r, edgeTriangles[r]);
                logEdge("  - Edg y ", y, edgeTriangles[y]);
            }
            replaceE2T(edgeTriangles[y], a, b);
            if constexpr (defaultDelaunayDiagnostics) {
                logEdge("  + Edg y ", y, edgeTriangles[y]);
                logEdge("  - Edg z ", y, edgeTriangles[z]);
            }
            replaceE2T(edgeTriangles[z], a, c);
            if constexpr (defaultDelaunayDiagnostics)
                logEdge("  + Edg z ", z, edgeTriangles[z]);
            affectedEdges.append(compat::Slice<std::uint32_t>{x, y, z, p, q, r});
            found = true;
            break;
        }
        if (!found) {
            if constexpr (defaultDelaunayDiagnostics) diagnostics::writeLine("FAILED!");
            break;
        }
        // Private bool[] is a zero-initialized byte array; no proxy references escape.
        std::vector<unsigned char> checked;
        try {
            checked.resize(edgeTriangles.size(), 0);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        const auto checkedAt = [&](std::size_t index, std::size_t line) -> unsigned char& {
            if (index >= checked.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, checked.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/triangle.d"), line);
            return checked[index];
        };
        for (std::uint32_t queue = 0; queue < affectedEdges.size(); ++queue) {
            const auto edge = affectedEdges[queue];
            const auto adjacent = edgeTriangles[edge];
            if constexpr (defaultDelaunayDiagnostics) diagnostics::writeLine(" ## Edge " +
                std::to_string(edge) + ": T " + tupleText(adjacent) + ": " +
                tupleText(triangles[adjacent.x]) + " " + tupleText(triangles[adjacent.y]));
            if (adjacent.x == adjacent.y) {
                if constexpr (defaultDelaunayDiagnostics) diagnostics::writeLine("  + Outer edge");
                continue;
            }
            orientTri(adjacent.x, edge, triangles, triangleEdges);
            orientTri(adjacent.y, edge, triangles, triangleEdges);
#ifndef NDEBUG
            if (triangles[adjacent.x].x != triangles[adjacent.y].y)
                throw compat::ngStaticError<compat::AssertError>("triangles do not share edge",
                    "source/nijigenerate/core/math/triangle.d", 651);
            if (triangles[adjacent.y].x != triangles[adjacent.x].y)
                throw compat::ngStaticError<compat::AssertError>("triangles do not share edge",
                    "source/nijigenerate/core/math/triangle.d", 652);
#endif
            const auto a = triangles[adjacent.x].x, c = triangles[adjacent.x].y;
            const auto d = triangles[adjacent.x].z, b = triangles[adjacent.y].z;
            const auto pb = readPoint(b), pa = readPoint(a), pc = readPoint(c), pd = readPoint(d);
            if (!inCircle(pb, pa, pc, pd)) {
                if constexpr (defaultDelaunayDiagnostics) diagnostics::writeLine("  + Meets condition");
                continue;
            }
            if constexpr (defaultDelaunayDiagnostics) diagnostics::writeLine("  - Flip!");
            const auto r = triangleEdges[adjacent.x].y, s = triangleEdges[adjacent.x].z;
            const auto p = triangleEdges[adjacent.y].y, q = triangleEdges[adjacent.y].z;
            triangles[adjacent.x] = {d, b, c};
            // Actual inmath Vector!(uint,2) declares alias t = y.
            triangles[adjacent.y] = {b, d, a};
            triangleEdges[adjacent.x] = {edge, q, r};
            triangleEdges[adjacent.y] = {edge, s, p};
            replaceE2T(edgeTriangles[q], adjacent.y, adjacent.x);
            replaceE2T(edgeTriangles[s], adjacent.x, adjacent.y);
            checkedAt(edge, 691) = 1;
            if (!checkedAt(p, 694)) affectedEdges.append(p);
            if (!checkedAt(q, 695)) affectedEdges.append(q);
            if (!checkedAt(r, 696)) affectedEdges.append(r);
            if (!checkedAt(s, 697)) affectedEdges.append(s);
        }
    }
    compat::Slice<compat::Vec3u> filtered;
    for (const auto triangle : triangles)
        if (triangle.x >= dropVertices && triangle.y >= dropVertices && triangle.z >= dropVertices)
            filtered.append(triangle);
    triangles = filtered;
    if (dropVertices > 0 && dropVertices < points.size()) {
        Vec2Array trimmed(points.size() - dropVertices);
        for (std::size_t i = 0; i < trimmed.size(); ++i) {
            auto destination = trimmed[i];
            const Vec2 source = points[i + dropVertices];
            destination = source;
        }
        points = std::move(trimmed);
    }
    for (auto& triangle : triangles) {
        triangle.x -= dropVertices;
        triangle.y -= dropVertices;
        triangle.z -= dropVertices;
    }
    return {std::move(points), triangles};
}
} // namespace

Triangulation triangulate(compat::Slice<::nicxlive::core::math::Vec2> vertices,
    ::nicxlive::core::math::Vec4 bounds) {
    return triangulateVertices(vertices.size(), [&](std::size_t i) { return vertices[i]; }, bounds);
}

Triangulation triangulate(mesh::MeshVertexArray vertices, ::nicxlive::core::math::Vec4 bounds) {
    return triangulateVertices(vertices.size(), [&](std::size_t i) { return vertices[i]; }, bounds);
}

Triangulation triangulatePositions(std::size_t count,
    const std::function<::nicxlive::core::math::Vec2(std::size_t)>& position,
    ::nicxlive::core::math::Vec4 bounds) {
    return triangulateVertices(count, position, bounds);
}
} // namespace nijigenerate::core::math::triangle
