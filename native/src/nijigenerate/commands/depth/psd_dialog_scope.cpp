#include "psd_dialog_scope.hpp"
#include <nijigenerate/commands/all_maps.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>

namespace nijigenerate::commands::depth::psd_dialog {

void PsdDepthDialogCommandScope::prepareSourceClassInfo() const {
    CommandScope::prepareSourceClassInfo();
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<PsdDepthDialogCommandScope>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        const auto argument = compat::RetainedString::fromStaticLiteral(Names::templateArgumentName);
        try {
            ngRegisterObjectTypeNames<PsdDepthDialogCommandScope>(name, argument);
            ngRegisterObjectClassName(typeid(PsdDepthDialogCommandScope), name);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return true;
    }();
    (void)registered;
}

compat::RetainedString PsdDepthDialogCommandScope::id() const { return compat::RetainedString::fromStaticLiteral("psd-depth-dialog"); }
compat::RetainedString PsdDepthDialogCommandScope::description() const { return compat::RetainedString::fromStaticLiteral("PSD depth import dialog"); }
compat::Slice<CommandScopeTransition> PsdDepthDialogCommandScope::completionCommands() const {
    compat::Slice<CommandScopeTransition> result;
    result.resize(1);
    result[0] = {compat::RetainedString(ngCommandIdFromKey(
        ngSourceCommandIdValue<detail::applyPsdDepthDialogIdOrigin>())),
        compat::RetainedString::fromStaticLiteral("Apply the PSD depth import and close the dialog")};
    return result;
}
compat::Slice<CommandScopeTransition> PsdDepthDialogCommandScope::cancellationCommands() const {
    compat::Slice<CommandScopeTransition> result;
    result.resize(1);
    result[0] = {compat::RetainedString(ngCommandIdFromKey(
        ngSourceCommandIdValue<detail::cancelPsdDepthDialogIdOrigin>())),
        compat::RetainedString::fromStaticLiteral("Discard dialog changes and close the dialog")};
    return result;
}

compat::RetainedString PsdDepthDialogCommandScope::typeName() const {
    return compat::RetainedString::fromStaticView(CommandScopeTypeName<PsdDepthDialogCommandScope>::value);
}

} // namespace nijigenerate::commands::depth::psd_dialog
