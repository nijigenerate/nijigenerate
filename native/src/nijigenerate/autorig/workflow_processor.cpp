#include "detail/workflow_processor.hpp"
#include "detail/workflow_source.hpp"
namespace nijigenerate::autorig::workflow {
using framework::detail::str;
using detail::require;
namespace detail {
AutoRigPortSpec port(compat::Slice<AutoRigPortSpec> ports, Text id) {
    for (const auto candidate : ports) if (candidate.id.view() == id.view()) return candidate;
    fail("Unknown AutoRig workflow port: "+id,66);
}
}
WorkflowProcessor::WorkflowProcessor(std::shared_ptr<AutoRigSessionManager> manager, Text id, AutoRigWorkflowSpec spec)
    : workflow(std::move(spec)) {
    prepareSourceClassInfo();
    provider = compat::ngRequireObject(manager)->processor(id);
    providerId = compat::ngRequireObject(provider)->procId();
    require(!workflow.id.empty() && !workflow.steps.empty(), "Empty AutoRig workflow",87);
    Table<AutoRigWorkflowStep> steps;
    for (const auto step : workflow.steps) {
        const auto stepId = str(step.id), taskId = str(step.taskId);
        require(!stepId.empty() && !steps.contains(stepId), "Invalid or duplicate workflow step",90); steps[stepId] = step;
        auto owner = manager->processor(str(step.processorId)); Table<AutoRigTaskSpec> catalog;
        for (const auto task : compat::ngRequireObject(owner)->tasks()) {
            const auto id = str(task.id); require(!id.empty() && !catalog.contains(id), "Invalid processor task catalog",95); catalog[id] = task;
        }
        require(catalog.contains(taskId),[&]{return "Unknown workflow task: "+taskId;},98);
        Table<bool> visiting; includeTask(stepId, taskId, owner, catalog, visiting);
        selectedTaskIds[stepId] = detail::instanceId(stepId, taskId);
    }
    for (const auto step : workflow.steps) {
        const auto selected = detail::aaAt(selectedTaskIds, str(step.id), 104);
        for (const auto dependency : step.dependencies) {
            require(steps.contains(str(dependency)), "Unknown workflow step dependency",106);
            updateDependency(selected, detail::aaAt(selectedTaskIds, str(dependency), 107));
        }
    }
    for (const auto binding : workflow.connections) {
        require(steps.contains(str(binding.sourceStep)) && steps.contains(str(binding.targetStep)), "Unknown workflow connection step",111);
        const auto source = detail::aaAt(selectedTaskIds, str(binding.sourceStep), 113);
        const auto target = targetId(str(binding.targetStep), str(binding.targetTask));
        const auto sourcePort = detail::port(taskSpec(source).outputs, str(binding.sourcePort));
        const auto targetPort = detail::port(taskSpec(target).inputs, str(binding.targetPort));
        require(sourcePort.kind == targetPort.kind, "AutoRig workflow connection kind mismatch",117);
        auto task = taskSpec(target);
        task.connections.append(AutoRigConnection{binding.targetPort, compat::RetainedString(source), binding.sourcePort});
        replaceTask(task);
    }
    Table<bool> bound, boundPublicInputs;
    for (const auto binding : workflow.inputBindings) {
        const auto publicPort = detail::port(workflow.inputs, str(binding.workflowInput));
        const auto target = targetId(str(binding.targetStep), str(binding.targetTask));
        const auto targetPort = detail::port(taskSpec(target).inputs, str(binding.targetPort));
        require(publicPort.kind == targetPort.kind, "AutoRig workflow input kind mismatch",128);
        const auto key = target + ":" + str(binding.targetPort);
        require(!bound.contains(key), "Duplicate AutoRig workflow input binding",130);
        bound[key] = true; boundPublicInputs[str(binding.workflowInput)] = true;
    }
    for (const auto publicPort : workflow.inputs)
        require(!publicPort.required || boundPublicInputs.contains(str(publicPort.id)),
            [&]{return "Missing required workflow input binding: "+str(publicPort.id);},135);
    Table<bool> outputs;
    for (const auto binding : workflow.outputBindings) {
        const auto publicPort = detail::port(workflow.outputs, str(binding.workflowOutput));
        require(!outputs.contains(str(binding.workflowOutput)), "Duplicate workflow output binding",140); outputs[str(binding.workflowOutput)] = true;
        require(steps.contains(str(binding.sourceStep)), "Unknown workflow output step",142);
        const auto sourcePort = detail::port(taskSpec(detail::aaAt(selectedTaskIds, str(binding.sourceStep), 143)).outputs, str(binding.sourcePort));
        require(publicPort.kind == sourcePort.kind, "AutoRig workflow output kind mismatch",144);
    }
    for (const auto publicPort : workflow.outputs)
        require(!publicPort.required || outputs.contains(str(publicPort.id)), "Missing required workflow output binding",147);
    for (const auto task : flattened) for (const auto input : task.inputs) {
        bool connected = false;
        for (const auto connection : task.connections) if (connection.input == input.id) connected = true;
        const auto key = str(task.id) + ":" + str(input.id);
        require(!input.required || connected || bound.contains(key),[&]{return "Missing required workflow task input: "+key;},153);
        require(!connected || !bound.contains(key), "Workflow input is both bound and connected",155);
    }
}
void WorkflowProcessor::includeTask(Text step, Text id, std::shared_ptr<AutoRigProcessor> owner,
    Table<AutoRigTaskSpec> catalog, Table<bool>& visiting) {
    const auto flatId = detail::instanceId(step, id); if (owners.contains(flatId)) return;
    require(!visiting.contains(id), "AutoRig task dependency cycle",163);
    const auto found = catalog.find(id); require(found != catalog.end(), "Unknown internal AutoRig task dependency",165);
    visiting[id] = true;
    compat::ngRunWithScopeExit([&] {
        auto task = found->second; task.dependencies = task.dependencies.duplicate(); task.connections = task.connections.duplicate();
        for (const auto dependency : task.dependencies) includeTask(step, str(dependency), owner, catalog, visiting);
        for (const auto connection : task.connections) includeTask(step, str(connection.sourceTask), owner, catalog, visiting);
        for (auto& dependency : task.dependencies) dependency = detail::instanceId(step, str(dependency));
        for (auto& connection : task.connections) connection.sourceTask = detail::instanceId(step, str(connection.sourceTask));
        task.id = compat::RetainedString(flatId); flattened.append(task); owners[flatId] = owner;
        originalTaskIds[flatId] = id; includedTasks[step][id] = flatId;
    }, [&] { visiting.erase(id); });
}
Text WorkflowProcessor::targetId(Text step, Text id) {
    const auto tasks = includedTasks.find(step); require(tasks != includedTasks.end(), "Unknown workflow target step",186);
    if (id.empty()) return detail::aaAt(selectedTaskIds, step, 187);
    const auto target = tasks->second.find(id);
    require(target != tasks->second.end(), "Workflow target task is not a dependency of its step",189); return target->second;
}
AutoRigTaskSpec WorkflowProcessor::taskSpec(Text id) {
    for (const auto task : flattened) if (task.id.view() == id.view()) return task;
    detail::fail(Text::fromStaticLiteral("Unknown workflow task instance"),195);
}
void WorkflowProcessor::replaceTask(AutoRigTaskSpec task) {
    for (auto& current : flattened) if (current.id == task.id) { current = task; return; }
    detail::fail(Text::fromStaticLiteral("Unknown workflow task instance"),203);
}
void WorkflowProcessor::updateDependency(Text target, Text source) {
    auto task = taskSpec(target); task.dependencies.append(compat::RetainedString(source)); replaceTask(task);
}
Text WorkflowProcessor::procId() {
    return "workflow-"+providerId+"-"+workflow.id;
}
void WorkflowProcessor::executeTask(Text id, std::shared_ptr<AutoRigTaskContext> context) {
    auto self = shared_from_this(); const auto owner = owners.find(id);
    require(owner != owners.end(), "Unknown workflow task owner",217); auto retained = owner->second;
    compat::ngRequireObject(retained)->executeTask(detail::aaAt(originalTaskIds, id, 218), context);
}
bool WorkflowProcessor::renderInputUI(Text id, std::shared_ptr<IAutoRigInputContext> context) {
    auto self = shared_from_this(); const auto owner = owners.find(id);
    require(owner != owners.end(), "Unknown workflow task owner",223); auto retained = owner->second;
    return compat::ngRequireObject(retained)->renderInputUI(detail::aaAt(originalTaskIds, id, 224), context);
}
bool WorkflowProcessor::renderOutputUI(Text id, std::shared_ptr<IAutoRigOutputContext> context) {
    auto self = shared_from_this(); const auto owner = owners.find(id);
    require(owner != owners.end(), "Unknown workflow task owner",229); auto retained = owner->second;
    return compat::ngRequireObject(retained)->renderOutputUI(detail::aaAt(originalTaskIds, id, 230), context);
}
}
