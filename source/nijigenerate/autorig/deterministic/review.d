module nijigenerate.autorig.deterministic.review;

import nijigenerate.autorig.deterministic.contracts;
import std.json : JSONValue, JSONType;
import std.conv : to;
import std.exception : enforce;
import std.algorithm : sort;

enum RigMaterialField : ubyte { role = 1, stationary = 2, feature = 4, side = 8 }

bool ngRigMaterialForcedStatic(bool active, string role, string semanticSource) {
    return !active || role == "background" || semanticSource == "full_body_backdrop_alpha_perimeter";
}

/** Merge only edited UI fields; an empty role restores automatic role inference. */
JSONValue ngRigEditedMaterialOverride(JSONValue previous, ubyte fields, string role,
    bool stationary, string feature, string side) {
    auto row = previous.type == JSONType.object ? previous.object.dup : cast(JSONValue[string])null;
    if (fields & RigMaterialField.role) {
        if (role.length) row["role"] = JSONValue(role);
        else row.remove("role");
    }
    if (fields & RigMaterialField.stationary) row["static"] = JSONValue(stationary);
    if (fields & RigMaterialField.feature) row["feature"] = JSONValue(feature);
    if (fields & RigMaterialField.side) row["side"] = JSONValue(side);
    return JSONValue(row);
}

/** Small, independent review artifacts never retain textures or dense vertex fields. */
JSONValue ngRigReviewCompact(JSONValue value, size_t depth = 0) {
    if (value.type == JSONType.array) {
        if (depth > 8 || value.array.length > 32 && value.array[0].type != JSONType.object)
            return JSONValue(["items":JSONValue(value.array.length)]);
        JSONValue[] items;
        foreach (entry; value.array) items ~= ngRigReviewCompact(entry, depth + 1);
        return JSONValue(items);
    }
    if (value.type == JSONType.object) {
        JSONValue[string] result;
        foreach (key, entry; value.object) {
            if (key == "data" || key == "alpha" || key == "cloud" || key == "offsets" ||
                key == "values" || key == "depth" || key == "draw_order_cloud" || key == "mapping") continue;
            if ((key == "vertices" || key == "uv" || key == "triangles" || key == "points" ||
                key == "allowed_vertices" || key == "skin_protected_vertices") && entry.type == JSONType.array)
                result[key] = JSONValue(["items":JSONValue(entry.array.length)]);
            else result[key] = depth > 8 ? JSONValue("…") : ngRigReviewCompact(entry, depth + 1);
        }
        return JSONValue(result);
    }
    return value;
}

JSONValue ngRigClassificationReview(JSONValue observation, JSONValue program) {
    JSONValue[] rows;
    foreach (material; observation["materials"].array) {
        JSONValue[string] row;
        foreach (key; ["uuid", "name", "path", "active", "static", "role", "feature", "side_hint", "side_override",
            "owner", "chart", "semantic_source", "support_part", "receiver"])
            if (auto value = key in material.object) row[key] = ngRigReviewCompact(*value);
        rows ~= JSONValue(row);
    }
    auto result = JSONValue(["classification":JSONValue(rows)]);
    if (program.type == JSONType.object) {
        if (auto scaffold = "scaffold" in program.object) result["scaffold"] = ngRigReviewCompact(*scaffold);
        if (auto hierarchy = "hierarchy" in program.object) result["hierarchy"] = ngRigReviewCompact(*hierarchy);
        JSONValue[] carriers;
        if (auto targets = "carriers" in program.object) foreach (target; targets.array) {
            JSONValue[string] row;
            foreach (key; ["part", "path", "role", "side", "owner", "chart", "bones", "domain_id"])
                if (auto entry = key in target.object) row[key] = *entry;
            carriers ~= JSONValue(row);
        }
        result["carriers"] = JSONValue(carriers);
    }
    return result;
}

bool ngRigReviewEnabled(JSONValue review, string id) {
    if (review.type == JSONType.null_) return true;
    enforce(review.type == JSONType.object, "Rig review settings must be an object");
    auto disabled = "disabled" in review.object;
    if (disabled is null) return true;
    foreach (entry; disabled.array) if (entry.str == id) return false;
    return true;
}

string ngRigReviewOperationId(string kind, JSONValue operation, string name = "") {
    auto part = ngRigUnsigned(operation[kind == "weld" ? "source" : "part"]).to!string;
    if (kind == "weld") return "weld:" ~ part ~ ":" ~ ngRigUnsigned(operation["target"]).to!string;
    if (kind == "mask") return "mask:" ~ part ~ ":" ~ ngRigUnsigned(operation["source"]).to!string;
    return kind ~ ":" ~ (name.length ? name ~ ":" : "") ~ part;
}

/** Observed feature frames remain available when authored controls are disabled. */
JSONValue ngRigControlGeometry(JSONValue controls) {
    JSONValue[] mechanisms;
    foreach (mechanism; controls["mechanisms"].array) {
        JSONValue[string] frame = ["name":mechanism["name"]];
        foreach (key; ["contact_curves","source_frame"])
            if (auto value = key in mechanism.object) frame[key] = *value;
        if (frame.length>1) mechanisms ~= JSONValue(frame);
    }
    return JSONValue(["mechanisms":JSONValue(mechanisms)]);
}

/** Filter the compiled plan before mutation; verification consumes this same filtered plan. */
JSONValue ngRigReviewControls(JSONValue controls, JSONValue review, out JSONValue operations) {
    JSONValue[] rows, mechanisms;
    void record(string id, JSONValue item) {
        rows ~= JSONValue(["id":JSONValue(id), "enabled":JSONValue(ngRigReviewEnabled(review,id)),
            "plan":ngRigReviewCompact(item)]);
    }
    foreach (mechanism; controls["mechanisms"].array) {
        auto name = mechanism["name"].str;
        auto id = "mechanism:" ~ name;
        record(id, JSONValue(["parameter":JSONValue(name), "axisX":mechanism["axisX"], "axisY":mechanism["axisY"]]));
        JSONValue[] kept;
        foreach (operation; mechanism["operations"].array) {
            auto operationId = ngRigReviewOperationId("control",operation,name);
            record(operationId, JSONValue(["parameter":JSONValue(name), "part":operation["part"],
                "keys":JSONValue(operation["keys"].array.length)]));
            if (ngRigReviewEnabled(review,id) && ngRigReviewEnabled(review,operationId)) kept ~= operation;
        }
        if (kept.length) {
            auto copy = JSONValue(mechanism.object.dup); copy["operations"] = JSONValue(kept); mechanisms ~= copy;
        }
    }
    auto result = JSONValue(controls.object.dup); result["mechanisms"] = JSONValue(mechanisms);
    foreach (key; ["masks", "draw_order"]) {
        JSONValue[] kept;
        foreach (operation; controls[key].array) {
            auto id = ngRigReviewOperationId(key == "masks" ? "mask" : "zsort",operation);
            record(id, operation);
            if (ngRigReviewEnabled(review,id)) kept ~= operation;
        }
        result[key] = JSONValue(kept);
    }
    operations = JSONValue(rows);
    return result;
}

/** Compare compact, main-thread snapshots by stable identity. */
JSONValue ngRigReviewChanges(JSONValue before, JSONValue after) {
    JSONValue[string] oldItems, newItems;
    foreach (item; before.array) oldItems[item["id"].str] = item;
    foreach (item; after.array) newItems[item["id"].str] = item;
    string[] ids = oldItems.keys;
    foreach (id; newItems.keys) if ((id in oldItems) is null) ids ~= id;
    ids.sort;
    JSONValue[] changes;
    foreach (id; ids) {
        auto oldItem = id in oldItems, newItem = id in newItems;
        if (oldItem !is null && newItem !is null && *oldItem == *newItem) continue;
        JSONValue[string] fields;
        if (oldItem !is null && newItem !is null) {
            foreach (key, value; newItem.object) {
                auto previous = key in oldItem.object;
                if (previous is null || *previous != value) fields[key] = JSONValue([
                    "before":previous is null ? JSONValue.init : *previous, "after":value]);
            }
            foreach (key,value; oldItem.object) if ((key in newItem.object) is null)
                fields[key] = JSONValue(["before":value,"after":JSONValue.init]);
        }
        changes ~= JSONValue(["id":JSONValue(id),
            "change":JSONValue(oldItem is null ? "added" : newItem is null ? "removed" : "modified"),
            "fields":JSONValue(fields),
            "before":oldItem is null ? JSONValue.init : *oldItem,
            "after":newItem is null ? JSONValue.init : *newItem]);
    }
    return JSONValue(changes);
}
