#include "welcome.hpp"
#include <texture.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/shadow.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/dpi.hpp>
#include <nijigenerate/core/i18n.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/utils/link.hpp>
#include <nijigenerate/ver.hpp>
#include <nijigenerate/config.hpp>
#include <nijigenerate/io/package.hpp>
#include <nijigenerate/io/save.hpp>
#include <nijigenerate/io/psd.hpp>
#include <nijigenerate/io/kra.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/resources/embedded.hpp>
#include <algorithm>
#include <cmath>
#include <span>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::windows::welcome {
using namespace widgets;
using core::translation::ngTranslateRetained;
using core::translation::ngTranslateCString;
namespace {
template<class Operation> decltype(auto) allocateWelcomeMemory(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
int toInt(float value) {
    if (!std::isfinite(value) || value < -2147483648.f || value >= 2147483648.f)
        return std::numeric_limits<int>::min();
    return static_cast<int>(value);
}
nicxlive::core::ShallowTexture load(std::string_view name) {
    const auto bytes = resources::ngEmbeddedResource(name).value();
    return nicxlive::core::ShallowTexture(std::span<const std::uint8_t>(bytes.data(), bytes.size()));
}
}
void WelcomeWindow::initialize() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    Window::initialize(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("nijigenerate Start")));
    auto image = load("ui/banner.png");
    banner = nicxlive::core::Texture::createSource(image);
    {
        const auto target = compat::ngRequireObject(banner);
        target->setAnisotropy(1.5f);
    }
    auto logo = load("ui/banner-logo.png");
    nicxlive::core::inTexPremultiply(logo.data);
    bannerLogo = nicxlive::core::Texture::createSource(logo);
    if (!core::settings::incSettingsGet<bool>("hasDoneQuickSetup", false)) step = 0;
    tmpUIScale = toInt(core::dpi::incGetUIScale() * 100);
    uiSize = {512, 384};
    shadowDrawList = ngCreateWindowDrawList();
}
void WelcomeWindow::onBeginUpdate() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration;
    const auto posX=compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const auto sizeX=compat::ngRequireObject(ImGui::GetMainViewport())->Size.x;
    const auto posY=compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const auto sizeY=compat::ngRequireObject(ImGui::GetMainViewport())->Size.y;
    ImGui::SetNextWindowPos({posX+sizeX/2,posY+sizeY/2}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize(uiSize, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(uiSize, uiSize);
    origWindowPadding = ImGui::GetStyle().WindowPadding;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
    Window::onBeginUpdate();
}
void WelcomeWindow::onEndUpdate() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    ImGui::PopStyleVar(2);
    Window::onEndUpdate();
}
void WelcomeWindow::onClose() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (step > 0) core::settings::incSettingsSet<bool>("hasDoneQuickSetup", true);
    ngDestroyWindowDrawList(shadowDrawList);
}
void WelcomeWindow::onUpdate() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    ngRenderWindowShadow(shadowDrawList, compat::ngRequireObject(ImGui::GetCurrentWindow())->OuterRectClipped);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, origWindowPadding);
    auto* viewport = compat::ngRequireObject(ImGui::GetWindowViewport());
    viewport->Flags |= ImGuiViewportFlags_TopMost | ImGuiViewportFlags_NoDecoration | ImGuiViewportFlags_NoTaskBarIcon;
    [[maybe_unused]] const auto origin = ImGui::GetCursorStartPos();
    if (ImGui::BeginChild("##BANNER", {0, 200})) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
        const auto position = ImGui::GetCursorScreenPos();
        auto* drawList=compat::ngRequireObject(ImGui::GetWindowDrawList());
        const auto bannerId=compat::ngRequireObject(banner)->getTextureId();
        drawList->AddImageRounded(static_cast<ImTextureID>(bannerId),
            {position.x + 1, position.y + 1}, {position.x + 511, position.y + 199},
            {0, 0}, {1, 1}, IM_COL32_WHITE, 10, ImDrawFlags_RoundCornersTop);
        ImGui::SetCursorPos({0, 0});
        const auto logoId=compat::ngRequireObject(bannerLogo)->getTextureId();
        const auto logoWidth=compat::ngRequireObject(bannerLogo)->width();
        const auto logoHeight=compat::ngRequireObject(bannerLogo)->height();
        ImGui::Image(static_cast<ImTextureID>(logoId),
            {static_cast<float>(logoWidth/5),static_cast<float>(logoHeight/5)});
        auto size = ngMeasureString(ver::INC_VERSION);
        ImGui::SetCursorPos({512 - (size.x + 12), 12});
        ngTextShadowed(ver::INC_VERSION);
        size = ngMeasureString(config::INC_BANNER_ARTIST_NAME);
        ImGui::SetCursorPos({512 - (size.x + 12), 200 - (size.y + 8)});
        ngTextBordered(config::INC_BANNER_ARTIST_NAME);
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked()) utils::link::ngOpenLink(config::INC_BANNER_ARTIST_PAGE);
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::Indent();
    if (ImGui::BeginChild("##CONFIG_AREA", {-4, 0}, false, ImGuiWindowFlags_NoScrollbar)) {
        const auto available = ngAvailableSpace();
        ImGui::PushTextWrapPos(available.x);
        switch (step) {
        case 0: {
            ngDummy({0, 4});
            ngTextShadowed(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Quick Setup")));
            ImGui::NewLine();
            ngDummy({available.x / 6, 64}); ImGui::SameLine(0, 0); ImGui::BeginGroup();
            ImGui::PushItemWidth(available.x / 3);
            const auto comboFlags = ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_HeightLargest;
            const auto languageLabel=ngTranslateCString("Language");
            const auto current = core::i18n::ngLocaleCurrentName();
            const compat::SourceCString currentName{compat::RetainedStringSlice(current)};
            compat::detail::ngSourceCStringPostconditions(current.view(), currentName.data());
            if (ImGui::BeginCombo(languageLabel.data(), currentName.data(), comboFlags)) {
                if (ImGui::Selectable("English")) {
                    core::i18n::ngLocaleSet({}); changesRequiresRestart = true;
                }
                for (auto entry : core::i18n::ngLocaleGetEntries()) {
                    const auto label = allocateWelcomeMemory([&] {
                        return compat::RetainedString(
                            std::string(entry.humanName.view()) + "##" + std::string(entry.code.view()));
                    });
                    const compat::SourceCString labelCString{compat::RetainedStringSlice(label)};
                    compat::detail::ngSourceCStringPostconditions(label.view(), labelCString.data());
                    if (ImGui::Selectable(labelCString.data())) {
                        core::i18n::ngLocaleSet(entry.code); changesRequiresRestart = true;
                    }
                }
                ImGui::EndCombo();
            }
            const auto themeLabel=ngTranslateCString("Color Theme");
            const auto themePreview=ngTranslateCString(core::ngGetDarkMode() ? "Dark" : "Light");
            if (ImGui::BeginCombo(themeLabel.data(), themePreview.data(), comboFlags)) {
                if (ImGui::Selectable(ngTranslateCString("Dark").data(), core::ngGetDarkMode())) core::ngSetDarkMode(true);
                if (ImGui::Selectable(ngTranslateCString("Light").data(), !core::ngGetDarkMode())) core::ngSetDarkMode(false);
                ImGui::EndCombo();
            }
#if defined(NG_USE_UI_SCALING) && !defined(__APPLE__)
            if (ImGui::InputInt(ngTranslateCString("UI Scale").data(), &tmpUIScale, 25, 50,
                               ImGuiInputTextFlags_EnterReturnsTrue)) {
                tmpUIScale = std::clamp(tmpUIScale, 100, 200);
                core::dpi::incSetUIScale(static_cast<float>(static_cast<double>(tmpUIScale) / 100.0));
            }
#endif
            if (changesRequiresRestart) {
                ImGui::NewLine(); ImGui::PushTextWrapPos(static_cast<float>(available.x / 1.15));
                ngTextColored({.8f, .2f, .2f, 1}, ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                    "nijigenerate needs to be restarted for some changes to take effect.")));
                ImGui::PopTextWrapPos();
            }
            ImGui::PopItemWidth(); ImGui::EndGroup();
            ngDummy({0, -32}); ngDummy({-64, 24}); ImGui::SameLine(0, 0);
            if (ngButtonColored(ngTranslateCString("Next").data(), {64, 24})) {
                core::settings::incSettingsSet<bool>("hasDoneQuickSetup", true); ++step;
            }
            break;
        }
        case 1: {
            ngDummy({0, 4});
            if (ImGui::BeginChild("##LHS", {(available.x - 8) / 2, 0}, false, ImGuiWindowFlags_NoScrollbar)) {
                ngTextShadowed(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Create Project")));
                ngDummy({0, 2}); ImGui::Indent();
                if (ngTextLinkWithIcon(compat::RetainedString::fromStaticLiteral("\356\231\264"),
                        ngTranslateRetained(compat::RetainedString::fromStaticLiteral("New...")))) {
                    io::save::incNewProjectAsk(); close();
                }
                if (ngTextLinkWithIcon(compat::RetainedString::fromStaticLiteral("\356\213\207"),
                        ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Open..."))))
                    if (io::save::incFileOpen()) close();
                if (ngTextLinkWithIcon(compat::RetainedString::fromStaticLiteral("\357\207\207"),
                        ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Import..."))))
                    ImGui::OpenPopup("IMPORT_OPTIONS");
                if (ImGui::BeginPopup("IMPORT_OPTIONS")) {
                    if (ImGui::MenuItem(ngTranslateCString("Import PSD...").data())) if (io::psd::incImportShowPSDDialog()) close();
                    if (ImGui::MenuItem(ngTranslateCString("Import KRA...").data())) if (io::kra::incImportShowKRADialog()) close();
                    ImGui::EndPopup();
                }
                ImGui::Unindent(); ngDummy({0, 6});
                ngTextShadowed(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Recent Projects")));
                ngDummy({0, 2}); ImGui::Indent();
                if (!project::incGetPrevProjects().empty()) {
                    std::size_t index = 0;
                    for (auto recent : project::incGetPrevProjects()) {
                        if (index++ >= 4) break;
                        const auto basename = platform::ngAutosaveBaseName(recent.view());
                        compat::RetainedStringSlice name;
                        if (basename.data()) {
                            const auto offset = static_cast<std::size_t>(basename.data() - recent.data());
                            name = recent.slice(offset, offset + basename.size());
                        }
                        if (ngTextLinkWithIcon(compat::RetainedString::fromStaticLiteral("\356\213\207"), name))
                            if (project::incOpenProject(recent)) close();
                    }
                } else ngTextShadowed(ngTranslateRetained(
                    compat::RetainedString::fromStaticLiteral("No recent projects...")));
                ImGui::Unindent();
            }
            ImGui::EndChild(); ImGui::SameLine(0, 4);
            if (ImGui::BeginChild("##RHS", {(available.x - 8) / 2, 0}, false, ImGuiWindowFlags_NoScrollbar)) {
                ngTextShadowed(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("On the Web")));
                ngDummy({0, 2}); ImGui::Indent();
                if constexpr (!config::INC_INFO_WEBSITE_URI.empty())
                    if (ngTextLinkWithIcon(compat::RetainedString::fromStaticLiteral("\356\240\213"),
                            ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Website"))))
                        utils::link::ngOpenLink(config::INC_INFO_WEBSITE_URI);
                if constexpr (!config::INC_INFO_DOCS_URI.empty())
                    if (ngTextLinkWithIcon(compat::RetainedString::fromStaticLiteral("\356\240\213"),
                            ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Documentation"))))
                        utils::link::ngOpenLink(config::INC_INFO_DOCS_URI);
                ImGui::Unindent();
            }
            ImGui::EndChild(); break;
        }
        default: close(); break;
        }
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild(); ImGui::Unindent(); ImGui::PopStyleVar();
}
}
