#include "node_commands.hpp"
#include "node_format.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/vertex/package.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/retained_exception_message.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nodes/deformable.hpp>
#include <array>
#include <new>
#include <utility>
namespace nijigenerate::commands::node::node {
using namespace base;
using namespace project;
using namespace actions::node;
using namespace core::actionstack;
using Result = CreateResult<NodeRef>;
namespace {
template<std::size_t N> compat::RetainedString tr(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
template<std::size_t N, class... Args>
compat::RetainedString format(const char (&pattern)[N], const Args&... args) {
    const auto translated = tr(pattern);
    const std::array<core::translation::TypedFormatArgument, sizeof...(Args)> values{
        core::translation::ngFormatArgument(args)...};
    return ::nijigenerate::commands::node::detail::retainNodeFormatText(
        core::translation::ngFormatTyped(translated.view(), values));
}
template<std::size_t N, std::size_t M>
detail::NodeCommandText commandText(const char (&label)[N], const char (&description)[M]) {
    auto translatedLabel = tr(label);
    auto translatedDescription = tr(description);
    return {std::move(translatedLabel), std::move(translatedDescription)};
}
template<std::uint32_t Line> NodeRef firstNode(const std::shared_ptr<Context>& context) {
    const auto nodes = actions::node::detail::requireActionObject(context)->nodes();
    if (nodes.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, nodes.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/node/node.d"), Line);
    return nodes[0];
}
}

template<bool E> detail::NodeCommandDescription
AddNodeCommandT<E>::sourceConstructorDescription(const compat::RetainedString& name) {
    return {format("Add Node %s under selection or root",name)};
}
template<bool E> AddNodeCommandT<E>::AddNodeCommandT(compat::RetainedString name, compat::RetainedString suffix)
    : AddNodeCommandT(sourceConstructorDescription(name),name,suffix) {}
template<bool E> AddNodeCommandT<E>::AddNodeCommandT(detail::NodeCommandDescription text,
        compat::RetainedString name, compat::RetainedString suffix)
    : NativeBase({},text.description,name,suffix) {
    this->prepareSourceClassInfo();
}
template<bool E> std::shared_ptr<typename AddNodeCommandT<E>::Result>
AddNodeCommandT<E>::runTyped(std::shared_ptr<Context> ctx) {
    const auto commandOwner = this->weak_from_this().lock();
    if (this->className.empty()) return Result::opCall(false,{},
        compat::RetainedString::fromStaticLiteral("Node className is required"));
    Nodes created;
    try {
        ctx = actions::node::detail::requireActionObject(ctx);
        if (ctx->hasNodes()) {
            const auto nodes = ctx->nodes();
            const auto name = this->className;
            const auto suffix = this->_suffix;
            created = ngAddNodes(nodes, name, suffix);
        } else if (ctx->hasPuppet() && ctx->puppet()) {
            const Nodes nodes{actions::node::detail::requireActionObject(ctx->puppet())->root};
            const auto name = this->className;
            const auto suffix = this->_suffix;
            created = ngAddNodes(nodes, name, suffix);
        }
    } catch (const compat::RangeError&) {
        return Result::opCall(false,{},compat::RetainedString::fromStaticLiteral("Failed to add node"));
    }
    return Result::opCall(!created.empty(),created,created.empty()
        ? compat::RetainedString::fromStaticLiteral("No nodes created") : compat::RetainedString{});
}
template<bool E> detail::NodeCommandDescription
InsertNodeCommandT<E>::sourceConstructorDescription(const compat::RetainedString& name) {
    return {format("Insert Node %s under parent of selection",name)};
}
template<bool E> InsertNodeCommandT<E>::InsertNodeCommandT(compat::RetainedString name,compat::RetainedString suffix)
    : InsertNodeCommandT(sourceConstructorDescription(name),name,suffix) {}
template<bool E> InsertNodeCommandT<E>::InsertNodeCommandT(detail::NodeCommandDescription text,
        compat::RetainedString name, compat::RetainedString suffix)
    : NativeBase({},text.description,name,suffix) {
    this->prepareSourceClassInfo();
}
template<bool E> std::shared_ptr<typename InsertNodeCommandT<E>::Result>
InsertNodeCommandT<E>::runTyped(std::shared_ptr<Context> ctx) {
    const auto commandOwner = this->weak_from_this().lock();
    if (this->className.empty()) return Result::opCall(false,{},
        compat::RetainedString::fromStaticLiteral("Node className is required"));
    Nodes created;
    try {
        ctx = actions::node::detail::requireActionObject(ctx);
        if (ctx->hasNodes()) {
            const auto nodes = ctx->nodes();
            const auto name = this->className;
            const auto suffix = this->_suffix;
            created = ngInsertNodes(nodes, name, suffix);
        } else if (ctx->hasPuppet() && ctx->puppet()) {
            const Nodes nodes{actions::node::detail::requireActionObject(ctx->puppet())->root};
            const auto name = this->className;
            const auto suffix = this->_suffix;
            created = ngInsertNodes(nodes, name, suffix);
        }
    } catch(const compat::RangeError&) {
        return Result::opCall(false,{},compat::RetainedString::fromStaticLiteral("Failed to insert node"));
    }
    return Result::opCall(!created.empty(),created,created.empty()
        ? compat::RetainedString::fromStaticLiteral("No nodes inserted") : compat::RetainedString{});
}
detail::NodeCommandDescription MoveNodeCommand::sourceConstructorDescription() { return {tr("Move Node ")}; }
MoveNodeCommand::MoveNodeCommand(NodeRef parent,std::uint64_t value,bool explicitValue)
    : MoveNodeCommand(sourceConstructorDescription(),parent,value,explicitValue) {}
MoveNodeCommand::MoveNodeCommand(detail::NodeCommandDescription text, NodeRef parent,
        std::uint64_t value, bool explicitValue)
    : ExCommand{{},text.description,parent,value} {
    initializeSourceFlag(explicitValue);
    prepareSourceClassInfo();
}
std::shared_ptr<CommandResult> MoveNodeCommand::run(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    if (!ctx->hasNodes() || ctx->nodes().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No nodes"));
    auto selected=incSelectedNodes(); Nodes moving;
    try {
        if(explicitNodes || ctx->nodes().size()>1) moving=ctx->nodes();
        else { auto child=firstNode<96>(ctx); moving=incNodeInSelection(child)?selected:Nodes{child}; }
        incMoveChildrenWithHistory(moving,newParent,index);
    } catch(const compat::ThrowableError&) { throw; }
    catch(const std::bad_alloc&) { throw; }
    catch(const std::exception& ex) {
        const auto title = core::translation::ngTranslateCString("Error");
        const auto body = compat::ngRetainExceptionMessage(ex);
        widgets::dialog::ngDialog(widgets::dialog::DialogTag::retained(title.data(),title.owner),
            widgets::dialog::DialogText(body));
        return CommandResult::opCall(false, compat::ngRetainExceptionMessage(ex));
    }
    return CommandResult::opCall(true);
}
template<bool E> detail::NodeCommandDescription
ConvertToCommandT<E>::sourceConstructorDescription(const compat::RetainedString& name) {
    return {format("Convert selected nodes to %s",name)};
}
template<bool E> ConvertToCommandT<E>::ConvertToCommandT(compat::RetainedString name)
    : ConvertToCommandT(sourceConstructorDescription(name),name) {}
template<bool E> ConvertToCommandT<E>::ConvertToCommandT(detail::NodeCommandDescription text,
        compat::RetainedString name)
    : NativeBase{{},text.description,name} {
    this->prepareSourceClassInfo();
}
template<bool E> std::shared_ptr<typename ConvertToCommandT<E>::Result>
ConvertToCommandT<E>::runTyped(std::shared_ptr<Context> ctx) {
    const auto commandOwner = this->weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    if (!ctx->hasNodes()) return Result::opCall(false, {},
        compat::RetainedString::fromStaticLiteral("No nodes"));
    if (this->className.empty()) return Result::opCall(false, {},
        compat::RetainedString::fromStaticLiteral("Destination node className is required"));
    const auto before = ctx->nodes().duplicate();
    const auto nodes = ctx->nodes();
    const auto className = this->className;
    const auto converted = ngConvertTo(nodes, className);
    return Result::opCall(!converted.empty(),converted,converted.empty()?tr("No nodes converted"):format("Nodes converted from %s",before.size()));
}
detail::NodeCommandText SetNodeNameCommand::sourceConstructorText() {
    return commandText("Set Node Name", "Rename selected nodes.");
}
SetNodeNameCommand::SetNodeNameCommand(compat::Slice<compat::RetainedString> names)
    : SetNodeNameCommand(sourceConstructorText(), names) {}
SetNodeNameCommand::SetNodeNameCommand(detail::NodeCommandText text, compat::Slice<compat::RetainedString> names)
    : ExCommand{std::move(text.label), std::move(text.description), names} { prepareSourceClassInfo(); }
std::shared_ptr<CommandResult> SetNodeNameCommand::run(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    using Name = nicxlive::core::common::NullableString;
    const auto context = actions::node::detail::requireActionObject(ctx);
    if (!context->hasNodes() || context->nodes().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No nodes"));
    if (newNames.empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No node names"));
    const auto nameCount = newNames.size();
    if (nameCount != context->nodes().size()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("newNames length must match context.nodes length"));
    {
        const auto names = newNames;
        for (auto name : names) if (name.empty())
            return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("Node name is empty"));
    }
    auto group = std::allocate_shared<actions::GroupAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});
    bool changed = false;
    const auto nodes = context->nodes();
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto node = nodes[i];
        const auto names = newNames;
        if (i >= names.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, names.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/node/node.d"), 154);
        const auto name = names[i];
        const auto receiver = actions::node::detail::requireActionObject(node);
        if (receiver->getName() == name.view()) continue;
        const auto previous = receiver->getName();
        const auto next = ext::nodes::ngRetainNodeName(compat::RetainedStringSlice(name));
        const auto pointer = std::shared_ptr<Name>(receiver, &receiver->name);
        group->addAction(std::allocate_shared<NodeValueChangeAction<nicxlive::core::nodes::Node, Name>>(
            nicxlive::core::d_runtime::SourceAllocator<NodeValueChangeAction<nicxlive::core::nodes::Node, Name>>{},
            compat::RetainedString::fromStaticLiteral("name"), node, previous, next, pointer));
        changed = true;
    }
    if (changed) { group->redo(); incActionPush(group); }
    return CommandResult::opCall(changed, changed ? compat::RetainedString::emptyLiteral()
        : compat::RetainedString::fromStaticLiteral("No node names changed"));
}
detail::NodeCommandDescription DeleteNodeCommand::sourceConstructorDescription() { return {tr("Delete Node")}; }
DeleteNodeCommand::DeleteNodeCommand(bool value) : DeleteNodeCommand(sourceConstructorDescription(),value) {}
DeleteNodeCommand::DeleteNodeCommand(detail::NodeCommandDescription text,bool value) : ExCommand{{},text.description} {
    initializeSourceFlag(value);
    prepareSourceClassInfo();
}
std::shared_ptr<CommandResult> DeleteNodeCommand::run(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    if (!ctx->hasNodes() || ctx->nodes().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No nodes"));
    auto n=firstNode<182>(ctx); auto selected=incSelectedNodes();
    if(explicitNodes) incDeleteChildrenWithHistory(ctx->nodes());
    else if(selected.size()>1) { incDeleteChildrenWithHistory(selected); incSelectNode(nullptr); }
    else { if(incNodeInSelection(n)) incSelectNode(nullptr); incDeleteChildWithHistory(n); }
    incSelectNode(nullptr); return CommandResult::opCall(true);
}
detail::NodeCommandText CutNodeCommand::sourceConstructorText() { return commandText("Cut", "Cut Node"); }
CutNodeCommand::CutNodeCommand() : CutNodeCommand(sourceConstructorText()) {}
CutNodeCommand::CutNodeCommand(detail::NodeCommandText text)
    : ExCommand{std::move(text.label), std::move(text.description)} { prepareSourceClassInfo(); }
std::shared_ptr<CommandResult> CutNodeCommand::run(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    if (!ctx->hasNodes() || ctx->nodes().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No nodes"));
    return CommandResult::opCall(true);
}
bool CutNodeCommand::runnable(std::shared_ptr<Context>) { return false; }
detail::NodeCommandText CopyNodeCommand::sourceConstructorText() { return commandText("Copy", "Copy Node"); }
CopyNodeCommand::CopyNodeCommand() : CopyNodeCommand(sourceConstructorText()) {}
CopyNodeCommand::CopyNodeCommand(detail::NodeCommandText text)
    : ExCommand{std::move(text.label), std::move(text.description)} { prepareSourceClassInfo(); }
std::shared_ptr<CommandResult> CopyNodeCommand::run(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    if (!ctx->hasNodes() || ctx->nodes().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No nodes"));
    auto n=firstNode<238>(ctx); auto selected=incSelectedNodes(); copyToClipboard(selected.empty()?Nodes{n}:selected);
    return CommandResult::opCall(true);
}
bool CopyNodeCommand::runnable(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    return ctx->hasNodes() && !ctx->nodes().empty();
}
detail::NodeCommandText PasteNodeCommand::sourceConstructorText() { return commandText("Paste", "Paste Node"); }
PasteNodeCommand::PasteNodeCommand() : PasteNodeCommand(sourceConstructorText()) {}
PasteNodeCommand::PasteNodeCommand(detail::NodeCommandText text)
    : ExCommand{std::move(text.label), std::move(text.description)} { prepareSourceClassInfo(); }
std::shared_ptr<CommandResult> PasteNodeCommand::run(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    if (!ctx->hasNodes() || ctx->nodes().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No nodes"));
    auto n=firstNode<260>(ctx);
    if(!clipboardNodes.empty()) { pasteFromClipboard(n); return CommandResult::opCall(true); }
    return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("Clipboard empty"));
}
bool PasteNodeCommand::runnable(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    return ctx->hasNodes() && !ctx->nodes().empty() && !clipboardNodes.empty();
}
detail::NodeCommandDescription ReloadNodeCommand::sourceConstructorDescription() { return {tr("Reload Node")}; }
ReloadNodeCommand::ReloadNodeCommand() : ReloadNodeCommand(sourceConstructorDescription()) {}
ReloadNodeCommand::ReloadNodeCommand(detail::NodeCommandDescription text)
    : ExCommand{text.description} { prepareSourceClassInfo(); }
std::shared_ptr<CommandResult> ReloadNodeCommand::run(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    if(ctx->hasNodes()) { incReloadNode(ctx->nodes()); return CommandResult::opCall(true); }
    return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No nodes"));
}
detail::NodeCommandDescription VertexModeCommand::sourceConstructorDescription() { return {tr("Edit Vertex")}; }
VertexModeCommand::VertexModeCommand() : VertexModeCommand(sourceConstructorDescription()) {}
VertexModeCommand::VertexModeCommand(detail::NodeCommandDescription text)
    : ExCommand{text.description} { prepareSourceClassInfo(); }
std::shared_ptr<CommandResult> VertexModeCommand::run(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    if(ctx->hasNodes() && !ctx->nodes().empty())
        if(auto d=std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(firstNode<293>(ctx)))
            if(!incArmedParameter()) viewport::vertex::incVertexEditStartEditing(d);
    return CommandResult::opCall(true);
}
detail::NodeCommandDescription ToggleVisibilityCommand::sourceConstructorDescription() { return {tr("Toggle Visibility")}; }
ToggleVisibilityCommand::ToggleVisibilityCommand() : ToggleVisibilityCommand(sourceConstructorDescription()) {}
ToggleVisibilityCommand::ToggleVisibilityCommand(detail::NodeCommandDescription text)
    : ExCommand{text.description} { prepareSourceClassInfo(); }
std::shared_ptr<CommandResult> ToggleVisibilityCommand::run(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    if (!ctx->hasNodes() || ctx->nodes().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No nodes"));
    const auto n = firstNode<312>(ctx);
    const auto receiver = actions::node::detail::requireActionObject(n);
    const bool enabled = !receiver->getEnabled();
    receiver->setEnabled(enabled);
    incActionPush(std::allocate_shared<NodeActiveAction>(
        nicxlive::core::d_runtime::SourceAllocator<NodeActiveAction>{}, n, enabled));
    return CommandResult::opCall(true);
}
detail::NodeCommandDescription CentralizeNodeCommand::sourceConstructorDescription() { return {tr("Centralize Node")}; }
CentralizeNodeCommand::CentralizeNodeCommand() : CentralizeNodeCommand(sourceConstructorDescription()) {}
CentralizeNodeCommand::CentralizeNodeCommand(detail::NodeCommandDescription text)
    : ExCommand{text.description} { prepareSourceClassInfo(); }
std::shared_ptr<CommandResult> CentralizeNodeCommand::run(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = actions::node::detail::requireActionObject(ctx);
    if (!ctx->hasNodes() || ctx->nodes().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No nodes"));
    const auto node = firstNode<328>(ctx);
    incActionPush(std::allocate_shared<NodeCentralizeAction>(
        nicxlive::core::d_runtime::SourceAllocator<NodeCentralizeAction>{}, node));
    return CommandResult::opCall(true);
}

template class AddNodeCommandT<true>; template class AddNodeCommandT<false>;
template class InsertNodeCommandT<true>; template class InsertNodeCommandT<false>;
template class ConvertToCommandT<true>; template class ConvertToCommandT<false>;
void ngInitNodeCommands() {
    initialization::Program::run<initialization::Table>();
}
}
