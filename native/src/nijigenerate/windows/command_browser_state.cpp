#include "command_browser.hpp"
#include "command_browser_helpers.hpp"
#include <nijigenerate/core/shortcut/base.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <cfloat>
namespace nijigenerate::windows::command_browser {
using namespace detail;
void CommandBrowserWindow::initialize() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    base::Window::initialize(tr("Command Browser").retainedString());
    initialized = false; argValues = {};
    ctxNodes = ctxParams = ctxArmed = ctxKeyPoint = emptyText();
    lastHasResult = false; lastSucceeded = false;
    lastMessage = lastResultType = lastResultDetail = emptyText();
    lastCreated = lastDeleted = lastLoaded = {};
    argNodeSelections = {}; argParamSelections = {}; argBindingSelections = {};
    ctxNodesSel = {}; ctxParamsSel = {}; ctxArmedSel = {}; ctxBindingsSel = {};
    contextDirty = false; captureDefaultContext();
}
void CommandBrowserWindow::resetSelection(CommandInfo info) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    selectedCmd = info.cmd; argValues = {};
    lastHasResult = false; lastSucceeded = false;
    lastMessage = lastResultType = lastResultDetail = emptyText();
    lastCreated = lastDeleted = lastLoaded = {};
    argNodeSelections = {}; argParamSelections = {}; argBindingSelections = {};
    ctxNodesSel = {}; ctxParamsSel = {}; ctxArmedSel = {}; ctxBindingsSel = {};
    if (info.captureDefaults) info.captureDefaults(selectedCmd,argValues);
    captureDefaultContext();
}
void CommandBrowserWindow::captureDefaultContext() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto ctx = core::shortcut::ngBuildExecutionContext();
    ctxNodesSel = compat::ngRequireObject(ctx)->hasNodes()
        ? compat::ngRequireObject(ctx)->nodes() : Nodes();
    ctxParamsSel = compat::ngRequireObject(ctx)->hasParameters()
        ? compat::ngRequireObject(ctx)->parameters() : Parameters();
    ctxArmedSel = compat::ngRequireObject(ctx)->hasArmedParameters()
        ? compat::ngRequireObject(ctx)->armedParameters() : Parameters();
    ctxBindingsSel = compat::ngRequireObject(ctx)->hasActiveBindings()
        ? compat::ngRequireObject(ctx)->activeBindings()
        : compat::ngRequireObject(ctx)->hasBindings()
            ? compat::ngRequireObject(ctx)->bindings() : Bindings();
    if (compat::ngRequireObject(ctx)->hasKeyPoint()) {
        const auto x = compat::ngRequireObject(ctx)->keyPoint().x;
        const auto y = compat::ngRequireObject(ctx)->keyPoint().y;
        ctxKeyPoint = format("%s,%s",x,y);
    } else ctxKeyPoint = emptyText();
    ctxNodes = ctxParams = ctxArmed = emptyText(); contextDirty = false;
}
void CommandBrowserWindow::onBeginUpdate() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    flags |= ImGuiWindowFlags_NoSavedSettings;
    ImGui::SetNextWindowSize(ImVec2(900,640),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(700,480),ImVec2(FLT_MAX,FLT_MAX));
    incIsCommandBrowserOpen = true;
    rebuildCommandInfos();
    if (!initialized) {
        filterText = emptyText(); selectedIndex = 0; selectedCmd = {}; initialized = true;
        argValues = {}; ctxNodes = ctxParams = ctxArmed = ctxKeyPoint = emptyText();
        lastHasResult = false; lastSucceeded = false;
        lastMessage = lastResultType = lastResultDetail = emptyText();
        lastCreated = lastDeleted = lastLoaded = {};
        argNodeSelections = {}; argParamSelections = {}; argBindingSelections = {};
        ctxNodesSel = {}; ctxParamsSel = {}; ctxArmedSel = {}; ctxBindingsSel = {};
        captureDefaultContext();
    }
    base::Window::onBeginUpdate();
}
void CommandBrowserWindow::onEndUpdate() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    incIsCommandBrowserOpen = false; base::Window::onEndUpdate();
}
void detail::textWrappedInColumn(BrowserText value) {
    const auto converted = sourceCString(value);
    textWrappedInColumn(converted.data());
}
}
