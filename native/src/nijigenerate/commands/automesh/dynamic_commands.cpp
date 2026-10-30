#include "dynamic.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <new>
#include <source_location>
#ifdef CMD_LOG
#include "detail/dynamic_log_enabled.hpp"
#endif

namespace nijigenerate::commands::automesh::dynamic {
namespace detail {
compat::RetainedString applyLabel(std::string_view name) {
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Apply AutoMesh (%s)"));
    const std::array args{core::translation::ngFormatArgument(name)};
    auto text = core::translation::ngFormatTyped(pattern.view(),args);
    try { return compat::RetainedString(std::move(text)); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::RetainedString applyDescription() {
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Apply AutoMesh to selected nodes"));
}
bool canApply(const std::shared_ptr<Context>& context) {
    const auto receiver=compat::ngRequireObject(context);
    auto nodes=receiver->hasNodes()?receiver->nodes():project::incSelectedNodes();
    for(const auto node:nodes)
        if(std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node)) return true;
    return false;
}
}
namespace {
template<class PT> requires(detail::processorRegistrationSupported<PT>())
std::shared_ptr<Command> put(compat::RetainedStringSlice id) {
    auto command=ngCreateRegisteredCommand<ApplyAutoMeshPT<PT>>();
    autoMeshApplyCommands[AutoMeshKey{id}]=command;
    return command;
}

template<class... PT> std::shared_ptr<Command> ensureForTypes(
        compat::RetainedStringSlice id, compat::TypeSequence<PT...>) {
    std::shared_ptr<Command> result;
    // Preserve static foreach order and return the newly created object.
    ([&] {
        constexpr auto processorId = detail::ProcessorCommandInfo<PT>::id;
        if constexpr (!processorId.empty()) {
            if (!result && processorId == id.view()) result = put<PT>(id);
        }
    }(), ...);
    return result;
}

template<class... PT> requires((detail::processorRegistrationSupported<PT>() && ...))
void initializeForTypes(compat::TypeSequence<PT...>) {
    // Original initialization, unlike ensure, does not skip empty IDs.
    (put<PT>(compat::RetainedString::fromStaticView(detail::ProcessorCommandInfo<PT>::id)), ...);
}
}
std::shared_ptr<Command> ngEnsureApplyAutoMeshCommand(compat::RetainedStringSlice id) {
    AutoMeshKey key{id};
    if(auto found=autoMeshApplyCommands.find(key);found!=autoMeshApplyCommands.end()) return found->second;
    return ensureForTypes(id, viewport::vertex::automesh::AutoMeshProcessorTypes{});
}
template<> void ngInitCommands<AutoMeshKey>() {
    const auto before=autoMeshApplyCommands.size();
    initializeForTypes(viewport::vertex::automesh::AutoMeshProcessorTypes{});
#ifdef CMD_LOG
    detail::cmdLogEnabled("[CMD] AutoMeshKey init: before=%s after=%s",
        before, autoMeshApplyCommands.size());
#else
    (void)before;
#endif
}
}
