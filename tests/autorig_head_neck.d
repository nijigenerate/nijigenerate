module autorig_head_neck_test;

import nijigenerate.autorig.deterministic.contracts : ngRigPoints, ngRigPoint;
import nijigenerate.autorig.deterministic.neck : ngRigInferHeadFrame, ngRigInferNeckBase;
import nijigenerate.autorig.deterministic.anatomy : ngRigSolveScaffold;
import nijigenerate.autorig.deterministic.templates : ngRigHumanoidPrior;
import nijigenerate.autorig.json : ngParseAutoRigJson;
import std.file : readText;
import std.math : abs;
import std.json : JSONValue;
import std.stdio : writeln;

void main(string[] args) {
    assert(args.length > 1, "Pass real PSD-derived head/neck JSON paths");
    foreach (path; args[1 .. $]) {
        auto input = ngParseAutoRigJson(readText(path));
        auto face = ngRigPoints(input["face"]), neck = ngRigPoints(input["neck"]);
        auto torso = ngRigPoints(input["torso"]), garments = ngRigPoints(input["garments"]);
        auto frame = ngRigInferHeadFrame(face,ngRigPoint(input["eye_right"]),ngRigPoint(input["eye_left"]));
        foreach (key; ["origin","tangent","normal","head_top","head_root"]) {
            auto actual = ngRigPoint(frame[key]), expected = ngRigPoint(input["frame"][key]);
            foreach (axis; 0 .. 2) assert(abs(actual[axis]-expected[axis]) < 1e-9);
        }
        auto result = ngRigInferNeckBase(face,neck,torso,garments,frame);
        auto actual = ngRigPoint(result["xy"]), expected = ngRigPoint(input["inference"]["xy"]);
        foreach (axis; 0 .. 2) assert(abs(actual[axis]-expected[axis]) < 1e-8);
        auto root = ngRigPoint(frame["head_root"]), tangent = ngRigPoint(frame["tangent"]);
        auto normal = ngRigPoint(frame["normal"]);
        assert(abs((actual[0]-root[0])*tangent[0]+(actual[1]-root[1])*tangent[1]) < 1e-9);
        assert((actual[0]-root[0])*normal[0]+(actual[1]-root[1])*normal[1] < 0);
        if (auto evidence = "evidence" in input.object) {
            auto scaffold = ngRigSolveScaffold(*evidence,ngRigHumanoidPrior());
            foreach (role, expectedPoint; input["scaffold"]["landmarks"].object) {
                auto point = ngRigPoint(scaffold["landmarks"][role]), reference = ngRigPoint(expectedPoint);
                foreach (axis; 0 .. 2) assert(abs(point[axis]-reference[axis]) < 1e-7);
            }
        }
        writeln(path, ": Python/D head frame, neck station and shared axis match");
    }
}
