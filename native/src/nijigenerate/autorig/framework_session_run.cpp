#include "framework.hpp"
#include "detail/framework_runtime.hpp"
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
        inputContexts[taskId] = std::allocate_shared<AutoRigInputContext>(
            SourceAllocator<AutoRigInputContext>{}, this, taskId);
        outputContexts[taskId] = std::allocate_shared<AutoRigOutputContext>(
            SourceAllocator<AutoRigOutputContext>{}, this, taskId);
    }
    for (const auto& [id, spec] : specs) {
        for (const auto dependency : spec.dependencies)
            enforce(specs.contains(str(dependency)) && dependency.view() != id.view(), [&] { return Text("Invalid AutoRig dependency"); }, 630);
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
        for (const auto con : spec.connections) if (con.sourceTask.view() == changed.view()) affected = true;
        if (!affected || aaAt(snapshots, id, 666).state == AutoRigTaskState::Stale) continue;
        if (aaAt(snapshots, id, 667).state == AutoRigTaskState::Succeeded) {
            auto snapshot = aaAt(snapshots, id, 668);
            snapshot.state = AutoRigTaskState::Stale;
            snapshots[id] = snapshot;
        }
        committedOutputs.erase(id); invalidateDependents(id);
    }
}
void AutoRigSession::runTask(Text id, Table<bool>& visiting, bool force) {
    auto self = shared_from_this();
    enforce(specs.contains(id), [&] { return Text("Unknown AutoRig task: " + id); }, 678);
    enforce(!visiting.contains(id), [&] { return Text("AutoRig task dependency cycle at " + id); }, 679);
    if (!force && aaAt(snapshots, id, 680).state == AutoRigTaskState::Succeeded) return;
    if (!force && aaAt(snapshots, id, 681).state == AutoRigTaskState::Failed &&
        aaAt(specs, id, 682).retainFailureOutputs && committedOutputs.contains(id)) return;
    visiting[id] = true;
    compat::ngRunWithScopeExit([&] {
        const auto spec = aaAt(specs, id, 685);
        for (const auto dep : spec.dependencies) runTask(str(dep), visiting, false);
        for (const auto con : spec.connections) runTask(str(con.sourceTask), visiting, false);
        enforce(!cancelRequested.load(), [&] { return Text("AutoRig run canceled"); }, 688);
        Table<AutoRigValue> inputs;
        if (auto supplied = suppliedInputs.find(id); supplied != suppliedInputs.end()) inputs = supplied->second.duplicate();
        for (const auto con : spec.connections) {
            const auto source = committedOutputs.find(str(con.sourceTask));
            enforce(source != committedOutputs.end(), [&] { return Text("AutoRig dependency has no outputs: " + str(con.sourceTask)); }, 694);
            const auto value = source->second.find(str(con.sourceOutput));
            enforce(value != source->second.end(), [&] { return Text("AutoRig dependency output missing: " + str(con.sourceOutput)); }, 696);
            inputs[str(con.input)] = value->second;
        }
        for (const auto port : spec.inputs) {
            const auto value = inputs.find(str(port.id));
            enforce(!port.required || value != inputs.end(), [&] { return Text("Missing AutoRig input: " + str(port.id)); }, 703);
            if (value != inputs.end()) enforce(value->second.kind == port.kind, [&] { return Text("AutoRig input kind mismatch: " + str(port.id)); }, 704);
        }
        AutoRigTaskSnapshot snapshot;
        {
            std::lock_guard lock(mutex_); invalidateDependents(id); committedOutputs.erase(id);
            snapshot = aaAt(snapshots, id, 711); ++snapshot.attempt; snapshot.state = AutoRigTaskState::Running;
            snapshot.message.reset(); snapshot.artifacts = {}; snapshots[id] = snapshot;
        }
        auto directory = joined(joined(directory_, id), "attempt-" + std::to_string(snapshot.attempt));
        auto context = std::allocate_shared<AutoRigTaskContext>(SourceAllocator<AutoRigTaskContext>{}, spec, directory, inputs,
            [self] { return self->cancelRequested.load(); }, [self, id](AutoRigArtifact artifact) {
                std::lock_guard lock(self->mutex_); auto current = aaAt(self->snapshots, id, 726);
                artifact.attempt = current.attempt; current.artifacts.append(artifact); self->snapshots[id] = current;
            }, editorDispatcher);
        context->sessionId_ = id_;
        context->workspaceProvider = [self](Text name, compat::SharedDelegate<std::shared_ptr<AutoRigWorkspace>()> create) {
            std::lock_guard lock(self->mutex_);
            if (auto existing = self->workspaces.find(name); existing != self->workspaces.end()) return existing->second;
            auto workspace = create(); enforce(workspace != nullptr, [&] { return Text("AutoRig workspace factory returned null"); }, 737);
            self->workspaces[name] = workspace; return workspace;
        };
        compat::ngRunWithScopeExit([&] {
            { std::lock_guard lock(mutex_); for (const auto& [name, value] : sessionValues) context->sessionValues[name] = ngCopyAutoRigValue(value); }
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
                { std::lock_guard lock(mutex_); committedOutputs[id] = context->outputs.duplicate(); }
            } catch (const compat::ThrowableException& error) {
                snapshot.state = cancelRequested.load() ? AutoRigTaskState::Canceled : AutoRigTaskState::Failed;
                snapshot.message = error.msg;
                if (spec.retainFailureOutputs && !context->reportedFailure.empty() && !cancelRequested.load()) {
                    bool complete = true;
                    for (const auto port : spec.outputs) if (port.required && !context->outputs.contains(str(port.id))) complete = false;
                    if (complete) { std::lock_guard lock(mutex_); committedOutputs[id] = context->outputs.duplicate(); }
                }
            }
            { std::lock_guard lock(mutex_); snapshot.artifacts = aaAt(snapshots, id, 782).artifacts; }
            for (const auto& [key, artifact] : context->artifacts) {
                auto item = artifact; item.attempt = snapshot.attempt;
                item.committed = snapshot.state == AutoRigTaskState::Succeeded || (spec.retainFailureOutputs && committedOutputs.contains(id));
                snapshot.artifacts.append(item);
            }
            { std::lock_guard lock(mutex_); snapshots[id] = snapshot; }
            enforce(snapshot.state == AutoRigTaskState::Succeeded || (spec.retainFailureOutputs && committedOutputs.contains(id)), [&] { return Text("AutoRig task " + id + " failed: " + snapshot.message); }, 791);
        }, [&] {
            context->inputs.reset(); context->outputs.reset(); context->artifacts.reset(); context->sessionValues.reset();
            context->workspaceProvider = {}; context->onPreview = {}; context->canceled = {}; context->editorDispatcher = {};
        });
    }, [&] { visiting.erase(id); });
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
    cancelRequested.store(false); busy = false;
}
void AutoRigSession::execute(Text id, bool force) { executeGroup({id}, force); }
void AutoRigSession::executeGroup(compat::Slice<Text> ids, bool force) {
    core::thread_context::ManagedThreadScope managedThread;
    auto self = shared_from_this(); Table<std::uint32_t> initial;
    {
        std::lock_guard lock(mutex_); enforce(!busy, [&] { return Text("AutoRig session is already executing"); }, 944);
        for (const auto& id : ids) if (auto i = snapshots.find(id); i != snapshots.end()) initial[id] = i->second.attempt;
        busy = true;
    }
    compat::ngRunWithScopeExit([&] {
        {
            std::lock_guard lock(mutex_); Table<bool> inspected;
            compat::SharedDelegate<void(Text)> prepare = [&](Text id) {
                enforce(specs.contains(id), [&] { return Text("Unknown AutoRig task: " + id); }, 955);
                if (inspected.contains(id)) return; inspected[id] = true;
                const auto spec = aaAt(specs, id, 958);
                for (const auto dep : spec.dependencies) prepare(str(dep));
                for (const auto con : spec.connections) prepare(str(con.sourceTask));
                if (aaAt(snapshots, id, 961).state == AutoRigTaskState::Failed) {
                    invalidateDependents(id); committedOutputs.erase(id); snapshots[id].state = AutoRigTaskState::Stale;
                }
            };
            for (const auto& id : ids) prepare(id);
        }
        Table<bool> visiting; for (const auto& id : ids) runTask(id, visiting, force);
    }, [&] { finishExecution(ids, initial); });
}
void AutoRigSession::cancel() { std::lock_guard lock(mutex_); cancelRequested.store(true); }
}
