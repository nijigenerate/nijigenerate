module autorig_state_storage_test;

import nijigenerate.autorig.deterministic.storage;
import nijigenerate.autorig.json : ngParseAutoRigJson;
import std.file : readText;
import std.json : JSONValue;
import std.stdio : writefln, writeln;
import std.conv : to;
import core.thread : Thread;
import core.memory : GC;
import nijigenerate.autorig.framework;

private class StorageProcessor : AutoRigProcessor {
    JSONValue source;
    RigStateStorage storage;
    override string procId() { return "state-storage-test"; }
    override string displayName() { return "State storage test"; }
    override AutoRigTaskSpec[] tasks() {
        return [AutoRigTaskSpec("observe", "Observe", null, null,
            [AutoRigPortSpec("state",AutoRigValueKind.Json)], null)];
    }
    override void executeTask(string id, AutoRigTaskContext context) {
        storage = ngRigStateStorage(context);
        context.publishJson("state",storage.snapshot(source,true));
    }
}

void main(string[] args) {
    assert(args.length == 2, "Pass a real PSD-derived cloud JSON path");
    auto input = ngParseAutoRigJson(readText(args[1]));
    JSONValue[] materials;
    ulong id;
    foreach (name, cloud; input.object)
        materials ~= JSONValue(["uuid":JSONValue(++id),"name":JSONValue(name),"cloud":cloud]);
    auto source = JSONValue(["materials":JSONValue(materials),"rootId":JSONValue(1UL)]);
    auto storage = new RigStateStorage();
    auto initial = storage.snapshot(source);
    auto baseline = storage.memoryInfo();
    auto originalBytes = source.toString().length;
    JSONValue[] states;
    foreach (stage; 0 .. 14) {
        auto owned = storage.restore(initial);
        assert(owned["materials"][0]["cloud"] == materials[0]["cloud"]);
        owned["completed_stage"] = JSONValue(stage.to!string);
        auto checkpoint = storage.snapshot(owned);
        assert(checkpoint["artifact_refs"] == initial["artifact_refs"]);
        assert(checkpoint.toString().length < 1024);
        states ~= checkpoint;
    }
    auto info = storage.memoryInfo();
    assert(info == baseline, "Repeated states duplicated source artifacts or numeric support");
    auto metadata = storage.artifact(initial,"materials");
    assert(("cloud" in metadata[0].object) is null);
    auto changed = storage.restore(initial);
    auto original = changed["materials"][0]["cloud"][0][0];
    changed["materials"][0]["cloud"][0][0] = JSONValue(-999.);
    assert(storage.restore(initial)["materials"][0]["cloud"][0][0] == original);
    auto worker = new Thread({
        foreach (checkpoint; states) {
            auto restored = storage.restore(checkpoint);
            assert(restored["materials"][0]["cloud"] == materials[0]["cloud"]);
        }
    });
    worker.start(); worker.join();
    // Fresh observations have their own support identity; an old checkpoint
    // restores its original generation even after a later observation.
    auto refreshed = storage.snapshot(source,true);
    assert(refreshed["artifact_refs"] != initial["artifact_refs"]);
    auto originalGeneration = storage.snapshot(storage.restore(initial));
    assert(originalGeneration["artifact_refs"] == initial["artifact_refs"]);
    storage.dispose();
    assert(storage.memoryInfo()["json_bytes"].uinteger == 0);
    assert(storage.memoryInfo()["cpu_bytes"].uinteger == 0);
    writefln("Real source JSON: %s bytes; retained metadata: %s bytes; CPU arrays: %s bytes; 14 state references; ownership and worker round trips passed",
        originalBytes,info["json_bytes"].uinteger,info["cpu_bytes"].uinteger);
    auto processor = new StorageProcessor(); processor.source = source;
    auto manager = new AutoRigSessionManager("out/state-storage-no-files");
    manager.registerProcessor(processor);
    auto session = manager.create(processor.procId());
    session.execute("observe");
    assert(session.memoryInfo()["cpu_bytes"].uinteger == baseline["cpu_bytes"].uinteger);
    manager.remove(session.id());
    assert(session.memoryInfo()["json_bytes"].uinteger == 0);
    assert(session.memoryInfo()["cpu_bytes"].uinteger == 0);
    assert(processor.storage.memoryInfo()["cpu_bytes"].uinteger == 0);
    writeln("Session deletion releases storage even when the session and workspace objects remain referenced");
}
