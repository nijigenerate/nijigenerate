#pragma once
#include <cstdint>
#include "framework.hpp"
#include <compat/object_metadata.hpp>
namespace nijigenerate::autorig::workflow {
enum class AutoRigWorkflowState : std::int32_t { Pending, Running, Succeeded, Failed, Canceled };
using namespace framework;
struct AutoRigWorkflowPreset { Text providerId; AutoRigWorkflowSpec spec; };
struct AutoRigWorkflowSnapshot {
    Text runId, providerId, workflowId;
    AutoRigWorkflowState state{};
    Text message;
    Table<AutoRigTaskSnapshot> steps;
};
class WorkflowProcessor;
class AutoRigWorkflowRun : public IAutoRigSessionEditContext,
    public std::enable_shared_from_this<AutoRigWorkflowRun> {
    std::shared_ptr<AutoRigSession> session_;
    std::shared_ptr<WorkflowProcessor> processor;
    Table<AutoRigValue> suppliedInputs;
    std::uint64_t inputRevision_{};
    compat::Slice<AutoRigWorkflowStep> orderedSteps_;
    void requireUIThread();
    friend class AutoRigWorkflowManager;
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    AutoRigWorkflowRun(std::shared_ptr<AutoRigSession>, std::shared_ptr<WorkflowProcessor>);
protected:
    virtual void prepareSourceClassInfo();
public:
    AutoRigWorkflowRun(const AutoRigWorkflowRun&) = delete;
    AutoRigWorkflowRun& operator=(const AutoRigWorkflowRun&) = delete;
    virtual Text toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AutoRigWorkflowRun>);
    virtual int opCmp(std::shared_ptr<AutoRigWorkflowRun>);
    virtual Text id();
    Text runId() override { return id(); }
    Text workflowId() override;
    virtual Text displayName();
    bool canEdit() override;
    compat::Slice<Text> contextNames() override;
    AutoRigValue contextValue(Text) override;
    compat::Slice<AutoRigPortSpec> outputPorts() override;
    bool hasOutput(Text) override;
    AutoRigArtifact outputArtifact(Text) override;
    virtual bool renderInputUI();
    virtual bool renderOutputUI();
    virtual Text directory();
    virtual std::shared_ptr<AutoRigSession> session() { return session_; }
    compat::Slice<AutoRigPortSpec> inputPorts() override;
    bool hasInput(Text) override;
    virtual std::uint64_t inputRevision() { return inputRevision_; }
    AutoRigValue input(Text) override;
    void setContextValue(Text, AutoRigValue) override;
    virtual void setContext(Json);
    virtual Text stepTaskId(Text);
    virtual compat::Slice<AutoRigWorkflowStep> orderedSteps();
    void setInput(Text, AutoRigValue) override;
    virtual void execute(bool force = false);
    virtual void executeStep(Text, bool force = false);
    virtual void cancel();
    AutoRigValue output(Text) override;
    virtual AutoRigWorkflowSnapshot snapshot(bool includeArtifacts = true);
};
Json ngAutoRigWorkflowDiagnostics(std::shared_ptr<AutoRigWorkflowRun>);
class AutoRigWorkflowManager : public std::enable_shared_from_this<AutoRigWorkflowManager> {
    std::shared_ptr<AutoRigSessionManager> sessions;
    Table<std::shared_ptr<AutoRigWorkflowRun>> runs, closedRuns;
protected:
    virtual void prepareSourceClassInfo();
public:
    explicit AutoRigWorkflowManager(std::shared_ptr<AutoRigSessionManager> value);
    virtual ~AutoRigWorkflowManager() = default;
    AutoRigWorkflowManager(const AutoRigWorkflowManager&) = delete;
    AutoRigWorkflowManager& operator=(const AutoRigWorkflowManager&) = delete;
    virtual Text toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<AutoRigWorkflowManager>);
    virtual int opCmp(std::shared_ptr<AutoRigWorkflowManager>);
    virtual compat::Slice<AutoRigWorkflowPreset> listPresets();
    virtual std::shared_ptr<AutoRigWorkflowRun> create(Text, Text);
    virtual std::shared_ptr<AutoRigWorkflowRun> createConfigured(Text, Text, Table<AutoRigValue>, Json);
    virtual std::shared_ptr<AutoRigWorkflowRun> get(Text);
    virtual std::shared_ptr<AutoRigWorkflowRun> reopen(Text);
    virtual void close(Text);
    virtual void remove(Text);
    virtual void disposeAll();
};
}

namespace nicxlive::core::d_runtime {
#define NG_WORKFLOW_TYPE_ORIGINS(Type, Reference) \
template<> struct SourceTypeSpellings<::nijigenerate::autorig::workflow::Type> { \
    inline static constexpr bool isNativeObject = Reference; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.workflow." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.workflow." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
};
NG_WORKFLOW_TYPE_ORIGINS(AutoRigWorkflowState, false)
NG_WORKFLOW_TYPE_ORIGINS(AutoRigWorkflowPreset, false)
NG_WORKFLOW_TYPE_ORIGINS(AutoRigWorkflowSnapshot, false)
NG_WORKFLOW_TYPE_ORIGINS(AutoRigWorkflowRun, true)
NG_WORKFLOW_TYPE_ORIGINS(AutoRigWorkflowManager, true)
#undef NG_WORKFLOW_TYPE_ORIGINS

#define NG_WORKFLOW_OBJECT_NAMES(Type) \
template<> struct SourceObjectTypeNames<::nijigenerate::autorig::workflow::Type> { \
    inline static constexpr char className[] = "nijigenerate.autorig.workflow." #Type; \
    inline static constexpr char templateArgumentName[] = "(nijigenerate.autorig.workflow." #Type ")"; \
};
NG_WORKFLOW_OBJECT_NAMES(AutoRigWorkflowRun)
NG_WORKFLOW_OBJECT_NAMES(AutoRigWorkflowManager)
#undef NG_WORKFLOW_OBJECT_NAMES
}
