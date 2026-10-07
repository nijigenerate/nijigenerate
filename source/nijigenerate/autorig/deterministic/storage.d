module nijigenerate.autorig.deterministic.storage;

import nijigenerate.autorig.framework : AutoRigWorkspace, AutoRigTaskContext;
import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.json : ngParseAutoRigJson;
import std.json : JSONValue, JSONType;
import std.exception : enforce;
import std.conv : to;
import std.digest.sha : sha256Of;
import std.digest : toHexString;

/** Python-style artifact separation: numeric source support is CPU workspace
    data, observations/results are individual artifacts, and state holds references.
    No editor objects, textures, file names or disk I/O belong to this store. */
class RigStateStorage : AutoRigWorkspace {
private:
    struct Support {
        immutable(Point2)[][string] points;
        immutable(ulong)[][string] runs;
    }
    Support[string] supports;
    string[string] artifacts;
    string[string] names;
    string[ulong] currentSupports;
    ulong generation;

    string retain(string name, JSONValue value) {
        auto encoded = value.toString();
        auto id = sha256Of(encoded).toHexString.idup;
        if ((id in artifacts) is null) artifacts[id] = encoded;
        names[id] = name;
        return id;
    }

    JSONValue materialMetadata(JSONValue material, bool freshSource) {
        auto result = JSONValue(material.object.dup);
        auto materialId = ngRigUnsigned(material["uuid"]);
        auto id = materialId in currentSupports;
        if (freshSource || id is null) {
            Support support;
            foreach (key; ["cloud", "landmark_cloud", "draw_order_cloud"])
                if (auto value = key in result.object) support.points[key] = ngRigPoints(*value).idup;
            foreach (key; ["alpha_runs_32", "alpha_runs_128"])
                if (auto value = key in result.object) {
                    ulong[] runs;
                    foreach (entry; value.array) runs ~= ngRigUnsigned(entry);
                    support.runs[key] = runs.idup;
                }
            auto token = (++generation).to!string;
            supports[token] = support;
            currentSupports[materialId] = token;
            result["_source_support"] = JSONValue(token);
        } else {
            enforce((*id in supports) !is null, "Missing source CPU support");
            result["_source_support"] = JSONValue(*id);
        }
        foreach (key; ["cloud", "landmark_cloud", "draw_order_cloud", "alpha_runs_32", "alpha_runs_128"])
            result.object.remove(key);
        return result;
    }

public:
    /** Freeze an owned task result. Unchanged artifacts retain the same identity. */
    JSONValue snapshot(JSONValue state, bool freshSource = false) {
        synchronized (this) {
            auto values = JSONValue(state.object.dup);
            if (auto materials = "materials" in values.object) {
                JSONValue[] metadata;
                foreach (material; materials.array) metadata ~= materialMetadata(material,freshSource);
                values["materials"] = JSONValue(metadata);
            }
            JSONValue[string] runtime, references;
            foreach (key, value; values.object) {
                bool runtimeField;
                foreach (candidate; ["rootId", "source_sha256", "editor_signature", "model_sha256",
                    "completed_stage", "finish_stages", "options"])
                    if (key == candidate) runtimeField = true;
                if (runtimeField) runtime[key] = value;
                else references[key] = JSONValue(retain(key, value));
            }
            runtime["storage_schema"] = JSONValue("rig-native-state-d/2");
            runtime["artifact_refs"] = JSONValue(references);
            return JSONValue(runtime);
        }
    }

    /** Materialize a worker-owned calculation view; never expose shared mutable JSON. */
    JSONValue restore(JSONValue state) {
        synchronized (this) {
            enforce(ngRigString(state,"storage_schema","") == "rig-native-state-d/2",
                "Unsupported native rig state storage");
            auto result = JSONValue(state.object.dup);
            result.object.remove("artifact_refs");
            result.object.remove("storage_schema");
            foreach (name, reference; state["artifact_refs"].object) {
                auto encoded = reference.str in artifacts;
                enforce(encoded !is null, "Missing native rig artifact: " ~ name);
                result[name] = ngParseAutoRigJson(*encoded);
            }
            if (auto materials = "materials" in result.object) foreach (ref material; materials.array) {
                auto token = material["_source_support"].str;
                auto support = token in supports;
                enforce(support !is null, "Missing source CPU support");
                currentSupports[ngRigUnsigned(material["uuid"])] = token;
                material.object.remove("_source_support");
                foreach (key, points; support.points) {
                    JSONValue[] rows;
                    foreach (point; points) rows ~= JSONValue([point[0],point[1]]);
                    material[key] = JSONValue(rows);
                }
                foreach (key, runs; support.runs) material[key] = JSONValue(runs.dup);
            }
            return result;
        }
    }

    /** Read an independently owned artifact for the normal output viewer. */
    JSONValue artifact(JSONValue state, string name) {
        synchronized (this) {
            auto references = state["artifact_refs"].object;
            auto reference = name in references;
            enforce(reference !is null, "Missing native rig artifact reference: " ~ name);
            return ngParseAutoRigJson(artifacts[reference.str]);
        }
    }

    override JSONValue memoryInfo() {
        synchronized (this) {
            ulong jsonBytes, cpuBytes;
            JSONValue[] entries;
            foreach (id, value; artifacts) {
                jsonBytes += value.length;
                entries ~= JSONValue(["id":JSONValue(id),"name":JSONValue(names[id]),
                    "bytes":JSONValue(cast(ulong)value.length)]);
            }
            foreach (id, support; supports) {
                foreach (key, points; support.points) cpuBytes += points.length * Point2.sizeof;
                foreach (key, runs; support.runs) cpuBytes += runs.length * ulong.sizeof;
            }
            return JSONValue(["json_bytes":JSONValue(jsonBytes),"cpu_bytes":JSONValue(cpuBytes),
                "source_support_count":JSONValue(cast(ulong)supports.length),"artifacts":JSONValue(entries)]);
        }
    }

    override void dispose() {
        synchronized (this) { supports = null; artifacts = null; names = null; currentSupports = null; }
    }
}

RigStateStorage ngRigStateStorage(AutoRigTaskContext context) {
    return cast(RigStateStorage)context.workspace("rig-state", { return new RigStateStorage(); });
}
