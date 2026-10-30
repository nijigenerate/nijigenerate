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
    result.resize(2);
    result[0] = AutoRigTaskSpec{s("observe-model"), ngAutoRigMessage(s("Observe imported model")), {},
        {port("options", AutoRigValueKind::Json)},
        {state, model, materials, port("observation", AutoRigValueKind::Json)}, {}};
    result[1] = AutoRigTaskSpec{s("compile-rig"), ngAutoRigMessage(s("Derive evidence and compile rig")), {},
        {state, materials, port("options", AutoRigValueKind::Json)},
        {program, state, port("evidence", AutoRigValueKind::Json)}, {}};
    // The original map's enumeration is unspecified; every entry is retained once.
    auto stage = [](auto&& id, auto&& label) {
        return std::pair{s(id), ngAutoRigMessage(s(label))};
    };
    // The source evaluates every map value before the foreach appends tasks.
    const std::pair<compat::RetainedStringSlice, compat::RetainedStringSlice> stages[]{
        stage("mesh-parts", "Prepare Part meshes"), stage("build-native-rig", "Build anatomical rig"),
        stage("prepare-source-groups", "Prepare source groups and facial composites"),
        stage("prepare-feature-composites", "Mesh eye and mouth mechanism composites"),
        stage("prepare-shoulders", "Measure proximal shoulder contours"),
        stage("weld-shoulders", "Connect matching native shoulder meshes"),
        stage("register-source-uv", "Register source texture placement"),
        stage("validate-depth-inputs", "Validate stored and effective depth"),
        stage("apply-shape-corrections", "Apply fixed-foot and near-cheek corrections"),
        stage("apply-secondary-physics", "Apply secondary physics"),
        stage("bake-depth-angles", "Bake face and body angles"),
        stage("apply-rig-controls", "Apply local rig controls")};
    for (const auto& [id, label] : stages)
        result.append(AutoRigTaskSpec{s(id), s(label), {}, {state, program, model}, {state, model}, {}});
    result.append(AutoRigTaskSpec{s("verify-saved-rig"), ngAutoRigMessage(s("Verify rig")), {}, {state, program, model},
        {port("report", AutoRigValueKind::Json)}, {}});
    result.append(AutoRigTaskSpec{s("compile-domain-layout"), ngAutoRigMessage(s("Compile shared source domains")), {},
        {state, program, model}, {state, program, model}, {}});
    for (auto& entry : result) {
        if (entry.id == "apply-rig-controls" || entry.id == "validate-depth-inputs" || entry.id == "bake-depth-angles"
            || entry.id == "apply-shape-corrections" || entry.id == "apply-secondary-physics" || entry.id == "verify-saved-rig")
            entry.retainFailureOutputs = true;
    }
    for (auto& entry : result) {
        entry.ownsActionBoundary = entry.id != "compile-rig";
        entry.outputs.append(port("review", AutoRigValueKind::Json, false));
        if (entry.id == "apply-rig-controls" || entry.id == "weld-shoulders")
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
        {s("source"), s("observe-model")}, {s("compile"), s("compile-rig")},
        {s("groups"), s("prepare-source-groups")}, {s("shoulders"), s("prepare-shoulders")},
        {s("mesh"), s("mesh-parts")}, {s("uv"), s("register-source-uv")},
        {s("composites"), s("prepare-feature-composites")}, {s("layout"), s("compile-domain-layout")},
        {s("build"), s("build-native-rig")}, {s("weld"), s("weld-shoulders")},
        {s("controls"), s("apply-rig-controls")}, {s("depth"), s("validate-depth-inputs")},
        {s("bake"), s("bake-depth-angles")}, {s("corrections"), s("apply-shape-corrections")},
        {s("physics"), s("apply-secondary-physics")}, {s("verify"), s("verify-saved-rig")}};
    result.steps.resize(std::size(steps));
    for (std::size_t index = 0; index < std::size(steps); ++index) {
        const auto& [id, task] = steps[index];
        result.steps[index] = AutoRigWorkflowStep{s(id), provider, s(task), {}};
    }
    result.inputBindings = {{s("options"), s("source"), s(""), s("options")}};
    result.inputBindings.append(AutoRigWorkflowInputBinding{s("options"), s("compile"), s(""), s("options")});
    auto connect = [&](auto&& source, auto&& output, auto&& target, auto&& input) {
        result.connections.append(AutoRigWorkflowConnection{s(source), s(output), s(target), s(""), s(input)});
    };
    connect("source", "state", "compile", "state"); connect("source", "materials", "compile", "materials");
    connect("compile", "state", "groups", "state"); connect("source", "model", "groups", "model");
    for (auto step : {s("groups"), s("shoulders"), s("mesh"), s("uv"), s("composites"), s("layout")})
        connect("compile", "program", step, "program");
    for (auto step : {s("build"), s("weld"), s("controls"), s("depth"), s("bake"), s("corrections"), s("physics"), s("verify")})
        connect("layout", "program", step, "program");
    auto previous = s("groups");
    for (auto step : {s("shoulders"), s("mesh"), s("uv"), s("composites"), s("layout"), s("build"), s("weld"),
            s("controls"), s("depth"), s("bake"), s("corrections"), s("physics"), s("verify")}) {
        connect(previous, "state", step, "state"); connect(previous, "model", step, "model"); previous = step;
    }
    result.outputBindings.resize(2);
    result.outputBindings[0] = AutoRigWorkflowOutputBinding{s("model"), s("physics"), s("model")};
    result.outputBindings[1] = AutoRigWorkflowOutputBinding{s("report"), s("verify"), s("report")};
    return result;
}
}
