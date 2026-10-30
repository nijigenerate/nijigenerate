#include "main_viewport.hpp"
#include "base_internal.hpp"
#include "base_controls_internal.hpp"
#include <nijigenerate/api/mcp/task/task.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/viewport/model/package.hpp>
#include <nijigenerate/viewport/vertex/package.hpp>
#include <nijigenerate/viewport/anim/package.hpp>
#include <nijigenerate/viewport/depth/viewport.hpp>
#include <nijigenerate/viewport/common/mesheditor/brushstate.hpp>
#include <runtime_state.hpp>
#include <math/mat4.hpp>
#include <array>
#include <bit>
#include <algorithm>
#include <cmath>
#include <limits>

namespace nijigenerate::viewport::base {
using nicxlive::core::math::Camera;
using nicxlive::core::math::Vec2;
using nicxlive::core::nodes::Node;
using nicxlive::core::param::Parameter;
using api::mcp::task::ngMcpExternalCommandsBlocked;
using namespace nicxlive::core;

std::shared_ptr<MainViewport> MainViewport::create() {
    return ngCreateDerived<MainViewport>();
}
void MainViewport::constructSource() {
    const auto mode = project::incEditMode();
    onEditModeChanged(mode);
}
void MainViewport::selectionChanged(compat::Slice<std::shared_ptr<Node>> nodes) {
    const auto keepAlive = shared_from_this();
    if (ngMcpExternalCommandsBlocked()) { selectionDeferred = true; return; }
    selectionDeferred = false;
    DelegationViewport::selectionChanged(nodes);
}
void MainViewport::armedParameterChanged(std::shared_ptr<Parameter> parameter) {
    const auto keepAlive = shared_from_this();
    if (ngMcpExternalCommandsBlocked()) { parameterDeferred = true; return; }
    parameterDeferred = false;
    DelegationViewport::armedParameterChanged(parameter);
}
void MainViewport::draw() {
    const auto keepAlive = shared_from_this();
    if (selectionDeferred && !ngMcpExternalCommandsBlocked()) selectionChanged(project::incSelectedNodes());
    if (parameterDeferred && !ngMcpExternalCommandsBlocked()) armedParameterChanged(project::incArmedParameter());
    const auto camera = inGetCameraHandle();
    const bool active = prepareDifferenceAggregation(camera);
    inBeginScene();
    static_cast<Viewport*>(this)->draw(camera);
    inEndScene();
    if (active) {
        int width = 0, height = 0;
        inGetViewport(width, height);
        const auto texture = inGetRenderImage();
        if (inEvaluateDifferenceAggregation(texture, width, height)) {
            DifferenceEvaluationResult result;
            if (inFetchDifferenceAggregationResult(result)) {
                core::ngDifferenceAggregationResult = result;
                core::ngDifferenceAggregationResultValid = true;
                ++core::ngDifferenceAggregationResultSerial;
            }
        } else {
            core::ngDifferenceAggregationResult = core::ngDifferenceAggregationInitialResult();
            core::ngDifferenceAggregationResultValid = false;
            core::ngDifferenceAggregationResultSerial = 0;
        }
    } else {
        core::ngDifferenceAggregationResult = core::ngDifferenceAggregationInitialResult();
        core::ngDifferenceAggregationResultValid = false;
        core::ngDifferenceAggregationResultSerial = 0;
    }
    if (shouldPostProcess) inPostProcessScene();
}
void MainViewport::poll() {
    core::input::ngInputPoll();
    const auto position = ImGui::GetItemRectMin();
    auto mouse = ImGui::GetMousePos();
    if (shouldMirrorViewport) mouse.x = ngGetMirrorX(mouse.x);
    core::input::ngInputSetViewportMouse(position.x - mouse.x, position.y - mouse.y);
}
void MainViewport::update(bool localOnly, bool presentationOnly) {
    const auto keepAlive = shared_from_this();
    if (selectionDeferred && !ngMcpExternalCommandsBlocked()) selectionChanged(project::incSelectedNodes());
    if (parameterDeferred && !ngMcpExternalCommandsBlocked()) armedParameterChanged(project::incArmedParameter());
    ImGuiIO io;
    ngMirrorIO(&io);
    const auto camera = inGetCameraHandle();
    if (!localOnly) detail::viewportMovement(&io, camera);
    if (presentationOnly) static_cast<Viewport*>(this)->updatePresentation(&io, camera);
    else static_cast<Viewport*>(this)->update(&io, camera);
}
void MainViewport::onEditModeChanged(project::EditMode mode) {
    const auto keepAlive = shared_from_this();
    switch (mode) {
    case project::EditMode::ModelEdit: subView(std::allocate_shared<model::ModelViewport>(
        nicxlive::core::d_runtime::SourceAllocator<model::ModelViewport>{})); break;
    case project::EditMode::VertexEdit: subView(std::allocate_shared<vertex::VertexViewport>(
        nicxlive::core::d_runtime::SourceAllocator<vertex::VertexViewport>{})); break;
    case project::EditMode::AnimEdit: subView(std::allocate_shared<anim::AnimationViewport>(
        nicxlive::core::d_runtime::SourceAllocator<anim::AnimationViewport>{})); break;
    case project::EditMode::DepthEdit: subView(std::allocate_shared<depth::viewport::DepthEditViewport>(
        nicxlive::core::d_runtime::SourceAllocator<depth::viewport::DepthEditViewport>{})); break;
    default: break;
    }
    if (subView()) {
        const auto nodes = project::incSelectedNodes();
        compat::ngRequireObject(subView())->selectionChanged(nodes);
        const auto parameter = project::incArmedParameter();
        compat::ngRequireObject(subView())->armedParameterChanged(parameter);
    }
}
bool MainViewport::prepareDifferenceAggregation(std::shared_ptr<Camera> camera) {
    core::ngDifferenceAggregationResolvedIndex = std::numeric_limits<std::size_t>::max();
    if (!common::mesheditor::brushstate::ngBrushHasTeacherPart()) {
        inSetDifferenceAggregationEnabled(false);
        return false;
    }
    const auto node = common::mesheditor::brushstate::ngBrushGetTeacherPart();
    int width = 0, height = 0;
    inGetViewport(width, height);
    DifferenceAggregationRegion region;
    if (!node || !computeDifferenceAggregationRegion(node, camera, width, height, region)) {
        inSetDifferenceAggregationEnabled(false);
        return false;
    }
    inSetDifferenceAggregationEnabled(true);
    inSetDifferenceAggregationRegion(region.x, region.y, region.width, region.height);
    return true;
}
bool MainViewport::computeDifferenceAggregationRegion(std::shared_ptr<Node> node,
    std::shared_ptr<Camera> camera, int width, int height, DifferenceAggregationRegion& region) {
    region = {};
    const auto bounds = compat::ngRequireObject(node)->getCombinedBoundsRect(true, true);
    if (!std::isfinite(bounds.w) || !std::isfinite(bounds.h) || bounds.w <= 0 || bounds.h <= 0)
        return false;
    const float left = bounds.x, bottom = bounds.y;
    const float right = bounds.x + bounds.w, top = bounds.y + bounds.h;
    const std::array<Vec2, 4> corners{{{left, bottom}, {right, bottom}, {left, top}, {right, top}}};
    const auto matrix = compat::ngRequireObject(camera)->matrix();
    float minX = std::numeric_limits<float>::max(), maxX = -minX;
    float minY = minX, maxY = -minX;
    bool valid = false;
    for (const auto corner : corners) {
        const std::array<float, 4> input{corner.x, corner.y, 0, 1};
        std::array<float, 4> clip{};
        for (int column = 0; column < 4; ++column) for (int row = 0; row < 4; ++row)
            clip[row] = math::detail::inmathAccumulate(clip[row], math::detail::inmathProduct(matrix[row][column], input[column]));
        if (std::fabs(clip[3]) < 1e-6f) continue;
        const Vec2 ndc{clip[0] / clip[3], clip[1] / clip[3]};
        if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y)) continue;
        const float px = (ndc.x * 0.5f + 0.5f) * width;
        const float py = (ndc.y * 0.5f + 0.5f) * height;
        if (!std::isfinite(px) || !std::isfinite(py)) continue;
        if (px < minX) minX = px;
        if (px > maxX) maxX = px;
        if (py < minY) minY = py;
        if (py > maxY) maxY = py;
        valid = true;
    }
    if (!valid) return false;
    const auto clamp = [](float value, float upper) {
#ifndef NDEBUG
        if (0.0f > upper) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Lower can't be greater than upper."),
            compat::RetainedString::fromStaticLiteral("std/algorithm/comparison.d"), 588);
#endif
        return value < 0.0f ? 0.0f : value > upper ? upper : value;
    };
    const float x0 = clamp(minX, static_cast<float>(width));
    const float x1 = clamp(maxX, static_cast<float>(width));
    const float y0 = clamp(minY, static_cast<float>(height));
    const float y1 = clamp(maxY, static_cast<float>(height));
    if (x1 <= x0 || y1 <= y0) return false;
    const int x = compat::ngFloatToInt32(std::floor(x0)), y = compat::ngFloatToInt32(std::floor(y0));
    const auto subtract = [](std::int32_t a, std::int32_t b) {
        return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a) - std::bit_cast<std::uint32_t>(b));
    };
    const int w = subtract(compat::ngFloatToInt32(std::ceil(x1)), x);
    const int h = subtract(compat::ngFloatToInt32(std::ceil(y1)), y);
    if (w <= 0 || h <= 0) return false;
    region = {x, y, w, h};
    return true;
}
std::shared_ptr<MainViewport> ngViewport() {
    auto& viewport = detail::ngMainViewportStorage();
    if (!viewport) viewport = MainViewport::create();
    return viewport;
}
void ngPresentTemporaryViewport(std::shared_ptr<Viewport> next) {
    const auto main = ngViewport();
    main->withdraw();
    main->subView(next);
    if (next) {
        next->selectionChanged(project::incSelectedNodes());
        next->armedParameterChanged(project::incArmedParameter());
        next->present();
    }
}
void ngSetViewportAlwaysUpdate(bool value) { ngViewport()->alwaysUpdate(value); }
}
