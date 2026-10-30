#include "framework.hpp"
#include "detail/framework_runtime.hpp"

namespace nijigenerate::autorig::framework {
using namespace detail;
AutoRigTaskContext::AutoRigTaskContext(AutoRigTaskSpec value, Text directory,
    Table<AutoRigValue> supplied, compat::SharedDelegate<bool()> cancel,
    compat::SharedDelegate<void(AutoRigArtifact)> preview, AutoRigEditorDispatcher dispatcher)
    : spec(std::move(value)), attemptDirectory(std::move(directory)), inputs(std::move(supplied)),
      canceled(std::move(cancel)), onPreview(std::move(preview)), editorDispatcher(std::move(dispatcher)) {
    prepareSourceClassInfo();
}
AutoRigPortSpec AutoRigTaskContext::requireOutput(Text id, AutoRigValueKind kind) {
    for (const auto port : spec.outputs) if (port.id.view() == id.view()) {
        enforce(port.kind == kind, [&] { return Text("AutoRig output kind mismatch: " + id); }, 444);
        enforce(!outputs.contains(id), [&] { return Text("AutoRig output already published: " + id); }, 445); return port;
    }
    throw compat::ThrowableException(Text::fromStaticLiteral("Unknown AutoRig output: ") + id,
        Text::fromStaticLiteral("source/nijigenerate/autorig/framework.d"), 448);
}
void AutoRigTaskContext::publish(AutoRigPortSpec port, AutoRigValue value, Text storagePath) {
    const auto id = str(port.id); outputs[id] = value;
    AutoRigArtifact item; item.taskId = str(spec.id); item.portId = id; item.kind = value.kind;
    item.storagePath = std::move(storagePath); item.mediaType = str(port.mediaType); item.viewHint = str(port.viewHint);
    item.value(std::move(value)); artifacts[id] = std::move(item);
}
std::shared_ptr<AutoRigWorkspace> AutoRigTaskContext::workspace(Text name,
    compat::SharedDelegate<std::shared_ptr<AutoRigWorkspace>()> create) {
    auto self = shared_from_this(); auto provider = workspaceProvider;
    enforce(static_cast<bool>(provider), [&] { return Text("AutoRig workspace is unavailable"); }, 462);
    return provider(std::move(name), std::move(create));
}
AutoRigValue AutoRigTaskContext::contextValue(Text name) {
    auto value = sessionValues.find(name);
    enforce(value != sessionValues.end(), [&] { return Text("Missing AutoRig session context: " + name); }, 468);
    return ngCopyAutoRigValue(value->second);
}
compat::Slice<Text> AutoRigTaskContext::contextNames() { return keys(sessionValues); }
bool AutoRigTaskContext::hasInput(Text id) { return inputs.contains(id); }
AutoRigValue AutoRigTaskContext::input(Text id) {
    auto value = inputs.find(id); enforce(value != inputs.end(), [&] { return Text("Missing AutoRig input: " + id); }, 478);
    return ngCopyAutoRigValue(value->second);
}
bool AutoRigTaskContext::isCanceled() {
    auto self = shared_from_this(); auto check = canceled; return check && check();
}
void AutoRigTaskContext::reportFailure(Text message) {
    enforce(spec.retainFailureOutputs && !message.empty(), [&] { return Text("Task does not support failure checkpoints"); }, 485);
    reportedFailure = std::move(message);
}
void AutoRigTaskContext::runOnMainThread(Action action) {
    auto self = shared_from_this(); enforce(static_cast<bool>(action), [&] { return Text("AutoRig editor action is null"); }, 490);
    auto dispatch = editorDispatcher; if (dispatch) dispatch(std::move(action)); else action();
}
Text AutoRigTaskContext::outputPath(Text id, Text extension) {
    requireOutput(id, AutoRigValueKind::Path);
    enforce(extension.empty() || (extension.view()[0] == '.' && extension.size() <= 20 &&
        safeId(extension.view().substr(1))), [&] { return Text("Invalid AutoRig output extension"); }, 497);
    return joined(attemptDirectory, id + extension);
}
void AutoRigTaskContext::publishPath(Text id, Text path) {
    auto self = shared_from_this();
    auto port = requireOutput(id, AutoRigValueKind::Path);
    enforce(fileExists(path), [&] { return Text("AutoRig output path does not exist: " + id); }, 505);
    enforce(path == outputPath(id) || (path.size() > outputPath(id).size() && [&] {
        const auto prefixLength = outputPath(id).size();
        if (prefixLength > path.size()) throw compat::ngStaticError<compat::ArraySliceError>(
            0, prefixLength, path.size(), Text::fromStaticLiteral("source/nijigenerate/autorig/framework.d"), 508);
        const auto prefix = compat::RetainedStringSlice(path).slice(0, prefixLength);
        if (prefix.view() != outputPath(id).view()) return false;
        const auto index = outputPath(id).size();
        if (index >= path.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, path.size(),
            Text::fromStaticLiteral("source/nijigenerate/autorig/framework.d"), 509);
        return path.view()[index] == '.';
    }()), [&] { return Text("AutoRig output path is not allocated by the session"); }, 507);
    publish(port, AutoRigValue::path(path), path);
}
void AutoRigTaskContext::publishFileName(Text id, Text name) {
    auto port = requireOutput(id, AutoRigValueKind::FileName);
    enforce(safeFileName(name), [&] { return Text("Invalid AutoRig file name"); }, 515); publish(port, AutoRigValue::fileName(name), {});
}
void AutoRigTaskContext::publishJson(Text id, Json value) {
    const auto port = requireOutput(id, AutoRigValueKind::Json);
    publish(port, AutoRigValue::jsonSnapshot(value), {});
}
void AutoRigTaskContext::publishBlob(Text id, compat::Slice<std::uint8_t> value) {
    const auto port = requireOutput(id, AutoRigValueKind::Blob);
    publish(port, AutoRigValue::blob(value), {});
}
void AutoRigTaskContext::previewJson(Text id, Json value, Text mediaType) {
    previewValue(std::move(id), AutoRigValue::jsonSnapshot(value), std::move(mediaType));
}
void AutoRigTaskContext::previewBlob(Text id, compat::Slice<std::uint8_t> value, Text mediaType) {
    previewValue(std::move(id), AutoRigValue::blob(value), std::move(mediaType));
}
void AutoRigTaskContext::previewValue(Text id, AutoRigValue value, Text mediaType) {
    auto self = shared_from_this(); enforce(safeId(id), [&] { return Text("Invalid AutoRig preview name"); }, 536);
    AutoRigArtifact item; item.taskId = str(spec.id); item.portId = std::move(id); item.kind = value.kind;
    item.mediaType = std::move(mediaType); item.preview = true; item.value(std::move(value));
    auto callback = onPreview; if (callback) callback(item);
}
Text AutoRigTaskContext::previewPath(Text id, Text extension) {
    enforce(safeId(id) && extension.size() > 1 && extension.view()[0] == '.' &&
        safeId(extension.view().substr(1)), [&] { return Text("Invalid AutoRig preview name"); }, 544);
    auto location = joined(attemptDirectory, "preview-" + id + "-" + std::to_string(++previewVersion) + extension);
    reservedPreviews[id] = location; return location;
}
void AutoRigTaskContext::publishPreview(Text id, Text path, Text mediaType) {
    auto self = shared_from_this(); auto reserved = reservedPreviews.find(id);
    enforce(reserved != reservedPreviews.end() && reserved->second == path && fileExists(path), [&] { return Text("AutoRig preview path was not allocated or written"); }, 554);
    reservedPreviews.erase(id); auto callback = onPreview;
    if (callback) {
        AutoRigArtifact item; item.taskId = str(spec.id); item.portId = std::move(id); item.kind = AutoRigValueKind::Path;
        item.storagePath = path; item.mediaType = std::move(mediaType); item.preview = true;
        item.value(AutoRigValue::path(std::move(path))); callback(item);
    }
}
}
