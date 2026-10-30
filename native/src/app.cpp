/*
    Copyright (c) 2020-2023, Inochi2D Project
    Copyright (c) 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    Native translation of the independently authored source/app.d.
*/
#include "app.hpp"
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core.hpp>
#include <nijigenerate/core/i18n.hpp>
#include <nijigenerate/core/shortcut/base.hpp>
#include <nijigenerate/core/shortcut/defaults.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/panels/package.hpp>
#include <nijigenerate/panels/inspector/package.hpp>
#include <nijigenerate/panels/logger.hpp>
#include <nijigenerate/panels/actionhistory.hpp>
#include <nijigenerate/panels/toolsettings.hpp>
#include <nijigenerate/panels/scene.hpp>
#include <nijigenerate/panels/timeline.hpp>
#include <nijigenerate/panels/animlist.hpp>
#include <nijigenerate/panels/nodes_menu.hpp>
#include <nijigenerate/panels/armedparam.hpp>
#include <nijigenerate/panels/autorig.hpp>
#include <nijigenerate/panels/agent.hpp>
#include <nijigenerate/panels/resource.hpp>
#include <nijigenerate/panels/parameters.hpp>
#include <nijigenerate/panels/shell/shell.hpp>
#include <nijigenerate/panels/viewport.hpp>
#include <nijigenerate/windows/base.hpp>
#include <nijigenerate/windows/welcome.hpp>
#include <nijigenerate/windows/flipconfig.hpp>
#include <nijigenerate/widgets/mainmenu.hpp>
#include <nijigenerate/widgets/modal.hpp>
#include <nijigenerate/widgets/output_internal.hpp>
#include <nijigenerate/viewport/package.hpp>
#include <nijigenerate/viewport/vertex/package.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/lasso.hpp>
#include <nijigenerate/commands/package.hpp>
#include <nijigenerate/commands/depth/bone.hpp>
#include <nijigenerate/commands/depth/detail/bone_flush.hpp>
#include <nijigenerate/io/autosave.hpp>
#include <nijigenerate/io/videoexport.hpp>
#include <nijigenerate/atlas/atlas.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/utils/crashdump.hpp>
#include <nijigenerate/api/mcp/task.hpp>
#include <nijigenerate/backend/runtime_gl_blend.hpp>
#include <runtime_state.hpp>
#include <timing.hpp>
#include <nodes/drawable.hpp>
#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#if defined(NG_HAVE_MCP)
#include <nijigenerate/api/mcp/server.hpp>
#endif
#if defined(NG_REGRESSION_SMOKE)
// The actual M194 header/body is required; no local smoke runner is substituted.
#include <nijigenerate/regression_smoke.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#endif

#if UINTPTR_MAX == UINT32_MAX
#pragma message("nijigenerate does not support compilation on 32 bit platforms")
#endif
static_assert(sizeof(void*) >= 8, "nijigenerate does not support 32-bit builds.");

namespace nijigenerate::app {
namespace {
class NativeExceptionReport final : public compat::ThrowableException {
    // Preserve the actual foreign exception allocation while the common crash
    // reporter consumes its message. This is a native interface classification,
    // not a reconstructed D exception type or implementation-defined RTTI name.
    std::exception_ptr origin;
    static compat::RetainedString message(const std::exception& error) {
        const auto* retained = dynamic_cast<const compat::ExceptionMessage*>(&error);
        return compat::RetainedString(retained ? retained->ngExceptionMessage() : std::string_view(error.what()));
    }
public:
    NativeExceptionReport(const std::exception& error, std::exception_ptr owner)
        : compat::ThrowableException(message(error)), origin(std::move(owner)) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "native.std.exception"; }
    std::exception_ptr originException() const noexcept { return origin; }
};
void openProject(Arguments args) {
    if (core::settings::incSettingsGet<bool>("hasDoneQuickSetup", false) && args.size() > 1) {
        project::incNewProject();
        project::incOpenProject(args[1]);
    } else {
        project::incNewProject();
        windows::base::incPushWindow(windows::base::ngCreateWindow<windows::welcome::WelcomeWindow>());
    }
}
void updateContents() {
    core::shortcut::incHandleShortcuts();
    widgets::incMainMenu();
    panels::incUpdatePanels();
    windows::base::incUpdateWindows();
    core::tasks::ngStatusUpdate();
    commands::depth::bone::ngFlushDepthBoneDirtyForFrame();
}
}
void incUpdate() {
    panels::autorig::ngAutoRigPollWorker();
    project::incAnimationUpdate();
    nicxlive::core::inUpdate();
    io::autosave::incCheckAutosave();
    core::ngBeginLoop();
    if (core::ngShouldProcess()) {
        core::shortcut::incHandleShortcuts();
        api::mcp::task::ngMcpProcessQueue();
        panels::autorig::ngAutoRigPollWorker();
        widgets::incMainMenu();
        panels::incUpdatePanels();
        windows::base::incUpdateWindows();
        core::tasks::ngStatusUpdate();
        commands::depth::bone::ngFlushDepthBoneDirtyForFrame();
    }
    core::ngEndLoop();
}
void incUpdateNoEv() {
    panels::autorig::ngAutoRigPollWorker();
    project::incAnimationUpdate();
    nicxlive::core::inUpdate();
    core::ngBeginLoopNoEv();
    if (core::ngShouldProcess()) updateContents();
    core::ngEndLoop();
}
int ngRunEditor(Arguments args) {
    core::thread_context::ngRegisterMainThread();
    commands::depth::bone::ngInitDepthBoneModule();
    widgets::output::ngBootstrapOutputThread();
    viewport::common::mesheditor::tools::lasso::ngBootstrapLassoThread();
    viewport::vertex::ngInitializeVertexViewportModule();
    viewport::ngInitViewportModule();
    panels::viewport::ngInitializeViewportPanelModule();
    panels::timeline::ngInitializeTimelinePanelModule();
    panels::animlist::ngInitializeAnimListPanelModule();
    panels::nodes::ngInitializeNodesPanelModule();
    panels::inspector::ngInitializeInspectorPanelModule();
    panels::scene::ngInitializeScenePanelModule();
    panels::toolsettings::ngInitializeToolSettingsPanelModule();
    panels::logger::ngInitializeLoggerPanelModule();
    panels::actionhistory::ngInitializeActionHistoryPanelModule();
    panels::parameters::ngInitializeParametersPanel();
    panels::armedparam::ngInitializeArmedParametersPanel();
    panels::resource::ngInitializeResourcePanelModule();
    panels::shell::ngInitializeShellPanelModule();
    panels::autorig::ngInitializeAutoRigPanelModule();
    panels::agent::ngInitializeAgentPanelModule();
    // Register after incPanels so main-thread teardown stops the Agent before
    // its registry is destroyed, including returns before the loop stop scope.
    struct AgentModuleExit {
        ~AgentModuleExit() noexcept(false) { panels::agent::ngShutdownAgentPanelModule(); }
    };
    thread_local AgentModuleExit agentModuleExit;
    (void)agentModuleExit;
    if (args.size() >= 3 && args[1].view() == "--crash-notify") {
        core::settings::incSettingsLoad();
        core::i18n::ngLocaleInitFromSettings();
        utils::crashdump::notifyCrashUser(args[2]);
        return 0;
    }
    try {
#if defined(NG_REGRESSION_SMOKE)
        auto smoke = regression_smoke::ngParseRegressionSmokeOptions(args);
#endif
        utils::crashdump::installNativeCrashDumpHandler();
        bool servicesStopped = false;
        const auto stopBackgroundServices = [&] {
            if (servicesStopped) return;
            servicesStopped = true;
            panels::autorig::ngAutoRigStopAll();
#if defined(NG_HAVE_MCP)
            api::mcp::server::ngMcpStop();
#endif
            panels::agent::ngAcpStopAll();
        };
        const auto run = [&]() -> int {
            core::settings::incSettingsLoad();
            core::i18n::ngLocaleInitFromSettings();
            nicxlive::core::nodes::inSetUpdateBounds(true);
            panels::incInitPanels();
            core::actionstack::incActionInit();
            core::ngOpenWindow();
#if defined(NG_REGRESSION_SMOKE)
            if (smoke.enabled) SDL_GL_SetSwapInterval(0);
#endif
            const bool fallback = core::settings::incSettingsGet<bool>("TripleBufferFallback",
                backend::runtime_gl::oglIsTripleBufferFallbackEnabled());
            backend::runtime_gl::oglSetTripleBufferFallback(fallback);
            backend::runtime_gl::oglApplyBlendingCapabilities();
            ext::incInitExt();
            windows::flipconfig::ngInitFlipConfig();
            panels::resource::ngInitResourcePanel();
            io::videoexport::incInitVideoExport();
            atlas::atlas::incInitAtlassing();
            nicxlive::core::inPostProcessingAddBasicLighting();
            commands::ngInitAllCommands();
            core::shortcut::ngRegisterDefaultShortcuts();
            core::shortcut::ngLoadShortcutsFromSettings();
#if defined(NG_HAVE_MCP)
#if defined(NG_REGRESSION_SMOKE)
            if (!smoke.enabled)
#endif
                api::mcp::server::ngMcpLoadSettings();
#endif
#if defined(NG_REGRESSION_SMOKE)
            const auto reportSmokeFailure = [] {
                const auto failure = std::string("Regression smoke failed: ") +
                    std::string(regression_smoke::ngRegressionSmokeFailureMessage().view());
                std::cerr << failure << '\n';
                std::ofstream output("regression-smoke-failure.txt", std::ios::binary);
                output.exceptions(std::ios::failbit | std::ios::badbit);
                output << failure << '\n';
            };
            if (smoke.enabled) {
                project::incNewProject();
                regression_smoke::ngSetupRegressionSmokeScenario(smoke.scenario);
                if (regression_smoke::ngRegressionSmokeFailed()) {
                    reportSmokeFailure(); stopBackgroundServices(); return 1;
                }
            } else
#endif
                openProject(args);
#if defined(NG_IN_NIGHTLY)
            const auto warning = core::translation::ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("Warning!"));
            const auto nightlyMessage = core::translation::ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral(
                    "You're running a nightly build of nijigenerate!\nnijigenerate may crash unexpectedly and you will likely encounter bugs.\nMake sure to save and back up your work often!"));
            widgets::modal::ngModalAdd(std::make_shared<widgets::modal::Nagscreen>(
                warning, nightlyMessage, 5));
#endif
#if defined(NG_REGRESSION_SMOKE)
            int frame = 0;
#endif
            while (!core::ngIsCloseRequested()) {
#if defined(NG_REGRESSION_SMOKE)
                if (smoke.enabled && !smoke.computerUse) incUpdateNoEv(); else incUpdate();
                if (smoke.enabled && ++frame >= smoke.frames) break;
                if (smoke.enabled && smoke.frameDelayMs > 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(smoke.frameDelayMs));
#else
                incUpdate();
#endif
            }
#if defined(NG_REGRESSION_SMOKE)
            if (smoke.enabled) {
                stopBackgroundServices();
                if (regression_smoke::ngRegressionSmokeFailed()) reportSmokeFailure();
                return regression_smoke::ngRegressionSmokeFailed() ? 1 : 0;
            }
#endif
            core::settings::incSettingsSave();
            stopBackgroundServices();
            core::ngFinalize();
            return 0;
        };
        int result = 0;
        compat::ngRunWithScopeExit([&] { result = run(); }, stopBackgroundServices);
        return result;
    } catch (const compat::Throwable& error) {
        utils::crashdump::crashdump(error);
#if defined(NG_DEBUG) && !defined(_WIN32)
        if (const auto* native = dynamic_cast<const NativeExceptionReport*>(&error))
            std::rethrow_exception(native->originException());
        throw;
#endif
    } catch (const std::exception& error) {
        // Current genuine native providers may expose std::exception plus the
        // retained message interface instead of the D Throwable hierarchy.
        // Keep them inside the original application's fatal-reporting boundary.
        const NativeExceptionReport report(error, std::current_exception());
        utils::crashdump::crashdump(report);
#if defined(NG_DEBUG) && !defined(_WIN32)
        throw;
#endif
    }
    return 0;
}
}

