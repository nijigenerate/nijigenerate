#include "framework.hpp"
#include "detail/framework_runtime.hpp"

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
    for (auto& [id, snapshot] : snapshots) snapshot.artifacts = {};
}
void AutoRigSession::setContextValue(Text name, AutoRigValue value) {
    std::lock_guard lock(mutex_); enforce(!busy, [&] { return Text("Cannot change AutoRig context during execution"); }, 858);
    enforce(!name.empty(), [&] { return Text("AutoRig context name is empty"); }, 859); sessionValues[name] = ngCopyAutoRigValue(value);
    ++contextRevision_;
    for (auto& [id, snapshot] : snapshots) if (snapshot.state == AutoRigTaskState::Succeeded) snapshot.state = AutoRigTaskState::Stale;
    committedOutputs.reset();
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
    enforce(specs.contains(id), [&] { return Text("Unknown AutoRig task"); }, 894); invalidateDependents(id); committedOutputs.erase(id);
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
    suppliedInputs[id][port] = ngCopyAutoRigValue(value); ++inputRevisions[id];
    if (committedOutputs.contains(id)) invalidateDependents(id);
    committedOutputs.erase(id);
    auto snapshot = aaAt(snapshots, id, 916);
    if (snapshot.state == AutoRigTaskState::Succeeded) snapshot.state = AutoRigTaskState::Stale;
    snapshots[id] = snapshot;
}
AutoRigTaskSnapshot AutoRigSession::task(Text id, bool include) {
    std::lock_guard lock(mutex_); const auto found = snapshots.find(id); enforce(found != snapshots.end(), [&] { return Text("Unknown AutoRig task"); }, 980);
    auto result = found->second; result.artifacts = include ? result.artifacts.duplicate() : compat::Slice<AutoRigArtifact>{}; return result;
}
AutoRigTaskSpec AutoRigSession::taskSpec(Text id) {
    const auto found = specs.find(id); enforce(found != specs.end(), [&] { return Text("Unknown AutoRig task"); }, 989);
    auto result = found->second; result.dependencies = result.dependencies.duplicate(); result.inputs = result.inputs.duplicate();
    result.outputs = result.outputs.duplicate(); result.connections = result.connections.duplicate(); return result;
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
AutoRigArtifact AutoRigSession::outputArtifact(Text id, Text port) {
    std::lock_guard lock(mutex_); enforce(committedOutputs.contains(id), [&] { return Text("AutoRig task has no committed outputs"); }, 1058);
    const auto found = snapshots.find(id); enforce(found != snapshots.end(), [&] { return Text("Unknown AutoRig task"); }, 1061);
    for (const auto artifact : found->second.artifacts)
        if (artifact.portId == port && artifact.committed && !artifact.preview) return artifact;
    throw compat::ThrowableException(Text::fromStaticLiteral("Unknown committed AutoRig artifact: ") + port,
        Text::fromStaticLiteral("source/nijigenerate/autorig/framework.d"), 1066);
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
    AutoRigConnection source; if (findConnection(port, source)) return session_->hasOutput(str(source.sourceTask), str(source.sourceOutput));
    return session_->hasSuppliedInput(taskId_, port);
}
AutoRigValue AutoRigInputContext::value(Text port) {
    const auto sessionOwner = compat::ngRequireObject(session_)->weak_from_this().lock();
    AutoRigConnection source; if (findConnection(port, source)) return session_->output(str(source.sourceTask), str(source.sourceOutput));
    return session_->suppliedInput(taskId_, port);
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
