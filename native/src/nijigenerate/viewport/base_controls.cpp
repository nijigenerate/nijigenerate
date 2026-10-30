#include "base_controls_internal.hpp"
// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/dpi.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <common/resource_key_map.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <runtime_state.hpp>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::viewport::base {
using nicxlive::core::math::Vec2;
namespace {
thread_local std::array<bool, ImGuiMouseButton_COUNT> draggingInViewport{}, dragging{};
using DragHandleMap = nicxlive::core::common::ResourceKeyMap<compat::RetainedStringSlice,
    std::shared_ptr<DraggingOnHandle>, common::transformhandle::detail::ActionKeyHash,
    common::transformhandle::detail::ActionKeyEqual>;
thread_local std::array<DragHandleMap, ImGuiMouseButton_COUNT> draggingOnHandle;
thread_local std::array<std::int32_t, ImGuiMouseButton_COUNT> numberOnHandle{};
thread_local bool movingViewport = false, movingPart = false;
thread_local float sx = std::numeric_limits<float>::quiet_NaN(), sy = std::numeric_limits<float>::quiet_NaN();
thread_local float csx = std::numeric_limits<float>::quiet_NaN(), csy = std::numeric_limits<float>::quiet_NaN();
template<class T, std::size_t N> T& mouseSlot(std::array<T, N>& values, int button, std::size_t line) {
    const auto index = static_cast<std::size_t>(button);
    if (index >= N) throw compat::ngStaticError<compat::ArrayIndexError>(index, N,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/base.d"), line);
    return values[index];
}
auto handle(int button, const compat::RetainedStringSlice& name, std::size_t line) {
    auto& handles = mouseSlot(draggingOnHandle, button, line);
    const auto found = handles.find(name);
    if (found == handles.end()) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/base.d"), line);
    return found->second;
}
compat::RetainedStringSlice retainedName(std::string_view name) {
    return compat::RetainedString(name);
}
void increment(std::int32_t& count, std::uint32_t amount) {
    count = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(count) + amount);
}
}
float ngGetMirrorX(float position) {
    const auto origin = ImGui::GetItemRectMin();
    return ngGetMirrorX2(position - origin.x) + origin.x;
}
float ngGetMirrorX2(float position) {
    int width, height;
    nicxlive::core::inGetViewport(width, height);
    return shouldMirrorViewport ? std::fmod(-position + width, static_cast<float>(width)) : position;
}
void ngMirrorIO(ImGuiIO* result) {
    const auto& io = ImGui::GetIO();
    if (!result) compat::ngFailNullAccess(true);
    *result = io;
    if (shouldMirrorViewport) result->MousePos.x = ngGetMirrorX(result->MousePos.x);
}
bool ngStartedDrag(int button) { return mouseSlot(dragging, button, 462); }
void ngBeginDrag(int button) { mouseSlot(dragging, button, 466) = true; }
void ngEndDrag(int button) { mouseSlot(dragging, button, 470) = false; }
bool ngDragStartedInViewport(int button) {
    return mouseSlot(draggingInViewport, button, 474) && mouseSlot(numberOnHandle, button, 474) == 0;
}
bool ngDragStartedOnHandle(int button, compat::RetainedStringSlice name) {
    return mouseSlot(draggingOnHandle, button, 478).contains(name) &&
        compat::ngRequireObject(handle(button, name, 478))->dragged;
}
void ngBeginDragInViewport(int button) { mouseSlot(draggingInViewport, button, 482) = true; }
void ngBeginDragOnHandle(int button, std::string_view name, Vec2 previous) {
    ngBeginDragOnHandle(button, retainedName(name), previous);
}
void ngBeginDragOnHandle(int button, compat::RetainedStringSlice name, Vec2 previous) {
    const auto position = core::input::ngInputGetMousePosition();
    auto& destination = mouseSlot(draggingOnHandle, button, 487);
    const auto key = name;
    const auto status = std::allocate_shared<DraggingOnHandle>(
        nicxlive::core::d_runtime::SourceAllocator<DraggingOnHandle>{}, position, previous);
    destination[key] = status;
    increment(mouseSlot(numberOnHandle, button, 488), 1);
}
bool ngGetDragOriginOnHandle(int button, compat::RetainedStringSlice name, Vec2& position) {
    position = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    const bool result = ngDragStartedOnHandle(button, name);
    position = compat::ngRequireObject(handle(button, name, 493))->dragOrigin;
    return result;
}
bool ngGetDragOrigValueOnHandle(int button, compat::RetainedStringSlice name, Vec2& value) {
    value = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    const bool result = ngDragStartedOnHandle(button, name);
    value = compat::ngRequireObject(handle(button, name, 499))->origValue;
    return result;
}
bool ngGetDragPrevValueOnHandle(int button, compat::RetainedStringSlice name, Vec2& value) {
    value = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    const bool result = ngDragStartedOnHandle(button, name);
    value = compat::ngRequireObject(handle(button, name, 505))->prevValue;
    return result;
}
bool ngGetDragPrevPosOnHandle(int button, compat::RetainedStringSlice name, Vec2& position) {
    position = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    const bool result = ngDragStartedOnHandle(button, name);
    position = compat::ngRequireObject(handle(button, name, 511))->prevPos;
    return result;
}
void ngSetDragPrevPosOnHandle(int button, compat::RetainedStringSlice name, Vec2 position) {
    const auto receiver = handle(button, name, 516);
    if (!receiver) compat::ngFailNullAccess(true);
    receiver->prevPos = position;
}
void ngSetDragPrevValueOnHandle(int button, compat::RetainedStringSlice name, Vec2 value) {
    const auto receiver = handle(button, name, 520);
    if (!receiver) compat::ngFailNullAccess(true);
    receiver->prevValue = value;
}
void ngEndDragInViewport(int button) { mouseSlot(draggingInViewport, button, 524) = false; }
void ngEndDragOnHandle(int button, compat::RetainedStringSlice name) {
    mouseSlot(draggingOnHandle, button, 528).erase(name);
    increment(mouseSlot(numberOnHandle, button, 529), UINT32_MAX);
}
std::shared_ptr<DraggingOnHandle> ngGetDragOnHandleStatus(int button, compat::RetainedStringSlice name) {
    return mouseSlot(draggingOnHandle, button, 533).contains(name) ? handle(button, name, 533) : nullptr;
}
bool ngDragStartedOnHandle(int button, std::string_view name) {
    return ngDragStartedOnHandle(button, retainedName(name));
}
bool ngGetDragOriginOnHandle(int button, std::string_view name, Vec2& position) {
    return ngGetDragOriginOnHandle(button, retainedName(name), position);
}
bool ngGetDragOrigValueOnHandle(int button, std::string_view name, Vec2& value) {
    return ngGetDragOrigValueOnHandle(button, retainedName(name), value);
}
bool ngGetDragPrevValueOnHandle(int button, std::string_view name, Vec2& value) {
    return ngGetDragPrevValueOnHandle(button, retainedName(name), value);
}
bool ngGetDragPrevPosOnHandle(int button, std::string_view name, Vec2& position) {
    return ngGetDragPrevPosOnHandle(button, retainedName(name), position);
}
void ngSetDragPrevPosOnHandle(int button, std::string_view name, Vec2 position) {
    ngSetDragPrevPosOnHandle(button, retainedName(name), position);
}
void ngSetDragPrevValueOnHandle(int button, std::string_view name, Vec2 value) {
    ngSetDragPrevValueOnHandle(button, retainedName(name), value);
}
void ngEndDragOnHandle(int button, std::string_view name) {
    ngEndDragOnHandle(button, retainedName(name));
}
std::shared_ptr<DraggingOnHandle> ngGetDragOnHandleStatus(int button, std::string_view name) {
    return ngGetDragOnHandleStatus(button, retainedName(name));
}
void DraggingOnHandle::lockOrientation(LockedOrientation orientation) {
    if (orientation == LockedOrientation::None || locked == LockedOrientation::None) locked = orientation;
}
compat::RetainedString DraggingOnHandle::toString() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DraggingOnHandle),
            compat::RetainedString::fromStaticLiteral("nijigenerate.viewport.base.DraggingOnHandle"));
        return true;
    }();
    (void)registered;
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DraggingOnHandle::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DraggingOnHandle::opEquals(std::shared_ptr<DraggingOnHandle> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DraggingOnHandle::opCmp(std::shared_ptr<DraggingOnHandle>) {
    const auto className = DraggingOnHandle::toString();
    const auto message = [&] {
        try {
            std::string bytes = "need opCmp for class ";
            bytes += className.view();
            return compat::RetainedString(std::move(bytes));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngDraggingOnHandleObjectEquals(std::shared_ptr<DraggingOnHandle> left,
    std::shared_ptr<DraggingOnHandle> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right));
}
void DraggingOnHandle::commitActions() {
    const auto keepAlive = shared_from_this();
    if (actions.size() == 1) {
        const auto capturedActions = actions;
        for (const auto& entry : capturedActions) {
            const auto action = entry.second;
            if (auto lazy = std::dynamic_pointer_cast<actions::LazyBoundAction>(action)) lazy->updateNewState();
            core::actionstack::incActionPush(action);
        }
    } else if (!actions.empty()) {
        auto keys = actions.keys();
        std::sort(keys.begin(), keys.end(), [](const auto& left, const auto& right) {
            return left.view() < right.view();
        });
        std::shared_ptr<actions::GroupAction> group;
        for (const auto& key : keys) {
            const auto found = actions.find(key);
            if (found == actions.end()) throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/base.d"), 984);
            const auto action = found->second;
            if (auto lazy = std::dynamic_pointer_cast<actions::LazyBoundAction>(action)) lazy->updateNewState();
            if (!group) group = std::allocate_shared<actions::GroupAction>(
                nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});
            group->addAction(action);
        }
        if (group) core::actionstack::incActionPush(group);
    }
}
void ngViewportReset() {
    viewportTargetPosition = {0, 0};
    viewportTargetZoom = 1;
    project::incSetEditMode(project::EditMode::ModelEdit);
}
compat::RetainedString ngGetViewportZoomMode() {
    if (core::settings::incSettingsCanGet(
            compat::RetainedString::fromStaticLiteral("ViewportZoomMode")))
        return core::settings::incSettingsGet<core::settings::SettingsString>(
            compat::RetainedString::fromStaticLiteral("ViewportZoomMode")).retainedString();
    return compat::RetainedString::fromStaticLiteral("ScreenCenter");
}
bool ngSetViewportZoomMode(compat::RetainedString value) {
    core::settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("ViewportZoomMode"),
        core::settings::SettingsString(std::move(value)));
    return true;
}
float ngGetViewportZoomSpeed() {
    return core::settings::incSettingsCanGet(
        compat::RetainedString::fromStaticLiteral("ViewportZoomSpeed")) ?
        core::settings::incSettingsGet<float>(
            compat::RetainedString::fromStaticLiteral("ViewportZoomSpeed")) : 1.0f;
}
bool ngSetViewportZoomSpeed(float value) {
    core::settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("ViewportZoomSpeed"), value);
    return true;
}
namespace detail {
void viewportMovement(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera) {
    const float uiScale = core::dpi::incGetUIScale();
    const bool canMove = project::incEditMode() != project::EditMode::DepthEdit ||
        compat::ngRequireObject(io)->KeyShift;
    if (!movingViewport && canMove && compat::ngRequireObject(io)->MouseDown[1] &&
            core::input::ngInputIsDragRequested()) {
        movingViewport = true;
        sx = compat::ngRequireObject(io)->MousePos.x; sy = compat::ngRequireObject(io)->MousePos.y;
        csx = compat::ngRequireObject(camera)->position.x; csy = compat::ngRequireObject(camera)->position.y;
    }
    if (movingViewport && !compat::ngRequireObject(io)->MouseDown[1]) movingViewport = false;
    if (movingViewport) {
        const Vec2 position{csx + ((compat::ngRequireObject(io)->MousePos.x - sx) / viewportZoom) * uiScale,
            csy + ((compat::ngRequireObject(io)->MousePos.y - sy) / viewportZoom) * uiScale};
        if (!camera) compat::ngFailNullAccess(true);
        camera->position = position;
        viewportTargetPosition = compat::ngRequireObject(camera)->position;
    }
    const auto mode = ngGetViewportZoomMode();
    if (mode == "ScreenCenter") viewportZoomToCenter(io, camera, uiScale);
    else if (mode == "MousePosition") viewportZoomToMouse(io, camera, uiScale);
}
void viewportZoomToMouse(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera, float uiScale) {
    const float speed = ngGetViewportZoomSpeed();
    if (compat::ngRequireObject(io)->MouseWheel == 0) return;
    const float previous = viewportZoom;
    viewportZoom += (compat::ngRequireObject(io)->MouseWheel * speed / 50) * viewportZoom * uiScale;
    viewportZoom = static_cast<float>(std::clamp(static_cast<double>(viewportZoom), viewportZoomMin, viewportZoomMax));
    if (!camera) compat::ngFailNullAccess(true);
    camera->scale = {viewportZoom, viewportZoom};
    viewportTargetZoom = viewportZoom;
    int width, height;
    nicxlive::core::inGetViewport(width, height);
    const auto panel = ImGui::GetItemRectMin();
    const Vec2 related{compat::ngRequireObject(io)->MousePos.x - (panel.x + static_cast<float>(width) / 2),
        compat::ngRequireObject(io)->MousePos.y - (panel.y + static_cast<float>(height) / 2)};
    const Vec2 after{related.x / viewportZoom * uiScale, related.y / viewportZoom * uiScale};
    const Vec2 before{related.x / previous * uiScale, related.y / previous * uiScale};
    compat::ngRequireObject(camera)->position.x -= before.x - after.x;
    compat::ngRequireObject(camera)->position.y -= before.y - after.y;
    viewportTargetPosition = compat::ngRequireObject(camera)->position;
}
void viewportZoomToCenter(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera, float uiScale) {
    const float speed = ngGetViewportZoomSpeed();
    if (compat::ngRequireObject(io)->MouseWheel == 0) return;
    viewportZoom += (compat::ngRequireObject(io)->MouseWheel / 50 * speed) * viewportZoom * uiScale;
    viewportZoom = static_cast<float>(std::clamp(static_cast<double>(viewportZoom), viewportZoomMin, viewportZoomMax));
    if (!camera) compat::ngFailNullAccess(true);
    camera->scale = {viewportZoom, viewportZoom};
    viewportTargetZoom = viewportZoom;
}
}
}
