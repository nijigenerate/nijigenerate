module nijigenerate.autorig.deterministic.pipeline;

import nijigenerate.autorig.framework;
import std.json : JSONValue;

alias RigNativeStage = JSONValue delegate(string, JSONValue, JSONValue, ubyte[], AutoRigTaskContext);

AutoRigTaskSpec[] ngRigPipelineTasks() {
    auto state = AutoRigPortSpec("state",AutoRigValueKind.Json);
    auto program = AutoRigPortSpec("program",AutoRigValueKind.Json);
    auto model = AutoRigPortSpec("model",AutoRigValueKind.Blob);
    auto materials = AutoRigPortSpec("materials",AutoRigValueKind.Json);
    AutoRigTaskSpec[] result = [
        AutoRigTaskSpec("observe-model",ngAutoRigMessage("Observe imported model"),null,
            [AutoRigPortSpec("options",AutoRigValueKind.Json)],
            [state,model,materials,AutoRigPortSpec("observation",AutoRigValueKind.Json)],null),
        AutoRigTaskSpec("compile-rig",ngAutoRigMessage("Derive evidence and compile rig"),null,
            [state,materials,AutoRigPortSpec("options",AutoRigValueKind.Json)],
            [program,state,AutoRigPortSpec("evidence",AutoRigValueKind.Json)],null)
    ];
    foreach (id, label; ["mesh-parts":ngAutoRigMessage("Prepare Part meshes"), "build-native-rig":ngAutoRigMessage("Build anatomical rig"),
        "prepare-source-groups":ngAutoRigMessage("Prepare source groups and facial composites"),
        "prepare-feature-composites":ngAutoRigMessage("Mesh eye and mouth mechanism composites"),
        "prepare-shoulders":ngAutoRigMessage("Measure proximal shoulder contours"),
        "weld-shoulders":ngAutoRigMessage("Connect matching native shoulder meshes"),
        "register-source-uv":ngAutoRigMessage("Register source texture placement"),
        "validate-depth-inputs":ngAutoRigMessage("Validate stored and effective depth"),
        "apply-shape-corrections":ngAutoRigMessage("Apply fixed-foot and near-cheek corrections"),
        "bake-depth-angles":ngAutoRigMessage("Bake face and body angles"), "apply-rig-controls":ngAutoRigMessage("Apply local rig controls")])
        result ~= AutoRigTaskSpec(id,label,null,[state,program,model],[state,model],null);
    result ~= AutoRigTaskSpec("verify-saved-rig",ngAutoRigMessage("Verify rig"),null,[state,program,model],
        [AutoRigPortSpec("report",AutoRigValueKind.Json)],null);
    result ~= AutoRigTaskSpec("compile-domain-layout",ngAutoRigMessage("Compile shared source domains"),null,[state,program,model],
        [state,program,model],null);
    foreach (ref entry; result) if (entry.id == "apply-rig-controls" || entry.id == "validate-depth-inputs" ||
        entry.id == "bake-depth-angles" || entry.id == "apply-shape-corrections" ||
        entry.id == "verify-saved-rig") entry.retainFailureOutputs = true;
    foreach (ref entry; result) {
        entry.ownsActionBoundary = entry.id != "compile-rig";
        entry.outputs ~= AutoRigPortSpec("review",AutoRigValueKind.Json,false);
        if (entry.id == "apply-rig-controls" || entry.id == "weld-shoulders")
            entry.inputs ~= AutoRigPortSpec("review",AutoRigValueKind.Json,false);
    }
    return result;
}

AutoRigWorkflowSpec ngRigModelWorkflow(string provider) {
    AutoRigWorkflowSpec result;
    result.id = "model-to-rig"; result.label = ngAutoRigMessage("Imported model to anatomical rig");
    result.preferred = true;
    result.inputDefaults["options"] = AutoRigValue.jsonValue(JSONValue(cast(JSONValue[string])null));
    result.description = ngAutoRigMessage("Observe, derive, mesh, build, bake, control and verify the imported model.");
    result.inputs = [AutoRigPortSpec("options",AutoRigValueKind.Json)];
    result.outputs = [AutoRigPortSpec("model",AutoRigValueKind.Blob),AutoRigPortSpec("report",AutoRigValueKind.Json)];
    result.steps = [AutoRigWorkflowStep("source",provider,"observe-model"),AutoRigWorkflowStep("compile",provider,"compile-rig"),
        AutoRigWorkflowStep("groups",provider,"prepare-source-groups"),
        AutoRigWorkflowStep("shoulders",provider,"prepare-shoulders"),
        AutoRigWorkflowStep("mesh",provider,"mesh-parts"),AutoRigWorkflowStep("uv",provider,"register-source-uv"),
        AutoRigWorkflowStep("composites",provider,"prepare-feature-composites"),
        AutoRigWorkflowStep("layout",provider,"compile-domain-layout"),
        AutoRigWorkflowStep("build",provider,"build-native-rig"),
        AutoRigWorkflowStep("weld",provider,"weld-shoulders"),
        AutoRigWorkflowStep("controls",provider,"apply-rig-controls"),
        AutoRigWorkflowStep("depth",provider,"validate-depth-inputs"),AutoRigWorkflowStep("bake",provider,"bake-depth-angles"),
        AutoRigWorkflowStep("corrections",provider,"apply-shape-corrections"),
        AutoRigWorkflowStep("verify",provider,"verify-saved-rig")];
    result.inputBindings = [AutoRigWorkflowInputBinding("options","source","","options")];
    result.inputBindings ~= AutoRigWorkflowInputBinding("options","compile","","options");
    void connect(string source, string output, string target, string input) {
        result.connections ~= AutoRigWorkflowConnection(source,output,target,"",input);
    }
    connect("source","state","compile","state");
    connect("source","materials","compile","materials");
    connect("compile","state","groups","state"); connect("source","model","groups","model");
    foreach (step; ["groups","shoulders","mesh","uv","composites","layout"]) connect("compile","program",step,"program");
    foreach (step; ["build","weld","controls","depth","bake","corrections","verify"]) connect("layout","program",step,"program");
    string previous = "groups";
    foreach (step; ["shoulders","mesh","uv","composites","layout","build","weld","controls","depth","bake","corrections","verify"]) {
        connect(previous,"state",step,"state"); connect(previous,"model",step,"model"); previous = step;
    }
    result.outputBindings = [AutoRigWorkflowOutputBinding("model","corrections","model"),
        AutoRigWorkflowOutputBinding("report","verify","report")];
    return result;
}
