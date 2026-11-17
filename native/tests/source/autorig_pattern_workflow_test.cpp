#include <nijigenerate/autorig/patterns/core.hpp>
#include <nijigenerate/autorig/patterns/processor.hpp>
#include <nijigenerate/autorig/detail/workflow_processor.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <iostream>
#include <stdexcept>

namespace {
namespace f = nijigenerate::autorig::framework;

class ConditionProcessor final : public f::AutoRigProcessor {
public:
    std::vector<std::uint32_t> accepted;
    f::Text procId() override { return "condition-regression"; }
    f::Text displayName() override { return "Condition regression"; }
    nijigenerate::compat::Slice<f::AutoRigTaskSpec> tasks() override {
        const auto text = [](const char* value) { return f::Text::fromStaticView(value); };
        const f::AutoRigPortSpec associations{text("target-associations"), f::AutoRigValueKind::Object};
        const f::AutoRigPortSpec state{text("state"), f::AutoRigValueKind::Json};
        f::AutoRigTaskSpec apply{text("apply"), text("Apply"), {}, {state}, {},
            {{text("state"), text("collect"), text("state")}}};
        apply.targetTags = {text("face/eye/left")};
        apply.tagCondition = text("left-eye");
        apply.supportsTargetScope = true;
        return {{text("collect"), text("Collect"), {}, {}, {state, associations}, {}}, apply};
    }
    void executeTask(f::Text task, std::shared_ptr<f::AutoRigTaskContext> context) override {
        if (task == "collect") {
            nijigenerate::core::settings::SettingsJsonObject state;
            state["rootId"] = std::uint64_t(42);
            context->publishJson("state", state);
            context->publishEvidence("target-associations", f::AutoRigTargetAssociations{"42", {
                {"face/eye/left", "left", 10}, {"face/eye/right", "right", 11}}});
            return;
        }
        accepted.clear();
        for (const auto id : {10u, 11u, 12u}) if (context->acceptsTarget(id)) accepted.push_back(id);
    }
};

class EvidenceProcessor final : public f::AutoRigProcessor {
public:
    bool failDetection{};
    int detectedValue = 10;
    int appliedValue{};
    int independentRuns{};
    std::vector<std::uint32_t> scopedNodes;
    std::weak_ptr<f::AutoRigSession> session;
    f::AutoRigExecutionProgress observedProgress;
    f::Text procId() override { return "evidence-regression"; }
    f::Text displayName() override { return "Evidence regression"; }
    nijigenerate::compat::Slice<f::AutoRigTaskSpec> tasks() override {
        const auto str = [](const char* value) { return nijigenerate::compat::RetainedString::fromStaticView(value); };
        const f::AutoRigPortSpec evidence{str("evidence"), f::AutoRigValueKind::Object};
        const f::AutoRigPortSpec checkpoint{str("checkpoint"), f::AutoRigValueKind::Object};
        f::AutoRigTaskSpec apply{str("apply"), str("Apply"), {}, {evidence, checkpoint}, {checkpoint},
            {{str("evidence"), str("detect"), str("evidence")}, {str("checkpoint"), str("source"), str("checkpoint")}}};
        apply.supportsTargetScope = true;
        apply.checkpointFallbacks = {str("checkpoint")};
        return {{str("source"), str("Source"), {}, {}, {checkpoint}, {}},
            {str("detect"), str("Detect"), {}, {}, {evidence}, {}}, apply,
            {str("independent"), str("Independent"), {}, {checkpoint}, {},
                {{str("checkpoint"), str("apply"), str("checkpoint")}}}};
    }
    void executeTask(f::Text id, std::shared_ptr<f::AutoRigTaskContext> context) override {
        if (const auto run = session.lock()) observedProgress = run->executionProgress();
        if (id == "source") context->publishEvidence("checkpoint", 5);
        else if (id == "detect") {
            if (failDetection) throw std::runtime_error("No detector result");
            context->publishEvidence("evidence", detectedValue);
        } else if (id == "apply") {
            appliedValue = *context->inputEvidence<int>("evidence");
            if (const auto scope = context->targetScope()) {
                for (const auto& item : scope->targets) scopedNodes.push_back(item.node);
                context->recordUnit({{10}, f::AutoRigTaskState::Succeeded, {}});
                context->recordUnit({{11}, f::AutoRigTaskState::Blocked, "missing test evidence"});
            }
            context->publishEvidence("checkpoint", appliedValue);
        } else if (id == "independent") {
            const int expected = appliedValue == 0 ? 5 : appliedValue;
            if (*context->inputEvidence<int>("checkpoint") != expected)
                throw std::runtime_error("Unexpected checkpoint");
            ++independentRuns;
        }
    }
};

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void checkTagConditions() {
    auto processor = std::make_shared<ConditionProcessor>();
    auto manager = std::make_shared<f::AutoRigSessionManager>("/tmp/autorig-tag-conditions");
    manager->registerProcessor(processor);
    auto run = manager->create(processor->procId());
    const auto manual = run->registerTargetAssociations({"42", {{"face/eye/left", "left", 11}}});
    run->execute("apply");
    expect(processor->accepted == std::vector<std::uint32_t>{10}, "Full execution ignored the tag condition");
    expect(run->evaluateTagCondition("left-eye", "42", 11).status == f::AutoRigTagMatch::Mismatch,
        "A conflicting manual tag was recognized");
    expect(run->evaluateTagCondition("left-eye", "42", 12).status == f::AutoRigTagMatch::MissingEvidence,
        "A missing association was recognized");
    const auto matched = run->evaluateTagCondition("left-eye", "42", 10);
    expect(matched.status == f::AutoRigTagMatch::Matched && matched.version == 1 &&
        matched.evidenceIds.size() == 1 && matched.evidenceIds[0] != manual,
        "Recognition lost its validated producer");
    run->setTargetRestriction("apply", "42", std::vector<std::uint32_t>{});
    const auto restriction = run->targetRestriction("apply");
    run->execute("apply");
    expect(processor->accepted.empty(), "An explicit empty selection reverted to all targets");
    expect(run->task("apply", false).inputEvidenceIds.at("target-settings") == restriction,
        "Execution did not pin the target setting as evidence");
    run->setTargetRestriction("apply", "42", std::nullopt);
    run->execute("apply");
    expect(processor->accepted == std::vector<std::uint32_t>{10},
        "Clearing target settings did not restore the detected target");
    manager->registerTagCondition({"left-eye", 2, {"face/eye/right"}});
    auto changed = manager->create(processor->procId());
    changed->execute("apply");
    expect(processor->accepted == std::vector<std::uint32_t>{11}, "Global condition did not reach a new workflow");
}

void checkEvidenceExecution() {
    const auto processor = std::make_shared<EvidenceProcessor>();
    const auto session = std::make_shared<f::AutoRigSession>(processor, "evidence-regression",
        "/tmp/evidence-regression", f::AutoRigTaskActionBoundary{}, f::AutoRigEditorDispatcher{});
    processor->session = session;
    using State = f::AutoRigTaskState;
    processor->failDetection = true;
    session->executeGroup({"apply", "independent"});
    expect(session->task("detect").state == State::Failed, "Detector failure was lost");
    expect(session->task("apply").state == State::Blocked, "Apply ran without required evidence");
    expect(session->task("independent").state == State::Succeeded, "Independent task did not use the fallback");
    expect(session->task("detect").attempt == 1, "Detector ran twice in one group");
    expect(processor->observedProgress.currentTask == "independent" &&
        processor->observedProgress.resolvedTasks == 3, "Live progress omitted a failed or blocked task");
    expect(session->executionProgress().currentTask.empty(), "Completed execution still shows a running task");

    processor->failDetection = false;
    session->executeGroup({"apply", "independent"});
    const auto first = session->outputEvidence("detect", "evidence");
    expect(!first.empty() && processor->appliedValue == 10, "New evidence was not applied");
    const auto originalObject = session->readEvidence(first).object<int>();

    processor->detectedValue = 20;
    session->execute("detect", true);
    expect(session->outputEvidence("detect", "evidence", 2) == first, "A prior attempt ID was lost");
    processor->failDetection = true;
    session->execute("detect", true);
    session->bindInputEvidence("apply", "evidence", first);
    session->executeGroup({"apply", "independent"});
    expect(session->task("detect").state == State::Failed, "Pinned input reran its producer");
    expect(session->task("apply").state == State::Succeeded && processor->appliedValue == 10,
        "Historical evidence was not applied");
    expect(session->readEvidence(first).object<int>() == originalObject, "Evidence object was copied");
    expect(session->task("apply").inputEvidenceIds.at("evidence") == first, "Attempt lost input provenance");
    const auto edited = session->registerInputEvidence("apply", "evidence", f::AutoRigValue::objectValue(30));
    session->bindInputEvidence("apply", "evidence", edited);
    session->executeGroup({"apply", "independent"});
    expect(processor->appliedValue == 30 && *session->readEvidence(first).object<int>() == 10,
        "Replacing selected evidence mutated history");

    const auto selected = session->registerTargetAssociations({"model", {
        {"face/eye/left", "left", 10}, {"face/eye/left", "left", 11}, {"face/eye/right", "right", 12}}});
    session->registerTargetAssociations({"other-model", {{"face/eye/left", "left", 13}}}, "other");
    const auto fullOutput = session->outputEvidence("apply", "checkpoint");
    const auto scoped = session->plan({"apply", "independent"}, {{"face/eye/left"}, {}, false}, "model");
    const auto scopedResult = session->execute(scoped);
    expect(processor->observedProgress.currentTask == "apply" &&
        processor->observedProgress.resolvedTasks == 0, "Scoped execution did not publish fresh live progress");
    expect(processor->scopedNodes == std::vector<std::uint32_t>({10, 11}), "Scope expanded or lost a target");
    expect(scopedResult.tasks.size() == 2 && scopedResult.tasks[0].state == State::PartiallySucceeded
        && scopedResult.tasks[1].state == State::Blocked, "Scoped results were not isolated");
    expect(session->outputEvidence("apply", "checkpoint") == fullOutput, "Scoped execution replaced full output");
    const auto info = session->evidenceInfo(scopedResult.tasks[0].artifacts[0].evidenceId);
    expect(info.scope && info.scope->evidenceIds.size() == 1 && info.scope->evidenceIds[0] == selected,
        "Scoped output omitted its association provenance");
    const auto none = session->plan({"apply"}, {{"face/mouth"}, {}, false}, "model");
    expect(session->execute(none).tasks[0].state == State::Blocked, "Empty scope ran global apply");

}
}
int main() {
    namespace f = nijigenerate::autorig::framework;
    namespace p = nijigenerate::autorig::patterns;
    namespace w = nijigenerate::autorig::workflow;
    try {
        checkEvidenceExecution();
        checkTagConditions();
        auto manager = std::make_shared<f::AutoRigSessionManager>(f::Text("/tmp/autorig-pattern-workflow-test"));
        for (const auto& spec : p::ngPatternSpecs()) {
            auto processor = std::make_shared<p::StructuralPatternProcessor>(spec.id);
            manager->registerProcessor(processor);
            auto workflows = processor->workflows();
            if (workflows.size() != 1) throw std::runtime_error("Expected one workflow");
            auto workflow = std::make_shared<w::WorkflowProcessor>(manager, processor->procId(), workflows[0]);
            if (workflow->tasks().size() != 3) throw std::runtime_error("Dependencies duplicated in workflow");
            for (const auto& task : workflow->tasks()) {
                if (task.id.empty()) throw std::runtime_error("Missing task id");
                if (task.outputs.empty()) throw std::runtime_error("Missing task outputs");
            }
        }
        std::cout << "All 35 processor registrations and workflow input/connection contracts passed\n";
    } catch (const nijigenerate::compat::ThrowableException& error) {
        std::cerr << error.msg.view() << '\n'; return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
