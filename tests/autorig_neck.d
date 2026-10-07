module autorig_neck;

import nijigenerate.autorig.deterministic.contracts : ngRigPoints, ngRigPoint;
import nijigenerate.autorig.deterministic.neck : ngRigInferNeckBase;
import nijigenerate.autorig.json : ngParseAutoRigJson;
import std.json : JSONValue;
import std.stdio : writeln;
import std.file : readText;
import std.math : abs;

void main(string[] args) {
    assert(args.length > 1, "Pass real PSD-derived cloud JSON paths");
    foreach (path; args[1 .. $]) {
        auto source = ngParseAutoRigJson(readText(path));
        auto face = ngRigPoints(source["face"]), neck = ngRigPoints(source["neck"]);
        auto torso = ngRigPoints(source["torso"]), garments = ngRigPoints(source["garments"]);
        auto separate = ngRigInferNeckBase(face, neck, torso, garments);
        auto merged = ngRigInferNeckBase(face, null, neck ~ torso, garments);
        auto a = ngRigPoint(separate["xy"]), b = ngRigPoint(merged["xy"]);
        assert(abs(a[0] - b[0]) < 1e-9 && abs(a[1] - b[1]) < 1e-9,
            "Part grouping changed the inferred neck location");
        writeln(JSONValue(["source":JSONValue(path), "inference":separate]).toString());
    }
}
