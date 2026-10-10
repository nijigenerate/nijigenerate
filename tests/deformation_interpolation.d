module deformation_interpolation;

import core.memory : GC;
import nijilive.core.param : Parameter;
import nijilive.core.param.binding : DeformationParameterBinding;
import nijilive.core.nodes.defstack : Deformation;
import nijilive.math : vec2, vec2u;
import inmath.interpolate : lerp;
import std.stdio : writeln;
import std.datetime.stopwatch : StopWatch;

private void benchmarkLinear(DeformationParameterBinding binding, bool direct) {
    enum iterations = 2000;
    foreach (trial; 0 .. 5) {
        GC.collect();
        auto allocatedBefore = GC.stats().allocatedInCurrentThread;
        StopWatch timer;
        timer.start();
        float checksum = 0.0f;
        foreach (i; 0 .. iterations) {
            auto offset = vec2((i % 101) / 100.0f, (i % 79) / 78.0f);
            auto value = direct ? binding.interpolateLinear(vec2u(1, 1), offset) :
                legacyLinear(binding, vec2u(1, 1), offset);
            checksum += value.vertexOffsets[i % 512].toVector().x;
        }
        timer.stop();
        writeln(direct ? "direct" : "legacy", " trial=", trial,
            " total_us=", timer.peek.total!"usecs",
            " allocated=", GC.stats().allocatedInCurrentThread - allocatedBefore,
            " checksum=", checksum);
    }
}

// Reference the previous whole-deformation algorithm, including its arithmetic order.
private Deformation legacyLinear(DeformationParameterBinding binding, vec2u left, vec2 offset) {
    Deformation p0, p1;
    if (binding.parameter.isVec2) {
        auto p00 = binding.values[left.x][left.y];
        auto p01 = binding.values[left.x][left.y + 1];
        auto p10 = binding.values[left.x + 1][left.y];
        auto p11 = binding.values[left.x + 1][left.y + 1];
        p0 = lerp!Deformation(p00, p01, offset.y);
        p1 = lerp!Deformation(p10, p11, offset.y);
    } else {
        p0 = binding.values[left.x][0];
        p1 = binding.values[left.x + 1][0];
    }
    return lerp!Deformation(p0, p1, offset.x);
}

void main() {
    auto parameter = new Parameter();
    auto binding = new DeformationParameterBinding(parameter);
    binding.values = new Deformation[][](4, 3);
    foreach (x; 0 .. 4) foreach (y; 0 .. 3) {
        binding.values[x][y].vertexOffsets.length = 512;
        foreach (i; 0 .. 512) binding.values[x][y].vertexOffsets[i] =
            vec2(cast(float)(x * 19.31 - y * 5.71 + i * 0.019),
                cast(float)(x * -8.93 + y * 11.71 - i * 0.023));
    }
    auto original = binding.values[0][0];
    auto originalPoint = original.vertexOffsets[0].toVector();
    Deformation assigned;
    assigned = original;
    assigned.vertexOffsets[0] = vec2(123);
    assert(original.vertexOffsets[0].toVector() == originalPoint);
    auto supplied = binding.values[1][0].vertexOffsets.dup;
    assigned.update(supplied);
    auto suppliedPoint = supplied[0].toVector();
    assigned.vertexOffsets[0] = vec2(456);
    assert(supplied[0].toVector() == suppliedPoint);
    foreach (twoDimensions; [false, true]) {
        parameter.isVec2 = twoDimensions;
        foreach (x; 0 .. 3) foreach (y; 0 .. 2) {
            foreach (xt; [-0.2f, 0.0f, 0.17f, 0.5f, 0.93f, 1.0f, 1.3f])
                foreach (yt; [0.0f, 0.23f, 0.5f, 1.0f]) {
                    auto expected = legacyLinear(binding, vec2u(x, y), vec2(xt, yt));
                    auto actual = binding.interpolateLinear(vec2u(x, y), vec2(xt, yt));
                    foreach (i; 0 .. 512) {
                        import std.format : format;
                        assert(actual.vertexOffsets[i].toVector() == expected.vertexOffsets[i].toVector(),
                            format("2D=%s left=%s offset=%s vertex=%s actual=%s expected=%s",
                                twoDimensions, vec2u(x,y), vec2(xt,yt), i,
                                actual.vertexOffsets[i].toVector(), expected.vertexOffsets[i].toVector()));
                    }
                    auto source = binding.values[x][0].vertexOffsets[0].toVector();
                    auto retained = actual;
                    actual.vertexOffsets[0] = vec2(777);
                    assert(retained.vertexOffsets[0].toVector() == expected.vertexOffsets[0].toVector());
                    assert(binding.values[x][0].vertexOffsets[0].toVector() == source);
                }
        }
    }
    parameter.isVec2 = true;
    auto before = GC.stats().allocatedInCurrentThread;
    auto expected = legacyLinear(binding, vec2u(1, 1), vec2(0.23, 0.71));
    auto legacyBytes = GC.stats().allocatedInCurrentThread - before;
    before = GC.stats().allocatedInCurrentThread;
    auto actual = binding.interpolateLinear(vec2u(1, 1), vec2(0.23, 0.71));
    auto directBytes = GC.stats().allocatedInCurrentThread - before;
    assert(directBytes < legacyBytes / 2);
    assert(actual.vertexOffsets[3].toVector() == expected.vertexOffsets[3].toVector());
    writeln("Exact interpolation and ownership passed; allocations ", legacyBytes, " -> ", directBytes);
    benchmarkLinear(binding, false);
    benchmarkLinear(binding, true);
}
