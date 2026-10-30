#include "package.hpp"
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <common/object_key_policy.hpp>
#include <nijigenerate/widgets/window.hpp>
#if defined(NG_MCP_TASK_PROVIDER_HEADER)
#include NG_MCP_TASK_PROVIDER_HEADER
#else
#include <nijigenerate/api/mcp/task.hpp>
#endif
#if defined(NG_DEBUG_IMGUI)
#include <nijigenerate/core/window.hpp>
#endif
#include <array>
#include <string>
#include <utility>
#include <new>
#include <stdexcept>
#include <source_location>

namespace nijigenerate::panels {
namespace {
template<class Operation> auto allocateString(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
core::settings::SettingsString visibilityKey(const PanelString& name) {
    return allocateString([&] {
        return core::settings::SettingsString(std::string(name.view()) + ".visible");
    });
}
} // namespace
void Panel::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Panel),
            PanelString::fromStaticLiteral("nijigenerate.panels.Panel"));
        nicxlive::core::d_runtime::ngRegisterObjectTypeNames<Panel>(
            PanelString::fromStaticLiteral("nijigenerate.panels.Panel"),
            PanelString::fromStaticLiteral("(nijigenerate.panels.Panel)"));
        return true;
    }();
    (void)registered;
}
PanelString Panel::toString() {
    [[maybe_unused]] const auto receiver = sourceOwner_.lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Panel::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool Panel::opEquals(std::shared_ptr<Panel> other) {
    return nicxlive::core::common::ngClassObjectIdentity(other) == dynamic_cast<const void*>(this);
}
int Panel::opCmp(std::shared_ptr<Panel>) {
    [[maybe_unused]] const auto receiver = sourceOwner_.lock();
    prepareSourceClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    auto message = allocateString([&] {
        return PanelString(std::string("need opCmp for class ") + std::string(className.view()));
    });
    throw compat::ThrowableException(std::move(message),
        PanelString::fromStaticLiteral("object.d"), 195);
}
Panel::Panel(PanelString name, PanelString display, bool initialVisibility)
    : visible(initialVisibility), name_(std::move(name)), displayName_(std::move(display)),
      defaultVisibility(initialVisibility) {}

void Panel::onBeginUpdate() {
    [[maybe_unused]] const auto receiver = sourceOwner_.lock();
    flags |= ImGuiWindowFlags_NoCollapse;
    drewContents = widgets::ngBegin(windowID.data(), &visible, flags);
    if (wasVisible != visible)
        core::settings::incSettingsSet(visibilityKey(name_), visible);
    if (!drewContents) return;
#if defined(NG_DEBUG_IMGUI)
    core::ngDebugImGuiState(compat::RetainedStringSlice(
        compat::RetainedString::fromStaticLiteral("Panel::onBeginUpdate")), 1);
#endif
    panelSpace = ImGui::GetContentRegionAvail();
}
void Panel::onEndUpdate() {
    [[maybe_unused]] const auto receiver = sourceOwner_.lock();
#if defined(NG_DEBUG_IMGUI)
    if (drewContents) core::ngDebugImGuiState(compat::RetainedStringSlice(
        compat::RetainedString::fromStaticLiteral("Panel::onEndUpdate")), -1);
#endif
    widgets::ngEnd();
    wasVisible = visible;
}
void Panel::onInit() {}

void Panel::init_() {
    [[maybe_unused]] const auto receiver = sourceOwner_.lock();
    onInit();
    displayName_ = core::translation::ngTranslateRetained(displayName_);
    if (core::settings::incSettingsCanGet(visibilityKey(name_))) {
        visible = core::settings::incSettingsGet<bool>(visibilityKey(name_));
        wasVisible = visible;
    }
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(displayName_), core::translation::ngFormatArgument(name_)};
    auto formatted = core::translation::ngFormatTyped("%s###%s", arguments);
    const auto formattedWindowID = allocateString([&] { return PanelString(std::move(formatted)); });
    compat::SourceCString nextWindowID(formattedWindowID.view());
    compat::detail::ngSourceCStringPostconditions(formattedWindowID.view(), nextWindowID.data());
    windowID = nextWindowID.retainedString();
    compat::SourceCString nextDisplayName(displayName_.view());
    compat::detail::ngSourceCStringPostconditions(displayName_.view(), nextDisplayName.data());
    displayNamePtr = nextDisplayName.retainedString();
}
PanelString Panel::name() const { return name_; }
PanelString Panel::displayName() const { return displayName_; }
PanelString Panel::displayNameC() const { return displayNamePtr; }
bool Panel::isActive() {
    return (static_cast<int>(project::incEditMode()) & static_cast<int>(activeModes)) > 0;
}
void Panel::update() {
    [[maybe_unused]] const auto receiver = sourceOwner_.lock();
    onBeginUpdate();
    if (drewContents) {
        ImGui::BeginDisabled(api::mcp::task::ngMcpExternalCommandsBlocked() && !supportsReadOnlyInteraction);
        onUpdate();
        ImGui::EndDisabled();
    }
    onEndUpdate();
}
bool Panel::getDefaultVisibility() const { return defaultVisibility; }

thread_local PanelSlice incPanels;
void incAddPanel(PanelPtr panel) {
    if (panel) panel->sourceOwner_ = panel;
    incPanels.append(std::move(panel));
}
void incUpdatePanels() {
    auto snapshot = incPanels;
    for (auto panel : snapshot) {
        if (!compat::ngRequireObject(panel)->visible && !panel->alwaysVisible) continue;
        if (!panel->isActive()) continue;
        panel->update();
    }
}
void incInitPanels() {
    auto snapshot = incPanels;
    for (auto panel : snapshot) compat::ngRequireObject(panel)->init_();
}
PanelPtr incFindPanelByName(PanelString name) {
    auto snapshot = incPanels;
    for (auto panel : snapshot) if (compat::ngRequireObject(panel)->name() == name) return panel;
    return {};
}
} // namespace nijigenerate::panels
