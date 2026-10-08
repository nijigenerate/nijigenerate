module nijigenerate.autorig.alpha_support_test;

import nijigenerate.autorig.deterministic.evidence : AlphaSupportIndex;
import nijigenerate.autorig.deterministic.contracts : Point2, ngRigNumber;
import nijigenerate.autorig.json : ngParseAutoRigJson;
import std.file : readText;
import std.algorithm : min;
import std.math : sqrt;
import std.datetime.stopwatch : StopWatch, AutoStart;
import std.stdio : writefln;

void main(string[] args) {
    assert(args.length == 2, "Pass a real PSD-derived cloud JSON path");
    auto source = ngParseAutoRigJson(readText(args[1]));
    Point2[][] clouds;
    Point2[] queries;
    foreach (name, cloud; source.object) {
        Point2[] points;
        foreach (point; cloud.array)
            points ~= cast(Point2)[ngRigNumber(point[0]), ngRigNumber(point[1])];
        if (!points.length) continue;
        clouds ~= points;
        // Use observed artwork points, including points far outside other parts.
        foreach (i; 0 .. min(8, points.length))
            queries ~= points[i * (points.length - 1) / min(7, points.length - 1 ? points.length - 1 : 1)];
    }
    auto timer = StopWatch(AutoStart.yes);
    double[][] distances;
    foreach (cloud; clouds) {
        auto index = new AlphaSupportIndex(cloud);
        double[] row;
        foreach (query; queries) row ~= index.distance(query);
        distances ~= row;
    }
    auto indexedMs = timer.peek.total!"msecs";
    size_t checked;
    foreach (i, cloud; clouds) foreach (j, query; queries) {
        double best = double.infinity;
        foreach (point; cloud) {
            auto dx = point[0] - query[0];
            auto dy = point[1] - query[1];
            best = min(best, dx * dx + dy * dy);
        }
        assert(distances[i][j] == sqrt(best), "Nearest support differs from exhaustive search");
        ++checked;
    }
    writefln("Real PSD supports=%s queries=%s exact checks=%s indexed_ms=%s total_ms=%s",
        clouds.length, queries.length, checked, indexedMs, timer.peek.total!"msecs");
}
