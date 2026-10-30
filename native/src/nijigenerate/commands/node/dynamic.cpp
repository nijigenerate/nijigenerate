#include "dynamic.hpp"
#include <common/string_key_map.hpp>
#include "dynamic_discovery.hpp"
#include "node_format.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>
namespace nijigenerate::commands::node::dynamic {
namespace {
// Only references reach allocateCommandResult. Its real Result allocation precedes
// these parameter conversions, whose C++ order is unspecified. Whichever conversion
// runs first captures the source's bool and slice descriptor before any text callback.
class ConvertedResultArguments {
    const base::Nodes& converted;
    const base::Nodes& before;
    bool captured = false;
    bool success = false;
    base::Nodes values;

    void capturePrefix() {
        if (captured) return;
        success = !converted.empty();
        values = converted;
        captured = true;
    }
public:
    ConvertedResultArguments(const base::Nodes& converted, const base::Nodes& before)
        : converted(converted), before(before) {}

    struct Success {
        ConvertedResultArguments& arguments;
        operator bool() const {
            arguments.capturePrefix();
            return arguments.success;
        }
    };
    struct Values {
        ConvertedResultArguments& arguments;
        operator base::Nodes() const {
            arguments.capturePrefix();
            return arguments.values;
        }
    };
    struct Message {
        ConvertedResultArguments& arguments;
        operator compat::RetainedString() const {
            arguments.capturePrefix();
            return arguments.converted.empty()
                ? ::nijigenerate::commands::node::detail::tr("No nodes converted")
                : ::nijigenerate::commands::node::detail::format(
                    "Nodes converted from %s", arguments.before.size());
        }
    };
};
} // namespace

template<bool E> ConvertNodeToCommand<E>::ConvertNodeToCommand(compat::RetainedString type)
    : ConvertNodeToCommand(SourceConstructorFrame::capture(type)) {}
template<bool E> typename ConvertNodeToCommand<E>::SourceConstructorFrame
ConvertNodeToCommand<E>::SourceConstructorFrame::capture(compat::RetainedString& type) {
    return {::nijigenerate::commands::node::detail::format("Convert To %s",type),
        ::nijigenerate::commands::node::detail::format("Convert selected nodes to %s",type),type};
}
template<bool E> ConvertNodeToCommand<E>::ConvertNodeToCommand(SourceConstructorFrame frame)
    : NativeBase(std::move(frame.label), std::move(frame.description), frame.type) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ConvertNodeToCommand<E>), E
        ? compat::RetainedString::fromStaticLiteral(
            "nijigenerate.commands.node.dynamic.ConvertNodeToCommand!true.ConvertNodeToCommand")
        : compat::RetainedString::fromStaticLiteral(
            "nijigenerate.commands.node.dynamic.ConvertNodeToCommand!false.ConvertNodeToCommand"));
}
template<bool E> bool ConvertNodeToCommand<E>::runnable(std::shared_ptr<Context> ctx) {
    const auto commandOwner = this->weak_from_this().lock();
    if(!compat::ngRequireObject(ctx)->hasNodes() || ctx->nodes().empty()) return false;
    auto from=base::ngGetCommonNodeType(ctx->nodes()); if(from.isNull()) return false;
    const auto map=base::conversionMap();
    if (const auto found = map.find(from.view())) {
        const auto values = *found;
        for (auto value : values) if (value == this->toType) return true;
    }
    return false;
}
template<bool E> std::shared_ptr<typename ConvertNodeToCommand<E>::Result>
ConvertNodeToCommand<E>::runTyped(std::shared_ptr<Context> ctx) {
    const auto commandOwner = this->weak_from_this().lock();
    if(!runnable(ctx)) return Result::opCall(false,{},
        compat::RetainedString::fromStaticLiteral("Context not convertible"));
    auto before=compat::ngRequireObject(ctx)->nodes().duplicate();
    const auto conversionNodes = ctx->nodes();
    const auto conversionType = this->toType;
    auto converted = base::ngConvertTo(conversionNodes, conversionType);
    ConvertedResultArguments arguments(converted, before);
    return ::nijigenerate::commands::detail::allocateCommandResult<Result>(
        ConvertedResultArguments::Success{arguments}, ConvertedResultArguments::Values{arguments},
        ConvertedResultArguments::Message{arguments});
}
template class ConvertNodeToCommand<true>; template class ConvertNodeToCommand<false>;
std::shared_ptr<node::AddNodeCommandT<false>> ensureAddNodeCommand(compat::RetainedString name,compat::RetainedString suffix) {
    AddNodeKey key(name,suffix); auto found=addNodeCommands.find(key);
    if(found!=addNodeCommands.end()) return found->second;
    auto command=ngCreateRegisteredCommand<node::AddNodeCommandT<false>>(name,suffix);
    addNodeCommands[key]=command; return command;
}
std::shared_ptr<node::InsertNodeCommandT<false>> ensureInsertNodeCommand(compat::RetainedString name,compat::RetainedString suffix) {
    InsertNodeKey key(name,suffix); auto found=insertNodeCommands.find(key);
    if(found!=insertNodeCommands.end()) return found->second;
    auto command=ngCreateRegisteredCommand<node::InsertNodeCommandT<false>>(name,suffix);
    insertNodeCommands[key]=command; return command;
}
void ngInitAddNodeCommands() { for(auto type:detail::discoverNodeTypes()) ensureAddNodeCommand(type); }
void ngInitInsertNodeCommands() { for(auto type:detail::discoverNodeTypes()) ensureInsertNodeCommand(type); }
void ngInitConvertNodeCommands() {
    nicxlive::core::common::StringKeyMap<compat::RetainedString> added;
    base::conversionMap().forEach([&](std::string_view, auto values) {
        for(auto to:values) {
            const compat::RetainedStringSlice id{to};
            if (added.contains(id.view())) continue;
            added.setRetained(to, to);
            ConvertToKey key{to}; if(convertNodeCommands.contains(key)) continue;
            auto command=ngCreateRegisteredCommand<ConvertNodeToCommand<false>>(to);
            convertNodeCommands[key]=command;
        }
    });
}
std::shared_ptr<ConvertNodeToCommand<false>> ensureConvertToCommand(compat::RetainedString type) {
    ConvertToKey key{type}; auto found=convertNodeCommands.find(key);
    if(found!=convertNodeCommands.end()) {
        auto result=found->second;
        if(compat::ngRequireObject(result)->toType != type) result->toType=type;
        return found->second;
    }
    auto command=ngCreateRegisteredCommand<ConvertNodeToCommand<false>>(type);
    convertNodeCommands[key]=command; return command;
}
}
