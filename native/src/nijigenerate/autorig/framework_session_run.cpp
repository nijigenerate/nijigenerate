#include "framework.hpp"
#include "detail/framework_runtime.hpp"
#include <algorithm>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/thread_context.hpp>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace nijigenerate::autorig::framework {
using namespace detail;
AutoRigSession::AutoRigSession(std::shared_ptr<AutoRigProcessor> value, Text id,
    Text directory, AutoRigTaskActionBoundary boundary, AutoRigEditorDispatcher dispatcher)
    : processor(std::move(value)), id_(std::move(id)), directory_(std::move(directory)),
      actionBoundary(std::move(boundary)), editorDispatcher(std::move(dispatcher)) {
    prepareSourceClassInfo();
    for (const auto spec : compat::ngRequireObject(processor)->tasks()) {
        const auto taskId = str(spec.id);
        enforce(safeId(taskId) && !specs.contains(taskId), [&] { return Text("Invalid or duplicate AutoRig task ID"); }, 612);
        Table<bool> ins, outs;
        for (const auto port : spec.inputs) {
            const auto id = str(port.id); enforce(safeId(id) && !ins.contains(id), [&] { return Text("Invalid or duplicate input port"); }, 616); ins[id] = true;
        }
        for (const auto port : spec.outputs) {
            const auto id = str(port.id); enforce(safeId(id) && !outs.contains(id), [&] { return Text("Invalid or duplicate output port"); }, 620); outs[id] = true;
        }
        specs[taskId] = spec; snapshots[taskId].taskId = taskId;
        if (!spec.tagCondition.empty() && !tagConditions.contains(spec.tagCondition)) {
            AutoRigTagCondition rule{str(spec.tagCondition)};
            for (const auto& tag : spec.conditionTags.empty() ? spec.targetTags : spec.conditionTags)
                rule.tags.push_back(str(tag));
            enforce(!rule.tags.empty(), [&] { return Text("Missing default AutoRig tag condition"); }, 624);
            tagConditions[rule.id] = std::move(rule);
        }
        inputContexts[taskId] = std::allocate_shared<AutoRigInputContext>(
            SourceAllocator<AutoRigInputContext>{}, this, taskId);
        outputContexts[taskId] = std::allocate_shared<AutoRigOutputContext>(
            SourceAllocator<AutoRigOutputContext>{}, this, taskId);
    }
    for (const auto& [id, spec] : specs) {
        for (const auto dependency : spec.dependencies)
            enforce(specs.contains(str(dependency)) && dependency.view() != id.view(), [&] { return Text("Invalid AutoRig dependency"); }, 630);
        for (const auto fallback : spec.checkpointFallbacks)
            enforce(hasPort(spec.inputs, fallback.view()) && hasPort(spec.outputs, fallback.view()) &&
                portKind(spec.inputs, fallback.view()) == portKind(spec.outputs, fallback.view()),
                [&] { return Text("Invalid AutoRig checkpoint fallback"); }, 631);
        Table<bool> connected;
        for (const auto connection : spec.connections) {
            const auto input = str(connection.input), sourceId = str(connection.sourceTask);
            enforce(!connected.contains(input), [&] { return Text("Duplicate AutoRig input connection"); }, 633); connected[input] = true;
            enforce(specs.contains(sourceId) && sourceId != id, [&] { return Text("Invalid AutoRig connection source"); }, 635);
            enforce(hasPort(spec.inputs, input), [&] { return Text("Unknown AutoRig connected input"); }, 637);
            const auto source = aaAt(specs, sourceId, 638);
            enforce(hasPort(source.outputs, connection.sourceOutput.view()), [&] { return Text("Unknown AutoRig source output"); }, 639);
            enforce(portKind(spec.inputs, input) == portKind(source.outputs, connection.sourceOutput.view()), [&] { return Text("AutoRig connection kind mismatch"); }, 640);
        }
    }
    Table<std::uint8_t> visits;
    for (const auto& id : keys(specs)) validateDependencies(id, visits);
}
AutoRigSession::~AutoRigSession() = default;
void AutoRigSession::validateDependencies(Text id, Table<std::uint8_t>& visits) {
    auto status = visits.find(id); if (status != visits.end() && status->second == 2) return;
    enforce(status == visits.end() || status->second != 1, [&] { return Text("AutoRig task dependency cycle at " + id); }, 651);
    visits[id] = 1; const auto spec = aaAt(specs, id, 653);
    for (const auto dep : spec.dependencies) validateDependencies(str(dep), visits);
    for (const auto con : spec.connections) validateDependencies(str(con.sourceTask), visits);
    visits[id] = 2;
}
void AutoRigSession::invalidateDependents(Text changed) {
    for (const auto& [id, spec] : specs) {
        bool affected = false;
        for (const auto dep : spec.dependencies) if (dep.view() == changed.view()) affected = true;
        for (const auto con : spec.connections)
            if (con.sourceTask.view() == changed.view() &&
                (!suppliedEvidence.contains(id) || !suppliedEvidence.at(id).contains(con.input))) affected = true;
        if (!affected || snapshots.at(id).state == AutoRigTaskState::Stale) continue;
        if (snapshots.at(id).state != AutoRigTaskState::Pending)
            snapshots[id].state = AutoRigTaskState::Stale;
        committedOutputs.erase(id);
        publishedEvidence.erase(id);
        invalidateDependents(id);
    }
}
Text AutoRigSession::resolveInputEvidence(Text task, Text port) {
    if (const auto supplied = suppliedEvidence.find(task); supplied != suppliedEvidence.end())
        if (const auto value = supplied->second.find(port); value != supplied->second.end()) return value->second;
    for (const auto connection : specs.at(task).connections)
        if (connection.input.view() == port.view())
            return resolveOutputEvidence(str(connection.sourceTask), str(connection.sourceOutput));
    return {};
}
Text AutoRigSession::resolveOutputEvidence(Text task, Text port) {
    if (const auto published = publishedEvidence.find(task); published != publishedEvidence.end())
        if (const auto value = published->second.find(port); value != published->second.end()) return value->second;
    for (const auto fallback : specs.at(task).checkpointFallbacks)
        if (fallback.view() == port.view()) return resolveInputEvidence(task, port);
    return {};
}
void AutoRigSession::runTask(Text id, Table<bool>& processed, const Table<bool>& forced, ScopedExecution* scoped) {
    if (processed.contains(id) || cancelRequested.load()) return;
    auto self = shared_from_this();
    const auto spec = specs.at(id);
    if (!scoped) for (const auto dep : spec.dependencies) runTask(str(dep), processed, forced);
    if (!scoped) for (const auto con : spec.connections) {
        if (suppliedEvidence.contains(id) && suppliedEvidence.at(id).contains(con.input)) continue;
        const auto source = str(con.sourceTask);
        if (forced.contains(source) || !publishedEvidence.contains(source) ||
            !publishedEvidence.at(source).contains(con.sourceOutput))
            runTask(source, processed, forced);
    }
    processed[id] = true;
    if (cancelRequested.load()) return;

    {
        Table<AutoRigValue> inputs;
        Table<Text> inputIds;
        Text missing;
        for (const auto port : spec.inputs) {
            const auto portId = str(port.id);
            auto evidenceId = resolveInputEvidence(id, portId);
            if (scoped && !(suppliedEvidence.contains(id) && suppliedEvidence.at(id).contains(portId))) {
                for (const auto connection : spec.connections) {
                    if (connection.input.view() != portId.view()) continue;
                    const auto producer = scoped->outputs.find(connection.sourceTask);
                    if (producer != scoped->outputs.end())
                        evidenceId = producer->second.contains(connection.sourceOutput) ?
                            producer->second.at(connection.sourceOutput) : Text{};
                }
            }
            if (scoped && spec.useCurrentCheckpoint && currentCheckpoint.contains(portId) &&
                !(suppliedEvidence.contains(id) && suppliedEvidence.at(id).contains(portId)))
                evidenceId = currentCheckpoint.at(portId);
            if (evidenceId.empty()) {
                if (port.required) missing = missing.empty() ? portId : missing + ", " + portId;
                continue;
            }
            inputs[portId] = evidence.at(evidenceId).value;
            inputIds[portId] = evidenceId;
        }
        std::shared_ptr<const AutoRigTargetScope> eligibleScope;
        if (!spec.tagCondition.empty()) {
            Text modelIdentity;
            if (inputs.contains("state")) {
                const auto& state = inputs.at("state").json;
                if (state.is_object()) if (const auto root = state.sourceObjectRef().if_contains("rootId")) {
                    if (root->is_uint64()) modelIdentity = Text(std::to_string(root->as_uint64()));
                    else if (root->is_int64()) modelIdentity = Text(std::to_string(root->as_int64()));
                }
            }
            if (modelIdentity.empty()) eligibleScope = std::make_shared<const AutoRigTargetScope>();
            else {
                auto eligible = resolveTargets(str(spec.tagCondition), modelIdentity);
                for (std::size_t i = 0; i < eligible.evidenceIds.size(); ++i)
                    inputIds[Text("tag-evidence-" + std::to_string(i))] = eligible.evidenceIds[i];
                if (const auto setting = targetRestrictions.find(id); setting != targetRestrictions.end()) {
                    inputIds[Text("target-settings")] = setting->second;
                    const auto selection = evidence.at(setting->second).value.object<AutoRigTargetAssociations>();
                    std::erase_if(eligible.targets, [&](const auto& target) {
                        return selection->modelIdentity != modelIdentity ||
                            std::ranges::none_of(selection->members, [&](const auto& member) {
                                return target.node == member.node;
                            });
                    });
                }
                eligibleScope = std::make_shared<const AutoRigTargetScope>(std::move(eligible));
            }
        }
        const auto previous = attemptedInputs.find(id);
        bool sameInputs = previous != attemptedInputs.end() && previous->second.size() == inputIds.size();
        if (sameInputs) for (const auto& [port, value] : inputIds)
            if (!previous->second.contains(port) || previous->second.at(port) != value) sameInputs = false;
        if (!scoped && sameInputs && !forced.contains(id) &&
            (snapshots.at(id).state == AutoRigTaskState::Succeeded ||
             snapshots.at(id).state == AutoRigTaskState::NotApplicable)) {
            std::lock_guard lock(mutex_);
            ++progress_.resolvedTasks;
            return;
        }
        if (scoped && spec.phase == AutoRigTaskPhase::Collect && !scoped->scope->selector.refreshEvidence &&
            sameInputs && snapshots.at(id).state == AutoRigTaskState::Succeeded) {
            std::lock_guard lock(mutex_);
            ++progress_.resolvedTasks;
            return;
        }
        if (scoped) scoped->outputs[id] = {};
        if (!missing.empty()) {
            std::lock_guard lock(mutex_);
            auto snapshot = snapshots.at(id);
            const auto message = Text("Missing AutoRig evidence: ") + missing;
            if (scoped || !sameInputs || snapshot.state != AutoRigTaskState::Blocked || snapshot.message != message) {
                snapshot.attempt = attempts.contains(id) ? attempts.at(id)[attempts.at(id).size() - 1].attempt + 1 : 1;
                snapshot.scope = scoped ? scoped->scope : nullptr;
                snapshot.units.clear();
                snapshot.inputEvidenceIds = inputIds;
                snapshot.artifacts = {};
                snapshot.state = AutoRigTaskState::Blocked;
                snapshot.message = message;
                if (scoped) scoped->report.tasks.push_back(snapshot);
                else {
                    attemptedInputs[id] = inputIds;
                    snapshots[id] = snapshot;
                }
                attempts[id].append(snapshot);
            }
            ++progress_.resolvedTasks;
            return;
        }
        AutoRigTaskSnapshot snapshot;
        {
            std::lock_guard lock(mutex_);
            if (!scoped && !sameInputs) {
                invalidateDependents(id);
                committedOutputs.erase(id);
                publishedEvidence.erase(id);
            }
            if (!scoped) attemptedInputs[id] = inputIds;
            snapshot.taskId = id;
            snapshot.attempt = attempts.contains(id) ? attempts.at(id)[attempts.at(id).size() - 1].attempt + 1 : 1;
            snapshot.state = AutoRigTaskState::Running;
            progress_.currentTask = id;
            snapshot.inputEvidenceIds = inputIds;
            snapshot.scope = scoped ? scoped->scope : nullptr;
            for (const auto& tag : spec.targetTags) snapshot.tags.push_back(str(tag));
            if (scoped) snapshot.tags = scoped->scope->selector.tags;
            else snapshots[id] = snapshot;
        }
        auto directory = joined(joined(directory_, id), "attempt-" + std::to_string(snapshot.attempt));
        auto context = std::allocate_shared<AutoRigTaskContext>(SourceAllocator<AutoRigTaskContext>{}, spec, directory, inputs,
            [self] { return self->cancelRequested.load(); }, [self, id, scoped, &snapshot](AutoRigArtifact artifact) {
                if (scoped) {
                    artifact.attempt = snapshot.attempt;
                    snapshot.artifacts.append(artifact);
                    return;
                }
                std::lock_guard lock(self->mutex_); auto current = aaAt(self->snapshots, id, 726);
                artifact.attempt = current.attempt; current.artifacts.append(artifact); self->snapshots[id] = current;
            }, editorDispatcher);
        context->sessionId_ = id_;
        if (scoped) context->scope_ = scoped->scope;
        context->inputEvidenceIds = inputIds;
        context->eligibleScope_ = eligibleScope;
        context->workspaceProvider = [self](Text name, compat::SharedDelegate<std::shared_ptr<AutoRigWorkspace>()> create) {
            std::lock_guard lock(self->mutex_);
            if (auto existing = self->workspaces.find(name); existing != self->workspaces.end()) return existing->second;
            auto workspace = create(); enforce(workspace != nullptr, [&] { return Text("AutoRig workspace factory returned null"); }, 737);
            self->workspaces[name] = workspace; return workspace;
        };
        compat::ngRunWithScopeExit([&] {
            { std::lock_guard lock(mutex_); for (const auto& [name, value] : sessionValues) context->sessionValues[name] = ngCopyAutoRigValue(value); }
            bool publishOutputs = false;
            try {
                Action executeInFiber = [self, context, id] {
#ifdef _WIN32
                    SYSTEM_INFO systemInfo{}; GetSystemInfo(&systemInfo);
                    const auto stackSize = static_cast<std::size_t>(systemInfo.dwPageSize) * 8 * 4;
#else
                    const auto stackSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE)) * 4 * 4;
#endif
                    core::tasks::TaskFiber fiber([self, context, id] {
                        compat::ngRequireObject(self->processor)->executeTask(id, context);
                    }, stackSize);
                    while (fiber.state() != core::tasks::TaskFiber::State::term) fiber.call();
                };
                auto boundary = actionBoundary;
                if (boundary && !spec.ownsActionBoundary) boundary(id, executeInFiber); else executeInFiber();
                enforce(!cancelRequested.load(), [&] { return Text("AutoRig run canceled"); }, 766);
                for (const auto port : spec.outputs)
                    enforce(!port.required || context->outputs.contains(str(port.id)), [&] { return Text("Missing AutoRig output: " + str(port.id)); }, 768);
                enforce(context->reportedFailure.empty(), [&] { return Text(context->reportedFailure); }, 770);
                snapshot.state = AutoRigTaskState::Succeeded;
                publishOutputs = true;
            } catch (const compat::ThrowableException& error) {
                snapshot.state = cancelRequested.load() ? AutoRigTaskState::Canceled : AutoRigTaskState::Failed;
                snapshot.message = error.msg;
                if (spec.retainFailureOutputs && !context->reportedFailure.empty() && !cancelRequested.load()) {
                    bool complete = true;
                    for (const auto port : spec.outputs) if (port.required && !context->outputs.contains(str(port.id))) complete = false;
                    publishOutputs = complete && (scoped || !publishedEvidence.contains(id));
                }
            } catch (const compat::ThrowableError& error) {
                // A native range/assertion failure must not leave a finished worker
                // advertised as a running task. Preserve its source location.
                snapshot.state = AutoRigTaskState::Failed;
                snapshot.message = Text(std::string(error.msg.view()) + " @ " +
                    std::string(error.file.view()) + ":" + std::to_string(error.line));
            } catch (const std::exception& error) {
                snapshot.state = AutoRigTaskState::Failed;
                snapshot.message = Text(error.what());
            }
            {
                std::lock_guard lock(mutex_);
                if (!scoped) snapshot.artifacts = snapshots.at(id).artifacts;
                if (!scoped && context->eligibleScope_) snapshot.scope = context->eligibleScope_;
                snapshot.units = std::move(context->units);
                if (snapshot.state == AutoRigTaskState::Succeeded && !snapshot.units.empty()) {
                    std::size_t applied = 0, unavailable = 0, failed = 0;
                    for (const auto& item : snapshot.units) {
                        applied += item.state == AutoRigTaskState::Succeeded;
                        unavailable += item.state == AutoRigTaskState::Blocked;
                        failed += item.state == AutoRigTaskState::Failed;
                    }
                    if ((unavailable || failed) && snapshot.message.empty())
                        snapshot.message = Text(std::to_string(unavailable + failed) + " AutoRig units did not complete");
                    if (unavailable || failed)
                        snapshot.state = applied ? AutoRigTaskState::PartiallySucceeded :
                            failed ? AutoRigTaskState::Failed : AutoRigTaskState::Blocked;
                    else if (!applied) snapshot.state = AutoRigTaskState::NotApplicable;
                } else if (scoped && spec.useCurrentCheckpoint && snapshot.state == AutoRigTaskState::Succeeded) {
                    snapshot.state = AutoRigTaskState::NotApplicable;
                }
                if (publishOutputs && !scoped) {
                    invalidateDependents(id);
                    committedOutputs[id] = context->outputs.duplicate();
                    publishedEvidence.erase(id);
                }
                for (const auto& [port, artifact] : context->artifacts) {
                    auto item = artifact;
                    item.attempt = snapshot.attempt;
                    item.committed = publishOutputs;
                    if (publishOutputs) {
                        const auto evidenceId = recordEvidence(id, port, context->outputs.at(port));
                        if (scoped) scoped->outputs[id][port] = evidenceId;
                        else publishedEvidence[id][port] = evidenceId;
                        item.evidenceId = evidenceId;
                        evidenceHistory[evidenceId] = {evidenceId, id, port, snapshot.attempt, inputIds, snapshot.scope};
                        evidenceArtifacts[evidenceId] = item;
                    }
                    snapshot.artifacts.append(item);
                }
                if (publishOutputs) {
                    const auto& output = scoped ? scoped->outputs[id] : publishedEvidence[id];
                    if (output.contains("state") && output.contains("model")) {
                        currentCheckpoint = {};
                        currentCheckpoint[Text("state")] = output.at("state");
                        currentCheckpoint[Text("model")] = output.at("model");
                    }
                }
                if (scoped) scoped->report.tasks.push_back(snapshot);
                else snapshots[id] = snapshot;
                attempts[id].append(snapshot);
                ++progress_.resolvedTasks;
                progress_.currentTask = {};
                ++selectionRevision_;
            }
        }, [&] {
            context->inputs.reset(); context->outputs.reset(); context->artifacts.reset(); context->sessionValues.reset();
            context->workspaceProvider = {}; context->onPreview = {}; context->canceled = {}; context->editorDispatcher = {};
        });
    }
}
void AutoRigSession::finishExecution(compat::Slice<Text> ids, Table<std::uint32_t>& initial) {
    std::lock_guard lock(mutex_);
    if (cancelRequested.load()) for (const auto& id : ids) {
        if (!snapshots.contains(id)) continue;
        if (aaAt(snapshots, id, 930).state == AutoRigTaskState::Succeeded) {
            const auto attempt = aaAt(snapshots, id, 931).attempt;
            const auto first = initial.find(id);
            if (attempt > (first == initial.end() ? 0 : first->second)) continue;
        }
        snapshots[id].state = AutoRigTaskState::Canceled;
        snapshots[id].message = Text::fromStaticLiteral("AutoRig run canceled");
    }
    progress_.currentTask = {};
    cancelRequested.store(false); busy = false;
}
void AutoRigSession::execute(Text id, bool force) { executeGroup({id}, force); }
void AutoRigSession::executeGroup(compat::Slice<Text> ids, bool force) {
    core::thread_context::ManagedThreadScope managedThread;
    auto self = shared_from_this(); Table<std::uint32_t> initial;
    {
        std::lock_guard lock(mutex_); enforce(!busy, [&] { return Text("AutoRig session is already executing"); }, 944);
        for (const auto& id : ids) if (auto i = snapshots.find(id); i != snapshots.end()) initial[id] = i->second.attempt;
        progress_ = {};
        busy = true;
    }
    compat::ngRunWithScopeExit([&] {
        Table<bool> processed, forced;
        for (const auto& id : ids) {
            enforce(specs.contains(id), [&] { return Text("Unknown AutoRig task: " + id); }, 955);
            if (force) forced[id] = true;
        }
        for (const auto& id : ids) runTask(id, processed, forced);
    }, [&] { finishExecution(ids, initial); });
}
AutoRigExecutionReport AutoRigSession::execute(const AutoRigScopedPlan& plan) {
    core::thread_context::ManagedThreadScope managedThread;
    auto self = shared_from_this();
    {
        std::lock_guard lock(mutex_);
        enforce(!busy && plan.sessionId == id_ && plan.revision == selectionRevision_ && bool(plan.scope),
            [&] { return Text("AutoRig execution plan is stale"); }, 963);
        progress_ = {};
        busy = true;
    }
    ScopedExecution execution{plan.scope, {}, {}};
    compat::ngRunWithScopeExit([&] {
        Table<bool> processed, forced;
        for (const auto& id : plan.tasks) {
            if (cancelRequested.load()) break;
            const auto& spec = specs.at(id);
            const bool related = spec.targetTags.empty() || std::ranges::any_of(spec.targetTags, [&](const auto& tag) {
                return std::ranges::any_of(execution.scope->targets, [&](const auto& target) {
                    return ngAutoRigTagMatches(target.tag.view(), tag.view());
                });
            });
            if (!execution.scope->targets.empty() && spec.supportsTargetScope && related)
                runTask(id, processed, forced, &execution);
            else {
                AutoRigTaskSnapshot item;
                item.taskId = id;
                item.scope = plan.scope;
                item.tags = plan.scope->selector.tags;
                if (execution.scope->targets.empty()) {
                    item.state = AutoRigTaskState::Blocked;
                    item.message = Text("No confirmed targets for the requested scope");
                } else if (!related) {
                    item.state = AutoRigTaskState::NotApplicable;
                    item.message = Text("Task has no confirmed target in the requested scope");
                } else {
                    item.state = AutoRigTaskState::Blocked;
                    item.message = Text("Task requires a complete model or an indivisible set of targets");
                }
                {
                    std::lock_guard lock(mutex_);
                    item.attempt = attempts.contains(id) ? attempts.at(id)[attempts.at(id).size()-1].attempt + 1 : 1;
                    attempts[id].append(item);
                    ++progress_.resolvedTasks;
                }
                execution.report.tasks.push_back(std::move(item));
            }
        }
    }, [&] {
        std::lock_guard lock(mutex_);
        progress_.currentTask = {};
        busy = false;
        cancelRequested.store(false);
    });
    return execution.report;
}
void AutoRigSession::cancel() { std::lock_guard lock(mutex_); cancelRequested.store(true); }
}
