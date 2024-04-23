module creator.panels.shell.output;

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

interface Output {
    void onUpdate();
}

class ListOutput(T) : Output {
protected:
    ShellPanel panel;

    Resource[] nodes;
    Resource[][Resource] children;
    Resource[] roots;
    bool[Resource] nodeIncluded;
public:
    this(ShellPanel panel) {
        this.panel = panel;
    }

    override
    void onUpdate() {
        if (nodes.length > 0) {
            igPushID("Output");

            void traverse(Resource res) {
                ImGuiTreeNodeFlags flags;
                bool isNode = res.type == ResourceType.Node;
                if (res !in children) flags |= ImGuiTreeNodeFlags.Leaf;
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
                if (igSelectable("%s%s".format(noIcon? "": incTypeIdToIcon(res.typeId), res.name).toStringz, selected, ImGuiSelectableFlags.AllowDoubleClick, ImVec2(0, 0))) {
                    if (isNode) {
                        Node node = (cast(Proxy!Node)res).obj;
                        incSelectNode(node);
                    }
                }
                if (igIsItemHovered() && igIsMouseDoubleClicked(ImGuiMouseButton.Left)) {
                    if (panel) {
                        string selectorStr = " %s#%d".format(res.typeId, res.uuid);
                        panel.addCommand(selectorStr);
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

    void setNodes(Resource[] nodes_) {
        nodes = nodes_;
        Resource[Resource] parentMap;
        roots.length = 0;
        bool[Resource] rootMap;
        bool[Resource][Resource] childMap;
        nodeIncluded.clear();
        foreach (n; nodes) {
            nodeIncluded[n] = true;
        }

        void addToMap(Resource res, int level = 0) {
            if (res in parentMap) return;
            auto source = res.source;
            if (level > 0) {
                while (source) {
                    if (source.source is null) break;
                    if (source in nodeIncluded) break;
                    source = source.source;
                }
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
        roots = rootMap.keys.sort!((a,b)=>a.name<b.name).array;
        children.clear();
        foreach (item; childMap.byKeyValue) {
            children[item.key] = item.value.keys.sort!((a,b)=>a.name<b.name).array;
        }
    }
}

alias NodeOutput = ListOutput!Node;