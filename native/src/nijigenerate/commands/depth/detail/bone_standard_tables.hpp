#pragma once
#include "standard_depth_fields.hpp"

namespace nijigenerate::commands::depth::bone {
namespace {

compat::Slice<StandardDepthParameterSpec> standardDepthParameterSpecs() {
    auto axis5 = compat::Slice<float>{0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    return {
        {compat::RetainedString::fromStaticLiteral("Face::Yaw-Pitch"), true, {-1.0f, -1.0f}, {1.0f, 1.0f},
            axis5.duplicate(), axis5.duplicate()},
        {compat::RetainedString::fromStaticLiteral("Face::Roll"), false, {-1.0f, 0.0f}, {1.0f, 0.0f},
            axis5.duplicate(), {0.0f}},
        {compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"), true, {-1.0f, -1.0f}, {1.0f, 1.0f},
            axis5.duplicate(), axis5.duplicate()},
        {compat::RetainedString::fromStaticLiteral("Body::Roll"), false, {-1.0f, 0.0f}, {1.0f, 0.0f},
            axis5.duplicate(), {0.0f}}};
}

compat::Slice<StandardDepthBoneBindingSpec> standardDepthBoneBindingSpecs() {
    return {
        {compat::RetainedString::fromStaticLiteral("Face::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Head"),
            compat::RetainedString::fromStaticLiteral("transform.r.y"), {
                {{-1, -1}, -0.5235988f},
                {{-1, 0}, -0.5235988f},
                {{-1, 1}, -0.5235988f},
                {{0, -1}, 0.0f},
                {{0, 0}, 0.0f},
                {{0, 1}, 0.0f},
                {{1, -1}, 0.5235988f},
                {{1, 0}, 0.5235988f},
                {{1, 1}, 0.5235988f},
            }},
        {compat::RetainedString::fromStaticLiteral("Face::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Head"),
            compat::RetainedString::fromStaticLiteral("transform.r.x"), {
                {{-1, -1}, -0.34906584f},
                {{-1, 1}, 0.34906584f},
                {{0, -1}, -0.34906584f},
                {{0, 0}, 0.0f},
                {{0, 1}, 0.34906584f},
                {{1, -1}, -0.34906584f},
                {{1, 0}, 0.0f},
                {{1, 1}, 0.34906584f},
            }},
        {compat::RetainedString::fromStaticLiteral("Face::Roll"),
            compat::RetainedString::fromStaticLiteral("Neck"),
            compat::RetainedString::fromStaticLiteral("transform.r.z"), {
                {{-1, 0}, -0.41800633f},
                {{0, 0}, 0.0f},
                {{1, 0}, 0.41887903f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Spine"),
            compat::RetainedString::fromStaticLiteral("transform.r.y"), {
                {{-1, -1}, -0.5235988f},
                {{-1, 0}, -0.5235988f},
                {{-1, 1}, -0.5235988f},
                {{0, -1}, 0.0f},
                {{0, 0}, 0.0f},
                {{0, 1}, 0.0f},
                {{1, -1}, 0.5235988f},
                {{1, 0}, 0.5235988f},
                {{1, 1}, 0.5235988f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Spine"),
            compat::RetainedString::fromStaticLiteral("transform.r.x"), {
                {{-1, -1}, -0.5235988f},
                {{-1, 0}, 0.0f},
                {{-1, 1}, 0.17453292f},
                {{0, -1}, -0.5235988f},
                {{0, 0}, 0.0f},
                {{0, 1}, 0.17453292f},
                {{1, -1}, -0.5235988f},
                {{1, 0}, 0.0f},
                {{1, 1}, 0.17453292f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Chest"),
            compat::RetainedString::fromStaticLiteral("transform.r.x"), {
                {{-1, -1}, 0.2617994f},
                {{-1, 0}, 0.0f},
                {{-1, 1}, 0.34906584f},
                {{0, -1}, 0.2617994f},
                {{0, 0}, 0.0f},
                {{0, 1}, 0.34906584f},
                {{1, -1}, 0.2617994f},
                {{1, 0}, 0.0f},
                {{1, 1}, 0.34906584f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Chest"),
            compat::RetainedString::fromStaticLiteral("transform.r.y"), {
                {{-1, -1}, 0.0f},
                {{0, 0}, 0.0f},
                {{1, 1}, 0.0f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Clavicle.L"),
            compat::RetainedString::fromStaticLiteral("transform.r.x"), {
                {{-1, 1}, 0.0f},
                {{0, 0}, 0.0f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Pelvis"),
            compat::RetainedString::fromStaticLiteral("transform.t.y"), {
                {{0, -1}, 26.0f},
                {{0, 0}, 0.0f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Shin.L"),
            compat::RetainedString::fromStaticLiteral("transform.t.y"), {
                {{0, -1}, 93.0f},
                {{0, 0}, 0.0f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Shin.R"),
            compat::RetainedString::fromStaticLiteral("transform.t.y"), {
                {{0, -1}, 93.0f},
                {{0, 0}, 0.0f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Shin.L"),
            compat::RetainedString::fromStaticLiteral("transform.t.x"), {
                {{0, -1}, 68.0f},
                {{0, 0}, 0.0f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Yaw-Pitch"),
            compat::RetainedString::fromStaticLiteral("Shin.R"),
            compat::RetainedString::fromStaticLiteral("transform.t.x"), {
                {{0, -1}, -67.0f},
                {{0, 0}, 0.0f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Roll"),
            compat::RetainedString::fromStaticLiteral("Pelvis"),
            compat::RetainedString::fromStaticLiteral("transform.t.x"), {
                {{-1, 0}, 30.0f},
                {{0, 0}, 0.0f},
                {{1, 0}, -30.0f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Roll"),
            compat::RetainedString::fromStaticLiteral("Pelvis"),
            compat::RetainedString::fromStaticLiteral("transform.t.y"), {
                {{-1, 0}, -28.0f},
                {{0, 0}, 0.0f},
                {{1, 0}, -28.0f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Roll"),
            compat::RetainedString::fromStaticLiteral("Pelvis"),
            compat::RetainedString::fromStaticLiteral("transform.r.z"), {
                {{-1, 0}, -0.049741887f},
                {{0, 0}, 0.0f},
                {{1, 0}, 0.049741887f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Roll"),
            compat::RetainedString::fromStaticLiteral("Spine"),
            compat::RetainedString::fromStaticLiteral("transform.r.z"), {
                {{-1, 0}, -0.15271631f},
                {{0, 0}, 0.0f},
                {{1, 0}, 0.15271631f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Roll"),
            compat::RetainedString::fromStaticLiteral("Chest"),
            compat::RetainedString::fromStaticLiteral("transform.r.y"), {
                {{-1, 0}, -0.0f},
                {{0, 0}, 0.0f},
                {{1, 0}, 0.0f},
            }},
        {compat::RetainedString::fromStaticLiteral("Body::Roll"),
            compat::RetainedString::fromStaticLiteral("Chest"),
            compat::RetainedString::fromStaticLiteral("transform.r.z"), {
                {{-1, 0}, -0.123918384f},
                {{0, 0}, 0.0f},
                {{1, 0}, 0.123918384f},
            }},
    };
}

} // namespace
} // namespace nijigenerate::commands::depth::bone
