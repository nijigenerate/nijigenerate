#include "framework.hpp"
#include "detail/framework_runtime.hpp"
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <tuple>

namespace nijigenerate::autorig::framework {
using namespace detail;
AutoRigInputContext::AutoRigInputContext(AutoRigSession* session, Text id)
    : session_(session), taskId_(std::move(id)) { prepareSourceClassInfo(); }
AutoRigOutputContext::AutoRigOutputContext(AutoRigSession* session, Text id)
    : session_(session), taskId_(std::move(id)) { prepareSourceClassInfo(); }
void AutoRigSession::restoreCommittedOutputs() {
    enforce(!busy, [&] { return Text("Cannot restore an executing AutoRig session"); }, 799);
}
Text AutoRigSession::processorId() { auto self = shared_from_this(); return compat::ngRequireObject(processor)->procId(); }
bool AutoRigSession::isBusy() { std::lock_guard lock(mutex_); return busy; }
AutoRigExecutionProgress AutoRigSession::executionProgress() {
    std::lock_guard lock(mutex_);
    return busy ? progress_ : AutoRigExecutionProgress{};
}
Json AutoRigSession::memoryInfo() {
    auto self = shared_from_this(); std::lock_guard lock(mutex_);
    std::uint64_t jsonBytes = 0, blobBytes = 0, previewBytes = 0, cpuBytes = 0;
    core::settings::SettingsJsonArray entries;
    for (const auto& [id, snapshot] : snapshots) for (const auto artifact : snapshot.artifacts) {
        const auto size = artifact.byteLength();
        if (artifact.kind == AutoRigValueKind::Json) jsonBytes += size;
        if (artifact.kind == AutoRigValueKind::Blob) blobBytes += size;
        if (artifact.preview) previewBytes += size;
        core::settings::SettingsJsonObject entry;
        entry["task"] = core::settings::SettingsString(id);
        entry["port"] = core::settings::SettingsString(artifact.portId);
        entry["kind"] = core::settings::SettingsString(valueKindText(artifact.kind)); entry["preview"] = artifact.preview;
        entry["bytes"] = static_cast<std::uint64_t>(size); entries.append(entry);
    }
    core::settings::SettingsJsonObject workspaceInfo;
    for (const auto& [name, workspace] : workspaces) {
        auto info = compat::ngRequireObject(workspace)->memoryInfo(); workspaceInfo[core::settings::SettingsString(name)] = info;
        jsonBytes += jsonIndex(info, Text::fromStaticLiteral("json_bytes")).as_uint64();
        cpuBytes += jsonIndex(info, Text::fromStaticLiteral("cpu_bytes")).as_uint64();
    }
    core::settings::SettingsJsonObject result;
    result["json_bytes"] = jsonBytes; result["blob_bytes"] = blobBytes; result["cpu_bytes"] = cpuBytes;
    result["workspaces"] = workspaceInfo; result["preview_bytes"] = previewBytes; result["artifacts"] = entries;
    return result;
}
void AutoRigSession::dispose() {
    auto self = shared_from_this(); std::lock_guard lock(mutex_);
    enforce(!busy, [&] { return Text("Cannot dispose a running AutoRig session"); }, 836);
    const auto ownedDirectory = absolutePath(directory_);
    enforce(pathBaseName(ownedDirectory) == id_, [&] { return Text("Invalid AutoRig session directory"); }, 840);
    if (fileExists(ownedDirectory)) {
        enforce(fileIsDirectory(ownedDirectory) && !fileIsSymlink(ownedDirectory), [&] { return Text("AutoRig session directory must be an owned directory"); }, 842);
        removeDirectoryRecurse(ownedDirectory);
    }
    for (const auto& [name, workspace] : workspaces) compat::ngRequireObject(workspace)->dispose();
    workspaces.reset(); committedOutputs.reset(); suppliedInputs.reset(); sessionValues.reset();
    evidence.reset(); evidenceHistory.reset(); targetEvidence.reset(); currentCheckpoint.reset(); publishedEvidence.reset(); suppliedEvidence.reset(); evidenceArtifacts.reset(); attemptedInputs.reset(); attempts.reset();
    targetRestrictions.reset();
    for (auto& [id, snapshot] : snapshots) snapshot.artifacts = {};
}
void AutoRigSession::setContextValue(Text name, AutoRigValue value) {
    std::lock_guard lock(mutex_); enforce(!busy, [&] { return Text("Cannot change AutoRig context during execution"); }, 858);
    enforce(!name.empty(), [&] { return Text("AutoRig context name is empty"); }, 859); sessionValues[name] = ngCopyAutoRigValue(value);
    ++contextRevision_;
    ++selectionRevision_;
    for (auto& [id, snapshot] : snapshots) if (snapshot.state == AutoRigTaskState::Succeeded) snapshot.state = AutoRigTaskState::Stale;
    committedOutputs.reset();
    publishedEvidence.reset();
}
AutoRigValue AutoRigSession::contextValue(Text name) {
    std::lock_guard lock(mutex_); const auto value = sessionValues.find(name);
    enforce(value != sessionValues.end(), [&] { return Text("Missing AutoRig session context: " + name); }, 872); return ngCopyAutoRigValue(value->second);
}
compat::Slice<Text> AutoRigSession::contextNames() { std::lock_guard lock(mutex_); return keys(sessionValues); }
AutoRigValueInfo AutoRigSession::contextInfo(Text name) {
    std::lock_guard lock(mutex_); const auto value = sessionValues.find(name);
    enforce(value != sessionValues.end(), [&] { return Text("Missing AutoRig session context: " + name); }, 882); return ngAutoRigValueInfo(value->second, contextRevision_);
}
std::uint64_t AutoRigSession::inputRevision(Text id) {
    std::lock_guard lock(mutex_); const auto value = inputRevisions.find(id); return value == inputRevisions.end() ? 0 : value->second;
}
void AutoRigSession::invalidateTask(Text id) {
    std::lock_guard lock(mutex_); enforce(!busy, [&] { return Text("Cannot invalidate AutoRig tasks during execution"); }, 893);
    ++selectionRevision_;
    enforce(specs.contains(id), [&] { return Text("Unknown AutoRig task"); }, 894); invalidateDependents(id); committedOutputs.erase(id); publishedEvidence.erase(id);
    auto snapshot = aaAt(snapshots, id, 897);
    snapshot.state = AutoRigTaskState::Stale;
    snapshots[id] = snapshot;
}
void AutoRigSession::setInput(Text id, Text port, AutoRigValue value) {
    std::lock_guard lock(mutex_); enforce(!busy, [&] { return Text("Cannot change AutoRig inputs during execution"); }, 905);
    const auto spec = specs.find(id);
    enforce(spec != specs.end() && hasPort(spec->second.inputs, port), [&] { return Text("Unknown AutoRig input"); }, 907);
    enforce(portKind(spec->second.inputs, port) == value.kind, [&] { return Text("AutoRig input kind mismatch"); }, 908);
    for (const auto con : spec->second.connections)
        enforce(con.input.view() != port.view(), [&] { return Text("Connected AutoRig input cannot be supplied directly"); }, 910);
    suppliedInputs[id][port] = ngCopyAutoRigValue(value);
    suppliedEvidence[id][port] = recordEvidence(id, port, suppliedInputs.at(id).at(port));
    ++inputRevisions[id];
    ++selectionRevision_;
    if (committedOutputs.contains(id)) invalidateDependents(id);
    committedOutputs.erase(id);
    publishedEvidence.erase(id);
    auto snapshot = aaAt(snapshots, id, 916);
    if (snapshot.state == AutoRigTaskState::Succeeded) snapshot.state = AutoRigTaskState::Stale;
    snapshots[id] = snapshot;
}
AutoRigTaskSnapshot AutoRigSession::task(Text id, bool include) {
    std::lock_guard lock(mutex_); const auto found = snapshots.find(id); enforce(found != snapshots.end(), [&] { return Text("Unknown AutoRig task"); }, 980);
    auto result = found->second; result.inputEvidenceIds = result.inputEvidenceIds.duplicate(); result.artifacts = include ? result.artifacts.duplicate() : compat::Slice<AutoRigArtifact>{}; return result;
}
AutoRigTaskSpec AutoRigSession::taskSpec(Text id) {
    const auto found = specs.find(id); enforce(found != specs.end(), [&] { return Text("Unknown AutoRig task"); }, 989);
    auto result = found->second; result.dependencies = result.dependencies.duplicate(); result.inputs = result.inputs.duplicate();
    result.outputs = result.outputs.duplicate(); result.connections = result.connections.duplicate();
    result.checkpointFallbacks = result.checkpointFallbacks.duplicate();
    result.targetTags = result.targetTags.duplicate();
    result.conditionTags = result.conditionTags.duplicate();
    return result;
}
bool AutoRigSession::hasSuppliedInput(Text id, Text port) {
    const auto spec = taskSpec(id); enforce(hasPort(spec.inputs, port), [&] { return Text("Unknown AutoRig input"); }, 1000);
    std::lock_guard lock(mutex_); const auto values = suppliedInputs.find(id);
    return values != suppliedInputs.end() && values->second.contains(port);
}
AutoRigValue AutoRigSession::suppliedInput(Text id, Text port) {
    const auto spec = taskSpec(id); enforce(hasPort(spec.inputs, port), [&] { return Text("Unknown AutoRig input"); }, 1009);
    std::lock_guard lock(mutex_); const auto values = suppliedInputs.find(id);
    enforce(values != suppliedInputs.end() && values->second.contains(port), [&] { return Text("AutoRig input has no supplied value"); }, 1012);
    return ngCopyAutoRigValue(aaAt(values->second, port, 1013));
}
bool AutoRigSession::hasOutput(Text id, Text port) {
    const auto spec = taskSpec(id); enforce(hasPort(spec.outputs, port), [&] { return Text("Unknown AutoRig output"); }, 1019);
    std::lock_guard lock(mutex_); const auto values = committedOutputs.find(id);
    return values != committedOutputs.end() && values->second.contains(port);
}
std::shared_ptr<IAutoRigInputContext> AutoRigSession::inputContext(Text id) {
    const auto found = inputContexts.find(id); enforce(found != inputContexts.end(), [&] { return Text("Unknown AutoRig task"); }, 1028);
    return {shared_from_this(), found->second.get()};
}
std::shared_ptr<IAutoRigOutputContext> AutoRigSession::outputContext(Text id) {
    const auto found = outputContexts.find(id); enforce(found != outputContexts.end(), [&] { return Text("Unknown AutoRig task"); }, 1034);
    return {shared_from_this(), found->second.get()};
}
bool AutoRigSession::renderInputUI(Text id) { auto self = shared_from_this(); return compat::ngRequireObject(processor)->renderInputUI(id, inputContext(id)); }
bool AutoRigSession::renderOutputUI(Text id) { auto self = shared_from_this(); return compat::ngRequireObject(processor)->renderOutputUI(id, outputContext(id)); }
AutoRigValue AutoRigSession::output(Text id, Text port) {
    std::lock_guard lock(mutex_); const auto values = committedOutputs.find(id);
    enforce(values != committedOutputs.end(), [&] { return Text("AutoRig task has no committed outputs"); }, 1049);
    const auto found = values->second.find(port); enforce(found != values->second.end(), [&] { return Text("Unknown AutoRig output"); }, 1051);
    return ngCopyAutoRigValue(found->second);
}
Text AutoRigSession::recordEvidence(Text task, Text port, AutoRigValue value) {
    const auto id = id_ + ":" + std::to_string(++nextEvidenceId);
    evidenceHistory[id] = {id, task, port};
    evidence[id] = {std::move(task), std::move(port), std::move(value)};
    return id;
}
bool ngAutoRigTagMatches(std::string_view tag, std::string_view selector) {
    return tag == selector || (tag.starts_with(selector) && tag.size() > selector.size() &&
        tag[selector.size()] == '/');
}
bool AutoRigTargetScope::contains(std::uint32_t node) const {
    return std::ranges::any_of(targets, [node](const auto& target) { return target.node == node; });
}
Text AutoRigSession::registerTargetAssociations(AutoRigTargetAssociations associations, Text slot) {
    std::lock_guard lock(mutex_);
    enforce(!busy && !slot.empty() && !associations.modelIdentity.empty(),
        [&] { return Text("Invalid or busy AutoRig target scope"); }, 1054);
    for (const auto& member : associations.members)
        enforce(!member.targetId.empty() && !member.tag.empty() && member.node != 0,
            [&] { return Text("Invalid AutoRig target association"); }, 1056);
    const auto id = recordEvidence({}, slot, AutoRigValue::objectValue(std::move(associations)));
    targetEvidence[slot] = id;
    for (const auto& [task, spec] : specs)
        if (!spec.tagCondition.empty()) invalidateTask(task);
    ++selectionRevision_;
    return id;
}
AutoRigTargetScope AutoRigSession::resolveTargetScope(const AutoRigTargetSelector& selector, Text modelIdentity,
    std::optional<std::uint32_t> node) {
    std::lock_guard lock(mutex_);
    AutoRigTargetScope result{selector, std::move(modelIdentity), {}, {}};
    std::unordered_set<std::string> seen, manuallyAssigned;
    std::unordered_map<std::uint32_t, std::vector<Text>> detectedTags;
    for (const auto& [task, outputs] : publishedEvidence) {
        if (!outputs.contains("target-associations")) continue;
        const auto source = evidence.at(outputs.at("target-associations")).value.object<AutoRigTargetAssociations>();
        if (source->modelIdentity != result.modelIdentity) continue;
        for (const auto& member : source->members) detectedTags[member.node].push_back(member.tag);
    }
    const auto collect = [&](Text id, bool manual) {
        const auto associations = evidence.at(id).value.object<AutoRigTargetAssociations>();
        if (associations->modelIdentity != result.modelIdentity) return;
        bool used = false;
        for (const auto& member : associations->members) {
            const std::string target(member.targetId.view());
            if (manual) {
                const auto detected = detectedTags.find(member.node);
                if (detected != detectedTags.end() && !std::ranges::any_of(detected->second, [&](const auto& tag) {
                    return ngAutoRigTagMatches(tag.view(), member.tag.view());
                })) continue;
                manuallyAssigned.insert(target);
            } else if (manuallyAssigned.contains(target)) continue;
            const bool tagMatches = selector.tags.empty() || std::ranges::any_of(selector.tags,
                [&](const auto& tag) { return ngAutoRigTagMatches(member.tag.view(), tag.view()); });
            const bool targetMatches = selector.targetIds.empty() ||
                std::ranges::find(selector.targetIds, member.targetId) != selector.targetIds.end();
            if (!tagMatches || !targetMatches || (node && member.node != *node)) continue;
            used = true;
            const auto key = std::string(member.tag.view()) + "\n" + std::string(member.targetId.view()) +
                "\n" + std::to_string(member.node);
            if (seen.insert(key).second) result.targets.push_back(member);
        }
        if (used) result.evidenceIds.push_back(id);
    };
    for (const auto& [slot, id] : targetEvidence) collect(id, true);
    for (const auto& [task, outputs] : publishedEvidence)
        if (outputs.contains("target-associations")) collect(outputs.at("target-associations"), false);
    std::ranges::sort(result.targets, {}, [](const auto& member) {
        return std::tuple(member.tag.view(), member.targetId.view(), member.node);
    });
    std::ranges::sort(result.evidenceIds, {}, [](const auto& id) { return id.view(); });
    return result;
}
void AutoRigSession::registerTagCondition(AutoRigTagCondition condition) {
    std::lock_guard lock(mutex_);
    enforce(!busy && !condition.id.empty() && condition.version && !condition.tags.empty() &&
        std::ranges::none_of(condition.tags, [](const auto& tag) { return tag.empty(); }),
        [&] { return Text("Invalid or busy AutoRig tag condition"); }, 1057);
    const auto id = condition.id;
    const auto prior = tagConditions.find(id);
    enforce(prior == tagConditions.end() || prior->second.version < condition.version ||
            (prior->second.version == condition.version && prior->second.tags == condition.tags),
        [&] { return Text("AutoRig tag condition version must increase"); }, 1057);
    tagConditions[id] = std::move(condition);
    for (const auto& [task, spec] : specs)
        if (spec.tagCondition.view() == id.view()) invalidateTask(task);
    ++selectionRevision_;
}
AutoRigTargetScope AutoRigSession::resolveTargets(Text condition, Text modelIdentity) {
    std::lock_guard lock(mutex_);
    const auto rule = tagConditions.find(condition);
    enforce(rule != tagConditions.end(), [&] { return Text("Unknown AutoRig tag condition: ") + condition; }, 1057);
    // A tag used as input must be justified by the automatic evidence for
    // that Node, or by a confirmed manual association for an unclassified Node.
    auto result = resolveTargetScope({rule->second.tags, {}, false}, std::move(modelIdentity));
    result.condition = condition;
    result.conditionVersion = rule->second.version;
    return result;
}
AutoRigTagEvaluation AutoRigSession::evaluateTagCondition(Text condition, Text modelIdentity, std::uint32_t node) {
    std::lock_guard lock(mutex_);
    const auto rule = tagConditions.find(condition);
    enforce(rule != tagConditions.end(), [&] { return Text("Unknown AutoRig tag condition: ") + condition; }, 1057);
    const auto eligible = resolveTargetScope({rule->second.tags, {}, false}, modelIdentity, node);
    AutoRigTagEvaluation result{AutoRigTagMatch::MissingEvidence, condition, rule->second.version, {}};
    if (eligible.contains(node)) {
        result.status = AutoRigTagMatch::Matched;
        result.evidenceIds = eligible.evidenceIds;
    } else if (resolveTargetScope({}, modelIdentity, node).contains(node)) result.status = AutoRigTagMatch::Mismatch;
    return result;
}
AutoRigScopedPlan AutoRigSession::plan(compat::Slice<Text> tasks, AutoRigTargetSelector selector,
    Text modelIdentity) {
    std::lock_guard lock(mutex_);
    enforce(!busy && (!selector.tags.empty() || !selector.targetIds.empty()) && !modelIdentity.empty(),
        [&] { return Text("A scoped AutoRig run requires a model identity and target selector"); }, 1057);
    for (const auto& tag : selector.tags)
        enforce(!tag.empty(), [&] { return Text("Empty AutoRig target tag"); }, 1057);
    auto targets = std::make_shared<const AutoRigTargetScope>(resolveTargetScope(selector, std::move(modelIdentity)));
    compat::Slice<Text> ordered;
    Table<bool> selected, visited;
    for (const auto& task : tasks) {
        enforce(specs.contains(task), [&] { return Text("Unknown AutoRig task: ") + task; }, 1057);
        selected[task] = true;
    }
    std::function<void(Text)> visit = [&](Text id) {
        if (visited.contains(id)) return;
        visited[id] = true;
        const auto& spec = specs.at(id);
        for (const auto dep : spec.dependencies) if (selected.contains(dep)) visit(str(dep));
        for (const auto con : spec.connections) if (selected.contains(con.sourceTask)) visit(str(con.sourceTask));
        ordered.append(id);
    };
    for (const auto& task : tasks) visit(task);
    return {id_, selectionRevision_, std::move(targets), ordered};
}
Text AutoRigSession::outputEvidence(Text task, Text port, std::uint32_t attempt) {
    std::lock_guard lock(mutex_);
    if (attempt != 0 && attempts.contains(task)) {
        for (const auto& snapshot : attempts.at(task))
            if (snapshot.attempt == attempt)
                for (const auto& artifact : snapshot.artifacts)
                    if (artifact.portId == port && !artifact.evidenceId.empty()) return artifact.evidenceId;
        return {};
    }
    const auto sources = publishedEvidence.find(task);
    if (sources == publishedEvidence.end() || !sources->second.contains(port)) return {};
    return sources->second.at(port);
}
Text AutoRigSession::inputEvidence(Text task, Text port) {
    std::lock_guard lock(mutex_);
    return resolveInputEvidence(task, port);
}
Text AutoRigSession::boundInputEvidence(Text task, Text port) {
    std::lock_guard lock(mutex_);
    return suppliedEvidence.contains(task) && suppliedEvidence.at(task).contains(port)
        ? suppliedEvidence.at(task).at(port) : Text{};
}
Text AutoRigSession::targetRestriction(Text task) {
    std::lock_guard lock(mutex_);
    const auto it = targetRestrictions.find(task);
    return it == targetRestrictions.end() ? Text{} : it->second;
}
void AutoRigSession::setTargetRestriction(Text task, Text model,
    std::optional<std::vector<std::uint32_t>> nodes) {
    std::lock_guard lock(mutex_);
    const auto spec = taskSpec(task);
    enforce(!busy && !spec.tagCondition.empty() && !model.empty(),
        [&] { return Text("Cannot change target settings for this AutoRig task"); }, 1059);
    if (!nodes) {
        if (targetRestrictions.erase(task)) invalidateTask(task);
        return;
    }
    const auto eligible = resolveTargets(spec.tagCondition.retainedString(), model);
    enforce(std::ranges::all_of(*nodes, [&](auto node) { return eligible.contains(node); }),
        [&] { return Text("Target settings contain a Node without matching evidence"); }, 1059);
    AutoRigTargetAssociations selection{model, {}};
    for (const auto& item : eligible.targets)
        if (std::ranges::find(*nodes, item.node) != nodes->end()) selection.members.push_back(item);
    invalidateTask(task);
    targetRestrictions[task] = recordEvidence(task, "target-settings", AutoRigValue::objectValue(std::move(selection)));
}
compat::Slice<AutoRigTaskSnapshot> AutoRigSession::taskAttempts(Text task) {
    std::lock_guard lock(mutex_);
    compat::Slice<AutoRigTaskSnapshot> result;
    if (attempts.contains(task)) for (auto snapshot : attempts.at(task)) {
        snapshot.artifacts = snapshot.artifacts.duplicate();
        snapshot.inputEvidenceIds = snapshot.inputEvidenceIds.duplicate();
        result.append(snapshot);
    }
    return result;
}
AutoRigEvidenceInfo AutoRigSession::evidenceInfo(Text id) {
    std::lock_guard lock(mutex_);
    enforce(evidenceHistory.contains(id), [&] { return Text("Unknown AutoRig evidence: ") + id; }, 1058);
    auto result = evidenceHistory.at(id);
    result.inputs = result.inputs.duplicate();
    return result;
}
std::vector<AutoRigTaskSnapshot> AutoRigSession::findAttempts(const AutoRigTargetSelector& selector) {
    std::lock_guard lock(mutex_);
    std::vector<AutoRigTaskSnapshot> result;
    for (const auto& [id, entries] : attempts) {
        for (auto item : entries) {
            const auto matchingTag = [&](const Text& tag) {
                return std::ranges::any_of(selector.tags, [&](const Text& wanted) {
                    return ngAutoRigTagMatches(tag.view(), wanted.view());
                });
            };
            if (!selector.tags.empty() && !std::ranges::any_of(item.tags, matchingTag)) continue;
            if (!selector.targetIds.empty() && (!item.scope ||
                !std::ranges::any_of(item.scope->targets, [&](const auto& target) {
                    return std::ranges::find(selector.targetIds, target.targetId) != selector.targetIds.end();
                }))) continue;
            item.artifacts = item.artifacts.duplicate();
            item.inputEvidenceIds = item.inputEvidenceIds.duplicate();
            result.push_back(std::move(item));
        }
    }
    return result;
}
AutoRigValue AutoRigSession::readEvidence(Text id) {
    std::lock_guard lock(mutex_);
    enforce(evidence.contains(id), [&] { return Text("Unknown AutoRig evidence: ") + id; }, 1058);
    return ngCopyAutoRigValue(evidence.at(id).value);
}
Text AutoRigSession::registerInputEvidence(Text task, Text port, AutoRigValue value) {
    std::lock_guard lock(mutex_);
    enforce(!busy, [&] { return Text("Cannot register AutoRig evidence during execution"); }, 1059);
    const auto spec = taskSpec(task);
    enforce(hasPort(spec.inputs, port) && portKind(spec.inputs, port) == value.kind,
        [&] { return Text("AutoRig evidence does not match the input port: ") + port; }, 1060);
    return recordEvidence(task, port, ngCopyAutoRigValue(std::move(value)));
}
void AutoRigSession::bindInputEvidence(Text task, Text port, Text id) {
    std::lock_guard lock(mutex_);
    const auto spec = taskSpec(task);
    enforce(hasPort(spec.inputs, port), [&] { return Text("Unknown AutoRig input"); }, 1061);
    if (!id.empty()) {
        enforce(evidence.contains(id), [&] { return Text("Unknown AutoRig evidence: ") + id; }, 1063);
        const auto& record = evidence.at(id);
        bool compatible = record.task == task && record.port == port;
        for (const auto connection : spec.connections)
            if (connection.input.view() == port.view() && record.task.view() == connection.sourceTask.view() &&
                record.port.view() == connection.sourceOutput.view()) compatible = true;
        enforce(compatible && portKind(spec.inputs, port) == record.value.kind,
            [&] { return Text("AutoRig evidence does not match the input port: ") + port; }, 1068);
    }
    invalidateTask(task);
    if (id.empty()) {
        suppliedEvidence[task].erase(port);
        suppliedInputs[task].erase(port);
    } else {
        suppliedEvidence[task][port] = id;
        suppliedInputs[task][port] = evidence.at(id).value;
    }
    ++inputRevisions[task];
}
AutoRigArtifact AutoRigSession::outputArtifact(Text id, Text port) {
    std::lock_guard lock(mutex_);
    const auto evidenceId = outputEvidence(id, port);
    enforce(!evidenceId.empty() && evidenceArtifacts.contains(evidenceId),
        [&] { return Text("Unknown committed AutoRig artifact: ") + port; }, 1066);
    return evidenceArtifacts.at(evidenceId);
}
bool AutoRigInputContext::findConnection(Text port, AutoRigConnection& connection) {
    connection = {};
    const auto spec = compat::ngRequireObject(session_)->taskSpec(taskId_); enforce(hasPort(spec.inputs, port), [&] { return Text("Unknown AutoRig input"); }, 1084);
    for (const auto item : spec.connections) if (item.input.view() == port.view()) { connection = item; return true; }
    return false;
}
compat::Slice<AutoRigPortSpec> AutoRigInputContext::ports() {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    return session_->taskSpec(taskId_).inputs;
}
bool AutoRigInputContext::isConnected(Text port) {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    AutoRigConnection source; return findConnection(port, source);
}
bool AutoRigInputContext::canEdit(Text port) {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    return !session_->isBusy() && !isConnected(port);
}
bool AutoRigInputContext::hasValue(Text port) {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    return !session_->inputEvidence(taskId_, port).empty();
}
AutoRigValue AutoRigInputContext::value(Text port) {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    return session_->readEvidence(session_->inputEvidence(taskId_, port));
}
void AutoRigInputContext::setValue(Text port, AutoRigValue value) {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    session_->setInput(taskId_, port, std::move(value));
}
compat::Slice<AutoRigPortSpec> AutoRigOutputContext::ports() {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    return session_->taskSpec(taskId_).outputs;
}
AutoRigTaskSnapshot AutoRigOutputContext::snapshot() {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    return session_->task(taskId_);
}
bool AutoRigOutputContext::hasValue(Text port) {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    return session_->hasOutput(taskId_, port);
}
AutoRigValue AutoRigOutputContext::value(Text port) {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    return session_->output(taskId_, port);
}
AutoRigArtifact AutoRigOutputContext::artifact(Text port) {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    return session_->outputArtifact(taskId_, port);
}
}
