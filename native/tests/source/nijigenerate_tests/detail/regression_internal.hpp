#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <math/mat4.hpp>
#include <math/veca.hpp>
#include <nodes/part.hpp>

// Module-private declarations shared by translations of the existing original.
// This header is not part of the public regression module interface.
namespace nijigenerate_tests::regression::detail {
void require(bool condition, nijigenerate::compat::RetainedString message);
void resetCase();
std::shared_ptr<nicxlive::core::nodes::Part> newPart(nijigenerate::compat::RetainedString name);
std::shared_ptr<nicxlive::core::nodes::Part> newMeshPart(nijigenerate::compat::RetainedString name);
bool near(float a, float b);
bool nearVec2(nicxlive::core::math::Vec2 a, nicxlive::core::math::Vec2 b);
bool nearVec3(nicxlive::core::math::Vec3 a, nicxlive::core::math::Vec3 b);
bool nearVec4(nicxlive::core::math::Vec4 a, nicxlive::core::math::Vec4 b);
bool nearVec2Array(nicxlive::core::math::Vec2Array a, nicxlive::core::math::Vec2Array b);
bool nearMat4(nicxlive::core::math::Mat4 a, nicxlive::core::math::Mat4 b);

void testOnionSliceCaptureGeometry();
void testCoreMathPathExtraction();
void testCoreMathSkeletonizeInvariants();
void testCoreMathTriangleInvariants();
void testCoreCvImageContoursDistanceTransform();
void testDepthDrawDataModelContracts();
void testDepthDrawFitZContracts();
}
