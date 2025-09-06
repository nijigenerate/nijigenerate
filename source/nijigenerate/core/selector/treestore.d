module nijigenerate.core.selector.treestore;

import std.algorithm : sort;
import std.array : array;
import nijigenerate.core.selector.resource : Resource;

// Lightweight, UI-agnostic tree builder for selector Resources.
// Builds parent-child relationships based on Resource.source and explicit flags.
//
// Template parameter `addNonIncluded` controls whether ancestors that are not
// part of the initial `nodes` set are added to the resulting forest:
// - true  (default): current behavior, climb ancestors and include them as
//                    parents/roots when needed.
// - false           : restrict relationships strictly within `nodes` (results);
//                    parents are linked only if also included in `nodes`.
class TreeStore_(bool addNonIncluded = true) {
public:
    Resource[] nodes;
    Resource[][Resource] children;
    Resource[] roots;
    bool[Resource] nodeIncluded;

    void reset() {
        children.clear();
        nodes.length = 0;
        nodeIncluded.clear();
        roots.length = 0;
    }

    void setResources(Resource[] nodes_) {
        nodes = nodes_;
        roots.length = 0;
        children.clear();
        nodeIncluded.clear();
        Resource[Resource] parentMap;
        bool[Resource] rootMap;
        bool[Resource][Resource] childMap;
        foreach (n; nodes) {
            nodeIncluded[n] = true;
        }

        void addToMap(Resource res, int level = 0) {
            if (res in parentMap) return;
            static if (addNonIncluded) {
                auto source = res.source;
                while (source) {
                    if (source.source is null) break;
                    if (source in nodeIncluded || source.explicit) break;
                    source = source.source;
                }
                if (source) {
                    parentMap[res] = source;
                    childMap.require(source);
                    childMap[source][res] = true;
                    // Recurse so that included/non-included ancestors are linked and
                    // roots can be determined.
                    addToMap(source, level + 1);
                } else {
                    rootMap[res] = true;
                }
            } else {
                // Restrict links to the provided `nodes` only. Find the nearest
                // ancestor that is also included; otherwise mark as root.
                auto source = res.source;
                Resource parentInSet = null;
                while (source) {
                    if (source in nodeIncluded) { parentInSet = source; break; }
                    source = source.source;
                }
                if (parentInSet !is null) {
                    parentMap[res] = parentInSet;
                    childMap.require(parentInSet);
                    childMap[parentInSet][res] = true;
                    // Do NOT recurse into parent when it's already part of nodes;
                    // its mapping will be processed in its own addToMap call.
                } else {
                    rootMap[res] = true;
                }
            }
        }

        foreach (res; nodes) {
            addToMap(res);
        }
        static if (addNonIncluded) {
            foreach (res; childMap.keys) {
                if (res !in parentMap || parentMap[res] is null) {
                    rootMap[res] = true;
                }
            }
        }
        roots = rootMap.keys.sort!((a,b)=>a.index<b.index).array;
        foreach (item; childMap.byKeyValue) {
            children[item.key] = item.value.keys.sort!((a,b)=>a.index<b.index).array;
        }
    }
}

// Backward-compatible alias: default behavior includes non-included ancestors.
alias TreeStore = TreeStore_!();
