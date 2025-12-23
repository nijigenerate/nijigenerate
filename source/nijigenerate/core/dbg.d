module nijigenerate.core.dbg;

public import nijilive.core.dbg;
import nijilive.core.dbg;
import nijilive.math : Vec3Array;
import nijilive.math : vec3;

/// Convenience wrapper that transparently forwards to nijilive once SoA ready.
void inDbgSetBuffer(T)(auto ref T points)
if (is(T == Vec3Array) || is(T == vec3[]))
{
    import nijilive.core.dbg : liveInDbgSetBuffer = inDbgSetBuffer;
    static if (is(T == Vec3Array)) {
        liveInDbgSetBuffer(points);
    } else {
        auto tmp = Vec3Array(points);
        liveInDbgSetBuffer(tmp);
    }
}
