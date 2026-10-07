module nijigenerate.autorig.deterministic.processor;

import nijigenerate.autorig.framework;
import nijigenerate.autorig.deterministic.face;
import nijigenerate.autorig.deterministic.anatomy;
import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.deterministic.templates;
import nijigenerate.autorig.deterministic.surface;
import nijigenerate.autorig.deterministic.evidence;
import nijigenerate.autorig.deterministic.program;
import nijigenerate.autorig.deterministic.pipeline;
import std.exception : enforce;
import std.json : JSONValue, JSONType;
import std.math : isFinite;
import std.file : copy;

private double number(JSONValue value) {
    enforce(value.type == JSONType.integer || value.type == JSONType.uinteger || value.type == JSONType.float_,
        "Expected a numeric value");
    double result = value.type == JSONType.float_ ? value.floating :
        value.type == JSONType.integer ? cast(double)value.integer : cast(double)value.uinteger;
    enforce(isFinite(result), "Expected a finite number");
    return result;
}

private double[] numbers(JSONValue value) {
    double[] result;
    foreach (item; value.array) result ~= number(item);
    return result;
}

private RigPoint[] points(JSONValue value) {
    RigPoint[] result;
    foreach (item; value.array) {
        auto coordinates = numbers(item);
        enforce(coordinates.length == 2, "Expected a two-dimensional point");
        RigPoint point = [coordinates[0], coordinates[1]];
        result ~= point;
    }
    return result;
}

private JSONValue pointsJson(RigPoint[] value) {
    JSONValue[] result;
    foreach (point; value) result ~= JSONValue([point[0], point[1]]);
    return JSONValue(result);
}

/** Deterministic rig computation tasks; each port is an inspectable artifact. */
class AnimeFrontViewRigProcessor : AutoRigProcessor {
    private JSONValue delegate(JSONValue, JSONValue, AutoRigTaskContext) applyProjection;
    private RigNativeStage nativeStage;

    this(JSONValue delegate(JSONValue, JSONValue, AutoRigTaskContext) applyProjection = null,
        RigNativeStage nativeStage = null) {
        this.applyProjection = applyProjection;
        this.nativeStage = nativeStage;
    }
    override string procId() { return "anime-front-view-rig"; }
    override string displayName() { return ngAutoRigMessage("Anime Front View Rig"); }

    override AutoRigTaskSpec[] tasks() {
        AutoRigTaskSpec[] result;
        foreach (id, label; ["preserve-face-orientation": ngAutoRigMessage("Preserve face orientation"),
            "fit-grid-depth": ngAutoRigMessage("Fit grid depth"), "solve-scaffold": ngAutoRigMessage("Solve anatomical scaffold"),
            "sample-anatomical-depth": ngAutoRigMessage("Sample anatomical depth"), "evaluate-template": ngAutoRigMessage("Evaluate surface template")]) {
            result ~= AutoRigTaskSpec(id, label, null,
                [AutoRigPortSpec("request", AutoRigValueKind.Json)],
                [AutoRigPortSpec("result", AutoRigValueKind.Json)], null);
        }
        if (applyProjection !is null)
            result ~= AutoRigTaskSpec("apply-face-projection", ngAutoRigMessage("Apply face projection"), null,
                [AutoRigPortSpec("projection", AutoRigValueKind.Json), AutoRigPortSpec("target", AutoRigValueKind.Json)],
                [AutoRigPortSpec("result", AutoRigValueKind.Json)], null);
        if (nativeStage !is null) result ~= ngRigPipelineTasks();
        return result;
    }

    override AutoRigWorkflowSpec[] workflows() {
        // Computation components are tasks, not separate user-facing workflows.
        return nativeStage is null ? null : [ngRigModelWorkflow(procId())];
    }

    override void executeTask(string taskId, AutoRigTaskContext context) {
        if (taskId == "observe-model") {
            enforce(nativeStage !is null,"Model observation is unavailable");
            auto state = nativeStage(taskId,context.input("options").json,JSONValue.init,"",context);
            JSONValue[] materials;
            foreach (material; state["materials"].array)
                materials ~= JSONValue(["name":material["name"],"path":material["path"],"active":material["active"]]);
            context.publishJson("materials",JSONValue(materials));
            context.publishJson("state",state);
            return;
        }
        if (taskId == "compile-rig") {
            auto state = context.input("state").json;
            auto observation = ngRigClassifyMaterials(state,context.input("options").json,context);
            ngRigCheckpoint(context);
            auto evidence = ngRigDeriveEvidence(observation);
            auto program = ngRigCompileProgram(observation,evidence);
            state = observation; state["evidence"] = evidence;
            context.publishJson("program",program); context.publishJson("state",state);
            return;
        }
        foreach (entry; ngRigPipelineTasks()) if (taskId == entry.id) {
            enforce(nativeStage !is null,"Native rig application is unavailable");
            auto state = context.input("state").json;
            JSONValue result;
            try {
                result = nativeStage(taskId,state,context.input("program").json,context.input("model").text,context);
                if (taskId == "compile-domain-layout") context.publishJson("program",ngRigCompileProgram(result,result["evidence"]));
                if (entry.retainFailureOutputs && taskId != "verify-saved-rig") {
                    auto attempts = ngRigGet(state,"finish_stages",JSONValue(cast(JSONValue[])null)).array.dup;
                    attempts ~= JSONValue(["stage":JSONValue(taskId),"succeeded":JSONValue(true)]);
                    result["finish_stages"] = JSONValue(attempts);
                }
            } catch (Exception error) {
                if (!entry.retainFailureOutputs || context.isCanceled()) throw error;
                auto attempts = ngRigGet(state,"finish_stages",JSONValue(cast(JSONValue[])null)).array.dup;
                attempts ~= JSONValue(["stage":JSONValue(taskId),"succeeded":JSONValue(false),"error":JSONValue(error.msg)]);
                if (taskId == "verify-saved-rig") result = JSONValue(["passed":JSONValue(false),
                    "finish_stages":JSONValue(attempts),"visual_review_required":JSONValue(true),
                    "error":JSONValue(error.msg),"source_sha256":state["source_sha256"]]);
                else {
                    // The native stage rolls back before throwing. Preserve that owned checkpoint.
                    result = state; result["finish_stages"] = JSONValue(attempts);
                    auto destination = context.outputPath("model",".inx");
                    copy(context.input("model").text,destination); context.publishPath("model",destination);
                }
                context.reportFailure(error.msg);
            }
            if (taskId == "verify-saved-rig") {
                auto attempts = ngRigGet(result,"finish_stages",
                    ngRigGet(state,"finish_stages",JSONValue(cast(JSONValue[])null))).array.dup;
                bool verificationRecorded;
                auto verified = ngRigGet(result,"readback_verified",result["passed"]).boolean;
                foreach (attempt; attempts) if (attempt["stage"].str == taskId) verificationRecorded = true;
                if (!verificationRecorded) attempts ~= JSONValue(["stage":JSONValue(taskId),
                    "succeeded":JSONValue(verified)]);
                result["finish_stages"] = JSONValue(attempts);
                bool complete = verified;
                foreach (attempt; attempts) complete = complete && attempt["succeeded"].boolean;
                result["all_finish_stages_succeeded"] = JSONValue(complete);
                result["numerical_stages_passed"] = JSONValue(complete && result["passed"].boolean);
                result["rig_complete"] = JSONValue(false);
                if (!complete) context.reportFailure("One or more finishing stages failed; inspect the completion report");
            }
            context.publishJson(taskId == "verify-saved-rig" ? "report" : "state",result);
            return;
        }
        if (taskId == "apply-face-projection") {
            enforce(applyProjection !is null, "Editor application is unavailable");
            context.publishJson("result", applyProjection(context.input("projection").json,
                context.input("target").json, context));
            return;
        }
        auto request = context.input("request").json;
        if (taskId == "evaluate-template") {
            ngRigCheckpoint(context);
            auto result = ngRigEvaluateTemplate(request);
            ngRigCheckpoint(context);
            context.publishJson("result",result);
            return;
        }
        if (taskId == "solve-scaffold" || taskId == "sample-anatomical-depth") {
            ngRigCheckpoint(context);
            auto prior = ngRigHumanoidPrior();
            auto result = taskId == "solve-scaffold" ? ngRigSolveScaffold(request["evidence"], prior) :
                JSONValue(ngRigDepthField(ngRigPoints(request["query"]), request["domain"],
                    request["scaffold"], prior));
            ngRigCheckpoint(context);
            context.publishJson("result", result);
            return;
        }
        auto xs = numbers(request["xs"]), ys = numbers(request["ys"]);
        bool canceled() { return context.isCanceled(); }
        JSONValue[string] output;
        if (taskId == "preserve-face-orientation") {
            auto direction = numbers(request["depthDirection"]);
            enforce(direction.length == 2, "Expected a two-dimensional depth direction");
            bool[] protectedVertices;
            if (auto protection = "protected" in request.object)
                foreach (item; protection.array) protectedVertices ~= item.boolean;
            auto projection = ngPreserveFaceOrientation(points(request["points"]), xs, ys,
                points(request["uv"]), [direction[0], direction[1]], number(request["width"]),
                protectedVertices, &canceled);
            output["points"] = pointsJson(projection.points);
            output["rawMinimumJacobian"] = JSONValue(projection.rawMinimum);
            output["correctedMinimumJacobian"] = JSONValue(projection.correctedMinimum);
            output["maximumCorrection"] = JSONValue(projection.maximumCorrection);
        } else if (taskId == "fit-grid-depth") {
            auto count = number(request["anchorCount"]);
            enforce(count >= 0 && count <= size_t.max && count == cast(size_t)count, "Invalid anchor count");
            auto fitted = ngFitGridDepth(xs, ys, numbers(request["base"]), points(request["query"]),
                numbers(request["truth"]), number(request["tolerance"]), cast(size_t)count, &canceled);
            output["passed"] = JSONValue(fitted.length != 0);
            output["depth"] = JSONValue(fitted);
        } else throw new Exception("Unknown deterministic rig task");
        enforce(!context.isCanceled(), "Deterministic rig task canceled");
        context.publishJson("result", JSONValue(output));
    }
}
