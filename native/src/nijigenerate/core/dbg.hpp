#pragma once
#include <math/veca.hpp>
#include <math/mat4.hpp>
#include <render/common.hpp>
#include <dbg.hpp>
#include <utility>
#include <memory>
#include <span>
#include <concepts>
#include <type_traits>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/array_access.hpp>

namespace nijigenerate::core::dbg {
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Vec4;
using nicxlive::core::math::Vec3Array;
using nicxlive::core::math::Mat4;
using nicxlive::core::RenderResourceHandle;
using nicxlive::core::dbg::inDbgDrawMeshOutlines;
using nicxlive::core::dbg::inDbgDrawMeshVertexPoints;
using nicxlive::core::dbg::inDbgDrawMeshOrientation;

// Keep the existing App class spelling and GL forward declaration valid.
class DebugProvider : public nicxlive::core::dbg::DebugProvider {
public:
    ~DebugProvider() override = default;
};
inline void ngInstallDebugProvider(std::shared_ptr<DebugProvider> provider,
    std::shared_ptr<nicxlive::core::RenderBackend> runtimeBackend) {
    nicxlive::core::dbg::ngInstallDebugProvider(std::move(provider), std::move(runtimeBackend));
}
using nicxlive::core::dbg::ngClearDebugProvider;
using nicxlive::core::dbg::inInitDebug;
using nicxlive::core::dbg::inDbgPointsSize;
using nicxlive::core::dbg::inDbgLineWidth;
using nicxlive::core::dbg::inDbgSetBuffer;
using nicxlive::core::dbg::inDbgDrawPoints;
using nicxlive::core::dbg::inDbgDrawLines;

// Project wrapper's exact mutable D input families. Existing runtime overloads
// above remain separate from this constrained project-template mapping.
template<class T>
concept ProjectDebugBuffer = std::same_as<std::remove_reference_t<T>, Vec3Array> ||
    std::same_as<std::remove_reference_t<T>, compat::Slice<Vec3>>;

template<ProjectDebugBuffer T>
void inDbgSetBuffer(T&& points) {
    if constexpr (std::same_as<std::remove_reference_t<T>, Vec3Array>) {
        // The original runtime value parameter retains its own descriptor.
        Vec3Array forwarded(points);
        // Qualifying the const view selects the existing non-template runtime API.
        inDbgSetBuffer(static_cast<const Vec3Array&>(forwarded));
    } else {
        const auto source = points;
        Vec3Array converted(source.size());
        for (std::size_t i = 0; i < source.size(); ++i) {
            const Vec3 point = compat::ngArrayAt(source, i);
            converted[i].set(point);
        }
        Vec3Array forwarded(converted);
        inDbgSetBuffer(static_cast<const Vec3Array&>(forwarded));
    }
}
}
