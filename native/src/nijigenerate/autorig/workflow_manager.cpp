#include "detail/workflow_processor.hpp"
#include "detail/workflow_source.hpp"
#include <exception>
namespace nijigenerate::autorig::workflow {
using detail::require;
using framework::detail::str;
compat::Slice<AutoRigWorkflowPreset> AutoRigWorkflowManager::listPresets() {
    auto self = shared_from_this(); compat::Slice<AutoRigWorkflowPreset> result;
    for (auto processor : compat::ngRequireObject(sessions)->listProcessors())
        for (const auto workflow : compat::ngRequireObject(processor)->workflows()) result.append({compat::ngRequireObject(processor)->procId(), workflow});
    return result;
}
std::shared_ptr<AutoRigWorkflowRun> AutoRigWorkflowManager::create(Text providerId, Text workflowId) {
    auto self = shared_from_this(); AutoRigWorkflowSpec spec; bool found = false;
    for (const auto workflow : compat::ngRequireObject(compat::ngRequireObject(sessions)->processor(providerId))->workflows()) if (workflow.id.view() == workflowId.view()) {
        require(!found, "Duplicate AutoRig workflow preset ID",435); spec = workflow; found = true;
    }
    require(found, "Unknown AutoRig workflow preset",439);
    auto adapter = std::allocate_shared<WorkflowProcessor>(SourceAllocator<WorkflowProcessor>{}, sessions, providerId, spec);
    auto session = compat::ngRequireObject(sessions)->createWithProcessor(adapter, compat::ngRequireObject(sessions)->rootDirectory());
    try {
        auto run = std::allocate_shared<AutoRigWorkflowRun>(SourceAllocator<AutoRigWorkflowRun>{}, session, adapter);
        for (const auto& [id, value] : spec.inputDefaults) compat::ngRequireObject(run)->setInput(id, value);
        runs[compat::ngRequireObject(run)->id()] = run; return run;
    } catch (...) {
        detail::failureCleanup([&] { compat::ngRequireObject(sessions)->remove(compat::ngRequireObject(session)->id()); });
    }
}
std::shared_ptr<AutoRigWorkflowRun> AutoRigWorkflowManager::createConfigured(Text providerId, Text workflowId,
    Table<AutoRigValue> inputs, Json context) {
    auto self = shared_from_this(); auto run = create(providerId, workflowId);
    try {
        for (const auto& [id, value] : inputs) compat::ngRequireObject(run)->setInput(id, value);
        compat::ngRequireObject(run)->setContext(context); return run;
    } catch (...) {
        detail::failureCleanup([&] { remove(compat::ngRequireObject(run)->id()); });
    }
}
std::shared_ptr<AutoRigWorkflowRun> AutoRigWorkflowManager::get(Text id) {
    const auto found = runs.find(id); require(found != runs.end(), "Unknown AutoRig workflow run",460); return found->second;
}
std::shared_ptr<AutoRigWorkflowRun> AutoRigWorkflowManager::reopen(Text id) {
    auto self = shared_from_this(); if (auto known = runs.find(id); known != runs.end()) return known->second;
    auto saved = closedRuns.find(id); require(saved != closedRuns.end(), "AutoRig session is no longer available in memory",467);
    auto run = saved->second; compat::ngRequireObject(sessions)->reopenWithProcessor(compat::ngRequireObject(run)->processor, id); closedRuns.erase(id); runs[id] = run; return run;
}
void AutoRigWorkflowManager::close(Text id) {
    auto self = shared_from_this(); closedRuns[id] = get(id); compat::ngRequireObject(sessions)->close(id); runs.erase(id);
}
void AutoRigWorkflowManager::remove(Text id) {
    auto self = shared_from_this(); require(runs.contains(id) || closedRuns.contains(id), "Unknown AutoRig workflow run",482);
    compat::ngRequireObject(sessions)->remove(id); runs.erase(id); closedRuns.erase(id);
}
void AutoRigWorkflowManager::disposeAll() {
    auto self = shared_from_this();
    for (const auto& [id, run] : runs) require(!compat::ngRequireObject(compat::ngRequireObject(run)->session())->isBusy(), "Cannot dispose a running AutoRig workflow",489);
    for (const auto& [id, run] : closedRuns) require(!compat::ngRequireObject(compat::ngRequireObject(run)->session())->isBusy(), "Cannot dispose a running AutoRig workflow",490);
    for (const auto& id : framework::detail::keys(runs)) remove(id);
    for (const auto& id : framework::detail::keys(closedRuns)) remove(id);
}
}
