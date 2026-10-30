#pragma once
#include "base.hpp"
#include "command_browser_internal.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/compat/slice.hpp>
namespace nijigenerate::windows::command_browser {
using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
using ParameterRef = std::shared_ptr<nicxlive::core::param::Parameter>;
using BindingRef = std::shared_ptr<nicxlive::core::param::ParameterBinding>;
using PuppetRef = std::shared_ptr<nicxlive::core::Puppet>;
using Nodes = compat::Slice<NodeRef>;
using Parameters = compat::Slice<ParameterRef>;
using Bindings = compat::Slice<BindingRef>;
using BrowserText = detail::BrowserText;
using Texts = compat::Slice<BrowserText>;
template<class V> using TextMap = detail::BrowserTextMap<V>;

class CommandBrowserWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    BrowserText filterText;
    std::size_t selectedIndex = 0;
    detail::CommandRef selectedCmd;
    bool initialized = false;
    detail::RawArguments argValues;
    BrowserText ctxNodes, ctxParams, ctxArmed, ctxKeyPoint;
    bool lastHasResult = false, lastSucceeded = false;
    BrowserText lastMessage, lastResultType, lastResultDetail;
    Texts lastCreated, lastDeleted, lastLoaded;
    TextMap<Nodes> argNodeSelections;
    TextMap<Parameters> argParamSelections;
    TextMap<Bindings> argBindingSelections;
    Nodes ctxNodesSel;
    Parameters ctxParamsSel, ctxArmedSel;
    Bindings ctxBindingsSel;
    std::uint32_t newPickerTemp = 0;
    bool contextDirty = false;
    void initialize();
    void resetSelection(detail::CommandInfo info);
    void captureDefaultContext();
    void drawInputsTable(detail::CommandInfo& info);
    void drawOutputsTable(detail::CommandInfo& info);
    void drawContextTable();
    void executeSelected(detail::CommandInfo& info);
protected:
    void onBeginUpdate() override;
    void onEndUpdate() override;
    void onUpdate() override;
public:
    CommandBrowserWindow() = default;
};

void renderNodeTree(NodeRef node, NodeRef& chosen, bool (*accepts)(NodeRef) = nullptr);
void renderParameterTree(PuppetRef puppet, ParameterRef& chosen);
void renderBindingTree(PuppetRef puppet, BindingRef& chosen);
BrowserText labelFor(NodeRef value);
BrowserText labelFor(ParameterRef value);
BrowserText labelFor(BindingRef value);
#ifdef NG_COMMAND_BROWSER_DIFFERENTIAL
BrowserText ngCommandBrowserDifferentialReport();
#endif
namespace detail {
std::uint32_t ensureCommandBrowserBindingId(BindingRef value);
BindingRef findBindingByCommandBrowserId(std::uint32_t id);
extern compat::Slice<CommandInfo> gCommandInfos;
extern BrowserClassMap<CommandRef, CommandInfo> gCommandInfosByCmd;
void rebuildCommandInfos();
BrowserText browserResultTypeName(const std::shared_ptr<commands::CommandResult>& result);
}
}

#include "command_browser_resources.hpp"
