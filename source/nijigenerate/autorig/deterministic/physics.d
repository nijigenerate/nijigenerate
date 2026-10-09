module nijigenerate.autorig.deterministic.physics;

import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.deterministic.evidence : AlphaSupportIndex;
import nijigenerate.autorig.deterministic.linear : ngRigLeastSquares;
import nijigenerate.autorig.deterministic.templates : ngRigMaterialRoles;
import nijigenerate.autorig.framework : AutoRigTaskContext;
import std.json : JSONValue, JSONType, parseJSON;
import std.algorithm : clamp, min, max, sort, canFind;
import std.array : array;
import std.math : sqrt, abs, isFinite, floor, ceil, rint;
import std.exception : enforce;
import std.regex : regex, matchFirst, replaceAll;
import std.string : toLower, replace, startsWith, split;
import std.conv : to;

/** CPU-only observations. No editor objects or borrowed texture storage. */
struct RigPhysicsAsset {
    Point2[] points;
    bool[] mask;
    size_t width, height;
    double[6] pixelToModel;
    bool affineMapping;
}

struct RigPhysicsMesh {
    Point2[] vertices;
    Point2[] uv;
    uint[] indices;
    // Row-major neutral affine transform: xx, xy, tx, yx, yy, ty.
    double[6] toModel;
}

struct RigPhysicsFrame {
    Point2 origin, tangent, normal;
    double length;
}

JSONValue ngRigPhysicsPolicy() { return parseJSON(import("autorig/physics-policy.json")); }

private Point2 sub(Point2 a, Point2 b) { return [a[0]-b[0],a[1]-b[1]]; }
private double dot(Point2 a, Point2 b) { return a[0]*b[0]+a[1]*b[1]; }
private double norm(Point2 p) { return sqrt(dot(p,p)); }
private Point2 unit(Point2 p) { auto length = norm(p); enforce(length>1e-6,"Degenerate physics direction"); p[] /= length; return p; }
private double smooth(double value) { auto s = clamp(value,0.,1.); return s*s*(3-2*s); }
private double percentile(double[] input, double fraction) {
    enforce(input.length>0,"Empty physics support"); auto values = input.dup.sort.array;
    double position = fraction*(values.length-1); size_t i = cast(size_t)position;
    return values[i]*(1-(position-i))+values[min(i+1,values.length-1)]*(position-i);
}
private Point2 mean(Point2[] points) {
    enforce(points.length>0,"Empty physics section"); Point2 result = [0.,0.];
    foreach (p; points) { result[0] += p[0]; result[1] += p[1]; }
    result[] /= points.length; return result;
}
private Point2 section(Point2[] points, double[] projection, double fraction, bool upper) {
    auto threshold = percentile(projection,fraction); Point2[] selected;
    foreach (i,p; points) if (upper ? projection[i]>=threshold : projection[i]<=threshold) selected ~= p;
    return mean(selected);
}
private double segmentDistance(Point2 p, Point2 a, Point2 b) {
    auto v = sub(b,a); double t = clamp(dot(sub(p,a),v)/max(dot(v,v),1e-12),0.,1.);
    return norm([p[0]-a[0]-t*v[0],p[1]-a[1]-t*v[1]]);
}
private double chainDistance(Point2 p, Point2[] joints) {
    double distance = double.infinity;
    foreach (i; 1 .. joints.length) distance = min(distance,segmentDistance(p,joints[i-1],joints[i]));
    return distance;
}

RigPhysicsFrame ngRigPhysicsFrame(Point2 fixed, Point2 free) {
    auto v = sub(free,fixed); auto length = norm(v);
    enforce(isFinite(length) && length>1e-6,"Degenerate physics support-to-free frame");
    v[] /= length; return RigPhysicsFrame(fixed,[v[1],-v[0]],v,length);
}

/** Exact nearest-neighbour protection uses the existing project index. */
double[] ngRigPhysicsWeights(JSONValue spec, Point2[] points, JSONValue policy) {
    auto f = ngRigPhysicsFrame(ngRigPoint(spec["fixed"]),ngRigPoint(spec["free"]));
    double band = ngRigNumber(policy["anchor_band_fraction"]);
    auto limb = ngRigGet(spec,"limb"); Point2[] joints;
    double radius;
    if (limb.type != JSONType.null_) { joints = ngRigPoints(limb["joints"]); radius = ngRigNumber(limb["radius"]); }
    auto protectedPoints = ngRigPoints(ngRigGet(spec,"protected_pixels",JSONValue(cast(JSONValue[])null)));
    auto boundaryPoints = ngRigPoints(ngRigGet(spec,"boundary_pixels",JSONValue(cast(JSONValue[])null)));
    auto protectedIndex = protectedPoints.length ? new AlphaSupportIndex(protectedPoints) : null;
    auto boundaryIndex = boundaryPoints.length ? new AlphaSupportIndex(boundaryPoints) : null;
    auto result = new double[points.length];
    foreach (i,p; points) {
        auto s = dot(sub(p,f.origin),f.normal)/f.length;
        double w = smooth((s-band)/(1-band));
        if (spec["pattern"].str == "two_ends") {
            auto t = smooth((s-band)/(1-2*band)); w = 16*t*t*(1-t)*(1-t);
        }
        if (joints.length) {
            w *= smooth((chainDistance(p,joints)-radius)/(radius*.75));
            foreach (joint; joints) w *= smooth((norm(sub(p,joint))-radius)/(radius*.75));
        }
        if (protectedIndex !is null) {
            auto margin = ngRigNumber(spec["protection_margin"]);
            w *= smooth((protectedIndex.distance(p)-margin)/margin);
        }
        if (boundaryIndex !is null) w *= smooth(boundaryIndex.distance(p)/ngRigNumber(spec["boundary_margin"]));
        result[i] = clamp(w,0.,1.);
    }
    return result;
}

private JSONValue currentRole(string name) {
    auto specification = parseJSON(import("autorig/physics-material-roles.json"));
    while (name.length && name[$-1] == '\0') name = name[0 .. $-1];
    auto normalized = replaceAll(name.toLower,regex("[\\s:\\-]+"),"_");
    normalized = replaceAll(normalized,regex("__+"),"_");
    while (normalized.length && normalized[0] == '_') normalized = normalized[1 .. $];
    while (normalized.length && normalized[$-1] == '_') normalized = normalized[0 .. $-1];
    bool decorator;
    foreach (prefix; specification["decorator_prefixes"].array) decorator |= normalized.startsWith(prefix.str);
    string[] tags;
    foreach (token; normalized.split("_")) {
        auto tag = token == "left" ? "l" : token == "right" ? "r" : token;
        if ((tag == "l" || tag == "r") && !tags.canFind(tag)) tags ~= tag;
    }
    JSONValue[] candidates;
    foreach (rule; specification[decorator ? "receivers" : "rules"].array) {
        auto pattern = rule["pattern"].str.replace("(?P<side>","(?<side>");
        if (matchFirst(normalized,regex(pattern)).empty) continue;
        bool needsSide = ngRigGet(rule,"side",JSONValue(false)).boolean ||
            (rule["owner"].str ~ rule["chart"].str).canFind("{side}");
        if (needsSide && tags.length != 1) return JSONValue.init;
        candidates ~= JSONValue(["usage":JSONValue(decorator ? "decoration" : rule["usage"].str)]);
    }
    return candidates.length == 1 ? candidates[0] : JSONValue.init;
}

/** Port of physics_structure.compile_structure, using imported-model alpha instead of files. */
JSONValue ngRigPhysicsStructure(JSONValue state, JSONValue program,
    out RigPhysicsAsset[ulong] assets, AutoRigTaskContext task = null) {
    enforce(ngRigString(program,"kind","humanoid") == "humanoid","Physics requires anatomical support");
    auto policy = ngRigPhysicsPolicy(); auto evidence = state["evidence"];
    auto rules = ngRigMaterialRoles()["rules"].array;
    Point2[string] joints;
    foreach (name,landmark; evidence["landmarks"].object) joints[name] = ngRigPoint(landmark["xy"]);
    JSONValue[ulong] carriers;
    foreach (carrier; program["carriers"].array) carriers[ngRigUnsigned(carrier["part"])] = carrier;
    JSONValue[ulong] materials;
    foreach (material; state["materials"].array) if (!material["static"].boolean) {
        auto id = ngRigUnsigned(material["uuid"]); auto row = JSONValue(material.object.dup);
        JSONValue role;
        foreach (rule; rules) if (rule["id"].str == material["role"].str) { role = rule; break; }
        enforce(role.type == JSONType.object,"Physics material has no structural role");
        auto tag = carriers[id]["side"].str.toLower;
        auto owner = role["owner"].str.replace("{side}",tag);
        auto chart = owner ~ "/" ~ role["chart"].str.replace("{side}",tag);
        row["owner"] = JSONValue(owner); row["chart"] = JSONValue(chart);
        row["usage"] = role["usage"]; row["rule"] = role["id"]; row["side"] = JSONValue(tag);
        if (auto receiver = "receiver" in material.object)
            row["usage"] = JSONValue("decoration");
        else {
            auto current = currentRole(material["name"].str);
            if (current.type != JSONType.null_ && current["usage"].str == "decoration")
                row["usage"] = current["usage"];
        }
        materials[id] = row;
    }
    auto ids = materials.keys.sort.array;
    JSONValue[] inventory, candidates; size_t[ulong] inventoryIndex;
    foreach (id; ids) {
        ngRigCheckpoint(task);
        auto m = materials[id];
        auto usage = m["usage"].str, owner = m["owner"].str, name = m["name"].str.toLower;
        inventoryIndex[id] = inventory.length;
        auto row = JSONValue(["target":JSONValue(id),"name":m["name"],"owner":m["owner"],
            "chart":m["chart"],"decision":JSONValue("exclude")]);
        inventory ~= row;
        void exclude(string reason, bool unresolved = false) {
            row["reason"] = JSONValue(reason);
            if (unresolved) row["decision"] = JSONValue("unresolved");
        }
        bool matches(string pattern) { return !matchFirst(name,regex(pattern)).empty; }
        bool mixed = usage == "anatomy" && owner.startsWith("arm:") && matches("sleeve|cloth|frill");
        auto current = currentRole(m["name"].str);
        if (current.type != JSONType.null_ && ["anatomy","terminal","feature"].canFind(current["usage"].str) && !mixed) {
            exclude("Human body/terminal/feature candidate protected by current anatomy rules, including stale garment classifications"); continue;
        }
        if (["anatomy","terminal","feature"].canFind(usage) && !mixed) {
            exclude("Human body, joints, hands/fingers, feet/shoes or face: skeleton motion only"); continue;
        }
        if (usage == "decoration") { exclude("Receiver decoration handled with its supported surface"); continue; }
        if (usage == "covering" && !owner.startsWith("arm:") && !owner.startsWith("leg:")) {
            exclude("Fitted torso/neck covering; no automatic chest/body sway"); continue;
        }
        assets[id] = ngRigPhysicsAsset(m,task);
        auto p = assets[id].points;
        bool included;
        scope(exit) if (!included) assets.remove(id);
        if (!p.length) { exclude("Empty alpha"); continue; }
        auto rule = m["rule"].str;
        string profile = "hanging", pattern = "one_end", bone;
        JSONValue limb; Point2 fixed, a, b, c, supportAxis; bool hasAxis;
        auto side = m["side"].str == "r" ? "R" : m["side"].str == "l" ? "L" : "";
        if ((owner.startsWith("arm:") || owner.startsWith("leg:")) && !side.length) {
            exclude("Shared or unknown limb side requires separate structural support regions",true); continue;
        }
        if (owner.startsWith("arm:")) {
            a = joints["shoulder." ~ side]; b = joints["elbow." ~ side]; c = joints["wrist." ~ side];
            auto radius = ngRigNumber(evidence["limb_radii"][owner])*ngRigNumber(policy["limb_protection_radius_scale"]);
            Point2[] chain = [a,b,c]; if (auto tip = "hand_tip." ~ side in joints) chain ~= *tip;
            double maximum = 0; foreach (point; p) maximum = max(maximum,chainDistance(point,chain));
            if (maximum<=radius*1.2) {
                exclude("Human body or fitted limb surface: entire alpha lies within the measured anatomical chain protection region");
                row["structural_authority"] = JSONValue(["joints":ngRigPointsJson(chain),"measured_radius":JSONValue(radius),
                    "max_alpha_distance":JSONValue(maximum)]); continue;
            }
            limb = JSONValue(["joints":ngRigPointsJson([a,b,c]),"radius":JSONValue(radius),"side":JSONValue(side)]);
            if (rule == "shoulder" || matches("cuff")) {
                fixed = rule == "shoulder" ? a : c; profile = "frill";
                bone = (rule == "shoulder" ? "UpperArm." : "Hand.") ~ side;
                if (matches("cuff") && !matches("bow|tassel|ribbon")) { supportAxis = sub(c,b); hasAxis = true; }
            } else if (rule == "sleeve" || mixed) {
                fixed = a; pattern = "two_ends"; profile = "sleeve"; bone = "UpperArm." ~ side;
            } else { exclude("Unresolved arm accessory material",true); continue; }
        } else if (owner.startsWith("leg:")) {
            if (rule == "thigh_accessory") { exclude("Fitted thigh band"); continue; }
            if (!matches("bow|ribbon|tassel|frill")) { exclude("Fitted ankle covering"); continue; }
            fixed = joints["ankle." ~ side]; bone = "Foot." ~ side;
            supportAxis = sub(fixed,joints["knee." ~ side]); hasAxis = true;
        } else if (usage == "covering") {
            exclude("Fitted torso/neck covering; no automatic chest/body sway"); continue;
        } else if (owner == "head") {
            bone = "Head";
            if (rule.startsWith("hair")) { fixed = joints["head_top"]; profile = "hair"; }
            else if (rule == "ear") {
                fixed = joints["head_top"]; profile = "ear";
                supportAxis = sub(fixed,joints["neck_base"]); hasAxis = true;
                auto upward = unit(supportAxis); double[] projection;
                foreach (point; p) projection ~= dot(sub(point,fixed),upward);
                if (percentile(projection,.95)<=0) {
                    exclude("Human body/head side ear without a verified protruding free region; skeleton motion only"); continue;
                }
            } else if (matches("bow|ribbon|tassel|frill")) fixed = joints["head_top"];
            else { exclude("Rigid fitted head covering"); continue; }
        } else if (owner == "torso") {
            if (["skirt","skirt_back","apron"].canFind(rule)) { fixed = joints["waist"]; profile = "sheet"; bone = "Pelvis"; }
            else if (rule == "tail") { fixed = joints["pelvis"]; bone = "Pelvis"; }
            else if (["chest_accessory","pelvis_accessory"].canFind(rule) && matches("bow|ribbon|tassel|chain")) {
                fixed = joints[rule == "chest_accessory" ? "neck_base" : "waist"];
                bone = rule == "chest_accessory" ? "Chest" : "Pelvis";
            } else { exclude("Rigid or unresolved torso accessory"); continue; }
        } else { exclude("No anatomical support",true); continue; }
        if (matches("chain|tassel|ribbon|bow")) { profile = "hanging"; pattern = "one_end"; }
        double[] distances; foreach (point; p) distances ~= norm(sub(point,fixed));
        auto root = section(p,distances,.05,false);
        auto direction = sub(mean(p),root);
        if (norm(direction)<1e-6) { exclude("No free direction",true); continue; }
        direction = unit(direction); double[] projection;
        foreach (point; p) projection ~= dot(sub(point,root),direction);
        auto free = section(p,projection,.95,true);
        if (hasAxis) {
            direction = unit(supportAxis); projection = null;
            foreach (point; p) projection ~= dot(point,direction);
            if (profile != "ear") root = section(p,projection,.05,false);
            free = section(p,projection,.95,true);
        }
        if (pattern == "two_ends") {
            direction = unit(sub(c,a)); projection = null;
            foreach (point; p) projection ~= dot(sub(point,a),direction);
            root = section(p,projection,.05,false); free = section(p,projection,.95,true);
        }
        auto spec = JSONValue(["id":JSONValue(m["chart"].str ~ "/" ~ id.to!string),
            "name":JSONValue("Physics::" ~ m["chart"].str ~ "::" ~ id.to!string),
            "target":JSONValue(id),"parent_bone":JSONValue(bone),"profile":JSONValue(profile),
            "pattern":JSONValue(pattern),"fixed":JSONValue(root[]),"free":JSONValue(free[]),
            "limb":limb,"owner":JSONValue(owner),"protected_pixels":JSONValue(cast(JSONValue[])null),
            "protection_margin":JSONValue(1.)]);
        if (limb.type != JSONType.null_) {
            auto chain = ngRigPoints(limb["joints"]);
            if (auto tip = "hand_tip." ~ side in joints) { chain ~= *tip; limb["joints"] = ngRigPointsJson(chain); }
            if (mixed) {
                // Preserve the Python unresolved decision, rather than invent a skin/cloth segmentation.
                exclude("Combined body/cloth lacks resolved anatomical surface boundary; protect entire Part until structural region recognition is verified",true);
                continue;
            }
            spec["protection_margin"] = JSONValue(max(1.,ngRigNumber(limb["radius"])*.08));
            spec["mixed_anatomy_cloth"] = JSONValue(false);
        }
        auto f = ngRigPhysicsFrame(root,free); spec["length"] = JSONValue(f.length);
        Point2[] sampled; auto stride = max(cast(size_t)1,p.length/10000);
        for (size_t i; i<p.length; i+=stride) sampled ~= p[i];
        auto weights = ngRigPhysicsWeights(spec,sampled,policy); size_t moving;
        foreach (w; weights) moving += w>1e-6;
        if (cast(double)moving/weights.length<ngRigNumber(policy["minimum_free_fraction"])) {
            exclude("No supported free cloth outside protected anatomy/joints",true); continue;
        }
        if (matches("chain")) { exclude("Chain/rigid pendant segmentation and multiple anchors not yet resolved",true); continue; }
        row["decision"] = JSONValue("include"); row["reason"] = JSONValue("Anatomical support with local free region");
        row["pattern"] = JSONValue(pattern); row["profile"] = JSONValue(profile);
        spec["targets"] = JSONValue([JSONValue(id)]); candidates ~= spec;
        included = true;
    }
    foreach (id; ids) if (materials[id]["usage"].str == "decoration") {
        auto receiver = ngRigGet(materials[id],"receiver"); ulong[] visited;
        while (receiver.type != JSONType.null_) {
            auto receiverId = ngRigUnsigned(receiver);
            if (visited.canFind(receiverId)) break;
            visited ~= receiverId;
            auto material = receiverId in materials;
            if (material is null || (*material)["usage"].str != "decoration") break;
            auto next = ngRigGet(*material,"receiver");
            if (next.type == JSONType.null_) break;
            receiver = next;
        }
        size_t selected = size_t.max, largest;
        foreach (i,spec; candidates) if (receiver.type != JSONType.null_ ? receiver == spec["target"] :
            materials[ngRigUnsigned(spec["target"])]["chart"].str == materials[id]["chart"].str &&
            ["sheet","hair","sleeve"].canFind(spec["profile"].str)) {
            auto count = assets[ngRigUnsigned(spec["target"])].points.length;
            if (selected == size_t.max || count>largest) { selected = i; largest = count; }
        }
        if (selected != size_t.max) {
            assets[id] = ngRigPhysicsAsset(materials[id],task);
            candidates[selected]["targets"].array ~= JSONValue(id);
            auto row = inventory[inventoryIndex[id]]; row["decision"] = JSONValue("carried");
            row["reason"] = JSONValue("Primary observed receiver field"); row["carrier"] = candidates[selected]["id"];
        }
    }
    size_t[] fronts;
    foreach (i,spec; candidates) if (["skirt","apron"].canFind(materials[ngRigUnsigned(spec["target"])]["rule"].str) &&
        spec["profile"].str == "sheet") fronts ~= i;
    if (fronts.length) {
        auto base = candidates[fronts[0]]; Point2[] cloud; ulong[] targets;
        foreach (i; fronts) {
            cloud ~= assets[ngRigUnsigned(candidates[i]["target"])].points;
            foreach (id; candidates[i]["targets"].array) if (!targets.canFind(ngRigUnsigned(id))) targets ~= ngRigUnsigned(id);
        }
        double[] y; foreach (p; cloud) y ~= p[1];
        auto root = section(cloud,y,.05,false), free = section(cloud,y,.95,true);
        base["fixed"] = JSONValue(root[]); base["free"] = JSONValue(free[]);
        base["name"] = JSONValue("Physics::WaistCloth"); base["id"] = JSONValue("waist-cloth");
        base["length"] = JSONValue(ngRigPhysicsFrame(root,free).length); base["targets"] = JSONValue(targets.sort.array);
        JSONValue[] merged;
        foreach (i,spec; candidates) if (i == fronts[0] || !fronts.canFind(i)) merged ~= spec;
        candidates = merged;
    }
    foreach (child; candidates) {
        auto childId = ngRigUnsigned(child["target"]); auto childName = materials[childId]["name"].str.toLower;
        if (matchFirst(childName,regex("bow|tassel|ribbon")).empty) continue;
        double best = double.infinity; JSONValue selected;
        foreach (host; candidates) {
            if (host["id"].str == child["id"].str || host["owner"].str != child["owner"].str) continue;
            auto hostId = ngRigUnsigned(host["target"]); auto hostName = materials[hostId]["name"].str.toLower;
            bool compatible = child["owner"].str == "head" ? childName.canFind("tassel") && hostName.canFind("bow") &&
                materials[hostId]["rule"].str == "headwear" : ["sheet","sleeve","frill"].canFind(host["profile"].str) &&
                materials[hostId]["chart"].str == materials[childId]["chart"].str;
            if (!compatible) continue;
            auto index = new AlphaSupportIndex(assets[hostId].points);
            auto distance = index.distance(ngRigPoint(child["fixed"]));
            if (distance>max(ngRigNumber(host["length"])*ngRigNumber(policy["attachment_distance_fraction"]),
                ngRigNumber(child["length"])*.15)) continue;
            if (distance<best || (distance == best && host["id"].str<selected["id"].str)) { best = distance; selected = host; }
        }
        if (selected.type != JSONType.null_) {
            child["support_group"] = selected["id"]; child["support_anchor"] = child["fixed"];
            inventory[inventoryIndex[childId]]["support_group"] = selected["id"];
        }
    }
    return JSONValue(["schema_version":JSONValue("rig-physics-structure-d/1"),"inventory":JSONValue(inventory),
        "groups":JSONValue(candidates),"policy_sha256":JSONValue(ngRigDigest(policy))]);
}

Point2[] ngRigPhysicsDisplacement(JSONValue spec, Point2[] points, Point2 key, JSONValue policy) {
    auto f = ngRigPhysicsFrame(ngRigPoint(spec["fixed"]),ngRigPoint(spec["free"]));
    auto amplitude = ngRigNumbers(policy["profiles"][spec["profile"].str]["amplitude_ratio"]);
    Point2 vector = [f.length*(amplitude[0]*key[0]*f.tangent[0]-amplitude[1]*key[1]*f.normal[0]),
        f.length*(amplitude[0]*key[0]*f.tangent[1]-amplitude[1]*key[1]*f.normal[1])];
    auto weights = ngRigPhysicsWeights(spec,points,policy); auto result = new Point2[points.length];
    foreach (i,w; weights) result[i] = [w*vector[0],w*vector[1]];
    return result;
}

Point2[] ngRigPhysicsProtectTriangles(Point2[] delta, Point2[] vertices, uint[] indices,
    JSONValue spec, JSONValue policy) {
    enforce(delta.length == vertices.length && indices.length%3 == 0,"Invalid physics triangle arrays");
    auto result = delta.dup; auto weights = ngRigPhysicsWeights(spec,vertices,policy);
    for (size_t i; i<indices.length; i+=3) {
        auto face = indices[i .. i+3]; bool fixed;
        foreach (id; face) { enforce(id<vertices.length,"Physics triangle outside mesh"); fixed |= weights[id]<=1e-12; }
        if (fixed) foreach (id; face) result[id] = [0.,0.];
    }
    return result;
}

bool ngRigPhysicsNoInversion(Point2[] vertices, uint[] indices, Point2[] values, double scale = 1.) {
    enforce(vertices.length == values.length && indices.length%3 == 0,"Invalid physics topology arrays");
    double area(Point2 a, Point2 b, Point2 c) { return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]); }
    for (size_t i; i<indices.length; i+=3) {
        Point2[3] base, posed;
        foreach (j; 0 .. 3) {
            auto id = indices[i+j]; enforce(id<vertices.length,"Physics triangle outside mesh");
            base[j] = vertices[id]; posed[j] = [base[j][0]+values[id][0]*scale,base[j][1]+values[id][1]*scale];
        }
        auto before = area(base[0],base[1],base[2]);
        if (abs(before)>1e-8 && !(before*area(posed[0],posed[1],posed[2])>0)) return false;
    }
    return true;
}

Point2 ngRigPhysicsTransform(double[6] matrix, Point2 p, bool direction = false) {
    return [matrix[0]*p[0]+matrix[1]*p[1]+(direction ? 0 : matrix[2]),
        matrix[3]*p[0]+matrix[4]*p[1]+(direction ? 0 : matrix[5])];
}

double[6] ngRigPhysicsInverse(double[6] m) {
    auto determinant = m[0]*m[4]-m[1]*m[3]; enforce(abs(determinant)>1e-20,"Singular physics frame");
    double[6] result = [m[4]/determinant,-m[1]/determinant,0.,-m[3]/determinant,m[0]/determinant,0.];
    auto shift = ngRigPhysicsTransform(result,[m[2],m[5]],true); result[2] = -shift[0]; result[5] = -shift[1];
    return result;
}

/** Reconstruct the complete source alpha from immutable CPU runs, without PSD I/O. */
RigPhysicsAsset ngRigPhysicsAsset(JSONValue material, AutoRigTaskContext task = null) {
    auto dimensions = ngRigNumbers(material["texture_size"]);
    auto width = cast(size_t)dimensions[0], height = cast(size_t)dimensions[1];
    enforce(width && height && width<=size_t.max/height,"Invalid physics alpha dimensions");
    RigPhysicsAsset result; result.width = width; result.height = height; result.mask = new bool[width*height];
    auto runs = material["alpha_runs_32"].array;
    for (size_t i; i<runs.length; i+=2) {
        auto start = cast(size_t)ngRigUnsigned(runs[i]), count = cast(size_t)ngRigUnsigned(runs[i+1]);
        enforce(start<=result.mask.length && count<=result.mask.length-start,"Invalid physics alpha runs");
        result.mask[start .. start+count] = true;
    }
    auto mapping = material["source_root_mapping"];
    auto vertices = ngRigPoints(mapping["vertices"]), uv = ngRigPoints(mapping["uv"]);
    double[][] design, values;
    foreach (i,p; uv) { design ~= [p[0]*width,p[1]*height,1.]; values ~= vertices[i][].dup; }
    auto fitted = ngRigLeastSquares(design,values);
    result.pixelToModel = [fitted[0][0],fitted[1][0],fitted[2][0],fitted[0][1],fitted[1][1],fitted[2][1]];
    result.affineMapping = true;
    foreach (i,p; uv) {
        auto mapped = ngRigPhysicsTransform(result.pixelToModel,[p[0]*width,p[1]*height]);
        foreach (axis; 0 .. 2)
            result.affineMapping &= abs(mapped[axis]-vertices[i][axis])<=1e-8*max(1.,abs(vertices[i][axis]));
    }
    struct Triangle { size_t[3] ids; Point2 a, b, c; double determinant; }
    Triangle[] triangles;
    foreach (face; mapping["triangles"].array) {
        auto ids = ngRigNumbers(face); enforce(ids.length == 3,"Invalid physics UV triangle");
        Triangle triangle;
        foreach (i,id; ids) {
            enforce(id>=0 && id<vertices.length && id==cast(size_t)id,"Physics UV index outside mesh");
            triangle.ids[i] = cast(size_t)id;
        }
        triangle.a = uv[triangle.ids[0]]; triangle.b = sub(uv[triangle.ids[1]],triangle.a);
        triangle.c = sub(uv[triangle.ids[2]],triangle.a);
        triangle.determinant = triangle.b[0]*triangle.c[1]-triangle.b[1]*triangle.c[0];
        if (abs(triangle.determinant)>=1e-14) triangles ~= triangle;
    }
    enforce(triangles.length>0,"Physics material has no valid UV triangles");
    foreach (y; 0 .. height) {
        if (y%64 == 0) ngRigCheckpoint(task);
        foreach (x; 0 .. width) if (result.mask[y*width+x]) {
            Point2 pixel = [(x+.5)/width,(y+.5)/height]; bool covered;
            foreach (triangle; triangles) {
                auto q = sub(pixel,triangle.a);
                auto v = (q[0]*triangle.c[1]-q[1]*triangle.c[0])/triangle.determinant;
                auto w = (triangle.b[0]*q[1]-triangle.b[1]*q[0])/triangle.determinant;
                if (v < -1e-8 || w < -1e-8 || v+w > 1+1e-8) continue;
                Point2 point;
                foreach (axis; 0 .. 2) point[axis] = (1-v-w)*vertices[triangle.ids[0]][axis]+
                    v*vertices[triangle.ids[1]][axis]+w*vertices[triangle.ids[2]][axis];
                result.points ~= result.affineMapping ?
                    ngRigPhysicsTransform(result.pixelToModel,[x+.5,y+.5]) : point;
                covered = true; break;
            }
            result.mask[y*width+x] = covered;
        }
    }
    return result;
}

private bool touchesProtectedPixels(Point2[3] polygon, bool[] mask, size_t width, size_t height) {
    // ImageDraw.polygon converts coordinates to integers before its inclusive scan conversion.
    int[2][3] points;
    foreach (i,p; polygon) points[i] = [cast(int)p[0],cast(int)p[1]];
    auto low = max(0,min(points[0][1],min(points[1][1],points[2][1])));
    auto high = min(cast(int)height-1,max(points[0][1],max(points[1][1],points[2][1])));
    foreach (y; low .. high+1) {
        double[] intersections;
        foreach (i; 0 .. 3) {
            auto a = points[i], b = points[(i+1)%3];
            if (a[1] == b[1]) {
                if (y == a[1]) { intersections ~= cast(double)a[0]; intersections ~= cast(double)b[0]; }
            } else if (y>=min(a[1],b[1]) && y<=max(a[1],b[1]))
                intersections ~= a[0]+cast(double)(y-a[1])*(b[0]-a[0])/(b[1]-a[1]);
        }
        if (!intersections.length) continue;
        intersections.sort;
        auto x0 = max(0,cast(int)ceil(intersections[0]-.5));
        auto x1 = min(cast(int)width-1,cast(int)floor(intersections[$-1]+.5));
        foreach (x; x0 .. x1+1) if (mask[cast(size_t)y*width+x]) return true;
    }
    return false;
}

private Point2 sampleHost(JSONValue host, Point2 key, Point2 anchor, RigPhysicsMesh[ulong] meshes) {
    auto id = ngRigUnsigned(host["target"]), mesh = meshes[id];
    Point2[] positions;
    foreach (v; mesh.vertices) positions ~= ngRigPhysicsTransform(mesh.toModel,v);
    Point2[] values;
    foreach (op; host["operations"].array) if (ngRigUnsigned(op["target"]) == id && ngRigPoint(op["key"]) == key) {
        auto offsets = ngRigNumbers(op["values"]);
        for (size_t i; i<offsets.length; i+=2) values ~= ngRigPhysicsTransform(mesh.toModel,[offsets[i],offsets[i+1]],true);
        break;
    }
    enforce(values.length == positions.length,"Physics host key is missing");
    for (size_t i; i<mesh.indices.length; i+=3) {
        auto ia = mesh.indices[i], ib = mesh.indices[i+1], ic = mesh.indices[i+2];
        auto a = positions[ia], b = sub(positions[ib],a), c = sub(positions[ic],a), q = sub(anchor,a);
        auto determinant = b[0]*c[1]-c[0]*b[1];
        if (abs(determinant)<1e-8) continue;
        double v = (q[0]*c[1]-c[0]*q[1])/determinant, w = (b[0]*q[1]-q[0]*b[1])/determinant;
        if (min(1-v-w,min(v,w))>=-1e-6) return [(1-v-w)*values[ia][0]+v*values[ib][0]+w*values[ic][0],
            (1-v-w)*values[ia][1]+v*values[ib][1]+w*values[ic][1]];
    }
    size_t closest; double distance = double.infinity;
    foreach (i,p; positions) { auto d = norm(sub(p,anchor)); if (d<distance) { distance = d; closest = i; } }
    return values[closest];
}

/** Port of apply_physics.compile_operations on the existing meshes; never remeshes. */
JSONValue ngRigCompilePhysicsOperations(ref JSONValue structure, RigPhysicsAsset[ulong] assets,
    RigPhysicsMesh[ulong] meshes, AutoRigTaskContext task = null) {
    auto policy = ngRigPhysicsPolicy(); JSONValue[] groups, checks;
    foreach (source; structure["groups"].array) {
        auto spec = JSONValue(source.object.dup); JSONValue[] operations, groupChecks, targets;
        foreach (target; source["targets"].array) {
            ngRigCheckpoint(task); auto id = ngRigUnsigned(target), mesh = meshes[id], asset = assets[id];
            auto inverse = ngRigPhysicsInverse(mesh.toModel); Point2[] p;
            foreach (v; mesh.vertices) p ~= ngRigPhysicsTransform(mesh.toModel,v);
            auto weights = ngRigPhysicsWeights(spec,p,policy);
            auto dense = JSONValue(spec.object.dup);
            if (!ngRigGet(spec,"mixed_anatomy_cloth",JSONValue(false)).boolean) {
                dense["limb"] = JSONValue.init; dense["protected_pixels"] = JSONValue(cast(JSONValue[])null);
            }
            auto cloudWeights = ngRigPhysicsWeights(dense,asset.points,policy);
            auto fixedMask = new bool[asset.mask.length]; size_t cloudIndex;
            foreach (i,occupied; asset.mask) if (occupied) fixedMask[i] = cloudWeights[cloudIndex++]<=1e-12;
            auto modelToPixel = asset.affineMapping ? ngRigPhysicsInverse(asset.pixelToModel) : [1.,0.,0.,0.,1.,0.];
            bool[] fixedVertices = new bool[p.length];
            for (size_t i; i<mesh.indices.length; i+=3) {
                Point2[3] q;
                foreach (j; 0 .. 3) {
                    auto vertex = mesh.indices[i+j];
                    if (asset.affineMapping) q[j] = ngRigPhysicsTransform(modelToPixel,p[vertex]);
                    else {
                        enforce(mesh.uv.length == mesh.vertices.length,"Physics UV count differs from mesh");
                        q[j] = [mesh.uv[vertex][0]*asset.width,mesh.uv[vertex][1]*asset.height];
                    }
                    q[j][0] -= .5; q[j][1] -= .5;
                }
                // Python rasterizes in a clipped face-local box, which affects truncation.
                auto x0 = max(0,cast(int)floor(min(q[0][0],min(q[1][0],q[2][0]))));
                auto y0 = max(0,cast(int)floor(min(q[0][1],min(q[1][1],q[2][1]))));
                auto x1 = min(cast(int)asset.width-1,cast(int)ceil(max(q[0][0],max(q[1][0],q[2][0]))));
                auto y1 = min(cast(int)asset.height-1,cast(int)ceil(max(q[0][1],max(q[1][1],q[2][1]))));
                if (x1<x0 || y1<y0) continue;
                auto width = cast(size_t)(x1-x0+1), height = cast(size_t)(y1-y0+1);
                bool[] mask = new bool[width*height];
                foreach (y; 0 .. height) mask[y*width .. (y+1)*width] =
                    fixedMask[(y+y0)*asset.width+x0 .. (y+y0)*asset.width+x0+width];
                foreach (ref point; q) { point[0] -= x0; point[1] -= y0; }
                if (touchesProtectedPixels(q,mask,width,height))
                    foreach (j; 0 .. 3) fixedVertices[mesh.indices[i+j]] = true;
            }
            Point2[][] values; Point2[] keys;
            foreach (x; [-1.,0.,1.]) foreach (y; [-1.,0.,1.]) {
                Point2 key = [x,y]; keys ~= key;
                auto delta = ngRigPhysicsProtectTriangles(ngRigPhysicsDisplacement(spec,p,key,policy),p,mesh.indices,dense,policy);
                foreach (i; 0 .. delta.length) if (weights[i]<=1e-12 || fixedVertices[i]) delta[i] = [0.,0.];
                foreach (ref value; delta) value = ngRigPhysicsTransform(inverse,value,true);
                values ~= delta;
            }
            double scale = 1.;
            bool preserved(double amplitude) {
                foreach (value; values) if (!ngRigPhysicsNoInversion(mesh.vertices,mesh.indices,value,amplitude)) return false;
                return true;
            }
            while (scale>=1./128 && !preserved(scale)) scale *= .5;
            enforce(scale>=1./128,"Physics field cannot preserve existing mesh topology");
            size_t moving, fixed; double maximum = 0;
            foreach (i,value; values[0]) moving += abs(value[0]*scale)>1e-6 || abs(value[1]*scale)>1e-6;
            foreach (weight; weights) fixed += weight == 0;
            if (!moving) {
                foreach (ref row; structure["inventory"].array) if (ngRigUnsigned(row["target"]) == id) {
                    row["decision"] = JSONValue(id == ngRigUnsigned(source["target"]) ? "unresolved" : "exclude");
                    row["reason"] = JSONValue("Existing mesh has no resolved free vertices; retain fully fixed region");
                }
                continue;
            }
            targets ~= target;
            foreach (i,value; values) {
                double[] flat; Point2[] quantized;
                foreach (v; value) {
                    maximum = max(maximum,norm(v)*scale);
                    Point2 q = [rint(v[0]*scale*1e6)/1e6,rint(v[1]*scale*1e6)/1e6];
                    quantized ~= q; flat ~= q[];
                }
                enforce(ngRigPhysicsNoInversion(mesh.vertices,mesh.indices,quantized),"Quantized physics key inverts a face");
                operations ~= JSONValue(["target":target,"key":JSONValue(keys[i][]),"values":JSONValue(flat)]);
            }
            groupChecks ~= JSONValue(["target":target,"moving_vertices":JSONValue(moving),"fixed_vertices":JSONValue(fixed),
                "amplitude_topology_scale":JSONValue(scale),"triangles_preserved":JSONValue(true),
                "neutral_zero":JSONValue(true),"maximum_displacement":JSONValue(maximum)]);
        }
        bool primary; foreach (target; targets) primary |= ngRigUnsigned(target) == ngRigUnsigned(source["target"]);
        if (!primary) continue;
        spec["operations"] = JSONValue(operations); spec["targets"] = JSONValue(targets); spec["checks"] = JSONValue(groupChecks);
        groups ~= spec; checks ~= groupChecks;
    }
    size_t[string] byId; foreach (i,g; groups) byId[g["id"].str] = i;
    foreach (child; groups) {
        auto support = ngRigString(child,"support_group"); auto index = support in byId;
        if (index is null) continue;
        auto host = groups[*index];
        foreach (target; child["targets"].array) {
            bool exists; foreach (id; host["targets"].array) exists |= ngRigUnsigned(id) == ngRigUnsigned(target);
            if (exists) continue;
            auto mesh = meshes[ngRigUnsigned(target)], inverse = ngRigPhysicsInverse(mesh.toModel);
            JSONValue[] carried; bool moving;
            foreach (x; [-1.,0.,1.]) foreach (y; [-1.,0.,1.]) {
                Point2 key = [x,y]; auto delta = sampleHost(host,key,ngRigPoint(child["support_anchor"]),meshes);
                auto local = ngRigPhysicsTransform(inverse,delta,true); double[] values;
                foreach (v; mesh.vertices) foreach (component; local) { auto q = rint(component*1e6)/1e6; values ~= q; moving |= q != 0; }
                carried ~= JSONValue(["target":target,"key":JSONValue(key[]),"values":JSONValue(values),"carried":JSONValue(true)]);
            }
            if (!moving) continue;
            host["operations"].array ~= carried; host["targets"].array ~= target;
        }
    }
    return JSONValue(["policy":policy,"groups":JSONValue(groups),"checks":JSONValue(checks)]);
}
