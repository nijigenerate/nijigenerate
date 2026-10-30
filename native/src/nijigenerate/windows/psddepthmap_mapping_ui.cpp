#include "psddepthmap.hpp"
#include "detail/psddepthmap_helpers.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/texture.hpp>
#include <nijigenerate/commands/depth/psd_dialog_commands.hpp>
#include <algorithm>
#include <bit>
#include <type_traits>
namespace nijigenerate::windows::psddepthmap {
using detail::helpers::allocatePsdDepthMemory;
using detail::helpers::decimal;
using detail::helpers::keyedText;
using detail::helpers::prefixedDecimal;
using detail::helpers::sourceCString;
namespace {
void text(compat::RetainedString value) { widgets::ngText(compat::RetainedStringSlice(value)); }
void text(std::string_view value) { widgets::ngText(value); }
void text(const std::string& value) { widgets::ngText(value); }
void text(const char* value) { widgets::ngText(value); }
template<class... T> std::string format(std::string_view pattern, T&&... values) {
    const std::array<core::translation::TypedFormatArgument,sizeof...(T)> arguments{
        core::translation::ngFormatArgument(std::forward<T>(values))...};
    return core::translation::ngFormatTyped(pattern, arguments);
}
}
void PSDDepthMapWindow::drawCompositePreviewTooltip(PsdDepthGridResult& gridResult) {
    const auto keepAlive = weak_from_this().lock();
    const auto gridStorage = preview.grids;
    auto texture = compositePreviewTexture(gridResult);
    if (texture == nullptr) return;

    ImGui::BeginTooltip();
    text(detail::helpers::tr("Composite Depth Preview"));
    const auto name = gridResult.grid != nullptr
        ? compat::RetainedString(compat::ngRequireObject(gridResult.grid)->getName())
        : compat::RetainedString::fromStaticLiteral("-");
    const auto width = gridResult.previewWidth;
    const auto height = gridResult.previewHeight;
    const auto left = gridResult.previewLeft;
    const auto top = gridResult.previewTop;
    text(format("%s  %dx%d  (%d, %d)", name, width, height, left, top));
    auto maxDimension = static_cast<float>(std::max(gridResult.previewWidth, gridResult.previewHeight));
    auto scale = maxDimension > 0 ? std::min(PreviewSize / maxDimension, 1.0f) : 1.0f;
    text(detail::helpers::tr("Raw PSD Depth"));
    const auto image = compat::ngRequireObject(texture)->getTextureId();
    ImGui::Image(
        static_cast<ImTextureID>(image),
        ImVec2(static_cast<float >(gridResult.previewWidth) * scale, static_cast<float >(gridResult.previewHeight) * scale)
    );
    ImGui::EndTooltip();
}

void PSDDepthMapWindow::ensureSelectedGridIndex() {
    auto other = hasOtherMappings();
    if (preview.grids.size() == 0 && !other) {
        selectedGridIndex = -1;
        return;
    }
    using UnsignedIndex = std::make_unsigned_t<std::ptrdiff_t>;
    const auto maxIndex = std::bit_cast<std::ptrdiff_t>(static_cast<UnsignedIndex>(preview.grids.size()) +
        static_cast<UnsignedIndex>(other ? 1 : 0));
    if (selectedGridIndex < 0 || selectedGridIndex >= maxIndex) {
        selectedGridIndex = 0;
    }
}

void PSDDepthMapWindow::drawGridList(float height) {
    ensureSelectedGridIndex();
    if (selectedGridIndex < 0) return;
    if (ImGui::BeginTable("###PsdDepthGridPreview", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, height))) {
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Use").data(), ImGuiTableColumnFlags_WidthFixed, 46);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Preview").data(), ImGuiTableColumnFlags_WidthFixed, 116);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Target").data());
        ImGui::TableHeadersRow();
        auto gridsSnapshot = preview.grids;
        for (std::size_t i = 0; i < gridsSnapshot.size(); ++i) {
            auto& gridResult = compat::ngArrayAt(gridsSnapshot, i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawGridEnabledCheckbox(gridResult.grid);
            ImGui::TableNextColumn();
            auto texture = compositePreviewTexture(gridResult);
            auto selected = selectedGridIndex == static_cast<std::ptrdiff_t >(i);
            widgets::ngTextureSlotUntitled(compat::RetainedStringSlice(prefixedDecimal("###gridPreview", i)),
                texture, ImVec2(104, 104), 24, ImGuiWindowFlags_NoInputs, selected);
            if (ImGui::IsItemHovered()) drawCompositePreviewTooltip(gridResult);
            ImGui::TableNextColumn();
            const auto name = gridResult.grid != nullptr
                ? compat::RetainedString(compat::ngRequireObject(gridResult.grid)->getName())
                : compat::RetainedString::fromStaticLiteral("-");
            const auto sampledLabel = detail::helpers::tr("Sampled");
            const auto sampledVertices = static_cast<int>(gridResult.sampledVertices);
            const auto missingLabel = detail::helpers::tr("Missing");
            const auto missingVertices = static_cast<int>(gridResult.missingVertices);
            const auto coverageSources = static_cast<int>(gridResult.coverageSources);
            const auto minLabel = detail::helpers::tr("Min");
            const auto minDepth = gridResult.minDepth;
            const auto maxLabel = detail::helpers::tr("Max");
            const auto maxDepth = gridResult.maxDepth;
            const auto skippedLabel = gridResult.skipped
                ? detail::helpers::tr("Skipped") : detail::helpers::tr("Will Apply");
            auto label = format("%s\n%s: %d  %s: %d\n%s: %d\n%s: %.3f  %s: %.3f\n%s",
                name, sampledLabel, sampledVertices, missingLabel, missingVertices,
                "Coverage", coverageSources, minLabel, minDepth, maxLabel, maxDepth, skippedLabel);
            const auto rowLabel = allocatePsdDepthMemory([&] {
                auto value = label + "###gridRow";
                const auto suffix = std::to_string(i);
                value.append(suffix);
                return compat::RetainedString(std::move(value));
            });
            const auto rowText = sourceCString(rowLabel);
            if (ImGui::Selectable(rowText.data(), selected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 104))) {
                selectedGridIndex = static_cast<std::ptrdiff_t >(i);
            }
        }
        if (hasOtherMappings()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            text("-");
            ImGui::TableNextColumn();
            auto selected = isOthersSelected();
            auto texture = firstOtherPreviewTexture();
            widgets::ngTextureSlotUntitled("###gridPreviewOthers", texture, ImVec2(104, 104), 24, ImGuiWindowFlags_NoInputs, selected);
                if (ImGui::IsItemHovered()) {
                    auto layerPath = firstOtherLayerPath();
                    if (layerPath.size()) drawLayerPreviewTooltip(layerPath, 0, true);
                }
            ImGui::TableNextColumn();
            const auto others = detail::helpers::tr("Others");
            const auto unmapped = detail::helpers::tr("Unmapped or ignored layers");
            const auto otherLabel = allocatePsdDepthMemory([&] {
                return compat::RetainedString(std::string(others.view()) + "\n" +
                    std::string(unmapped.view()) + "###gridRowOthers");
            });
            const auto otherText = sourceCString(otherLabel);
            if (ImGui::Selectable(otherText.data(),
                selected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 104))) {
                selectedGridIndex = static_cast<std::ptrdiff_t >(preview.grids.size());
            }
        }
        ImGui::EndTable();
    }
}


void PSDDepthMapWindow::drawLayerZControls(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
    const auto keepAlive = weak_from_this().lock();
    const auto keyValue = keyedText("layerZControls", layerPath, targetGridUuid);
    const auto keyText = sourceCString(keyValue);
    ImGui::PushID(keyText.data());
    struct Pop { ~Pop() { ImGui::PopID(); } } pop;
    auto transform = layerTransform(layerPath, targetGridUuid);
    if (widgets::ngCheckbox(core::translation::ngTranslateCString("Invert Depth").data(), &transform.invert)) {
        auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthInverted>(ctx, layerPath, transform.invert);
    }
    if (ImGui::DragFloat(core::translation::ngTranslateCString("Z Scale").data(),
        &transform.zScale, 0.01f, -100.0f, 100.0f, "%.3f")) {
        auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthScale>(ctx, layerPath, transform.zScale);
    }
    if (ImGui::DragFloat(core::translation::ngTranslateCString("Z Offset").data(),
        &transform.zOffset, 0.01f, -100.0f, 100.0f, "%.3f")) {
        auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthOffset>(ctx, layerPath, transform.zOffset);
    }
    if (widgets::ngButtonColored(core::translation::ngTranslateCString("Reset Z").data(), ImVec2(120, 0))) {
        auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::ResetPsdDepthDialogLayerDepthTransform>(ctx, layerPath);
    }
}

void PSDDepthMapWindow::drawMappingLayerRow(PsdDepthLayerMapping& mapping, DeformableRef grid, PsdDepthGridLayerMask* layerMask) {
    const auto keepAlive = weak_from_this().lock();
    bool problem = !mapping.matched || mapping.ambiguous || mapping.ignored;
    if (onlyProblemLayers && !problem) return;
    const auto mappingStorage = preview.mappings;
    const auto composedStorage = preview.composedLayers;
    auto previewLayer = findLayerPreview(mapping.layerPath, mapping.targetGridUuid);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    drawComposedLayerEnabledCheckbox(mapping.layerPath, mapping.targetGridUuid);
    ImGui::TableNextColumn();
    drawLayerPreviewHoverText(mapping.layerPath, mapping.layerPath, mapping.targetGridUuid, false);
    ImGui::TableNextColumn();
    drawLayerPreviewHoverText(mapping.layerName, mapping.layerPath, mapping.targetGridUuid, true);
    ImGui::TableNextColumn();
    drawMatchedNodeText(mapping);
    ImGui::TableNextColumn();
    if (drawManualMappingCombo(mapping)) previewDirty = true;
    ImGui::TableNextColumn();
    text(mapping.status);
    ImGui::TableNextColumn();
    if (layerMask != nullptr) text(decimal(layerMask->sampledVertices));
    else text(compat::RetainedString::fromStaticLiteral("-"));
    ImGui::TableNextColumn();
    if (layerMask != nullptr) text(decimal(layerMask->selectedVertices));
    else text(compat::RetainedString::fromStaticLiteral("-"));
    ImGui::TableNextColumn();
    if (previewLayer != nullptr) text(decimal(previewLayer->depthStats.maskedPixels));
    else text(compat::RetainedString::fromStaticLiteral("-"));
    ImGui::TableNextColumn();
    if (previewLayer != nullptr) text(decimal(previewLayer->depthStats.zeroPixels));
    else text(compat::RetainedString::fromStaticLiteral("-"));
    ImGui::TableNextColumn();
    if (previewLayer != nullptr && previewLayer->depthStats.hasDepth) {
        text(format("%.3f", previewLayer->depthStats.rangeDepth01));
    } else text(compat::RetainedString::fromStaticLiteral("-"));
}

void PSDDepthMapWindow::drawSelectedGridLayers(float height) {
    const auto keepAlive = weak_from_this().lock();
    ensureSelectedGridIndex();
    if (selectedGridIndex < 0) return;

    if (isOthersSelected()) {
        text(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Target: Others")));
    } else {
        auto gridResult = compat::ngArrayAt(preview.grids, static_cast<std::size_t>(selectedGridIndex));
        const auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Target: %s"));
        const auto name = gridResult.grid != nullptr
            ? compat::RetainedString(compat::ngRequireObject(gridResult.grid)->getName())
            : compat::RetainedString::fromStaticLiteral("-");
        text(format(pattern.view(), name));
    }

    if (ImGui::BeginTable("###PsdDepthGridMasks", 11, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, height))) {
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Use").data(), ImGuiTableColumnFlags_WidthFixed, 46);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Path").data());
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Depth Layer").data());
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Matched Node").data());
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Remap").data());
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Status").data());
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Sampled").data(), ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Selected").data(), ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Mask px").data(), ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Zero px").data(), ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Range").data(), ImGuiTableColumnFlags_WidthFixed, 64);
        ImGui::TableHeadersRow();
        if (isOthersSelected()) {
            for (auto mappings = preview.mappings; auto& mapping : mappings) {
                if (mapping.matched && mapping.targetGridUuid != 0) continue;
                drawMappingLayerRow(mapping);
            }
        } else {
            auto gridResult = compat::ngArrayAt(preview.grids, static_cast<std::size_t>(selectedGridIndex));
            for (auto masks = gridResult.layerMasks; auto& layerMask : masks) {
                const auto layerPath = layerMask.layerPath;
                const auto uuid = compat::ngRequireObject(gridResult.grid)->getUUID();
                // Keep the queried array cells alive through callbacks in the row.
                const auto mappingStorage = preview.mappings;
                auto mapping = findMapping(layerPath, uuid);
                if (mapping == nullptr) continue;
                drawMappingLayerRow(*mapping, gridResult.grid, &layerMask);
            }
        }
        ImGui::EndTable();
    }
}

void PSDDepthMapWindow::drawComposedSourceLayers(float height) {
    const auto keepAlive = weak_from_this().lock();
    if (preview.composedLayers.size() == 0) return;
    text(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Composed Source Layers")));
    if (ImGui::BeginTable("###PsdDepthComposedSourceLayers", 8,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
        ImVec2(0, height))) {
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Use").data(), ImGuiTableColumnFlags_WidthFixed, 46);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Preview").data(), ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Source Layer").data());
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Target").data());
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Mask px").data(), ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Zero px").data(), ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Range").data(), ImGuiTableColumnFlags_WidthFixed, 64);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Status").data(), ImGuiTableColumnFlags_WidthFixed, 92);
        ImGui::TableHeadersRow();
        auto layers = preview.composedLayers;
        for (std::size_t i = 0; i < layers.size(); ++i) {
            auto& layer = compat::ngArrayAt(layers, i);
            auto layerPath = layer.layerPath.size() ? layer.layerPath : layer.id;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawComposedLayerEnabledCheckbox(layerPath, layer.targetGridUuid);
            ImGui::TableNextColumn();
            auto texture = layerPreviewTexture(layerPath, layer.targetGridUuid, true);
            widgets::ngTextureSlotUntitled(compat::RetainedStringSlice(
                prefixedDecimal("###composedSourceLayerPreview", i)), texture,
                ImVec2(60, 60), 18, ImGuiWindowFlags_NoInputs, false);
            if (ImGui::IsItemHovered() && texture != nullptr) {
                drawLayerPreviewTooltip(layerPath, layer.targetGridUuid, true);
            }
            ImGui::TableNextColumn();
            drawLayerPreviewHoverText(layer.colorLayerName.size() ? layer.colorLayerName : layer.layerName,
                layerPath, layer.targetGridUuid, true);
            ImGui::TableNextColumn();
            text(layer.targetGridName.size() ? layer.targetGridName : "-");
            ImGui::TableNextColumn();
            text(decimal(layer.depthStats.maskedPixels));
            ImGui::TableNextColumn();
            text(decimal(layer.depthStats.zeroPixels));
            ImGui::TableNextColumn();
            text(layer.depthStats.hasDepth ? format("%.3f", layer.depthStats.rangeDepth01) : "-");
            ImGui::TableNextColumn();
            const auto stateLabel = layer.enabled
                ? (layer.targetGridUuid != 0
                    ? core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Bound"))
                    : core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Unbound")))
                : core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Disabled"));
            text(stateLabel);
        }
        ImGui::EndTable();
    }
}

void PSDDepthMapWindow::drawGridPreview(float height) {
    const auto keepAlive = weak_from_this().lock();
    if (preview.grids.size() == 0 && !hasOtherMappings()) return;
    auto leftWidth = std::max(260.0f, widgets::ngAvailableSpace().x * 0.38f);
    if (ImGui::BeginTable("###PsdDepthGridReviewLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp, ImVec2(0, height))) {
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Targets").data(), ImGuiTableColumnFlags_WidthFixed, leftWidth);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Depth Layers").data());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        drawGridList(height);
        ImGui::TableNextColumn();
        drawSelectedGridLayers(height);
        ImGui::EndTable();
    }
}

void PSDDepthMapWindow::drawSourceMappingTab(float height) {
    const auto keepAlive = weak_from_this().lock();
    auto lines = diagnostics();
    if (lines.size()) {
        drawDiagnostics();
        if (errorMessage.size()) return;
        ImGui::Separator();
    }
    if (preview.grids.size() == 0 && !hasOtherMappings() && preview.composedLayers.size() == 0) {
        text(core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "No mapped targets were found. Remap a source layer to a GridDeformer or PathDeformer.")));
        return;
    }
    if (preview.composedLayers.size() > 0) {
        auto sourceHeight = std::min(220.0f, std::max(120.0f, height * 0.34f));
        drawComposedSourceLayers(sourceHeight);
        ImGui::Separator();
        drawGridPreview(std::max(120.0f, height - sourceHeight - 16.0f));
    } else {
        drawGridPreview(height);
    }
}

std::shared_ptr<PsdDepthComposedLayer> PSDDepthMapWindow::selected3DAdjustLayerPreview() {
    if (preview.composedLayers.size() == 0) return nullptr;
    using UnsignedIndex = std::make_unsigned_t<std::ptrdiff_t>;
    const auto lastIndex = std::bit_cast<std::ptrdiff_t>(
        static_cast<UnsignedIndex>(preview.composedLayers.size()) - UnsignedIndex{1});
#ifndef NDEBUG
    if (lastIndex < 0) {
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Lower can't be greater than upper."),
            compat::RetainedString::fromStaticLiteral("std/algorithm/comparison.d"), 588);
    }
#endif
    selected3DAdjustLayerIndex = selected3DAdjustLayerIndex < 0 ? 0 :
        selected3DAdjustLayerIndex > lastIndex ? lastIndex : selected3DAdjustLayerIndex;
    auto layers = preview.composedLayers;
    return detail::helpers::retainedElement(layers,
        &compat::ngArrayAt(layers, static_cast<std::size_t>(selected3DAdjustLayerIndex)));
}

void PSDDepthMapWindow::draw3DAdjustLayerControlsPanel(float height) {
    const auto keepAlive = weak_from_this().lock();
    const auto layersLabel = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Layers"));
    const auto layerCount = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(preview.composedLayers.size()));
    text(format("%s: %d", layersLabel, layerCount));
    const auto compositionLabel = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Composition"));
    const auto compositionModeName = preview.compositionModeName;
    text(format("%s: %s", compositionLabel, compositionModeName));
    ImGui::Separator();

    if (preview.composedLayers.size() == 0) return;
    auto listHeight = std::clamp(height * 0.34f, 140.0f, std::max(140.0f, height - 260.0f));
    if (ImGui::BeginChild("###PsdDepth3DLayerList", ImVec2(0, listHeight), true)) {
        auto layers = preview.composedLayers;
        for (std::size_t i = 0; i < layers.size(); ++i) {
            auto& layerPreview = compat::ngArrayAt(layers, i);
            const auto index = std::bit_cast<std::ptrdiff_t>(
                static_cast<std::make_unsigned_t<std::ptrdiff_t>>(i));
            auto selected = selected3DAdjustLayerIndex == index;
            auto label = format("%s\n%d x %d  (%d, %d)###psdDepth3DLayer%d", 
                layerPreview.layerName.size() ? layerPreview.layerName : layerPreview.layerPath,
                layerPreview.width,
                layerPreview.height,
                layerPreview.left,
                layerPreview.top,
                std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i))
            );
            const auto labelValue = allocatePsdDepthMemory([&] {
                return compat::RetainedString(std::move(label));
            });
            const auto labelText = sourceCString(labelValue);
            if (ImGui::Selectable(labelText.data(), selected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 42))) {
                selected3DAdjustLayerIndex = index;
            }
        }
    }
    ImGui::EndChild();

    auto selectedLayer = selected3DAdjustLayerPreview();
    if (selectedLayer == nullptr) return;
    auto layerPreview = *selectedLayer;

    ImGui::Separator();
    if (ImGui::BeginChild("###PsdDepth3DLayerDetail", ImVec2(0, 0), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        drawLayerPreviewHoverText(
            layerPreview.layerName,
            layerPreview.layerPath,
            layerPreview.targetGridUuid,
            true);
        const auto layerLabel = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Layer"));
        const auto width = layerPreview.width;
        const auto height = layerPreview.height;
        const auto left = layerPreview.left;
        const auto top = layerPreview.top;
        text(format("%s: %dx%d  (%d, %d)", layerLabel, width, height, left, top));

        drawComposedLayerShowCheckbox(layerPreview.layerPath, layerPreview.targetGridUuid);
        drawComposedLayerDepthEnabledCheckbox(layerPreview.layerPath, layerPreview.targetGridUuid);
        if (layerPreview.depthEnabled) {
            drawLayerZControls(layerPreview.layerPath, layerPreview.targetGridUuid);
        } else {
            text(core::translation::ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("Attached to depth below")));
        }
    }
    ImGui::EndChild();
}
}
