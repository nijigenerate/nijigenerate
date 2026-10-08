module nijigenerate.autorig.deterministic.evidence;

import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.deterministic.templates;
import nijigenerate.autorig.deterministic.neck : ngRigInferNeckBase, ngRigInferHeadFrame;
import nijigenerate.autorig.deterministic.registered : ngRigFacialLandmarks;
import std.json : JSONValue;
import std.algorithm : sort, min, max;
import std.array : array;
import std.exception : enforce;
import std.math : sqrt, abs;
import std.regex : regex, matchFirst, replaceAll;
import std.string : toLower, replace, split, join, startsWith;
import nijigenerate.autorig.framework : AutoRigTaskContext;

/** Propagate receiver inactivity after alpha support has been observed for all Parts. */
void ngRigPropagateClippingVisibility(ref JSONValue[] materials) {
    size_t[ulong] indices;
    foreach (i, material; materials) indices[ngRigUnsigned(material["uuid"])] = i;
    bool changed;
    do {
        changed = false;
        foreach (ref material; materials) if (material["active"].boolean) {
            if (auto receiver = "receiver" in material.object) {
                auto index = ngRigUnsigned(*receiver) in indices;
                if (index !is null && !materials[*index]["active"].boolean) {
                    material["active"] = JSONValue(false);
                    changed = true;
                }
            }
        }
    } while (changed);
}

/** Resolve a rendered folder receiver to its first active material in PSD source order. */
void ngRigResolveGroupReceivers(ref JSONValue[] materials, JSONValue[] groups, bool classified = false) {
    bool[ulong] materialIds, groupIds;
    foreach (material; materials) materialIds[ngRigUnsigned(material["uuid"])] = true;
    foreach (group; groups) groupIds[ngRigUnsigned(group["uuid"])] = true;
    foreach (ref material; materials) {
        auto receiver = "receiver" in material.object;
        if (receiver is null) continue;
        auto originalReceiver = ngRigGet(material,"mask_receiver",*receiver);
        auto id = ngRigUnsigned(originalReceiver);
        if ((id in materialIds) !is null || (id in groupIds) is null) continue;
        JSONValue selected;
        double order = double.infinity;
        foreach (candidate; materials) {
            if (!candidate["active"].boolean) continue;
            if (classified && ngRigGet(candidate,"static",JSONValue(false)).boolean) continue;
            auto ancestors = "ancestor_ids" in candidate.object;
            if (ancestors is null) continue;
            bool belongs;
            foreach (ancestor; ancestors.array) if (ngRigUnsigned(ancestor) == id) belongs = true;
            auto candidateOrder = ngRigNumber(candidate["source_order"]);
            if (belongs && candidateOrder < order) { selected = candidate["uuid"]; order = candidateOrder; }
        }
        material["mask_receiver"] = originalReceiver;
        if (order == double.infinity) {
            if (classified) material["static"] = JSONValue(true);
            else material["active"] = JSONValue(false);
        }
        else material["receiver"] = selected;
    }
}

private string nameTokens(string name) {
    while (name.length && name[$-1] == '\0') name = name[0 .. $-1];
    while (name.length && (name[0] == '*' || name[0] == '#' || name[0] == ' ')) name = name[1 .. $];
    auto result = replaceAll(name.toLower,regex("[\\s:\\-]+"),"_");
    result = replaceAll(result,regex("__+"),"_");
    while (result.length && result[0] == '_') result = result[1 .. $];
    while (result.length && result[$-1] == '_') result = result[0 .. $-1];
    result = replaceAll(result,regex("(?<=[a-z])(?=[0-9])|(?<=[0-9])(?=[a-z])"),"_");
    string[string] aliases = ["irides":"iris","eyewhite":"sclera","eye_white":"sclera",
        "eyeslash":"eyelash","eylid":"eyelid","innewr":"inner","sholder":"shoulder",
        "cloths":"clothes","sodenhair":"side_hair"];
    foreach (source, target; aliases)
        result = replaceAll(result,regex("(?<![a-z])" ~ source ~ "(?![a-z])"),target);
    return result;
}

private string semanticRole(string name, string[] ancestors) {
    auto n = nameTokens(name);
    string[] parents;
    foreach (parent; ancestors) parents ~= nameTokens(parent);
    bool matches(string pattern, string value = "") {
        return !matchFirst(value.length ? value : n,regex(pattern)).empty;
    }
    if (matches("(^|_)(background|backdrop)(_|$)")) return "background";
    if (matches("fox_decor|plush|baggage|objects",parents.join("_") ~ "_" ~ n)) return "pelvis_accessory";
    if (matches("(^|_)(mouth|lip|tongue|teeth)(_|$)")) return "face_feature";
    foreach (parent; parents) if (matches("^mouth(?:_composite)?$",parent)) return "face_feature";
    if (matches("(^|_)(eyebrow|brow|sclera|eyeball|iris|pupil|canthus|eye_corner|side_eyelash|eyelid|eyeline|lid|lash|eyelash|double_eyelid)(_|$)|(^|_)eye_(white|highlight|light)(_|$)|^nose(?:_[0-9]+)?$"))
        return "face_feature";
    if (matches("^(face|head_skin|head)(?:_[0-9]+)?$")) return "face";
    if (matches("^neck(?:_[0-9]+)?$")) return "neck";
    if (matches("(^|_)(hair|bang|bangs|braid)(_|$)")) {
        if (matches("back")) return "hair_back";
        if (matches("side|cheek|curl|braid")) return "hair_side";
        return "hair_front";
    }
    if (matches("earring|earwear")) return "headwear";
    if (matches("(^|_)(ear|ears)(_|$)")) return "ear";
    if (matches("(^|_)(headwear|headband|cap|forehead|temple|head|hair)_(bow|ribbon|flower|watch|cross)|^(headwear|headband|cap)(_|$)"))
        return "headwear";
    if (matches("(^|_)(shoe|shoes|foot|footwear)(_|$)")) return "foot";
    if (matches("(^|_)(hand|palm|finger|thumb)(_|$)")) return "hand";
    if (matches("handwear")) return "arm";
    if (matches("(^|_)(arm|arms|upperarm|forearm)(_|$)")) return "arm";
    if (matches("(^|_)(leg|legs|legwear|thigh|shin|pants|socks)(_|$)")) return "leg";
    if (matches("(^|_)(sleeve|cuff|shoulder)(_|$)")) return "sleeve";
    if (matches("tail")) return "tail";
    if (matches("apron")) return "apron";
    if (matches("skirt|bottomwear|train")) return matches("back") ? "skirt_back" : "skirt";
    if (matches("corset|belt|waistwear")) return "waistwear";
    if (matches("clothes|topwear|bodice|collar|parker|pocket")) return "bodice";
    if (matches("(^|_)(body|torso|chest|pelvis)(_|$)")) return "torso";
    if (matches("waist|hip")) return "pelvis_accessory";
    foreach_reverse (parent; parents) {
        if (matches("hand",parent)) return "hand";
        if (matches("arms|arm",parent)) return "sleeve";
        if (matches("legs|leg",parent)) return "leg";
        if (matches("skirt",parent)) return "skirt";
        if (matches("upper_body|torso|body",parent)) return "chest_accessory";
        if (matches("cap|headwear",parent)) return "headwear";
        if (matches("back_head|back_hair",parent)) return "hair_back";
        if (matches("head",parent)) return "headwear";
    }
    return "";
}

string ngRigMaterialFeature(string name, string[] ancestors = null) {
    auto n = nameTokens(name);
    bool matches(string pattern) { return !matchFirst(n,regex(pattern)).empty; }
    bool mouth = matches("(^|_)(mouth|lip|tongue|teeth)(_|$)");
    foreach (parent; ancestors) if (!matchFirst(nameTokens(parent),regex("^mouth(?:_composite)?$")).empty) mouth = true;
    if (mouth) {
        if (matches("tongue")) return "mouth_tongue";
        if (matches("teeth")) return matches("upper") ? "mouth_upper_teeth" : "mouth_lower_teeth";
        if (matches("outline")) return "mouth_outline";
        if (matches("lip")) return matches("upper") ? "mouth_upper_lip" : "mouth_lower_lip";
        return "mouth";
    }
    if (matches("(^|_)(eyebrow|brow)(_|$)")) return "brow";
    if (matches("(^|_)(sclera|eyeball)(_|$)")) return "sclera";
    if (matches("(^|_)(iris|pupil)(_|$)|(^|_)eye_(highlight|light)(_|$)")) return "iris";
    if (matches("(^|_)(canthus|eye_corner|side_eyelash)(_|$)")) return "corner";
    if (matches("(^|_)(eyelid|eyeline|lid|lash|eyelash|double_eyelid)(_|$)"))
        return matches("lower|bottom") ? "lower" : matches("double|fold") ? "fold" : matches("side") ? "corner" : "upper";
    if (matches("^nose(?:_[0-9]+)?$")) return "nose";
    return "";
}

/** Exact nearest-alpha queries without an additional solver or spatial library. */
package(nijigenerate.autorig) class AlphaSupportIndex {
    private struct Entry {
        Point2 point;
        double[4] bounds;
        size_t left = size_t.max, right = size_t.max;
    }
    private Entry[] entries;
    private size_t root;
    this(Point2[] points) { root = build(points.dup,0); }
    private size_t build(Point2[] points, size_t axis) {
        if (!points.length) return size_t.max;
        points.sort!((a,b) => a[axis] < b[axis]);
        size_t middle = points.length/2, id = entries.length;
        auto point = points[middle];
        entries ~= Entry(point,[point[0],point[1],point[0],point[1]]);
        auto left = build(points[0 .. middle],1-axis);
        auto right = build(points[middle+1 .. $],1-axis);
        entries[id].left = left; entries[id].right = right;
        foreach (child; [left,right]) if (child != size_t.max) {
            auto bounds = entries[child].bounds;
            entries[id].bounds[0] = min(entries[id].bounds[0],bounds[0]);
            entries[id].bounds[1] = min(entries[id].bounds[1],bounds[1]);
            entries[id].bounds[2] = max(entries[id].bounds[2],bounds[2]);
            entries[id].bounds[3] = max(entries[id].bounds[3],bounds[3]);
        }
        return id;
    }
    double distance(Point2 query) {
        double best = double.infinity;
        double lowerBound(size_t id) {
            if (id == size_t.max) return double.infinity;
            auto bounds = entries[id].bounds;
            double dx = max(0.,max(bounds[0]-query[0],query[0]-bounds[2]));
            double dy = max(0.,max(bounds[1]-query[1],query[1]-bounds[3]));
            return dx*dx+dy*dy;
        }
        void visit(size_t id) {
            if (id == size_t.max) return;
            auto node = entries[id];
            double dx = query[0]-node.point[0], dy = query[1]-node.point[1];
            best = min(best,dx*dx+dy*dy);
            // Use exact subtree bounds, including the node itself. Plane-only
            // pruning visits most points when ornament roots lie outside a cloud.
            auto leftDistance = lowerBound(node.left), rightDistance = lowerBound(node.right);
            size_t near = leftDistance <= rightDistance ? node.left : node.right;
            size_t far = leftDistance <= rightDistance ? node.right : node.left;
            if (min(leftDistance,rightDistance)<=best) visit(near);
            if (max(leftDistance,rightDistance)<=best) visit(far);
        }
        visit(root); return sqrt(best);
    }
}

private double quantile(double[] values, double fraction) {
    enforce(values.length > 0, "Empty measured support");
    auto sorted = values.dup.sort.array;
    double index = fraction*(sorted.length-1);
    size_t first = cast(size_t)index, last = min(first+1,sorted.length-1);
    return sorted[first]*(1-(index-first))+sorted[last]*(index-first);
}

private double norm(Point2 p) { return sqrt(p[0]*p[0]+p[1]*p[1]); }
private Point2 minus(Point2 a, Point2 b) { return [a[0]-b[0],a[1]-b[1]]; }
private Point2 midpoint(Point2 a, Point2 b) { return [(a[0]+b[0])/2,(a[1]+b[1])/2]; }

/** Quantile section stations preserve measured tilt and asymmetric silhouettes. */
Point2 ngRigMeasuredSection(Point2[] cloud, double fraction, Point2 axis = [0.,1.]) {
    double length = norm(axis); enforce(length>0 && cloud.length>0, "Degenerate measured section"); axis[] /= length;
    Point2 perpendicular = [axis[1],-axis[0]];
    double[] longitudinal;
    foreach (p; cloud) longitudinal ~= p[0]*axis[0]+p[1]*axis[1];
    double low = quantile(longitudinal,.01), high = quantile(longitudinal,.99), station = low+fraction*(high-low);
    double[] transverse;
    foreach (i, p; cloud) if (abs(longitudinal[i]-station)<=max(1.,(high-low)*.025))
        transverse ~= p[0]*perpendicular[0]+p[1]*perpendicular[1];
    if (!transverse.length) {
        size_t[] order;
        foreach (i; 0 .. cloud.length) order ~= i;
        order.sort!((a,b)=>abs(longitudinal[a]-station)<abs(longitudinal[b]-station));
        foreach (i; order[0 .. max(1,cloud.length/100)])
            transverse ~= cloud[i][0]*perpendicular[0]+cloud[i][1]*perpendicular[1];
    }
    double coordinate = quantile(transverse,.5);
    return [coordinate*perpendicular[0]+station*axis[0],coordinate*perpendicular[1]+station*axis[1]];
}

/** Name candidates are a preliminary hint; full classification also uses structure and alpha. */
string[] ngRigMaterialRoleCandidates(string materialName) {
    auto role = semanticRole(materialName,null);
    return role.length && role != "background" ? [role] : null;
}

JSONValue ngRigClassifyMaterials(JSONValue observation, JSONValue options, AutoRigTaskContext context = null) {
    auto specification = ngRigMaterialRoles();
    auto overrides = ngRigGet(options,"materials",JSONValue(cast(JSONValue[string])null));
    JSONValue[] materials;
    bool[] explicitRole;
    foreach (original; observation["materials"].array) {
        ngRigCheckpoint(context);
        auto record = JSONValue(original.object.dup);
        string[] ancestors;
        if (auto parentNames = "ancestors" in original.object) foreach (parent; parentNames.array) ancestors ~= parent.str;
        else {
            auto path = original["path"].str.split("/");
            if (path.length>1) ancestors = path[0 .. $-1];
        }
        auto role = semanticRole(original["name"].str,ancestors);
        auto feature = ngRigMaterialFeature(original["name"].str,ancestors);
        if (role == "pelvis_accessory") feature = "";
        auto overrideRecord = original["path"].str in overrides.object;
        bool overridden = overrideRecord !is null && "role" in (*overrideRecord).object;
        if (overrideRecord !is null && "role" in (*overrideRecord).object) {
            role = (*overrideRecord)["role"].str;
            bool valid;
            foreach (rule; specification["rules"].array) if (rule["id"].str == role) valid = true;
            enforce(valid,"Unknown material role override: " ~ role);
        }
        bool active = original["active"].boolean;
        bool stationary = overrideRecord !is null && ngRigGet(*overrideRecord,"static",JSONValue(false)).boolean;
        record["static"] = JSONValue(stationary || !active || role == "background");
        record["role"] = JSONValue(role);
        record["feature"] = JSONValue(feature);
        record["semantic_source"] = JSONValue(overridden ? "explicit_override" : "model_name_and_ancestry_candidate");
        string side;
        foreach (token; original["name"].str.toLower.replace("-","_").split("_")) {
            if (token == "l" || token == "left") side = "L";
            if (token == "r" || token == "right") side = "R";
        }
        record["side_hint"] = JSONValue(overrideRecord is null ? side : ngRigString(*overrideRecord,"side",side));
        materials ~= record;
        explicitRole ~= overridden;
    }
    // Preserve the backdrop classifier using measured alpha support and perimeter coverage.
    Point2[] faceSupport, footSupport;
    foreach (material; materials) if (!material["static"].boolean) {
        if (material["role"].str == "face") faceSupport ~= ngRigPoints(material["cloud"]);
        if (material["role"].str == "foot") footSupport ~= ngRigPoints(material["cloud"]);
    }
    double[4] supportBounds(Point2[] points) {
        double[4] bounds = [double.infinity,double.infinity,-double.infinity,-double.infinity];
        foreach (p; points) {
            bounds[0]=min(bounds[0],p[0]); bounds[1]=min(bounds[1],p[1]);
            bounds[2]=max(bounds[2],p[0]); bounds[3]=max(bounds[3],p[1]);
        }
        return bounds;
    }
    if (faceSupport.length && footSupport.length) {
        auto faceBounds = supportBounds(faceSupport), footBounds = supportBounds(footSupport);
        double[] footY; foreach (p; footSupport) footY ~= p[1];
        foreach (ref material; materials) if (!material["static"].boolean && material["role"].str == "ear") {
            auto bounds = supportBounds(ngRigPoints(material["cloud"]));
            if (bounds[2]-bounds[0]>4*(faceBounds[2]-faceBounds[0]) &&
                bounds[3]-bounds[1]>.85*(footBounds[3]-faceBounds[1]) && bounds[3]>=quantile(footY,.5) &&
                ngRigNumber(ngRigGet(material,"opaque_perimeter_coverage",JSONValue(0.)))>=.65) {
                material["static"] = JSONValue(true);
                material["semantic_source"] = JSONValue("full_body_backdrop_alpha_perimeter");
            }
        }
    }
    // Imported clipping links replace PSD clipping_base_id as the receiver evidence.
    ngRigResolveGroupReceivers(materials,
        ngRigGet(observation,"groups",JSONValue(cast(JSONValue[])null)).array,true);
    size_t[uint] byId;
    foreach (i, material; materials) byId[cast(uint)ngRigNumber(material["uuid"])] = i;
    bool[] visiting = new bool[materials.length], resolved = new bool[materials.length];
    void inherit(size_t i) {
        if (resolved[i] || visiting[i]) return;
        visiting[i] = true;
        if (!explicitRole[i]) if (auto receiver = "receiver" in materials[i].object) {
            if (auto target = cast(uint)ngRigNumber(*receiver) in byId) {
                inherit(*target);
                auto role = materials[*target]["role"].str;
                if (role.length && role != "background") {
                    materials[i]["role"] = JSONValue(role);
                    if (!materials[i]["feature"].str.length) materials[i]["feature"] = materials[*target]["feature"];
                    materials[i]["semantic_source"] = JSONValue("imported_clipping_receiver");
                }
            }
        }
        visiting[i] = false; resolved[i] = true;
    }
    foreach (i; 0 .. materials.length) inherit(i);
    size_t[] known;
    AlphaSupportIndex[] indices;
    foreach (i, material; materials) if (!material["static"].boolean && material["role"].str.length) {
        auto cloud = ngRigPoints(material["cloud"]);
        if (cloud.length) { known ~= i; indices ~= new AlphaSupportIndex(cloud); }
    }
    foreach (i; 0 .. materials.length) {
        if (materials[i]["static"].boolean || materials[i]["role"].str.length) continue;
        ngRigCheckpoint(context);
        if (!known.length) {
            materials[i]["role"] = JSONValue("local");
            materials[i]["semantic_source"] = JSONValue("local_mechanism_without_anatomy");
            continue;
        }
        auto cloud = ngRigPoints(materials[i]["cloud"]);
        double[] ys; foreach (p; cloud) ys ~= p[1];
        double threshold = quantile(ys,.05);
        Point2[] roots; foreach (p; cloud) if (p[1]<=threshold) roots ~= p;
        double best = double.infinity;
        size_t support;
        JSONValue[] ranked;
        foreach (j, candidate; known) {
            ngRigCheckpoint(context);
            double[] distances;
            foreach (point; roots) distances ~= indices[j].distance(point);
            double distance = quantile(distances,.5);
            ranked ~= JSONValue(["part":materials[candidate]["uuid"],"distance":JSONValue(distance)]);
            if (distance<best || (distance==best && materials[candidate]["path"].str<materials[support]["path"].str)) {
                best = distance; support = candidate;
            }
        }
        ranked.sort!((a,b) => ngRigNumber(a["distance"])<ngRigNumber(b["distance"]));
        materials[i]["role"] = materials[support]["role"];
        materials[i]["support_part"] = materials[support]["uuid"];
        materials[i]["support_candidates"] = JSONValue(ranked[0 .. min(4,ranked.length)]);
        materials[i]["semantic_source"] = JSONValue("alpha_proximal_support_candidate");
    }
    foreach (ref material; materials) if (!material["static"].boolean) {
        if (material["role"].str == "local") {
            material["owner"] = JSONValue("local"); material["chart"] = JSONValue("local"); continue;
        }
        foreach (rule; specification["rules"].array) if (rule["id"].str == material["role"].str) {
            material["owner"] = rule["owner"]; material["chart"] = rule["chart"]; break;
        }
        enforce("owner" in material.object,"Unsupported automatic material role");
    }
    auto result = JSONValue(observation.object.dup);
    result["materials"] = JSONValue(materials);
    result["options"] = options;
    return result;
}

JSONValue ngRigDeriveEvidence(JSONValue observation) {
    auto materials = observation["materials"].array;
    bool hasFace, humanoidSupport;
    size_t active, mouthCount;
    foreach (material; materials) if (!material["static"].boolean) {
        ++active;
        auto role = material["role"].str;
        hasFace = hasFace || role == "face";
        humanoidSupport = humanoidSupport || role == "torso" || role == "bodice" || role == "leg" || role == "arm";
        if (ngRigString(material,"feature","").startsWith("mouth")) ++mouthCount;
    }
    enforce(active>0,"Imported model has no active deforming material");
    string kind = hasFace && humanoidSupport ? "humanoid" : hasFace ? "face" : mouthCount == active ? "mouth" : "local";
    if (kind != "humanoid") return JSONValue(["kind":JSONValue(kind),
        "source_sha256":observation["source_sha256"],"observation_sha256":JSONValue(ngRigDigest(observation))]);
    Point2[] select(string[] roles, string side = "", double center = 0) {
        Point2[] points;
        foreach (material; materials) {
            if (material["static"].boolean) continue;
            bool selected;
            foreach (role; roles) if (material["role"].str == role) selected = true;
            if (!selected) continue;
            foreach (p; ngRigPoints(material["cloud"]))
                if (!side.length || (p[0]<center) == (side == "R")) points ~= p;
        }
        return points;
    }
    auto face = select(["face"]), torso = select(["torso","bodice","waistwear"]);
    enforce(face.length>0 && torso.length>0, "Humanoid evidence needs visible face and torso support");
    double[] faceX, faceY;
    foreach (p; face) { faceX ~= p[0]; faceY ~= p[1]; }
    double center = quantile(faceX,.5);
    auto neck = select(["neck"]);
    JSONValue[string] landmarks, radii;
    void put(string role, Point2 p, string provenance = "measured") {
        landmarks[role] = JSONValue(["xy":JSONValue(p[]),"provenance":JSONValue(provenance),
            "weight":JSONValue(1.), "method":JSONValue("PSD alpha quantile section")]);
    }
    auto facial = ngRigFacialLandmarks(observation);
    auto headFrame = ngRigInferHeadFrame(face,midpoint(facial["eye_r_inner"],facial["eye_r_outer"]),
        midpoint(facial["eye_l_inner"],facial["eye_l_outer"]));
    put("head_top",ngRigPoint(headFrame["head_top"])); put("head_root",ngRigPoint(headFrame["head_root"]));
    auto neckInference = ngRigInferNeckBase(face,neck,select(["torso"]),select(["bodice","waistwear"]),headFrame);
    auto neckBase = ngRigPoint(neckInference["xy"]);
    put("neck_base",neckBase,neckInference["provenance"].str);
    landmarks["neck_base"]["method"] = neckInference["method"];
    foreach (side; ["R","L"]) {
        auto arm = select(["arm"],side,center), hand = select(["hand"],side,center);
        if (!arm.length) arm = select(["sleeve"],side,center);
        arm ~= hand;
        auto leg = select(["leg"],side,center), foot = select(["foot"],side,center);
        enforce(arm.length && leg.length, "Missing anatomical limb support: " ~ side);
        auto armStart = ngRigMeasuredSection(arm,.01), armEnd = ngRigMeasuredSection(hand.length ? hand : arm,.99);
        auto armAxis = minus(armEnd,armStart);
        put("shoulder." ~ side,ngRigMeasuredSection(arm,.02,armAxis));
        put("elbow." ~ side,ngRigMeasuredSection(arm,.52,armAxis),"prior");
        put("wrist." ~ side,ngRigMeasuredSection(hand.length ? hand : arm,hand.length ? .03 : .82,armAxis),
            hand.length ? "measured" : "prior");
        put("hand_tip." ~ side,armEnd);
        auto hip = ngRigMeasuredSection(leg,.01), distal = ngRigMeasuredSection(foot.length ? foot : leg,.98);
        auto legAxis = minus(distal,hip);
        put("hip." ~ side,hip,"prior"); put("knee." ~ side,ngRigMeasuredSection(leg,.47,legAxis),"prior");
        put("ankle." ~ side,ngRigMeasuredSection(foot.length ? foot : leg,foot.length ? .08 : .87,legAxis),
            foot.length ? "measured" : "prior"); put("foot_tip." ~ side,distal);
        foreach (family; ["arm","leg"]) {
            auto cloud = family == "arm" ? arm : leg;
            auto axis = family == "arm" ? armAxis : legAxis; axis[] /= norm(axis);
            double[] t; foreach (p; cloud) t ~= p[0]*axis[0]+p[1]*axis[1];
            double low = quantile(t,.01), high = quantile(t,.99);
            double[] widths;
            foreach (station; 0 .. 15) {
                double target = low+(.15+.7*station/14)*(high-low);
                double[] cross;
                foreach (i, p; cloud) if (abs(t[i]-target)<=max(1.,(high-low)*.025))
                    cross ~= p[0]*axis[1]-p[1]*axis[0];
                if (cross.length) widths ~= quantile(cross,1)-quantile(cross,0);
            }
            radii[family ~ ":" ~ side.toLower] = JSONValue(max(1.,quantile(widths,.5)/2));
        }
    }
    auto pelvis = midpoint(ngRigPoint(landmarks["hip.L"]["xy"]),ngRigPoint(landmarks["hip.R"]["xy"]));
    put("pelvis",pelvis,"prior");
    auto torsoAxis = minus(pelvis,neckBase);
    double lengthSquared = torsoAxis[0]*torsoAxis[0]+torsoAxis[1]*torsoAxis[1];
    enforce(lengthSquared>0,"Degenerate observed torso axis");
    auto chest = ngRigMeasuredSection(torso,.4), waist = ngRigMeasuredSection(torso,.9);
    double chestFraction = ((chest[0]-neckBase[0])*torsoAxis[0]+(chest[1]-neckBase[1])*torsoAxis[1])/lengthSquared;
    double waistFraction = ((waist[0]-neckBase[0])*torsoAxis[0]+(waist[1]-neckBase[1])*torsoAxis[1])/lengthSquared;
    bool skinTorso;
    foreach (material; materials) if (!material["static"].boolean && material["role"].str == "torso") skinTorso = true;
    double gap = ngRigNumber(ngRigHumanoidPrior()["torso_axis"]["minimum_station_gap"]);
    bool measuredStations = skinTorso && chestFraction>gap && waistFraction-chestFraction>gap && 1-waistFraction>gap;
    if (!measuredStations) { chestFraction = .35; waistFraction = .75; }
    put("chest",[neckBase[0]+chestFraction*torsoAxis[0],neckBase[1]+chestFraction*torsoAxis[1]],"prior");
    put("waist",[neckBase[0]+waistFraction*torsoAxis[0],neckBase[1]+waistFraction*torsoAxis[1]],"prior");
    foreach (role; ["chest","waist"]) landmarks[role]["method"] = JSONValue(measuredStations ?
        "observed garment station projected onto the shared anatomical torso axis" :
        "common ordered torso stations; garment sections fall outside anatomical station constraints");
    auto faceLow = Point2.init, faceHigh = Point2.init;
    faceLow = [quantile(faceX,.01),quantile(faceY,.01)]; faceHigh = [quantile(faceX,.99),quantile(faceY,.99)];
    double shoulderWidth = norm(minus(ngRigPoint(landmarks["shoulder.L"]["xy"]),ngRigPoint(landmarks["shoulder.R"]["xy"])));
    JSONValue[string] volumes;
    volumes["head"] = JSONValue(["center":JSONValue(midpoint(faceLow,faceHigh)[]),
        "radii":JSONValue([(faceHigh[0]-faceLow[0])/2,(faceHigh[1]-faceLow[1])/2])]);
    volumes["torso"] = JSONValue(["center":JSONValue(midpoint(neckBase,pelvis)[]),
        "radii":JSONValue([shoulderWidth*.45,norm(torsoAxis)/2])]);
    JSONValue[] identity = [JSONValue([1.,0.,0.]),JSONValue([0.,1.,0.]),JSONValue([0.,0.,1.])];
    auto result = JSONValue(["kind":JSONValue("humanoid"),"landmarks":JSONValue(landmarks),
        "neck_inference":neckInference, "head_frame":headFrame,
        "volumes":JSONValue(volumes), "limb_radii":JSONValue(radii),"source_to_model":JSONValue(identity),
        "observation_sha256":JSONValue(ngRigDigest(observation)),"source_sha256":observation["source_sha256"]]);
    foreach (family; ["arm","leg"]) result["limb_radii"][family ~ ":both"] = JSONValue(
        (ngRigNumber(radii[family ~ ":r"])+ngRigNumber(radii[family ~ ":l"]))/2);
    if (auto overrides = "landmarks" in observation["options"].object)
        foreach (role, point; overrides.object) {
            enforce((role in landmarks) !is null, "Unknown anatomical landmark override: " ~ role);
            auto p = ngRigPoint(point);
            result["landmarks"][role] = JSONValue(["xy":JSONValue(p[]),"provenance":JSONValue("visual_estimate"),
                "weight":JSONValue(1.)]);
        }
    return result;
}
