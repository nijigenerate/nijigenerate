#include "detail/workflow_processor.hpp"
#include "detail/workflow_source.hpp"
#include <nijigenerate/core/thread_context.hpp>
#include <algorithm>

namespace nijigenerate::autorig::workflow {
using framework::detail::str;
using detail::require;
namespace {
AutoRigValue restoredValue(Json stored) {
    AutoRigValue value;
    const auto kind=detail::jsonString(detail::jsonIndex(stored,"kind"));
    value.kind=detail::storedKind(kind.view());
    switch (value.kind) {
    case AutoRigValueKind::Json: value.json = detail::jsonIndex(stored,"value"); break;
    case AutoRigValueKind::Blob:
        for (const auto entry : detail::jsonArray(detail::jsonIndex(stored,"value"))) {
            const auto byte=detail::jsonInteger(entry);
            value.bytes().append(detail::storedByte(byte));
        }
        break;
    case AutoRigValueKind::FileName: case AutoRigValueKind::Path:
        value.text=detail::jsonString(detail::jsonIndex(stored,"value"));break;
    }
    return value;
}
}
AutoRigWorkflowRun::AutoRigWorkflowRun(std::shared_ptr<AutoRigSession> session, std::shared_ptr<WorkflowProcessor> value)
    : session_(std::move(session)), processor(std::move(value)) {
    prepareSourceClassInfo();
    orderedSteps();
}
Text AutoRigWorkflowRun::id() { return compat::ngRequireObject(session_)->id(); }
Text AutoRigWorkflowRun::workflowId() { return compat::ngRequireObject(processor)->workflow.id; }
Text AutoRigWorkflowRun::displayName() { return compat::ngRequireObject(processor)->workflow.label.empty() ? workflowId() : compat::ngRequireObject(processor)->workflow.label; }
bool AutoRigWorkflowRun::canEdit() { return !compat::ngRequireObject(session_)->isBusy(); }
compat::Slice<Text> AutoRigWorkflowRun::contextNames() { return compat::ngRequireObject(session_)->contextNames(); }
AutoRigValue AutoRigWorkflowRun::contextValue(Text name) { return compat::ngRequireObject(session_)->contextValue(name); }
compat::Slice<AutoRigPortSpec> AutoRigWorkflowRun::outputPorts() { return compat::ngRequireObject(processor)->workflow.outputs.duplicate(); }
bool AutoRigWorkflowRun::hasOutput(Text id) {
    for (const auto binding : compat::ngRequireObject(processor)->workflow.outputBindings) if (binding.workflowOutput.view() == id.view())
        return compat::ngRequireObject(compat::ngRequireObject(session_)->outputContext(stepTaskId(str(binding.sourceStep))))->hasValue(str(binding.sourcePort));
    return false;
}
AutoRigArtifact AutoRigWorkflowRun::outputArtifact(Text id) {
    for (const auto binding : compat::ngRequireObject(processor)->workflow.outputBindings) if (binding.workflowOutput.view() == id.view())
        return compat::ngRequireObject(session_)->outputArtifact(stepTaskId(str(binding.sourceStep)), str(binding.sourcePort));
    detail::fail(Text::fromStaticLiteral("Unknown workflow output"),281);
}
void AutoRigWorkflowRun::requireUIThread() {
    require(core::thread_context::ngHasManagedThread() && core::thread_context::ngIsMainThread(),
        "AutoRig session UI must run on the main thread",295);
}
bool AutoRigWorkflowRun::renderInputUI() {
    requireUIThread(); auto self = shared_from_this();
    return compat::ngRequireObject(compat::ngRequireObject(processor)->provider)->renderSessionInputUI(workflowId(), self);
}
bool AutoRigWorkflowRun::renderOutputUI() {
    requireUIThread(); auto self = shared_from_this();
    return compat::ngRequireObject(compat::ngRequireObject(processor)->provider)->renderSessionOutputUI(workflowId(), self);
}
Text AutoRigWorkflowRun::directory() { return compat::ngRequireObject(session_)->directory(); }
compat::Slice<AutoRigPortSpec> AutoRigWorkflowRun::inputPorts() { return compat::ngRequireObject(processor)->workflow.inputs.duplicate(); }
bool AutoRigWorkflowRun::hasInput(Text id) { return suppliedInputs.contains(id); }
AutoRigValue AutoRigWorkflowRun::input(Text id) {
    const auto value = suppliedInputs.find(id); require(value != suppliedInputs.end(), [&]{return "Missing AutoRig workflow input: " + id;},307);
    return ngCopyAutoRigValue(value->second);
}
void AutoRigWorkflowRun::setContextValue(Text name, AutoRigValue value) { compat::ngRequireObject(session_)->setContextValue(name, value); }
void AutoRigWorkflowRun::setContext(Json values) {
    const auto receiver = weak_from_this().lock();
    detail::require(values.is_object(),"AutoRig session context must be an object",315);
    for (const auto& member : detail::jsonObjectRef(values))
        setContextValue(member.name.retainedString(),restoredValue(member.item));
}
Text AutoRigWorkflowRun::stepTaskId(Text id) {
    const auto found = compat::ngRequireObject(processor)->selectedTaskIds.find(id); require(found != compat::ngRequireObject(processor)->selectedTaskIds.end(), "Unknown workflow step",320);
    return found->second;
}
compat::Slice<AutoRigWorkflowStep> AutoRigWorkflowRun::orderedSteps() {
    // The same body also runs from this class's actual constructor.
    const auto receiver = weak_from_this().lock();
    if (!orderedSteps_.empty()) return orderedSteps_.duplicate();
    Table<std::size_t> selectedIndices;
    for (std::size_t i = 0; i < compat::ngRequireObject(processor)->workflow.steps.size(); ++i)
        selectedIndices[stepTaskId(str(compat::ngRequireObject(processor)->workflow.steps[i].id))] = i;
    Table<bool> visiting, visited; compat::Slice<AutoRigWorkflowStep> result;
    compat::SharedDelegate<void(Text)> visit = [&](Text id) {
        if (visited.contains(id)) return;
        require(!visiting.contains(id), "AutoRig workflow dependency cycle",334); visiting[id] = true;
        const auto task = compat::ngRequireObject(processor)->taskSpec(id);
        for (const auto dep : task.dependencies) visit(str(dep));
        for (const auto con : task.connections) visit(str(con.sourceTask));
        visiting.erase(id); visited[id] = true;
        if (auto index = selectedIndices.find(id); index != selectedIndices.end())
            result.append(detail::arrayAt(compat::ngRequireObject(processor)->workflow.steps, index->second, 342));
    };
    for (const auto step : compat::ngRequireObject(processor)->workflow.steps) visit(stepTaskId(str(step.id)));
    orderedSteps_ = result; return result.duplicate();
}
void AutoRigWorkflowRun::setInput(Text id, AutoRigValue value) {
    const auto receiver = weak_from_this().lock();
    require(!compat::ngRequireObject(session_)->isBusy(), "Workflow is running",350); const auto spec = detail::port(compat::ngRequireObject(processor)->workflow.inputs, id);
    require(spec.kind == value.kind, "AutoRig workflow input kind mismatch",352);
    for (const auto binding : compat::ngRequireObject(processor)->workflow.inputBindings) if (binding.workflowInput.view() == id.view())
        compat::ngRequireObject(session_)->setInput(compat::ngRequireObject(processor)->targetId(str(binding.targetStep), str(binding.targetTask)), str(binding.targetPort), value);
    suppliedInputs[id] = ngCopyAutoRigValue(value); ++inputRevision_;
}
void AutoRigWorkflowRun::execute(bool force) {
    auto self = shared_from_this(); compat::Slice<Text> selected;
    for (const auto step : orderedSteps()) selected.append(stepTaskId(str(step.id))); compat::ngRequireObject(session_)->executeGroup(selected, force);
}
void AutoRigWorkflowRun::executeStep(Text id, bool force) { auto self = shared_from_this(); compat::ngRequireObject(session_)->execute(stepTaskId(id), force); }
void AutoRigWorkflowRun::cancel() { compat::ngRequireObject(session_)->cancel(); }
AutoRigValue AutoRigWorkflowRun::output(Text id) {
    for (const auto binding : compat::ngRequireObject(processor)->workflow.outputBindings) if (binding.workflowOutput.view() == id.view())
        return compat::ngRequireObject(session_)->output(stepTaskId(str(binding.sourceStep)), str(binding.sourcePort));
    detail::fail(Text::fromStaticLiteral("Unknown workflow output"),377);
}
AutoRigWorkflowSnapshot AutoRigWorkflowRun::snapshot(bool includeArtifacts) {
    const auto receiver = weak_from_this().lock();
    AutoRigWorkflowSnapshot result; result.runId = id(); result.providerId = compat::ngRequireObject(processor)->providerId; result.workflowId = compat::ngRequireObject(processor)->workflow.id;
    const auto running = compat::ngRequireObject(session_)->isBusy(); result.state = running ? AutoRigWorkflowState::Running : AutoRigWorkflowState::Pending;
    bool allSucceeded = true;
    for (const auto step : compat::ngRequireObject(processor)->workflow.steps) {
        const auto id = str(step.id); result.steps[id] = compat::ngRequireObject(session_)->task(stepTaskId(id), includeArtifacts);
        if (detail::aaAt(result.steps, id, 390).state != AutoRigTaskState::Succeeded) allSucceeded = false;
    }
    for (const auto spec : compat::ngRequireObject(processor)->flattened) {
        const auto task = compat::ngRequireObject(session_)->task(str(spec.id), false);
        if (task.state == AutoRigTaskState::Failed) {
            allSucceeded = false; if (!running) result.state = AutoRigWorkflowState::Failed; result.message = task.message;
        } else if (task.state == AutoRigTaskState::Canceled) {
            allSucceeded = false; if (!running) result.state = AutoRigWorkflowState::Canceled; result.message = task.message;
        }
    }
    if (!running) {
        if (compat::ngRequireObject(session_)->isCanceled()) result.state = AutoRigWorkflowState::Canceled;
        else if (allSucceeded) result.state = AutoRigWorkflowState::Succeeded;
    }
    return result;
}
Json ngAutoRigWorkflowDiagnostics(std::shared_ptr<AutoRigWorkflowRun> run) {
    core::settings::SettingsJsonObject result;
    for (const auto step : compat::ngRequireObject(run)->orderedSteps()) {
        const auto id = compat::ngRequireObject(run)->stepTaskId(str(step.id));
        if (step.id.view() == "weld" && !compat::ngRequireObject(compat::ngRequireObject(run)->session())->isBusy())
            for (const auto artifact : compat::ngRequireObject(compat::ngRequireObject(run)->session())->task(id).artifacts)
                if (artifact.preview && artifact.portId == "welding-correspondence-mismatch")
                    result["welding_correspondence_mismatch"] = artifact.value().json;
        if (step.id.view() == "compile") {
            core::settings::SettingsJsonArray profile;
            for (const auto artifact : compat::ngRequireObject(compat::ngRequireObject(run)->session())->task(id).artifacts)
                if (artifact.preview && artifact.portId == "compile-profile") profile.append(artifact.value().json);
            if (!profile.empty()) result["compile_profile"] = profile;
        }
        if (step.id.view() != "verify" || compat::ngRequireObject(compat::ngRequireObject(run)->session())->isBusy()) continue;
        bool reportPort = false;
        for (const auto output : compat::ngRequireObject(compat::ngRequireObject(run)->session())->taskSpec(id).outputs)
            if (output.id.view() == "report" && output.kind == AutoRigValueKind::Json) reportPort = true;
        if (!reportPort || !compat::ngRequireObject(compat::ngRequireObject(run)->session())->hasOutput(id, "report")) continue;
        const auto report = compat::ngRequireObject(compat::ngRequireObject(run)->session())->output(id, "report").json;
        if (!report.is_object()) continue;
        const auto& reportObject=detail::jsonObjectRef(report);
        core::settings::SettingsJsonObject summary;
        for (const auto key : {"passed", "error", "readback_verified", "finish_stages", "all_finish_stages_succeeded", "numerical_stages_passed", "rig_complete"})
            if (const auto entry = reportObject.if_contains(key)) summary[key] = *entry;
        if (const auto findings = reportObject.if_contains("numerical_findings"); findings && findings->is_array()) {
            auto entries = findings->as_array(); summary["numerical_findings_count"] = static_cast<std::uint64_t>(entries.size());
            summary["numerical_findings"] = entries.slice(0, std::min<std::size_t>(entries.size(), 32)).duplicate();
        }
        result["verification"] = summary;
    }
    return result;
}
}
