module nijigenerate.autorig.deterministic.presentation;

import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.framework : ngAutoRigMessage;
import std.json : JSONValue, JSONType;
import std.conv : to;
import std.format : format;
import std.string : join, startsWith;

struct RigReviewTable {
    string title;
    string[] columns;
    string[][] rows;
    size_t page;
}

alias RigReviewTranslate = string delegate(string);

/** Resolve display names from the review schema, not the identifier-free material picker. */
string[ulong] ngRigReviewMaterialNames(JSONValue report) {
    string[ulong] names;
    if (report.type != JSONType.object) return names;
    auto classification = "classification" in report.object;
    if (classification is null || classification.type != JSONType.array) return names;
    foreach (row; classification.array) {
        if (row.type != JSONType.object) continue;
        auto id = "uuid" in row.object;
        auto path = "path" in row.object;
        auto name = "name" in row.object;
        if (id is null || id.type != JSONType.integer && id.type != JSONType.uinteger) continue;
        auto label = path !is null && path.type == JSONType.string ? path : name;
        if (label !is null && label.type == JSONType.string) names[ngRigUnsigned(*id)] = label.str;
    }
    return names;
}

private string numericText(JSONValue value) {
    if (value.type == JSONType.null_) return "—";
    if (value.type == JSONType.string) return value.str;
    if (value.type == JSONType.true_ || value.type == JSONType.false_) return value.boolean ? "✓" : "—";
    if (value.type == JSONType.integer) return value.integer.to!string;
    if (value.type == JSONType.uinteger) return value.uinteger.to!string;
    if (value.type == JSONType.float_) return format("%.2f",value.floating);
    if (value.type == JSONType.array) {
        string[] parts;
        foreach (entry; value.array) parts ~= numericText(entry);
        return parts.join(", ");
    }
    return "—";
}

/** Domain-specific tables, never a generic JSON tree or a dump of internal fields. */
RigReviewTable[] ngRigReviewTables(JSONValue report, string[ulong] names = null, RigReviewTranslate translate = null) {
    if (report.type != JSONType.object) return null;
    string t(string message) { return translate is null ? message : translate(message); }
    string name(JSONValue id) {
        auto found = ngRigUnsigned(id) in names;
        return found is null ? t(ngAutoRigMessage("Unresolved target")) : *found;
    }
    RigReviewTable[] tables;
    if (auto rows = "classification" in report.object) {
        auto table = RigReviewTable(t(ngAutoRigMessage("Material classification")),[
            t(ngAutoRigMessage("Material")),t(ngAutoRigMessage("Role")),t(ngAutoRigMessage("Feature")),
            t(ngAutoRigMessage("Rigging")),t(ngAutoRigMessage("Classification evidence"))]);
        foreach (row; rows.array) {
            names[ngRigUnsigned(row["uuid"])] = row["name"].str;
            auto stationary = ngRigGet(row,"static",JSONValue(!row["active"].boolean)).boolean;
            auto role = ngRigString(row,"role","");
            table.rows ~= [row["name"].str,ngRigReviewClassLabel(role,translate),
                ngRigReviewClassLabel(ngRigString(row,"feature",""),translate),
                t(stationary ? ngAutoRigMessage("Static artwork") : ngAutoRigMessage("Enabled")),
                ngRigReviewEvidenceLabel(ngRigString(row,"semantic_source",""),translate)];
        }
        tables ~= table;
    }
    if (auto rows = "carriers" in report.object) {
        auto table = RigReviewTable(t(ngAutoRigMessage("Rig targets and bone assignments")),[
            t(ngAutoRigMessage("Material")),t(ngAutoRigMessage("Model side")),t(ngAutoRigMessage("Bones"))]);
        foreach (row; rows.array) table.rows ~= [ngRigString(row,"path",name(row["part"])),
            ngRigReviewClassLabel(ngRigString(row,"side",""),translate),numericText(ngRigGet(row,"bones"))];
        tables ~= table;
    }
    if (auto scaffold = "scaffold" in report.object) if (auto rows = "bones" in scaffold.object) {
        auto table = RigReviewTable(t(ngAutoRigMessage("Anatomical scaffold")),[
            t(ngAutoRigMessage("Bone")),t(ngAutoRigMessage("Parent")),t(ngAutoRigMessage("Bone head position")),
            t(ngAutoRigMessage("Bone tail position")),t(ngAutoRigMessage("Bone rest angle"))]);
        foreach (row; rows.array) table.rows ~= [row["id"].str,numericText(row["parent"]),
            numericText(row["head"]),numericText(row["tail"]),numericText(ngRigGet(row,"rest_roll"))];
        tables ~= table;
    }
    if (auto hierarchy = "hierarchy" in report.object) if (auto groups = "groups" in hierarchy.object) {
        auto table = RigReviewTable(t(ngAutoRigMessage("Planned hierarchy")),[
            t(ngAutoRigMessage("Group")),t(ngAutoRigMessage("Parent")),t(ngAutoRigMessage("Bone")),
            t(ngAutoRigMessage("Position"))]);
        foreach (group; groups.array) {
            auto parent = group["parent"];
            table.rows ~= [group["id"].str,ngRigString(parent,"group",
                ("node" in parent.object) is null ? "—" : name(parent["node"])),
                group["bone"].str,numericText(group["origin"])];
        }
        tables ~= table;
    }
    if (auto rows = "changes" in report.object) {
        foreach (row; rows.array) foreach (key; ["before","after"]) {
            auto item = row[key];
            if (item.type != JSONType.object || !item["id"].str.startsWith("node:")) continue;
            names[item["id"].str[5 .. $].to!ulong] = item["name"].str;
        }
        auto table = RigReviewTable(t(ngAutoRigMessage("Model changes")),[
            t(ngAutoRigMessage("Target")),t(ngAutoRigMessage("Action")),t(ngAutoRigMessage("Changed item")),
            t(ngAutoRigMessage("Before")),t(ngAutoRigMessage("After"))]);
        foreach (row; rows.array) {
            auto action = row["change"].str;
            auto item = row[action == "removed" ? "before" : "after"];
            auto target = item["name"].str;
            if (action != "modified") {
                table.rows ~= [target,t(action == "added" ? ngAutoRigMessage("Added") : ngAutoRigMessage("Removed")),
                    ngRigReviewClassLabel(ngRigString(item,"type","Parameter"),translate),action == "removed" ? target : "—",
                    action == "added" ? target : "—"];
                if (action == "added") foreach (field; ["parent_name","rest_head","rest_tail","rest_roll",
                    "minimum","maximum","vertices","triangles"]) if (auto value = field in item.object)
                    table.rows ~= [target,t(ngAutoRigMessage("Added")),ngRigReviewFieldLabel(field,translate),
                        "—",numericText(*value)];
                continue;
            }
            bool visible;
            foreach (field; ["name","type","parent_name","enabled","zsort","translation","rotation","scale",
                "vertices","triangles","vertex_bounds","opacity","rest_head","rest_tail","rest_roll",
                "inherit_parent","minimum","maximum","default","axisX","axisY","maximum_offset"]) {
                auto change = field in row["fields"].object;
                if (change is null) continue;
                table.rows ~= [target,t(ngAutoRigMessage("Modified")),ngRigReviewFieldLabel(field,translate),
                    numericText((*change)["before"]),numericText((*change)["after"])];
                visible = true;
            }
            foreach (field; ["bindings","welding","masks"]) if (auto change = field in row["fields"].object) {
                auto summary(JSONValue items, bool updated = false) {
                    if (items.type != JSONType.array) return "—";
                    string[] entries;
                    foreach (entry; items.array) {
                        auto id = ngRigGet(entry,"target",ngRigGet(entry,"source"));
                        auto text = ngRigString(entry,"target_name",id.type == JSONType.null_ ? "" : name(id));
                        if (field == "bindings") text ~= format(" / %s / %s %s / %s",
                            entry["property"].str,numericText(entry["authored_keys"]),t(ngAutoRigMessage("keys")),
                            numericText(ngRigGet(entry,"maximum_offset")));
                        if (field == "welding") {
                            text ~= " / " ~ numericText(ngRigGet(entry,"paired_vertices")) ~ " " ~
                                t(ngAutoRigMessage("Paired vertices")) ~ " / " ~
                                t(ngAutoRigMessage("Weight")) ~ " " ~ numericText(ngRigGet(entry,"weight"));
                            if (updated) foreach (previous; (*change)["before"].array)
                                if (previous["target"] == entry["target"] &&
                                    ngRigGet(previous,"indices_sha256") != ngRigGet(entry,"indices_sha256"))
                                    text ~= " / " ~ t(ngAutoRigMessage("Correspondence updated"));
                        }
                        entries ~= text;
                    }
                    return entries.length ? entries.join("\n") : "—";
                }
                table.rows ~= [target,t(ngAutoRigMessage("Modified")),ngRigReviewFieldLabel(field,translate),
                    summary((*change)["before"]),summary((*change)["after"],true)]; visible = true;
            }
            if (!visible) table.rows ~= [target,t(ngAutoRigMessage("Modified")),
                t(ngAutoRigMessage("Mesh or deformation values")),t(ngAutoRigMessage("Previous values")),
                t(ngAutoRigMessage("Updated values"))];
        }
        if (!table.rows.length) table.rows ~= [t(ngAutoRigMessage("No model changes")),"—","—","—","—"];
        tables ~= table;
    }
    if (auto passed = "passed" in report.object) {
        auto table = RigReviewTable(t(ngAutoRigMessage("Verification")),[
            t(ngAutoRigMessage("Check")),t(ngAutoRigMessage("Result"))]);
        table.rows ~= [t(ngAutoRigMessage("Numerical rig verification")),
            t(passed.boolean ? ngAutoRigMessage("Passed") : ngAutoRigMessage("Failed"))];
        foreach (field; ["bones","baked_keys","intermediate_samples"])
            if (auto value = field in report.object) table.rows ~= [ngRigReviewFieldLabel(field,translate),numericText(*value)];
        if (auto findings = "numerical_findings" in report.object) foreach (finding; findings.array)
            table.rows ~= [ngRigString(finding,"parameter",t(ngAutoRigMessage("Model"))),finding["message"].str];
        if (auto error = "error" in report.object) table.rows ~= [t(ngAutoRigMessage("Error")),error.str];
        if (auto visual = "visual_review_required" in report.object) if (visual.boolean)
            table.rows ~= [t(ngAutoRigMessage("Visual review")),t(ngAutoRigMessage("Required"))];
        tables ~= table;
    }
    if (auto details = "details" in report.object) if (auto pairs = "shoulder_pairs" in details.object) {
        auto table = RigReviewTable(t(ngAutoRigMessage("Shoulder welding")),[
            t(ngAutoRigMessage("Source")),t(ngAutoRigMessage("Target")),t(ngAutoRigMessage("Result")),
            t(ngAutoRigMessage("Paired vertices"))]);
        foreach (pair; pairs.array) table.rows ~= [name(pair["source"]),name(pair["target"]),
            ngRigReviewClassLabel(ngRigString(pair,"status",pair["matching"].boolean ? "matching" : "not_matching"),translate),
            numericText(ngRigGet(pair,"paired_vertices"))];
        if (table.rows.length) tables ~= table;
    }
    if (auto details = "details" in report.object) if (auto depth = "depth_validation" in details.object) {
        auto table = RigReviewTable(t(ngAutoRigMessage("Depth verification")),[
            t(ngAutoRigMessage("Check")),t(ngAutoRigMessage("Result"))]);
        if (ngRigGet(*depth,"applicable",JSONValue(false)).boolean) {
            table.rows ~= [t(ngAutoRigMessage("Depth units")),t(ngAutoRigMessage("Verified"))];
            table.rows ~= [t(ngAutoRigMessage("Observed depth scale")),numericText((*depth)["observed_scale"])];
            table.rows ~= [t(ngAutoRigMessage("Planned depth scale")),numericText((*depth)["compiled_scale"])];
            table.rows ~= [t(ngAutoRigMessage("Depth surfaces")),(*depth)["surfaces"].array.length.to!string];
        } else table.rows ~= [t(ngAutoRigMessage("Depth surfaces")),t(ngAutoRigMessage("Not applicable"))];
        tables ~= table;
    }
    return tables;
}

string ngRigReviewFieldLabel(string field, RigReviewTranslate translate = null) {
    static immutable fields = ["name":ngAutoRigMessage("Name"),"type":ngAutoRigMessage("Node type"),
        "parent_name":ngAutoRigMessage("Parent"),"enabled":ngAutoRigMessage("Enabled"),
        "zsort":ngAutoRigMessage("Draw order"),"translation":ngAutoRigMessage("Position"),
        "rotation":ngAutoRigMessage("Rotation"),"scale":ngAutoRigMessage("Scale"),
        "vertices":ngAutoRigMessage("Vertices"),"triangles":ngAutoRigMessage("Triangles"),
        "vertex_bounds":ngAutoRigMessage("Mesh bounds"),"opacity":ngAutoRigMessage("Opacity"),
        "rest_head":ngAutoRigMessage("Bone head position"),"rest_tail":ngAutoRigMessage("Bone tail position"),
        "rest_roll":ngAutoRigMessage("Bone rest angle"),"inherit_parent":ngAutoRigMessage("Inherit parent motion"),
        "bindings":ngAutoRigMessage("Parameter bindings"),"welding":ngAutoRigMessage("Welding"),
        "masks":ngAutoRigMessage("Clipping masks"),"minimum":ngAutoRigMessage("Minimum"),
        "maximum":ngAutoRigMessage("Maximum"),"default":ngAutoRigMessage("Default value"),
        "axisX":ngAutoRigMessage("Horizontal keys"),"axisY":ngAutoRigMessage("Vertical keys"),
        "bones":ngAutoRigMessage("Bones"),"baked_keys":ngAutoRigMessage("Baked keys"),
        "intermediate_samples":ngAutoRigMessage("Intermediate samples")];
    auto label = field in fields;
    auto result = label is null ? field : *label;
    return translate is null ? result : translate(result);
}

string ngRigReviewClassLabel(string value, RigReviewTranslate translate = null) {
    static immutable labels = ["face":ngAutoRigMessage("Face"),"face_feature":ngAutoRigMessage("Facial feature"),
        "neck":ngAutoRigMessage("Neck"),"torso":ngAutoRigMessage("Torso"),"arm":ngAutoRigMessage("Arm"),
        "hand":ngAutoRigMessage("Hand"),"leg":ngAutoRigMessage("Leg"),"foot":ngAutoRigMessage("Foot"),
        "sleeve":ngAutoRigMessage("Sleeve"),"bodice":ngAutoRigMessage("Upper clothing"),
        "waistwear":ngAutoRigMessage("Waist clothing"),"thigh_accessory":ngAutoRigMessage("Thigh accessory"),
        "ankle_accessory":ngAutoRigMessage("Ankle accessory"),"skirt":ngAutoRigMessage("Front skirt"),
        "skirt_back":ngAutoRigMessage("Back skirt"),"apron":ngAutoRigMessage("Apron"),
        "shoulder":ngAutoRigMessage("Shoulder"),"ear":ngAutoRigMessage("Ear"),
        "headwear":ngAutoRigMessage("Head accessory"),"tail":ngAutoRigMessage("Tail"),
        "chest_accessory":ngAutoRigMessage("Chest accessory"),"pelvis_accessory":ngAutoRigMessage("Hip accessory"),
        "local":ngAutoRigMessage("Local mechanism"),"background":ngAutoRigMessage("Background"),
        "Part":ngAutoRigMessage("Material"),"GridDeformer":ngAutoRigMessage("Grid deformer"),
        "DepthBone":ngAutoRigMessage("Bone"),"DynamicComposite":ngAutoRigMessage("Dynamic composite"),
        "Composite":ngAutoRigMessage("Composite"),"Node":ngAutoRigMessage("Group"),
        "Parameter":ngAutoRigMessage("Parameter"),
        "L":ngAutoRigMessage("Model left"),"R":ngAutoRigMessage("Model right"),"Both":ngAutoRigMessage("Both sides"),
        "hair_front":ngAutoRigMessage("Front hair"),"hair_side":ngAutoRigMessage("Side hair"),
        "hair_back":ngAutoRigMessage("Back hair"),"sclera":ngAutoRigMessage("Eye white"),
        "iris":ngAutoRigMessage("Iris"),"upper":ngAutoRigMessage("Upper eyelid"),
        "lower":ngAutoRigMessage("Lower eyelid"),"corner":ngAutoRigMessage("Eye corner"),
        "brow":ngAutoRigMessage("Eyebrow"),"fold":ngAutoRigMessage("Eyelid fold"),"nose":ngAutoRigMessage("Nose"),
        "mouth":ngAutoRigMessage("Mouth"),"mouth_tongue":ngAutoRigMessage("Tongue"),
        "mouth_upper_teeth":ngAutoRigMessage("Upper teeth"),"mouth_lower_teeth":ngAutoRigMessage("Lower teeth"),
        "mouth_outline":ngAutoRigMessage("Mouth outline"),"mouth_upper_lip":ngAutoRigMessage("Upper lip"),
        "mouth_lower_lip":ngAutoRigMessage("Lower lip"),"disabled_by_user":ngAutoRigMessage("Disabled by user"),
        "not_matching":ngAutoRigMessage("No matching seam"),"matching":ngAutoRigMessage("Matching seam"),
        "no_native_vertex_pairs":ngAutoRigMessage("No matching vertices"),
        "applied_readback_verified":ngAutoRigMessage("Applied and verified")];
    auto label = value in labels;
    auto result = label is null ? value.length ? value : ngAutoRigMessage("None") : *label;
    return translate is null ? result : translate(result);
}

string ngRigReviewEvidenceLabel(string value, RigReviewTranslate translate = null) {
    static immutable labels = ["explicit_override":ngAutoRigMessage("Explicit review setting"),
        "model_name_and_ancestry_candidate":ngAutoRigMessage("Material name and parent hierarchy"),
        "full_body_backdrop_alpha_perimeter":ngAutoRigMessage("Background detected from alpha extent"),
        "imported_clipping_receiver":ngAutoRigMessage("Inherited from clipping receiver"),
        "local_mechanism_without_anatomy":ngAutoRigMessage("Local mechanism without anatomical support"),
        "alpha_proximal_support_candidate":ngAutoRigMessage("Nearest anatomical alpha support")];
    auto label = value in labels;
    auto result = label is null ? value.length ? value : ngAutoRigMessage("Not classified yet") : *label;
    return translate is null ? result : translate(result);
}
