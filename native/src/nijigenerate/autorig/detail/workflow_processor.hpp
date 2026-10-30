#pragma once
#include "../workflow.hpp"
#include "framework_runtime.hpp"
namespace nijigenerate::autorig::workflow {
namespace detail {
AutoRigPortSpec port(compat::Slice<AutoRigPortSpec>, Text);
Text instanceId(Text, Text);
}
class WorkflowProcessor : public AutoRigProcessor {
protected:
    void prepareSourceClassInfo() override;
public:
    Text providerId;
    std::shared_ptr<AutoRigProcessor> provider;
    AutoRigWorkflowSpec workflow;
    compat::Slice<AutoRigTaskSpec> flattened;
    Table<std::shared_ptr<AutoRigProcessor>> owners;
    Table<Text> originalTaskIds, selectedTaskIds;
    Table<Table<Text>> includedTasks;
    WorkflowProcessor(std::shared_ptr<AutoRigSessionManager>, Text, AutoRigWorkflowSpec);
    WorkflowProcessor(const WorkflowProcessor&) = delete;
    WorkflowProcessor& operator=(const WorkflowProcessor&) = delete;
    virtual void includeTask(Text, Text, std::shared_ptr<AutoRigProcessor>, Table<AutoRigTaskSpec>, Table<bool>&);
    virtual Text targetId(Text, Text);
    virtual AutoRigTaskSpec taskSpec(Text);
    virtual void replaceTask(AutoRigTaskSpec);
    virtual void updateDependency(Text, Text);
    Text procId() override;
    Text displayName() override { return workflow.label; }
    compat::Slice<AutoRigTaskSpec> tasks() override { return flattened; }
    void executeTask(Text, std::shared_ptr<AutoRigTaskContext>) override;
    bool renderInputUI(Text, std::shared_ptr<IAutoRigInputContext>) override;
    bool renderOutputUI(Text, std::shared_ptr<IAutoRigOutputContext>) override;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::workflow::WorkflowProcessor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.workflow.WorkflowProcessor)";
    inline static constexpr char unqualifiedArgument[] = "(WorkflowProcessor)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.workflow.WorkflowProcessor";
    inline static constexpr char unqualifiedElement[] = "WorkflowProcessor";
};
template<> struct SourceObjectTypeNames<::nijigenerate::autorig::workflow::WorkflowProcessor> {
    inline static constexpr char className[] = "nijigenerate.autorig.workflow.WorkflowProcessor";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.autorig.workflow.WorkflowProcessor)";
};
}
