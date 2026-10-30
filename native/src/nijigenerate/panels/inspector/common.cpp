#include "common.hpp"
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/api/mcp/task/task.hpp>
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/phobos_unicode_exception.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/utils.hpp>
#include <param/binding_impl.hpp>
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <boost/multiprecision/cpp_bin_float.hpp>
#endif
#include <array>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <vector>

namespace nijigenerate::panels::inspector {
thread_local ImVec4 CategoryTextColor(0.36f, 0.45f, 0.35f, 1.0f);
namespace {
template<class Storage> void reserveInspectorMemory(Storage& storage,std::size_t length) {
    try {
        storage.reserve(length);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class Build> std::string inspectorTextBacking(Build&& build) {
    try {
        return std::forward<Build>(build)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

compat::RetainedString inspectorPrefixedLabel(std::string_view prefix, std::string_view name) {
    return detail::inspectorNameBytes([&] {
        std::string bytes(prefix);
        if (!name.empty()) bytes.append(name);
        return bytes;
    });
}

template<class Range, class View> std::string joinInspectorStrings(
    const Range& values, std::string_view separator, View view) {
    if (values.empty()) return {};
    std::size_t length = 0;
    for (const auto& value : values) length += view(value).size();
    length += (values.size() - 1) * separator.size();
    auto result = inspectorTextBacking([&] { return std::string(length, '\0'); });
    std::size_t position = 0;
    const auto put = [&](std::string_view bytes, std::size_t sourceLine) {
        for (const auto byte : bytes) {
            if (position >= result.size())
                throw compat::ngStaticError<compat::ArrayIndexError>(position, result.size(),
                    compat::RetainedString::fromStaticLiteral("std/array.d"), sourceLine);
            result[position++] = byte;
        }
    };
    put(view(values[0]), 2258);
    for (std::size_t i = 1; i < values.size(); ++i) {
        put(separator, 2263);
        put(view(values[i]), 2265);
    }
    return result;
}
}

void incModelModeHeader(const std::shared_ptr<Node>& node) {
    const auto target = compat::ngRequireObject(node);
    ImGui::PushID(static_cast<int>(target->getUUID()));
    const auto icon = utils::incTypeIdToIcon(target->typeId());
    const std::array arguments{core::translation::ngFormatArgument(icon)};
    const auto type = detail::formatInspectorText("%s", arguments);
    const auto size = widgets::ngMeasureString(type);
    const compat::RetainedString oldName(target->getName());
    auto newName = oldName;
    if (widgets::ngInputText(compat::RetainedString::fromStaticLiteral("###MODEL_NODE_HEADER"),
        widgets::ngAvailableSpace().x - 24, newName)) {
        try {
            auto terminated = std::allocate_shared<compat::SourceCString>(
                nicxlive::core::d_runtime::SourceAllocator<compat::SourceCString>{}, newName);
            compat::detail::ngSourceCStringPostconditions(newName.view(),terminated->data());
            const auto length = std::strlen(terminated->data());
            newName = compat::RetainedString::retainedView(terminated, terminated->data(), length);
        } catch (const compat::UTFException&) {}
        if (oldName.view() != newName.view()) {
            using Name = nicxlive::core::common::NullableString;
            auto slot = std::shared_ptr<Name>(target, &target->name);
            using NameAction = actions::node::NodeValueChangeAction<Node, Name>;
            auto action = std::allocate_shared<NameAction>(nicxlive::core::d_runtime::SourceAllocator<NameAction>{},
                compat::RetainedString::fromStaticLiteral("name"), target,
                oldName.toNullableString(), newName.toNullableString(), std::move(slot));
            action->redo();
            core::actionstack::incActionPush(action);
        }
    }
    ImGui::SameLine(0, 0);
    widgets::ngDummy(ImVec2(-size.x, size.y));
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(type.data(), type.empty() ? type.data() : type.data() + type.size());
    ImGui::PopID();
}

void incCommonNonEditHeader(const std::shared_ptr<Node>& node) {
    const auto target = compat::ngRequireObject(node);
    ImGui::PushID(static_cast<int>(target->getUUID()));
    const auto icon = utils::incTypeIdToIcon(target->typeId());
    const std::array arguments{core::translation::ngFormatArgument(icon)};
    const auto type = detail::formatInspectorText("%s", arguments);
    const auto size = widgets::ngMeasureString(type);
    const compat::RetainedString name(target->getName());
    ImGui::TextUnformatted(name.data(), name.empty() ? name.data() : name.data() + name.size());
    ImGui::SameLine(0, 0);
    widgets::ngDummy(ImVec2(-size.x, size.y));
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(type.data(), type.empty() ? type.data() : type.data() + type.size());
    ImGui::PopID();
    ImGui::Separator();
}

void ngMultiEditHeader(Selection<Node> nodes) {
    detail::drawProjectedHeader(nodes.size(), [nodes](std::size_t index) { return nodes[index]; });
}
void detail::drawProjectedHeader(std::size_t length,
    const std::function<std::shared_ptr<Node>(std::size_t)>& read) {
    if (length == 0) throw compat::ngStaticError<compat::ArrayIndexError>(0, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/inspector/common.d"), 235);
    ImGui::PushID(static_cast<int>(compat::ngRequireObject(read(0))->getUUID()));
    std::vector<std::string_view, nicxlive::core::d_runtime::SourceAllocator<std::string_view>> icons;
    reserveInspectorMemory(icons,length);
    for (std::size_t i = 0; i < length; ++i) {
        const auto node = read(i);
        icons.push_back(utils::incTypeIdToIcon(compat::ngRequireObject(node)->typeId()));
    }
    const auto type = joinInspectorStrings(icons, "", [](std::string_view value) { return value; });
    const auto size = widgets::ngMeasureString(type);
    std::vector<compat::RetainedString,
        nicxlive::core::d_runtime::SourceAllocator<compat::RetainedString>> nodeNames;
    reserveInspectorMemory(nodeNames,length);
    for (std::size_t i = 0; i < length; ++i) {
        const auto node = read(i);
        nodeNames.emplace_back(compat::ngRequireObject(node)->getName());
    }
    const auto names = joinInspectorStrings(nodeNames, ",",
        [](const compat::RetainedString& value) { return value.view(); });
    ImGui::TextUnformatted(names.data(), names.data() + names.size());
    ImGui::SameLine(0, 0);
    widgets::ngDummy(ImVec2(-size.x, size.y));
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(type.data(), type.data() + type.size());
    ImGui::PopID();
    ImGui::Separator();
}

bool ngInspectorDeformFloatDragVal(compat::RetainedString name, float* result, float speed, bool rotation) {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    using SourceReal = boost::multiprecision::cpp_bin_float_double_extended;
    const SourceReal pi = boost::multiprecision::ldexp(SourceReal(std::uint64_t{0xc90fdaa22168c235}), -62);
    const SourceReal toDegrees = SourceReal{180} / pi;
    const SourceReal toRadians = pi / SourceReal{180};
    const auto asFloat = [](const SourceReal& value) { return value.convert_to<float>(); };
#else
    using SourceReal = long double;
    constexpr SourceReal toDegrees = SourceReal{180} / std::numbers::pi_v<SourceReal>;
    constexpr SourceReal toRadians = std::numbers::pi_v<SourceReal> / SourceReal{180};
    const auto asFloat = [](SourceReal value) { return static_cast<float>(value); };
#endif
    const float initial = *compat::ngRequireObject(result);
    float current = rotation ? asFloat(SourceReal(toDegrees * SourceReal(initial))) : initial;
    if (widgets::ngDragFloat(compat::RetainedStringSlice(name), &current, speed,
        -std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
        compat::RetainedStringSlice(rotation ? compat::RetainedString::fromStaticLiteral("%.2f°")
            : compat::RetainedString::fromStaticLiteral("%.2f")), ImGuiSliderFlags_NoRoundToFormat)) {
        *result = rotation ? asFloat(SourceReal(toRadians * SourceReal(current))) : current;
        return true;
    }
    return false;
}
bool ngInspectorDeformInputFloat(compat::RetainedString name, float* result, float step, float fast) {
    const compat::SourceCString label{name};
    compat::detail::ngSourceCStringPostconditions(name.view(),label.data());
    return ImGui::InputFloat(label.data(), result, step, fast, "%.2f");
}
bool ngInspectorDeformSliderFloat(compat::RetainedString name, float* result, float min, float max) {
    const compat::SourceCString label{name};
    compat::detail::ngSourceCStringPostconditions(name.view(),label.data());
    return ImGui::SliderFloat(label.data(), result, min, max, "%.2f");
}
bool ngInspectorDeformDragFloat(compat::RetainedString name, float* result, float speed, float min, float max,
    const char* format) {
    const compat::SourceCString label{name};
    compat::detail::ngSourceCStringPostconditions(name.view(),label.data());
    return ImGui::DragFloat(label.data(), result, speed, min, max, format);
}
float incInspectorDeformGetValue(const std::shared_ptr<Node>& node, const std::shared_ptr<Parameter>& parameter,
    compat::RetainedString name, Cursor cursor) {
    const auto target = node;
    const auto param = parameter;
    float value = compat::ngRequireObject(target)->getDefaultValueSource(name);
    if (auto binding = std::dynamic_pointer_cast<nicxlive::core::param::ValueParameterBinding>(
        compat::ngRequireObject(param)->getBinding(target, name.toNullableString())))
        value = binding->getValue({cursor.x, cursor.y});
    return value;
}

namespace detail {
compat::RetainedString formatInspectorText(std::string_view pattern,
    std::span<const core::translation::TypedFormatArgument> arguments) {
    std::string result;
    const auto consumed = core::translation::detail::formatTypedTo([&](std::string_view bytes) {
        try {
            result.append(bytes);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }, pattern, arguments);
    if (consumed != arguments.size()) {
        auto message = inspectorTextBacking([&] {
            return "Orphan format arguments: args[" + std::to_string(consumed) + ".." +
                std::to_string(arguments.size()) + "]";
        });
        core::translation::detail::formatFailure(std::move(message), "std/format/package.d", 785);
    }
    if (result.empty()) return {};
    return inspectorNameBytes([&] { return std::move(result); });
}

void inspectWithPolicy(const std::function<void()>& contents) {
    if (api::mcp::task::ngMcpExternalCommandsBlocked()) widgets::ngWithReadOnlyCategoryContents(contents);
    else contents();
}

void inspectSelectionHeader(Selection<Node> targets, ModelEditSubMode mode) {
    if (targets.size() == 1) {
        if (mode == ModelEditSubMode::Layout) incModelModeHeader(targets[0]);
        else if (mode == ModelEditSubMode::Deform) incCommonNonEditHeader(targets[0]);
    } else if (targets.size() > 1) ngMultiEditHeader(std::move(targets));
}
bool beginSharedCombo(compat::RetainedStringSlice identifier) {
    const compat::SourceCString id(identifier);
    compat::detail::ngSourceCStringPostconditions(identifier.view(),id.data());
    const auto preview = core::translation::ngTranslateCString("Select Value");
    return ImGui::BeginCombo(id.data(), preview.data());
}
bool selectSharedValue(compat::RetainedStringSlice label) {
    const compat::SourceCString text(label);
    compat::detail::ngSourceCStringPostconditions(label.view(),text.data());
    return ImGui::Selectable(text.data(), false);
}
void endSharedCombo() { ImGui::EndCombo(); }
void beginSharedGroup() { ImGui::BeginGroup(); }
void endSharedGroup() { ImGui::EndGroup(); }
void setSharedWidth() { ImGui::SetNextItemWidth(ImGui::CalcItemWidth()); }
float sharedWidth() { return ImGui::CalcItemWidth(); }
void setSharedWidth(float width) { ImGui::SetNextItemWidth(width); }
void pushInspectorActionGroup() { core::actionstack::incActionPushGroup(); }
void popInspectorActionGroup() { core::actionstack::incActionPopGroup(); }
bool editInspectorColor(float (&rgb)[3]) { return ImGui::ColorEdit3("###COLORADJ", rgb); }
float inspectorTextWidth(compat::RetainedStringSlice value) {
    const compat::SourceCString text(value);
    compat::detail::ngSourceCStringPostconditions(value.view(),text.data());
    return ImGui::CalcTextSize(text.data()).x;
}
bool beginInspectorColorCombo() {
    const auto preview = core::translation::ngTranslateCString("Select Color");
    return ImGui::BeginCombo("###COLOR_COMBO", preview.data());
}
void drawInspectorColorRow(std::shared_ptr<Node> node, const float (&rgb)[3], float width, bool& selected) {
    const compat::RetainedString rowName(compat::ngRequireObject(node)->getName());
    const auto rowLabel=inspectorPrefixedLabel("##row_",rowName.view());
    const compat::SourceCString row(rowLabel);
    compat::detail::ngSourceCStringPostconditions(rowLabel.view(),row.data());
    selected = ImGui::Selectable(row.data(), false, ImGuiSelectableFlags_None, ImVec2(ImGui::CalcItemWidth(), 20));
    ImGui::SameLine();
    const compat::RetainedString textName(compat::ngRequireObject(node)->getName());
    const compat::SourceCString text(textName);
    compat::detail::ngSourceCStringPostconditions(textName.view(),text.data());
    // The source passes the live name as igText's format string.
    ImGui::Text(text.data());
    ImGui::SameLine(width + 10);
    const compat::RetainedString colorName(compat::ngRequireObject(node)->getName());
    const auto colorLabel=inspectorPrefixedLabel("##color_",colorName.view());
    const compat::SourceCString color(colorLabel);
    compat::detail::ngSourceCStringPostconditions(colorLabel.view(),color.data());
    ImGui::ColorButton(color.data(), ImVec4(rgb[0], rgb[1], rgb[2], 1), ImGuiColorEditFlags_None, ImVec2(20, 20));
}
} // namespace detail
} // namespace nijigenerate::panels::inspector
