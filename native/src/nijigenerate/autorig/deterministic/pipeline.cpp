#include "pipeline.hpp"
#include <iterator>
#include <utility>
namespace nijigenerate::autorig::deterministic::pipeline {
using namespace framework;
namespace {
template<std::size_t N> compat::RetainedStringSlice s(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
compat::RetainedStringSlice s(compat::RetainedStringSlice value) { return value; }
template<std::size_t N> AutoRigPortSpec port(const char (&id)[N], AutoRigValueKind kind, bool required = true) {
    return {s(id), kind, required};
}
}
compat::Slice<AutoRigTaskSpec> ngRigPipelineTasks() {
    const auto state = port("state", AutoRigValueKind::Json), program = port("program", AutoRigValueKind::Json);
    const auto model = port("model", AutoRigValueKind::Blob), materials = port("materials", AutoRigValueKind::Json);
    compat::Slice<AutoRigTaskSpec> result;
    result.resize(3);
    result[0] = AutoRigTaskSpec{s("observe-model"), ngAutoRigMessage(s("Observe imported model")), {},
        {port("options", AutoRigValueKind::Json)},
        {state, model, materials, port("observation", AutoRigValueKind::Json)}, {}};
    result[1] = AutoRigTaskSpec{s("compile-rig"), ngAutoRigMessage(s("Derive evidence and compile rig")), {},
        {state, materials, port("measurements", AutoRigValueKind::Object), port("options", AutoRigValueKind::Json)},
        {program, state, port("evidence", AutoRigValueKind::Json),
            port("target-associations", AutoRigValueKind::Object)}, {}};
    result[2] = AutoRigTaskSpec{s("measure-anatomy"), ngAutoRigMessage(s("Collect anatomical evidence")), {},
        {state}, {port("measurements", AutoRigValueKind::Object)}, {}};
    result.append(AutoRigTaskSpec{s("detect-rig-exclusions"), ngAutoRigMessage(s("Detect basic rig exclusions")), {},
        {state, port("options", AutoRigValueKind::Json)},
        {state, port("targets", AutoRigValueKind::Json)}, {}});
    result.append(AutoRigTaskSpec{s("verify-rig-exclusions"), ngAutoRigMessage(s("Verify basic rig exclusions")), {},
        {state, program}, {state, port("report", AutoRigValueKind::Json)}, {}});
    // The original map's enumeration is unspecified; every entry is retained once.
    auto stage = [](auto&& id, auto&& label) {
        return std::pair{s(id), ngAutoRigMessage(s(label))};
    };
    // The source evaluates every map value before the foreach appends tasks.
    const std::pair<compat::RetainedStringSlice, compat::RetainedStringSlice> stages[]{
        stage("mesh-parts", "Prepare Part meshes"), stage("build-native-rig", "Build anatomical rig"),
        stage("prepare-source-groups", "Prepare source groups and facial composites"),
        stage("prepare-feature-composites", "Mesh eye and mouth mechanism composites"),
        stage("weld-shoulders", "Weld matching anatomical attachment meshes"),
        stage("register-source-uv", "Register source texture placement"),
        stage("validate-depth-inputs", "Validate stored and effective depth"),
        stage("apply-fixed-foot", "Apply fixed-foot corrections"),
        stage("apply-rear-hair", "Apply rear hair corrections"),
        stage("apply-shape-corrections", "Apply near-cheek corrections"),
        stage("apply-secondary-physics", "Apply secondary physics"),
        stage("bake-depth-angles", "Bake face and body angles"),
        stage("apply-rig-controls", "Apply local rig controls")};
    for (const auto& [id, label] : stages)
        result.append(AutoRigTaskSpec{s(id), s(label), {}, {state, program, model}, {state, model}, {}});
    for (const auto& [id, label] : {stage("plan-fixed-foot", "Collect fixed-foot correction evidence"),
            stage("plan-rear-hair", "Collect rear hair correction evidence"),
            stage("plan-cheek", "Collect cheek correction evidence")})
        result.append(AutoRigTaskSpec{id, label, {}, {state, program},
            {port("plan", AutoRigValueKind::Object)}, {}});
    for (const auto& [id, label] : {stage("prepare-shoulders", "Collect shoulder seam evidence"),
            stage("detect-layered-ears", "Collect layered ear attachment evidence"),
            stage("detect-hip-attachments", "Collect hip attachment evidence")})
        result.append(AutoRigTaskSpec{id, label, {}, {state, program},
            {port("pairs", AutoRigValueKind::Object)}, {}});
    result.append(AutoRigTaskSpec{s("verify-saved-rig"), ngAutoRigMessage(s("Verify rig")), {}, {state, program, model},
        {port("report", AutoRigValueKind::Json)}, {}});
    result.append(AutoRigTaskSpec{s("compile-domain-layout"), ngAutoRigMessage(s("Compile shared source domains")), {},
        {state, program, model}, {state, program, model}, {}});
    for (auto& entry : result) {
        if (entry.id.view() == "apply-rig-controls" || entry.id.view() == "validate-depth-inputs" || entry.id.view() == "bake-depth-angles"
            || entry.id.view() == "apply-fixed-foot" || entry.id.view() == "apply-rear-hair"
            || entry.id.view() == "apply-shape-corrections" || entry.id.view() == "apply-secondary-physics" || entry.id.view() == "verify-saved-rig")
            entry.retainFailureOutputs = true;
    }
    for (auto& entry : result) {
        if (entry.id.view() == "apply-fixed-foot" || entry.id.view() == "apply-rear-hair" ||
            entry.id.view() == "apply-shape-corrections") {
            entry.inputs.append(port("plan", AutoRigValueKind::Object));
            entry.checkpointFallbacks = {s("state"), s("model")};
        }
        if (entry.id.view() == "mesh-parts")
            for (const auto pair : {s("shoulder-pairs"), s("ear-pairs"), s("hip-pairs")})
                entry.inputs.append({pair, AutoRigValueKind::Object, false});
        const auto id = entry.id.view();
        if (id == "observe-model" || id == "detect-rig-exclusions" || id == "measure-anatomy" ||
            id == "prepare-shoulders" || id.starts_with("detect-") || id.starts_with("plan-"))
            entry.phase = AutoRigTaskPhase::Collect;
        else if (id.starts_with("compile-")) entry.phase = AutoRigTaskPhase::Plan;
        else if (id.starts_with("verify-") || id == "validate-depth-inputs") entry.phase = AutoRigTaskPhase::Verify;
        if (id == "mesh-parts" || id == "register-source-uv" || id == "prepare-feature-composites" ||
            id == "weld-shoulders" || id == "apply-rig-controls" || id == "apply-rear-hair" ||
            id == "apply-fixed-foot" || id == "apply-shape-corrections" ||
            id == "apply-secondary-physics") entry.supportsTargetScope = true;
        entry.useCurrentCheckpoint = entry.supportsTargetScope;
        if (id == "prepare-feature-composites") {
            entry.targetTags = {s("face/eye"), s("face/mouth")};
            entry.tagCondition = s("face-mechanism");
        }
        if (id == "apply-rig-controls") {
            entry.targetTags = {s("face/eye"), s("face/mouth"), s("face/skin"), s("head/ear")};
            entry.tagCondition = s("face-control");
        }
        if (id == "plan-cheek" || id == "apply-shape-corrections") entry.targetTags = {s("face/cheek")};
        if (id == "plan-fixed-foot" || id == "apply-fixed-foot") {
            entry.targetTags = {s("body/leg")};
            entry.tagCondition = s("leg-grid");
        }
        if (id == "plan-rear-hair" || id == "apply-rear-hair") {
            entry.targetTags = {s("head/hair/back")};
            entry.tagCondition = s("rear-hair");
        }
        if (id == "plan-cheek" || id == "apply-shape-corrections")
            entry.tagCondition = s("face-ridge");
        if (id == "mesh-parts" || id == "register-source-uv" || id == "apply-secondary-physics") {
            entry.tagCondition = s("rig-material");
            entry.conditionTags = {s("rig/material")};
        }
        if (id == "weld-shoulders") {
            entry.tagCondition = s("attachment-seam");
            entry.conditionTags = {s("body/shoulder"), s("body/hip"), s("head/ear")};
        }
        if (id == "plan-cheek" || id == "prepare-shoulders" || id == "detect-layered-ears" ||
            id == "detect-hip-attachments")
            entry.outputs.append(port("target-associations", AutoRigValueKind::Object, false));
        if (id == "prepare-shoulders") entry.targetTags = {s("body/shoulder")};
        if (id == "detect-hip-attachments") entry.targetTags = {s("body/hip")};
        if (id == "detect-layered-ears") entry.targetTags = {s("head/ear")};
        entry.ownsActionBoundary = entry.id.view() != "compile-rig" && entry.id.view() != "detect-rig-exclusions"
            && entry.id.view() != "verify-rig-exclusions";
        entry.outputs.append(port("review", AutoRigValueKind::Json, false));
        if (entry.id.view() == "apply-rig-controls" || entry.id.view() == "weld-shoulders")
            entry.inputs.append(port("review", AutoRigValueKind::Json, false));
    }
    return result;
}
AutoRigWorkflowSpec ngRigModelWorkflow(compat::RetainedString provider) {
    AutoRigWorkflowSpec result;
    result.id = s("model-to-rig").retainedString();
    result.label = ngAutoRigMessage(s("Imported model to anatomical rig")).retainedString();
    result.preferred = true;
    result.inputDefaults[s("options").retainedString()] =
        AutoRigValue::jsonValue(core::settings::SettingsJsonObject{});
    result.description = ngAutoRigMessage(
        s("Observe, derive, mesh, build, bake, control and verify the imported model.")).retainedString();
    result.inputs.resize(1);
    result.inputs[0] = port("options", AutoRigValueKind::Json);
    result.outputs.resize(2);
    result.outputs[0] = port("model", AutoRigValueKind::Blob);
    result.outputs[1] = port("report", AutoRigValueKind::Json);
    const std::pair<compat::RetainedStringSlice, compat::RetainedStringSlice> steps[]{
        {s("source"), s("observe-model")}, {s("exclusions"), s("detect-rig-exclusions")},
        {s("measure"), s("measure-anatomy")}, {s("compile"), s("compile-rig")},
        {s("exclusion-check"), s("verify-rig-exclusions")},
        {s("groups"), s("prepare-source-groups")}, {s("shoulders"), s("prepare-shoulders")},
        {s("ear-attachments"), s("detect-layered-ears")}, {s("hip-attachments"), s("detect-hip-attachments")},
        {s("mesh"), s("mesh-parts")}, {s("uv"), s("register-source-uv")},
        {s("composites"), s("prepare-feature-composites")}, {s("layout"), s("compile-domain-layout")},
        {s("build"), s("build-native-rig")}, {s("weld"), s("weld-shoulders")},
        {s("controls"), s("apply-rig-controls")}, {s("depth"), s("validate-depth-inputs")},
        {s("bake"), s("bake-depth-angles")},
        {s("foot-plan"), s("plan-fixed-foot")}, {s("rear-plan"), s("plan-rear-hair")}, {s("cheek-plan"), s("plan-cheek")},
        {s("feet"), s("apply-fixed-foot")}, {s("rear"), s("apply-rear-hair")}, {s("corrections"), s("apply-shape-corrections")},
        {s("physics"), s("apply-secondary-physics")}, {s("verify"), s("verify-saved-rig")}};
    result.steps.resize(std::size(steps));
    for (std::size_t index = 0; index < std::size(steps); ++index) {
        const auto& [id, task] = steps[index];
        result.steps[index] = AutoRigWorkflowStep{s(id), provider, s(task), {}};
    }
    result.inputBindings = {{s("options"), s("source"), s(""), s("options")}};
    result.inputBindings.append(AutoRigWorkflowInputBinding{s("options"), s("compile"), s(""), s("options")});
    result.inputBindings.append(AutoRigWorkflowInputBinding{s("options"), s("exclusions"), s(""), s("options")});
    auto connect = [&](auto&& source, auto&& output, auto&& target, auto&& input) {
        result.connections.append(AutoRigWorkflowConnection{s(source), s(output), s(target), s(""), s(input)});
    };
    connect("source", "state", "exclusions", "state");
    connect("exclusions", "state", "measure", "state");
    connect("exclusions", "state", "compile", "state");
    connect("measure", "measurements", "compile", "measurements");
    connect("source", "materials", "compile", "materials");
    connect("compile", "state", "exclusion-check", "state");
    connect("compile", "program", "exclusion-check", "program");
    connect("exclusion-check", "state", "groups", "state"); connect("source", "model", "groups", "model");
    for (auto step : {s("groups"), s("shoulders"), s("ear-attachments"), s("hip-attachments"), s("mesh"), s("uv"), s("composites"), s("layout")})
        connect("compile", "program", step, "program");
    for (auto step : {s("build"), s("weld"), s("controls"), s("depth"), s("bake"), s("feet"), s("rear"), s("corrections"), s("physics"), s("verify")})
        connect("layout", "program", step, "program");
    for (auto step : {s("foot-plan"), s("rear-plan"), s("cheek-plan")}) {
        connect("bake", "state", step, "state");
        connect("layout", "program", step, "program");
    }
    connect("foot-plan", "plan", "feet", "plan");
    connect("rear-plan", "plan", "rear", "plan");
    connect("cheek-plan", "plan", "corrections", "plan");
    connect("groups", "state", "shoulders", "state");
    connect("groups", "state", "ear-attachments", "state");
    connect("groups", "state", "hip-attachments", "state");
    connect("shoulders", "pairs", "mesh", "shoulder-pairs");
    connect("ear-attachments", "pairs", "mesh", "ear-pairs");
    connect("hip-attachments", "pairs", "mesh", "hip-pairs");
    auto previous = s("groups");
    for (auto step : {s("mesh"), s("uv"), s("composites"), s("layout"), s("build"), s("weld"),
            s("controls"), s("depth"), s("bake"), s("feet"), s("rear"), s("corrections"), s("physics"), s("verify")}) {
        connect(previous, "state", step, "state"); connect(previous, "model", step, "model"); previous = step;
    }
    result.outputBindings.resize(2);
    result.outputBindings[0] = AutoRigWorkflowOutputBinding{s("model"), s("physics"), s("model")};
    result.outputBindings[1] = AutoRigWorkflowOutputBinding{s("report"), s("verify"), s("report")};
    return result;
}
}
