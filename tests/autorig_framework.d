module autorig_framework_test;

import nijigenerate.autorig;
import core.thread : Thread;
import core.thread.fiber : Fiber;
import std.conv : to;
import std.file : exists, readText;
import std.json : JSONValue, parseJSON;
import std.path : buildPath;

private class TestRigProcessor : AutoRigProcessor, IAutoRigInputEditor, IAutoRigOutputViewer,
    IAutoRigSessionEditor, IAutoRigSessionViewer {
    bool sawSessionEditor;
    bool sawSessionViewer;
    int prepareCalls;
    int finishCalls;
    int editorCalls;
    bool sawWorkerThread;
    bool failFinish;
    bool cyclic;
    bool invalidWorkflow;
    bool cancelPrepare;
    bool checkContext;
    AutoRigSession cancelSession;
    string lastUITaskId;
    string lastUIInstanceId;
    bool sawConnectedInput;
    bool sawOutputArtifact;

    override string procId() { return "test-rig"; }
    override string displayName() { return "Test rig"; }

    override void configureSession(string workflowId, IAutoRigSessionEditContext context) {
        assert(workflowId == "prepare-only" && context.workflowId() == workflowId);
        assert(context.runId().length && context.canEdit());
        assert(context.hasInput("source") && context.input("source").json.integer == 4);
        context.setContextValue("ui", AutoRigValue.jsonValue(JSONValue(7)));
        sawSessionEditor = true;
    }

    override void viewSession(string workflowId, IAutoRigSessionViewContext context) {
        assert(workflowId == "prepare-only" && context.workflowId() == workflowId);
        assert(context.contextValue("ui").json.integer == 7);
        assert(context.outputPorts().length == 1 && context.hasOutput("data"));
        assert(context.output("data").json.integer == 5);
        assert(context.outputArtifact("data").storagePath.length == 0);
        assert(context.outputArtifact("data").value.json.integer == 5);
        sawSessionViewer = true;
    }

    override AutoRigWorkflowSpec[] workflows() {
        AutoRigWorkflowSpec cross;
        cross.id = "cross-plugin";
        cross.label = "Prepare and consume";
        cross.inputs = [AutoRigPortSpec("source", AutoRigValueKind.Json)];
        cross.outputs = [AutoRigPortSpec("result", AutoRigValueKind.Blob)];
        cross.steps = [
            AutoRigWorkflowStep("consume", "other-rig", "consume"),
            AutoRigWorkflowStep("prepare", "test-rig", "prepare")
        ];
        cross.inputBindings = [AutoRigWorkflowInputBinding("source", "prepare", null, "source")];
        cross.connections = [AutoRigWorkflowConnection("prepare", "data", "consume", null, "data")];
        cross.outputBindings = [AutoRigWorkflowOutputBinding("result", "consume", "result")];

        AutoRigWorkflowSpec simple;
        simple.id = "prepare-only";
        simple.label = "Prepare";
        simple.inputs = [AutoRigPortSpec("source", AutoRigValueKind.Json)];
        simple.outputs = [AutoRigPortSpec("data", AutoRigValueKind.Json)];
        simple.steps = [AutoRigWorkflowStep("prepare", "test-rig", "prepare")];
        simple.inputBindings = [AutoRigWorkflowInputBinding("source", "prepare", null, "source")];
        simple.outputBindings = [AutoRigWorkflowOutputBinding("data", "prepare", "data")];
        AutoRigWorkflowSpec internal;
        internal.id = "finish-internal";
        internal.label = "Finish with internal dependency";
        internal.inputs = [AutoRigPortSpec("source", AutoRigValueKind.Json)];
        internal.outputs = [AutoRigPortSpec("model", AutoRigValueKind.Blob)];
        internal.steps = [AutoRigWorkflowStep("finish", "test-rig", "finish")];
        internal.inputBindings = [AutoRigWorkflowInputBinding("source", "finish", "prepare", "source")];
        internal.outputBindings = [AutoRigWorkflowOutputBinding("model", "finish", "model")];
        AutoRigWorkflowSpec artifacts;
        artifacts.id = "all-value-kinds";
        artifacts.label = "Transfer every value kind";
        artifacts.inputs = [AutoRigPortSpec("source", AutoRigValueKind.Json)];
        artifacts.outputs = [AutoRigPortSpec("summary", AutoRigValueKind.Json)];
        artifacts.steps = [
            AutoRigWorkflowStep("prepare", "test-rig", "prepare"),
            AutoRigWorkflowStep("path", "test-rig", "emit-path"),
            AutoRigWorkflowStep("inspect", "other-rig", "inspect-artifacts")
        ];
        artifacts.inputBindings = [
            AutoRigWorkflowInputBinding("source", "prepare", null, "source"),
            AutoRigWorkflowInputBinding("source", "path", null, "source")
        ];
        artifacts.connections = [
            AutoRigWorkflowConnection("prepare", "data", "inspect", null, "data"),
            AutoRigWorkflowConnection("prepare", "binary", "inspect", null, "binary"),
            AutoRigWorkflowConnection("prepare", "name", "inspect", null, "name"),
            AutoRigWorkflowConnection("path", "model", "inspect", null, "model")
        ];
        artifacts.outputBindings = [AutoRigWorkflowOutputBinding("summary", "inspect", "summary")];
        if (invalidWorkflow) {
            auto invalid = cross;
            invalid.id = "invalid-kind";
            invalid.connections = [AutoRigWorkflowConnection("prepare", "binary", "consume", null, "data")];
            return [cross, simple, internal, artifacts, invalid];
        }
        return [cross, simple, internal, artifacts];
    }

    override AutoRigTaskSpec[] tasks() {
        if (cyclic) return [
            AutoRigTaskSpec("a", "A", ["b"]),
            AutoRigTaskSpec("b", "B", ["a"])
        ];
        return [
            AutoRigTaskSpec("prepare", "Prepare", null,
                [AutoRigPortSpec("source", AutoRigValueKind.Json)],
                [AutoRigPortSpec("data", AutoRigValueKind.Json),
                    AutoRigPortSpec("binary", AutoRigValueKind.Blob),
                    AutoRigPortSpec("name", AutoRigValueKind.FileName)], null),
            AutoRigTaskSpec("finish", "Finish", ["prepare"],
                [AutoRigPortSpec("data", AutoRigValueKind.Json)],
                [AutoRigPortSpec("model", AutoRigValueKind.Blob)],
                [AutoRigConnection("data", "prepare", "data")]),
            AutoRigTaskSpec("emit-path", "Emit path", null,
                [AutoRigPortSpec("source", AutoRigValueKind.Json)],
                [AutoRigPortSpec("model", AutoRigValueKind.Path)])
        ];
    }

    override void executeTask(string taskId, AutoRigTaskContext context) {
        assert(Fiber.getThis() !is null);
        if (!Thread.getThis().isMainThread) sawWorkerThread = true;
        Fiber.yield();
        context.runOnMainThread({ ++editorCalls; });
        if (taskId == "prepare") {
            if (checkContext) {
                assert(context.contextValue("artifact").text == "reference.json");
                auto metadata = context.contextValue("metadata").json;
                assert(metadata["label"].str == "original");
                metadata["label"] = JSONValue("task edit");
                assert(context.contextValue("metadata").json["label"].str == "original");
                auto bytes = context.contextValue("bytes").readBlob();
                assert(bytes == [cast(ubyte)1, 2]);
                bytes[0] = 9;
                assert(context.contextValue("bytes").readBlob()[0] == 1);
                bool rejected;
                try cancelSession.setContextValue("artifact", AutoRigValue.path("changed"));
                catch (Exception error) rejected = true;
                assert(rejected);
                context.previewBlob("image", [cast(ubyte)3, 4], "image/png");
            }
            ++prepareCalls;
            auto source = context.input("source").json.integer;
            context.previewJson("progress", JSONValue(source));
            if (cancelPrepare) {
                cancelSession.cancel();
                assert(context.isCanceled());
                return;
            }
            context.publishJson("data", JSONValue(source + 1));
            context.publishBlob("binary", [cast(ubyte)0, 1, 2]);
            context.publishFileName("name", "source.txt");
        } else if (taskId == "finish") {
            ++finishCalls;
            auto value = context.input("data").json.integer;
            if (failFinish) throw new Exception("expected failure");
            context.publishBlob("model", cast(ubyte[])value.to!string.dup);
        } else if (taskId == "emit-path") {
            auto value = context.input("source").json.integer;
            auto location = context.outputPath("model", ".txt");
            // This fixture explicitly supplies an external file; the framework
            // does not create directories or persist task artifacts itself.
            import std.file : write, mkdirRecurse;
            import std.path : dirName;
            mkdirRecurse(location.dirName);
            write(location, value.to!string);
            context.publishPath("model", location);
        } else {
            throw new Exception("Unknown test task");
        }
    }

    override void configureInput(string taskId, IAutoRigInputContext context) {
        lastUITaskId = taskId;
        lastUIInstanceId = context.taskId();
        if (taskId == "prepare") {
            assert(context.ports().length == 1 && context.canEdit("source"));
            if (!context.hasValue("source")) context.setValue("source", AutoRigValue.jsonValue(JSONValue(3)));
        } else if (taskId == "finish") {
            sawConnectedInput = !context.canEdit("data") && context.isConnected("data") &&
                context.hasValue("data") &&
                context.value("data").json.integer > 0;
        }
    }

    override void viewOutput(string taskId, IAutoRigOutputContext context) {
        lastUITaskId = taskId;
        lastUIInstanceId = context.taskId();
        if (taskId == "prepare") {
            auto snapshot = context.snapshot();
            sawOutputArtifact = snapshot.state == AutoRigTaskState.Succeeded &&
                snapshot.artifacts.length > 0 && context.hasValue("data") &&
                context.artifact("data").storagePath.length == 0 &&
                context.artifact("data").value.json.integer > 0;
        }
    }
}

private class OtherRigProcessor : AutoRigProcessor, IAutoRigInputEditor, IAutoRigOutputViewer {
    int calls;
    bool fail;
    bool cancelNow;
    AutoRigWorkflowRun activeWorkflow;
    string lastUITaskId;
    bool sawConnectedInput;
    bool sawOutput;

    override string procId() { return "other-rig"; }
    override string displayName() { return "Other rig"; }
    override AutoRigTaskSpec[] tasks() {
        return [
            AutoRigTaskSpec("consume", "Consume", null,
                [AutoRigPortSpec("data", AutoRigValueKind.Json)],
                [AutoRigPortSpec("result", AutoRigValueKind.Blob)]),
            AutoRigTaskSpec("inspect-artifacts", "Inspect artifacts", null,
                [AutoRigPortSpec("data", AutoRigValueKind.Json),
                    AutoRigPortSpec("binary", AutoRigValueKind.Blob),
                    AutoRigPortSpec("name", AutoRigValueKind.FileName),
                    AutoRigPortSpec("model", AutoRigValueKind.Path)],
                [AutoRigPortSpec("summary", AutoRigValueKind.Json)])
        ];
    }
    override void executeTask(string taskId, AutoRigTaskContext context) {
        assert(Fiber.getThis() !is null);
        Fiber.yield();
        ++calls;
        if (taskId == "inspect-artifacts") {
            JSONValue[string] summary;
            summary["data"] = context.input("data").json;
            summary["byteCount"] = JSONValue(cast(int)context.input("binary").readBlob().length);
            summary["name"] = JSONValue(context.input("name").text);
            summary["model"] = JSONValue(readText(context.input("model").text));
            context.publishJson("summary", JSONValue(summary));
            return;
        }
        assert(taskId == "consume");
        if (fail) throw new Exception("expected workflow failure");
        if (cancelNow) {
            activeWorkflow.cancel();
            assert(context.isCanceled());
            return;
        }
        auto value = cast(ubyte)context.input("data").json.integer;
        context.publishBlob("result", [value]);
    }

    override void configureInput(string taskId, IAutoRigInputContext context) {
        lastUITaskId = taskId;
        if (taskId == "consume")
            sawConnectedInput = context.isConnected("data") && context.hasValue("data");
    }

    override void viewOutput(string taskId, IAutoRigOutputContext context) {
        lastUITaskId = taskId;
        if (taskId == "consume")
            sawOutput = context.hasValue("result") && context.value("result").readBlob().length == 1;
    }
}

private void testSessionContext() {
    auto processor = new TestRigProcessor();
    processor.checkContext = true;
    auto manager = new AutoRigSessionManager(buildPath("out", "autorig-session-context-runs"));
    manager.registerProcessor(processor);
    manager.registerProcessorAlias("previous-test-rig", "test-rig");
    assert(manager.processor("previous-test-rig") is processor);
    assert(manager.listProcessors().length == 1);
    auto workflows = new AutoRigWorkflowManager(manager);
    auto run = workflows.create("test-rig", "prepare-only");
    processor.cancelSession = run.session();
    run.setInput("source", AutoRigValue.jsonValue(JSONValue(4)));
    auto inputRevision = run.inputRevision();
    assert(inputRevision > 0);
    auto taskInputRevision = run.session().inputRevision(run.stepTaskId("prepare"));
    assert(taskInputRevision > 0);
    run.setInput("source", AutoRigValue.jsonValue(JSONValue(4)));
    assert(run.inputRevision() > inputRevision);
    assert(run.session().inputRevision(run.stepTaskId("prepare")) > taskInputRevision);
    auto source = run.input("source");
    assert(source.json.integer == 4 && run.inputPorts().length == 1);
    auto metadata = JSONValue(["label": JSONValue("original")]);
    run.setContextValue("metadata", AutoRigValue.jsonValue(metadata));
    metadata["label"] = JSONValue("caller edit");
    run.setContextValue("bytes", AutoRigValue.blob([cast(ubyte)1, 2]));
    auto info = run.session().contextInfo("bytes");
    assert(info.kind == AutoRigValueKind.Blob && info.byteLength == 2 && info.text.length == 0);
    auto revision = info.revision;
    run.setContextValue("bytes", AutoRigValue.blob([cast(ubyte)1, 2]));
    assert(run.session().contextInfo("bytes").revision > revision);
    run.setContextValue("artifact", AutoRigValue.path("reference.json"));
    assert(run.renderInputUI() && processor.sawSessionEditor);
    auto worker = new Thread({ run.execute(); });
    worker.start(); worker.join();
    assert(run.output("data").json.integer == 5);
    assert(!exists(run.directory()));
    auto artifact = run.outputArtifact("data");
    assert(artifact.value.json.integer == 5 && artifact.storagePath.length == 0);
    auto previews = run.session().task(run.stepTaskId("prepare")).artifacts;
    assert(previews[0].preview && previews[0].value.readBlob() == [cast(ubyte)3, 4]);
    assert(previews[0].storagePath.length == 0 && previews[0].mediaType == "image/png");
    assert(previews[1].preview && previews[1].value.json.integer == 4);
    assert(run.snapshot(false).steps["prepare"].artifacts.length == 0);
    assert(run.snapshot().steps["prepare"].artifacts.length > 0);
    assert(run.renderOutputUI() && processor.sawSessionViewer);
    bool rejectedWorkerUI;
    auto uiWorker = new Thread({
        try run.renderInputUI();
        catch (Exception error) rejectedWorkerUI = true;
    });
    uiWorker.start(); uiWorker.join();
    assert(rejectedWorkerUI);
    auto id = run.id();
    workflows.close(id);
    auto restored = workflows.reopen(id);
    processor.cancelSession = restored.session();
    assert(restored.input("source").json.integer == 4);
    assert(restored.session().contextValue("bytes").readBlob() == [cast(ubyte)1, 2]);
    assert(restored.session().contextValue("metadata").json["label"].str == "original");
    auto calls = processor.prepareCalls;
    restored.execute();
    assert(processor.prepareCalls == calls);
    restored.setContextValue("artifact", AutoRigValue.path("reference.json"));
    assert(restored.session().task(restored.stepTaskId("prepare")).state == AutoRigTaskState.Stale);
    workflows.close(id);
    restored = workflows.reopen(id);
    processor.cancelSession = restored.session();
    assert(restored.session().task(restored.stepTaskId("prepare")).state == AutoRigTaskState.Stale);
    restored.execute();
    assert(processor.prepareCalls == calls + 1);
    workflows.remove(id);
    bool deleted;
    try workflows.reopen(id);
    catch (Exception error) deleted = true;
    assert(deleted);
    bool unregistered;
    try manager.get(id);
    catch (Exception error) unregistered = true;
    assert(unregistered);
    auto closed = workflows.create("test-rig", "prepare-only");
    auto closedId = closed.id();
    workflows.close(closedId);
    workflows.remove(closedId);
    deleted = false;
    try workflows.reopen(closedId);
    catch (Exception error) deleted = true;
    assert(deleted);
    import std.stdio : writeln;
    writeln("AutoRig session context checks passed");
}

private void benchmarkUIMetadata() {
    import std.datetime.stopwatch : StopWatch;
    import std.stdio : writefln;
    auto manager = new AutoRigSessionManager(buildPath("out", "autorig-ui-benchmark"));
    manager.registerProcessor(new TestRigProcessor());
    auto session = manager.create("test-rig");
    enum size_t payloadSize = 16 * 1024 * 1024;
    session.setContextValue("model", AutoRigValue.blob(new ubyte[payloadSize]));
    size_t copied, metadata;
    auto oldTimer = StopWatch(); oldTimer.start();
    foreach (i; 0 .. 32) copied += session.contextValue("model").bytes.length;
    oldTimer.stop();
    auto newTimer = StopWatch(); newTimer.start();
    foreach (i; 0 .. 32) metadata += session.contextInfo("model").byteLength;
    newTimer.stop();
    assert(copied == metadata && copied == 32 * payloadSize);
    assert(!exists(session.directory()));
    writefln("UI payload lookup, 32 reads of 16 MiB: copied=%s us; metadata=%s us; eliminated=%s MiB",
        oldTimer.peek.total!"usecs", newTimer.peek.total!"usecs", copied / (1024 * 1024));
}

void main(string[] args) {
    if (args.length > 1 && args[1] == "--ui-performance") {
        testSessionContext(); benchmarkUIMetadata(); return;
    }
    if (args.length > 1 && args[1] == "--context-only") { testSessionContext(); return; }
    auto threadedProcessor = new TestRigProcessor();
    auto threadedManager = new AutoRigSessionManager(buildPath("out", "autorig-tests"));
    threadedManager.registerProcessor(threadedProcessor);
    auto threadedRun = threadedManager.create("test-rig");
    threadedRun.setInput("prepare", "source", AutoRigValue.jsonValue(JSONValue(1)));
    auto worker = new Thread({ threadedRun.execute("prepare"); });
    worker.start();
    worker.join();
    assert(threadedProcessor.prepareCalls == 1 && threadedProcessor.sawWorkerThread);

    auto processor = new TestRigProcessor();
    auto manager = new AutoRigSessionManager(buildPath("out", "autorig-tests"));
    manager.registerProcessor(processor);
    string[] actionEvents;
    int editorDispatches;
    manager.setTaskActionBoundary((string taskId, void delegate() execute) {
        actionEvents ~= "begin:" ~ taskId;
        scope(exit) actionEvents ~= "end:" ~ taskId;
        execute();
    });
    manager.setEditorDispatcher((void delegate() action) {
        ++editorDispatches;
        action();
    });
    assert(manager.listProcessors().length == 1);
    auto run = manager.create("test-rig");
    auto uiRun = manager.create("test-rig");
    auto inputs = uiRun.inputContext("prepare");
    assert(inputs is uiRun.inputContext("prepare") && !inputs.hasValue("source"));
    inputs.setValue("source", AutoRigValue.jsonValue(JSONValue(3)));
    assert(uiRun.renderInputUI("prepare"));
    assert(processor.lastUITaskId == "prepare" && processor.lastUIInstanceId == "prepare");
    assert(uiRun.suppliedInput("prepare", "source").json.integer == 3);
    uiRun.execute("finish");
    auto outputs = uiRun.outputContext("prepare");
    assert(outputs is uiRun.outputContext("prepare") && outputs.hasValue("data"));
    assert(uiRun.renderInputUI("finish") && processor.sawConnectedInput);
    assert(uiRun.renderOutputUI("prepare") && processor.sawOutputArtifact);
    inputs.setValue("source", AutoRigValue.jsonValue(JSONValue(5)));
    assert(!outputs.hasValue("data") && uiRun.task("finish").state == AutoRigTaskState.Stale);
    manager.close(uiRun.id());
    processor.prepareCalls = 0;
    processor.finishCalls = 0;
    processor.editorCalls = 0;
    editorDispatches = 0;
    actionEvents = null;
    run.setInput("prepare", "source", AutoRigValue.jsonValue(JSONValue(2)));
    run.execute("finish");
    assert(actionEvents == ["begin:prepare", "end:prepare", "begin:finish", "end:finish"]);
    assert(processor.editorCalls == 2 && editorDispatches == 2);
    assert(processor.prepareCalls == 1 && processor.finishCalls == 1);
    assert(cast(string)run.output("finish", "model").readBlob() == "3");
    assert(run.task("prepare").artifacts.length == 4);
    assert(run.task("prepare").artifacts[0].preview);
    assert(run.task("prepare").artifacts[0].storagePath.length == 0);
    assert(run.task("finish").artifacts[0].storagePath.length == 0);
    assert(!exists(run.directory()));
    assert(run.output("prepare", "binary").readBlob() == [cast(ubyte)0, 1, 2]);
    assert(run.output("prepare", "name").text == "source.txt");

    run.executeGroup(["prepare", "finish"]);
    assert(processor.prepareCalls == 1 && processor.finishCalls == 1);
    assert(actionEvents.length == 4);

    run.setInput("prepare", "source", AutoRigValue.jsonValue(JSONValue(6)));
    assert(run.task("finish").state == AutoRigTaskState.Stale);
    run.execute("finish");
    assert(processor.prepareCalls == 2 && processor.finishCalls == 2);
    assert(cast(string)run.output("finish", "model").readBlob() == "7");
    assert(run.task("prepare").attempt == 2);

    processor.failFinish = true;
    bool failed;
    actionEvents = null;
    try run.execute("finish", true);
    catch (Exception error) failed = true;
    assert(actionEvents == ["begin:finish", "end:finish"]);
    assert(failed && run.task("finish").state == AutoRigTaskState.Failed);
    assert(run.task("finish").attempt == 3);
    assert(run.task("finish").message.length > 0);
    assert(!exists(run.directory()));

    processor.failFinish = false;
    processor.cancelPrepare = true;
    processor.cancelSession = run;
    run.setInput("prepare", "source", AutoRigValue.jsonValue(JSONValue(8)));
    bool canceled;
    try run.execute("prepare");
    catch (Exception error) canceled = true;
    assert(canceled && run.task("prepare").state == AutoRigTaskState.Canceled);
    processor.cancelPrepare = false;
    run.execute("finish");
    assert(cast(string)run.output("finish", "model").readBlob() == "9");
    manager.close(run.id());

    auto cyclicProcessor = new TestRigProcessor();
    cyclicProcessor.cyclic = true;
    auto cyclicManager = new AutoRigSessionManager(buildPath("out", "autorig-tests"));
    cyclicManager.registerProcessor(cyclicProcessor);
    bool rejectedCycle;
    try cyclicManager.create("test-rig");
    catch (Exception error) rejectedCycle = true;
    assert(rejectedCycle);

    auto other = new OtherRigProcessor();
    manager.registerProcessor(other);
    auto workflows = new AutoRigWorkflowManager(manager);
    assert(workflows.listPresets().length == 4);
    auto cross = workflows.create("test-rig", "cross-plugin");
    assert(cross.orderedSteps().length == 2);
    assert(cross.orderedSteps()[0].id == "prepare" && cross.orderedSteps()[1].id == "consume");
    cross.setInput("source", AutoRigValue.jsonValue(JSONValue(4)));
    actionEvents = null;
    cross.executeStep("consume");
    assert(actionEvents == ["begin:" ~ cross.stepTaskId("prepare"),
        "end:" ~ cross.stepTaskId("prepare"),
        "begin:" ~ cross.stepTaskId("consume"),
        "end:" ~ cross.stepTaskId("consume")]);
    assert(cross.snapshot().state == AutoRigWorkflowState.Succeeded);
    assert(cross.output("result").readBlob() == [cast(ubyte)5]);
    assert(cross.session().id() == cross.id());
    assert(cross.session().directory() == cross.directory());
    assert(manager.get(cross.id()) is cross.session());
    assert(!exists(cross.directory()));
    assert(cross.session().task(cross.stepTaskId("prepare")).state == AutoRigTaskState.Succeeded);
    assert(cross.session().task(cross.stepTaskId("consume")).state == AutoRigTaskState.Succeeded);
    assert(cross.session().renderInputUI(cross.stepTaskId("prepare")));
    assert(processor.lastUITaskId == "prepare" &&
        processor.lastUIInstanceId == cross.stepTaskId("prepare"));
    assert(cross.session().renderOutputUI(cross.stepTaskId("prepare")) && processor.sawOutputArtifact);
    assert(cross.session().renderInputUI(cross.stepTaskId("consume")) && other.sawConnectedInput);
    assert(cross.session().renderOutputUI(cross.stepTaskId("consume")) && other.sawOutput);
    assert(other.lastUITaskId == "consume");
    assert(cross.snapshot().runId == cross.id());
    assert(cross.snapshot().steps["prepare"].taskId == cross.stepTaskId("prepare"));
    auto previousCalls = other.calls;
    cross.execute();
    assert(other.calls == previousCalls);
    cross.execute(true);
    assert(other.calls == previousCalls + 1);
    previousCalls = other.calls;
    cross.setInput("source", AutoRigValue.jsonValue(JSONValue(8)));
    assert(cross.session().task(cross.stepTaskId("consume")).state == AutoRigTaskState.Stale);
    cross.execute();
    assert(cross.output("result").readBlob() == [cast(ubyte)9]);
    assert(other.calls == previousCalls + 1);
    workflows.close(cross.id());

    auto simple = workflows.create("test-rig", "prepare-only");
    simple.setInput("source", AutoRigValue.jsonValue(JSONValue(10)));
    simple.execute();
    assert(simple.output("data").json.integer == 11);
    workflows.close(simple.id());

    auto internal = workflows.create("test-rig", "finish-internal");
    internal.setInput("source", AutoRigValue.jsonValue(JSONValue(14)));
    internal.execute();
    assert(cast(string)internal.output("model").readBlob() == "15");
    assert(internal.session().task("6_finish_prepare").state == AutoRigTaskState.Succeeded);
    workflows.close(internal.id());

    auto allKinds = workflows.create("test-rig", "all-value-kinds");
    allKinds.setInput("source", AutoRigValue.jsonValue(JSONValue(7)));
    allKinds.execute();
    assert(allKinds.session().id() == allKinds.id());
    assert(allKinds.stepTaskId("prepare") != allKinds.stepTaskId("path"));
    auto summary = allKinds.output("summary").json;
    assert(summary["data"].integer == 8 && summary["byteCount"].integer == 3);
    assert(summary["name"].str == "source.txt" && summary["model"].str == "7");
    workflows.close(allKinds.id());

    other.fail = true;
    auto retry = workflows.create("test-rig", "cross-plugin");
    retry.setInput("source", AutoRigValue.jsonValue(JSONValue(12)));
    bool workflowFailed;
    try retry.execute();
    catch (Exception error) workflowFailed = true;
    assert(workflowFailed && retry.snapshot().state == AutoRigWorkflowState.Failed);
    assert(retry.session().task(retry.stepTaskId("prepare")).state == AutoRigTaskState.Succeeded);
    auto prepareCalls = processor.prepareCalls;
    other.fail = false;
    retry.execute();
    assert(processor.prepareCalls == prepareCalls);
    assert(retry.output("result").readBlob() == [cast(ubyte)13]);
    workflows.close(retry.id());

    other.cancelNow = true;
    auto cancelRun = workflows.create("test-rig", "cross-plugin");
    other.activeWorkflow = cancelRun;
    cancelRun.setInput("source", AutoRigValue.jsonValue(JSONValue(20)));
    bool workflowCanceled;
    try cancelRun.execute();
    catch (Exception error) workflowCanceled = true;
    assert(workflowCanceled && cancelRun.snapshot().state == AutoRigWorkflowState.Canceled);
    other.cancelNow = false;
    cancelRun.execute();
    assert(cancelRun.output("result").readBlob() == [cast(ubyte)21]);
    workflows.close(cancelRun.id());

    processor.invalidWorkflow = true;
    bool rejectedKind;
    try workflows.create("test-rig", "invalid-kind");
    catch (Exception error) rejectedKind = true;
    assert(rejectedKind);
}
