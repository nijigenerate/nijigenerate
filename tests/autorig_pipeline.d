module autorig_pipeline;

import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.deterministic.evidence;
import nijigenerate.autorig.deterministic.program;
import nijigenerate.autorig.deterministic.controls;
import nijigenerate.autorig.deterministic.observation;
import nijigenerate.autorig.deterministic.geometry : ngRigSourceUVRegistration, ngRigVerifySourceUV,
    ngRigClosestTriangleWeights, ngRigSameTextureMapping, ngRigValidateGrid;
import nijigenerate.autorig.deterministic.processor;
import nijigenerate.autorig.framework;
import nijigenerate.autorig.workflow;
import core.thread.fiber : Fiber;
import std.json : JSONValue, parseJSON;
import nijigenerate.autorig.json : ngParseAutoRigJson;
import std.file : tempDir, write;
import std.path : buildPath;
import std.uuid : randomUUID;
import std.math : abs, isFinite;

JSONValue ngTestModelObservation() {
    JSONValue[] materials;
    void rectangle(string name, double[4] bounds) {
        Point2[] points;
        foreach (y; cast(int)bounds[1] .. cast(int)bounds[3]+1)
            foreach (x; cast(int)bounds[0] .. cast(int)bounds[2]+1) { Point2 p = [cast(double)x,cast(double)y]; points ~= p; }
        materials ~= JSONValue(["name":JSONValue(name),"path":JSONValue("/" ~ name),
            "uuid":JSONValue(cast(ulong)(materials.length+1)),"active":JSONValue(true),
            "bounds":JSONValue(bounds[]),"cloud":ngRigPointsJson(points)]);
    }
    rectangle("face",[-12.,0.,12.,20.]); rectangle("neck",[-3.,19.,3.,31.]);
    rectangle("torso",[-15.,30.,15.,65.]);
    rectangle("arm_r",[-30.,29.,-20.,65.]); rectangle("hand_r",[-30.,65.,-20.,76.]);
    rectangle("arm_l",[20.,29.,30.,65.]); rectangle("hand_l",[20.,65.,30.,76.]);
    rectangle("leg_r",[-12.,65.,-4.,105.]); rectangle("shoe_r",[-12.,105.,-3.,112.]);
    rectangle("leg_l",[4.,65.,12.,105.]); rectangle("shoe_l",[3.,105.,12.,112.]);
    rectangle("sclera_r",[-9.,7.,-3.,11.]); rectangle("sclera_l",[3.,7.,9.,11.]);
    rectangle("iris_r",[-7.,7.,-5.,11.]); rectangle("iris_l",[5.,7.,7.,11.]);
    rectangle("mouth",[-4.,14.,4.,16.]);
    rectangle("nose",[-1.,11.,1.,13.]);
    return JSONValue(["rootId":JSONValue(cast(uint)100),"source_sha256":JSONValue("synthetic-test"),
        "options":JSONValue(cast(JSONValue[string])null),"materials":JSONValue(materials)]);
}

void ngTestRigPipeline() {
    // A landmark-aligned narrow cell remains valid relative to its own rest area.
    Point2[] narrow = [[0.,0.],[.0002,0.],[10.,0.],[0.,1.],[.0002,1.],[10.,1.]];
    auto narrowCheck = ngRigValidateGrid(narrow,3,.02,narrow);
    assert(ngRigNumber(narrowCheck["min_area_ratio"]) == 1);
    auto folded = narrow.dup; folded[4] = [-.0002,1.];
    bool rejected;
    try { ngRigValidateGrid(folded,3,.02,narrow); } catch (Exception error) { rejected = true; }
    assert(rejected);
    auto original = parseJSON(ngTestModelObservation().toString());
    assert(ngRigMaterialRoleCandidates("hair-curl-r") == ["hair_side"]);
    assert(ngRigMaterialRoleCandidates("plush-head") == ["pelvis_accessory"]);
    assert(ngRigMaterialRoleCandidates("earring-l") == ["headwear"]);
    assert(ngRigMaterialRoleCandidates("eye-white-r") == ["face_feature"]);
    assert(ngRigMaterialRoleCandidates("*# eywhite") == null);
    assert(ngRigMaterialRoleCandidates("*# eyewhite") == ["face_feature"]);
    auto unknownSource = ngTestModelObservation();
    auto unknown = JSONValue(unknownSource["materials"][0].object.dup);
    unknown["uuid"] = JSONValue(cast(uint)999);
    unknown["name"] = JSONValue("影_髪から顔への落ち影");
    unknown["path"] = JSONValue("/MODEL/unknown-shadow");
    unknownSource["materials"].array ~= unknown;
    auto inherited = ngRigClassifyMaterials(unknownSource,unknownSource["options"]);
    auto shadow = inherited["materials"].array[$-1];
    assert(shadow["role"].str == "face" && shadow["semantic_source"].str == "alpha_proximal_support_candidate");
    unknown["receiver"] = JSONValue(cast(uint)3);
    unknownSource["materials"].array[$-1] = unknown;
    inherited = ngRigClassifyMaterials(unknownSource,unknownSource["options"]);
    assert(inherited["materials"].array[$-1]["role"].str == "torso");
    auto observation = ngRigClassifyMaterials(original,original["options"]);
    auto stationary = ngRigClassifyMaterials(original,JSONValue(["materials":JSONValue([
        "/face":JSONValue(["static":JSONValue(true)])])]));
    assert(stationary["materials"][0]["static"].boolean);
    auto evidence = ngRigDeriveEvidence(observation);
    auto program = ngRigCompileProgram(observation,evidence);
    assert(program["scaffold"]["bones"].array.length == 21);
    assert(program["carriers"].array.length == original["materials"].array.length);
    assert(program["domains"].array.length<program["carriers"].array.length);
    assert(program["domains"].array.length == 6);
    assert(program["hierarchy"]["groups"].array.length == 6);
    assert(ngRigUnsigned(program["hierarchy"]["body_origin"]) == 3);
    assert(ngRigUnsigned(program["hierarchy"]["face_origin"]) == 1);
    assert(program["hierarchy"]["groups"][1]["id"].str == "Head::Root");
    assert(ngRigUnsigned(program["hierarchy"]["groups"][1]["parent"]["node"]) == 3);
    // Model identity is provenance, not a rigging policy. Reassign every
    // imported identity and source location while preserving semantic artwork.
    auto anonymous = parseJSON(original.toString());
    anonymous["rootId"] = JSONValue(cast(uint)700100);
    anonymous["source_sha256"] = JSONValue("different-source-identity");
    foreach (ref material; anonymous["materials"].array) {
        material["uuid"] = JSONValue(ngRigUnsigned(material["uuid"])+700000);
        material["path"] = JSONValue("/asset_copy" ~ material["path"].str);
    }
    anonymous = ngRigClassifyMaterials(anonymous,anonymous["options"]);
    auto anonymousProgram = ngRigCompileProgram(anonymous,ngRigDeriveEvidence(anonymous));
    assert(anonymousProgram["scaffold"]["bones"] == program["scaffold"]["bones"]);
    assert(anonymousProgram["native_drivers"] == program["native_drivers"]);
    assert(anonymousProgram["native_depth_scale"] == program["native_depth_scale"]);
    assert(anonymousProgram["reference_template"] == program["reference_template"]);
    foreach (i, carrier; program["carriers"].array) {
        auto renamed = anonymousProgram["carriers"][i];
        assert(ngRigUnsigned(renamed["part"]) == ngRigUnsigned(carrier["part"])+700000);
        foreach (field; ["domain_id","owner","chart","bones","xs","ys","depth","depth_model_units"])
            assert(renamed[field] == carrier[field],"Model identity changed " ~ field);
    }
    assert(ngRigUnsigned(anonymousProgram["hierarchy"]["body_origin"]) == 700003);
    assert(ngRigUnsigned(anonymousProgram["hierarchy"]["face_origin"]) == 700001);
    auto partitioned = parseJSON(observation.toString());
    partitioned["groups"] = JSONValue([
        JSONValue(["uuid":JSONValue(501),"parent":JSONValue(100),"name":JSONValue("proximal")]),
        JSONValue(["uuid":JSONValue(502),"parent":JSONValue(100),"name":JSONValue("distal")])]);
    partitioned["materials"][3]["parent"] = JSONValue(501);
    partitioned["materials"][4]["parent"] = JSONValue(502);
    auto sharedProgram = ngRigCompileProgram(partitioned,ngRigDeriveEvidence(partitioned));
    assert(sharedProgram["domains"].array.length == 6);
    assert(sharedProgram["carriers"][3]["domain_id"].str == sharedProgram["carriers"][4]["domain_id"].str);
    assert(sharedProgram["carriers"][3]["bones"] == sharedProgram["carriers"][4]["bones"]);
    auto spanning = parseJSON(original.toString());
    auto bridge = JSONValue(spanning["materials"][3].object.dup);
    bridge["uuid"] = JSONValue(777); bridge["name"] = JSONValue("arm_shared"); bridge["path"] = JSONValue("/arm_shared");
    auto points = ngRigPoints(bridge["cloud"]); auto bothPoints = points.dup;
    foreach (p; points) { p[0] = -p[0]; bothPoints ~= p; }
    bridge["cloud"] = ngRigPointsJson(bothPoints); bridge["bounds"] = JSONValue([-30.,29.,30.,65.]);
    spanning["materials"].array ~= bridge;
    spanning = ngRigClassifyMaterials(spanning,spanning["options"]);
    auto spanningProgram = ngRigCompileProgram(spanning,ngRigDeriveEvidence(spanning));
    auto sharedArm = spanningProgram["carriers"].array[$-1];
    assert(sharedArm["owner"].str == "arm:both" && sharedArm["side"].str == "Both");
    assert(sharedArm["bones"].array.length == 6);
    bool hasRelief;
    foreach (z; ngRigNumbers(sharedArm["depth"])) { assert(isFinite(z)); if (z != 0) hasRelief = true; }
    assert(hasRelief);
    assert(ngRigNumber(program["native_depth_scale"]) > 1);
    JSONValue[] targets;
    foreach (carrier; program["carriers"].array) {
        auto target = carrier;
        target["grid"] = carrier["part"];
        auto bounds = ngRigNumbers(carrier["bounds"]);
        Point2[] world = [[bounds[0],bounds[1]],[bounds[2],bounds[1]],[bounds[0],bounds[3]],[bounds[2],bounds[3]]];
        target["world"] = ngRigPointsJson(world);
        target["mapping"] = JSONValue(["vertices":ngRigPointsJson(world),
            "triangles":JSONValue([JSONValue([0,1,2]),JSONValue([1,3,2])])]);
        target["root_to_local_direction"] = JSONValue([1.,0.,0.,1.]);
        targets ~= target;
    }
    observation["targets"] = JSONValue(targets);
    auto fixedFeet = ngRigCompileFixedFootCorrections(observation,program);
    assert(fixedFeet["operations"].array.length>0);
    foreach (operation; fixedFeet["operations"].array) {
        foreach (value; ngRigNumbers(operation["values"])) {
            assert(isFinite(value));
            if (ngRigUnsigned(operation["key"][0]) == 2) assert(value == 0);
        }
    }
    auto controls = ngRigCompileControls(observation);
    assert(controls["masks"].array.length == 2 && controls["mechanisms"].array.length == 5);
    foreach (mechanism; controls["mechanisms"].array) foreach (operation; mechanism["operations"].array)
        foreach (key; operation["keys"].array) foreach (offset; ngRigNumbers(key["offsets"])) assert(offset == offset);

    ubyte[] rgba = [0,0,0,255,0,0,0,255,0,0,0,255,0,0,0,255];
    JSONValue[] triangles = [JSONValue([0,1,2]),JSONValue([1,3,2])];
    auto mesh = JSONValue(["vertices":ngRigPointsJson([[2.,3.],[4.,3.],[2.,5.],[4.,5.]]),
        "uv":ngRigPointsJson([[0.,0.],[1.,0.],[0.,1.],[1.,1.]]),"triangles":JSONValue(triangles)]);
    auto reloadedMesh = parseJSON(mesh.toString());
    assert(ngRigSameTextureMapping(mesh,reloadedMesh));
    reloadedMesh["vertices"][0][0] = JSONValue(2.001);
    assert(!ngRigSameTextureMapping(mesh,reloadedMesh));
    auto cloud = ngRigTextureSupport(rgba,2,2,mesh);
    assert(cloud.length == 4 && abs(cloud[0][0]-2.5)<1e-12 && abs(cloud[0][1]-3.5)<1e-12);
    auto projected = ngRigProjectControlOrientation([[0.,0.],[1.,0.],[0.,1.]],
        JSONValue([JSONValue([0,1,2])]),[0.,0.,0.,0.,0.,-2.]);
    assert(1+projected[5]-projected[1]>.00399);
    assert(projected[0] == 0 && projected[2] == 0 && projected[4] == 0);
    ubyte[] separate = new ubyte[5*4];
    separate[3] = separate[7] = separate[19] = 255;
    auto component = ngRigLargestAlphaComponent(separate,5,1);
    assert(component[3] == 255 && component[7] == 255 && component[19] == 0);
    auto alphaMaterial = JSONValue(["texture_size":JSONValue([2,2]),"source_root_mapping":mesh,
        "alpha_runs_128":ngRigAlphaRuns(rgba,128)]);
    assert(ngRigAlphaCoverage(alphaMaterial,[[2.5,3.5],[4.5,3.5]]) == .5);
    ubyte[] padded = new ubyte[6*4*4];
    foreach (y; 1 .. 3) foreach (x; 1 .. 5) padded[(y*6+x)*4+3] = 255;
    auto contourMaterial = JSONValue(["texture_size":JSONValue([6,4]),
        "alpha_runs_32":ngRigAlphaRuns(padded,32),"source_root_mapping":JSONValue([
            "vertices":ngRigPointsJson([[0.,0.],[6.,0.],[0.,4.],[6.,4.]]),
            "uv":mesh["uv"],"triangles":mesh["triangles"]])]);
    auto contour = ngRigAlphaContour(contourMaterial);
    assert(contour["opaque_pixels"].uinteger == 8 && contour["points"].array.length == 8);
    foreach (normal; ngRigPoints(contour["normals"])) assert(abs(normal[0]^^2+normal[1]^^2-1)<1e-9);
    auto localObservation = JSONValue(original.object.dup);
    auto localMaterial = JSONValue(original["materials"][0].object.dup);
    localMaterial["name"] = JSONValue("unknown_ornament"); localMaterial["path"] = JSONValue("/unknown_ornament");
    localObservation["materials"] = JSONValue([localMaterial]);
    auto localClassified = ngRigClassifyMaterials(localObservation,JSONValue(cast(JSONValue[string])null));
    auto localEvidence = ngRigDeriveEvidence(localClassified);
    assert(localEvidence["kind"].str == "local");
    auto localProgram = ngRigCompileProgram(localClassified,localEvidence);
    assert(localProgram["kind"].str == "local" && !("scaffold" in localProgram.object));
    auto transformed = JSONValue(mesh.object.dup);
    transformed["vertices"] = ngRigPointsJson([[6.,8.],[10.,8.],[6.,12.],[10.,12.]]);
    auto registration = ngRigSourceUVRegistration(mesh,transformed);
    auto scale = ngRigNumbers(registration["scale"]), shift = ngRigNumbers(registration["shift"]);
    assert(abs(scale[0]-.5)<1e-10 && abs(scale[1]-.5)<1e-10);
    assert(abs(shift[0]+1)<1e-10 && abs(shift[1]+1)<1e-10);
    assert(ngRigVerifySourceUV(mesh,mesh)<1e-12);
    bool changedFrame;
    try { ngRigVerifySourceUV(mesh,transformed); } catch (Exception) { changedFrame = true; }
    assert(changedFrame);
    assert(ngRigClosestTriangleWeights([2.,.5],[0.,0.],[1.,0.],[0.,1.]) == [0.,1.,0.]);
    assert(ngRigClosestTriangleWeights([.5,-1.],[0.,0.],[1.,0.],[0.,1.]) == [.5,.5,0.]);
    assert(program["native_drivers"].array.length == 42);
    foreach (driver; program["native_drivers"].array) foreach (value; driver["values"].array) {
        auto point = ngRigNumbers(value["key"]);
        if (point == [0.,0.]) assert(ngRigNumber(value["value"]) == 0);
    }

    // Exercise the actual residual compiler with owned source alpha and native bake snapshots.
    auto cheekState = JSONValue(observation.object.dup);
    auto cheekMaterials = observation["materials"].array.dup;
    ubyte[] facePixels = new ubyte[24*20*4];
    foreach (i; 0 .. 24*20) facePixels[i*4+3] = 255;
    auto skin = JSONValue(cheekMaterials[0].object.dup);
    skin["texture_size"] = JSONValue([24,20]); skin["alpha_runs_32"] = ngRigAlphaRuns(facePixels,32);
    skin["source_root_mapping"] = JSONValue(["vertices":ngRigPointsJson([[-12.,0.],[12.,0.],[-12.,20.],[12.,20.]]),
        "uv":mesh["uv"],"triangles":mesh["triangles"]]); cheekMaterials[0] = skin;
    auto temple = JSONValue(cheekMaterials[11].object.dup);
    temple["uuid"] = JSONValue(cast(ulong)1001); temple["feature"] = JSONValue("brow");
    temple["cloud"] = ngRigPointsJson([[-9.,4.],[-3.,4.]]); cheekMaterials ~= temple;
    cheekState["materials"] = JSONValue(cheekMaterials);
    auto cheekTargets = targets.dup;
    foreach (ref target; cheekTargets) {
        target = JSONValue(target.object.dup);
        target["origin"] = JSONValue([0.,0.]); target["grid"] = JSONValue(cast(ulong)2000);
    }
    auto templeTarget = JSONValue(cheekTargets[11].object.dup);
    templeTarget["part"] = temple["uuid"]; cheekTargets ~= templeTarget;
    cheekState["targets"] = JSONValue(cheekTargets); cheekState["controls"] = controls;
    cheekState["source_uv_program_sha256"] = JSONValue("synthetic-uv");
    JSONValue[] headKeys;
    auto faceGrid = ngRigPoints(cheekTargets[0]["points"]);
    foreach (side; [-1.,0.,1.]) {
        double[] offsets; foreach (point; faceGrid) { offsets ~= 0.; offsets ~= 0.; }
        headKeys ~= JSONValue(["grid":JSONValue(cast(ulong)2000),"key":JSONValue([cast(int)(side+1),1]),
            "offsets":JSONValue(offsets),"projection":JSONValue([1.,0.,0.,1.,side*.3,0.,0.,0.])]);
    }
    cheekState["native_head_keys"] = JSONValue(headKeys);
    cheekState["depth_angle_program_sha256"] = JSONValue(ngRigDigest(JSONValue(headKeys)));
    auto cheekReport = ngRigCompileCheekCorrections(cheekState,program);
    assert(ngRigDigest(cheekReport) == ngRigDigest(ngParseAutoRigJson(cheekReport.toString())));
    assert(cheekReport["applicable"].boolean && cheekReport["operations"].array.length == 3);
    foreach (operation; cheekReport["operations"].array) {
        auto allowed = operation["allowed_vertices"].array, values = ngRigNumbers(operation["values"]);
        foreach (value; values) assert(isFinite(value));
        foreach (i,permission; allowed) if (!permission.boolean || operation["near_side"].integer == 0)
            assert(values[i*2] == 0 && values[i*2+1] == 0);
    }
    foreach (cheekObservation; cheekReport["observations"].array)
        assert(isFinite(ngRigNumber(cheekObservation["maximum_local_residual"])));

    string[] stages;
    auto processor = new AnimeFrontViewRigProcessor(null,
        (string stage, JSONValue state, JSONValue plan, ubyte[] model, AutoRigTaskContext context) {
            assert(Fiber.getThis() !is null);
            stages ~= stage;
            if (stage == "observe-model") state = parseJSON(ngTestModelObservation().toString());
            else assert(plan["scaffold"]["bones"].array.length == 21);
            if (stage == "verify-saved-rig") return JSONValue(["passed":JSONValue(true)]);
            context.publishBlob("model",[cast(ubyte)1,2,3]);
            return state;
        });
    auto sessions = new AutoRigSessionManager(buildPath(tempDir(),"autorig-pipeline-" ~ randomUUID().toString()));
    sessions.registerProcessor(processor);
    auto workflows = new AutoRigWorkflowManager(sessions);
    assert(workflows.listPresets().length == 1);
    assert(workflows.listPresets()[0].spec.id == "model-to-rig");
    auto run = workflows.create(processor.procId(),"model-to-rig");
    auto input = run.session().inputContext(run.stepTaskId("source"));
    assert(input.hasValue("options") && input.value("options").json.object.length == 0);
    run.setInput("options",AutoRigValue.jsonValue(JSONValue(["render":JSONValue(true)])));
    assert(run.orderedSteps().length == 15);
    run.execute();
    assert(run.snapshot().state == AutoRigWorkflowState.Succeeded);
    assert(stages == ["observe-model","prepare-source-groups","prepare-shoulders","mesh-parts","register-source-uv",
        "prepare-feature-composites","compile-domain-layout","build-native-rig","weld-shoulders",
        "apply-rig-controls","validate-depth-inputs","bake-depth-angles","apply-shape-corrections","verify-saved-rig"]);
    assert(run.output("report").json["passed"].boolean && run.output("model").readBlob().length>0);
    auto detached = run.session().output(run.stepTaskId("source"),"state").json;
    assert(("materials" in detached.object) is null);
    auto originalReference = detached["artifact_refs"]["materials"].str;
    detached["artifact_refs"]["materials"] = JSONValue("detached-reader-edit");
    assert(run.session().output(run.stepTaskId("source"),"state").json[
        "artifact_refs"]["materials"].str == originalReference);
    auto observationMetadata = run.session().output(run.stepTaskId("source"),"observation").json;
    assert(("cloud" in observationMetadata[0].object) is null);
    auto completedCount = stages.length;
    run.execute();
    assert(stages.length == completedCount);
    workflows.close(run.id());
    auto restored = workflows.reopen(run.id());
    assert(restored.id() == run.id() && restored.snapshot().state == AutoRigWorkflowState.Succeeded);
    assert(restored.session().inputContext(restored.stepTaskId("source")).value("options").json["render"].boolean);
    restored.execute();
    assert(stages.length == completedCount && restored.output("report").json["passed"].boolean);
    auto sourceAttempt = run.session().task(run.stepTaskId("source")).attempt;
    run.session().inputContext(run.stepTaskId("compile")).setValue("options",AutoRigValue.jsonValue(
        JSONValue(["materials":JSONValue(["/iris_r":JSONValue(["static":JSONValue(true)])])])));
    run.executeStep("compile",true);
    assert(run.session().task(run.stepTaskId("source")).attempt == sourceAttempt);
    assert(run.session().outputContext(run.stepTaskId("compile")).value("program").json["carriers"].array.length
        == original["materials"].array.length-1);

    string[] attempts;
    bool failControls = true;
    auto failingProcessor = new AnimeFrontViewRigProcessor(null,
        (string stage, JSONValue state, JSONValue plan, ubyte[] model, AutoRigTaskContext context) {
            attempts ~= stage;
            if (stage == "observe-model") state = ngTestModelObservation();
            if (stage == "apply-rig-controls" && failControls) throw new Exception("synthetic local-control failure");
            if (stage == "verify-saved-rig") return JSONValue(["passed":JSONValue(true)]);
            context.publishBlob("model",[cast(ubyte)1,2,3]);
            return state;
        });
    auto diagnosticSessions = new AutoRigSessionManager(buildPath(tempDir(),"autorig-finish-" ~ randomUUID().toString()));
    diagnosticSessions.registerProcessor(failingProcessor);
    auto diagnosticWorkflows = new AutoRigWorkflowManager(diagnosticSessions);
    auto diagnosticRun = diagnosticWorkflows.create(failingProcessor.procId(),"model-to-rig");
    diagnosticRun.execute();
    assert(diagnosticRun.snapshot().state == AutoRigWorkflowState.Failed);
    assert(diagnosticRun.session().task(diagnosticRun.stepTaskId("controls")).state == AutoRigTaskState.Failed);
    assert(attempts[$-4 .. $] == ["validate-depth-inputs","bake-depth-angles","apply-shape-corrections","verify-saved-rig"]);
    auto completion = diagnosticRun.output("report").json;
    assert(!completion["all_finish_stages_succeeded"].boolean && !completion["rig_complete"].boolean);
    assert(completion["finish_stages"].array[0]["error"].str == "synthetic local-control failure");
    failControls = false;
    diagnosticRun.execute();
    assert(diagnosticRun.snapshot().state == AutoRigWorkflowState.Succeeded);
    assert(diagnosticRun.session().task(diagnosticRun.stepTaskId("controls")).attempt == 2);
    assert(diagnosticRun.output("report").json["all_finish_stages_succeeded"].boolean);
}
