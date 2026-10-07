module nijigenerate.autorig.deterministic.surface;

import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.deterministic.geometry;
import nijigenerate.autorig.deterministic.depth;
import nijigenerate.autorig.deterministic.templates;
import std.json : JSONValue, JSONType;
import std.exception : enforce;
import std.math : abs;

/** Fit and evaluate a compiled-in front chart with explicit source evidence. */
JSONValue ngRigEvaluateTemplate(JSONValue request) {
    auto definition = ngRigTemplate(request["template"].str);
    auto bounds = ngRigNumbers(request["bounds"]);
    enforce(bounds.length == 4, "Expected four source bounds");
    double width = bounds[2]-bounds[0];
    enforce(width>0 && bounds[3]>bounds[1], "Empty surface bounds");
    auto supplied = ngRigGet(request,"parameters",JSONValue(cast(JSONValue[string])null));
    JSONValue[string] parameters;
    foreach (tunable; definition["tunables"].array) {
        auto id = tunable["id"].str;
        auto candidate = ngRigGet(supplied,id,tunable["default"]);
        double number = ngRigNumber(candidate);
        enforce(number>=ngRigNumber(tunable["min"]) && number<=ngRigNumber(tunable["max"]),
            "Template parameter exceeds declared range: " ~ id);
        parameters[id] = JSONValue(number);
    }
    foreach (id, candidate; supplied.object) enforce((id in parameters) !is null, "Unknown template parameter: " ~ id);
    double[] xs, ys;
    foreach (column; definition["guide_grid"]["columns"].array) xs ~= ngRigNumber(column["u"]);
    foreach (row; definition["guide_grid"]["rows"].array) ys ~= ngRigNumber(row["v"]);
    auto uv = ngRigUVGrid(xs,ys);
    auto evidence = ngRigGet(request,"landmarks",JSONValue(cast(JSONValue[string])null));
    bool sourceFit = ngRigGet(request,"sourceFit",JSONValue(true)).boolean;
    Point2[] source, target;
    bool[string] known;
    foreach (landmark; definition["landmarks"].array) {
        auto id = landmark["id"].str; known[id] = true;
        auto suppliedPoint = id in evidence.object;
        enforce(!sourceFit || !landmark["required"].boolean || suppliedPoint !is null,
            "Missing required source landmark: " ~ id);
        if (suppliedPoint !is null) { source ~= ngRigPoint(landmark["uv"]); target ~= ngRigPoint(*suppliedPoint); }
    }
    foreach (id, point; evidence.object) enforce((id in known) !is null, "Unknown source landmark: " ~ id);
    double[4] box = [bounds[0],bounds[1],bounds[2],bounds[3]];
    auto guide = ngRigFitGuide(uv,box,source,target);
    auto validation = ngRigValidateGrid(guide,xs.length);
    double[] host;
    if (auto suppliedHost = "hostDepth" in request.object) host = ngRigNumbers(*suppliedHost);
    auto depth = ngRigEvaluateDepth(uv,definition["geometry"]["operators"],JSONValue(parameters),host);
    Point3[] xyz;
    foreach (i, p; guide) {
        enforce(abs(depth[i])<=ngRigNumber(definition["validation"]["max_depth_width"]), "Template depth exceeds bound");
        Point3 q = [p[0],p[1],depth[i]*width]; xyz ~= q;
    }
    Point3 pose = [0.,0.,0.], pivot = [(bounds[0]+bounds[2])/2,(bounds[1]+bounds[3])/2,0.];
    if (auto suppliedPose = "pose" in request.object) {
        auto values = ngRigNumbers(*suppliedPose); enforce(values.length == 3, "Expected yaw/pitch/roll"); pose[] = values[];
    }
    if (auto suppliedPivot = "pivot" in request.object) {
        auto values = ngRigNumbers(*suppliedPivot); enforce(values.length == 3, "Expected a 3D pivot"); pivot[] = values[];
    }
    auto rotated = ngRigRotate(xyz,pivot,pose);
    Point2[] projected;
    foreach (p; rotated) { Point2 q = [p[0],p[1]]; projected ~= q; }
    projected = ngRigLocalCorrections(projected,uv,pose,[0.,0.,0.],definition["correction_rules"],JSONValue(parameters),width);
    auto posedValidation = ngRigValidateGrid(projected,xs.length);
    return JSONValue(["schema_version":JSONValue("rig-template-surface-d/1"), "template":definition["id"],
        "version":definition["version"], "source_fit":JSONValue(sourceFit), "xs":JSONValue(xs), "ys":JSONValue(ys),
        "uv":ngRigPointsJson(uv), "neutral":ngRigPointsJson(guide), "depth":JSONValue(depth),
        "projected":ngRigPointsJson(projected), "neutral_validation":validation, "posed_validation":posedValidation,
        "request_sha256":JSONValue(ngRigDigest(request))]);
}
