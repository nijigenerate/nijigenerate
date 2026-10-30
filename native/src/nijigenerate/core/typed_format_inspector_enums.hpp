#pragma once
#include "typed_format_enum.hpp"
#include <nodes/path_deformer.hpp>
#include <nodes/simple_physics_driver.hpp>
#include <array>

namespace nijigenerate::core::translation {
template<class NativeTag> struct PhysicsStringEnumMember {
    NativeTag value;
    std::string_view name;
    std::string_view stringValue;
};
template<> struct FormatEnumTraits<nicxlive::core::nodes::SourcePhysicsModel> {
    using NativeTag = nicxlive::core::nodes::PhysicsModel;
    inline static constexpr std::string_view name = "PhysicsModel";
    inline static constexpr std::array members{
        PhysicsStringEnumMember{NativeTag::Pendulum, "Pendulum", "pendulum"},
        PhysicsStringEnumMember{NativeTag::SpringPendulum, "SpringPendulum", "spring_pendulum"}};
};
template<> struct FormatEnumTraits<nicxlive::core::nodes::SourceParamMapMode> {
    using NativeTag = nicxlive::core::nodes::ParamMapMode;
    inline static constexpr std::string_view name = "ParamMapMode";
    inline static constexpr std::array members{
        PhysicsStringEnumMember{NativeTag::AngleLength, "AngleLength", "angle_length"},
        PhysicsStringEnumMember{NativeTag::XY, "XY", "xy"},
        PhysicsStringEnumMember{NativeTag::LengthAngle, "LengthAngle", "length_angle"},
        PhysicsStringEnumMember{NativeTag::YX, "YX", "yx"}};
};

// A string-backed D enum carries its real descriptor through the same enum
// writer. Native integer tags below remain separate compatibility declarations.
template<> struct FormatStringEnumTraits<nicxlive::core::nodes::SourcePhysicsModel> {
    using Value = nicxlive::core::nodes::SourcePhysicsModel;
    static auto raw(const Value& value) { return value.raw(); }
    static auto typeName() { return Value::sourceTypeName(); }
    static auto memberName(const Value& value) { return value.memberName(); }
};
template<> struct FormatStringEnumTraits<nicxlive::core::nodes::SourceParamMapMode> {
    using Value = nicxlive::core::nodes::SourceParamMapMode;
    static auto raw(const Value& value) { return value.raw(); }
    static auto typeName() { return Value::sourceTypeName(); }
    static auto memberName(const Value& value) { return value.memberName(); }
};

// Actual enums returned by M170 SharedValue descriptors in pathdeform.d and
// simplephysics.d. Metadata describes the genuine canonical enum declarations;
// this does not accept any unrelated enum with a similar member spelling.
template<> struct FormatEnumTraits<nicxlive::core::nodes::PathDeformer::CurveType> {
    using Value = nicxlive::core::nodes::PathDeformer::CurveType;
    inline static constexpr std::string_view name = "CurveType";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::Bezier, "Bezier"},
        FormatEnumMember<Value>{Value::Spline, "Spline"}};
};
template<> struct FormatEnumTraits<nicxlive::core::nodes::PhysicsModel> {
    using Value = nicxlive::core::nodes::PhysicsModel;
    inline static constexpr std::string_view name = "PhysicsModel";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::Pendulum, "Pendulum"},
        FormatEnumMember<Value>{Value::SpringPendulum, "SpringPendulum"}};
};
template<> struct FormatEnumTraits<nicxlive::core::nodes::ParamMapMode> {
    using Value = nicxlive::core::nodes::ParamMapMode;
    inline static constexpr std::string_view name = "ParamMapMode";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::AngleLength, "AngleLength"},
        FormatEnumMember<Value>{Value::XY, "XY"},
        FormatEnumMember<Value>{Value::LengthAngle, "LengthAngle"},
        FormatEnumMember<Value>{Value::YX, "YX"}};
};
} // namespace nijigenerate::core::translation
