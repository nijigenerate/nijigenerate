#pragma once
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <math/veca.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cmath>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>

namespace nijigenerate::commands::vertex::define_mesh::detail {
using VertexInputError = compat::ThrowableException;
template<std::size_t N> [[noreturn]] void failInput(const char (&message)[N], std::size_t line) {
    const auto ownedMessage = [&] {
        try {
            return compat::RetainedString(std::string(message, N - 1));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    throw VertexInputError(ownedMessage,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/vertex/define_mesh.d"), line);
}
inline nicxlive::core::math::Vec2Array parseFlatVertices(compat::Slice<float> vertices) {
    if (vertices.isNull()) failInput("vertices not provided", 20);
    if (vertices.empty()) failInput("vertices empty", 21);
    if (vertices.size() % 2) failInput("vertices length must be even (x,y pairs)", 22);
    nicxlive::core::math::Vec2Array result;
    result.setLength(vertices.size() / 2);
    for (std::size_t i = 0; i < result.size(); ++i) {
        const float x = vertices[i * 2], y = vertices[i * 2 + 1];
        if (!std::isfinite(x) || !std::isfinite(y))
            failInput("vertices must contain finite x,y values", 29);
        result.set(i, nicxlive::core::math::Vec2{x, y});
    }
    return result;
}
inline nicxlive::core::math::Vec2Array makeGridVertices(
    compat::Slice<float> axisX, compat::Slice<float> axisY) {
    if (axisX.isNull() || axisY.isNull()) failInput("axisX or axisY not provided", 36);
    if (axisX.size() < 2) failInput("axisX must contain at least 2 values", 37);
    if (axisY.size() < 2) failInput("axisY must contain at least 2 values", 38);
    nicxlive::core::math::Vec2Array result;
    for (float y : axisY) {
        if (!std::isfinite(y)) failInput("axisY must contain finite values", 42);
        for (float x : axisX) {
            if (!std::isfinite(x)) failInput("axisX must contain finite values", 44);
            result.push_back(nicxlive::core::math::Vec2{x, y});
        }
    }
    return result;
}
}
