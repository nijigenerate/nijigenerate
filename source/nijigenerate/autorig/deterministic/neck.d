module nijigenerate.autorig.deterministic.neck;

import nijigenerate.autorig.deterministic.contracts : Point2, Point3;
import std.algorithm : min, max, sort;
import std.array : array;
import std.exception : enforce;
import std.json : JSONValue;
import std.math : abs, sqrt;

private double quantile(double[] values, double fraction) {
    enforce(values.length > 0, "Empty neck support");
    auto sorted = values.dup.sort.array;
    double index = fraction * (sorted.length - 1);
    auto first = cast(size_t)index;
    auto last = min(first + 1, sorted.length - 1);
    return sorted[first] * (1 - (index - first)) + sorted[last] * (index - first);
}

private Point2 section(Point2[] points, double fraction) {
    double[] ys;
    foreach (p; points) ys ~= p[1];
    double low = quantile(ys, .01), high = quantile(ys, .99);
    double y = low + fraction * (high - low);
    double[] xs;
    foreach (p; points) if (abs(p[1] - y) <= max(1., (high - low) * .025)) xs ~= p[0];
    if (!xs.length) {
        size_t[] order;
        foreach (i; 0 .. points.length) order ~= i;
        order.sort!((a, b) => abs(points[a][1] - y) < abs(points[b][1] - y));
        foreach (i; order[0 .. max(1, points.length / 100)]) xs ~= points[i][0];
    }
    return [quantile(xs, .5), y];
}

private Point2 project(Point2 point, Point2 origin, Point2 down, Point2 tangent) {
    Point2 delta = [point[0] - origin[0], point[1] - origin[1]];
    return [delta[0] * down[0] + delta[1] * down[1], delta[0] * tangent[0] + delta[1] * tangent[1]];
}

private Point3[] profile(Point2[] cloud, Point2 origin, Point2 down, Point2 tangent,
    double height, double width) {
    Point2[] projected;
    foreach (point; cloud) projected ~= project(point, origin, down, tangent);
    Point3[] rows;
    double center = 0;
    foreach (i; 0 .. 129) {
        double station = height * i / 128;
        double[] cross;
        foreach (p; projected)
            if (abs(p[0] - station) <= height * .012 && abs(p[1]) <= width * .9) cross ~= p[1];
        if (cross.length < 3) continue;
        cross.sort();
        bool found;
        double bestDistance = double.infinity, bestLeft, bestRight, nextCenter;
        size_t start;
        foreach (end; 1 .. cross.length + 1) {
            if (end < cross.length && cross[end] - cross[end - 1] <= width * .06) continue;
            if (end - start >= 3) {
                auto piece = cross[start .. end];
                double left = quantile(piece, .05), right = quantile(piece, .95), middle = (left + right) / 2;
                double distance = abs(middle - center);
                if (right - left >= width * .08 && abs(middle) <= width * .6 &&
                    (!found || distance < bestDistance || distance == bestDistance && left < bestLeft)) {
                    found = true; bestDistance = distance; bestLeft = left; bestRight = right; nextCenter = middle;
                }
            }
            start = end;
        }
        if (found) {
            center = nextCenter;
            Point3 row = [station, bestRight - bestLeft, center];
            rows ~= row;
        }
    }
    return rows;
}

/** Python riglib.neck parity: observe narrow skin and its transition into the torso. */
JSONValue ngRigInferNeckBase(Point2[] face, Point2[] neck, Point2[] torso, Point2[] garments = null) {
    enforce(face.length > 0, "Neck inference needs head support");
    auto origin = section(face, 1), top = section(face, 0);
    Point2 down = [origin[0] - top[0], origin[1] - top[1]];
    double height = sqrt(down[0] * down[0] + down[1] * down[1]);
    enforce(height > 0, "Degenerate head frame");
    down[] /= height;
    Point2 tangent = [down[1], -down[0]];
    double[] faceU;
    foreach (p; face) faceU ~= project(p, origin, down, tangent)[1];
    double width = quantile(faceU, .99) - quantile(faceU, .01);
    enforce(width > 0, "Degenerate head width");
    auto body = torso.length ? torso : garments;
    auto support = neck ~ body;
    enforce(support.length > 0, "Neck inference needs neck or body support");
    auto frame = JSONValue(["origin":JSONValue(origin[]), "tangent":JSONValue(tangent[]),
        "normal":JSONValue([-down[0], -down[1]])]);
    JSONValue report(Point2 point, string method, string structure, double narrow,
        Point3[] rows, JSONValue score, string provenance) {
        JSONValue[] samples;
        foreach (row; rows) samples ~= JSONValue(row[]);
        return JSONValue(["xy":JSONValue(point[]), "method":JSONValue(method),
            "structure":JSONValue(structure), "body_support":JSONValue(torso.length ? "skin" : "garment"),
            "frame":frame, "head_height":JSONValue(height), "head_width":JSONValue(width),
            "narrow_width":JSONValue(narrow), "width_profile":JSONValue(samples),
            "fit_mean_squared_error":score, "provenance":JSONValue(provenance)]);
    }
    Point2 position(double station, double transverse) {
        return [origin[0] + station * down[0] + transverse * tangent[0],
            origin[1] + station * down[1] + transverse * tangent[1]];
    }
    auto narrowSupport = neck.length ? neck : torso;
    auto neckRows = profile(narrowSupport, origin, down, tangent, height, width);
    if (neckRows.length >= 4) {
        double[] widths;
        foreach (row; neckRows) widths ~= row[1];
        double narrow = quantile(widths, .2), broad = quantile(widths, .85);
        if (narrow < width * .75 && broad < narrow * 1.8 && neckRows[$ - 1][0] < height * .7) {
            double[] ts;
            foreach (p; narrowSupport) ts ~= project(p, origin, down, tangent)[0];
            double station = quantile(ts, .99);
            double[] cross;
            foreach (p; narrowSupport) {
                auto uv = project(p, origin, down, tangent);
                if (abs(uv[0] - station) <= height * .025) cross ~= uv[1];
            }
            return report(position(station, quantile(cross, .5)), "narrow_neck_inferior_attachment",
                neck.length ? "separate_narrow_neck_material" : "narrow_neck_in_body_material",
                narrow, neckRows, JSONValue(null), "measured");
        }
    }
    auto rows = profile(support, origin, down, tangent, height, width);
    enforce(rows.length > 0, "No central neck/body cross sections");
    double[] proximal;
    foreach (row; rows) if (row[0] <= height * .45) proximal ~= row[1];
    double narrow = proximal.length ? quantile(proximal, .2) : 0;
    size_t first = size_t.max, crossing = size_t.max;
    foreach (i, row; rows) if (row[1] <= narrow * 1.25) { first = i; break; }
    if (narrow > 0 && narrow < width * .75 && first != size_t.max) {
        foreach (i; first + 3 .. rows.length >= 2 ? rows.length - 2 : 0)
            if (rows[i][1] >= narrow * 1.5 && rows[i + 1][1] >= narrow * 1.5 && rows[i + 2][1] >= narrow * 1.5) {
                crossing = i; break;
            }
    }
    if (crossing != size_t.max) {
        size_t end = rows.length - 1;
        foreach (i; crossing .. rows.length) if (rows[i][1] >= narrow * 2.2) { end = i; break; }
        auto sample = rows[first .. min(rows.length, end + 4)];
        double bestScore = double.infinity, station;
        size_t bestCandidate;
        bool fitted;
        foreach (candidate; first + 2 .. crossing + 1) {
            double knot = rows[candidate][0], sx = 0, sy = 0, sxx = 0, sxy = 0;
            foreach (row; sample) {
                double x = max(0., row[0] - knot), y = row[1];
                sx += x; sy += y; sxx += x * x; sxy += x * y;
            }
            double n = sample.length, determinant = n * sxx - sx * sx;
            if (determinant <= 0) continue;
            double slope = (n * sxy - sx * sy) / determinant, intercept = (sy - slope * sx) / n;
            if (slope <= 0 || intercept < narrow * .65 || intercept > narrow * 1.5) continue;
            double residual = 0;
            foreach (row; sample) {
                double error = row[1] - intercept - slope * max(0., row[0] - knot);
                residual += error * error;
            }
            residual /= n;
            if (residual < bestScore) {
                fitted = true; bestScore = residual; station = knot; bestCandidate = candidate;
            }
        }
        if (fitted) {
            double[] centers;
            auto start = max(first, bestCandidate >= 2 ? bestCandidate - 2 : 0);
            foreach (row; rows[start .. min(rows.length, bestCandidate + 3)]) centers ~= row[2];
            return report(position(station, quantile(centers, .5)), "narrow_neck_to_broad_torso_change",
                neck.length ? "neck_material_includes_body" : "neck_in_body_material",
                narrow, rows, JSONValue(bestScore), "measured");
        }
    }
    double[] upper;
    foreach (p; body) {
        auto uv = project(p, origin, down, tangent);
        if (uv[0] > 0 && abs(uv[1]) < width * .45) upper ~= uv[0];
    }
    enforce(upper.length > 0, "Unresolved neck/body attachment");
    double station = min(quantile(upper, .02), height * .55);
    return report(position(station, 0), "occluded_neck_upper_body_support",
        neck.length ? "neck_material_includes_body" : "neck_in_body_material",
        narrow, rows, JSONValue(null), "visual_estimate");
}
