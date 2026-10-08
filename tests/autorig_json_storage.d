module autorig_json_storage;

import nijigenerate.autorig.framework;
import nijigenerate.autorig.json : ngParseAutoRigJson;
import core.memory : GC;
import std.file : readText, exists;
import std.json : JSONValue;
import std.stdio : writefln;
import std.datetime.stopwatch : StopWatch;
import std.conv : to;

private class SnapshotProcessor : AutoRigProcessor {
    JSONValue payload;
    override string procId() { return "json-storage-test"; }
    override string displayName() { return "JSON storage test"; }
    override AutoRigTaskSpec[] tasks() {
        AutoRigTaskSpec[] result;
        foreach (i; 0 .. 12) {
            AutoRigTaskSpec spec;
            spec.id = "stage-" ~ i.to!string;
            spec.outputs = [AutoRigPortSpec("state", AutoRigValueKind.Json)];
            result ~= spec;
        }
        return result;
    }
    override void executeTask(string id, AutoRigTaskContext context) {
        context.publishJson("state", payload);
    }
}

private void createAndRemove(AutoRigSessionManager manager, SnapshotProcessor processor) {
    auto session = manager.create(processor.procId());
    foreach (spec; processor.tasks()) session.execute(spec.id);
    manager.remove(session.id());
}

void main(string[] args) {
    assert(args.length == 3, "Pass old|packed|cycles and a real PSD cloud JSON path");
    auto processor = new SnapshotProcessor();
    processor.payload = ngParseAutoRigJson(readText(args[2]));
    // Include a non-integral coordinate to check binary-double round trips.
    processor.payload["precision"] = JSONValue(1.2345678901234567);
    if (args[1] == "cycles") {
        auto manager = new AutoRigSessionManager("out/json-storage-no-files");
        manager.registerProcessor(processor);
        GC.collect(); GC.collect();
        auto baseline = GC.stats().usedSize;
        foreach (i; 0 .. 4) {
            createAndRemove(manager, processor);
            GC.collect(); GC.collect();
            auto used = GC.stats().usedSize;
            writefln("Delete cycle %s: baseline=%s MiB; used=%s MiB; delta=%s KiB",
                i + 1, baseline / (1024 * 1024), used / (1024 * 1024),
                (cast(long)used - cast(long)baseline) / 1024);
            assert(used < baseline + 8 * 1024 * 1024, "Deleted session retained large artifact payloads");
        }
        return;
    }
    AutoRigValue[] old;
    AutoRigSession session;
    if (args[1] == "old") {
        foreach (i; 0 .. 12) old ~= ngCopyAutoRigValue(AutoRigValue.jsonValue(processor.payload));
    } else {
        auto manager = new AutoRigSessionManager("out/json-storage-no-files");
        manager.registerProcessor(processor);
        session = manager.create(processor.procId());
        foreach (spec; processor.tasks()) session.execute(spec.id);
        auto artifact = session.task("stage-0").artifacts[0];
        assert(artifact.storagePath.length == 0 && artifact.byteLength > 0);
        auto info = session.memoryInfo();
        assert(info["json_bytes"].uinteger == 12 * artifact.byteLength);
        assert(info["blob_bytes"].uinteger == 0 && info["artifacts"].array.length == 12);
        auto owned = artifact.value.json;
        assert(owned["precision"].floating == processor.payload["precision"].floating);
        assert(owned["face"] == processor.payload["face"]);
        owned["face"][0][0] = JSONValue(-999.);
        assert(artifact.value.json["face"][0][0] == processor.payload["face"][0][0]);
        assert(!exists(session.directory()));
    }
    GC.collect();
    auto timer = StopWatch(); timer.start();
    foreach (i; 0 .. 8) GC.collect();
    timer.stop();
    auto stats = GC.stats();
    writefln("%s: used=%s MiB; eight collections=%s ms", args[1],
        stats.usedSize / (1024 * 1024), timer.peek.total!"msecs");
    // Keep both representations live until after collection measurements.
    if (session !is null) assert(session.task("stage-11").artifacts.length == 1);
    else assert(old.length == 12 && old[$ - 1].json["face"] == processor.payload["face"]);
}
