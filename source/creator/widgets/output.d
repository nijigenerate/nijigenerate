module creator.widgets.output;

import std.array;
import std.string;
import std.algorithm;
import i18n;
import inochi2d;
import creator;
import creator.core;
import creator.core.selector;
import creator.panels;
import creator.utils;
import creator.widgets;
import creator.panels.shell;
import creator.panels.inspector;
import creator.panels.parameters;
import creator.ext;

private {
    string parameterGrabStr;

    void onNodeView(Node node) {
        if (node !is null && node != incActivePuppet().root) {

            // Per-edit mode inspector drawers
            switch(incEditMode()) {
                case EditMode.ModelEdit:
                    if (incArmedParameter()) {
                        Parameter param = incArmedParameter();
                        vec2u cursor = param.findClosestKeypoint();
                        incCommonNonEditHeader(node);
                        incInspectorDeformTRS(node, param, cursor);

                        // Node Part Section
                        if (Part part = cast(Part)node) {
                            incInspectorDeformPart(part, param, cursor);
                        }

                        if (Composite composite = cast(Composite)node) {
                            incInspectorDeformComposite(composite, param, cursor);
                        }

                        if (SimplePhysics phys = cast(SimplePhysics)node) {
                            incInspectorDeformSimplePhysics(phys, param, cursor);
                        }

                    } else {
                        incModelModeHeader(node);
                        incInspectorModelTRS(node);

                        // Node Camera Section
                        if (ExCamera camera = cast(ExCamera)node) {
                            incInspectorModelCamera(camera);
                        }

                        // Node Drawable Section
                        if (Composite composite = cast(Composite)node) {
                            incInspectorModelComposite(composite);
                        }


                        // Node Drawable Section
                        if (Drawable drawable = cast(Drawable)node) {
                            incInspectorModelDrawable(drawable);
                        }

                        // Node Part Section
                        if (Part part = cast(Part)node) {
                            incInspectorModelPart(part);
                        }

                        // Node SimplePhysics Section
                        if (SimplePhysics part = cast(SimplePhysics)node) {
                            incInspectorModelSimplePhysics(part);
                        }

                        // Node MeshGroup Section
                        if (MeshGroup group = cast(MeshGroup)node) {
                            incInspectorModelMeshGroup(group);
                        }
                    }
                
                break;
                default:
                    incCommonNonEditHeader(node);
                    break;
            }
        } else incInspectorModelInfo();        
    }

    void onParameterView(ulong index, Parameter param) {
        incParameterView!(false, false, true)(cast(int)index, param, &parameterGrabStr, false, incActivePuppet.parameters);
    }

}


interface CommandIssuer {
    void addCommand(string);
    void setCommand(string);
    void addPopup(Resource);
}

interface Output {
    void onUpdate();
}

private {
    void setTransparency(float alpha, float text) {
        ImGuiCol[] colIDs = [ImGuiCol.WindowBg, ImGuiCol.Text, ImGuiCol.FrameBg, ImGuiCol.Button, ImGuiCol.Border, ImGuiCol.PopupBg];
        foreach (id; colIDs) {
            ImVec4 style;
            style = *igGetStyleColorVec4(id);
            style.w = id == ImGuiCol.Text? text: alpha;
            igPushStyleColor(id, style);
        }
    }    
}

class NodeOutput : Output {
protected:
    CommandIssuer panel;

    Resource[] nodes;
    Resource[][Resource] children;
    Resource[] roots;
    bool[Resource] nodeIncluded;
    Resource focused = null;
public:
    this(CommandIssuer panel) {
        this.panel = panel;
    }

    override
    void onUpdate() {
        if (nodes.length > 0) {
            igPushID("Output");
            bool[uint] contentsDrawn;

            void traverse(Resource res) {
                ImGuiTreeNodeFlags flags;
                bool isNode = res.type == ResourceType.Node;
                if (res !in children && res.type != ResourceType.Parameter) flags |= ImGuiTreeNodeFlags.Leaf;
                flags |= ImGuiTreeNodeFlags.DefaultOpen;
                flags |= ImGuiTreeNodeFlags.OpenOnArrow;
                bool opened = igTreeNodeEx(cast(void*)res.uuid, flags, "");
                bool selected = false;
                bool noIcon = false;
                igSameLine();
                if (isNode) {
                    Node node = (cast(Proxy!Node)res).obj;
                    selected = incNodeInSelection(node);
                    if (auto part = cast(Part)node) {
                        if (node.typeId == "Part") {
                            noIcon = true;
                            incTextureSlotUntitled("ICON", part.textures[0], ImVec2(20, 20), 1, ImGuiWindowFlags.NoInputs);
                        }
                        igSameLine();
                    }
                }
                if (res !in nodeIncluded) {
                    igPushStyleColor(ImGuiCol.Text, igGetStyle().Colors[ImGuiCol.TextDisabled]);
                }

                bool isActive = false;
                switch (res.type) {
                case ResourceType.Parameter:
                    auto param = to!Parameter(res);
                    isActive = incArmedParameter() == param;
                    break;
                case ResourceType.Binding:
                    auto binding = to!ParameterBinding(res);
                    if (incArmedParameter() && incArmedParameter().bindings.canFind(binding)) {
                        isActive = binding.isSet(incParamPoint());
                    }
                    break;
                default:
//                    incButtonColored(isArmed ? "" : "", ImVec2(24, 24), isArmed ? ImVec4(1f, 0f, 0f, 1f) : *igGetStyleColorVec4(ImGuiCol.Text));
                }

                if (igSelectable("%s%s%s".format(noIcon? "": incTypeIdToIcon(res.typeId), isActive? "":"", res.name).toStringz, selected, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, 20))) {
                    if (isNode) {
                        Node node = (cast(Proxy!Node)res).obj;
                        incSelectNode(node);
                    }
                }
                bool isHovered = igIsItemHovered(); // Check if the selectable is hovered
                const char* popupName = "###%x".format(res.uuid).toStringz;
                if (isHovered && (isNode || res.type == ResourceType.Parameter)) {
                    if (igIsItemClicked(ImGuiMouseButton.Right)) {
                        igOpenPopup(popupName);
                    }
                }

                void showContents(Resource res) {
                    if (res.uuid in contentsDrawn) return;
                    ImVec2 size = ImVec2(20, 20);
                    if (igButton("\ue763", size)) {
                        if (panel)
                            panel.addPopup(res);
                    }

                    if (res.type == ResourceType.Node) {
                        igSameLine();
                        igText(res.name.toStringz);
                        Node node = to!Node(res);
                        onNodeView(node);
                    } else if (res.type == ResourceType.Parameter) {
                        Parameter param = to!Parameter(res);
                        igSameLine();
                        incParameterViewEditButtons!(false, true)(res.index, param, incActivePuppet.parameters, true);
                        igSameLine();
                        igText(res.name.toStringz);
                        onParameterView(res.index, param);
                    }
                    contentsDrawn[res.uuid] = true;
                };

                if (igBeginPopup(popupName)) {
                    showContents(res);
                    igEndPopup();
                }
                if (isHovered && igIsMouseDoubleClicked(ImGuiMouseButton.Left)) {
                    if (panel) {
                        string selectorStr = " %s#%d".format(res.typeId, res.uuid);
                        panel.setCommand(selectorStr);
                    }
                }
                if (res !in nodeIncluded) {
                    igPopStyleColor();
                }

                if (opened) {
                    if (res in children)
                        foreach (child; children[res])
                            traverse(child);
                    igTreePop();
                }
            }
            foreach (r; roots) {
                traverse(r);
            }
            igPopID();
        }
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
                addToMap(source, level + 1);
            } else {
                rootMap[res] = true;
            }
        }

        foreach (res; nodes) {
            addToMap(res);
        }
        foreach (res; childMap.keys) {
            if (res !in parentMap || parentMap[res] is null) {
                rootMap[res] = true;
            }
        }
        roots = rootMap.keys.sort!((a,b)=>a.index<b.index).array;
        foreach (item; childMap.byKeyValue) {
            children[item.key] = item.value.keys.sort!((a,b)=>a.index<b.index).array;
        }
    }
}

class ViewOutput : Output {
protected:
    CommandIssuer panel;

    Resource[] nodes;
    Resource[][Resource] children;
    Resource[] roots;
    bool[uint] pinned;
    bool[Resource] nodeIncluded;
    Resource focused = null;
    string parameterGrabStr;
public:
    this(CommandIssuer panel) {
        this.panel = panel;
    }

    override
    void onUpdate() {
        bool[uint] contentsDrawn;
        ulong[] removed;
        foreach (i, res; nodes) {
            void showContents(Resource res) {
                if (res.uuid in contentsDrawn) return;
                ImVec2 size = ImVec2(20, 20);
                igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(0, 0));
                igPushStyleVar(ImGuiStyleVar.ItemInnerSpacing, ImVec2(0, 0));
                igPushStyleVar(ImGuiStyleVar.FrameBorderSize, 0.0f);
                if (igButton("\ue5cd", size)) {                    
                    removed ~= i;
                }

                if (res.type == ResourceType.Node) {
                    igSameLine();
                    igText(res.name.toStringz);
                    Node node = (cast(Proxy!Node)res).obj();
                    onNodeView(node);
                } else if (res.type == ResourceType.Parameter) {
                    Parameter param = (cast(Proxy!Parameter)res).obj;
                    igSameLine();
                    incParameterViewEditButtons!(false, true)(res.index, param, incActivePuppet.parameters, true);
                    igSameLine();
                    igText(res.name.toStringz);
                    onParameterView(res.index, param);
                }
                igPopStyleVar(3);
                contentsDrawn[res.uuid] = true;
            };

            const char* popupName = "###%x".format(res.uuid).toStringz;
            setTransparency(focused == res? 0.8: 0.1, focused == res? 0.8: 0.3);
            ImGuiWindowFlags popupFlags;
            popupFlags = ImGuiWindowFlags.NoTitleBar;
            if (res.type == ResourceType.Parameter) {
                popupFlags |= ImGuiWindowFlags.NoResize;
            }
            if (igBegin(popupName, null, popupFlags)) {
                ImVec2 cursorPos;
                igGetMousePos(&cursorPos);
                ImVec2 windowPos;
                igGetWindowPos(&windowPos);
                ImVec2 windowSize;
                if (res.type == ResourceType.Node) {
                    windowSize.x = 250;
                    igSetWindowSize(windowSize);
                } else if (res.type == ResourceType.Parameter) {
                    Parameter param = (cast(Proxy!Parameter)res).obj;
                    windowSize = ImVec2(166, param.isVec2? 186: 92);
                    igSetWindowSize(windowSize);
                }
                igGetWindowSize(&windowSize);

                ImVec2 windowEndPos = ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y);
                bool isCursorInside = (cursorPos.x >= windowPos.x && cursorPos.x <= windowEndPos.x) &&
                                    (cursorPos.y >= windowPos.y && cursorPos.y <= windowEndPos.y);
                
                if (isCursorInside) {
                    focused = res;
                } else if (focused == res) {
                    focused = null;
                }

                showContents(res);
                igEnd();
            }
            igPopStyleColor(6);
        }
        foreach_reverse (r; removed) {
            nodes = nodes.remove(r);
        }
    }

    void addResources(Resource[] nodes_) {
        nodes ~= nodes_;
    }
}