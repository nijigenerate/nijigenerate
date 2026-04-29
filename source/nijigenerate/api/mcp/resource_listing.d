module nijigenerate.api.mcp.resource_listing;

import std.conv : to;
import std.json : JSONType, JSONValue;

import nijigenerate.project : incActivePuppet;
import nijilive;
import nijilive.core.nodes : Node;

private JSONValue resourceEntry(string uri, string name) {
    JSONValue[string] entry;
    entry["uri"] = JSONValue(uri);
    entry["name"] = JSONValue(name);
    return JSONValue(entry);
}

JSONValue buildCurrentResourceList() {
    JSONValue[] resources;

    resources ~= resourceEntry(
        "resource://nijigenerate/resources/find?selector=*",
        "Start Here: Explore Entire Model Tree"
    );
    resources ~= resourceEntry(
        "resource://nijigenerate/guides/resources",
        "Resources Guide"
    );
    resources ~= resourceEntry(
        "resource://nijigenerate/guides/selectors",
        "Selectors Guide"
    );
    resources ~= resourceEntry(
        "resource://nijigenerate/guides/find",
        "Find Guide"
    );

    auto puppet = incActivePuppet();
    if (puppet !is null) {
        bool[uint] seen;

        void addNodeResource(Node node) {
            if (node is null) return;
            if (node.uuid in seen) return;
            seen[node.uuid] = true;
            resources ~= resourceEntry(
                "resource://nijigenerate/resources/" ~ to!string(node.uuid),
                node.name
            );
            foreach (child; node.children) addNodeResource(child);
        }

        addNodeResource(puppet.root);

        foreach (param; puppet.parameters) {
            if (param is null) continue;
            if (param.uuid in seen) continue;
            seen[param.uuid] = true;
            resources ~= resourceEntry(
                "resource://nijigenerate/resources/" ~ to!string(param.uuid),
                param.name
            );
        }
    }

    return JSONValue(["resources": JSONValue(resources)]);
}

void rewriteResourcesListResponse(ref JSONValue response, ref JSONValue request) {
    if (request.type != JSONType.object || "method" !in request.object) return;
    if (request["method"].type != JSONType.string) return;
    if (request["method"].str != "resources/list") return;
    if (response.type != JSONType.object || "result" !in response.object) return;

    response["result"] = buildCurrentResourceList();
}
