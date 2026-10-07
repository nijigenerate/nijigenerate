module nijigenerate.autorig.deterministic.hierarchy;

import nijigenerate.autorig.deterministic.contracts;
import std.json : JSONValue, JSONType;
import std.algorithm : canFind, sort;
import std.array : array;
import std.exception : enforce;
import std.conv : to;
import std.string : startsWith;

/** Compile semantic material origins independently of the imported source tree. */
JSONValue ngRigCompileHierarchy(JSONValue observation, JSONValue[] carriers, JSONValue scaffold) {
    JSONValue[ulong] nodes;
    ulong root = ngRigUnsigned(observation["rootId"]);
    nodes[root] = JSONValue(["uuid":observation["rootId"],"parent":JSONValue.init,"type":JSONValue("Node")]);
    foreach (node; ngRigGet(observation,"hierarchy_nodes",JSONValue(cast(JSONValue[])null)).array)
        nodes[ngRigUnsigned(node["uuid"])] = node;
    foreach (node; ngRigGet(observation,"groups",JSONValue(cast(JSONValue[])null)).array)
        if ((ngRigUnsigned(node["uuid"]) in nodes) is null) {
            auto copy = JSONValue(node.object.dup); copy["type"] = JSONValue("DynamicComposite");
            nodes[ngRigUnsigned(node["uuid"])] = copy;
        }
    JSONValue[ulong] materials;
    foreach (order,material; observation["materials"].array) {
        ulong id = ngRigUnsigned(material["uuid"]); materials[id] = material;
        if ((id in nodes) is null) nodes[id] = JSONValue(["uuid":material["uuid"],
            "parent":ngRigGet(material,"parent",observation["rootId"]),"type":JSONValue("Part"),
            "source_order":JSONValue(order)]);
    }
    ulong[][ulong] children;
    foreach (id,node; nodes) if (node["parent"].type != JSONType.null_)
        children[ngRigUnsigned(node["parent"])] ~= id;
    ulong[] chain(ulong id) {
        ulong[] result;
        while (true) {
            enforce(!result.canFind(id),"Cyclic source hierarchy"); result ~= id;
            auto node = id in nodes; enforce(node !is null,"Missing source ancestor");
            if ((*node)["parent"].type == JSONType.null_) return result;
            id = ngRigUnsigned((*node)["parent"]);
        }
    }
    string[ulong] assigned;
    ulong[][string] domains;
    JSONValue[string] definitions;
    foreach (carrier; carriers) {
        ulong id = ngRigUnsigned(carrier["part"]); auto domain = carrier["domain_id"].str;
        assigned[id] = domain; domains[domain] ~= id; definitions[domain] = carrier;
    }
    auto first = chain(ngRigUnsigned(carriers[0]["part"])); ulong scopeId; bool found;
    foreach (candidate; first[1 .. $]) {
        bool common = true;
        foreach (id,domain; assigned) if (!chain(id)[1 .. $].canFind(candidate)) common = false;
        if (common) { scopeId = candidate; found = true; break; }
    }
    enforce(found,"No common material render scope");
    ulong[][ulong] descendants;
    ulong[] partsBelow(ulong id) {
        if (auto cached = id in descendants) return *cached;
        ulong[] result;
        if (nodes[id]["type"].str == "Part") result ~= id;
        if (auto list = id in children) foreach (child; *list) result ~= partsBelow(child);
        descendants[id] = result; return result;
    }
    ulong[ulong] receivers;
    foreach (id,material; materials) if (auto receiver = "receiver" in material.object)
        receivers[id] = ngRigUnsigned(*receiver);
    bool[ulong] clipScopes;
    foreach (id,node; nodes) {
        auto type = node["type"].str;
        if (type != "Composite" && type != "DynamicComposite") continue;
        if (ngRigString(node,"blend_mode","Normal") != "Normal" || ngRigScalar(node,"opacity",1) != 1) continue;
        auto parts = partsBelow(id); ulong[] bases;
        foreach (part; parts) if ((part in receivers) is null) bases ~= part;
        if (parts.length<=1 || bases.length != 1) continue;
        bool clipped = true;
        foreach (part; parts) if (part != bases[0])
            if ((part in receivers) is null || receivers[part] != bases[0]) clipped = false;
        if (clipped) clipScopes[id] = true;
    }
    ulong anchor(string domain) {
        auto ids = domains[domain]; ulong[] candidates;
        foreach (id; ids) if ((id in receivers) is null) candidates ~= id;
        enforce(candidates.length>0,"Material domain has no unclipped origin: " ~ domain);
        if (definitions[domain]["owner"].str == "head" && definitions[domain]["chart"].str == "face") {
            ulong[] face;
            foreach (id; candidates) if (materials[id]["role"].str == "face") face ~= id;
            if (face.length) candidates = face;
        }
        double best = -1; ulong selected;
        foreach (id; candidates) {
            auto b = ngRigNumbers(ngRigGet(materials[id],"bounds",definitions[domain]["bounds"]));
            double area = (b[2]-b[0])*(b[3]-b[1]);
            if (area>best) { best = area; selected = id; }
        }
        return selected;
    }
    auto domainIds = domains.keys.sort.array;
    string body;
    foreach (id; domainIds) if (definitions[id]["owner"].str == "torso" && definitions[id]["chart"].str == "body") body = id;
    if (!body.length) foreach (id; domainIds) {
        auto definition = definitions[id]; auto chart = definition["chart"].str;
        if (definition["owner"].str == "torso" && (chart == "topwear_front" || chart == "topwear_waist"))
            if (!body.length || domains[id].length>domains[body].length) body = id;
    }
    enforce(body.length>0,"No torso origin material");
    ulong bodyOrigin = anchor(body);
    JSONValue[string] bones; foreach (bone; scaffold["bones"].array) bones[bone["id"].str] = bone;
    JSONValue[] groups;
    void group(string id, JSONValue parent, string bone) {
        groups ~= JSONValue(["id":JSONValue(id),"parent":parent,"bone":JSONValue(bone),
            "origin":JSONValue(bones[bone]["head"].array[0 .. 2])]);
    }
    group("Body::Root",JSONValue(["node":JSONValue(scopeId)]),"Pelvis");
    group("Head::Root",JSONValue(["node":JSONValue(bodyOrigin)]),"Head");
    foreach (family; ["arm","leg"]) foreach (side; ["L","R"]) {
        bool exists;
        foreach (id; domainIds) if (definitions[id]["owner"].str == family ~ ":" ~ (side == "R" ? "r" : "l")) exists = true;
        if (exists) group((family == "arm" ? "Arm" : "Leg") ~ "::Root::" ~ side,
            JSONValue(["node":JSONValue(family == "arm" ? bodyOrigin : scopeId)]),
            (family == "arm" ? "UpperArm." : "Thigh.") ~ side);
    }
    JSONValue[string] parents, units, origins;
    foreach (id; domainIds) {
        auto definition = definitions[id]; auto owner = definition["owner"].str, chart = definition["chart"].str;
        JSONValue parent;
        if (id == body) parent = JSONValue(["group":JSONValue("Body::Root")]);
        else if (owner == "head") parent = JSONValue(["group":JSONValue("Head::Root")]);
        else if (owner == "torso") parent = JSONValue(["node":JSONValue(bodyOrigin)]);
        else {
            auto skin = owner ~ "/skin@semantic";
            if (chart != "skin" && (skin in domains) !is null) parent = JSONValue(["node":JSONValue(anchor(skin))]);
            else if (definition["side"].str == "Both") parent = JSONValue(["node":JSONValue(owner.startsWith("arm:") ? bodyOrigin : scopeId)]);
            else parent = JSONValue(["group":JSONValue((owner.startsWith("arm:") ? "Arm" : "Leg") ~ "::Root::" ~ definition["side"].str)]);
        }
        parents[id] = parent; origins[id] = JSONValue(anchor(id));
        ulong[] topUnits;
        foreach (part; domains[id]) {
            ulong top = part;
            auto ancestry = chain(part);
            foreach (candidate; ancestry[1 .. $]) {
                if (candidate == scopeId || (candidate in clipScopes) !is null) break;
                auto parts = partsBelow(candidate); bool complete = parts.length>0;
                foreach (descendant; parts) if (!domains[id].canFind(descendant)) complete = false;
                if (!complete) break;
                top = candidate;
            }
            if (!topUnits.canFind(top)) topUnits ~= top;
        }
        topUnits.sort!((a,b)=>ngRigScalar(nodes[a],"source_order",0)<ngRigScalar(nodes[b],"source_order",0));
        units[id] = JSONValue(topUnits);
    }
    JSONValue faceOrigin;
    foreach (id; domainIds) if (definitions[id]["owner"].str == "head" && definitions[id]["chart"].str == "face")
        faceOrigin = JSONValue(anchor(id));
    JSONValue[string] clipping;
    foreach (part,receiver; receivers)
        if ((part in assigned) !is null && (receiver in assigned) !is null && assigned[part] == assigned[receiver])
            clipping[part.to!string] = JSONValue(receiver);
    return JSONValue(["render_scope":JSONValue(scopeId),"body_domain":JSONValue(body),"face_origin":faceOrigin,
        "clipping_receivers":JSONValue(clipping),
        "body_origin":JSONValue(bodyOrigin),"groups":JSONValue(groups),"surface_parents":JSONValue(parents),
        "render_units":JSONValue(units),"origin_parts":JSONValue(origins)]);
}
