module nijigenerate.autorig.deterministic.depth;

import nijigenerate.autorig.deterministic.contracts;
import std.json : JSONValue, JSONType;
import std.exception : enforce;
import std.algorithm : max;
import std.math : sqrt, pow, cos, sin, PI, abs, isFinite;

private double scalar(JSONValue value, JSONValue parameters) {
    return ngRigNumber(value.type == JSONType.string ? parameters[value.str] : value);
}

Point2[] ngRigLocalCorrections(Point2[] positions, Point2[] material, Point3 pose,
    Point3 neutral, JSONValue rules, JSONValue parameters, double scale) {
    enforce(positions.length == material.length && isFinite(scale) && scale > 0, "Invalid correction dimensions");
    auto result = positions.dup;
    double basis(Point3 angles, JSONValue driver) {
        auto axis = ngRigString(driver,"axis","yaw"), kind = ngRigString(driver,"basis","sin");
        enforce(axis == "yaw" || axis == "pitch" || axis == "roll", "Unknown correction axis");
        double x = angles[axis == "yaw" ? 0 : axis == "pitch" ? 1 : 2]*PI/180*value(driver,"scale",parameters,1);
        if (kind == "linear") return x;
        if (kind == "sin") return sin(x);
        if (kind == "sin2") return sin(x)*sin(x);
        enforce(kind == "positive_sin", "Unknown correction basis");
        return max(0.,sin(x));
    }
    foreach (rule; rules.array) {
        enforce(ngRigString(rule,"type","local") == "local", "Unknown correction operator");
        known(rule,["type","center","radius","rotation_degrees","direction","amplitude","driver","pin_edges"]);
        auto direction = pair(rule,"direction",parameters,[1.,0.]);
        double norm = sqrt(direction[0]*direction[0]+direction[1]*direction[1]);
        enforce(norm > 0, "Degenerate correction direction"); direction[] /= norm;
        auto driver = ngRigGet(rule,"driver",JSONValue(cast(JSONValue[string])null));
        known(driver,["axis","basis","scale"]);
        double response = basis(pose,driver)-basis(neutral,driver);
        double amplitude = scalar(rule["amplitude"],parameters)*scale*response;
        string[] edges;
        foreach (entry; ngRigGet(rule,"pin_edges",JSONValue(cast(JSONValue[])null)).array) {
            auto edge = entry.str;
            enforce(edge == "u0" || edge == "u1" || edge == "v0" || edge == "v1", "Unknown pinned edge");
            foreach (previous; edges) enforce(previous != edge, "Duplicate pinned edge");
            edges ~= edge;
        }
        foreach (i, uv; material) {
            enforce(uv[0]>=0 && uv[0]<=1 && uv[1]>=0 && uv[1]<=1, "Correction outside material UV");
            double weight = ngRigCompactSupport(uv,rule,parameters);
            foreach (edge; edges) {
                double coordinate = uv[edge[0] == 'v'];
                if (edge[1] == '1') coordinate = 1-coordinate;
                weight *= coordinate*coordinate;
            }
            foreach (axis; 0 .. 2) {
                result[i][axis] += weight*amplitude*direction[axis];
                enforce(isFinite(result[i][axis]), "Nonfinite corrected point");
            }
        }
    }
    return result;
}

private double value(JSONValue op, string key, JSONValue parameters, double fallback) {
    return scalar(ngRigGet(op,key,JSONValue(fallback)),parameters);
}

private Point2 pair(JSONValue record, string key, JSONValue parameters, Point2 fallback) {
    auto entries = ngRigGet(record,key,JSONValue(fallback[])).array;
    enforce(entries.length == 2, "Expected a depth operator pair");
    return [scalar(entries[0],parameters),scalar(entries[1],parameters)];
}

private void known(JSONValue op, string[] allowed) {
    foreach (key, entry; op.object) {
        bool found = key == "id" || key == "label" || key == "description";
        foreach (candidate; allowed) if (key == candidate) found = true;
        enforce(found, "Unknown depth operator field: " ~ key);
    }
}

private double profile(double coordinate, JSONValue op, string key, JSONValue parameters, double fallback) {
    auto points = ngRigGet(op,key);
    if (points.type == JSONType.null_) return fallback;
    enforce(points.array.length > 0, "Empty depth profile");
    Point2[] parsed;
    foreach (entry; points.array) {
        enforce(entry.array.length == 2, "Invalid depth profile pair");
        Point2 p = [scalar(entry[0],parameters),scalar(entry[1],parameters)];
        enforce(p[0]>=0 && p[0]<=1 && (!parsed.length || p[0]>parsed[$-1][0]), "Unordered depth profile");
        parsed ~= p;
    }
    if (coordinate <= parsed[0][0]) return parsed[0][1];
    foreach (i; 1 .. parsed.length) if (coordinate <= parsed[i][0]) {
        double t = (coordinate-parsed[i-1][0])/(parsed[i][0]-parsed[i-1][0]);
        return parsed[i-1][1]*(1-t)+parsed[i][1]*t;
    }
    return parsed[$-1][1];
}

private double section(Point2 p, JSONValue op, JSONValue parameters, bool swept, bool flat = false) {
    double center = value(op,"center_u",parameters,.5), radius = value(op,"radius_u",parameters,.5);
    if (swept) {
        center = profile(p[1],op,"center_u_profile",parameters,center);
        radius = profile(p[1],op,"radius_u_profile",parameters,radius);
    }
    enforce(radius>0, "Nonpositive depth section radius");
    auto cross = flat ? "flat" : ngRigString(op,"cross_section","ellipse");
    double factor = 1;
    if (cross == "ellipse") {
        double exponent = value(op,"exponent",parameters,.5);
        enforce(exponent>0, "Nonpositive depth section exponent");
        double q = (p[0]-center)/radius;
        factor = pow(max(0.,1-q*q),exponent);
    } else if (cross == "angular") {
        auto theta = pair(op,"theta_degrees",parameters,[-90.,90.]);
        enforce(theta[0]!=theta[1], "Degenerate angular depth interval");
        factor = cos((theta[0]+p[0]*(theta[1]-theta[0]))*PI/180);
    } else enforce(cross == "flat", "Unknown depth cross section");
    return value(op,"base",parameters,0)+
        (swept ? profile(p[1],op,"center_depth_profile",parameters,0) : 0)+
        value(op,"depth",parameters,1)*profile(p[1],op,"depth_profile",parameters,1)*factor;
}

double ngRigCompactSupport(Point2 p, JSONValue op, JSONValue parameters) {
    auto center = pair(op,"center",parameters,[.5,.5]), radius = pair(op,"radius",parameters,[.5,.5]);
    enforce(radius[0]>0 && radius[1]>0, "Nonpositive compact support radius");
    double angle = value(op,"rotation_degrees",parameters,0)*PI/180;
    double dx = p[0]-center[0], dy = p[1]-center[1];
    double x = (dx*cos(angle)+dy*sin(angle))/radius[0], y = (-dx*sin(angle)+dy*cos(angle))/radius[1];
    double distance = sqrt(x*x+y*y), remaining = max(0.,1-distance);
    return pow(remaining,4)*(4*distance+1);
}

/** Evaluate all declarative template depth operators without expression evaluation. */
double[] ngRigEvaluateDepth(Point2[] query, JSONValue operators, JSONValue parameters, double[] host = null) {
    enforce(parameters.type == JSONType.object, "Depth parameters must be an object");
    string[] sectionFields = ["type","depth","depth_profile","center_u","radius_u","exponent",
        "base","cross_section","theta_degrees"];
    auto result = new double[query.length]; result[] = 0;
    foreach (p; query) enforce(p[0]>=0 && p[0]<=1 && p[1]>=0 && p[1]<=1, "Depth query outside material UV");
    foreach (op; operators.array) {
        auto kind = op["type"].str;
        bool ordinary = kind == "section_surface" || kind == "section" || kind == "flared_shell";
        bool swept = kind == "swept_surface" || kind == "swept_tube" || kind == "ribbon";
        if (ordinary || swept) known(op,sectionFields ~ (swept ?
            ["center_depth_profile","center_u_profile","radius_u_profile"] : cast(string[])null));
        else if (kind == "compact_relief") known(op,["type","center","radius","height","rotation_degrees"]);
        else if (kind == "host_offset") known(op,["type","offset"]);
        else if (kind == "constant") known(op,["type","value"]);
        else if (kind == "bend") known(op,["type","amplitude","axis","power","origin"]);
        else if (kind == "wave") known(op,["type","amplitude","axis","frequency","phase","fade_axis","fade_power"]);
        else if (kind == "ribbon_network") known(op,["type","ribbons","outside_depth"]);
        else throw new Exception("Unknown template depth operator: " ~ kind);
        if (kind == "host_offset") enforce(host.length == query.length && host !is null, "Missing explicit host depth");
        foreach (i, p; query) {
            double addition = 0;
            if (ordinary || swept) addition = section(p,op,parameters,swept);
            else if (kind == "compact_relief") addition = scalar(op["height"],parameters)*ngRigCompactSupport(p,op,parameters);
            else if (kind == "host_offset") addition = host[i]+value(op,"offset",parameters,0);
            else if (kind == "constant") addition = value(op,"value",parameters,0);
            else if (kind == "bend") {
                auto axis = ngRigString(op,"axis","v");
                enforce(axis == "u" || axis == "v", "Unknown bend axis");
                double power = value(op,"power",parameters,2);
                enforce(power>0, "Nonpositive bend power");
                double distance = p[axis == "v"]-value(op,"origin",parameters,0);
                addition = scalar(op["amplitude"],parameters)*(distance < 0 ? -1 : distance > 0 ? 1 : 0)*pow(abs(distance),power);
            } else if (kind == "wave") {
                auto axis = ngRigString(op,"axis","u");
                enforce(axis == "u" || axis == "v", "Unknown wave axis");
                double fade = 1;
                auto fadeAxis = ngRigGet(op,"fade_axis");
                if (fadeAxis.type != JSONType.null_) {
                    enforce(fadeAxis.str == "u" || fadeAxis.str == "v", "Unknown wave fade axis");
                    double power = value(op,"fade_power",parameters,1);
                    enforce(power>0, "Nonpositive wave fade power");
                    fade = pow(p[fadeAxis.str == "v"],power);
                }
                addition = scalar(op["amplitude"],parameters)*fade*
                    sin(2*PI*(value(op,"frequency",parameters,1)*p[axis == "v"]+value(op,"phase",parameters,0)));
            } else if (kind == "ribbon_network") {
                enforce(op["ribbons"].array.length > 0, "Empty ribbon network");
                bool assigned = false;
                addition = value(op,"outside_depth",parameters,0);
                foreach (ribbon; op["ribbons"].array) {
                    known(ribbon,["support","depth","depth_profile","base","center_depth_profile"]);
                    auto box = ngRigNumbers(ngRigGet(ribbon,"support",JSONValue([0.,0.,1.,1.])));
                    enforce(box.length == 4 && box[0]>=0 && box[1]>=0 && box[2]<=1 && box[3]<=1 &&
                        box[2]>box[0] && box[3]>box[1], "Invalid ribbon support rectangle");
                    if (p[0]<box[0] || p[0]>box[2] || p[1]<box[1] || p[1]>box[3]) continue;
                    Point2 local = [(p[0]-box[0])/(box[2]-box[0]),(p[1]-box[1])/(box[3]-box[1])];
                    double z = section(local,ribbon,parameters,true,true);
                    enforce(!assigned || abs(addition-z)<=1e-9, "Incompatible overlapping ribbon depths");
                    assigned = true; addition = z;
                }
            }
            result[i] += addition;
            enforce(isFinite(result[i]), "Nonfinite template depth");
        }
    }
    return result;
}
