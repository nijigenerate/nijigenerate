module nijigenerate_tests.numeric_serialization;
import nijilive.fmt.serialize;
import nijilive.core.param;
import nijilive.core.nodes.defstack;
import nijilive.math;
import std.math : isNaN, isInfinity;
import std.stdio : writeln;
import std.algorithm.searching : canFind;
import std.array : appender;

void ngTestNumericSerialization() {
    import nijilive.math.serialization : inRecoverDeformationOffsets;
    auto mesh = Vec2Array([vec2(0, 0), vec2(10, 0), vec2(0, 10), vec2(10, 10), vec2(5, 5)]);
    auto field = Vec2Array([vec2(1, 2), vec2(21, 12), vec2(31, -8), vec2(51, 2), vec2(0, 99)]);
    assert(inRecoverDeformationOffsets(mesh, field, [0, 0, 0, 0, 1]) == 1);
    assert(field[4] == vec2(26, 99));
    assert(field[0] == vec2(1, 2) && field[3] == vec2(51, 2));
    field[4] = vec2(0, 0);
    assert(inRecoverDeformationOffsets(mesh, field, [0, 0, 0, 0, 3]) == 2);
    assert(field[4] == vec2(26, 2));
    assert(inToJson(float.nan) == `"nan"`);
    assert(inToJson(float.infinity) == `"+inf"`);
    assert(inToJson(-float.infinity) == `"-inf"`);
    assert(inToJson([float.nan, 1.25f]) == `["nan",1.25]`);
    assert(inToJson(`"nan"`) == `"\"nan\""`);
    assert(deserialize!float(inToJson(float.nan)).isNaN);
    vec2 optional = vec2(1, 2);
    assert(optional.deserialize(Fghj.init) is null && optional == vec2(1, 2));
    assert(optional.deserialize(parseJson("null")) is null && optional == vec2(1, 2));
    foreach (token; [`"nan"`, `"\"nan\""`, `null`]) {
        float value;
        assert(inDeserializeNumber(parseJson(token), value) is null && value.isNaN);
    }
    float infinity;
    assert(inDeserializeNumber(parseJson(`"\"-inf\""`), infinity) is null);
    assert(infinity.isInfinity && infinity < 0);
    auto parameter = new Parameter("RoundTrip", false);
    parameter.axisPoints = [[0f], [0f]];
    auto binding = new DeformationParameterBinding(parameter);
    auto data = parseJson(`{"node":123,"param_name":"deform","values":[[[["\"nan\"",2],[3,"inf"]]]],"isSet":[[true]]}`);
    assert(binding.deserializeFromFghj(data) is null);
    auto offsets = binding.getValue(vec2u(0)).vertexOffsets;
    assert(offsets.length == 2 && offsets[0] == vec2(0, 2) && offsets[1] == vec2(3, 0));
    assert(inLastLoadDiagnostics.recoveredComponents == 2);
    assert(inLastLoadDiagnostics.warnings[0].canFind("RoundTrip"));
    offsets[0] = vec2(float.nan, 2);
    binding.values[0][0].vertexOffsets = offsets;
    bool rejected;
    try {
        auto app = appender!(char[]);
        auto serializer = inCreateSerializer(app);
        binding.serializeSelf(serializer);
    } catch (Exception error) {
        rejected = error.msg.canFind("vertex 0") && error.msg.canFind("RoundTrip");
    }
    assert(rejected);
    bool contextual;
    try {
        binding.deserializeFromFghj(parseJson(`{"node":123,"param_name":"deform",` ~
            `"values":[[[["broken",2]]]],"isSet":[[true]]}`));
    } catch (Exception error) {
        contextual = error.msg.canFind("RoundTrip") && error.msg.canFind("[0,0]");
    }
    assert(contextual);
    contextual = false;
    try {
        binding.deserializeFromFghj(parseJson(`{"node":123,"param_name":"deform",` ~
            `"values":[[[[1]]]],"isSet":[[true]]}`));
    } catch (Exception error) {
        contextual = error.msg.canFind("vertex 0") && error.msg.canFind("RoundTrip");
    }
    assert(contextual);
    writeln("PASS: numeric serialization, nested arrays, legacy tokens, deformation recovery, contextual save/load errors");
}
unittest { ngTestNumericSerialization(); }
