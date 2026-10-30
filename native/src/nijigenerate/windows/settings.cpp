#include "settings.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/range_error.hpp>
#if defined(_M_X64) || defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#include <xmmintrin.h>
#endif
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/window_theme.hpp>
#include <nijigenerate/core/dpi.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/i18n.hpp>
#include <nijigenerate/core/shortcut/persistence.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/io/autosave.hpp>
#include <nijigenerate/io/package.hpp>
#include <nijigenerate/io/save_settings.hpp>
#include <nijigenerate/io/depthmap_psd_names.hpp>
#include <nijigenerate/backend/runtime_gl_blend.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <algorithm>
#include <bit>
#include <cstring>
#if defined(NIJIGENERATE_HAVE_MCP)
#include <nijigenerate/api/mcp/server.hpp>
#endif
namespace nijigenerate::windows::settings {
using namespace core::settings;
using namespace core::dpi;
using namespace core::i18n;
using namespace core::actionstack;
using namespace core::shortcut;
using namespace viewport::base;
using namespace widgets;
using namespace io::autosave;
using namespace backend::runtime_gl;
namespace {
template<std::size_t N> compat::RetainedString tr(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
template<std::size_t N> SettingsString literal(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
int toInt(float value) {
#if defined(_M_X64) || defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
    // Execute the selected native truncating conversion, including its FP flags.
    return _mm_cvtt_ss2si(_mm_set_ss(value));
#else
    // Preserve the existing other-target mapping without claiming its FP profile.
    return !std::isfinite(value) || double(value) < -2147483648.0 || double(value) >= 2147483648.0
        ? std::numeric_limits<int>::min() : static_cast<int>(value);
#endif
}
// Branch ordinals are private lowering of the original string switch.
enum class PaneCase { LookAndFeel, Viewport, Accessibility, Rendering, FileHandling, Shortcuts, Integration, Unknown };
PaneCase paneCase(const SettingsPane& pane) {
    if (pane == SettingsPane::LookAndFeel) return PaneCase::LookAndFeel;
    if (pane == SettingsPane::Viewport) return PaneCase::Viewport;
    if (pane == SettingsPane::Accessibility) return PaneCase::Accessibility;
    if (pane == SettingsPane::Rendering) return PaneCase::Rendering;
    if (pane == SettingsPane::FileHandling) return PaneCase::FileHandling;
    if (pane == SettingsPane::Shortcuts) return PaneCase::Shortcuts;
    if (pane == SettingsPane::Integration) return PaneCase::Integration;
    return PaneCase::Unknown;
}
}
thread_local bool incIsSettingsOpen = false;
bool ngIsSettingsOpen() { return incIsSettingsOpen; }
void SettingsWindow::initialize() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    Window::initialize(tr("Settings"));
    targetUIScale = incGetUIScale();
    tmpUIScale = toInt(incGetUIScale() * 100);
    ngLoadShortcutsFromSettings();
}
void SettingsWindow::beginSection(const char* title) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    ngBeginCategory(title, IncCategoryFlags::NoCollapse); ngDummy(ImVec2(0, 4));
}
void SettingsWindow::endSection() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    ngDummy(ImVec2(0, 4));
    ngEndCategory();
}
void SettingsWindow::onBeginUpdate() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    flags |= ImGuiWindowFlags_NoSavedSettings;
    incIsSettingsOpen = true;
    const auto positionX = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const auto halfWidth = compat::ngRequireObject(ImGui::GetMainViewport())->Size.x / 2;
    const auto positionY = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const auto halfHeight = compat::ngRequireObject(ImGui::GetMainViewport())->Size.y / 2;
    ImVec2 position(positionX + halfWidth, positionY + halfHeight);
    ImVec2 size(640, 480);
    ImGui::SetNextWindowPos(position, ImGuiCond_Appearing, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(size, ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
    Window::onBeginUpdate();
}
void SettingsWindow::onClose() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    ngSaveShortcutsToSettings();
#if defined(NIJIGENERATE_HAVE_MCP)
    const bool enabled = incSettingsGet<bool>(literal("MCP.Enabled"), false);
    const auto host = incSettingsGet<SettingsString>(literal("MCP.Host"), literal("127.0.0.1"));
    const auto port = static_cast<std::uint16_t>(incSettingsGet<int>(literal("MCP.Port"), 8088));
    api::mcp::server::ngMcpApplySettings(enabled, host, port);
    api::mcp::server::ngMcpAuthEnabled(incSettingsGet<bool>(literal("MCP.authEnabled"), false));
#endif
    incSettingsSave();
    incIsSettingsOpen = false;
}
void SettingsWindow::onUpdate() {
    auto keepAlive = weak_from_this().lock();
    float available = ngAvailableSpace().x;
    if (ImGui::BeginChild("SettingsSidebar", ImVec2(available / 3.5, -28), true)) {
        ImGui::PushTextWrapPos(128);
        const std::pair<SettingsPane, const char*> panes[]{
            {SettingsPane::LookAndFeel,"Look and Feel"}, {SettingsPane::Viewport,"Viewport"},
            {SettingsPane::Accessibility,"Accessbility"}, {SettingsPane::Rendering,"Rendering"},
            {SettingsPane::FileHandling,"File Handling"}, {SettingsPane::Shortcuts,"Shortcuts"},
            {SettingsPane::Integration,"Integration"}};
        for (auto [pane, label] : panes) {
            const auto translatedLabel=core::translation::ngTranslateCString(label);
            if (ImGui::Selectable(translatedLabel.data(), settingsPane == pane)) settingsPane = pane;
        }
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild(); ImGui::SameLine(0, 4);
    if (ImGui::BeginChild("SettingsContent", ImVec2(0, -28), true)) {
        available = ngAvailableSpace().x;
        ImGui::PushItemWidth(available / 2);
        switch (paneCase(settingsPane)) {
        case PaneCase::LookAndFeel: {
            beginSection(core::translation::ngTranslateCString("Look and Feel").data());
            const auto themeLabel=core::translation::ngTranslateCString("Color Theme");
            const auto themePreview=core::translation::ngTranslateCString(core::ngGetDarkMode() ? "Dark" : "Light");
            if (ImGui::BeginCombo(themeLabel.data(), themePreview.data())) {
                const auto darkLabel=core::translation::ngTranslateCString("Dark");
                if (ImGui::Selectable(darkLabel.data(), core::ngGetDarkMode())) core::ngSetDarkMode(true);
                const auto lightLabel=core::translation::ngTranslateCString("Light");
                if (ImGui::Selectable(lightLabel.data(), !core::ngGetDarkMode())) core::ngSetDarkMode(false);
                ImGui::EndCombo();
            }
            const auto languageLabel=core::translation::ngTranslateCString("Language");
            const auto currentLanguage = ngLocaleCurrentName();
            const auto languagePreview = detail::settingsCString(currentLanguage.view());
            if (ImGui::BeginCombo(languageLabel.data(), languagePreview.data())) {
                if (ImGui::Selectable("English")) { ngLocaleSet(nullptr); changesRequiresRestart = true; }
                for (auto entry : ngLocaleGetEntries()) {
                    const auto label = detail::allocateSettingsMemory([&] {
                        std::string result;
                        if (!entry.humanName.empty()) result.append(entry.humanName.data(), entry.humanName.size());
                        result.append("##");
                        if (!entry.code.empty()) result.append(entry.code.data(), entry.code.size());
                        return result;
                    });
                    if (ImGui::Selectable(detail::settingsCString(label).data())) {
                        ngLocaleSet(entry.code); changesRequiresRestart = true;
                    }
                }
                ImGui::EndCombo();
            }
#if defined(NG_USE_UI_SCALING) && !defined(__APPLE__)
            if (ImGui::InputInt(core::translation::ngTranslateCString("UI Scale").data(), &tmpUIScale, 25, 50, ImGuiInputTextFlags_EnterReturnsTrue)) {
                tmpUIScale = std::clamp(tmpUIScale, 100, 200); incSetUIScale(float(tmpUIScale) / 100.0);
            }
#endif
            endSection(); beginSection(core::translation::ngTranslateCString("Undo History").data());
            int maxHistory = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(incActionGetUndoHistoryLength()));
            if (ImGui::DragInt(core::translation::ngTranslateCString("Max Undo History").data(), &maxHistory, 1, 1, 1000, "%d"))
                incActionSetUndoHistoryLength(maxHistory);
            endSection();
#if defined(__linux__)
            beginSection(core::translation::ngTranslateCString("Linux Tweaks").data());
            bool disable = incSettingsGet<bool>(literal("DisableCompositor"));
            if (ngCheckbox(core::translation::ngTranslateCString("Disable Compositor").data(), &disable)) incSettingsSet(literal("DisableCompositor"), disable);
            endSection();
#endif
            break;
        }
        case PaneCase::Shortcuts: drawShortcutsPane(); break;
        case PaneCase::Accessibility: {
            beginSection(core::translation::ngTranslateCString("Accessibility").data());
            bool useFont = incSettingsGet<bool>(literal("useOpenDyslexic"));
            if (ngCheckbox(core::translation::ngTranslateCString("Use OpenDyslexic Font").data(), &useFont)) {
                incSettingsSet(literal("useOpenDyslexic"), useFont); changesRequiresRestart = true;
            }
            ngTooltip(tr("Use the OpenDyslexic font for Latin text characters.")); endSection(); break;
        }
        case PaneCase::Rendering: {
            beginSection(core::translation::ngTranslateCString("Rendering").data());
            bool fallback = incSettingsGet<bool>(literal("TripleBufferFallback"), oglIsTripleBufferFallbackEnabled());
            if (ngCheckbox(core::translation::ngTranslateCString("Enable triple buffer fallback").data(), &fallback)) {
                incSettingsSet(literal("TripleBufferFallback"), fallback);
                oglSetTripleBufferFallback(fallback); oglApplyBlendingCapabilities();
            }
            ngTooltip(tr("Fallback to double buffering when triple buffering is unstable.")); endSection(); break;
        }
        case PaneCase::FileHandling: {
            beginSection(core::translation::ngTranslateCString("Autosaves").data());
            bool enabled = incGetAutosaveEnabled();
            if (ngCheckbox(core::translation::ngTranslateCString("Enable Autosaves").data(), &enabled)) incSetAutosaveEnabled(enabled);
            int frequency = incGetAutosaveInterval();
            if (ImGui::InputInt(core::translation::ngTranslateCString("Save Interval (Minutes)").data(), &frequency, 5, 15, ImGuiInputTextFlags_EnterReturnsTrue))
                incSetAutosaveInterval(frequency);
            int limit = incGetAutosaveFileLimit();
            if (ImGui::InputInt(core::translation::ngTranslateCString("Maximum Autosaves").data(), &limit, 1, 5, ImGuiInputTextFlags_EnterReturnsTrue))
                incSetAutosaveFileLimit(limit);
            endSection(); beginSection(core::translation::ngTranslateCString("Import behaviour").data());
            SettingsStringMap showing{{literal("Ask"), tr("Always ask")},
                {literal("Preserve"), tr("Preserve")}, {literal("NotPreserve"), tr("Don't preserve")}};
            auto selectedPtr = showing.find(io::incGetKeepLayerFolder().view());
            auto selected = selectedPtr ? *selectedPtr : literal("Ask");
            auto keep = incSettingsGet<SettingsString>(literal("KeepLayerFolder"));
            const auto preserveLabel=core::translation::ngTranslateCString("Preserve structure");
            const auto preservePreview = detail::settingsCString(selected.view());
            if (ImGui::BeginCombo(preserveLabel.data(), preservePreview.data())) {
                for (const auto& [key, label] : showing)
                    if (ImGui::Selectable(detail::settingsCString(label.view()).data(), keep == key)) io::incSetKeepLayerFolder(key);
                ImGui::EndCombo();
            }
            endSection(); beginSection(core::translation::ngTranslateCString("Layer group replacement").data());
            SettingsStringMap options{{literal("Node"),tr("Node")},
                {literal("MeshGroup"),tr("MeshGroup")},{literal("DynamicComposite"),tr("DynamicComposite")}};
            auto current = incSettingsGet<SettingsString>(literal("LayerGroupReplacement"), literal("Node"));
            const auto displayFallback = tr("Node");
            auto displayPtr = options.find(current.view());
            auto display = displayPtr ? *displayPtr : displayFallback;
            const auto replacementLabel=core::translation::ngTranslateCString("Replace LayerGroup with");
            const auto replacementPreview = detail::settingsCString(display.view());
            if (ImGui::BeginCombo(replacementLabel.data(), replacementPreview.data())) {
                for (const auto& [key, label] : options) if (ImGui::Selectable(detail::settingsCString(label.view()).data(), current == key)) {
                    incSettingsSet(literal("LayerGroupReplacement"), key); current = key;
                }
                ImGui::EndCombo();
            }
            ngTooltip(tr("Node type to create for imported layer groups when preserving structure.")); endSection();
            beginSection(core::translation::ngTranslateCString("On close project").data());
            auto saveOptions = io::save::incGetSaveProjectOption();
            auto saveSelectedPtr = saveOptions.find(io::save::incGetSaveProjectOnClose().view());
            auto saveSelected = saveSelectedPtr ? *saveSelectedPtr : literal("Ask");
            const auto saveLabel=core::translation::ngTranslateCString("Save project");
            const auto savePreview = detail::settingsCString(saveSelected.view());
            if (ImGui::BeginCombo(saveLabel.data(), savePreview.data())) {
                for (const auto& [key, label] : saveOptions)
                    if (ImGui::Selectable(detail::settingsCString(label.view()).data(), saveSelected == key)) io::save::incSetSaveProjectOnClose(key);
                ImGui::EndCombo();
            }
            ngTooltip(tr("Should changes be saved automatically when closing a project?")); endSection(); break;
        }
        case PaneCase::Viewport: {
            beginSection(core::translation::ngTranslateCString("Viewport").data());
            const auto mode = ngGetViewportZoomMode();
            if (mode != "ScreenCenter" && mode != "MousePosition")
                throw compat::ngStaticError<compat::RangeError>(
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/settings.d"), 311);
            const auto zoomLabel=core::translation::ngTranslateCString("Zoom Mode");
            const auto zoomPreview=core::translation::ngTranslateCString(mode == "ScreenCenter" ? "To Screen Center" : "To Mouse Position");
            if (ImGui::BeginCombo(zoomLabel.data(), zoomPreview.data())) {
                const auto screenLabel=core::translation::ngTranslateCString("To Screen Center");
                if (ImGui::Selectable(screenLabel.data(), incSettingsGet<SettingsString>(literal("ViewportZoomMode")).view() == "ScreenCenter"))
                    ngSetViewportZoomMode("ScreenCenter");
                const auto mouseLabel=core::translation::ngTranslateCString("To Mouse Position");
                if (ImGui::Selectable(mouseLabel.data(), incSettingsGet<SettingsString>(literal("ViewportZoomMode")).view() == "MousePosition"))
                    ngSetViewportZoomMode("MousePosition");
                ImGui::EndCombo();
            }
            float speed = ngGetViewportZoomSpeed();
            if (ImGui::DragFloat(core::translation::ngTranslateCString("Zoom Speed").data(), &speed, .1, 1, 50, "%f")) ngSetViewportZoomSpeed(speed);
            endSection(); break;
        }
        case PaneCase::Integration: {
#if defined(NIJIGENERATE_HAVE_MCP)
            beginSection(core::translation::ngTranslateCString("MCP Server").data());
            bool enabled = incSettingsGet<bool>(literal("MCP.Enabled"), false);
            if (ngCheckbox(core::translation::ngTranslateCString("Enable MCP HTTP Server").data(), &enabled)) incSettingsSet(literal("MCP.Enabled"), enabled);
            if (enabled) {
                auto host = incSettingsGet<SettingsString>(literal("MCP.Host"), literal("127.0.0.1"));
                int port = incSettingsGet<int>(literal("MCP.Port"), 8088);
                std::array<char,128> hostBuffer; hostBuffer.fill(static_cast<char>(0xff));
                if (host.size() < hostBuffer.size()) {
                    if (!host.empty()) std::memcpy(hostBuffer.data(),host.data(),host.size());
                    hostBuffer[host.size()]=0;
                }
                else hostBuffer[0] = 0;
                if (ImGui::InputText(core::translation::ngTranslateCString("Host").data(),hostBuffer.data(),hostBuffer.size(),ImGuiInputTextFlags_EnterReturnsTrue))
                    incSettingsSet(literal("MCP.Host"), SettingsString(detail::retainSettingsBuffer(hostBuffer.data())));
                if (ImGui::InputInt(core::translation::ngTranslateCString("Port").data(),&port,1,10,ImGuiInputTextFlags_EnterReturnsTrue))
                    incSettingsSet(literal("MCP.Port"),std::clamp(port,1,65535));
                const auto hostPreview = detail::retainSettingsBuffer(hostBuffer.data());
                auto lower = io::depthmap_psd::ngPsdDepthLowerString(hostPreview);
                if (lower.view() != "127.0.0.1" && lower.view() != "localhost") {
                    ImGui::PushTextWrapPos(384);
                    ngTextColored(ImVec4(.9,.2,.2,1),tr("Warning: MCP server host is not local. Others may connect to your nijigenerate. Use only on a trusted network and ensure proper firewall/network protections."));
                    ImGui::PopTextWrapPos();
                }
                ngTooltip(tr("Changes will apply when you click Done."));
                bool auth = incSettingsGet<bool>(literal("MCP.authEnabled"),false);
                if (ngCheckbox(core::translation::ngTranslateCString("Enabled authorization.").data(),&auth)) incSettingsSet(literal("MCP.authEnabled"),auth);
            } else ngTooltip(tr("Server disabled. Click Done to apply."));
            endSection();
#endif
            beginSection(core::translation::ngTranslateCString("Coding Agent (ACP)").data());
            auto command = incSettingsGet<SettingsString>(literal("ACP.Command"),literal("./out/nijigenerate-agent"));
            auto directory = incSettingsGet<SettingsString>(literal("ACP.Workdir"),literal(""));
            std::array<char,256> commandBuffer{}, directoryBuffer{};
            if (command.size() < commandBuffer.size()) {
                if (!command.empty()) std::memcpy(commandBuffer.data(),command.data(),command.size());
                commandBuffer[command.size()]=0;
            }
            if (directory.size() < directoryBuffer.size()) {
                if (!directory.empty()) std::memcpy(directoryBuffer.data(),directory.data(),directory.size());
                directoryBuffer[directory.size()]=0;
            }
            if (ImGui::InputText(core::translation::ngTranslateCString("Command").data(),commandBuffer.data(),commandBuffer.size(),ImGuiInputTextFlags_EnterReturnsTrue))
                incSettingsSet(literal("ACP.Command"), SettingsString(detail::retainSettingsBuffer(commandBuffer.data())));
            if (ImGui::InputText(core::translation::ngTranslateCString("Working Dir (optional)").data(),directoryBuffer.data(),directoryBuffer.size(),ImGuiInputTextFlags_EnterReturnsTrue))
                incSettingsSet(literal("ACP.Workdir"), SettingsString(detail::retainSettingsBuffer(directoryBuffer.data())));
            ngTooltip(tr("Command line is saved immediately.")); endSection(); break;
        }
        default: ngLabelOver(tr("No settings for this category."),ImVec2(0,0),true); break;
        }
        ImGui::PopItemWidth();
    }
    ImGui::EndChild();
    if (ImGui::BeginChild("SettingsButtons",ImVec2(0,0),false,ImGuiWindowFlags_NoScrollbar)) {
        if (changesRequiresRestart) {
            ImGui::PushTextWrapPos(384);
            ngTextColored(ImVec4(.8,.2,.2,1),tr("nijigenerate needs to be restarted for some changes to take effect."));
            ImGui::PopTextWrapPos(); ImGui::SameLine(0,0);
        }
        ngDummy(ImVec2(-64,0)); ImGui::SameLine(0,0);
        if (ngButtonColored(core::translation::ngTranslateCString("Done").data(),ImVec2(64,24))) close();
    }
    ImGui::EndChild();
}
}
