#pragma once
#include <nijigenerate/commands/command.hpp>
#include <compat/source_mutex.hpp>
#include <compat/source_condition.hpp>

namespace nijigenerate::commands::automesh::dynamic::detail {
class AutoMeshApplyResult : public CommandResult {
    std::shared_ptr<nicxlive::core::d_runtime::SourceMutex> lock;
    std::shared_ptr<nicxlive::core::d_runtime::SourceCondition> condition;
    bool done = false;
    std::shared_ptr<CommandResult> finalResult;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshApplyResult();
    virtual void complete(std::shared_ptr<CommandResult> result);
    std::shared_ptr<CommandResult> waitForCompletion() override;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::automesh::dynamic::detail::AutoMeshApplyResult> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.automesh.dynamic.AutoMeshApplyResult)";
    inline static constexpr char unqualifiedArgument[] = "(AutoMeshApplyResult)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.automesh.dynamic.AutoMeshApplyResult";
    inline static constexpr char unqualifiedElement[] = "AutoMeshApplyResult";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::commands::automesh::dynamic::detail::AutoMeshApplyResult> {
    inline static constexpr char className[] =
        "nijigenerate.commands.automesh.dynamic.AutoMeshApplyResult";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.commands.automesh.dynamic.AutoMeshApplyResult)";
};
} // namespace nicxlive::core::d_runtime
