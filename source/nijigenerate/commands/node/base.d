module nijigenerate.commands.node.base;

import nijigenerate.commands.base;

import nijigenerate.viewport.vertex;
import nijigenerate.viewport.common.mesh : applyMeshToTarget, IncMesh;
import nijilive.core.meshdata : MeshData;
import nijigenerate.widgets.dragdrop;
import nijigenerate.actions;
import nijigenerate.core.actionstack;
import nijigenerate.panels;
import nijigenerate.ext;
import nijigenerate.utils.transform;
import nijigenerate;
import nijigenerate.widgets;
import nijigenerate.ext;
import nijigenerate.core;
import nijigenerate.core.input;
import nijigenerate.utils;
import nijilive;
import nijilive.core.meshdata : MeshData;
import std.algorithm;
import std.string;
import std.array;
import std.format;
import std.conv;
import std.utf;
import i18n;


    Node[] clipboardNodes;

    void copyToClipboard(Node[] nodes) {
        clipboardNodes.length = 0;
        foreach (node; nodes) {
            auto newNode = node.dup;
            clipboardNodes ~= newNode;
        }
    }

    void pasteFromClipboard(Node parent) {
        if (parent !is null) {
            incActionPush(new NodeMoveAction(clipboardNodes, parent, 0));
            foreach (node; clipboardNodes) {
                incReloadNode([node]);
            }
            clipboardNodes.length = 0;
        }
    }

    string[][string] _conversionMap;
    string[][string] conversionMap() {
        if (_conversionMap.length == 0) {
            _conversionMap = [
                "Node": ["MeshGroup", "DynamicComposite"],
                "DynamicComposite": ["MeshGroup", "Node", "Part", "Composite", "GridDeformer"],
                "MeshGroup": ["DynamicComposite", "Node", "GridDeformer"],
                "GridDeformer": ["MeshGroup", "DynamicComposite"],
                "Composite": ["DynamicComposite", "Node"]
            ];
        }
        return _conversionMap;
    }

    void insertNodesAux(Node[] parents, Node[] children, string className, string suffixName) {
        if (children && parents.length != children.length)
            children = null;
        incActionPushGroup();
        foreach (i, p; parents) {
            Node newChild = inInstantiateNode(className, null);
            string nodeName = null;
            if (suffixName && suffixName.length != 0) {
                if (children)
                    nodeName = children[i].name ~ suffixName;
                else
                    nodeName = p.name ~ suffixName;
            }
            incAddChildWithHistory(newChild, p, nodeName);
            if (children) {
                newChild.localTransform.translation = children[i].localTransform.translation;
                newChild.transformChanged();
                incActionPush(new NodeMoveAction([children[i]], newChild));
            }
        }
        incActionPopGroup();
    }


void incReloadNode(Node[] nodes) {
    foreach (node; nodes) {
        incReloadNode(node.children);
        foreach (child; node.children) {
            child.notifyChange(child);
        }
        node.clearCache();
    }
}

void ngAddNodes(Node[] parents, string className, string _suffixName = null) {
    insertNodesAux(parents, null, className, _suffixName);
}

void ngInsertNodes(Node[] children, string className, string _suffixName = null) {
    insertNodesAux(children.map!((v)=>v.parent).array, children, className, _suffixName);
}

string ngGetCommonNodeType(Node[] nodes) {
    string type = null;
    foreach (node; nodes) {
        if (!type) { 
            type = node.typeId; 
        } else if (type != node.typeId) {
            return null;
        }
    }
    return type;
}

bool ngCanConvertTo(Node[] nodes, string toType) {
    import std.algorithm : canFind;
    import nijigenerate.viewport.common.mesh : IncMesh, isGrid;

    if (!toType || nodes.length == 0) return false;
    string fromType = ngGetCommonNodeType(nodes);
    if (!fromType.length) return false;

    auto map = conversionMap();
    auto ptr = fromType in map;
    if (ptr is null) return false;
    if (!(*ptr).canFind(toType)) return false;

    if (toType == "GridDeformer") {
        foreach (node; nodes) {
            if (!(cast(MeshGroup)node || cast(DynamicComposite)node)) {
                return false;
            }

            Drawable drawable = cast(Drawable)node;
            if (drawable is null) {
                return false;
            }

            auto meshWrapper = new IncMesh(drawable.getMesh());
            float[][] axes;
            if (!meshWrapper.vertices.isGrid(axes)) {
                return false;
            }
        }
    }

    return true;
}

void ngConvertTo(Node[] nodes, string toType) {
    if (!ngCanConvertTo(nodes, toType)) return;

    auto group = new GroupAction();
    Node[] newNodes = [];
    foreach (node; nodes.dup) {
        Node newNode = inInstantiateNode(toType);
        newNode.copyFrom(node, true, false);
        group.addAction(new NodeReplaceAction(node, newNode, true));
        ngApplySourceGeometry(node, newNode);
        newNodes ~= newNode;
        newNode.notifyChange(newNode, NotifyReason.StructureChanged);
    }
    incActionPush(group);
    incSelectNodes(newNodes);
}

void ngApplySourceGeometry(Node source, Node target) {
    Deformable srcDef = cast(Deformable)source;
    Deformable dstDef = cast(Deformable)target;
    if (srcDef is null || dstDef is null) {
        return;
    }

    vec2[] baseVertices = srcDef.vertices.dup;
    MeshData meshCopy;
    MeshData* meshPtr = null;

    if (auto srcDrawable = cast(Drawable)srcDef) {
        meshCopy = srcDrawable.getMesh().copy();
        meshPtr = &meshCopy;
        baseVertices = meshCopy.vertices.dup;
    }

    if (baseVertices.length == 0) return;

    applyMeshToTarget(dstDef, baseVertices, meshPtr);

    auto dstBase = dstDef.vertices;
    vec2[] actualVertices = ngCollectActualVertices(srcDef);
    ngMapOffsets(dstBase, actualVertices, dstDef);
}

vec2[] ngCollectBaseVertices(Deformable def) {
    vec2[] result = def.vertices.dup;
    return result;
}

vec2[] ngCollectActualVertices(Deformable def) {
    vec2[] result = def.vertices.dup;
    size_t count = result.length;
    size_t limit = def.deformation.length;
    if (limit > count) limit = count;
    foreach (i; 0 .. limit) {
        result[i] += def.deformation[i];
    }
    return result;
}

void ngMapOffsets(const(vec2)[] base, const(vec2)[] actual, Deformable target) {
    target.deformation.length = base.length;
    if (base.length == 0) return;

    const float tolerance = 1e-4;
    const float tol2 = tolerance * tolerance;
    bool[] used;
    used.length = actual.length;

    foreach (i, basePos; base) {
        size_t bestIdx = size_t.max;
        float bestDist = float.max;
        foreach (j, actPos; actual) {
            if (used[j]) continue;
            float dx = actPos.x - basePos.x;
            float dy = actPos.y - basePos.y;
            float dist = dx*dx + dy*dy;
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = j;
            }
        }
        vec2 offset = vec2(0, 0);
        if (bestIdx != size_t.max && bestDist <= tol2) {
            offset = actual[bestIdx] - basePos;
            used[bestIdx] = true;
        }
        target.deformation[i] = offset;
    }
    target.updateDeform();
}
