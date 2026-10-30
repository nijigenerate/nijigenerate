#pragma once

#include <param/parameter.hpp>
#include <param/binding_impl.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>

#include <cstdint>
#include <limits>

// Implementation-only helpers for the real AutoFlip body. This header is not
// part of the public transform interface or a replacement binding/runtime.
namespace nijigenerate::utils::transform::detail {

// The original local vec2u components are uint32, independently of the native
// runtime's size_t transport. Preserve that value boundary after out writes.
inline nicxlive::core::param::Vec2u sourceIndexValue(nicxlive::core::param::Vec2u value) noexcept {
    return {static_cast<std::uint32_t>(value.x), static_cast<std::uint32_t>(value.y)};
}

template<typename T>
T interpolateValueAt(nicxlive::core::param::ParameterBindingImpl<T>* binding,
        nicxlive::core::param::Vec2u index, [[maybe_unused]] std::int32_t axis) {
    auto* receiver = compat::ngRequireObject(binding);
    const auto bindingLifetime = receiver->weak_from_this().lock();
    nicxlive::core::param::Vec2 offset;
    {
        const auto parameterLifetime = receiver->getParameter();
        offset = compat::ngRequireObject(receiver->parameter)->getKeypointOffset(index);
    }
    nicxlive::core::param::Vec2u sourceIndex{};
    nicxlive::core::param::Vec2 subOffset{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    {
        const auto parameterLifetime = receiver->getParameter();
        compat::ngRequireObject(receiver->parameter)->findOffset(offset, sourceIndex, subOffset);
    }
    sourceIndex = sourceIndexValue(sourceIndex);
    return receiver->interpolate(sourceIndex, subOffset);
}

} // namespace nijigenerate::utils::transform::detail
