#include "base_controls_internal.hpp"
#include "base_internal.hpp"
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/actions/binding_add_remove_action.hpp>
#include <nijigenerate/actions/binding_value_action.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/viewport/common/mesheditor/package.hpp>
#include <nijigenerate/widgets/viewport.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <runtime_state.hpp>
#include <param/binding_impl.hpp>
#include <resource.hpp>
#include <common/nullable_string.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::viewport::base {
namespace {
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec4;
using Node = nicxlive::core::nodes::Node;
using NodeRef = std::shared_ptr<Node>;
using Parameter = nicxlive::core::param::Parameter;
using ValueBinding = nicxlive::core::param::ValueParameterBinding;
using Vec2u = nicxlive::core::param::Vec2u;
using Nodes = compat::Slice<NodeRef>;
using common::transformhandle::ngViewportTransformHandleAdapter;
compat::RetainedString dragName(const nicxlive::core::common::NullableString& prefix,
        std::string_view suffix) {
    try {
        std::string bytes(prefix.view());
        bytes.append(suffix);
        return compat::RetainedString(std::move(bytes));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
void notify(NodeRef node) {
    actions::node::detail::requireActionObject(node)->notifyChange(
        node, nicxlive::core::nodes::NotifyReason::AttributeChanged);
}
bool owned(NodeRef node, Nodes selected) {
    while (actions::node::detail::requireActionObject(node)->parentPtr()) {
        const auto parent = actions::node::detail::requireActionObject(node)->parentPtr();
        const auto count = selected.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto candidate = selected[i];
            if (nicxlive::core::ngResourceObjectEquals(candidate, parent)) return true;
        }
        node = actions::node::detail::requireActionObject(node)->parentPtr();
    }
    return false;
}
common::transformhandle::ActionMap& dragActions(const std::shared_ptr<DraggingOnHandle>& status) {
    return actions::node::detail::requireActionObject(status)->actions;
}
Vec2u keypoint(const std::shared_ptr<Parameter>& parameter) {
    return parameter ? parameter->findClosestKeypoint() : Vec2u{0, 0};
}
template<std::size_t N> float bindingValue(std::shared_ptr<Parameter> parameter, NodeRef node,
        const char (&name)[N], Vec2u index, float initial) {
    const auto binding = std::dynamic_pointer_cast<ValueBinding>(
        actions::node::detail::requireActionObject(parameter)->getBinding(
            std::static_pointer_cast<nicxlive::core::Resource>(node),
            ext::nodes::ngRetainNodeName(compat::RetainedStringSlice(
                compat::RetainedString::fromStaticLiteral(name)))));
    return binding ? binding->getValue(index) : initial;
}
template<std::size_t N> void addValueAction(std::shared_ptr<DraggingOnHandle> status, const char (&name)[N], NodeRef node,
    float oldValue, float& newValue) {
    auto& destination = dragActions(status);
    const auto key = compat::RetainedString::fromStaticLiteral(name);
    using Action = actions::node::NodeValueChangeAction<Node, float>;
    const auto action = std::allocate_shared<Action>(nicxlive::core::d_runtime::SourceAllocator<Action>{},
        compat::RetainedString::fromStaticLiteral(name), node, oldValue, newValue,
        std::shared_ptr<float>(node, &newValue));
    destination[key] = action;
}
Vec2 rotated(Vec2 point, float angle) {
    const float c = static_cast<float>(std::cos(static_cast<long double>(angle)));
    const float s = static_cast<float>(std::sin(static_cast<long double>(angle)));
    const float rotation[3][3]{{c, -s, 0}, {s, c, 0}, {0, 0, 1}};
    float matrix[3][3]{};
    using nicxlive::core::math::detail::inmathAccumulate;
    using nicxlive::core::math::detail::inmathProduct;
    for (int r = 0; r < 3; ++r) for (int col = 0; col < 3; ++col)
        for (int k = 0; k < 3; ++k)
            matrix[r][col] = inmathAccumulate(matrix[r][col], inmathProduct(rotation[r][k], k == col ? 1.0f : 0.0f));
    const float input[]{point.x, point.y, 1};
    float result[3]{};
    for (int col = 0; col < 3; ++col) for (int row = 0; row < 3; ++row)
        result[row] = inmathAccumulate(result[row], inmathProduct(matrix[row][col], input[col]));
    return {result[0], result[1]};
}
}
void ngViewportTransformHandle() {
    const auto camera = nicxlive::core::inGetCameraHandle();
    ImGuiIO io;
    ngMirrorIO(&io);
    const auto parameter = project::incArmedParameter();
    if (project::incSelectedNodes().empty()) return;
    const auto handleBounds = [](NodeRef node) { return ngViewportTransformHandleAdapter(node)->bounds(node); };
    Vec4 total = handleBounds(project::incSelectedNodes()[0]);
    for (auto node : project::incSelectedNodes()) {
        const auto candidate = handleBounds(node);
        total = {std::min(total.x, candidate.x), std::min(total.y, candidate.y),
            std::max(total.z, candidate.z), std::max(total.w, candidate.w)};
    }
    const auto low = core::input::WorldToViewport(total.x, total.y);
    const auto high = core::input::WorldToViewport(total.z, total.w);
    Vec4 bounds{low.x, low.y, high.x, high.y};
    if (shouldMirrorViewport) bounds = {ngGetMirrorX2(bounds.z), bounds.y, ngGetMirrorX2(bounds.x), bounds.w};
    const auto armed = project::incArmedParameter();
    compat::RetainedStringSlice name;
    constexpr auto button = ImGuiMouseButton_Left;
    const auto changeParameter = [&](NodeRef node, std::shared_ptr<Parameter> param,
        const auto& property, Vec2u index, float value) {
        if (value == 0) return;
        const auto bindingName = ext::nodes::ngRetainNodeName(compat::RetainedStringSlice(
            compat::RetainedString::fromStaticLiteral(property)));
        auto binding = std::dynamic_pointer_cast<ValueBinding>(
            actions::node::detail::requireActionObject(param)->getBinding(
                std::static_pointer_cast<nicxlive::core::Resource>(node), bindingName));
        const auto status = ngGetDragOnHandleStatus(button, name);
        if (!binding) {
            binding = std::dynamic_pointer_cast<ValueBinding>(param->createBinding(
                std::static_pointer_cast<nicxlive::core::Resource>(node), bindingName));
            param->addBinding(binding);
            auto& destination = dragActions(status);
            const auto key = compat::RetainedString::fromStaticLiteral("Add");
            const auto action = actions::binding::ngCreateBindingAddRemoveAction<
                actions::binding::ParameterBindingAddAction>(param, binding);
            destination[key] = action;
        }
        if (const auto editor = model::deform::ngViewportModelDeformGetEditor())
            if (const auto single = editor->getEditorFor(node)) single->adjustPathTransform();
        const compat::RetainedStringSlice actionKey(compat::RetainedString::fromStaticLiteral(property));
        if (!dragActions(status).contains(actionKey)) {
            auto& destination = dragActions(status);
            const auto key = actionKey;
            const auto action = actions::binding::ngCreateBindingValueActionDeferred<
                actions::binding::ParameterBindingValueChangeAction<float, ValueBinding>>([&] {
                    const auto label = ext::nodes::ngRetainNodeName(
                        actions::node::detail::requireActionObject(binding)->getNameDescriptor());
                    return std::tuple{label, binding, index.x, index.y};
                });
            destination[key] = action;
        }
        binding->setValue(index, value);
    };
    bool grouping = false;
    const auto beginGroup = [&] {
        if (project::incSelectedNodes().size() > 1 && !grouping) {
            grouping = true;
            core::actionstack::incActionPushGroup();
        }
    };
    const auto endGroup = [&] {
        if (grouping) { grouping = false; core::actionstack::incActionPopGroup(); }
    };
    for (const auto node : project::incSelectedNodes()) {
        name = dragName(actions::node::detail::requireActionObject(node)->getName(), "move");
        const auto index = keypoint(armed);
        if (!ngDragStartedOnHandle(button, name)) continue;
        Vec2 previous;
        ngGetDragPrevValueOnHandle(button, name, previous);
        const auto status = ngGetDragOnHandleStatus(button, name);
        if (ImGui::IsMouseDown(button)) {
            Vec2 origin;
            ngGetDragOriginOnHandle(button, name, origin);
            const auto mouse = core::input::ngInputGetMousePosition();
            const Vec2 relative{-(mouse.x - origin.x), -(mouse.y - origin.y)};
            float x = previous.x + relative.x, y = previous.y + relative.y;
            if (io.KeyCtrl) { x = std::round(x / 5) * 5; y = std::round(y / 5) * 5; }
            actions::node::detail::requireActionObject(status)->lockOrientation(io.KeyShift ?
                (std::abs(relative.x) > std::abs(relative.y) ?
                LockedOrientation::Vertical : LockedOrientation::Horizontal) : LockedOrientation::None);
            if (status->locked == LockedOrientation::Vertical) y = previous.y;
            if (status->locked == LockedOrientation::Horizontal) x = previous.x;
            if (armed) {
                changeParameter(node, armed, "transform.t.x", index, x);
                changeParameter(node, armed, "transform.t.y", index, y);
                notify(node);
            } else {
                node->localTransform.translation.x = x;
                node->localTransform.translation.y = y;
                notify(node);
            }
        } else {
            if (!armed) {
                if (node->localTransform.translation.x != previous.x)
                    addValueAction(status, "X", node, previous.x, node->localTransform.translation.x);
                if (node->localTransform.translation.y != previous.y)
                    addValueAction(status, "Y", node, previous.y, node->localTransform.translation.y);
            }
            beginGroup();
            actions::node::detail::requireActionObject(status)->commitActions();
            ngEndDragOnHandle(button, name);
            ngEndDrag(button);
        }
    }
    endGroup();
    widgets::ngBeginViewportToolArea(compat::RetainedStringSlice(name),
        {bounds.x - detail::handleSize, bounds.y - detail::handleSize});
    widgets::ngButtonColored("", {detail::handleSize, detail::handleSize});
    if (ImGui::IsItemHovered() && ImGui::IsMouseDown(button)) for (const auto node : project::incSelectedNodes()) {
        if (owned(node, project::incSelectedNodes())) continue;
        name = dragName(actions::node::detail::requireActionObject(node)->getName(), "move");
        const auto index = keypoint(armed);
        if (ngDragStartedOnHandle(button, name)) continue;
        ngBeginDrag(button);
        if (armed) ngBeginDragOnHandle(button, name, {
            bindingValue(parameter, node, "transform.t.x", index, 0),
            bindingValue(parameter, node, "transform.t.y", index, 0)});
        else ngBeginDragOnHandle(button, name, {node->localTransform.translation.x, node->localTransform.translation.y});
    }
    widgets::ngEndViewportToolArea();
    widgets::ngBeginViewportToolArea("AREA_MODE", {bounds.z, bounds.w});
    ImGui::SetWindowFontScale(1.5f);
    widgets::ngTextBordered(parameter ? "\ue3fa" : "");
    ImGui::SetWindowFontScale(1);
    widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(parameter ?
        compat::RetainedString::fromStaticLiteral("Editing armed parameter...") :
        compat::RetainedString::fromStaticLiteral("Editing base transform..."))));
    widgets::ngEndViewportToolArea();

    for (const auto node : project::incSelectedNodes()) {
        name = dragName(actions::node::detail::requireActionObject(node)->getName(), "scale");
        const auto index = keypoint(armed);
        if (!ngDragStartedOnHandle(button, name)) continue;
        Vec2 previous;
        ngGetDragPrevValueOnHandle(button, name, previous);
        const auto status = ngGetDragOnHandleStatus(button, name);
        if (ImGui::IsMouseDown(button)) {
            Vec2 original;
            ngGetDragOriginOnHandle(button, name, original);
            auto mouse = core::input::ngInputGetMousePosition();
            const Vec2 origin{-(total.x + total.z) / 2, -(total.y + total.w) / 2};
            mouse = {mouse.x - origin.x, mouse.y - origin.y};
            original = {original.x - origin.x, original.y - origin.y};
            original = rotated(original, node->localTransform.rotation.z);
            mouse = rotated(mouse, node->localTransform.rotation.z);
            const float rx = original.x == 0 ? 0 : mouse.x / original.x;
            const float ry = original.y == 0 ? 0 : mouse.y / original.y;
            float x = previous.x * rx, y = previous.y * ry;
            if (io.KeyShift) {
                if (io.KeyAlt) {
                    const float scale = std::sqrt(mouse.x * mouse.x + mouse.y * mouse.y) /
                        std::sqrt(original.x * original.x + original.y * original.y);
                    x = previous.x * scale; y = previous.y * scale;
                } else {
                    actions::node::detail::requireActionObject(status)->lockOrientation(std::abs(rx) > std::abs(ry) ?
                        LockedOrientation::Vertical : LockedOrientation::Horizontal);
                    if (status->locked == LockedOrientation::Vertical) y = previous.y;
                    else if (status->locked == LockedOrientation::Horizontal) x = previous.x;
                }
            } else actions::node::detail::requireActionObject(status)->lockOrientation(LockedOrientation::None);
            if (io.KeyCtrl) {
                x = std::floor(x * viewportTransformSnap) / viewportTransformSnap;
                y = std::floor(y * viewportTransformSnap) / viewportTransformSnap;
            }
            if (armed) {
                changeParameter(node, armed, "transform.s.x", index, x);
                changeParameter(node, armed, "transform.s.y", index, y);
                notify(node);
            } else ngViewportTransformHandleAdapter(node)->applyScale(node, {x, y});
        } else {
            if (!armed) {
                ngViewportTransformHandleAdapter(node)->endScale(node, previous, dragActions(status));
                notify(node);
            }
            beginGroup(); actions::node::detail::requireActionObject(status)->commitActions();
            ngEndDrag(button); ngEndDragOnHandle(button, name);
        }
    }
    endGroup();
    if (project::incSelectedNodes().size() == 1) {
        widgets::ngBeginViewportToolArea(compat::RetainedStringSlice(name),
            {bounds.x - detail::handleSize, bounds.w});
        widgets::ngButtonColored("", {detail::handleSize, detail::handleSize});
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(button)) for (const auto node : project::incSelectedNodes()) {
            if (owned(node, project::incSelectedNodes())) continue;
            name = dragName(actions::node::detail::requireActionObject(node)->getName(), "scale");
            const auto index = keypoint(armed);
            if (ngDragStartedOnHandle(button, name)) continue;
            ngBeginDrag(button);
            if (armed) ngBeginDragOnHandle(button, name, {
                bindingValue(parameter, node, "transform.s.x", index, 1),
                bindingValue(parameter, node, "transform.s.y", index, 1)});
            else {
                const auto adapter = ngViewportTransformHandleAdapter(node);
                adapter->beginScale(node);
                ngBeginDragOnHandle(button, name, adapter->scaleHandleValue(node));
            }
        }
        widgets::ngEndViewportToolArea();
    }

    for (const auto node : project::incSelectedNodes()) {
        name = dragName(actions::node::detail::requireActionObject(node)->getName(), "rotate");
        const auto index = keypoint(armed);
        if (!ngDragStartedOnHandle(button, name)) continue;
        const auto status = ngGetDragOnHandleStatus(button, name);
        if (ImGui::IsMouseDown(button)) {
            Vec2 previousPosition, previousValue;
            ngGetDragPrevPosOnHandle(button, name, previousPosition);
            ngGetDragPrevValueOnHandle(button, name, previousValue);
            auto mouse = core::input::ngInputGetMousePosition();
            ngSetDragPrevPosOnHandle(button, name, mouse);
            const auto world = core::math::ngTransformHomogeneousPoint(node->transform().matrix(), {0, 0, 0});
            const Vec2 origin{-world.x, -world.y};
            mouse = {mouse.x - origin.x, mouse.y - origin.y};
            previousPosition = {previousPosition.x - origin.x, previousPosition.y - origin.y};
            const float previousArg = std::atan2(previousPosition.y, previousPosition.x);
            const float nextArg = std::atan2(mouse.y, mouse.x);
            float difference = nextArg - previousArg;
            constexpr long double pi = std::numbers::pi_v<long double>;
            while (difference > pi) difference = static_cast<float>(difference - 2 * pi);
            while (difference < -pi) difference = static_cast<float>(difference + 2 * pi);
            float value = previousValue.x + difference;
            ngSetDragPrevValueOnHandle(button, name, {value, 0});
            if (io.KeyCtrl) {
                const auto degrees = static_cast<long double>(value) * (180 / pi);
                value = static_cast<float>((std::round(degrees / 5) * 5) * (pi / 180));
            }
            if (armed) changeParameter(node, armed, "transform.r.z", index, value);
            else node->localTransform.rotation.z = value;
            notify(node);
        } else {
            Vec2 original;
            ngGetDragOrigValueOnHandle(button, name, original);
            if (!armed && node->localTransform.rotation.z != original.x)
                addValueAction(status, "Z", node, original.x, node->localTransform.rotation.z);
            beginGroup(); actions::node::detail::requireActionObject(status)->commitActions();
            ngEndDrag(button); ngEndDragOnHandle(button, name);
        }
    }
    endGroup();
    if (project::incSelectedNodes().size() == 1) {
        widgets::ngBeginViewportToolArea(compat::RetainedStringSlice(name),
            {bounds.z, bounds.y - detail::handleSize});
        widgets::ngButtonColored("", {detail::handleSize, detail::handleSize});
        if (ImGui::IsItemHovered() && ImGui::IsMouseDown(button)) for (const auto node : project::incSelectedNodes()) {
            if (owned(node, project::incSelectedNodes())) continue;
            name = dragName(actions::node::detail::requireActionObject(node)->getName(), "rotate");
            const auto index = keypoint(armed);
            if (ngDragStartedOnHandle(button, name)) continue;
            ngBeginDrag(button);
            ngBeginDragOnHandle(button, name, {
                armed ? bindingValue(parameter, node, "transform.r.z", index, 0) : node->localTransform.rotation.z, 0});
        }
        widgets::ngEndViewportToolArea();
    }
}
}
