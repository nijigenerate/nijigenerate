#include "welding.hpp"
#include "homogeneous.hpp"
#include "detail/welding_sort.hpp"
#include <common/resource_key_map.hpp>
#include <compat/phobos_string_hash.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <functional>
#include <string_view>
#include <new>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace nijigenerate::core::math::welding {
namespace {
Vec2 subtract(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 scale(Vec2 a, float b) { return {a.x * b, a.y * b}; }
using Edge = std::array<std::size_t, 2>;
template<class Operation> auto allocateArray(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class T> Slice<T> duplicateArray(std::span<const T> values) {
    if (values.empty()) return {};
    return allocateArray([&] { return Slice<T>(values); });
}
template<class T> Slice<T> allocateValues(std::size_t count, T value = {}) {
    if (!count) return {};
    return allocateArray([&] { return Slice<T>(std::vector<T>(count, value)); });
}
template<class T> struct IntegralPairHash {
    std::size_t operator()(const std::array<T, 2>& key) const noexcept {
        static_assert(std::is_integral_v<T>);
        // Integral static arrays select hashOf's bitwise bytesHash path.
        return nicxlive::core::d_runtime::ngPhobosStringHash(std::string_view(
            reinterpret_cast<const char*>(key.data()), sizeof(T) * 2));
    }
};
template<class T, class Value> using PairMap = nicxlive::core::common::ResourceKeyMap<
    std::array<T, 2>, Value, IntegralPairHash<T>, std::equal_to<std::array<T, 2>>>;
// Avoid undefined float-to-integer conversion. LDC x64 returns the integer
// indefinite value for nonfinite/out-of-range coordinates; see contract.md.
std::int64_t quantize(float value) {
    const auto rounded = std::round(value * 10000);
    if (!std::isfinite(rounded) || rounded >= 9223372036854775808.0L ||
        rounded < -9223372036854775808.0L) return std::numeric_limits<std::int64_t>::min();
    return static_cast<std::int64_t>(rounded);
}
Slice<Edge> boundaryEdges(std::span<const Vec2> vertices, std::span<const std::uint16_t> indices) {
    PairMap<std::int64_t, std::size_t> canonical;
    auto ids = allocateValues<std::size_t>(vertices.size());
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const std::array<std::int64_t, 2> key{quantize(vertices[i].x), quantize(vertices[i].y)};
        const auto previous = canonical.find(key);
        if (previous != canonical.end()) ids[i] = previous->second;
        else { ids[i] = i; canonical[key] = i; }
    }
    PairMap<std::size_t, std::size_t> counts;
    PairMap<std::size_t, Edge> representatives;
    for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
        auto a = indices[t], b = indices[t + 1], c = indices[t + 2];
        if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) continue;
        auto ab = subtract(vertices[b], vertices[a]), ac = subtract(vertices[c], vertices[a]);
        if (std::abs(ab.x * ac.y - ab.y * ac.x) <= 0.0001f) continue;
        std::array<std::size_t, 3> triangle{a, b, c};
        for (std::size_t i = 0; i < 3; ++i) {
            auto x = triangle[i], y = triangle[(i + 1) % 3];
            Edge key = ids[x] < ids[y] ? Edge{ids[x], ids[y]} : Edge{ids[y], ids[x]};
            auto count = counts[key];
            ++static_cast<std::size_t&>(count);
            representatives[key] = Edge{x, y};
        }
    }
    Slice<Edge> result;
    for (auto [key, count] : counts) if (count == 1) result.append(representatives.at(key));
    const auto before = [](const Edge& a, const Edge& b) {
        return a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]);
    };
    static_assert(sizeof(std::size_t) == 4 || sizeof(std::size_t) == 8);
    if constexpr (sizeof(std::size_t) == 8) {
        detail::value_sort::sort<sizeof(std::size_t) * 2>(std::span<Edge>{result.data(), result.size()},
            before, "Failed to sort range of type ulong[2][]");
    } else {
        detail::value_sort::sort<sizeof(std::size_t) * 2>(std::span<Edge>{result.data(), result.size()},
            before, "Failed to sort range of type uint[2][]");
    }
    return result;
}
}
WeldingIndexPair ngRemapWeldingLinks(std::span<const std::ptrdiff_t> forward,
    std::span<const std::ptrdiff_t> reverse, std::span<const std::ptrdiff_t> oldToNew, std::size_t newCount) {
    WeldingIndexPair result;
    result.forward = allocateArray([&] {
        return nicxlive::core::common::SharedArray<std::ptrdiff_t>(newCount, 0);
    });
    result.reverse = allocateArray([&] {
        return nicxlive::core::common::SharedArray<std::ptrdiff_t>(reverse.size(), 0);
    });
    if(!result.forward.empty())std::fill(result.forward.begin(),result.forward.end(),std::ptrdiff_t{-1});
    if(!result.reverse.empty())std::fill(result.reverse.begin(),result.reverse.end(),std::ptrdiff_t{-1});
    for (std::size_t i = 0; i < forward.size(); ++i) {
        auto target = forward[i];
        if (i < oldToNew.size() && oldToNew[i] >= 0 && std::size_t(oldToNew[i]) < newCount &&
            target >= 0 && std::size_t(target) < reverse.size() && reverse[target] >= 0)
            result.forward[oldToNew[i]] = target;
    }
    for (std::size_t i = 0; i < reverse.size(); ++i) {
        auto source = reverse[i];
        if (source >= 0 && std::size_t(source) < oldToNew.size() && oldToNew[source] >= 0 &&
            std::size_t(oldToNew[source]) < newCount && std::size_t(source) < forward.size() && forward[source] >= 0)
            result.reverse[i] = oldToNew[source];
    }
    for (std::size_t i = 0; i < result.forward.size(); ++i) {
        auto target = result.forward[i];
        if (target >= 0 && result.reverse[target] < 0) result.reverse[target] = static_cast<std::ptrdiff_t>(i);
    }
    return result;
}
WeldingMeshRefinement ngRefineWeldingMesh(std::span<const Vec2> fineWorld,
    std::span<const std::uint16_t> fineIndices, std::span<const Vec2> coarseWorld,
    std::span<const Vec2> coarseLocal, std::span<const std::uint16_t> coarseIndices,
    Mat4 worldToCoarse, float distanceLimit) {
    WeldingMeshRefinement result{duplicateArray(coarseLocal), duplicateArray(coarseIndices), 0};
    auto world = duplicateArray(coarseWorld);
    Slice<bool> boundary;
    boundary.resize(fineWorld.size());
    auto spacing = allocateValues<float>(fineWorld.size(), std::numeric_limits<float>::quiet_NaN());
    for (auto& value : spacing) value = std::numeric_limits<float>::infinity();
    for (auto edge : boundaryEdges(fineWorld, fineIndices)) {
        boundary[edge[0]] = boundary[edge[1]] = true;
        auto delta = subtract(fineWorld[edge[1]], fineWorld[edge[0]]);
        auto length = delta.x * delta.x + delta.y * delta.y;
        for (auto index : edge) spacing[index] = std::min(spacing[index], length);
    }
    auto limit = distanceLimit * distanceLimit;
    Slice<bool> anchors;
    anchors.resize(coarseWorld.size());
    for (std::size_t j = 0; j < coarseWorld.size(); ++j) for (auto point : fineWorld) {
        auto delta = subtract(point, coarseWorld[j]);
        if (delta.x * delta.x + delta.y * delta.y < limit) { anchors[j] = true; break; }
    }
    auto insideCoarse = [&](Vec2 point) {
        for (std::size_t t = 0; t + 2 < coarseIndices.size(); t += 3) {
            auto a = coarseIndices[t], b = coarseIndices[t + 1], c = coarseIndices[t + 2];
            if (a >= coarseWorld.size() || b >= coarseWorld.size() || c >= coarseWorld.size()) continue;
            auto ab = subtract(coarseWorld[b], coarseWorld[a]), ac = subtract(coarseWorld[c], coarseWorld[a]);
            auto p = subtract(point, coarseWorld[a]);
            auto determinant = ab.x * ac.y - ab.y * ac.x;
            if (std::abs(determinant) <= 0.0001f) continue;
            auto u = (p.x * ac.y - p.y * ac.x) / determinant;
            auto v = (ab.x * p.y - ab.y * p.x) / determinant;
            if (u >= 0 && v >= 0 && u + v <= 1) return true;
        }
        return false;
    };
    for (std::size_t i = 0; i < fineWorld.size(); ++i) {
        auto point = fineWorld[i];
        if (!boundary[i] || !std::isfinite(point.x) || !std::isfinite(point.y)) continue;
        bool matched = false;
        for (auto existing : world) {
            auto delta = subtract(point, existing);
            if (delta.x * delta.x + delta.y * delta.y < limit) { matched = true; break; }
        }
        if (matched) continue;
        float reach = limit;
        if (std::isfinite(spacing[i]) && !insideCoarse(point)) {
            for (auto edge : boundaryEdges(coarseWorld, coarseIndices)) {
                if (!anchors[edge[0]] && !anchors[edge[1]]) continue;
                auto start = coarseWorld[edge[0]], delta = subtract(coarseWorld[edge[1]], start);
                auto length = delta.x * delta.x + delta.y * delta.y;
                if (length <= 0) continue;
                auto offset = subtract(point, start);
                auto t = std::min(1.f, std::max(0.f, (offset.x * delta.x + offset.y * delta.y) / length));
                auto error = subtract(point, add(start, scale(delta, t)));
                if (error.x * error.x + error.y * error.y < spacing[i]) {
                    reach = std::max(reach, spacing[i]);
                    break;
                }
            }
        }
        float nearest = reach;
        Edge selected{};
        bool found = false;
        for (auto edge : boundaryEdges(world, result.indices)) {
            auto start = world[edge[0]], delta = subtract(world[edge[1]], start);
            auto length = delta.x * delta.x + delta.y * delta.y;
            if (length <= 0) continue;
            auto offset = subtract(point, start);
            auto t = std::min(1.f, std::max(0.f, (offset.x * delta.x + offset.y * delta.y) / length));
            if (t <= 0.0001f || t >= 0.9999f) continue;
            auto error = subtract(point, add(start, scale(delta, t)));
            auto distance = error.x * error.x + error.y * error.y;
            if (distance < nearest) { nearest = distance; selected = edge; found = true; }
        }
        if (!found || result.vertices.size() >= std::numeric_limits<std::uint16_t>::max()) continue;
        auto vertex = static_cast<std::uint16_t>(result.vertices.size());
        for (std::size_t t = 0; t + 2 < result.indices.size(); t += 3) {
            bool split = false;
            for (std::size_t j = 0; j < 3; ++j) {
                auto a = result.indices[t + j], b = result.indices[t + (j + 1) % 3];
                if (!((a == selected[0] && b == selected[1]) || (a == selected[1] && b == selected[0]))) continue;
                auto c = result.indices[t + (j + 2) % 3];
                result.indices[t] = a; result.indices[t + 1] = vertex; result.indices[t + 2] = c;
                const auto added = allocateArray([&] { return Slice<std::uint16_t>{vertex, b, c}; });
                result.indices.append(added);
                split = true;
                break;
            }
            if (split) break;
        }
        world.append(point);
        const auto local = ::nijigenerate::core::math::ngTransformHomogeneousPoint(
            worldToCoarse, {point.x, point.y, 0.0f});
        result.vertices.append(Vec2{local.x, local.y});
        ++result.addedVertices;
    }
    return result;
}
Slice<std::ptrdiff_t> ngMatchWeldingVertices(std::span<const Vec2> source,
    std::span<const Vec2> target, std::optional<std::span<const std::ptrdiff_t>> previous, float distanceLimit) {
    struct Candidate {
        std::size_t source{};
        std::size_t target{};
        float distance{std::numeric_limits<float>::quiet_NaN()};
    };
    Slice<bool> allowed;
    allowed.resize(target.size());
    if (!previous) for (auto& value : allowed) value = true;
    if (previous) for (auto index : *previous) if (index >= 0 && std::size_t(index) < target.size()) allowed[index] = true;
    Slice<Candidate> candidates;
    for (std::size_t i = 0; i < source.size(); ++i) {
        auto a = source[i];
        if (!std::isfinite(a.x) || !std::isfinite(a.y)) continue;
        for (std::size_t j = 0; j < target.size(); ++j) {
            auto b = target[j];
            if (!allowed[j] || !std::isfinite(b.x) || !std::isfinite(b.y)) continue;
            auto delta = subtract(a, b);
            auto distance = delta.x * delta.x + delta.y * delta.y;
            if (distance < distanceLimit * distanceLimit) candidates.append({i, j, distance});
        }
    }
    constexpr auto sourceCandidateBytes = ((2 * sizeof(std::size_t) + sizeof(float)
        + sizeof(std::size_t) - 1) / sizeof(std::size_t)) * sizeof(std::size_t);
    detail::value_sort::sort<sourceCandidateBytes>(std::span<Candidate>{candidates.data(), candidates.size()},
        [](auto a, auto b) {
            return a.distance < b.distance || (a.distance == b.distance &&
                (a.source < b.source || (a.source == b.source && a.target < b.target)));
        }, "Failed to sort range of type Candidate[]");
    auto indices = allocateValues<std::ptrdiff_t>(source.size());
    for (auto& index : indices) index = -1;
    for (auto candidate : candidates) {
        if (indices[candidate.source] >= 0) continue;
        indices[candidate.source] = static_cast<std::ptrdiff_t>(candidate.target);
    }
    return indices;
}
}
