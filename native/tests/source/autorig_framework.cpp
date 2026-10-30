// Whole original tests/autorig_framework.d; no additional test cases.
#include "autorig_framework.hpp"
#include <nijigenerate/autorig/framework.hpp>
#include <nijigenerate/autorig/workflow.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <nijigenerate/compat/phobos_file_write.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <nijigenerate/compat/phobos_monotime.hpp>
#include <nijigenerate/compat/thread_exception.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include <chrono>
#include <exception>
#include <initializer_list>
#include <memory>
#include <semaphore>
#include <source_location>
#include <thread>
#include <utility>

namespace autorig_framework_test {
namespace {
namespace compat = nijigenerate::compat;
namespace settings = nijigenerate::core::settings;
namespace threads = nijigenerate::core::thread_context;
namespace format = nijigenerate::core::translation;
using namespace nijigenerate::autorig::framework;
using namespace nijigenerate::autorig::workflow;
using Json = settings::SettingsJsonValue;
using Array = settings::SettingsJsonArray;
using Bytes = compat::Slice<std::uint8_t>;
template<class T> using Ptr = std::shared_ptr<T>;
template<class T> using Slice = compat::Slice<T>;
template<std::size_t N> Text text(const char (&value)[N]) { return Text::fromStaticLiteral(value); }
template<class T, class... Args> Ptr<T> create(Args&&... args) {
    return std::allocate_shared<T>(SourceAllocator<T>{}, std::forward<Args>(args)...);
}
template<class T, std::size_t N> void prepareOriginalClass(const char (&name)[N]) {
    static const bool prepared = [&] {
        try {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(T), text(name));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return true;
    }();
    (void)prepared;
}
void require(std::size_t sourceLine, bool condition, Text message = text("Assertion failure")) {
    if (!condition) throw compat::ngStaticError<compat::AssertError>(message,
        text("tests/autorig_framework.d"), sourceLine);
}
Json get(Json value, std::string_view name) { return value.sourceIndexValue(name); }
std::int64_t integer(Json value) { return value.as_int64(); }
Text string(Json value) { return value.as_string().retainedString(); }
Json object(std::initializer_list<std::pair<Text, Json>> members) {
    settings::SettingsJsonObject result;
    for (const auto& [name, value] : members) result[name] = value;
    return Json(result);
}
template<class Produce> void set(Json& target, Text name, Produce&& produce) {
    target.sourceIndexAssign(settings::SettingsString(name),
        [&](Json& slot) { slot = produce(); },
        [&] { return produce(); });
}
bool equal(Bytes left, Bytes right) {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) if (left[i] != right[i]) return false;
    return true;
}
template<class Range> bool bytesEqual(const Range& left, std::initializer_list<std::uint8_t> right) {
    if (left.size() != right.size()) return false;
    std::size_t i = 0;
    for (const auto value : right) if (left[i++] != value) return false;
    return true;
}
Text decimal(std::int64_t value) {
    const std::array arguments{format::ngFormatArgument(value)};
    return Text(format::ngFormatTyped("%s", arguments));
}
Text path(Text a, Text b) {
    const std::array segments{a, b};
    return compat::phobos::ngBuildPath(segments);
}
bool exists(Text value) { return compat::phobos::ngFileExists(value.view()); }
void writeln(std::string_view value) {
    nijigenerate::providers::mcp::stdio::standardOutput().writeln(value);
}
template<class... Values> void writefln(std::string_view pattern, Values... values) {
    using format::ngFormatArgument;
    const std::array<format::TypedFormatArgument, sizeof...(Values)> arguments{ngFormatArgument(values)...};
    writeln(format::ngFormatTyped(pattern, arguments));
}
class Stopwatch {
    compat::SourceMonoTime start_;
    compat::SourceMonoTime::Duration elapsed_{};
public:
    void start() { start_ = compat::SourceMonotonicClock::now(); }
    void stop() { elapsed_ += compat::SourceMonotonicClock::now() - start_; }
    auto microseconds() const { return std::chrono::duration_cast<std::chrono::microseconds>(elapsed_).count(); }
};
class Worker {
    Action action_;
    std::thread worker_;
    std::exception_ptr failure_;
public:
    explicit Worker(Action action) : action_(std::move(action)) {}
    void start() {
        try {
            worker_ = std::thread([this] {
                try { threads::ManagedThreadScope attached; action_(); }
                catch (...) { failure_ = std::current_exception(); }
            });
        } catch (const std::system_error&) { compat::ngOnThreadError(text("Error creating thread")); }
    }
    void join() {
        try { worker_.join(); }
        catch (const std::system_error&) {
            compat::ngThrowThreadException(text("Unable to join thread"),
                text("core/thread/osthread.d"),
#ifdef _WIN32
                606);
#else
                617);
#endif
        }
        if (failure_) std::rethrow_exception(failure_);
    }
    ~Worker() { if (worker_.joinable()) worker_.join(); }
};
AutoRigPortSpec port(Text id, AutoRigValueKind kind) { return {id, kind}; }
AutoRigTaskSpec task(Text id, Text label, Slice<compat::RetainedStringSlice> dependencies = {},
        Slice<AutoRigPortSpec> inputs = {}, Slice<AutoRigPortSpec> outputs = {},
        Slice<AutoRigConnection> connections = {}) {
    return {id, label, dependencies, inputs, outputs, connections};
}

class TestRigProcessor : public AutoRigProcessor, public IAutoRigInputEditor, public IAutoRigOutputViewer,
        public IAutoRigSessionEditor, public IAutoRigSessionViewer {
protected:
    void prepareSourceClassInfo() override {
        AutoRigProcessor::prepareSourceClassInfo();
        prepareOriginalClass<TestRigProcessor>("autorig_framework_test.TestRigProcessor");
    }
public:
    bool sawSessionEditor = false, sawSessionViewer = false;
    int prepareCalls = 0, finishCalls = 0, editorCalls = 0;
    bool sawWorkerThread = false, failFinish = false, cyclic = false, invalidWorkflow = false;
    bool unboundWorkflowInput = false, cancelPrepare = false, checkContext = false;
    Ptr<AutoRigSession> cancelSession;
    Text lastUITaskId, lastUIInstanceId;
    bool sawConnectedInput = false, sawOutputArtifact = false;
    Ptr<AutoRigWorkflowRun> runningWorkflow;
    Text procId() override { return text("test-rig"); }
    Text displayName() override { return text("Test rig"); }
    void configureSession(Text workflowId, Ptr<IAutoRigSessionEditContext> context) override {
        require(37, workflowId == text("prepare-only") && context->workflowId() == workflowId);
        require(38, !context->runId().empty() && context->canEdit());
        require(39, context->hasInput("source") && integer(context->input("source").json) == 4);
        context->setContextValue("ui", AutoRigValue::jsonValue(Json(7)));
        sawSessionEditor = true;
    }
    void viewSession(Text workflowId, Ptr<IAutoRigSessionViewContext> context) override {
        require(45, workflowId == text("prepare-only") && context->workflowId() == workflowId);
        require(46, integer(context->contextValue("ui").json) == 7);
        require(47, context->outputPorts().size() == 1 && context->hasOutput("data"));
        require(48, integer(context->output("data").json) == 5);
        require(49, context->outputArtifact("data").storagePath.empty());
        require(50, integer(context->outputArtifact("data").value().json) == 5);
        sawSessionViewer = true;
    }
    Slice<AutoRigWorkflowSpec> workflows() override {
        AutoRigWorkflowSpec cross;
        cross.id = "cross-plugin"; cross.label = "Prepare and consume";
        cross.inputs = {port("source", AutoRigValueKind::Json)};
        cross.outputs = {port("result", AutoRigValueKind::Blob)};
        cross.steps = {{text("consume"), text("other-rig"), text("consume")},
            {text("prepare"), text("test-rig"), text("prepare")}};
        cross.inputBindings = {{text("source"), text("prepare"), {}, text("source")}};
        cross.connections = {{text("prepare"), text("data"), text("consume"), {}, text("data")}};
        cross.outputBindings = {{text("result"), text("consume"), text("result")}};
        AutoRigWorkflowSpec simple;
        simple.id = "prepare-only"; simple.label = "Prepare";
        simple.inputs = {port("source", AutoRigValueKind::Json)};
        simple.outputs = {port("data", AutoRigValueKind::Json)};
        simple.steps = {{text("prepare"), text("test-rig"), text("prepare")}};
        simple.inputBindings = {{text("source"), text("prepare"), {}, text("source")}};
        simple.outputBindings = {{text("data"), text("prepare"), text("data")}};
        auto diagnostics = simple;
        diagnostics.id = "verify-without-report";
        diagnostics.steps = {{text("verify"), text("test-rig"), text("prepare")}};
        diagnostics.inputBindings = {{text("source"), text("verify"), {}, text("source")}};
        diagnostics.outputBindings = {{text("data"), text("verify"), text("data")}};
        AutoRigWorkflowSpec internal;
        internal.id = "finish-internal"; internal.label = "Finish with internal dependency";
        internal.inputs = {port("source", AutoRigValueKind::Json)};
        internal.outputs = {port("model", AutoRigValueKind::Blob)};
        internal.steps = {{text("finish"), text("test-rig"), text("finish")}};
        internal.inputBindings = {{text("source"), text("finish"), text("prepare"), text("source")}};
        internal.outputBindings = {{text("model"), text("finish"), text("model")}};
        AutoRigWorkflowSpec artifacts;
        artifacts.id = "all-value-kinds"; artifacts.label = "Transfer every value kind";
        artifacts.inputs = {port("source", AutoRigValueKind::Json)};
        artifacts.outputs = {port("summary", AutoRigValueKind::Json)};
        artifacts.steps = {{text("prepare"), text("test-rig"), text("prepare")},
            {text("path"), text("test-rig"), text("emit-path")},
            {text("inspect"), text("other-rig"), text("inspect-artifacts")}};
        artifacts.inputBindings = {{text("source"), text("prepare"), {}, text("source")},
            {text("source"), text("path"), {}, text("source")}};
        artifacts.connections = {{text("prepare"), text("data"), text("inspect"), {}, text("data")},
            {text("prepare"), text("binary"), text("inspect"), {}, text("binary")},
            {text("prepare"), text("name"), text("inspect"), {}, text("name")},
            {text("path"), text("model"), text("inspect"), {}, text("model")}};
        artifacts.outputBindings = {{text("summary"), text("inspect"), text("summary")}};
        if (invalidWorkflow) {
            auto invalid = cross; invalid.id = "invalid-kind";
            invalid.connections = {{text("prepare"), text("binary"), text("consume"), {}, text("data")}};
            return {cross, simple, internal, artifacts, invalid};
        }
        if (unboundWorkflowInput) {
            auto invalid = simple; invalid.id = "unbound-input";
            invalid.inputs = simple.inputs.duplicate();
            invalid.inputs.append(port("unused", AutoRigValueKind::Json));
            return {cross, simple, internal, artifacts, invalid};
        }
        return {cross, simple, internal, artifacts, diagnostics};
    }
    Slice<AutoRigTaskSpec> tasks() override {
        if (cyclic) return {task("a", "A", {text("b")}), task("b", "B", {text("a")})};
        return {task("prepare", "Prepare", {}, {port("source", AutoRigValueKind::Json)},
            {port("data", AutoRigValueKind::Json), port("binary", AutoRigValueKind::Blob),
                port("name", AutoRigValueKind::FileName)}),
            task("finish", "Finish", {text("prepare")}, {port("data", AutoRigValueKind::Json)},
                {port("model", AutoRigValueKind::Blob)}, {{text("data"), text("prepare"), text("data")}}),
            task("emit-path", "Emit path", {}, {port("source", AutoRigValueKind::Json)},
                {port("model", AutoRigValueKind::Path)})};
    }
    void executeTask(Text taskId, Ptr<AutoRigTaskContext> context) override {
        require(147, nijigenerate::core::tasks::TaskFiber::hasCurrent());
        if (!threads::ngIsMainThread()) sawWorkerThread = true;
        nijigenerate::core::tasks::TaskFiber::yield();
        context->runOnMainThread([this] { ++editorCalls; });
        if (taskId == text("prepare")) {
            if (runningWorkflow) require(153, runningWorkflow->snapshot(false).state == AutoRigWorkflowState::Running);
            if (checkContext) {
                require(155, context->contextValue("artifact").text == text("reference.json"));
                auto metadata = context->contextValue("metadata").json;
                require(157, string(get(metadata, "label")) == text("original"));
                set(metadata, "label", [&] { return Json("task edit"); });
                require(159, string(get(context->contextValue("metadata").json, "label")) == text("original"));
                auto bytes = context->contextValue("bytes").readBlob();
                require(161, bytesEqual(bytes, {1, 2})); bytes[0] = 9;
                require(163, context->contextValue("bytes").readBlob()[0] == 1);
                bool rejected = false;
                try { cancelSession->setContextValue("artifact", AutoRigValue::path("changed")); }
                catch (const compat::ThrowableException&) { rejected = true; }
                require(167, rejected);
                context->previewBlob("image", Bytes{3, 4}, "image/png");
            }
            ++prepareCalls;
            auto source = integer(context->input("source").json);
            context->previewJson("progress", Json(source));
            if (cancelPrepare) { cancelSession->cancel(); require(175, context->isCanceled()); return; }
            context->publishJson("data", Json(source + 1));
            context->publishBlob("binary", Bytes{0, 1, 2});
            context->publishFileName("name", "source.txt");
        } else if (taskId == text("finish")) {
            ++finishCalls;
            auto value = integer(context->input("data").json);
            if (failFinish) throw compat::ThrowableException(text("expected failure"), text("tests/autorig_framework.d"), 184);
            auto valueText = decimal(value);
            Bytes bytes;
            for (const auto byte : valueText.view()) bytes.append(static_cast<std::uint8_t>(byte));
            context->publishBlob("model", bytes);
        } else if (taskId == text("emit-path")) {
            auto value = integer(context->input("source").json);
            auto location = context->outputPath("model", ".txt");
            compat::phobos::ngMkdirRecurse(compat::phobos::directory_detail::dirName(location.view()));
            compat::phobos::ngFileWrite(location, decimal(value).view());
            context->publishPath("model", location);
            auto preview = context->previewPath("path-preview", ".txt");
            compat::phobos::ngFileWrite(preview, "preview");
            context->publishPreview("path-preview", preview, "text/plain");
        } else throw compat::ThrowableException(text("Unknown test task"), text("tests/autorig_framework.d"), 200);
    }
    void configureInput(Text taskId, Ptr<IAutoRigInputContext> context) override {
        lastUITaskId = taskId; lastUIInstanceId = context->taskId();
        if (taskId == text("prepare")) {
            require(208, context->ports().size() == 1 && context->canEdit("source"));
            if (!context->hasValue("source")) context->setValue("source", AutoRigValue::jsonValue(Json(3)));
        } else if (taskId == text("finish"))
            sawConnectedInput = !context->canEdit("data") && context->isConnected("data") &&
                context->hasValue("data") && integer(context->value("data").json) > 0;
    }
    void viewOutput(Text taskId, Ptr<IAutoRigOutputContext> context) override {
        lastUITaskId = taskId; lastUIInstanceId = context->taskId();
        if (taskId == text("prepare")) {
            auto snapshot = context->snapshot();
            sawOutputArtifact = snapshot.state == AutoRigTaskState::Succeeded && !snapshot.artifacts.empty() &&
                context->hasValue("data") && context->artifact("data").storagePath.empty() &&
                integer(context->artifact("data").value().json) > 0;
        }
    }
};

class OtherRigProcessor : public AutoRigProcessor, public IAutoRigInputEditor, public IAutoRigOutputViewer {
protected:
    void prepareSourceClassInfo() override {
        AutoRigProcessor::prepareSourceClassInfo();
        prepareOriginalClass<OtherRigProcessor>("autorig_framework_test.OtherRigProcessor");
    }
public:
    int calls = 0;
    bool fail = false, cancelNow = false;
    Ptr<AutoRigWorkflowRun> activeWorkflow;
    Text lastUITaskId;
    bool sawConnectedInput = false, sawOutput = false;
    Text procId() override { return "other-rig"; }
    Text displayName() override { return "Other rig"; }
    Slice<AutoRigTaskSpec> tasks() override {
        return {task("consume", "Consume", {}, {port("data", AutoRigValueKind::Json)},
            {port("result", AutoRigValueKind::Blob)}),
            task("inspect-artifacts", "Inspect artifacts", {},
                {port("data", AutoRigValueKind::Json), port("binary", AutoRigValueKind::Blob),
                    port("name", AutoRigValueKind::FileName), port("model", AutoRigValueKind::Path)},
                {port("summary", AutoRigValueKind::Json)})};
    }
    void executeTask(Text taskId, Ptr<AutoRigTaskContext> context) override {
        require(255, nijigenerate::core::tasks::TaskFiber::hasCurrent());
        nijigenerate::core::tasks::TaskFiber::yield();
        ++calls;
        if (taskId == text("inspect-artifacts")) {
            settings::SettingsJsonObject summary;
            summary[settings::SettingsString("data")] = context->input("data").json;
            summary[settings::SettingsString("byteCount")] =
                Json(static_cast<int>(context->input("binary").readBlob().size()));
            summary[settings::SettingsString("name")] =
                Json(settings::SettingsString(context->input("name").text));
            summary[settings::SettingsString("model")] = Json(settings::SettingsString(
                compat::phobos::ngReadUtf8Text(context->input("model").text)));
            context->publishJson("summary", Json(summary)); return;
        }
        require(267, taskId == text("consume"));
        if (fail) throw compat::ThrowableException(text("expected workflow failure"), text("tests/autorig_framework.d"), 268);
        if (cancelNow) { activeWorkflow->cancel(); require(271, context->isCanceled()); return; }
        auto value = static_cast<std::uint8_t>(integer(context->input("data").json));
        context->publishBlob("result", Bytes{value});
    }
    void configureInput(Text taskId, Ptr<IAutoRigInputContext> context) override {
        lastUITaskId = taskId;
        if (taskId == text("consume")) sawConnectedInput = context->isConnected("data") && context->hasValue("data");
    }
    void viewOutput(Text taskId, Ptr<IAutoRigOutputContext> context) override {
        lastUITaskId = taskId;
        if (taskId == text("consume")) sawOutput = context->hasValue("result") && context->value("result").readBlob().size() == 1;
    }
};

void testSessionContext() {
    auto processor = create<TestRigProcessor>(); processor->checkContext = true;
    auto manager = create<AutoRigSessionManager>(path("out", "autorig-session-context-runs"));
    manager->registerProcessor(processor);
    manager->registerProcessorAlias("previous-test-rig", "test-rig");
    require(297, manager->processor("previous-test-rig") == processor);
    require(298, manager->listProcessors().size() == 1);
    auto workflows = create<AutoRigWorkflowManager>(manager);
    auto run = workflows->create("test-rig", "prepare-only");
    processor->cancelSession = run->session();
    run->setInput("source", AutoRigValue::jsonValue(Json(4)));
    auto inputRevision = run->inputRevision(); require(304, inputRevision > 0);
    auto taskInputRevision = run->session()->inputRevision(run->stepTaskId("prepare"));
    require(306, taskInputRevision > 0);
    run->setInput("source", AutoRigValue::jsonValue(Json(4)));
    require(308, run->inputRevision() > inputRevision);
    require(309, run->session()->inputRevision(run->stepTaskId("prepare")) > taskInputRevision);
    auto source = run->input("source");
    require(311, integer(source.json) == 4 && run->inputPorts().size() == 1);
    auto metadata = object({{text("label"), Json("original")}});
    run->setContextValue("metadata", AutoRigValue::jsonValue(metadata));
    set(metadata, "label", [&] { return Json("caller edit"); });
    run->setContextValue("bytes", AutoRigValue::blob(Bytes{1, 2}));
    auto info = run->session()->contextInfo("bytes");
    require(317, info.kind == AutoRigValueKind::Blob && info.byteLength == 2 && info.text.empty());
    auto revision = info.revision;
    run->setContextValue("bytes", AutoRigValue::blob(Bytes{1, 2}));
    require(320, run->session()->contextInfo("bytes").revision > revision);
    run->setContextValue("artifact", AutoRigValue::path("reference.json"));
    require(322, run->renderInputUI() && processor->sawSessionEditor);
    Worker worker([&] { run->execute(); }); worker.start(); worker.join();
    require(325, integer(run->output("data").json) == 5);
    require(326, !exists(run->directory()));
    auto artifact = run->outputArtifact("data");
    require(328, integer(artifact.value().json) == 5 && artifact.storagePath.empty());
    auto previews = run->session()->task(run->stepTaskId("prepare")).artifacts;
    require(330, previews[0].preview && bytesEqual(previews[0].value().readBlob(), {3, 4}));
    require(331, previews[0].storagePath.empty() && previews[0].mediaType == text("image/png"));
    require(332, previews[1].preview && integer(previews[1].value().json) == 4);
    require(333, run->snapshot(false).steps.at("prepare").artifacts.empty());
    require(334, !run->snapshot().steps.at("prepare").artifacts.empty());
    require(335, run->renderOutputUI() && processor->sawSessionViewer);
    bool rejectedWorkerUI = false;
    Worker uiWorker([&] {
        try { run->renderInputUI(); }
        catch (const compat::ThrowableException&) { rejectedWorkerUI = true; }
    });
    uiWorker.start(); uiWorker.join(); require(342, rejectedWorkerUI);
    auto id = run->id(); workflows->close(id);
    auto restored = workflows->reopen(id); processor->cancelSession = restored->session();
    require(347, integer(restored->input("source").json) == 4);
    require(348, bytesEqual(restored->session()->contextValue("bytes").readBlob(), {1, 2}));
    require(349, string(get(restored->session()->contextValue("metadata").json, "label")) == text("original"));
    auto calls = processor->prepareCalls; restored->execute(); require(352, processor->prepareCalls == calls);
    restored->setContextValue("artifact", AutoRigValue::path("reference.json"));
    require(354, restored->session()->task(restored->stepTaskId("prepare")).state == AutoRigTaskState::Stale);
    workflows->close(id); restored = workflows->reopen(id); processor->cancelSession = restored->session();
    require(358, restored->session()->task(restored->stepTaskId("prepare")).state == AutoRigTaskState::Stale);
    restored->execute(); require(360, processor->prepareCalls == calls + 1);
    workflows->remove(id);
    bool deleted = false;
    try { workflows->reopen(id); } catch (const compat::ThrowableException&) { deleted = true; }
    require(365, deleted);
    bool unregistered = false;
    try { manager->get(id); } catch (const compat::ThrowableException&) { unregistered = true; }
    require(369, unregistered);
    auto closed = workflows->create("test-rig", "prepare-only"); auto closedId = closed->id();
    workflows->close(closedId); workflows->remove(closedId); deleted = false;
    try { workflows->reopen(closedId); } catch (const compat::ThrowableException&) { deleted = true; }
    require(377, deleted);
    writeln("AutoRig session context checks passed");
}

void benchmarkUIMetadata() {
    auto manager = create<AutoRigSessionManager>(path("out", "autorig-ui-benchmark"));
    manager->registerProcessor(create<TestRigProcessor>());
    auto session = manager->create("test-rig");
    constexpr std::size_t payloadSize = 16 * 1024 * 1024;
    Bytes payload; payload.resize(payloadSize);
    session->setContextValue("model", AutoRigValue::blob(payload));
    std::size_t copied = 0, metadata = 0;
    Stopwatch oldTimer; oldTimer.start();
    for (int i = 0; i < 32; ++i) copied += session->contextValue("model").bytes().size();
    oldTimer.stop();
    Stopwatch newTimer; newTimer.start();
    for (int i = 0; i < 32; ++i) metadata += session->contextInfo("model").byteLength;
    newTimer.stop();
    require(397, copied == metadata && copied == 32 * payloadSize);
    require(398, !exists(session->directory()));
    writefln("UI payload lookup, 32 reads of 16 MiB: copied=%s us; metadata=%s us; eliminated=%s MiB",
        oldTimer.microseconds(), newTimer.microseconds(), copied / (1024 * 1024));
}

class CheckpointRigProcessor : public AutoRigProcessor {
protected:
    void prepareSourceClassInfo() override {
        AutoRigProcessor::prepareSourceClassInfo();
        prepareOriginalClass<CheckpointRigProcessor>("autorig_framework_test.CheckpointRigProcessor");
    }
public:
    bool fail = true;
    Text procId() override { return "checkpoint-test"; }
    Text displayName() override { return "Checkpoint test"; }
    Slice<AutoRigWorkflowSpec> workflows() override {
        AutoRigWorkflowSpec workflow; workflow.id = "checkpoint-workflow"; workflow.label = "Checkpoint workflow";
        workflow.inputs = {port("input", AutoRigValueKind::Json)};
        workflow.outputs = {port("result", AutoRigValueKind::Json)};
        workflow.steps = {{text("consumer"), procId(), text("consumer")}};
        workflow.inputBindings = {{text("input"), text("consumer"), text("source"), text("input")}};
        workflow.outputBindings = {{text("result"), text("consumer"), text("result")}};
        return {workflow};
    }
    Slice<AutoRigTaskSpec> tasks() override {
        auto source = task("source", "Source", {}, {port("input", AutoRigValueKind::Json)},
            {port("data", AutoRigValueKind::Json)});
        source.retainFailureOutputs = true;
        return {source, task("consumer", "Consumer", {}, {port("input", AutoRigValueKind::Json)},
            {port("result", AutoRigValueKind::Json)}, {{text("input"), text("source"), text("data")}})};
    }
    void executeTask(Text taskId, Ptr<AutoRigTaskContext> context) override {
        if (taskId == text("source")) {
            context->publishJson("data", context->input("input").json);
            if (fail) context->reportFailure("Retained test checkpoint");
        } else context->publishJson("result", context->input("input").json);
    }
};

void testChangedFailureCheckpoint() {
    auto manager = create<AutoRigSessionManager>(path("out", "autorig-checkpoint-tests"));
    auto processor = create<CheckpointRigProcessor>(); manager->registerProcessor(processor);
    auto session = manager->create("checkpoint-test");
    session->setInput("source", "input", AutoRigValue::jsonValue(Json(1)));
    session->execute("consumer");
    require(443, session->task("source").state == AutoRigTaskState::Failed);
    require(444, integer(session->output("consumer", "result").json) == 1);
    session->setInput("source", "input", AutoRigValue::jsonValue(Json(2)));
    require(446, session->task("consumer").state == AutoRigTaskState::Stale);
    session->execute("consumer");
    require(448, integer(session->output("consumer", "result").json) == 2);
    require(449, session->task("source").attempt == 2);
    auto workflows = create<AutoRigWorkflowManager>(manager);
    auto run = workflows->create("checkpoint-test", "checkpoint-workflow");
    run->setInput("input", AutoRigValue::jsonValue(Json(3))); run->execute();
    require(454, run->snapshot().steps.at("consumer").state == AutoRigTaskState::Succeeded);
    require(455, run->snapshot().state == AutoRigWorkflowState::Failed);
    run->execute(true);
    require(457, run->session()->task("8_consumer_source").attempt == 2);
    require(458, run->snapshot().state == AutoRigWorkflowState::Failed);
    processor->fail = false; run->execute();
    require(461, run->snapshot().state == AutoRigWorkflowState::Succeeded);
    require(462, run->session()->task("8_consumer_source").attempt == 3);
    require(463, integer(run->output("result").json) == 3);
}

void testFailedRequestSetup() {
    auto manager = create<AutoRigSessionManager>(path("out", "autorig-setup-tests"));
    manager->registerProcessor(create<CheckpointRigProcessor>());
    class ObservedManager : public AutoRigWorkflowManager {
    protected:
        void prepareSourceClassInfo() override {
            AutoRigWorkflowManager::prepareSourceClassInfo();
            prepareOriginalClass<ObservedManager>("autorig_framework_test.testFailedRequestSetup.ObservedManager");
        }
    public:
        Ptr<AutoRigWorkflowRun> created;
        explicit ObservedManager(Ptr<AutoRigSessionManager> manager) : AutoRigWorkflowManager(manager) {}
        Ptr<AutoRigWorkflowRun> create(Text providerId, Text workflowId) override {
            created = AutoRigWorkflowManager::create(providerId, workflowId); return created;
        }
    };
    auto workflows = create<ObservedManager>(manager);
    auto nonObjectContext = Json(Array{Json(1)});
    auto badKind = object({{text("kind"), Json("Json")}});
    auto badContext = object({{text("bad"), badKind}});
    for (auto context : Array{nonObjectContext, badContext}) {
        bool rejected = false;
        Table<AutoRigValue> input; input["input"] = AutoRigValue::jsonValue(Json(42));
        try { workflows->createConfigured("checkpoint-test", "checkpoint-workflow", input, context); }
        catch (const compat::ThrowableException&) { rejected = true; }
        require(484, rejected);
        bool absent = false;
        try { workflows->get(workflows->created->id()); } catch (const compat::ThrowableException&) { absent = true; }
        require(488, absent); absent = false;
        try { manager->get(workflows->created->id()); } catch (const compat::ThrowableException&) { absent = true; }
        require(492, absent);
        require(493, get(workflows->created->session()->memoryInfo(), "json_bytes").as_uint64() == 0);
    }
}

void testStartupCancellation() {
    auto processor = create<TestRigProcessor>();
    auto manager = create<AutoRigSessionManager>(path("out", "autorig-startup-tests"));
    manager->registerProcessor(processor);
    auto run = manager->create("test-rig");
    run->setInput("prepare", "source", AutoRigValue::jsonValue(Json(1))); run->cancel();
    bool canceled = false;
    try { run->execute("prepare"); }
    catch (const compat::ThrowableException& error) { canceled = error.msg.view().find("canceled") != std::string_view::npos; }
    require(507, canceled && processor->prepareCalls == 0 && !run->isBusy());
    require(508, run->task("prepare").state == AutoRigTaskState::Canceled);
    run->execute("prepare"); require(510, run->task("prepare").state == AutoRigTaskState::Succeeded);
    run->cancel(); try { run->execute("prepare", true); } catch (const compat::ThrowableException&) {}
    require(514, run->task("prepare").state == AutoRigTaskState::Canceled);
    std::counting_semaphore<> entered(0), released(0);
    manager->setEditorDispatcher([&](Action action) { entered.release(); released.acquire(); action(); });
    auto threaded = manager->create("test-rig");
    threaded->setInput("prepare", "source", AutoRigValue::jsonValue(Json(1)));
    Worker worker([&] { try { threaded->execute("prepare"); } catch (const compat::ThrowableException&) {} });
    worker.start(); entered.acquire(); require(528, threaded->isBusy());
    threaded->cancel(); released.release(); worker.join();
    require(532, threaded->task("prepare").state == AutoRigTaskState::Canceled && !threaded->isBusy());
    auto workflows = create<AutoRigWorkflowManager>(manager);
    auto workflow = workflows->create("test-rig", "prepare-only");
    workflow->setInput("source", AutoRigValue::jsonValue(Json(1))); workflow->cancel();
    try { workflow->execute(); } catch (const compat::ThrowableException&) {}
    require(539, workflow->snapshot().state == AutoRigWorkflowState::Canceled);
}

void testExplicitActionBoundaryOwnership() {
    class StageNamedProcessor : public AutoRigProcessor {
    protected:
        void prepareSourceClassInfo() override {
            AutoRigProcessor::prepareSourceClassInfo();
            prepareOriginalClass<StageNamedProcessor>(
                "autorig_framework_test.testExplicitActionBoundaryOwnership.StageNamedProcessor");
        }
    public:
        bool owns = false;
        int mutations = 0;
        Text procId() override { return "third-party"; }
        Text displayName() override { return "Third party"; }
        Slice<AutoRigTaskSpec> tasks() override {
            auto spec = task("mesh-parts", "Third party mesh"); spec.ownsActionBoundary = owns; return {spec};
        }
        void executeTask(Text, Ptr<AutoRigTaskContext> context) override {
            context->runOnMainThread([this] { ++mutations; });
            context->runOnMainThread([this] { ++mutations; });
        }
    };
    for (const auto owns : {false, true}) {
        auto manager = create<AutoRigSessionManager>(text("out/boundary-ownership-tests"));
        auto processor = create<StageNamedProcessor>(); processor->owns = owns;
        int boundaries = 0;
        manager->setTaskActionBoundary([&](Text, Action execute) { ++boundaries; execute(); });
        manager->registerProcessor(processor); manager->create(processor->procId())->execute("mesh-parts");
        require(566, processor->mutations == 2 && boundaries == (owns ? 0 : 1));
    }
}

void testBlobOwnership() {
    const auto baseline = ngAutoRigNativeBlobMemoryInfo();
    Bytes source{1, 2, 3}; auto stored = AutoRigValue::blob(source); source[0] = 9;
    require(574, bytesEqual(stored.readBlob(), {1, 2, 3}));
    auto workerValue = ngCopyAutoRigValue(stored);
    auto readOnlyBytes = workerValue.readImmutableBlob();
    auto workerBytes = workerValue.readBlob(); workerBytes[0] = 8;
    require(579, stored.readBlob()[0] == 1);
    workerValue.bytes()[0] = 7;
    require(581, workerValue.readBlob()[0] == 7 && stored.readBlob()[0] == 1);
    require(582, readOnlyBytes[0] == 1);
    auto editedCopy = ngCopyAutoRigValue(workerValue); workerValue.bytes()[0] = 6;
    require(585, editedCopy.readBlob()[0] == 7);
    stored = {}; workerValue = {}; editedCopy = {};
    // D590 collects before reading the independent immutable copy. The native
    // counterpart checks the actual released payload owners, not GC cycles.
    require(590, ngAutoRigNativeBlobMemoryInfo() == baseline,
        text("Released values must restore native payload count and bytes"));
    require(591, bytesEqual(readOnlyBytes, {1, 2, 3}));
}

void testDependencyScratchCollection() {
    class FailingChainProcessor : public AutoRigProcessor {
    protected:
        void prepareSourceClassInfo() override {
            AutoRigProcessor::prepareSourceClassInfo();
            prepareOriginalClass<FailingChainProcessor>(
                "autorig_framework_test.testDependencyScratchCollection.FailingChainProcessor");
        }
    public:
        Ptr<AutoRigSession> session;
        bool cancelFirst = false;
        int calls = 0;
        Text procId() override { return "failing-chain"; }
        Text displayName() override { return "Failing chain"; }
        Slice<AutoRigTaskSpec> tasks() override {
            Slice<AutoRigTaskSpec> result;
            for (int i = 0; i < 8; ++i) {
                auto spec = task(decimal(i), "Stage"); spec.reclaimScratchMemory = true;
                if (i) spec.dependencies = {decimal(i - 1)};
                result.append(spec);
            }
            return result;
        }
        void executeTask(Text, Ptr<AutoRigTaskContext>) override {
            ++calls;
            if (cancelFirst) session->cancel();
            else throw compat::ThrowableException(text("First stage failed"), text("tests/autorig_framework.d"), 615);
        }
    };
    for (const auto cancelFirst : {false, true}) {
        const auto baseline = ngAutoRigNativeBlobMemoryInfo();
        auto manager = create<AutoRigSessionManager>(text("out/scratch-dependency-tests"));
        auto processor = create<FailingChainProcessor>(); processor->cancelFirst = cancelFirst;
        manager->registerProcessor(processor); processor->session = manager->create(processor->procId());
        bool failed = false;
        try { processor->session->execute("7"); } catch (const compat::ThrowableException&) { failed = true; }
        require(629, failed && processor->calls == 1);
        // D624-630 observes one GC cycle for the first task and none for waiting
        // tasks. There is no native collector. Check the actual terminal task,
        // execution-scope cleanup and live payload totals instead. This case
        // allocates no blobs, so equal totals alone would not prove cleanup.
        const auto first = processor->session->task("0");
        require(630, !processor->session->isBusy() && first.attempt == 1 &&
            first.state == (cancelFirst ? AutoRigTaskState::Canceled : AutoRigTaskState::Failed) &&
            ngAutoRigNativeBlobMemoryInfo() == baseline,
            text("Failed or canceled task scopes must finish without retaining native payloads"));
        for (int i = 1; i < 8; ++i) require(632, processor->session->task(decimal(i)).attempt == 0);
        manager->disposeAll();
        require(624, ngAutoRigNativeBlobMemoryInfo() == baseline,
            text("Disposed dependency sessions must restore native payload count and bytes"));
    }
}

void testNativeBlobPressure() {
    const auto beforeWorker = ngAutoRigNativeBlobMemoryInfo();
    Worker worker([] {
        Bytes source; source.resize(4 * 1024 * 1024); source[0] = 37;
        auto retained = AutoRigValue::blob(source);
        auto baseline = ngAutoRigNativeBlobMemoryInfo();
        for (int i = 0; i < 40; ++i) {
            auto value = AutoRigValue::blob(source); require(648, value.kind == AutoRigValueKind::Blob);
        }
        // D643-650 uses collection count as an allocation-pressure observation.
        // Native replacement scopes release their real owners directly. Compare
        // both live counters after the same replacements, without a fake epoch.
        require(650, ngAutoRigNativeBlobMemoryInfo() == baseline,
            text("Replacement scopes must not accumulate native payload count or bytes"));
        require(652, ngAutoRigNativeBlobMemoryInfo()[1] < baseline[1] + 128 * 1024 * 1024,
            text("Unreachable native payloads must not accumulate across replacements"));
        require(654, retained.readImmutableBlob()[0] == 37,
            text("Pressure-driven collection must preserve retained payloads"));
    });
    worker.start(); worker.join();
    require(643, ngAutoRigNativeBlobMemoryInfo() == beforeWorker,
        text("Worker scope exit must release its retained native payloads"));
}

void testWorkerBlobRelease() {
    auto baseline = ngAutoRigNativeBlobMemoryInfo();
    const auto releaseOnWorker = [] {
        Worker worker([] {
            Bytes source; source.resize(1024 * 1024);
            auto value = AutoRigValue::blob(source); require(668, value.readBlob().size() == 1024 * 1024);
        });
        worker.start(); worker.join();
    };
    releaseOnWorker();
    // D663/674/675 forces GC around the worker. Native join has already ended
    // that ownership scope; inspect both actual live totals, not collection count.
    const auto released = ngAutoRigNativeBlobMemoryInfo();
    require(676, released[0] == baseline[0] && released[1] == baseline[1]);
}

bool eventsEqual(Slice<Text> events, std::initializer_list<Text> expected) {
    if (events.size() != expected.size()) return false;
    std::size_t i = 0;
    for (auto value : expected) if (events[i++] != value) return false;
    return true;
}
bool blobText(AutoRigValue value, std::string_view expected) {
    auto bytes = value.readBlob();
    return bytes.size() == expected.size() &&
        std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()) == expected;
}
}

void ngMain(Slice<Text> args) {
    if (args.size() > 1 && args[1] == text("--native-pressure")) { testNativeBlobPressure(); return; }
    testNativeBlobPressure(); testDependencyScratchCollection(); testWorkerBlobRelease();
    testBlobOwnership(); testChangedFailureCheckpoint(); testFailedRequestSetup();
    testStartupCancellation(); testExplicitActionBoundaryOwnership();
    if (args.size() > 1 && args[1] == text("--ui-performance")) {
        testSessionContext(); benchmarkUIMetadata(); return;
    }
    if (args.size() > 1 && args[1] == text("--context-only")) { testSessionContext(); return; }
    auto threadedProcessor = create<TestRigProcessor>();
    auto threadedManager = create<AutoRigSessionManager>(path("out", "autorig-tests"));
    threadedManager->registerProcessor(threadedProcessor);
    auto threadedRun = threadedManager->create("test-rig");
    threadedRun->setInput("prepare", "source", AutoRigValue::jsonValue(Json(1)));
    Worker worker([&] { threadedRun->execute("prepare"); }); worker.start(); worker.join();
    require(701, threadedProcessor->prepareCalls == 1 && threadedProcessor->sawWorkerThread);

    auto processor = create<TestRigProcessor>();
    auto manager = create<AutoRigSessionManager>(path("out", "autorig-tests"));
    manager->registerProcessor(processor);
    Slice<Text> actionEvents;
    int editorDispatches = 0;
    manager->setTaskActionBoundary([&](Text taskId, Action execute) {
        actionEvents.append(text("begin:") + taskId);
        compat::ngRunWithScopeExit([&] { execute(); },
            [&] { actionEvents.append(text("end:") + taskId); });
    });
    manager->setEditorDispatcher([&](Action action) { ++editorDispatches; action(); });
    require(717, manager->listProcessors().size() == 1);
    auto run = manager->create("test-rig"); auto uiRun = manager->create("test-rig");
    auto inputs = uiRun->inputContext("prepare");
    require(721, inputs == uiRun->inputContext("prepare") && !inputs->hasValue("source"));
    inputs->setValue("source", AutoRigValue::jsonValue(Json(3)));
    require(723, uiRun->renderInputUI("prepare"));
    require(724, processor->lastUITaskId == text("prepare") && processor->lastUIInstanceId == text("prepare"));
    require(725, integer(uiRun->suppliedInput("prepare", "source").json) == 3);
    uiRun->execute("finish");
    auto outputs = uiRun->outputContext("prepare");
    require(728, outputs == uiRun->outputContext("prepare") && outputs->hasValue("data"));
    require(729, uiRun->renderInputUI("finish") && processor->sawConnectedInput);
    require(730, uiRun->renderOutputUI("prepare") && processor->sawOutputArtifact);
    inputs->setValue("source", AutoRigValue::jsonValue(Json(5)));
    require(732, !outputs->hasValue("data") && uiRun->task("finish").state == AutoRigTaskState::Stale);
    manager->close(uiRun->id());
    processor->prepareCalls = 0; processor->finishCalls = 0; processor->editorCalls = 0;
    editorDispatches = 0; actionEvents = {};
    run->setInput("prepare", "source", AutoRigValue::jsonValue(Json(2))); run->execute("finish");
    require(741, eventsEqual(actionEvents, {text("begin:prepare"), text("end:prepare"), text("begin:finish"), text("end:finish")}));
    require(742, processor->editorCalls == 2 && editorDispatches == 2);
    require(743, processor->prepareCalls == 1 && processor->finishCalls == 1);
    require(744, blobText(run->output("finish", "model"), "3"));
    require(745, run->task("prepare").artifacts.size() == 4);
    require(746, run->task("prepare").artifacts[0].preview);
    require(747, run->task("prepare").artifacts[0].storagePath.empty());
    require(748, run->task("finish").artifacts[0].storagePath.empty());
    require(749, !exists(run->directory()));
    require(750, bytesEqual(run->output("prepare", "binary").readBlob(), {0, 1, 2}));
    require(751, run->output("prepare", "name").text == text("source.txt"));
    run->executeGroup({text("prepare"), text("finish")});
    require(754, processor->prepareCalls == 1 && processor->finishCalls == 1);
    require(755, actionEvents.size() == 4);
    run->setInput("prepare", "source", AutoRigValue::jsonValue(Json(6)));
    require(758, run->task("finish").state == AutoRigTaskState::Stale);
    run->execute("finish"); require(760, processor->prepareCalls == 2 && processor->finishCalls == 2);
    require(761, blobText(run->output("finish", "model"), "7"));
    require(762, run->task("prepare").attempt == 2);
    processor->failFinish = true;
    bool failed = false; actionEvents = {};
    try { run->execute("finish", true); } catch (const compat::ThrowableException&) { failed = true; }
    require(769, eventsEqual(actionEvents, {text("begin:finish"), text("end:finish")}));
    require(770, failed && run->task("finish").state == AutoRigTaskState::Failed);
    require(771, run->task("finish").attempt == 3);
    require(772, !run->task("finish").message.empty()); require(773, !exists(run->directory()));
    processor->failFinish = false; processor->cancelPrepare = true; processor->cancelSession = run;
    run->setInput("prepare", "source", AutoRigValue::jsonValue(Json(8)));
    bool canceled = false;
    try { run->execute("prepare"); } catch (const compat::ThrowableException&) { canceled = true; }
    require(782, canceled && run->task("prepare").state == AutoRigTaskState::Canceled);
    processor->cancelPrepare = false; run->execute("finish");
    require(785, blobText(run->output("finish", "model"), "9")); manager->close(run->id());
    auto cyclicProcessor = create<TestRigProcessor>(); cyclicProcessor->cyclic = true;
    auto cyclicManager = create<AutoRigSessionManager>(path("out", "autorig-tests"));
    cyclicManager->registerProcessor(cyclicProcessor);
    bool rejectedCycle = false;
    try { cyclicManager->create("test-rig"); } catch (const compat::ThrowableException&) { rejectedCycle = true; }
    require(795, rejectedCycle);

    auto other = create<OtherRigProcessor>(); manager->registerProcessor(other);
    auto workflows = create<AutoRigWorkflowManager>(manager);
    require(800, workflows->listPresets().size() == 5);
    auto cross = workflows->create("test-rig", "cross-plugin");
    require(802, cross->orderedSteps().size() == 2);
    require(803, cross->orderedSteps()[0].id.view() == "prepare" && cross->orderedSteps()[1].id.view() == "consume");
    cross->setInput("source", AutoRigValue::jsonValue(Json(4))); actionEvents = {};
    cross->executeStep("consume");
    require(807, eventsEqual(actionEvents, {text("begin:") + cross->stepTaskId("prepare"),
        text("end:") + cross->stepTaskId("prepare"), text("begin:") + cross->stepTaskId("consume"),
        text("end:") + cross->stepTaskId("consume")}));
    require(811, cross->snapshot().state == AutoRigWorkflowState::Succeeded);
    require(812, bytesEqual(cross->output("result").readBlob(), {5}));
    require(813, cross->session()->id() == cross->id()); require(814, cross->session()->directory() == cross->directory());
    require(815, manager->get(cross->id()) == cross->session()); require(816, !exists(cross->directory()));
    require(817, cross->session()->task(cross->stepTaskId("prepare")).state == AutoRigTaskState::Succeeded);
    require(818, cross->session()->task(cross->stepTaskId("consume")).state == AutoRigTaskState::Succeeded);
    require(819, cross->session()->renderInputUI(cross->stepTaskId("prepare")));
    require(820, processor->lastUITaskId == text("prepare") && processor->lastUIInstanceId == cross->stepTaskId("prepare"));
    require(822, cross->session()->renderOutputUI(cross->stepTaskId("prepare")) && processor->sawOutputArtifact);
    require(823, cross->session()->renderInputUI(cross->stepTaskId("consume")) && other->sawConnectedInput);
    require(824, cross->session()->renderOutputUI(cross->stepTaskId("consume")) && other->sawOutput);
    require(825, other->lastUITaskId == text("consume")); require(826, cross->snapshot().runId == cross->id());
    require(827, cross->snapshot().steps.at("prepare").taskId == cross->stepTaskId("prepare"));
    auto previousCalls = other->calls; cross->execute(); require(830, other->calls == previousCalls);
    cross->execute(true); require(832, other->calls == previousCalls + 1);
    previousCalls = other->calls; cross->setInput("source", AutoRigValue::jsonValue(Json(8)));
    require(835, cross->session()->task(cross->stepTaskId("consume")).state == AutoRigTaskState::Stale);
    cross->execute(); require(837, bytesEqual(cross->output("result").readBlob(), {9}));
    require(838, other->calls == previousCalls + 1); workflows->close(cross->id());
    auto simple = workflows->create("test-rig", "prepare-only");
    require(842, ngAutoRigWorkflowDiagnostics(simple).sourceObjectNoRef().empty());
    simple->setInput("source", AutoRigValue::jsonValue(Json(10))); simple->execute();
    require(845, integer(simple->output("data").json) == 11); workflows->close(simple->id());
    auto diagnostics = workflows->create("test-rig", "verify-without-report");
    diagnostics->setInput("source", AutoRigValue::jsonValue(Json(5))); diagnostics->execute();
    require(850, ngAutoRigWorkflowDiagnostics(diagnostics).sourceObjectNoRef().empty()); workflows->remove(diagnostics->id());
    auto internal = workflows->create("test-rig", "finish-internal");
    internal->setInput("source", AutoRigValue::jsonValue(Json(14))); internal->execute();
    require(856, blobText(internal->output("model"), "15"));
    require(857, internal->session()->task("6_finish_prepare").state == AutoRigTaskState::Succeeded);
    workflows->close(internal->id());
    auto allKinds = workflows->create("test-rig", "all-value-kinds");
    allKinds->setInput("source", AutoRigValue::jsonValue(Json(7))); allKinds->execute();
    require(863, allKinds->session()->id() == allKinds->id());
    require(864, allKinds->stepTaskId("prepare") != allKinds->stepTaskId("path"));
    auto summary = allKinds->output("summary").json;
    require(866, integer(get(summary, "data")) == 8 && integer(get(summary, "byteCount")) == 3);
    require(867, string(get(summary, "name")) == text("source.txt") && string(get(summary, "model")) == text("7"));
    auto ownedPath = allKinds->session()->output(allKinds->stepTaskId("path"), "model").text;
    auto ownedDirectory = allKinds->session()->directory(); require(870, exists(ownedPath));
    bool foundPreview = false;
    for (auto artifact : allKinds->session()->task(allKinds->stepTaskId("path")).artifacts) {
        if (!artifact.preview) continue;
        require(874, artifact.kind == AutoRigValueKind::Path && artifact.value().kind == AutoRigValueKind::Path);
        require(875, artifact.value().text == artifact.storagePath && exists(artifact.value().text)); foundPreview = true;
    }
    require(878, foundPreview); workflows->close(allKinds->id()); workflows->remove(allKinds->id());
    require(881, !exists(ownedPath) && !exists(ownedDirectory));
    auto activeOwned = workflows->create("test-rig", "all-value-kinds");
    activeOwned->setInput("source", AutoRigValue::jsonValue(Json(1))); activeOwned->execute();
    auto closedOwned = workflows->create("test-rig", "all-value-kinds");
    closedOwned->setInput("source", AutoRigValue::jsonValue(Json(2))); closedOwned->execute();
    workflows->close(closedOwned->id());
    auto activeDirectory = activeOwned->directory(); auto closedDirectory = closedOwned->directory();
    require(891, exists(activeDirectory) && exists(closedDirectory)); workflows->disposeAll(); manager->disposeAll();
    require(894, !exists(activeDirectory) && !exists(closedDirectory)); workflows->disposeAll(); manager->disposeAll();
    other->fail = true;
    auto retry = workflows->create("test-rig", "cross-plugin");
    retry->setInput("source", AutoRigValue::jsonValue(Json(12)));
    bool workflowFailed = false;
    try { retry->execute(); } catch (const compat::ThrowableException&) { workflowFailed = true; }
    require(904, workflowFailed && retry->snapshot().state == AutoRigWorkflowState::Failed);
    require(905, retry->session()->task(retry->stepTaskId("prepare")).state == AutoRigTaskState::Succeeded);
    auto prepareCalls = processor->prepareCalls; other->fail = false; retry->execute();
    require(909, processor->prepareCalls == prepareCalls); require(910, bytesEqual(retry->output("result").readBlob(), {13}));
    other->fail = true; try { retry->executeStep("consume", true); } catch (const compat::ThrowableException&) {}
    require(914, retry->snapshot().state == AutoRigWorkflowState::Failed);
    other->fail = false; processor->runningWorkflow = retry; retry->execute(true); processor->runningWorkflow.reset();
    require(919, retry->snapshot().state == AutoRigWorkflowState::Succeeded); workflows->close(retry->id());
    other->cancelNow = true; auto cancelRun = workflows->create("test-rig", "cross-plugin");
    other->activeWorkflow = cancelRun; cancelRun->setInput("source", AutoRigValue::jsonValue(Json(20)));
    bool workflowCanceled = false;
    try { cancelRun->execute(); } catch (const compat::ThrowableException&) { workflowCanceled = true; }
    require(929, workflowCanceled && cancelRun->snapshot().state == AutoRigWorkflowState::Canceled);
    other->cancelNow = false; cancelRun->execute(); require(932, bytesEqual(cancelRun->output("result").readBlob(), {21}));
    workflows->close(cancelRun->id());
    processor->invalidWorkflow = true; bool rejectedKind = false;
    try { workflows->create("test-rig", "invalid-kind"); } catch (const compat::ThrowableException&) { rejectedKind = true; }
    require(939, rejectedKind); processor->invalidWorkflow = false; processor->unboundWorkflowInput = true;
    bool rejectedInput = false;
    try { workflows->create("test-rig", "unbound-input"); }
    catch (const compat::ThrowableException& error) {
        rejectedInput = error.msg.view().find("Missing required workflow input binding: unused") != std::string_view::npos;
    }
    require(945, rejectedInput);
}
}
