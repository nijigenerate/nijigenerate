#include "parameditor.hpp"
#include "parameditor_index_sort.hpp"
#include <nijigenerate/commands/parameter/prop_commands.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/phobos_float_sort.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <bit>
#include <limits>
#include <vector>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>

namespace nijigenerate::windows::parameditor {
namespace {
template<class Allocate> auto allocateParameterEditorMemory(Allocate allocate) {
    try {
        return allocate();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<std::size_t N> void showError(const char (&message)[N]) {
    using namespace widgets::dialog;
    const auto title = core::translation::ngTranslateCString("Error");
    const auto body = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(message));
    const auto owner = body.retentionOwner();
    ngDialog(DialogTag::retained(title.pointer, title.owner),
        owner.use_count() ? DialogText::retained(body.view(), owner) : DialogText::borrowed(body.view()));
}
}
bool ParamEditorWindow::isValidName() {
    const auto ex = std::dynamic_pointer_cast<ext::ExPuppet>(project::incActivePuppet());
    if (!ex) return true;
    const auto found = ex->findParameter(paramName.retainedString());
    if (!found) return true;
    const auto foundId = found->getUUID();
    const auto currentId = compat::ngRequireObject(param)->getUUID();
    return foundId == currentId;
}
float ParamEditorWindow::mapAxisLocal(std::uint32_t axis, float value) {
    const float lo = axis == 0 ? min.x : min.y, hi = axis == 0 ? max.x : max.y;
    return hi == lo ? 0 : (value - lo) / (hi - lo);
}
float ParamEditorWindow::unmapAxisLocal(std::uint32_t axis, float norm) {
    const float lo = axis == 0 ? min.x : min.y, hi = axis == 0 ? max.x : max.y;
    return lo + norm * (hi - lo);
}
void ParamEditorWindow::findEndPoint() {
    for (std::size_t i = 0; i < points[0].size(); ++i) if (!points[0][i].fixed) endPoint.x = static_cast<float>(i);
    for (std::size_t i = 0; i < points[1].size(); ++i) if (!points[1][i].fixed) endPoint.y = static_cast<float>(i);
}
compat::Slice<widgets::EditableAxisPoint>& ParamEditorWindow::axisPoints(std::uint64_t axis, std::size_t sourceLine) {
    if (axis >= points.size()) throw compat::ngStaticError<compat::ArrayIndexError>(axis, points.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/parameditor.d"), sourceLine);
    return points[static_cast<std::size_t>(axis)];
}
widgets::EditableAxisPoint& ParamEditorWindow::axisPoint(std::uint64_t axis, std::size_t index,
    std::size_t sourceLine) {
    auto& values = axisPoints(axis, sourceLine);
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/parameditor.d"), sourceLine);
    return values[index];
}
compat::Slice<std::size_t> ParamEditorWindow::orderPoints(std::uint64_t axis,
    std::size_t countLine, std::size_t compareLine) {
    const auto receiver = weak_from_this().lock();
    const auto count = axisPoints(axis, countLine).size();
    compat::Slice<std::size_t> order;
    if (count) order = allocateParameterEditorMemory([&] {
        return compat::Slice<std::size_t>{std::vector<std::size_t>(count)};
    });
    for (std::size_t i = 0; i < count; ++i) order[i] = i;
    const auto before = [this, axis, compareLine](std::size_t left, std::size_t right) {
        const float leftValue = axisPoint(axis, left, compareLine).value;
        const float rightValue = axisPoint(axis, right, compareLine).value;
        return leftValue < rightValue;
    };
    detail::index_sort::sort(std::span<std::size_t>(order.data(), order.size()), before);
    return order;
}
void ParamEditorWindow::createPoint(std::uint64_t axis) {
    const auto order = orderPoints(axis, 73, 75);
    const auto n = order.size();
    float value;
    if (n >= 2) {
        std::size_t best = 0;
        float gap = -std::numeric_limits<float>::infinity();
        for (std::size_t k = 0; k + 1 < n; ++k) {
            const float left = axisPoint(axis, order[k], 82).value;
            const float right = axisPoint(axis, order[k + 1], 83).value;
            const float candidate = right - left;
            if (candidate > gap) { gap = candidate; best = k; }
        }
        const float left = axisPoint(axis, order[best], 87).value;
        const float right = axisPoint(axis, order[best + 1], 88).value;
        value = (left + right) * 0.5f;
    } else if (n == 1) value = (axis == 0 ? min.x + max.x : min.y + max.y) * 0.5f;
    else value = 0;
    const float norm = mapAxisLocal(static_cast<std::uint32_t>(axis), value);
    axisPoints(axis, 96).append(widgets::EditableAxisPoint{-1, false, value, norm});
    findEndPoint();
}
void ParamEditorWindow::axisPointList(std::uint64_t axis, ImVec2 available) {
    using namespace widgets;
    int deleteIndex = -1;
    ImGui::Indent(); ImGui::PushID(static_cast<int>(axis));
    if (ImGui::BeginChild("###AXIS_ADJ", ImVec2(0, available.y))) {
        if (axisPoints(axis, 105).size() > 2) {
            const auto order = orderPoints(axis, 107, 108);
            for (std::size_t pos = 0; pos < order.size(); ++pos) {
                const auto index = order[pos];
                // Retain the descriptor while createPoint can replace its backing.
                auto retainedPoints = axisPoints(axis, 110);
                if (index >= retainedPoints.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                    index, retainedPoints.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/parameditor.d"), 110);
                auto& point = retainedPoints[index];
                if (point.fixed) continue;
                Vec2 range;
                if (point.origIndex != -1) {
                    range = {pos > 0 ? axisPoint(axis, order[pos - 1], 114).value : (axis == 0 ? min.x : min.y),
                        pos + 1 < order.size() ? axisPoint(axis, order[pos + 1], 115).value : (axis == 0 ? max.x : max.y)};
                } else range = axis == 0 ? Vec2{min.x, max.x} : Vec2{min.y, max.y};
                range.x += 0.01f; range.y += -0.01f;
                ImGui::SetNextItemWidth(80); ImGui::PushID(static_cast<int>(index));
                if (ngDragFloat("adj_offset", &point.value, 0.01f, range.x, range.y, "%.2f", ImGuiSliderFlags_NoRoundToFormat))
                    point.normValue = mapAxisLocal(static_cast<std::uint32_t>(axis), point.value);
                ImGui::SameLine(0, 0);
                if (static_cast<float>(index) == (axis == 0 ? endPoint.x : endPoint.y)) {
                    ngDummy(ImVec2(-52, 32)); ImGui::SameLine(0, 0);
                    if (ngButtonColored("\xee\xa4\xae", ImVec2(24, 24))) deleteIndex = static_cast<int>(index);
                    ImGui::SameLine(0, 0);
                    if (ngButtonColored("\xee\x85\x85", ImVec2(24, 24))) createPoint(axis);
                } else {
                    ngDummy(ImVec2(-28, 32)); ImGui::SameLine(0, 0);
                    if (ngButtonColored("\xee\xa4\xae", ImVec2(24, 24))) deleteIndex = static_cast<int>(index);
                }
                ImGui::PopID();
            }
        } else {
            ngDummy(ImVec2(-28, 32)); ImGui::SameLine(0, 0);
            if (ngButtonColored("\xee\x85\x85", ImVec2(24, 24))) createPoint(axis);
        }
    }
    ImGui::EndChild(); ImGui::PopID(); ImGui::Unindent();
    if (deleteIndex != -1) {
        auto values = axisPoints(axis, 153);
        const auto removeIndex = static_cast<std::uint32_t>(deleteIndex);
        // Phobos stable remove pops the prefix, then the removed element.
        if (removeIndex >= values.size()) {
#ifndef NDEBUG
            throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
                compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#else
            if (removeIndex > values.size())
                throw compat::ngStaticError<compat::ArraySliceError>(removeIndex, values.size(), values.size(),
                    compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
            throw compat::ngStaticError<compat::ArraySliceError>(1, 0, 0,
                compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
#endif
        }
        for (std::size_t i = removeIndex; i + 1 < values.size(); ++i) values[i] = values[i + 1];
        axisPoints(axis, 153) = values.slice(0, values.size() - 1); findEndPoint();
    }
}
void ParamEditorWindow::onBeginUpdate() {
    const auto receiver = weak_from_this().lock();
    ImGui::SetNextWindowSize(ImVec2(768, 384), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(384, 192),
        ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
    base::Window::onBeginUpdate();
}
void ParamEditorWindow::onUpdate() {
    const auto receiver = weak_from_this().lock();
    using namespace widgets;
    using core::translation::ngTranslateRetained;
    using core::translation::ngTranslateCString;
    ImGui::PushID(param.get());
    if (ImGui::BeginChild("###ParamEditorContent", ImVec2(0, -36))) {
        ngText(compat::RetainedStringSlice{ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Parameter Name"))});
        ImGui::Indent(); ngInputText("Name", paramName); ImGui::Unindent();
        // D function statics are thread-local and shared among instances on that thread.
        static thread_local Vec2 prevMin{NAN, NAN}, prevMax{NAN, NAN};
        static thread_local bool initialized = false;
        if (!initialized) { prevMin = min; prevMax = max; initialized = true; }
        if (prevMin.x != min.x || prevMin.y != min.y || prevMax.x != max.x || prevMax.y != max.y) {
            for (std::size_t axis = 0; axis < points.size(); ++axis)
                for (auto& point : points[axis]) point.value = unmapAxisLocal(static_cast<std::uint32_t>(axis), point.normValue);
            findEndPoint(); prevMin = min; prevMax = max;
        }
        auto available = ngAvailableSpace();
        const float space = compat::ngRequireObject(param)->isVec2 ? 128.0f : 32.0f;
        if (ImGui::BeginChild("###ControllerView", ImVec2(192, available.y))) {
            ngDummy(ImVec2(0, available.y / 2 - space / 2));
            ngControllerAxisDemo("###CONTROLLER", param, points, ImVec2(192, space));
        }
        ImGui::EndChild(); ImGui::SameLine(0, 0); ImGui::BeginGroup();
        if (ImGui::BeginChild("###ControllerSettings", ImVec2(0, 0))) {
            available = ngAvailableSpace();
            const auto bounds = [&](unsigned axis) {
                auto& lo = axis == 0 ? min.x : min.y;
                auto& hi = axis == 0 ? max.x : max.y;
                ImGui::Indent(); ImGui::SetNextItemWidth(64); ImGui::PushID(static_cast<int>(axis * 2));
                ngDragFloat(axis == 0 ? "adj_x_min" : "adj_y_min", &lo, 1, -std::numeric_limits<float>::max(),
                    hi - 1, "%.2f", ImGuiSliderFlags_NoRoundToFormat);
                ImGui::PopID(); ImGui::SameLine(0, 4); ImGui::SetNextItemWidth(64); ImGui::PushID(static_cast<int>(axis * 2 + 1));
                ngDragFloat(axis == 0 ? "adj_x_max" : "adj_y_max", &hi, 1, lo + 1, std::numeric_limits<float>::max(),
                    "%.2f", ImGuiSliderFlags_NoRoundToFormat);
                ImGui::PopID(); ImGui::Unindent();
                axisPointList(axis, ngAvailableSpace());
            };
            if (compat::ngRequireObject(param)->isVec2) {
                if (ImGui::BeginChild("###AxisX", ImVec2((available.x - 8) * 0.5f, 0))) { ngText("X"); bounds(0); }
                ImGui::EndChild(); ImGui::SameLine(0, 8);
                if (ImGui::BeginChild("###AxisY", ImVec2(0, 0))) { ngText("Y"); bounds(1); }
                ImGui::EndChild();
            } else {
                ngText(compat::RetainedStringSlice{ngTranslateRetained(
                    compat::RetainedString::fromStaticLiteral("Breakpoints"))});
                bounds(0);
            }
        }
        ImGui::EndChild(); ImGui::EndGroup();
    }
    ImGui::EndChild();
    if (ImGui::BeginChild("###SettingsBtns", ImVec2(0, 0))) {
        ngDummy(ImVec2(-132, 0)); ImGui::SameLine(0, 0);
        if (ngButtonColored(ngTranslateCString("Cancel").pointer, ImVec2(64, 24))) close();
        ImGui::SameLine(0, 4);
        if (ngButtonColored(ngTranslateCString("Save").pointer, ImVec2(64, 24))) {
            bool success = true;
            for (const auto axis : points) {
                for (std::size_t x = 0; x < axis.size() && success; ++x)
                    for (std::size_t y = 0; y < axis.size(); ++y)
                        if (x != y && axis[x].normValue == axis[y].normValue) {
                            showError("One or more axes points are overlapping, this is not allowed.");
                            success = false; break;
                        }
                if (!success) break;
            }
            if (success) {
                if (!isValidName()) showError("Name is already taken");
                else {
                    using namespace commands::parameter::prop;
                    const auto context = std::allocate_shared<commands::Context>(
                        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
                    context->puppet(project::incActivePuppet()); context->parameters({param});
                    const auto currentName = compat::ngRequireObject(param)->getName();
                    if (currentName != paramName.view())
                        ::nijigenerate::commands::ngDispatchRegisteredCommand<SetParameterNameCommand>(
                            ::nijigenerate::commands::parameter::prop::commands,
                            ParamPropCommand::SetParameterName, context,
                            {"ParamPropCommand.SetParameterName", "ParamPropCommand", "SetParameterNameCommand"},
                            paramName);
                    compat::Slice<float> x, y;
                    for (const auto point : points[0]) x.append(point.normValue);
                    compat::phobos_float_sort::sort({x.data(), x.size()});
                    if (compat::ngRequireObject(param)->isVec2) {
                        for (const auto point : points[1]) y.append(point.normValue);
                        compat::phobos_float_sort::sort({y.data(), y.size()});
                    }
                    std::array<float, 2> minArr{min.x, min.y};
                    std::array<float, 2> maxArr{max.x, max.y};
                    if (compat::ngRequireObject(param)->isVec2) {
                        ::nijigenerate::commands::ngDispatchRegisteredCommand<ApplyParameterPropsAxesCommand>(
                            ::nijigenerate::commands::parameter::prop::commands,
                            ParamPropCommand::ApplyParameterPropsAxes, context,
                            {"ParamPropCommand.ApplyParameterPropsAxes", "ParamPropCommand",
                                "ApplyParameterPropsAxesCommand"},
                            minArr, maxArr, x, y);
                    } else {
                        ::nijigenerate::commands::ngDispatchRegisteredCommand<ApplyParameterPropsAxesCommand>(
                            ::nijigenerate::commands::parameter::prop::commands,
                            ParamPropCommand::ApplyParameterPropsAxes, context,
                            {"ParamPropCommand.ApplyParameterPropsAxes", "ParamPropCommand",
                                "ApplyParameterPropsAxesCommand"},
                            minArr, maxArr, x, compat::Slice<float>{});
                    }
                    close();
                }
            }
        }
    }
    ImGui::EndChild(); ImGui::PopID();
}
void ParamEditorWindow::initialize(std::shared_ptr<nicxlive::core::param::Parameter>& parameter, ParamEditorTab tab) {
    param = parameter; defaultTab = tab;
    // Source .dup allocates independent bytes and returns null for zero length.
    const auto sourceName = compat::ngRequireObject(parameter)->getName();
    paramName = allocateParameterEditorMemory([&] {
        return sourceName.empty() ? compat::RetainedStringSlice{} :
            compat::RetainedStringSlice{compat::RetainedString(std::string(sourceName.view()))};
    });
    const compat::Slice<float> minimum{compat::ngRequireObject(parameter)->min.x,
        compat::ngRequireObject(parameter)->min.y};
    min = {minimum[0], minimum[1]};
    const compat::Slice<float> maximum{compat::ngRequireObject(parameter)->max.x,
        compat::ngRequireObject(parameter)->max.y};
    max = {maximum[0], maximum[1]};
    for (std::size_t i = 0; i < points.size(); ++i) {
        points[i].resize(compat::ngRequireObject(parameter)->axisPoints[i].size());
        for (std::size_t j = 0; j < points[i].size(); ++j) {
            points[i][j].origIndex = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(j));
            points[i][j].normValue = compat::ngRequireObject(parameter)->axisPoints[i][j];
            points[i][j].value = unmapAxisLocal(static_cast<std::uint32_t>(i), points[i][j].normValue);
        }
        if (points[i].empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, points[i].size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/parameditor.d"), 332);
        points[i][0].fixed = true; points[i][points[i].size() - 1].fixed = true;
    }
    findEndPoint();
    base::Window::initialize(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Parameter Editor")));
}
}
