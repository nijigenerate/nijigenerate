module nijigenerate.commands.node.base;

import nijigenerate.commands.base;

import nijigenerate.viewport.vertex;
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
        if (auto remap = ngApplySourceGeometry(node, newNode)) {
            group.addAction(remap);
        }
        newNodes ~= newNode;
        newNode.notifyChange(newNode, NotifyReason.StructureChanged);
    }
    incActionPush(group);
    incSelectNodes(newNodes);
}

GroupAction ngApplySourceGeometry(Node source, Node target) {
    Deformable srcDef = cast(Deformable)source;
    Deformable dstDef = cast(Deformable)target;
    if (srcDef is null || dstDef is null) {
        return null;
    }

    auto mapping = ngBuildVertexMapping(srcDef.vertices, dstDef.vertices);
    auto remapGroup = new GroupAction();

    if (dstDef.vertices.length > 0) {
        auto oldDeform = dstDef.deformation.dup;
        auto newDeform = ngComputeRemappedDeformation(srcDef, mapping, dstDef.vertices.length);
        remapGroup.addAction(new NodeValueChangeAction!(Deformable, vec2[])("deformation", dstDef, oldDeform, newDeform, &dstDef.deformation));

        dstDef.deformation = newDeform;
        dstDef.updateDeform();
        dstDef.notifyChange(dstDef, NotifyReason.StructureChanged);
    }

    auto puppet = incActivePuppet();
    if (puppet) {
        foreach (param; puppet.parameters) {
            if (auto group = cast(ExParameterGroup)param) {
                foreach(_, ref child; group.children) {
                    ngApplyBindingRemap(remapGroup, child, cast(DeformationParameterBinding)child.getBinding(dstDef, "deform"), mapping);
                }
            } else {
                ngApplyBindingRemap(remapGroup, param, cast(DeformationParameterBinding)param.getBinding(dstDef, "deform"), mapping);
            }
        }
    }

    return remapGroup.empty() ? null : remapGroup;
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

size_t[] ngBuildVertexMapping(const(vec2)[] srcVertices, const(vec2)[] dstVertices) {
    size_t[] mapping;
    mapping.length = dstVertices.length;
    bool[] used;
    used.length = srcVertices.length;

    foreach (i, dstPos; dstVertices) {
        size_t bestIdx = size_t.max;
        float bestDist = float.max;
        foreach (j, srcPos; srcVertices) {
            if (used[j]) continue;
            float dx = srcPos.x - dstPos.x;
            float dy = srcPos.y - dstPos.y;
            float dist = dx*dx + dy*dy;
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = j;
            }
        }
        if (bestIdx != size_t.max) {
            used[bestIdx] = true;
        }
        mapping[i] = bestIdx;
    }
    return mapping;
}

vec2[] ngComputeRemappedDeformation(Deformable srcDef, size_t[] mapping, size_t targetCount) {
    vec2[] newDeform;
    newDeform.length = targetCount;
    foreach (i, idx; mapping) {
        vec2 value = vec2(0, 0);
        if (idx != size_t.max && idx < srcDef.deformation.length) {
            value = srcDef.deformation[idx];
        }
        newDeform[i] = value;
    }
    return newDeform;
}

void ngApplyBindingRemap(GroupAction remapGroup, Parameter param, DeformationParameterBinding binding, size_t[] mapping) {
    if (!binding) return;

    Deformation[][] newValues;
    newValues.length = binding.values.length;
    bool anyChange = false;

    foreach (x; 0 .. binding.values.length) {
        newValues[x].length = binding.values[x].length;
        foreach (y; 0 .. binding.values[x].length) {
            auto current = binding.values[x][y];
            vec2[] remapped;
            remapped.length = mapping.length;
            foreach (i, idx; mapping) {
                vec2 value = vec2(0, 0);
                if (idx != size_t.max && idx < current.vertexOffsets.length) {
                    value = current.vertexOffsets[idx];
                }
                remapped[i] = value;
            }
            if (remapped != current.vertexOffsets) {
                anyChange = true;
            }
            newValues[x][y] = current;
            newValues[x][y].vertexOffsets = remapped;
        }
    }

    if (!anyChange) return;

    remapGroup.addAction(new ParameterChangeBindingsAction("Remap Deformation", param, [binding]));

    foreach (x; 0 .. binding.values.length) {
        foreach (y; 0 .. binding.values[x].length) {
            binding.values[x][y].vertexOffsets = newValues[x][y].vertexOffsets.dup;
        }
    }
    binding.reInterpolate();
    if (auto node = cast(Node)binding.getTarget().target) {
        node.notifyChange(node, NotifyReason.AttributeChanged);
    }
}
