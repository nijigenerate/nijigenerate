module nijigenerate.autorig.deterministic.editor;

import nijigenerate.autorig.framework : AutoRigTaskContext;
import nijigenerate.commands.base : Context, ngRunCommand;
import nijigenerate.commands.model.set_deform_binding : SetDeformBindingCommand;
import nijigenerate.project : incActivePuppet;
import nijilive.math : vec2u;
import nijilive.core.nodes : Node;
import nijilive.core.nodes.deformable : Deformable;
import std.exception : enforce;
import std.json : JSONValue, JSONType;
import std.math : isFinite, abs;

private uint identifier(JSONValue value) {
    enforce(value.type == JSONType.uinteger || value.type == JSONType.integer && value.integer >= 0,
        "Expected an unsigned model identifier");
    auto number = value.type == JSONType.uinteger ? value.uinteger : cast(ulong)value.integer;
    enforce(number <= uint.max, "Model identifier out of range");
    return cast(uint)number;
}

private double coordinate(JSONValue value) {
    enforce(value.type == JSONType.float_ || value.type == JSONType.integer || value.type == JSONType.uinteger,
        "Expected a deformation coordinate");
    double result = value.type == JSONType.float_ ? value.floating :
        value.type == JSONType.integer ? cast(double)value.integer : cast(double)value.uinteger;
    enforce(isFinite(result), "Nonfinite deformation coordinate");
    return result;
}

/** Apply one explicitly addressed key through the ordinary undoable editor command. */
JSONValue ngApplyFaceProjection(JSONValue projection, JSONValue target, AutoRigTaskContext task) {
    auto posed = projection["points"].array;
    auto rest = target["restPoints"].array;
    enforce(posed.length == rest.length && posed.length > 0, "Projection and rest point counts differ");
    float[] offsets;
    foreach (i; 0 .. posed.length) {
        enforce(posed[i].array.length == 2 && rest[i].array.length == 2, "Expected 2D model-local points");
        foreach (axis; 0 .. 2) {
            double offset = coordinate(posed[i][axis]) - coordinate(rest[i][axis]);
            enforce(isFinite(offset) && offset >= -float.max && offset <= float.max, "Offset exceeds model range");
            offsets ~= cast(float)offset;
        }
    }
    auto rootId = identifier(target["rootId"]), nodeId = identifier(target["nodeId"]);
    auto parameterId = identifier(target["parameterId"]);
    auto key = target["keyPoint"].array;
    enforce(key.length == 2, "Expected a two-dimensional key point");
    auto keyPoint = vec2u(identifier(key[0]), identifier(key[1]));
    enforce(!task.isCanceled(), "Face application canceled");
    task.runOnMainThread({
        enforce(!task.isCanceled(), "Face application canceled");
        auto puppet = incActivePuppet();
        enforce(puppet !is null && puppet.root.uuid == rootId, "Active puppet differs from task target");
        auto node = puppet.find!Node(nodeId);
        auto parameter = puppet.findParameter(parameterId);
        enforce(node !is null && parameter !is null, "Face target or parameter no longer exists");
        auto deformable = cast(Deformable)node;
        enforce(deformable !is null && deformable.vertices.length == rest.length,
            "Face target mesh changed since observation");
        foreach (i; 0 .. rest.length) {
            auto vertex = deformable.vertices[i];
            double x = coordinate(rest[i][0]), y = coordinate(rest[i][1]);
            enforce(abs(vertex.x - x) <= 1e-4 * (1 + abs(x)) &&
                abs(vertex.y - y) <= 1e-4 * (1 + abs(y)), "Face rest geometry changed since observation");
        }
        auto context = new Context();
        context.puppet = puppet;
        context.nodes = [node];
        context.parameters = [parameter];
        context.keyPoint = keyPoint;
        context.hasExplicitKeyPoint = true;
        auto result = ngRunCommand(new SetDeformBindingCommand("deform", offsets), context);
        enforce(result.succeeded, result.message);
    });
    return JSONValue(["applied": JSONValue(true), "nodeId": JSONValue(nodeId),
        "parameterId": JSONValue(parameterId), "vertices": JSONValue(posed.length)]);
}
