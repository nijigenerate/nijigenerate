// Original tests/deformation_interpolation.d; existing cases only, no execution on import.
#include "deformation_interpolation.hpp"
#include <nijigenerate/compat/phobos_monotime.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/typed_format_vec2.hpp>
#include <compat/assert_error.hpp>
#include <compat/phobos_stdio.hpp>
#include <compat/source_allocator.hpp>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace deformation_interpolation {
namespace {
namespace core = nicxlive::core;
namespace runtime = core::d_runtime;
namespace format = nijigenerate::core::translation;
using core::math::Vec2;
using core::nodes::Deformation;
using core::param::Vec2u;
using Binding = core::param::DeformationParameterBinding;
const auto sourceFile = runtime::RetainedString::fromStaticLiteral("tests/deformation_interpolation.d");

void sourceAssert(bool condition, std::size_t line) {
    if (!condition) throw runtime::ngStaticError<runtime::AssertError>(runtime::RetainedString{}, sourceFile, line);
}
bool equal(Vec2 left, Vec2 right) { return left.x == right.x && left.y == right.y; }

void interpolationAssert(bool condition, bool twoDimensions, Vec2u left, Vec2 offset,
        std::size_t vertex, const core::math::Vec2Array& actual, const core::math::Vec2Array& expected) {
    if (condition) return;
    // Vector!(uint,2).toString formats its genuine two-element component array,
    // just as the canonical Vec2 format producer does for float components.
    const nijigenerate::compat::Slice<std::uint32_t> leftComponents{left.x, left.y};
    const auto actualPoint = actual[vertex].toVector();
    const auto expectedPoint = expected[vertex].toVector();
    const std::array arguments{format::ngFormatArgument(twoDimensions), format::ngFormatArgument(leftComponents),
        format::ngFormatArgument(offset), format::ngFormatArgument(vertex), format::ngFormatArgument(actualPoint),
        format::ngFormatArgument(expectedPoint)};
    auto message = format::detail::retainFormatMessage(format::ngFormatTyped(
        "2D=%s left=%s offset=%s vertex=%s actual=%s expected=%s", arguments));
    throw runtime::ngStaticError<runtime::AssertError>(std::move(message), sourceFile, 81);
}
}

void benchmarkLinear(std::shared_ptr<Binding> binding, bool direct) {
    constexpr std::size_t iterations = 2000;
    for (int trial = 0; trial < 5; ++trial) {
        // No collector or GC-counter stand-in: that measurement is unavailable.
        const auto start = nijigenerate::compat::SourceMonotonicClock::now();
        float checksum = 0.0f;
        for (std::size_t i = 0; i < iterations; ++i) {
            const Vec2 offset{static_cast<float>(i % 101) / 100.0f, static_cast<float>(i % 79) / 78.0f};
            // Keep the genuine result representation of each production API;
            // a bridge conversion here would add a benchmark-only deep copy.
            if (direct) {
                auto value = runtime::ngRequireObject(binding)->interpolateLinear(Vec2u{1, 1}, offset);
                checksum += value.vertexOffsets[i % 512].toVector().x;
            } else {
                auto value = legacyLinear(binding, Vec2u{1, 1}, offset);
                checksum += value.vertexOffsets[i % 512].toVector().x;
            }
        }
        const auto stop = nijigenerate::compat::SourceMonotonicClock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();
        const std::array arguments{format::ngFormatArgument(direct ? std::string_view("direct") :
            std::string_view("legacy")), format::ngFormatArgument(trial), format::ngFormatArgument(elapsed),
            format::ngFormatArgument(checksum)};
        runtime::phobos::stdio::standardOutput().writeln(format::ngFormatTyped(
            "%s trial=%s total_us=%s allocated=unavailable checksum=%s", arguments));
    }
}

Deformation legacyLinear(std::shared_ptr<Binding> binding, Vec2u left, Vec2 offset) {
    Deformation p0, p1;
    if (runtime::ngRequireObject(runtime::ngRequireObject(binding)->parameter)->isVec2) {
        Deformation p00(binding->values[left.x][left.y]);
        Deformation p01(binding->values[left.x][left.y + 1]);
        Deformation p10(binding->values[left.x + 1][left.y]);
        Deformation p11(binding->values[left.x + 1][left.y + 1]);
        p0 = core::param::bindingLerpValue<Deformation>(p00, p01, offset.y);
        p1 = core::param::bindingLerpValue<Deformation>(p10, p11, offset.y);
    } else {
        p0 = Deformation(binding->values[left.x][0]);
        p1 = Deformation(binding->values[left.x + 1][0]);
    }
    return core::param::bindingLerpValue<Deformation>(p0, p1, offset.x);
}

void ngDeformationInterpolationMain() {
    auto parameter = std::allocate_shared<core::param::Parameter>(runtime::SourceAllocator<core::param::Parameter>{});
    auto binding = core::param::ParameterBindingImpl<core::param::DeformSlot>::createSource<Binding>(parameter);
    binding->values.resize(4);
    for (std::size_t x = 0; x < 4; ++x) binding->values[x].resize(3);
    for (std::size_t x = 0; x < 4; ++x) for (std::size_t y = 0; y < 3; ++y) {
        binding->values[x][y].vertexOffsets.resize(512);
        for (std::size_t i = 0; i < 512; ++i)
            binding->values[x][y].vertexOffsets[i] = Vec2{
                static_cast<float>(x * 19.31 - y * 5.71 + i * 0.019),
                static_cast<float>(x * -8.93 + y * 11.71 - i * 0.023)};
    }
    auto original = binding->values[0][0];
    auto originalPoint = original.vertexOffsets[0].toVector();
    Deformation assigned;
    assigned = Deformation(original);
    assigned.vertexOffsets[0] = Vec2{123, 123};
    sourceAssert(equal(original.vertexOffsets[0].toVector(), originalPoint), 66);
    auto supplied = binding->values[1][0].vertexOffsets.dup();
    assigned.update(supplied);
    auto suppliedPoint = supplied[0].toVector();
    assigned.vertexOffsets[0] = Vec2{456, 456};
    sourceAssert(equal(supplied[0].toVector(), suppliedPoint), 71);
    for (const bool twoDimensions : {false, true}) {
        parameter->isVec2 = twoDimensions;
        for (std::size_t x = 0; x < 3; ++x) for (std::size_t y = 0; y < 2; ++y) {
            for (const float xt : {-0.2f, 0.0f, 0.17f, 0.5f, 0.93f, 1.0f, 1.3f})
                for (const float yt : {0.0f, 0.23f, 0.5f, 1.0f}) {
                    auto expected = legacyLinear(binding, Vec2u{x, y}, Vec2{xt, yt});
                    auto actual = binding->interpolateLinear(Vec2u{x, y}, Vec2{xt, yt});
                    for (std::size_t i = 0; i < 512; ++i) {
                        const auto actualPoint = actual.vertexOffsets[i].toVector();
                        const auto expectedPoint = expected.vertexOffsets[i].toVector();
                        interpolationAssert(equal(actualPoint, expectedPoint),
                            twoDimensions, Vec2u{x, y}, Vec2{xt, yt}, i, actual.vertexOffsets, expected.vertexOffsets);
                    }
                    auto source = binding->values[x][0].vertexOffsets[0].toVector();
                    auto retained = actual;
                    actual.vertexOffsets[0] = Vec2{777, 777};
                    const auto retainedPoint = retained.vertexOffsets[0].toVector();
                    const auto expectedPoint = expected.vertexOffsets[0].toVector();
                    sourceAssert(equal(retainedPoint, expectedPoint), 89);
                    sourceAssert(equal(binding->values[x][0].vertexOffsets[0].toVector(), source), 90);
                }
        }
    }
    parameter->isVec2 = true;
    auto expected = legacyLinear(binding, Vec2u{1, 1}, Vec2{0.23f, 0.71f});
    auto actual = binding->interpolateLinear(Vec2u{1, 1}, Vec2{0.23f, 0.71f});
    // Source101's GC allocatedInCurrentThread ratio is intentionally unavailable.
    // No zero, live-heap delta or allocator-hook result is substituted for it.
    const auto actualPoint = actual.vertexOffsets[3].toVector();
    const auto expectedPoint = expected.vertexOffsets[3].toVector();
    sourceAssert(equal(actualPoint, expectedPoint), 102);
    runtime::phobos::stdio::standardOutput().writeln(
        "Exact interpolation and ownership passed; allocations unavailable "
        "(GC allocation metrics and ratio assertion excluded by no-GC policy)");
    benchmarkLinear(binding, false);
    benchmarkLinear(binding, true);
}
}
