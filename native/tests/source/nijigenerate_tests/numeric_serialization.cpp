// Original: source/nijigenerate_tests/numeric_serialization.d (BSD-2-Clause).
#include "numeric_serialization.hpp"
#include <compat/assert_error.hpp>
#include <compat/phobos_stdio.hpp>
#include <fmt/serialize.hpp>
#include <load_diagnostics.hpp>
#include <math/serialization.hpp>
#include <param/binding_impl.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

namespace nijigenerate_tests::numeric_serialization {
namespace {
namespace core = nicxlive::core;
namespace math = core::math;
namespace serde = core::serde;
namespace runtime = core::d_runtime;
using math::Vec2;
using math::Vec2Array;

void sourceAssert(bool condition, std::size_t line) {
    if (!condition) throw runtime::ngStaticError<runtime::AssertError>(
        runtime::RetainedString::fromStaticLiteral("Assertion failure"),
        runtime::RetainedString::fromStaticLiteral("source/nijigenerate_tests/numeric_serialization.d"), line);
}

// Lower the original vector equality without introducing another vector type.
bool equal(Vec2 left, Vec2 right) { return left.x == right.x && left.y == right.y; }

float deserializeFloat(std::string_view json) {
    const auto document = serde::parseTypedJson(json);
    float value = std::numeric_limits<float>::quiet_NaN();
    if (auto error = serde::deserializeValue(document.root(), value)) throw *error.object();
    return value;
}
}

void ngTestNumericSerialization() {
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto inf = std::numeric_limits<float>::infinity();
    auto mesh = Vec2Array{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}, Vec2{10, 10}, Vec2{5, 5}};
    auto field = Vec2Array{Vec2{1, 2}, Vec2{21, 12}, Vec2{31, -8}, Vec2{51, 2}, Vec2{0, 99}};
    sourceAssert(math::inRecoverDeformationOffsets(mesh, field,
        core::common::SharedArray<std::uint8_t>{0, 0, 0, 0, 1}) == 1, 15);
    sourceAssert(equal(field[4].toVector(), Vec2{26, 99}), 16);
    sourceAssert(equal(field[0].toVector(), Vec2{1, 2}) && equal(field[3].toVector(), Vec2{51, 2}), 17);
    field[4] = Vec2{0, 0};
    sourceAssert(math::inRecoverDeformationOffsets(mesh, field,
        core::common::SharedArray<std::uint8_t>{0, 0, 0, 0, 3}) == 2, 19);
    sourceAssert(equal(field[4].toVector(), Vec2{26, 2}), 20);
    sourceAssert(core::fmt::inToJson(nan) == R"("nan")", 21);
    sourceAssert(core::fmt::inToJson(inf) == R"("+inf")", 22);
    sourceAssert(core::fmt::inToJson(-inf) == R"("-inf")", 23);
    sourceAssert(core::fmt::inToJson(core::common::SharedArray<float>{nan, 1.25f}) == R"(["nan",1.25])", 24);
    sourceAssert(core::fmt::inToJson(runtime::RetainedString::fromStaticLiteral(R"("nan")")) ==
        R"("\"nan\"")", 25);
    sourceAssert(std::isnan(deserializeFloat(core::fmt::inToJson(nan))), 26);
    Vec2 optional{1, 2};
    sourceAssert(!math::deserialize(optional, serde::TypedJsonView{}) && equal(optional, Vec2{1, 2}), 28);
    sourceAssert(!math::deserialize(optional, serde::parseTypedJson("null").root()) &&
        equal(optional, Vec2{1, 2}), 29);
    const std::array tokens{R"("nan")", R"("\"nan\"")", "null"};
    for (const auto token : tokens) {
        float value = nan;
        sourceAssert(!serde::deserializeNumber(serde::parseTypedJson(token).root(), value) && std::isnan(value), 32);
    }
    float infinity = nan;
    sourceAssert(!serde::deserializeNumber(serde::parseTypedJson(R"("\"-inf\"")").root(), infinity), 35);
    sourceAssert(std::isinf(infinity) && infinity < 0, 36);
    auto parameter = core::param::Parameter::createSourceDeferred(
        [] { return core::common::NullableString::fromStaticLiteral("RoundTrip"); }, [] { return false; });
    parameter->axisPoints = {core::common::SharedArray<float>{0.0f}, core::common::SharedArray<float>{0.0f}};
    auto binding = core::param::ParameterBindingImpl<core::param::DeformSlot>::
        createSource<core::param::DeformationParameterBinding>(parameter);
    auto data = serde::parseTypedJson(
        R"({"node":123,"param_name":"deform","values":[[[["\"nan\"",2],[3,"inf"]]]],"isSet":[[true]]})");
    sourceAssert(!binding->deserializeFromFghj(data.root()), 41);
    auto offsets = binding->getValue(core::param::Vec2u{0}).vertexOffsets;
    sourceAssert(offsets.size() == 2 && equal(offsets[0].toVector(), Vec2{0, 2}) &&
        equal(offsets[1].toVector(), Vec2{3, 0}), 43);
    sourceAssert(serde::inLastLoadDiagnostics.recoveredComponents == 2, 44);
    sourceAssert(serde::inLastLoadDiagnostics.warnings[0].find("RoundTrip") != std::string::npos, 45);
    offsets[0] = Vec2{nan, 2};
    binding->values[0][0].vertexOffsets = offsets;
    bool rejected = false;
    try {
        serde::TypedJsonWriter serializer;
        (void)binding->serializeSelf(serializer);
    } catch (const runtime::ThrowableException& error) {
        rejected = error.msg.view().find("vertex 0") != std::string_view::npos &&
            error.msg.view().find("RoundTrip") != std::string_view::npos;
    }
    sourceAssert(rejected, 56);
    bool contextual = false;
    try {
        (void)binding->deserializeFromFghj(serde::parseTypedJson(
            R"({"node":123,"param_name":"deform","values":[[[["broken",2]]]],"isSet":[[true]]})").root());
    } catch (const runtime::ThrowableException& error) {
        contextual = error.msg.view().find("RoundTrip") != std::string_view::npos &&
            error.msg.view().find("[0,0]") != std::string_view::npos;
    }
    sourceAssert(contextual, 64);
    contextual = false;
    try {
        (void)binding->deserializeFromFghj(serde::parseTypedJson(
            R"({"node":123,"param_name":"deform","values":[[[[1]]]],"isSet":[[true]]})").root());
    } catch (const runtime::ThrowableException& error) {
        contextual = error.msg.view().find("vertex 0") != std::string_view::npos &&
            error.msg.view().find("RoundTrip") != std::string_view::npos;
    }
    sourceAssert(contextual, 72);
    runtime::phobos::stdio::standardOutput().writeln(
        "PASS: numeric serialization, nested arrays, legacy tokens, deformation recovery, contextual save/load errors");
}

void ngNumericSerializationUnittest() { ngTestNumericSerialization(); }
}
