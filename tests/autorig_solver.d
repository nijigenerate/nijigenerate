module autorig_solver;

import nijigenerate.autorig.solver.quadratic;
import nijigenerate.autorig.deterministic.face;
import nijigenerate.autorig.deterministic.processor;
import nijigenerate.autorig.deterministic.templates;
import nijigenerate.autorig.deterministic.geometry;
import nijigenerate.autorig.deterministic.linear;
import nijigenerate.autorig.deterministic.anatomy;
import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.deterministic.surface;
import std.math : isFinite;
import nijigenerate.autorig.json : ngParseAutoRigJson;
import nijigenerate.autorig.framework;
import nijigenerate.autorig.workflow;
import std.json : JSONValue;
import std.file : tempDir, write;
import std.path : buildPath;
import std.uuid : randomUUID;
import std.math : abs;
import std.stdio : writeln;
import autorig_pipeline : ngTestRigPipeline;
import nijigenerate.autorig.deterministic.evidence : ngRigClassifyMaterials, ngRigDeriveEvidence;
import nijigenerate.autorig.deterministic.program : ngRigCompileProgram;
import std.file : readText;
import std.json : parseJSON;
import std.datetime.stopwatch : StopWatch;

void main(string[] args) {
    if (args.length == 5 && args[1] == "--audit-owned-state") {
        import nijigenerate.autorig.deterministic.observation : ngRigPrepareShoulders;
        import nijigenerate.autorig.deterministic.controls : ngRigCompileControls, ngRigCompileCheekCorrections;
        auto state = ngParseAutoRigJson(readText(args[2]));
        auto program = ngParseAutoRigJson(readText(args[3]));
        auto controls = ngRigCompileControls(state);
        auto corrections = ngRigCompileCheekCorrections(state,program);
        auto shoulders = ngRigPrepareShoulders(state,program);
        write(args[4],JSONValue(["controls":controls,"corrections":corrections,"shoulders":shoulders]).toString());
        writeln("Replayed original D compilers on owned imported-model state");
        return;
    }
    if (args.length == 4 && args[1] == "--compile-observation") {
        auto source = ngParseAutoRigJson(readText(args[2]));
        auto observation = ngRigClassifyMaterials(source,source["options"]);
        auto evidence = ngRigDeriveEvidence(observation);
        auto program = ngRigCompileProgram(observation,evidence);
        write(args[3],JSONValue(["evidence":evidence,"program":program]).toString());
        return;
    }
    if (args.length == 3 && args[1] == "--classify-observation") {
        auto source = parseJSON(readText(args[2]));
        auto timer = StopWatch(); timer.start();
        auto result = ngRigClassifyMaterials(source,JSONValue(cast(JSONValue[string])null));
        size_t active, automatic;
        foreach (material; result["materials"].array) if (!material["static"].boolean) {
            ++active;
            assert(material["role"].str.length>0 && "owner" in material.object);
            if (material["semantic_source"].str == "alpha_proximal_support_candidate") ++automatic;
        }
        writeln("Classified ",active," active materials; alpha support fallback: ",automatic,
            "; elapsed ms: ",timer.peek.total!"msecs");
        return;
    }
    ngTestRigPipeline();
    import std.math : sin;
    foreach (i; 0 .. 1000) {
        auto value = JSONValue(sin(i*.123)*10000);
        assert(ngParseAutoRigJson(value.toString()).floating == value.floating);
    }
    auto exactJson = JSONValue(["escaped\"key":JSONValue([JSONValue(-3147.01493669930278),
        JSONValue(-0.0),JSONValue(true),JSONValue("1.25")]),"empty":JSONValue(cast(JSONValue[])null)]);
    assert(ngRigDigest(ngParseAutoRigJson(exactJson.toString())) == ngRigDigest(exactJson));
    ngRigValidateEmbeddedTemplates();
    assert(ngRigTemplateNames().length == 19);
    auto embedded = ngRigTemplate("face_head");
    embedded["id"] = JSONValue("changed");
    assert(ngRigTemplate("face_head")["id"].str == "face_head");
    foreach (id; ngRigTemplateNames()) {
        JSONValue[string] request;
        request["template"] = JSONValue(id);
        request["bounds"] = JSONValue([0.,0.,100.,100.]);
        request["sourceFit"] = JSONValue(false);
        if (id == "surface_layer") {
            auto definition = ngRigTemplate(id);
            auto host = new double[definition["guide_grid"]["rows"].array.length *
                definition["guide_grid"]["columns"].array.length];
            host[] = 0; request["hostDepth"] = JSONValue(host);
        }
        auto surface = ngRigEvaluateTemplate(JSONValue(request));
        assert(surface["neutral"] == surface["projected"]);
        foreach (z; surface["depth"].array) assert(isFinite(ngRigNumber(z)));
        request["pose"] = JSONValue([12.,8.,7.]);
        surface = ngRigEvaluateTemplate(JSONValue(request));
        assert(surface["posed_validation"]["local_orientation_preserved"].boolean);
    }
    auto solved = ngRigLeastSquares([[1.,0.],[0.,1.],[1.,1.]], [[2.],[3.],[5.]]);
    assert(abs(solved[0][0]-2) < 1e-12 && abs(solved[1][0]-3) < 1e-12);
    Point2[] guideUV = [[0.,0.],[1.,0.],[0.,1.],[1.,1.]];
    Point2[] guideXY = [[2.,3.],[4.,3.],[2.,6.],[4.,6.]];
    auto guide = ngRigFitGuide(guideUV,[0.,0.,1.,1.],guideUV,guideXY);
    foreach (i, p; guide) foreach (axis; 0 .. 2) assert(abs(p[axis]-guideXY[i][axis]) < 1e-12);
    assert(ngRigValidateGrid(guide,2)["local_orientation_preserved"].boolean);
    auto midpoint = ngRigSampleGrid(guide,[0.,1.],[0.,1.],[[.5,.5]])[0];
    assert(abs(midpoint[0]-3)<1e-12 && abs(midpoint[1]-4.5)<1e-12);
    auto rotated = ngRigRotate([[1.,0.,0.]],[0.,0.,0.],[0.,0.,90.])[0];
    assert(abs(rotated[0])<1e-12 && abs(rotated[1]-1)<1e-12);
    QuadraticProblem p;
    p.variables = 1;
    p.constraints = 1;
    p.objective = [SparseEntry(0, 0, 2)];
    p.linear = [-4.];
    p.matrix = [SparseEntry(0, 0, 1)];
    p.lower = [0.];
    p.upper = [1.];
    auto r = ngSolveQuadratic(p);
    assert(r.solved() && abs(r.solution[0] - 1) < 1e-5 && r.constraintViolation < 1e-5);
    p.objective = null;
    p.linear = [-1.];
    r = ngSolveQuadratic(p);
    assert(r.solved() && abs(r.solution[0] - 1) < 1e-5);
    p.constraints = 2;
    p.matrix = [SparseEntry(0, 0, 1), SparseEntry(1, 0, 1)];
    p.lower = [0., 2.];
    p.upper = [1., 3.];
    r = ngSolveQuadratic(p);
    assert(!r.solved() && r.solution.length == 0);

    double[] xs = [0., 1.], ys = [0., 1.];
    RigPoint[] uv = [[0.,0.], [1.,0.], [0.,1.], [1.,1.]];
    auto projection = ngPreserveFaceOrientation(uv, xs, ys, uv, [1., 0.], 1);
    assert(projection.maximumCorrection == 0 && projection.rawMinimum == 1);
    RigPoint[] thin = [[0.,0.], [.04,0.], [0.,1.], [.04,1.]];
    projection = ngPreserveFaceOrientation(thin, xs, ys, uv, [1.,0.], 1);
    assert(projection.correctedMinimum >= .0549 && projection.maximumCorrection <= .04001);
    auto fitted = ngFitGridDepth(xs, ys, [1.,1.,1.,1.], uv,
        [1.01,1.01,1.01,1.01], .02, 4);
    assert(fitted.length == 4);
    foreach (value; fitted) assert(abs(value - 1.01) < 1e-5);
    bool canceled() { return true; }
    bool rejected;
    try { ngSolveQuadratic(p, SolverOptions.init, &canceled); }
    catch (Exception) { rejected = true; }
    assert(rejected);
    bool applied;
    auto processor = new AnimeFrontViewRigProcessor(
        (JSONValue projection, JSONValue target, AutoRigTaskContext context) {
            assert(projection["correctedMinimumJacobian"].floating >= .0549);
            assert(target["nodeId"].integer == 123);
            applied = true;
            return JSONValue(["applied": JSONValue(true)]);
        });
    auto sessions = new AutoRigSessionManager(buildPath(tempDir(), "autorig-solver-" ~ randomUUID().toString()));
    sessions.registerProcessor(processor);
    assert(processor.workflows().length == 0);
    auto run = sessions.create(processor.procId());
    JSONValue[] thinJson, uvJson;
    foreach (point; thin) thinJson ~= JSONValue(point[]);
    foreach (point; uv) uvJson ~= JSONValue(point[]);
    run.setInput("preserve-face-orientation", "request", AutoRigValue.jsonValue(JSONValue([
        "xs": JSONValue(xs), "ys": JSONValue(ys), "points": JSONValue(thinJson),
        "uv": JSONValue(uvJson), "depthDirection": JSONValue([1.,0.]), "width": JSONValue(1.)])));
    run.execute("preserve-face-orientation");
    run.setInput("apply-face-projection", "projection", run.outputContext("preserve-face-orientation").value("result"));
    run.setInput("apply-face-projection", "target", AutoRigValue.jsonValue(JSONValue(["nodeId": JSONValue(123)])));
    run.execute("apply-face-projection");
    assert(applied && run.outputContext("apply-face-projection").value("result").json["applied"].boolean);
    assert(run.task("apply-face-projection").state == AutoRigTaskState.Succeeded);
    import std.file : exists;
    assert(!exists(run.directory()));
    sessions.close(run.id());
    auto restored = sessions.reopenWithProcessor(processor,run.id());
    restored.restoreCommittedOutputs();
    assert(restored.task("apply-face-projection").state == AutoRigTaskState.Succeeded);
    assert(restored.outputContext("apply-face-projection").value("result").json["applied"].boolean);
    assert(!exists(run.directory()));
    auto restartedSessions = new AutoRigSessionManager(sessions.rootDirectory());
    bool rejectedRestart;
    try { restartedSessions.reopenWithProcessor(processor,run.id()); }
    catch (Exception error) { rejectedRestart = true; }
    assert(rejectedRestart);
    writeln("AutoRig OSQP and face projection checks passed");
}
