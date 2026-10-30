#include "psddepthmap.hpp"
#include "detail/psddepthmap_helpers.hpp"
#include <nijigenerate/io/depthmap_psd_enum_functions.hpp>
#include <nijigenerate/viewport/depth/draw/gpu.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <puppet.hpp>
#include <nijigenerate/io/package.hpp>
#include <nijigenerate/io/depthmap_psd_targets.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/commands/depth/psd_dialog_commands.hpp>
namespace nijigenerate::windows::psddepthmap {
using namespace commands::depth::map;
using detail::helpers::allocatePsdDepthMemory;
using detail::helpers::decimal;
using detail::helpers::keyedText;
using detail::helpers::sourceCString;
using viewport::depth::draw::ngDepthDrawGpuLayerSampleSupportsConvolution;
bool PSDDepthMapWindow::drawEnumCombo(compat::RetainedString label, PsdDepthConvolution& value) {
    const auto keepAlive = weak_from_this().lock();
    auto current = ngPsdDepthConvolutionName(value);
    bool changed = false;
    const auto labelText = sourceCString(label);
    const auto currentText = sourceCString(current);
    if (ImGui::BeginCombo(labelText.data(), currentText.data())) {
        for (const char* name : ConvolutionNames) {
            const auto nameValue = compat::RetainedString::fromStaticView(name);
            auto option = ngPsdDepthConvolutionFromString(nameValue);
            if (settings.useGpuComposition && !ngDepthDrawGpuLayerSampleSupportsConvolution(
                static_cast<int>(ngPsdDepthConvolutionToDepthImage(option)))) continue;
            bool selected = current.view() == name;
            const auto optionText = sourceCString(nameValue);
            if (ImGui::Selectable(optionText.data(), selected)) {
                value = option;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool PSDDepthMapWindow::drawEnumCombo(compat::RetainedString label, PsdDepthMissingPolicy& value) {
    const auto keepAlive = weak_from_this().lock();
    auto current = ngPsdDepthMissingPolicyName(value);
    bool changed = false;
    const auto labelText = sourceCString(label);
    const auto currentText = sourceCString(current);
    if (ImGui::BeginCombo(labelText.data(), currentText.data())) {
        for (const char* name : MissingPolicyNames) {
            bool selected = current.view() == name;
            const auto nameValue = compat::RetainedString::fromStaticView(name);
            const auto optionText = sourceCString(nameValue);
            if (ImGui::Selectable(optionText.data(), selected)) {
                value = ngPsdDepthMissingPolicyFromString(nameValue);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool PSDDepthMapWindow::drawEnumCombo(compat::RetainedString label, PsdDepthChannel& value) {
    const auto keepAlive = weak_from_this().lock();
    auto current = ngPsdDepthChannelName(value);
    bool changed = false;
    const auto labelText = sourceCString(label);
    const auto currentText = sourceCString(current);
    if (ImGui::BeginCombo(labelText.data(), currentText.data())) {
        for (const char* name : ChannelNames) {
            bool selected = current.view() == name;
            const auto nameValue = compat::RetainedString::fromStaticView(name);
            const auto optionText = sourceCString(nameValue);
            if (ImGui::Selectable(optionText.data(), selected)) {
                value = ngPsdDepthChannelFromString(nameValue);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool PSDDepthMapWindow::isCustomConvolution() {
    switch (settings.convolution) {
        case PsdDepthConvolution::Nearest:
        case PsdDepthConvolution::Box3x3:
        case PsdDepthConvolution::Box5x5:
        case PsdDepthConvolution::Gaussian3x3:
        case PsdDepthConvolution::Gaussian5x5:
        case PsdDepthConvolution::Median3x3:
        case PsdDepthConvolution::Frontmost3x3:
        case PsdDepthConvolution::Backmost3x3:
            return false;
        case PsdDepthConvolution::BoxCustom:
        case PsdDepthConvolution::GaussianCustom:
        case PsdDepthConvolution::MedianCustom:
        case PsdDepthConvolution::FrontmostCustom:
        case PsdDepthConvolution::BackmostCustom:
            return true;
        default:
            throw compat::ngStaticError<compat::SwitchError>(
                compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/psddepthmap.d"), 1106);
    }
}


compat::Slice<DeformableRef> PSDDepthMapWindow::currentTargets() {
    auto puppet = project::incActivePuppet();
    if (!puppet || !puppet->root) return {};
    compat::Slice<DeformableRef> targets;
    for (auto grid : puppet->findNodesType<nicxlive::core::nodes::GridDeformer>(puppet->root))
        targets.append(grid);
    for (auto path : puppet->findNodesType<nicxlive::core::nodes::PathDeformer>(puppet->root))
        targets.append(path);
    return targets;
}
compat::RetainedString PSDDepthMapWindow::currentMappingLabel(PsdDepthLayerMapping& mapping) {
    const auto keepAlive = weak_from_this().lock();
    if (settings.ignoredLayerPaths.contains(mapping.layerPath.view()))
        return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Ignore"));
    const auto overridePath = mapping.layerPath;
    const auto overrides = settings.layerTargetGridUuidOverrides;
    if (auto overrideUuid = overrides.findRetained(overridePath.view())) {
        for (auto grid : currentTargets()) {
            const auto gridUuid = compat::ngRequireObject(grid)->getUUID();
            const auto uuid = decimal(gridUuid);
            if (uuid.view() == overrideUuid->view())
                return grid->getName().empty() ? *overrideUuid : compat::RetainedString(grid->getName());
        }
        return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Missing Target"));
    }
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Auto"));
}
void PSDDepthMapWindow::drawOptions() {
    const auto keepAlive = weak_from_this().lock();
    widgets::ngText(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Color Source"))));
    ImGui::SameLine();
    const auto colorLabel = settings.colorSourcePath.size() ? settings.colorSourcePath
        : core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("<active target/art>"));
    const std::array<core::translation::TypedFormatArgument, 1> colorArguments{
        core::translation::ngFormatArgument(colorLabel)};
    widgets::ngText(core::translation::ngFormatTyped("%s", colorArguments));
    ImGui::SameLine();
    const char* allPatterns[] = {"*.png", "*.psd"};
    const char* pngPatterns[] = {"*.png"};
    const char* psdPatterns[] = {"*.psd"};
    const io::TFD_Filter colorFilters[] = {
        {allPatterns, "Color source (*.png, *.psd)"},
        {pngPatterns, "Portable Network Graphics (*.png)"},
        {psdPatterns, "Photoshop Document (*.psd)"}
    };
    if (ImGui::Button(core::translation::ngTranslateCString("Select Color Source").data())) {
        auto colorPath = io::incShowImportDialog(colorFilters, core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Select Color Source...")));
        if (colorPath.size()) {
            auto ctx = dialogCommandContext();
            commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogColorSource>(ctx, colorPath.retainedString());
        }
    }
    if (settings.colorSourcePath.size()) {
        ImGui::SameLine();
        if (ImGui::Button(core::translation::ngTranslateCString("Clear Color Source").data())) {
            auto ctx = dialogCommandContext();
            commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<
                commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogColorSource>(
                    ctx, compat::RetainedString::emptyLiteral());
        }
    }
    auto invert = settings.invert;
    if (widgets::ngCheckbox(core::translation::ngTranslateCString("Invert Depth").data(), &invert)) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogInvert>(ctx, invert);
    }
    widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Default: white is front and black is back."))));
    auto backDepth = settings.backDepth;
    if (ImGui::DragFloat(core::translation::ngTranslateCString("Back Depth").data(),
        &backDepth, 0.01f, -10.0f, 10.0f, "%.3f")) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogBackDepth>(ctx, backDepth);
    }
    auto frontDepth = settings.frontDepth;
    if (ImGui::DragFloat(core::translation::ngTranslateCString("Front Depth").data(),
        &frontDepth, 0.01f, -10.0f, 10.0f, "%.3f")) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogFrontDepth>(ctx, frontDepth);
    }
    auto depthScale = settings.depthScale;
    if (ImGui::DragFloat(core::translation::ngTranslateCString("Depth Scale").data(),
        &depthScale, 0.01f, 0.0f, 100.0f, "%.3f")) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogDepthScale>(ctx, depthScale);
    }
    widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Imported depth values are multiplied by this scale before applying."))));
    auto channel = settings.channel;
    if (drawEnumCombo(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Channel")), channel)) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogChannel>(ctx, channel);
    }
    auto convolution = settings.convolution;
    if (drawEnumCombo(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Sampling")), convolution)) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogSampling>(ctx, convolution);
    }
    if (isCustomConvolution()) {
        auto customRadius = settings.customRadius;
        if (ImGui::DragInt(core::translation::ngTranslateCString("Custom Radius").data(), &customRadius, 0.1f, 1, 64)) {
            auto ctx = dialogCommandContext();
            commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogCustomRadius>(ctx, customRadius);
        }
    }
    auto alphaThreshold = settings.alphaThreshold;
    if (ImGui::DragFloat(core::translation::ngTranslateCString("Alpha Threshold").data(),
        &alphaThreshold, 0.001f, 0.0f, 1.0f, "%.3f")) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogAlphaThreshold>(ctx, alphaThreshold);
    }
    auto missingPolicy = settings.missingPolicy;
    if (drawEnumCombo(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Missing Vertex Pixel")), missingPolicy)) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogMissingPolicy>(ctx, missingPolicy);
    }
    auto repairContourBand = settings.repairContourBand;
    if (widgets::ngCheckbox(core::translation::ngTranslateCString("Repair contour band").data(), &repairContourBand)) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogContourRepair>(ctx, repairContourBand);
    }
    auto smoothWavySurface = settings.smoothWavySurface;
    if (widgets::ngCheckbox(core::translation::ngTranslateCString("Smooth wavy surface").data(), &smoothWavySurface)) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogSurfaceSmoothing>(ctx, smoothWavySurface);
    }
    if (ImGui::Button(core::translation::ngTranslateCString("Fill alpha-depth gaps").data())) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::FillPsdDepthDialogAlphaDepthGaps>(ctx);
    }
    auto useGpuComposition = settings.useGpuComposition;
    if (widgets::ngCheckbox(core::translation::ngTranslateCString("GPU Composition").data(), &useGpuComposition)) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogGpuComposition>(ctx, useGpuComposition);
    }
    widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(
            "When enabled, apply must use the GPU composition path. CPU fallback is treated as an error."))));
    auto matchDirectGridName = settings.matchDirectGridName;
    if (widgets::ngCheckbox(core::translation::ngTranslateCString("Direct Grid Name Match").data(), &matchDirectGridName)) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogDirectGridMatch>(ctx, matchDirectGridName);
    }
    widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Also match PSD layer names directly against GridDeformer names."))));
    auto problemFilter = onlyProblemLayers;
    if (widgets::ngCheckbox(core::translation::ngTranslateCString("Only show problem layers").data(), &problemFilter)) {
        auto ctx = dialogCommandContext();
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogProblemFilter>(ctx, problemFilter);
    }
}


bool PSDDepthMapWindow::drawManualMappingCombo(PsdDepthLayerMapping& mapping) {
    const auto keepAlive = weak_from_this().lock();
    bool changed = false;
    const auto current = currentMappingLabel(mapping);
    const auto widget = allocatePsdDepthMemory([&] {
        return compat::RetainedString("###map" + std::string(mapping.layerPath.view()));
    });
    const auto widgetText = sourceCString(widget);
    const auto currentText = sourceCString(current);
    if (ImGui::BeginCombo(widgetText.data(), currentText.data())) {
        const bool autoSelected = !settings.ignoredLayerPaths.contains(mapping.layerPath.view()) &&
            !settings.layerTargetGridUuidOverrides.contains(mapping.layerPath.view());
        const auto autoLabel = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Auto"));
        const auto autoText = sourceCString(autoLabel);
        if (ImGui::Selectable(autoText.data(), autoSelected)) {
            auto ctx = dialogLayerCommandContext(mapping.layerPath, mapping.targetGridUuid);
            changed = compat::ngRequireObject(commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<
                commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingAuto>(
                    ctx, mapping.layerPath))->succeeded;
        }
        const bool ignoreSelected = settings.ignoredLayerPaths.contains(mapping.layerPath.view());
        const auto ignoreLabel = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Ignore"));
        const auto ignoreText = sourceCString(ignoreLabel);
        if (ImGui::Selectable(ignoreText.data(), ignoreSelected)) {
            auto ctx = dialogLayerCommandContext(mapping.layerPath, mapping.targetGridUuid);
            changed = compat::ngRequireObject(commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<
                commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingIgnored>(
                    ctx, mapping.layerPath))->succeeded;
        }
        ImGui::Separator();
        for (auto grid : currentTargets()) {
            const auto gridUuid = compat::ngRequireObject(grid)->getUUID();
            const auto uuid = decimal(gridUuid);
            bool selected = false;
            if (auto overrideUuid = settings.layerTargetGridUuidOverrides.findRetained(mapping.layerPath.view()))
                selected = overrideUuid->view() == uuid.view();
            const auto label = grid->getName().empty() ? uuid : compat::RetainedString(grid->getName());
            const auto labelText = sourceCString(label);
            if (ImGui::Selectable(labelText.data(), selected)) {
                auto ctx = dialogLayerCommandContext(mapping.layerPath, mapping.targetGridUuid);
                changed = compat::ngRequireObject(commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<
                    commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingTarget>(
                        ctx, mapping.layerPath, std::static_pointer_cast<nicxlive::core::nodes::Node>(grid)))->succeeded;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}
void PSDDepthMapWindow::drawGridEnabledCheckbox(DeformableRef grid) {
    const auto keepAlive = weak_from_this().lock();
    if (!grid) { widgets::ngText("-"); return; }
    const auto keyUuid = compat::ngRequireObject(grid)->getUUID();
    const auto key = decimal(keyUuid);
    auto& currentSettings = settings;
    const auto uuid = compat::ngRequireObject(grid)->getUUID();
    bool enabled = ngPsdDepthGridEnabled(currentSettings, uuid);
    const auto label = allocatePsdDepthMemory([&] {
        return compat::RetainedString("###useGrid" + std::string(key.view()));
    });
    const auto labelText = sourceCString(label);
    if (widgets::ngCheckbox(labelText.data(), &enabled)) {
        auto ctx = dialogNodeCommandContext(grid);
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogTargetEnabled>(ctx, enabled);
    }
}
void PSDDepthMapWindow::drawComposedLayerEnabledCheckbox(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
    const auto keepAlive = weak_from_this().lock();
    auto layer = findComposedLayer(layerPath, targetGridUuid);
    if (!layer) return;
    bool enabled = layer->enabled;
    const auto keyValue = keyedText("###useComposedLayer", layerPath, targetGridUuid);
    const auto keyText = sourceCString(keyValue);
    if (widgets::ngCheckbox(keyText.data(), &enabled)) {
        auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerEnabled>(ctx, layerPath, enabled);
    }
}
void PSDDepthMapWindow::drawComposedLayerShowCheckbox(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
    const auto keepAlive = weak_from_this().lock();
    auto layer = findComposedLayer(layerPath, targetGridUuid);
    if (!layer) return;
    bool visible = layer->visible;
    const auto keyValue = keyedText("showComposedLayer", layerPath, targetGridUuid);
    const auto keyText = sourceCString(keyValue);
    ImGui::PushID(keyText.data());
    struct Pop { ~Pop() { ImGui::PopID(); } } pop;
    if (widgets::ngCheckbox(core::translation::ngTranslateCString("Show").data(), &visible)) {
        auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerVisible>(ctx, layerPath, visible);
    }
}
void PSDDepthMapWindow::drawComposedLayerDepthEnabledCheckbox(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
    const auto keepAlive = weak_from_this().lock();
    auto layer = findComposedLayer(layerPath, targetGridUuid);
    if (!layer) return;
    bool enabled = layer->depthEnabled;
    const auto keyValue = keyedText("depthEnabledComposedLayer", layerPath, targetGridUuid);
    const auto keyText = sourceCString(keyValue);
    ImGui::PushID(keyText.data());
    struct Pop { ~Pop() { ImGui::PopID(); } } pop;
    if (widgets::ngCheckbox(core::translation::ngTranslateCString("Enable Depth").data(), &enabled)) {
        auto ctx = dialogLayerCommandContext(layerPath, targetGridUuid);
        commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthEnabled>(ctx, layerPath, enabled);
    }
    widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(
            "When disabled, use the nearest enabled depth below at each document coordinate."))));
}
}
