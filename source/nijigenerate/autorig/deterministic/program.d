module nijigenerate.autorig.deterministic.program;

import nijigenerate.autorig.deterministic.contracts;
import nijigenerate.autorig.deterministic.templates;
import nijigenerate.autorig.deterministic.anatomy;
import nijigenerate.autorig.deterministic.hierarchy;
import nijigenerate.autorig.deterministic.registered : ngRigRegisterProgram;
import std.json : JSONValue;
import std.exception : enforce;
import std.algorithm : clamp, min, max, sort, canFind;
import std.array : array;
import std.math : ceil, sqrt, abs, PI, rint;
import std.conv : to;
import std.string : replace, startsWith, toLower;

JSONValue ngRigCompileProgram(JSONValue observation, JSONValue evidence) {
    auto rigKind = ngRigString(evidence,"kind","humanoid");
    if (rigKind != "humanoid") {
        JSONValue[] targets;
        double[] sourceXs;
        foreach (material; observation["materials"].array) if (!material["static"].boolean && material["role"].str == "face")
            foreach (p; ngRigPoints(material["cloud"])) sourceXs ~= p[0];
        if (!sourceXs.length) foreach (material; observation["materials"].array) if (!material["static"].boolean)
            foreach (p; ngRigPoints(material["cloud"])) sourceXs ~= p[0];
        sourceXs.sort; double center = (sourceXs[sourceXs.length/2]+sourceXs[(sourceXs.length-1)/2])/2;
        foreach (material; observation["materials"].array) if (!material["static"].boolean) {
            auto cloud = ngRigPoints(material["cloud"]);
            double[] xs; foreach (point; cloud) xs ~= point[0]; xs.sort;
            double position = (xs[xs.length/2]+xs[(xs.length-1)/2])/2;
            targets ~= JSONValue(["part":material["uuid"],"path":material["path"],
                "owner":material["owner"],"chart":material["chart"],
                "role":material["role"],"side":JSONValue(position<center ? "R" : "L")]);
        }
        enforce(targets.length>0,"No active local rig materials");
        auto result = JSONValue(["schema_version":JSONValue("rig-native-program-d/1"),
            "kind":JSONValue(rigKind),"carriers":JSONValue(targets),"rootId":observation["rootId"],
            "source_sha256":observation["source_sha256"],"observation_sha256":JSONValue(ngRigDigest(observation)),
            "evidence_sha256":JSONValue(ngRigDigest(evidence))]);
        result["content_sha256"] = JSONValue(ngRigDigest(result));
        return result;
    }
    auto prior = ngRigHumanoidPrior();
    auto scaffold = ngRigSolveScaffold(evidence,prior);
    double height = ngRigNumber(scaffold["body_height"]), padding = height*ngRigNumber(prior["bounds_padding_body_height"]);
    double step = height*ngRigNumber(prior["grid_step_body_height"]);
    double[] faceXs;
    foreach (material; observation["materials"].array) if (!material["static"].boolean && material["role"].str == "face")
        foreach (p; ngRigPoints(material["cloud"])) faceXs ~= p[0];
    faceXs.sort;
    double center = faceXs.length ? (faceXs[faceXs.length/2]+faceXs[(faceXs.length-1)/2])/2 :
        ngRigPoint(scaffold["landmarks"]["head_root"])[0];
    JSONValue[string] domains;
    string[ulong] materialDomains;
    string[ulong] materialSides;
    JSONValue[ulong] activeMaterials;
    foreach (material; observation["materials"].array) if (!material["static"].boolean)
        activeMaterials[ngRigUnsigned(material["uuid"])] = material;
    JSONValue groupingSource(JSONValue material, ulong[] visited = null) {
        auto id = ngRigUnsigned(material["uuid"]);
        enforce(!visited.canFind(id), "Cyclic clipping receiver in material grouping");
        if (auto receiver = "receiver" in material.object) {
            auto source = ngRigUnsigned(*receiver) in activeMaterials;
            enforce(source !is null, "Clipping receiver is outside the active material set");
            return groupingSource(*source, visited ~ id);
        }
        return material;
    }
    foreach (material; observation["materials"].array) if (!material["static"].boolean) {
        // Match Python's recursive receiver-based grouping, including explicit roles.
        auto grouping = groupingSource(material);
        auto cloud = ngRigPoints(grouping["cloud"]);
        double[] sourceXs; foreach (p; cloud) sourceXs ~= p[0]; sourceXs.sort;
        double position = (sourceXs[sourceXs.length/2]+sourceXs[(sourceXs.length-1)/2])/2;
        string side = position<center ? "R" : "L";
        auto role = grouping["role"].str;
        if (role == "arm" || role == "hand" || role == "leg" || role == "foot" || role == "sleeve") {
            size_t left;
            foreach (p; cloud) if (p[0]<center) ++left;
            double fraction = cast(double)left/cloud.length;
            if (fraction>.15 && fraction<.85) side = "Both";
        }
        auto owner = grouping["owner"].str.replace("{side}",side.toLower);
        auto chart = grouping["chart"].str.replace("{side}",side.toLower);
        if (owner == "torso" && chart == "neck") chart = "body";
        if ((owner.startsWith("arm:") || owner.startsWith("leg:")) && (chart == "hand" || chart == "foot"))
            chart = "skin";
        auto parent = observation["rootId"];
        string id = owner ~ "/" ~ chart ~ "@semantic";
        materialDomains[ngRigUnsigned(material["uuid"])] = id; materialSides[ngRigUnsigned(material["uuid"])] = side;
        double[] matrix = [1.,0.,0.,0.,1.,0.];
        double[] bounds = [double.infinity,double.infinity,-double.infinity,-double.infinity];
        foreach (p; cloud) {
            double x = p[0], y = p[1];
            bounds[0] = min(bounds[0],x); bounds[1] = min(bounds[1],y);
            bounds[2] = max(bounds[2],x); bounds[3] = max(bounds[3],y);
        }
        if (auto nominal = "bounds" in material.object) bounds = ngRigNumbers(*nominal);
        if (auto existing = id in domains) {
            auto previous = ngRigNumbers((*existing)["local_bounds"]);
            bounds[0] = min(bounds[0],previous[0]); bounds[1] = min(bounds[1],previous[1]);
            bounds[2] = max(bounds[2],previous[2]); bounds[3] = max(bounds[3],previous[3]);
            (*existing)["local_bounds"] = JSONValue(bounds);
            (*existing)["parts"].array ~= material["uuid"];
        } else domains[id] = JSONValue(["id":JSONValue(id),"owner":JSONValue(owner),"chart":JSONValue(chart),
            "parent":parent,"parent_to_root":JSONValue(matrix),"local_bounds":JSONValue(bounds),
            "parts":JSONValue([material["uuid"]])]);
    }
    double[4] extent = [double.infinity,double.infinity,-double.infinity,-double.infinity];
    JSONValue[] carriers;
    foreach (material; observation["materials"].array) {
        if (material["static"].boolean) continue;
        auto role = material["role"].str;
        auto domainId = materialDomains[ngRigUnsigned(material["uuid"])]; auto assembly = domains[domainId];
        auto owner = assembly["owner"].str; auto side = materialSides[ngRigUnsigned(material["uuid"])];
        auto bounds = ngRigNumbers(assembly["local_bounds"]);
        auto matrix = ngRigNumbers(assembly["parent_to_root"]);
        double xScale = sqrt(matrix[0]^^2+matrix[3]^^2), yScale = sqrt(matrix[1]^^2+matrix[4]^^2);
        enforce(bounds.length == 4 && bounds[2]>bounds[0] && bounds[3]>bounds[1], "Invalid material bounds");
        bounds[0] -= padding/xScale; bounds[1] -= padding/yScale;
        bounds[2] += padding/xScale; bounds[3] += padding/yScale;
        size_t nx = cast(size_t)clamp(ceil((bounds[2]-bounds[0])*xScale/step),
            ngRigNumber(prior["minimum_grid_segments"]),ngRigNumber(prior["maximum_grid_segments"]));
        size_t ny = cast(size_t)clamp(ceil((bounds[3]-bounds[1])*yScale/step),
            ngRigNumber(prior["minimum_grid_segments"]),ngRigNumber(prior["maximum_grid_segments"]));
        double[] xs, ys; Point2[] points;
        foreach (i; 0 .. nx+1) xs ~= rint((bounds[0]+(bounds[2]-bounds[0])*i/nx)*1e4)/1e4;
        foreach (i; 0 .. ny+1) ys ~= rint((bounds[1]+(bounds[3]-bounds[1])*i/ny)*1e4)/1e4;
        double[] rootBounds = [double.infinity,double.infinity,-double.infinity,-double.infinity];
        foreach (y; ys) foreach (x; xs) {
            Point2 p = [matrix[0]*x+matrix[1]*y+matrix[2],matrix[3]*x+matrix[4]*y+matrix[5]]; points ~= p;
            rootBounds[0] = min(rootBounds[0],p[0]); rootBounds[1] = min(rootBounds[1],p[1]);
            rootBounds[2] = max(rootBounds[2],p[0]); rootBounds[3] = max(rootBounds[3],p[1]);
            extent[0] = min(extent[0],p[0]); extent[1] = min(extent[1],p[1]);
            extent[2] = max(extent[2],p[0]); extent[3] = max(extent[3],p[1]);
        }
        auto chart = assembly["chart"].str;
        string kind = "surface";
        double offset = 0;
        string[] bones;
        if (owner == "head") {
            bones = ["Head"];
            if (chart == "hair:back") { kind = "back_hair"; offset = height*.003; }
            else if (chart.startsWith("hair") || chart.startsWith("headwear") || chart.startsWith("ear"))
                offset = height*.004;
        } else if (owner == "torso") {
            bones = ["Pelvis","Spine","Chest","Neck"];
            if (chart == "skirt" || chart == "skirt_back" || chart == "apron") {
                kind = chart == "skirt_back" ? "skirt_back" : "skirt_front"; bones = ["Pelvis"];
                if (chart == "apron") offset = height*.003;
            } else if (chart == "tail") { kind = "appendage"; bones = ["Pelvis"]; offset = -height*.025; }
            else if (chart == "neck") { kind = "neck"; bones = ["Neck"]; }
            else if (chart == "attachment:chest") bones = ["Chest"];
            else if (chart == "attachment:pelvis") bones = ["Pelvis"];
        } else if (owner.startsWith("arm:") || owner.startsWith("leg:")) {
            string[] names = owner.startsWith("arm:") ? ["UpperArm","Forearm","Hand"] : ["Thigh","Shin","Foot"];
            if (chart == "hand") names = ["Hand"];
            if (chart == "foot" || chart == "attachment:ankle") names = ["Foot"];
            if (chart == "attachment:thigh") names = ["Thigh"];
            foreach (tag; side == "Both" ? ["R","L"] : [side]) foreach (name; names) bones ~= name ~ "." ~ tag;
            if (chart == "sleeve" || chart == "shoulder") offset = ngRigNumber(evidence["limb_radii"][owner])*.2;
        } else throw new Exception("Unsupported anatomical material owner: " ~ owner);
        JSONValue[string] domain = ["owner":JSONValue(owner),"kind":JSONValue(kind),"side":JSONValue(side),
            "support_bounds":JSONValue(rootBounds),"offset":JSONValue(offset)];
        if (owner.startsWith("arm:") || owner.startsWith("leg:"))
            domain["radius"] = evidence["limb_radii"][owner];
        double[] depths;
        if (side == "Both") { depths = new double[points.length]; depths[] = 0; }
        else depths = ngRigDepthField(points,JSONValue(domain),scaffold,prior);
        carriers ~= JSONValue(["part":material["uuid"],"path":material["path"],"role":material["role"],
            "chart":assembly["chart"],"unit":ngRigGet(material,"carrier_unit",material["uuid"]),
            "owner":JSONValue(owner),"side":JSONValue(side),"bounds":JSONValue(rootBounds),"xs":JSONValue(xs),
            "domain_id":JSONValue(domainId),"parent":assembly["parent"],"parent_to_root":assembly["parent_to_root"],
            "ys":JSONValue(ys),"points":ngRigPointsJson(points),"depth":JSONValue(depths),
            "bones":JSONValue(bones),"domain":JSONValue(domain)]);
    }
    enforce(carriers.length>0, "No active rig materials");
    double depthScale = max(1.,max(extent[2]-extent[0],extent[3]-extent[1])*.42/2.9);
    foreach (ref carrier; carriers) {
        auto depths = ngRigNumbers(carrier["depth"]);
        foreach (ref depth; depths) depth = rint(depth/depthScale*1e6)/1e6;
        carrier["depth"] = JSONValue(depths);
    }
    JSONValue[] assembled;
    foreach (id; domains.keys.sort.array) assembled ~= domains[id];
    JSONValue[] drivers;
    void driver(string parameter, string bone, string binding, string priorKey, bool vertical, bool twoDimensional) {
        JSONValue[] values; auto magnitude = ngRigNumber(prior["drivers"][priorKey])*PI/180;
        foreach (x; [-1.,-.5,0.,.5,1.]) foreach (y; twoDimensional ? [-1.,-.5,0.,.5,1.] : [0.])
            values ~= JSONValue(["key":JSONValue([x,y]),"value":JSONValue((vertical ? y : x)*magnitude)]);
        drivers ~= JSONValue(["parameter":JSONValue(parameter),"bone":JSONValue(bone),
            "binding":JSONValue(binding),"values":JSONValue(values)]);
    }
    driver("Face::Yaw-Pitch","Head","transform.r.y","head_yaw_degrees",false,true);
    driver("Face::Yaw-Pitch","Head","transform.r.x","head_pitch_degrees",true,true);
    driver("Face::Roll","Neck","transform.r.z","head_roll_degrees",false,false);
    driver("Body::Yaw-Pitch","Spine","transform.r.y","body_yaw_degrees",false,true);
    driver("Body::Yaw-Pitch","Spine","transform.r.x","body_pitch_degrees",true,true);
    driver("Body::Roll","Pelvis","transform.r.z","body_roll_degrees",false,false);
    auto program = JSONValue(["schema_version":JSONValue("rig-native-program-d/1"),"scaffold":scaffold,
        "kind":JSONValue(rigKind),
        "native_depth_scale":JSONValue(depthScale),
        "domains":JSONValue(assembled),
        "native_drivers":JSONValue(drivers),
        "carriers":JSONValue(carriers),"rootId":observation["rootId"],
        "source_sha256":observation["source_sha256"],"observation_sha256":JSONValue(ngRigDigest(observation)),
        "evidence_sha256":JSONValue(ngRigDigest(evidence))]);
    program = ngRigRegisterProgram(program,observation);
    program["hierarchy"] = ngRigCompileHierarchy(observation,program["carriers"].array,program["scaffold"]);
    program["content_sha256"] = JSONValue(ngRigDigest(program));
    return program;
}
