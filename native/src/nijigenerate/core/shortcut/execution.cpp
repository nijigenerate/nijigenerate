#include "base_internal.hpp"
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/panels/inspector/base.hpp>
#include <nijigenerate/widgets/modal/base.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/source_allocator.hpp>
#include <imgui.h>

namespace nijigenerate::core::shortcut {
namespace {
std::shared_ptr<commands::Context> buildExecutionContext() {
    auto context = std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
    context->puppet(project::incActivePuppet());
    auto nodes = detail::selectedNodesProvider ? detail::selectedNodesProvider() : project::incSelectedNodes();
    if (!nodes.empty()) context->nodes(nodes);
    auto armed = project::incArmedParameter();
    if (armed) context->armedParameters({armed});
    auto parameters = project::incSelectedParams();
    if (!parameters.empty()) context->parameters(parameters);
    if (detail::selectedBindingsProvider) {
        auto bindings = detail::selectedBindingsProvider();
        if (!bindings.empty()) context->activeBindings(bindings);
    }
    if (project::incArmedParameter()) {
        auto bindings = compat::ngRequireObject(project::incArmedParameter())->bindings;
        context->bindings(commands::Context::Bindings::retainedView(
            bindings.retentionOwner(), bindings.data(), bindings.size()));
    }
    if (detail::paramPointProvider) context->keyPoint(detail::paramPointProvider());
    commands::Context::Inspectors inspectors;
    bool resolvedInspectors = false;
    static thread_local std::shared_ptr<panels::inspector::InspectorPanel> panel;
    if (!panel) {
        const auto candidates = panels::incPanels;
        for (auto candidate : candidates) {
            panel = std::dynamic_pointer_cast<panels::inspector::InspectorPanel>(candidate);
            if (panel) break;
        }
    }
    if (panel) {
        if (panel->activeNodeInspectors) {
            const auto holder = panel->activeNodeInspectors;
            const auto instances = compat::ngRequireObject(holder)->getAll();
            for (const auto& inspector : instances) inspectors.append(inspector);
            resolvedInspectors = !inspectors.empty();
        }
        if (!inspectors.empty()) context->inspectors(inspectors);
    }
    if (!resolvedInspectors && !nodes.empty()) {
        auto holder = panels::inspector::ngNodeInspector(nodes);
        auto instances = compat::ngRequireObject(holder)->getAll();
        if (!instances.empty()) context->inspectors(instances);
    }
    return context;
}
}

std::shared_ptr<commands::Context> ngBuildExecutionContext() { return buildExecutionContext(); }

void incHandleShortcuts() {
    if (widgets::modal::ngModalIsOpen()) return;
    if (detail::shortcutCaptureActive) return;
    (void)ImGui::GetIO();
    // D foreach retains the AA handle and copies each value; it does not first
    // allocate an array containing every ActionEntry.
    const auto registry = detail::shortcutEntries;
    for (auto [command, entry] : registry) {
        (void)command;
        if (!commands::ngCommandAllowedInCurrentContext(entry.command)) continue;
        if (input::ngShortcut(compat::RetainedStringSlice{entry.shortcut}, entry.repeat)) {
            auto context = buildExecutionContext();
            if (commands::ngCommandAllowedInCurrentContext(entry.command) &&
                    compat::ngRequireObject(entry.command)->runnable(context))
                (void)commands::ngRunCommand(entry.command, context);
            break;
        }
    }
}
}
