module nijigenerate.autorig.deterministic.templates;

import std.json : JSONValue, parseJSON;
import std.exception : enforce;
import std.digest.sha : sha256Of;
import std.digest : toHexString;
import std.string : toLower;

/** Compile-time resources; callers receive independent parsed JSON values. */
private immutable string[string] resources;
shared static this() {
    resources = [
        "face_head": import("autorig/templates/face_head.json"),
        "eye_opening": import("autorig/templates/eye_opening.json"),
        "mouth_opening": import("autorig/templates/mouth_opening.json"),
        "torso": import("autorig/templates/torso.json"),
        "neck": import("autorig/templates/neck.json"),
        "shoulder": import("autorig/templates/shoulder.json"),
        "arm": import("autorig/templates/arm.json"),
        "leg": import("autorig/templates/leg.json"),
        "hand": import("autorig/templates/hand.json"),
        "foot": import("autorig/templates/foot.json"),
        "skirt": import("autorig/templates/skirt.json"),
        "sleeve": import("autorig/templates/sleeve.json"),
        "hanging_sheet": import("autorig/templates/hanging_sheet.json"),
        "tube": import("autorig/templates/tube.json"),
        "leaf": import("autorig/templates/leaf.json"),
        "ribbon_network": import("autorig/templates/ribbon_network.json"),
        "frill": import("autorig/templates/frill.json"),
        "surface_layer": import("autorig/templates/surface_layer.json"),
        "rigid_panel": import("autorig/templates/rigid_panel.json")
    ];
}

string[] ngRigTemplateNames() {
    return ["face_head", "eye_opening", "mouth_opening", "torso", "neck", "shoulder", "arm", "leg", "hand", "foot", "skirt", "sleeve", "hanging_sheet", "tube", "leaf", "ribbon_network", "frill", "surface_layer", "rigid_panel"];
}

JSONValue ngRigTemplate(string id) {
    auto source = id in resources;
    enforce(source !is null, "Unknown embedded rig template: " ~ id);
    auto result = parseJSON(*source);
    enforce(result["id"].str == id && result["schema_version"].str == "semantic-grid-template/1",
        "Invalid embedded rig template identity");
    return result;
}

JSONValue ngRigTemplateManifest() {
    return parseJSON(import("autorig/templates/manifest.json"));
}

JSONValue ngRigTemplateSchema() {
    return parseJSON(import("autorig/templates/template.schema.json"));
}

JSONValue ngRigHumanoidPrior() {
    return parseJSON(import("autorig/humanoid-prior.json"));
}

JSONValue ngRigMaterialRoles() {
    return parseJSON(import("autorig/material-roles.json"));
}

void ngRigValidateEmbeddedTemplates() {
    auto manifest = ngRigTemplateManifest();
    enforce(manifest["templates"].array.length == resources.length, "Embedded template count mismatch");
    bool[string] seen;
    foreach (entry; manifest["templates"].array) {
        auto id = entry["id"].str;
        enforce((id in seen) is null, "Duplicate embedded template identity");
        seen[id] = true;
        auto source = id in resources;
        enforce(source !is null, "Manifest template is missing: " ~ id);
        auto digest = sha256Of(*source).toHexString;
        enforce(digest[].toLower == entry["sha256"].str, "Embedded template hash mismatch: " ~ id);
        auto value = ngRigTemplate(id);
        enforce(value["version"].str == entry["version"].str, "Embedded template version mismatch");
    }
}
