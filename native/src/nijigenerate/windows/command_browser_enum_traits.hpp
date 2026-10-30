#pragma once
#include "command_browser_argument_parser.hpp"
#include <nijigenerate/io/depthmap_psd_types.hpp>
#include <common/utils.hpp>
#include <nodes/part.hpp>
#include <nodes/path_deformer.hpp>
#include <nodes/simple_physics_driver.hpp>
#include <nijigenerate/core/typed_format_inspector_enums.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/enums.hpp>
#include <array>

namespace nijigenerate::windows::command_browser::detail {
// Original core/nodes/common.d:171; the native-only Additive alias is not a source member.
template<> struct BrowserEnumTraits<nicxlive::core::nodes::BlendMode> {
    using E = nicxlive::core::nodes::BlendMode;
    static constexpr std::string_view typeName = "BlendMode";
    static constexpr std::array<std::pair<std::string_view,E>,19> members{{
        {"Normal", E::Normal}, {"Multiply", E::Multiply}, {"Screen", E::Screen},
        {"Overlay", E::Overlay}, {"Darken", E::Darken}, {"Lighten", E::Lighten},
        {"ColorDodge", E::ColorDodge}, {"LinearDodge", E::LinearDodge}, {"AddGlow", E::AddGlow},
        {"ColorBurn", E::ColorBurn}, {"HardLight", E::HardLight}, {"SoftLight", E::SoftLight},
        {"Difference", E::Difference}, {"Exclusion", E::Exclusion}, {"Subtract", E::Subtract},
        {"Inverse", E::Inverse}, {"DestinationIn", E::DestinationIn},
        {"ClipToLower", E::ClipToLower}, {"SliceFromLower", E::SliceFromLower}}};
};
// Original core/nodes/deformer/path.d:29, represented by the genuine nested native enum.
template<> struct BrowserEnumTraits<nicxlive::core::nodes::PathDeformer::CurveType> {
    using E = nicxlive::core::nodes::PathDeformer::CurveType;
    static constexpr std::string_view typeName = "CurveType";
    static constexpr std::array<std::pair<std::string_view,E>,2> members{{
        {"Bezier", E::Bezier}, {"Spline", E::Spline}}};
};
template<> struct BrowserEnumTraits<nicxlive::core::nodes::PhysicsModel> {
    using E = nicxlive::core::nodes::PhysicsModel;
    static constexpr bool sourceIntegral = false; // D base type is string.
    static constexpr std::string_view typeName = "PhysicsModel";
    static constexpr std::string_view descriptionChoices = "Pendulum=pendulum|SpringPendulum=spring_pendulum";
    static constexpr std::array<std::string_view,2> sourceLiterals{{
        "\"pendulum\"", "\"spring_pendulum\""}};
    static constexpr std::array<std::pair<std::string_view,E>,2> members{{
        {"Pendulum", E::Pendulum}, {"SpringPendulum", E::SpringPendulum}}};
};
template<> struct BrowserEnumTraits<nicxlive::core::nodes::ParamMapMode> {
    using E = nicxlive::core::nodes::ParamMapMode;
    static constexpr bool sourceIntegral = false; // D base type is string.
    static constexpr std::string_view typeName = "ParamMapMode";
    static constexpr std::string_view descriptionChoices = "AngleLength=angle_length|XY=xy|LengthAngle=length_angle|YX=yx";
    static constexpr std::array<std::string_view,4> sourceLiterals{{
        "\"angle_length\"", "\"xy\"", "\"length_angle\"", "\"yx\""}};
    static constexpr std::array<std::pair<std::string_view,E>,4> members{{
        {"AngleLength", E::AngleLength}, {"XY", E::XY}, {"LengthAngle", E::LengthAngle}, {"YX", E::YX}}};
};
template<> struct BrowserEnumTraits<nicxlive::core::nodes::SourcePhysicsModel>
    : BrowserEnumTraits<nicxlive::core::nodes::PhysicsModel> {};
template<> struct BrowserEnumTraits<nicxlive::core::nodes::SourceParamMapMode>
    : BrowserEnumTraits<nicxlive::core::nodes::ParamMapMode> {};

template<> struct BrowserEnumTraits<viewport::common::mesheditor::tools::enums::VertexToolMode> {
    using E = viewport::common::mesheditor::tools::enums::VertexToolMode;
    static constexpr std::string_view typeName = "VertexToolMode";
    static constexpr std::array<std::pair<std::string_view,E>,10> members{{
        {"Points", E::Points}, {"Connect", E::Connect}, {"PathDeform", E::PathDeform}, {"Grid", E::Grid},
        {"Brush", E::Brush}, {"BezierDeform", E::BezierDeform}, {"AltGridDeform", E::AltGridDeform},
        {"AltBezierDeform", E::AltBezierDeform}, {"LassoSelection", E::LassoSelection}, {"EdgeCutter", E::EdgeCutter}}};
};
template<> struct BrowserEnumTraits<nicxlive::core::common::InterpolateMode> {
    using E = nicxlive::core::common::InterpolateMode;
    static constexpr std::string_view typeName = "InterpolateMode";
    // The native provider spells value 2 Step; the source public name is Stepped.
    // COUNT is a declared source enum member, so reflection must expose it too.
    static constexpr std::array<std::pair<std::string_view,E>,6> members{{
        {"Nearest", E::Nearest}, {"Linear", E::Linear}, {"Stepped", E::Step},
        {"Cubic", E::Cubic}, {"Bezier", E::Bezier}, {"COUNT", static_cast<E>(5)}}};
};
template<> struct BrowserEnumTraits<nicxlive::core::nodes::MaskingMode> {
    using E = nicxlive::core::nodes::MaskingMode;
    static constexpr std::string_view typeName = "MaskingMode";
    static constexpr std::array<std::pair<std::string_view,E>,2> members{{
        {"Mask", E::Mask}, {"DodgeMask", E::DodgeMask}}};
};
template<> struct BrowserEnumTraits<io::depthmap_psd::PsdDepthChannel> {
    using E = io::depthmap_psd::PsdDepthChannel;
    static constexpr std::string_view typeName = "PsdDepthChannel";
    static constexpr std::array<std::pair<std::string_view,E>,5> members{{
        {"AverageRGB", E::AverageRGB}, {"R", E::R}, {"G", E::G}, {"B", E::B}, {"Luminance", E::Luminance}}};
};
template<> struct BrowserEnumTraits<io::depthmap_psd::PsdDepthMissingPolicy> {
    using E = io::depthmap_psd::PsdDepthMissingPolicy;
    static constexpr std::string_view typeName = "PsdDepthMissingPolicy";
    static constexpr std::array<std::pair<std::string_view,E>,4> members{{
        {"KeepExisting", E::KeepExisting}, {"SetZero", E::SetZero}, {"SetBack", E::SetBack}, {"SkipGrid", E::SkipGrid}}};
};
template<> struct BrowserEnumTraits<io::depthmap_psd::PsdDepthConvolution> {
    using E = io::depthmap_psd::PsdDepthConvolution;
    static constexpr std::string_view typeName = "PsdDepthConvolution";
    static constexpr std::array<std::pair<std::string_view,E>,13> members{{
        {"Nearest", E::Nearest}, {"Box3x3", E::Box3x3}, {"Box5x5", E::Box5x5},
        {"Gaussian3x3", E::Gaussian3x3}, {"Gaussian5x5", E::Gaussian5x5}, {"Median3x3", E::Median3x3},
        {"Frontmost3x3", E::Frontmost3x3}, {"Backmost3x3", E::Backmost3x3}, {"BoxCustom", E::BoxCustom},
        {"GaussianCustom", E::GaussianCustom}, {"MedianCustom", E::MedianCustom},
        {"FrontmostCustom", E::FrontmostCustom}, {"BackmostCustom", E::BackmostCustom}}};
};
}
