#pragma once

#include <nijigenerate/commands/base.hpp>
#include <nijigenerate/commands/source_diagnostic_text.hpp>

namespace nijigenerate::commands::depth::psd_dialog {

class PsdDepthDialogCommandScope final : public CommandScope {
public:
    PsdDepthDialogCommandScope() = default;
    PsdDepthDialogCommandScope(const PsdDepthDialogCommandScope&) = delete;
    PsdDepthDialogCommandScope& operator=(const PsdDepthDialogCommandScope&) = delete;
    compat::RetainedString id() const override;
    compat::RetainedString description() const override;
    compat::Slice<CommandScopeTransition> completionCommands() const override;
    compat::Slice<CommandScopeTransition> cancellationCommands() const override;
    compat::RetainedString typeName() const override;
protected:
    void prepareSourceClassInfo() const override;
};

} // namespace nijigenerate::commands::depth::psd_dialog

namespace nijigenerate::commands {

template<> struct CommandScopeTypeName<depth::psd_dialog::PsdDepthDialogCommandScope> {
    static constexpr std::string_view value =
        "nijigenerate.commands.depth.psd_dialog.PsdDepthDialogCommandScope";
};

} // namespace nijigenerate::commands

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<
        ::nijigenerate::commands::depth::psd_dialog::PsdDepthDialogCommandScope> {
    inline static constexpr char className[] =
        "nijigenerate.commands.depth.psd_dialog.PsdDepthDialogCommandScope";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.commands.depth.psd_dialog.PsdDepthDialogCommandScope)";
};
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::depth::psd_dialog::PsdDepthDialogCommandScope> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.depth.psd_dialog.PsdDepthDialogCommandScope)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepthDialogCommandScope)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.depth.psd_dialog.PsdDepthDialogCommandScope";
    inline static constexpr char unqualifiedElement[] = "PsdDepthDialogCommandScope";
    NG_SOURCE_TYPE_DIAGNOSTIC_MEMBERS("PsdDepthDialogCommandScope")
};
} // namespace nicxlive::core::d_runtime
