module nijigenerate.windows.automeshbatch;

import nijigenerate.windows.base;
import nijigenerate.viewport.common.mesh;
import nijigenerate.core;
import nijigenerate.widgets;
import nijigenerate.ext;
import nijigenerate.utils;
import nijigenerate;
import nijilive;
import i18n;
import std.string;
import std.algorithm.iteration;

class AutoMeshBatchWindow : Modal {
private:
    alias ApplicableClass = Part;

    string nodeFilter;

    enum PreviewSize = 128f;
    Node[] nodes;
    bool[uint] selected;
    Node active;

    void apply() {
    }

    vec4 previewImage(Part part, ImVec2 centerPos, float previewSize, ImVec2 uv0 = ImVec2(0, 0), ImVec2 uv1 = ImVec2(1, 1), ImVec4 tintColor=ImVec4(1, 1, 1, 1), IncMesh mesh = null) {
        if (part.textures[0].width < previewSize && part.textures[0].height < previewSize)
            previewSize = max(part.textures[0].width, part.textures[0].height);

        float widthScale = previewSize / cast(float)part.textures[0].width;
        float heightScale = previewSize / cast(float)part.textures[0].height;
        float fscale = min(widthScale, heightScale);
        
        vec4 bounds = vec4(0, 0, part.textures[0].width*fscale, part.textures[0].height*fscale);
        if (widthScale > heightScale) bounds.x = (previewSize-bounds.z)/2;
        else if (widthScale < heightScale) bounds.y = (previewSize-bounds.w)/2;

        ImVec2 tl;
        igGetCursorPos(&tl);

        igItemSize(ImVec2(previewSize, previewSize));

        igSetCursorPos(
            ImVec2(tl.x+centerPos.x-previewSize/2+bounds.x, tl.y+centerPos.y-previewSize/2+bounds.y)
        );

        igImage(
            cast(void*)part.textures[0].getTextureId(), 
            ImVec2(bounds.z, bounds.w), uv0, uv1, tintColor
        );
        return bounds;

    }

    void treeView() {

        import std.algorithm.searching : canFind;
        foreach(i, ref Node node; nodes) {
            if (nodeFilter.length > 0 && !node.name.toLower.canFind(nodeFilter.toLower)) continue;

            igPushID(cast(int)i);

            if (igSelectable("###%x".format(node.uuid).toStringz, active == node, ImGuiSelectableFlagsI.SpanAvailWidth | ImGuiSelectableFlags.AllowItemOverlap)) {
                active = node;
            }
            igSameLine(0, 0);
            if ((cast(ApplicableClass)node) !is null) {
                if (ngCheckbox("###check%x".format(node.uuid).toStringz, &(selected[node.uuid]))) {
                    import std.stdio;
                    writefln("toggled %s, %s", node.name, selected[node.uuid]);
                }
                igSameLine(0, 0);
            } else {
                igSameLine(30, 0);
            }
            igText((incTypeIdToIcon(node.typeId)~node.name).toStringz);

            // Incredibly cursed preview image
            if (igIsItemHovered()) {
                igBeginTooltip();
                    incText(incTypeIdToIcon(node.typeId)~node.name);
                    // Calculate render size
                    if (auto part = cast(Part)node) {
                        previewImage(part, ImVec2(PreviewSize/2, PreviewSize/2), PreviewSize);
                    }
                igEndTooltip();
            }
            igPopID();
        }

    }

protected:

    override
    void onBeginUpdate() {
        flags |= ImGuiWindowFlags.NoSavedSettings;
//        incIsSettingsOpen = true;
        
        ImVec2 wpos = ImVec2(
            igGetMainViewport().Pos.x+(igGetMainViewport().Size.x/2),
            igGetMainViewport().Pos.y+(igGetMainViewport().Size.y/2),
        );

        ImVec2 uiSize = ImVec2(
            800, 
            600
        );

        igSetNextWindowPos(wpos, ImGuiCond.Appearing, ImVec2(0.5, 0.5));
        igSetNextWindowSize(uiSize, ImGuiCond.Appearing);
        igSetNextWindowSizeConstraints(uiSize, ImVec2(float.max, float.max));
        super.onBeginUpdate();
    }

    override
    void onUpdate() {
        ImVec2 space = incAvailableSpace();
        float gapspace = 8;
        float childWidth = (space.x/2);
        float childHeight = floor(space.y-28-6);
        float previewSize = min(space.x/2 - gapspace, childHeight);
        float filterWidgetHeight = 26;
        float optionsListHeight = 26;

        igBeginGroup();
            // Selection
            if (igBeginChild("###Nodes", ImVec2(childWidth, childHeight))) {
                incInputText("##", childWidth, nodeFilter);

                igBeginListBox("###NodeList", ImVec2(childWidth, childHeight-filterWidgetHeight));
                    treeView();
                igEndListBox();
            }
            igEndChild();

            igSameLine(0, gapspace);

            // Preview
            if (igBeginChild("###Preview", ImVec2(previewSize, previewSize), true, ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse)) {
                vec4 bounds;
                if (active !is null) {
                    if (auto part = cast(Part)active)
                        bounds = previewImage(part, ImVec2(previewSize / 2, previewSize / 2), previewSize - gapspace * 2);
                }

            }
            igEndChild();

        igEndGroup();


        igBeginGroup();
            incDummy(ImVec2(-192, 0));
            igSameLine(0, 0);
            // 
            if (incButtonColored(__("Cancel"), ImVec2(96, 24))) {
                this.close();
                
                igEndGroup();
                return;
            }
            igSameLine(0, 0);
            if (incButtonColored(__("Save"), ImVec2(96, 24))) {
                apply();
                this.close();
                
                igEndGroup();
                return;
            }
        igEndGroup();
    }

//    override
//    void onClose() {
//        import core.memory : GC;
//        GC.collect();
//        GC.minimize();
//    }

    void close() {
        incModalCloseTop();
    }

public:
    ~this() { }

    this() {
        auto puppet = incActivePuppet();
        nodes = puppet.findNodesType!Node(puppet.root);
        nodes.each!((n) {
            selected.require(n.uuid);
            selected[n.uuid] = (cast(ApplicableClass)n) !is null;
        });
        // Removing unused pairs (happens when target nodes are removed.)

        super(_("Automesh Batching"), true);
        flags |= ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse;
    }
}
