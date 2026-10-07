module nijigenerate.autorig.deterministic.anatomy;

import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.deterministic.linear;
import std.algorithm : sort, clamp;
import std.array : array;
import std.exception : enforce;
import std.json : JSONValue;
import std.math : sqrt, abs, ceil;
import std.string : startsWith;

private double length(Point2 p) { return sqrt(p[0]*p[0] + p[1]*p[1]); }
private Point2 subtract(Point2 a, Point2 b) { return [a[0]-b[0], a[1]-b[1]]; }
private double dot(Point2 a, Point2 b) { return a[0]*b[0]+a[1]*b[1]; }

/** Anatomical equalities and weighted observations determine one connected scaffold. */
JSONValue ngRigSolveScaffold(JSONValue evidence, JSONValue prior) {
    bool[string] seen;
    foreach (bone; prior["bone_graph"].array) {
        seen[bone[2].str] = true;
        seen[bone[3].str] = true;
    }
    auto roles = seen.keys.sort.array;
    size_t[string] index;
    Point2[] source;
    double[] weights;
    auto transform = evidence["source_to_model"].array;
    enforce(transform.length == 3, "Expected a 3x3 source transform");
    auto t0 = ngRigNumbers(transform[0]), t1 = ngRigNumbers(transform[1]), t2 = ngRigNumbers(transform[2]);
    enforce(t0.length == 3 && t1.length == 3 && t2 == [0.,0.,1.], "Invalid source transform");
    double determinant = t0[0]*t1[1]-t0[1]*t1[0];
    enforce(determinant > 0 && abs(t0[0]*t0[0]+t1[0]*t1[0]-determinant) < 1e-6 * determinant &&
        abs(t0[1]*t0[1]+t1[1]*t1[1]-determinant) < 1e-6 * determinant &&
        abs(t0[0]*t0[1]+t1[0]*t1[1]) < 1e-6 * determinant, "Source transform is not a similarity");
    foreach (i, role; roles) {
        index[role] = i;
        auto entry = evidence["landmarks"][role];
        auto provenance = entry["provenance"].str;
        enforce(provenance == "measured" || provenance == "prior" || provenance == "visual_estimate",
            "Invalid landmark provenance");
        auto p = ngRigPoint(entry["xy"]);
        Point2 modelPoint = [t0[0]*p[0]+t0[1]*p[1]+t0[2], t1[0]*p[0]+t1[1]*p[1]+t1[2]];
        source ~= modelPoint;
        double weight = ngRigScalar(entry, "weight", 1);
        enforce(weight > 0, "Invalid landmark weight");
        weights ~= weight;
    }
    auto down = subtract(source[index["head_root"]], source[index["head_top"]]);
    enforce(length(down) > 0, "Degenerate head axis");
    if (dot(subtract(source[index["neck_base"]], source[index["head_root"]]), down) <= 0)
        foreach (axis; 0 .. 2) source[index["neck_base"]][axis] =
            (source[index["shoulder.L"]][axis]+source[index["shoulder.R"]][axis])/2;
    auto policy = prior["torso_axis"];
    auto start = policy["start"].str, end = policy["end"].str;
    auto origin = source[index[start]];
    Point2 tip = [0.,0.];
    auto hips = policy["end_attachment"].array;
    foreach (hip; hips) foreach (axis; 0 .. 2) tip[axis] += source[index[hip.str]][axis]/hips.length;
    auto direction = subtract(tip, origin);
    double length2 = dot(direction, direction);
    enforce(length2 > 0, "Degenerate torso axis");
    double[string] fractions;
    double previous = 0;
    foreach (station; policy["stations"].array) {
        double fraction = dot(subtract(source[index[station.str]], origin), direction)/length2;
        enforce(fraction-previous >= ngRigNumber(policy["minimum_station_gap"]), "Invalid torso station order");
        fractions[station.str] = fraction;
        previous = fraction;
    }
    enforce(1-previous >= ngRigNumber(policy["minimum_station_gap"]), "Invalid torso end station");
    string[] independent;
    foreach (role; roles) if (role != end && (role in fractions) is null) independent ~= role;
    auto basis = ngRigZeroMatrix(roles.length, independent.length);
    foreach (column, role; independent) basis[index[role]][column] = 1;
    foreach (hip; hips) foreach (column; 0 .. independent.length)
        basis[index[end]][column] += basis[index[hip.str]][column]/hips.length;
    foreach (role, fraction; fractions) foreach (column; 0 .. independent.length)
        basis[index[role]][column] = (1-fraction)*basis[index[start]][column]+fraction*basis[index[end]][column];
    double[][] rows, targets;
    foreach (i, role; roles) if ((role in fractions) is null) {
        double weight = sqrt(weights[i]);
        auto row = basis[i].dup;
        row[] *= weight;
        rows ~= row;
        targets ~= [source[i][0]*weight, source[i][1]*weight];
    }
    foreach (constraint; prior["joint_constraints"].array) {
        auto row = new double[independent.length]; row[] = 0;
        double weight = sqrt(ngRigNumber(constraint["weight"]));
        foreach (role, coefficient; constraint["terms"].object) foreach (column; 0 .. independent.length)
            row[column] += ngRigNumber(coefficient)*weight*basis[index[role]][column];
        rows ~= row;
        targets ~= [0.,0.];
    }
    auto solved = ngRigLeastSquares(rows, targets);
    Point2[] fitted;
    JSONValue[string] points, residuals;
    foreach (i, role; roles) {
        Point2 p = [0.,0.];
        foreach (column; 0 .. independent.length) foreach (axis; 0 .. 2)
            p[axis] += basis[i][column]*solved[column][axis];
        fitted ~= p;
        points[role] = JSONValue(p[]);
        residuals[role] = JSONValue(length(subtract(p, source[i])));
    }
    Point2 feet;
    foreach (axis; 0 .. 2) feet[axis] = (fitted[index["foot_tip.L"]][axis]+fitted[index["foot_tip.R"]][axis])/2;
    double height = length(subtract(fitted[index["head_top"]], feet));
    enforce(height > 0, "Invalid body height");
    foreach (role, residual; residuals)
        enforce(ngRigNumber(residual)/height <= ngRigNumber(prior["acceptance"]["joint_fit_max_relative_residual"]),
            "Landmark residual exceeds tolerance: " ~ role);
    JSONValue[] bones;
    foreach (definition; prior["bone_graph"].array) {
        auto a = fitted[index[definition[2].str]], b = fitted[index[definition[3].str]];
        enforce(length(subtract(a,b)) >= height*1e-5, "Zero-length anatomical bone");
        bones ~= JSONValue(["id":definition[0], "parent":definition[1], "head":JSONValue([a[0],a[1],0.]),
            "tail":JSONValue([b[0],b[1],0.]), "lock_to_root":JSONValue(definition[0].str.startsWith("Foot.")),
            "rest_roll":JSONValue(0.)]);
    }
    JSONValue[string] volumes;
    foreach (name, spec; evidence["volumes"].object) {
        auto center = ngRigPoint(spec["center"]), radii = ngRigPoint(spec["radii"]);
        center = [t0[0]*center[0]+t0[1]*center[1]+t0[2], t1[0]*center[0]+t1[1]*center[1]+t1[2]];
        radii[] *= sqrt(determinant);
        auto axis = name == "head" ? subtract(fitted[index["head_root"]],fitted[index["head_top"]]) :
            subtract(fitted[index[end]],fitted[index[start]]);
        double axisLength = length(axis); axis[] /= axisLength;
        if (name == "torso") {
            foreach (i; 0 .. 2) center[i] = (fitted[index[start]][i]+fitted[index[end]][i])/2;
            radii[1] = axisLength/2;
        }
        JSONValue[] frame = [JSONValue([axis[1],axis[0]]), JSONValue([-axis[0],axis[1]])];
        volumes[name] = JSONValue(["center":JSONValue(center[]), "radii":JSONValue(radii[]), "frame":JSONValue(frame)]);
    }
    return JSONValue(["schema_version":JSONValue("rig-scaffold-d/1"), "landmarks":JSONValue(points),
        "bones":JSONValue(bones), "volumes":JSONValue(volumes), "body_height":JSONValue(height),
        "residual_model_units":JSONValue(residuals), "evidence_sha256":JSONValue(ngRigDigest(evidence))]);
}

double[] ngRigDepthField(Point2[] query, JSONValue domain, JSONValue scaffold, JSONValue prior) {
    auto owner = domain["owner"].str, kind = domain["kind"].str;
    auto landmarks = scaffold["landmarks"];
    double[] result;
    double distance(Point2 p, Point2 a, Point2 b) {
        auto axis = subtract(b,a); double length2 = dot(axis,axis);
        enforce(length2 > 0, "Degenerate depth axis");
        double t = clamp(dot(subtract(p,a),axis)/length2,0.,1.);
        return length([p[0]-a[0]-t*axis[0],p[1]-a[1]-t*axis[1]]);
    }
    foreach (p; query) {
        double z = 0;
        if (owner == "head" || owner == "torso") {
            auto volume = scaffold["volumes"][owner];
            auto center = ngRigPoint(volume["center"]), radii = ngRigPoint(volume["radii"]);
            enforce(radii[0]>0 && radii[1]>0, "Nonpositive volume radius");
            auto f0 = ngRigPoint(volume["frame"][0]), f1 = ngRigPoint(volume["frame"][1]);
            auto q = subtract(p,center);
            double u = (q[0]*f0[0]+q[1]*f1[0])/radii[0];
            double v = (q[0]*f0[1]+q[1]*f1[1])/radii[1];
            z = ngRigNumber(prior["depth_ratios"][owner])*radii[0]*
                (owner == "head" ? clamp(1-u*u-v*v,0.,1.) : sqrt(clamp(1-u*u,0.,1.)));
        } else {
            string[] roles = owner.startsWith("arm:") ? ["shoulder","elbow","wrist"] : ["hip","knee","ankle"];
            auto side = domain["side"].str;
            double d = double.infinity;
            foreach (i; 0 .. 2) {
                double local = distance(p,ngRigPoint(landmarks[roles[i] ~ "." ~ side]),
                    ngRigPoint(landmarks[roles[i+1] ~ "." ~ side]));
                if (local < d) d = local;
            }
            double radius = ngRigNumber(domain["radius"]);
            enforce(radius > 0, "Nonpositive limb radius");
            z = ngRigNumber(prior["depth_ratios"]["limb"])*radius*sqrt(clamp(1-d*d/(radius*radius),0.,1.));
        }
        if (kind == "neck") {
            double width = length(subtract(ngRigPoint(landmarks["shoulder.L"]),ngRigPoint(landmarks["shoulder.R"])))*.14;
            enforce(width > 0, "Degenerate neck width");
            double d = distance(p,ngRigPoint(landmarks["head_root"]),ngRigPoint(landmarks["neck_base"]));
            z = width*sqrt(clamp(1-d*d/(width*width),0.,1.));
        } else if (kind == "skirt_front" || kind == "skirt_back" || kind == "free_cloth") {
            auto b = ngRigNumbers(domain["support_bounds"]);
            double t = clamp((p[1]-b[1])/(b[3]-b[1]>1 ? b[3]-b[1] : 1),0.,1.);
            double radius = (b[2]-b[0])/2*(.45+.55*t), divisor = radius>1 ? radius : 1;
            double q = (p[0]-(b[0]+b[2])/2)/divisor;
            z = ngRigNumber(prior["depth_ratios"]["skirt"])*radius*sqrt(clamp(1-q*q,0.,1.));
            if (kind == "skirt_back") z = -z;
            z += ngRigScalar(domain,"offset",0);
        } else if (kind == "back_hair") z = -z-ngRigScalar(domain,"offset",0);
        else if (kind == "appendage") z = ngRigScalar(domain,"offset",0);
        else z += ngRigScalar(domain,"offset",0);
        result ~= z;
    }
    return result;
}
