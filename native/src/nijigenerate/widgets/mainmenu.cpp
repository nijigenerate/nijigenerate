#include "mainmenu.hpp"
#include <nijigenerate/commands/menu_item.hpp>
#include <nijigenerate/commands/puppet/base.hpp>
#include <nijigenerate/commands/view/panel_registry.hpp>
#include <nijigenerate/panels/package.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/logo.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/path.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/io/autosave.hpp>
#include <nijigenerate/io/save.hpp>
#include <nijigenerate/io/videoexport.hpp>
#include <nijigenerate/api/mcp/task.hpp>
#include <nijigenerate/viewport/common/mesheditor/brushstate.hpp>
#include <nijigenerate/windows/about.hpp>
#include <nijigenerate/utils/link.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/config.hpp>
#include <texture.hpp>
#include <array>
#include <limits>
#include <climits>
#include <optional>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>
namespace nijigenerate::widgets {
namespace {
using core::translation::ngTranslateCString;
template<class Operation> decltype(auto) allocateMenuMemory(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::SourceCString menuCString(std::string_view input) {
    compat::SourceCString result(input);
    compat::detail::ngSourceCStringPostconditions(input, result.data());
    return result;
}
template<std::size_t N> compat::RetainedStringSlice translated(const char (&text)[N]) {
    return compat::RetainedStringSlice(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(text)));
}
template<class... T> compat::RetainedString format(std::string_view pattern,T&&... values) {
    const std::array args{core::translation::ngFormatArgument(std::forward<T>(values))...};
    auto text = core::translation::ngFormatTyped(pattern,args);
    return allocateMenuMemory([&] { return compat::RetainedString(std::move(text)); });
}
compat::RetainedStringSlice basename(compat::RetainedStringSlice path) {
    const auto result = compat::phobos::directory_detail::baseName(path.view());
    if (!result.data()) return {};
    const auto offset = static_cast<std::size_t>(result.data() - path.data());
    return path.slice(offset, offset + result.size());
}
float clampWidth(float x) { return x < 0.f ? 0.f : static_cast<float>(INT_MAX) < x ? static_cast<float>(INT_MAX) : x; }
}
using namespace project;
using namespace commands;
using namespace commands::puppet::base;
using commands::puppet::file::FileCommand;
using commands::puppet::edit::EditCommand;
using commands::puppet::view::ViewCommand;
using commands::puppet::tool::ToolCommand;
using commands::node::node::NodeCommand;
using commands::view::panel::ensureTogglePanelCommand;
using core::shortcut::ngShortcutFor;
using namespace core::actionstack;
using namespace core::settings;
using namespace core;
using namespace io::autosave;
using namespace io::save;
using namespace io::videoexport;
using namespace config;
using core::path::incGetAppConfigPath;
using api::mcp::task::ngMcpExternalCommandsBlocked;
void incMainMenu() {

    bool editingAllowed = !ngMcpExternalCommandsBlocked();
    auto& io = ImGui::GetIO();
        auto ctx = std::allocate_shared<commands::Context>(
            nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
        ctx->puppet(incActivePuppet());
        if (incSelectedNodes().size() > 0)
            ctx->nodes(incSelectedNodes());
    

        if (!incSettingsGet("hasDoneQuickSetup", false)) ImGui::BeginDisabled();

        if(ImGui::BeginMainMenuBar()) {
                
            ImVec2 pos;
            pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(pos.x-(ImGui::GetStyle().WindowPadding.x/2), pos.y));

            ImVec2 avail;
            avail = ImGui::GetContentRegionAvail();
                ImGui::Image(
                    static_cast<ImTextureID>(compat::ngRequireObject(core::ngGetLogoI2D())->getTextureId()), 
                    ImVec2(avail.y*2, avail.y*2), 
                    ImVec2(0, 0), ImVec2(1, 1), 
                    ImVec4(1, 1, 1, 1), 
                    ImVec4(0, 0, 0, 0)
                );
                
                ImGui::Separator();

                if (ImGui::BeginMenu(ngTranslateCString("File").data(), editingAllowed)) {
                    commands::ngMenuItemFor<FileCommand::NewFile>(ctx);

                    commands::ngMenuItemFor<FileCommand::ShowOpenFileDialog>(ctx);

                    auto prevProjects = incGetPrevProjects();
                    auto prevAutosaves = incGetPrevAutosaves();
                    if (ImGui::BeginMenu(ngTranslateCString("Recent").data(), prevProjects.size() > 0)) {
                        
                        if (ImGui::BeginMenu(ngTranslateCString("Autosaves").data(), prevAutosaves.size() > 0)) {
                            for (const auto saveRecord : prevAutosaves) {
                                const auto name = basename(saveRecord.autosavePath);
                                const auto autosavePath = menuCString(name.view());
                                if (ImGui::MenuItem(autosavePath.data(), "", false, true)) {
                                    incOpenProject(
                                        saveRecord.mainsavePath,
                                        saveRecord.autosavePath
                                    );
                                }
                                ngTooltip(saveRecord.autosavePath);
                            }
                            ImGui::EndMenu();
                        }

                        for (const auto project : incGetPrevProjects()) {
                            const auto name = basename(project);
                            const auto projectPath = menuCString(name.view());
                            if (ImGui::MenuItem(projectPath.data(), "", false, true)) {
                                incOpenProject(project);
                            }
                            ngTooltip(project);
                        }
                        ImGui::EndMenu();
                    }
                    
                    commands::ngMenuItemFor<FileCommand::ShowSaveFileDialog>(ctx);
                    
                    commands::ngMenuItemFor<FileCommand::ShowSaveFileAsDialog>(ctx);

                    if (ImGui::BeginMenu(ngTranslateCString("Import").data(), true)) {
                        commands::ngMenuItemFor<FileCommand::ShowImportPSDDialog>(ctx);
                        ngTooltip(translated("Import a standard Photoshop PSD file."));
                        commands::ngMenuItemFor<FileCommand::ShowImportPSDDepthMapDialog>(ctx);
                        ngTooltip(translated("Import PSD or PNG depth sources as deformer depth maps."));
                        commands::ngMenuItemFor<FileCommand::ShowImportKRADialog>(ctx);
                        ngTooltip(translated("Import a standard Krita KRA file."));

                        commands::ngMenuItemFor<FileCommand::ShowImportINPDialog>(ctx);
                        ngTooltip(translated("Import existing puppet file, editing options limited"));

                        commands::ngMenuItemFor<FileCommand::ShowImportImageFolderDialog>(ctx);
                        ngTooltip(translated("Supports PNGs, TGAs and JPEGs."));
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu(ngTranslateCString("Merge").data(), true)) {
                        commands::ngMenuItemFor<FileCommand::ShowMergePSDDialog>(ctx);
                        ngTooltip(translated("Merge layers from Photoshop document"));

                        commands::ngMenuItemFor<FileCommand::ShowMergeKRADialog>(ctx);
                        ngTooltip(translated("Merge layers from Krita document"));

                        commands::ngMenuItemFor<FileCommand::ShowMergeImageFileDialog>(ctx);
                        ngTooltip(translated("Merges (adds) selected image files to project"));

                        commands::ngMenuItemFor<FileCommand::ShowMergeINPDialog>(ctx);
                        ngTooltip(translated("Merge another nijigenerate project in to this one"));
                        
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu(ngTranslateCString("Export").data(), true)) {
                        commands::ngMenuItemFor<FileCommand::ShowExportToINPDialog>(ctx);
                        if (ImGui::BeginMenu(ngTranslateCString("Image").data(), true)) {
                            commands::ngMenuItemFor<FileCommand::ShowExportToPNGDialog>(ctx);

                            commands::ngMenuItemFor<FileCommand::ShowExportToJpegDialog>(ctx);

                            commands::ngMenuItemFor<FileCommand::ShowExportToTGADialog>(ctx);

                            ImGui::EndMenu();
                        }
                        commands::ngMenuItemFor<FileCommand::ShowExportToVideoDialog>(ctx, false, incVideoCanExport());
                        ImGui::EndMenu();
                    }

                    // Close Project option
                    commands::ngMenuItemFor<FileCommand::CloseProject>(ctx);

                    // Quit option
                    if (ImGui::MenuItem(ngTranslateCString("Quit").data(), "Alt+F4", false, true)) incExitSaveAsk();
                    ImGui::EndMenu();
                }
                
                if (ImGui::BeginMenu(ngTranslateCString("Edit").data(), editingAllowed)) {
                    commands::ngMenuItemFor<EditCommand::Undo>(ctx, false, incActionCanUndo());
                    commands::ngMenuItemFor<EditCommand::Redo>(ctx, false, incActionCanRedo());
                    
                    ImGui::Separator();
                    // Enable via Command.runnable; no hard-coded enabled flags
                    commands::ngMenuItemFor<NodeCommand::CutNode>(ctx);
                    commands::ngMenuItemFor<NodeCommand::CopyNode>(ctx);
                    commands::ngMenuItemFor<NodeCommand::PasteNode>(ctx);

                    ImGui::Separator();
                    commands::ngMenuItemFor<EditCommand::ShowSettingsWindow>(ctx);
                    commands::ngMenuItemFor<EditCommand::ShowCommandBrowserWindow>(ctx);
                    
                    #ifndef NDEBUG
                    {
                        ImGui::Spacing();
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(0.7, 0.5, 0.5, 1), ngTranslateCString("ImGui Debugging").data());

                        ImGui::Separator();
                        if(ImGui::MenuItem(ngTranslateCString("Style Editor").data(), "", dbgShowStyleEditor, true)) dbgShowStyleEditor = !dbgShowStyleEditor;
                        if(ImGui::MenuItem(ngTranslateCString("ImGui Debugger").data(), "", dbgShowDebugger, true)) dbgShowDebugger = !dbgShowDebugger;
                        if(ImGui::MenuItem(ngTranslateCString("ImGui Metrics").data(), "", dbgShowMetrics, true)) dbgShowMetrics = !dbgShowMetrics;
                        if(ImGui::MenuItem(ngTranslateCString("ImGui Stack Tool").data(), "", dbgShowStackTool, true)) dbgShowStackTool = !dbgShowStackTool;
                    }
                    #endif
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu(ngTranslateCString("View").data(), true)) {
                    commands::ngMenuItemFor<ViewCommand::SetDefaultLayout>(ctx);
                    ImGui::Separator();

                    // Spacing
                    ImGui::Spacing();
                    ImGui::Spacing();

                    ImGui::TextColored(ImVec4(0.7, 0.5, 0.5, 1), ngTranslateCString("Panels").data());
                    ImGui::Separator();

                    const auto menuPanels = panels::incPanels;
                    for (const auto panel : menuPanels) {
                        // Skip panels that'll always be visible
                        if (compat::ngRequireObject(panel)->alwaysVisible) continue;
                        bool enabled = compat::ngRequireObject(panel)->isActive();
                        // Use dynamic command instance
                        auto cmdInst = ensureTogglePanelCommand(panel);
                        auto lbl = compat::ngRequireObject(cmdInst)->label();
                        // Show shortcut hint if any
                        
                        auto sc = ngShortcutFor(cmdInst);
                        std::optional<compat::SourceCString> scZ;
                        if (!sc.empty()) scZ.emplace(menuCString(sc.view()));
                        const char* pShortcut = scZ ? scZ->data() : nullptr;
                        const auto labelZ = menuCString(lbl.view());
                        if (ImGui::MenuItem(labelZ.data(), pShortcut,
                                compat::ngRequireObject(panel)->visible, enabled)) {
                            ngRunCommand(cmdInst, ctx);
                        }
                        if (!enabled) {
                            ngTooltip(translated("Panel is not visible in current edit mode."));
                        }
                    }

                    // Spacing
                    ImGui::Spacing();
                    ImGui::Spacing();

                    ImGui::TextColored(ImVec4(0.7, 0.5, 0.5, 1), ngTranslateCString("Configuration").data());

                    // Opens the directory where configuration resides in the user's file browser.
                    if (ImGui::MenuItem(ngTranslateCString("Open Configuration Folder").data(), nullptr, false, true)) {
                        utils::link::ngOpenLink(incGetAppConfigPath());
                    }

                    // Spacing
                    ImGui::Spacing();
                    ImGui::Spacing();
                    
                    
                    ImGui::TextColored(ImVec4(0.7, 0.5, 0.5, 1), ngTranslateCString("Extras").data());

                    ImGui::Separator();
                    commands::ngMenuItemFor<ViewCommand::ShowSaveScreenshotDialog>(ctx);
                    ngTooltip(translated("Saves screenshot as PNG of the editor framebuffer."));
                    commands::ngMenuItemFor<ViewCommand::ShowStatusForNerds>(ctx, core::ngShowStatsForNerds, true);


                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu(ngTranslateCString("Tools").data(), editingAllowed)) {

                    ImGui::TextColored(ImVec4(0.7, 0.5, 0.5, 1), ngTranslateCString("Puppet Data").data());
                    ImGui::Separator();

                    // Opens the directory where configuration resides in the user's file browser.
                    commands::ngMenuItemFor<ToolCommand::ShowImportSessionDataDialog>(ctx);
                    ngTooltip(translated("Imports tracking data from an exported nijilive model which has been set up in Inochi Session."));
                    

                    ImGui::TextColored(ImVec4(0.7, 0.5, 0.5, 1), ngTranslateCString("Puppet Texturing").data());
                    ImGui::Separator();

                    // Premultiply textures, causing every pixel value in every texture to
                    // be multiplied by their Alpha (transparency) component
                    commands::ngMenuItemFor<ToolCommand::PremultTexture>(ctx);
                    ngTooltip(translated("Premultiplies textures by their alpha component.\n\nOnly use this if your textures look garbled after importing files from an older version of nijigenerate."));
                    
                    commands::ngMenuItemFor<ToolCommand::RebleedTexture>(ctx);
                    ngTooltip(translated("Causes color to bleed out in to fully transparent pixels, this solves outlines on straight alpha compositing.\n\nOnly use this if your game engine can't use premultiplied alpha."));

                    commands::ngMenuItemFor<ToolCommand::RegenerateMipmaps>(ctx);
                    ngTooltip(translated("Regenerates the puppet's mipmaps."));

                    commands::ngMenuItemFor<ToolCommand::GenerateFakeLayerName>(ctx);
                    ngTooltip(translated("Generates fake layer info based on node names"));

                    // Spacing
                    ImGui::Spacing();
                    ImGui::Spacing();

                    ImGui::TextColored(ImVec4(0.7, 0.5, 0.5, 1), ngTranslateCString("Puppet Recovery").data());
                    ImGui::Separator();

                    // FULL REPAIR
                    commands::ngMenuItemFor<ToolCommand::AttemptRepairPuppet>(ctx);
                    ngTooltip(translated("Attempts all the recovery and repair methods below on the currently loaded model"));

                    // REGEN NODE IDs
                    commands::ngMenuItemFor<ToolCommand::RegenerateNodeIDs>(ctx);
                    ngTooltip(translated("Regenerates all the unique IDs for the model"));

                    // Spacing
                    ImGui::Spacing();
                    ImGui::Spacing();
                    ImGui::Separator();
                    commands::ngMenuItemFor<ToolCommand::AttemptRepairPuppet>(ctx);
                    ngTooltip(translated("Attempts to verify and repair INP files"));

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu(ngTranslateCString("Help").data(), true)) {

                    if(ImGui::MenuItem(ngTranslateCString("Online Documentation").data(), "", false, true)) {
                        utils::link::ngOpenLink("https://github.com/nijigenerate/nijigenerate/wiki");
                    }
                    
                    if(ImGui::MenuItem(ngTranslateCString("nijilive Documentation").data(), "", false, true)) {
                        utils::link::ngOpenLink("https://github.com/nijigenerate/nijilive/wiki");
                    }
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    

                    if (ImGui::MenuItem(ngTranslateCString("Report a Bug").data())) {
                        utils::link::ngOpenLink(INC_BUG_REPORT_URI);
                    }
                    if (ImGui::MenuItem(ngTranslateCString("Request a Feature").data())) {
                        utils::link::ngOpenLink(INC_FEATURE_REQ_URI);
                    }
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();


                    if(ImGui::MenuItem(ngTranslateCString("About").data(), "", false, true)) {
                        windows::base::incPushWindow(windows::base::ngCreateWindow<windows::about::AboutWindow>());
                    }
                    ImGui::EndMenu();
                }
        }
        ImVec2 avail;
        avail = ImGui::GetContentRegionAvail();
        float tabBarWidth;
        tabBarWidth = clampWidth(avail.x - 128 * 2);

        // We need to pre-calculate the size of the right adjusted section
        // This code is very ugly because imgui doesn't really exactly understand this
        // stuff natively.
        ImVec2 secondSectionLength = ImVec2(0, 0);
        bool teacherDiffActive = viewport::common::mesheditor::brushstate::ngBrushHasTeacherPart();
        const auto statsPlaceholder = teacherDiffActive ?
            compat::RetainedString::fromStaticLiteral("1000ms | Diff 0.000") :
            compat::RetainedString::fromStaticLiteral("1000ms");
        if (core::ngShowStatsForNerds) { // Extra padding I guess
            secondSectionLength.x += ImGui::GetStyle().ItemSpacing.x;
            secondSectionLength.x += ngMeasureString(statsPlaceholder).x;
        }
        ImGui::Dummy(ImVec2(tabBarWidth - secondSectionLength.x, 0));
        if (core::ngShowStatsForNerds) {
            const auto fpsText = format("%.0fms",1000.f/io.Framerate);
            auto statsText = fpsText;
            if (teacherDiffActive) {
                compat::RetainedString diffValue;
                if (ngDifferenceAggregationResultValid) {
                    double sumTotals = 0;
                    double sumWeights = 0;
                    const auto tileCount = ngDifferenceAggregationResult.tileTotals.size();
                    for (std::size_t i = 0; i < tileCount; ++i) {
                        sumTotals += ngDifferenceAggregationResult.tileTotals[i];
                        sumWeights += ngDifferenceAggregationResult.tileCounts[i];
                    }
                    if (sumWeights > 0) {
                        diffValue = format("%.3f",sumTotals / sumWeights);
                    } else if (ngDifferenceAggregationResult.alpha > 0) {
                        const double total = ngDifferenceAggregationResult.red + ngDifferenceAggregationResult.green
                            + ngDifferenceAggregationResult.blue;
                        diffValue = format("%.3f",total / ngDifferenceAggregationResult.alpha);
                    } else {
                        diffValue = compat::RetainedString::fromStaticLiteral("--");
                    }
                } else {
                    diffValue = compat::RetainedString::fromStaticLiteral("--");
                }
                const auto suffix = format(" | Diff %s", diffValue);
                statsText = allocateMenuMemory([&] {
                    auto combined = std::string(statsText.view());
                    combined.append(suffix.view());
                    return compat::RetainedString(std::move(combined));
                });
            }
            const auto placeholderWidth = ngMeasureString(statsPlaceholder).x;
            const auto statsWidth = ngMeasureString(statsText).x;
            float textAreaDummyWidth = placeholderWidth - statsWidth;
            if (textAreaDummyWidth > 0) ngDummy(ImVec2(textAreaDummyWidth, 0));
            ngText(statsText);
        }
        ImGui::SetNextItemWidth (avail.x - tabBarWidth);
        ImGui::BeginDisabled(!editingAllowed);
        ImGui::BeginTabBar("###ModeTab");
            if(incEditMode() != EditMode::VertexEdit && incEditMode() != EditMode::DepthEdit) {
                auto mode = incEditMode(); // snapshot for this frame

                // Use a persistent tab selection to avoid per-frame toggling.
                // Only request SetSelected when mode changed externally (e.g., shortcut).
                static thread_local EditMode tabSelected = EditMode::ModelEdit;
                ImGuiTabItemFlags modelFlags = ImGuiTabItemFlags_None;
                ImGuiTabItemFlags animFlags  = ImGuiTabItemFlags_None;
                bool syncExternalMode = tabSelected != mode;
                if (syncExternalMode) {
                    if (mode == EditMode::ModelEdit) modelFlags = ImGuiTabItemFlags_SetSelected;
                    else if (mode == EditMode::AnimEdit) animFlags = ImGuiTabItemFlags_SetSelected;
                    tabSelected = mode;
                }

                // Render tabs
                const auto modelText = [] {
                    const auto title = translated("Edit Puppet");
                    return allocateMenuMemory([&] {
                        std::string result("\xef\x82\x97");
                        result.append(title.view());
                        return compat::RetainedString(std::move(result));
                    });
                }();
                const auto modelLabel = menuCString(modelText.view());
                bool modelTabVisible = ImGui::BeginTabItem(modelLabel.data(), nullptr, modelFlags);
                bool modelTabClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
                if (modelTabVisible) {
                    ImGui::EndTabItem();
                }
                if ((modelTabClicked || (modelTabVisible && !syncExternalMode)) && mode != EditMode::ModelEdit)
                    incSetEditMode(EditMode::ModelEdit);
                ngTooltip(translated("Edit Puppet"));

                const auto animText = [] {
                    const auto title = translated("Edit Animation");
                    return allocateMenuMemory([&] {
                        std::string result("\xee\x9c\x9c");
                        result.append(title.view());
                        return compat::RetainedString(std::move(result));
                    });
                }();
                const auto animLabel = menuCString(animText.view());
                bool animTabVisible = ImGui::BeginTabItem(animLabel.data(), nullptr, animFlags);
                bool animTabClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
                if (animTabVisible) {
                    ImGui::EndTabItem();
                }
                if ((animTabClicked || (animTabVisible && !syncExternalMode)) && mode != EditMode::AnimEdit)
                    incSetEditMode(EditMode::AnimEdit);
                ngTooltip(translated("Edit Animation"));
            }
        ImGui::EndTabBar();
        ImGui::EndDisabled();
        ImGui::EndMainMenuBar();

        // For quick-setup stuff
        if (!incSettingsGet("hasDoneQuickSetup", false)) ImGui::EndDisabled();

    // ImGui Debug Stuff
    if (dbgShowStyleEditor) ImGui::ShowStyleEditor(&ImGui::GetStyle());
    if (dbgShowDebugger) ImGui::ShowAboutWindow(&dbgShowDebugger);
    if (dbgShowStackTool) ImGui::ShowIDStackToolWindow();
    if (dbgShowMetrics) ImGui::ShowMetricsWindow();
}

}
