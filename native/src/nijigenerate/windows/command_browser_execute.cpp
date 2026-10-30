#include "command_browser_helpers.hpp"
#include <nijigenerate/core/shortcut/base.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <new>
#include <string_view>
namespace nijigenerate::windows::command_browser {
using namespace detail;
using namespace commands;
namespace {
template<class R> BrowserText resultLabel(R value) {
    if (!value) return compat::RetainedString::fromStaticLiteral("(null)");
    if constexpr (std::same_as<R,BindingRef>) return ext::nodes::ngRetainNodeName(value->getNameDescriptor());
    else if constexpr (std::same_as<R,NodeRef>) return ext::nodes::ngRetainNodeName(value->getName());
    else return ext::nodes::ngRetainNodeName(value->getName());
}
template<class R> void captureResources(const std::shared_ptr<CommandResult>& result,
    Texts& created, Texts& deleted, Texts& loaded) {
    if (auto values = std::dynamic_pointer_cast<CreateResult<R>>(result)) {
        const auto resources = values->created;
        for (auto value : resources) created.append(resultLabel(value));
    }
    if (auto values = std::dynamic_pointer_cast<DeleteResult<R>>(result)) {
        const auto resources = values->deleted;
        for (auto value : resources) deleted.append(resultLabel(value));
    }
    if (auto values = std::dynamic_pointer_cast<LoadResult<R>>(result)) {
        const auto resources = values->loaded;
        for (auto value : resources) loaded.append(resultLabel(value));
    }
    // The original extra ExCommandResult<R> branch statically tests R itself for
    // Create/Delete/Load ancestry. None of its three resource handle types match.
}
}
void CommandBrowserWindow::executeSelected(CommandInfo& info) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    auto ctx = core::shortcut::ngBuildExecutionContext();
    auto puppet = project::incActivePuppet();
    auto parseList = [&](BrowserText& source, Parameters& parameters, Parameters&, Nodes& nodes) {
        // Source splitter retains the original descriptor and traverses lazily:
        // callbacks may replace source without replacing this active input.
        const auto captured = source;
        for (std::size_t start = 0;;) {
            const auto end = captured.view().find(',', start);
            auto part = captured.slice(start, end == std::string_view::npos ? captured.size() : end);
            auto stripped = compat::ngStripUnicode(part.view());
            if (!stripped.empty()) {
                std::uint32_t id;
                if (parseInteger(stripped,id)) {
                    try {
                        if (puppet) {
                            auto parameter = puppet->findParameter(id);
                            if (parameter) parameters.append(parameter);
                            auto node = puppet->findNodeById(id);
                            if (node) nodes.append(node);
                        }
                    } catch (const compat::ThrowableException&) {}
                }
            }
            if (end == std::string_view::npos) break;
            start = end + 1;
        }
    };
    Parameters paramsOverride, armedOverride;
    Nodes nodesOverride;
    if (!ctxParamsSel.empty()) paramsOverride = ctxParamsSel;
    else parseList(ctxParams,paramsOverride,armedOverride,nodesOverride);
    if (!ctxArmedSel.empty()) armedOverride = ctxArmedSel;
    else if (!ctxArmed.empty()) {
        Parameters temporary, armedTemporary; Nodes dummy;
        parseList(ctxArmed,temporary,armedTemporary,dummy);
        if (!armedTemporary.empty()) armedOverride = armedTemporary;
    }
    if (!ctxNodesSel.empty()) nodesOverride = ctxNodesSel;
    else if (!ctxNodes.empty()) {
        Parameters temporary; parseList(ctxNodes,temporary,armedOverride,nodesOverride);
    }
    Bindings bindingsOverride;
    if (!ctxBindingsSel.empty()) bindingsOverride = ctxBindingsSel;
    if (contextDirty) {
        compat::ngRequireObject(ctx)->parameters(paramsOverride);
        compat::ngRequireObject(ctx)->armedParameters(armedOverride);
        compat::ngRequireObject(ctx)->nodes(nodesOverride);
        compat::ngRequireObject(ctx)->bindings(bindingsOverride);
        compat::ngRequireObject(ctx)->activeBindings(bindingsOverride);
    } else {
        if (!paramsOverride.empty()) compat::ngRequireObject(ctx)->parameters(paramsOverride);
        if (!armedOverride.empty()) compat::ngRequireObject(ctx)->armedParameters(armedOverride);
        if (!nodesOverride.empty()) compat::ngRequireObject(ctx)->nodes(nodesOverride);
        if (!bindingsOverride.empty()) {
            compat::ngRequireObject(ctx)->bindings(bindingsOverride);
            compat::ngRequireObject(ctx)->activeBindings(bindingsOverride);
        }
    }
    if (!ctxKeyPoint.empty()) {
        auto parts = split(ctxKeyPoint);
        if (parts.size() >= 2) {
            try {
                std::uint32_t x,y;
                if (parseInteger(compat::ngStripUnicode(parts[0].view()),x) &&
                    parseInteger(compat::ngStripUnicode(parts[1].view()),y)) {
                    compat::ngRequireObject(ctx)->keyPoint(Context::KeyPoint{x,y});
                    compat::ngRequireObject(ctx)->hasExplicitKeyPoint(true);
                }
            } catch (const compat::ThrowableException&) {}
        }
    }
    if (info.applyArgs && selectedCmd) info.applyArgs(selectedCmd,argValues);
    std::shared_ptr<CommandResult> result;
    try {
        if (!ngCommandAllowedInCurrentContext(selectedCmd))
            result = CommandResult::opCall(false,"Command is not available in the current context");
        else result = ngRunCommand(selectedCmd,ctx);
        lastHasResult = true;
        lastSucceeded = compat::ngRequireObject(result)->succeeded;
        lastMessage = BrowserText(compat::ngRequireObject(result)->message);
        lastResultType = browserResultTypeName(result); lastResultDetail = emptyText();
        lastCreated = lastDeleted = lastLoaded = Texts();
        captureResources<NodeRef>(result,lastCreated,lastDeleted,lastLoaded);
        captureResources<ParameterRef>(result,lastCreated,lastDeleted,lastLoaded);
        captureResources<BindingRef>(result,lastCreated,lastDeleted,lastLoaded);
    } catch (const compat::ThrowableException& error) {
        lastHasResult = true; lastSucceeded = false;
        lastMessage = concat(compat::RetainedString::fromStaticLiteral("Exception: "),
            BrowserText(error.msg));
        lastResultType = compat::RetainedString::fromStaticLiteral("Exception"); lastResultDetail = emptyText();
    }
}
}
