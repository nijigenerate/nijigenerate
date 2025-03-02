module nijigenerate.windows.automeshbatch;

import nijigenerate.windows.base;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.vertex;
import nijigenerate.core.math.mesh;
import nijigenerate.core;
import nijigenerate.widgets;
import nijigenerate.ext;
import nijigenerate.utils;
import nijigenerate;
import nijilive;
import i18n;
import std.string;
import std.array;
import std.range;
import std.algorithm.iteration;
import std.concurrency;
import std.stdio;
import nijilive.core.dbg;
import core.thread.osthread;
import core.time;

private {
    alias ApplicableClass = Part;
    class StartProcess {
        const ApplicableClass target;
    public:
        this(const ApplicableClass t) { target = t; }
    }
    class EndProcess {
        const ApplicableClass target;
        const IncMesh mesh;
    public:
        this(const ApplicableClass t, const IncMesh m) { target = t; mesh = m; }
    }

}

class AutoMeshBatchWindow : Modal {
private:
    alias ApplicableClass = Part;

    string nodeFilter;

    enum PreviewSize = 128f;
    Node[] nodes;
    IncMesh[uint] meshes;
    bool[uint] selected;
    Node active;

    enum Status { Waiting, Running, Succeeded, Failed };
    Status[uint] status;

    Tid processingThread;
    bool running = false;

    void apply() {
        foreach (node; nodes) {
            auto part = cast(ApplicableClass)node;
            if (part is null) continue;
            auto mesh = meshes[node.uuid];
            applyMeshToTarget(part, mesh.vertices, &mesh);
        }
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
        ImVec2 screenPos;
        igGetCursorPos(&tl);

        igItemSize(ImVec2(previewSize, previewSize));

        igSetCursorPos(
            ImVec2(tl.x+centerPos.x-previewSize/2+bounds.x, tl.y+centerPos.y-previewSize/2+bounds.y)
        );

        igGetCursorScreenPos(&screenPos);
        igImage(
            cast(void*)part.textures[0].getTextureId(), 
            ImVec2(bounds.z, bounds.w), uv0, uv1, tintColor
        );

        if (mesh) {
            ImDrawList* drawList = igGetWindowDrawList();
            ImS32 lineColor = igGetColorU32(ImVec4(0.7, 0.7, 0.7, 1));
            vec2 cPos    = vec2(part.textures[0].width / 2, part.textures[0].height / 2);
            vec2 scrPos  = vec2(screenPos.x, screenPos.y);
            foreach (v; mesh.vertices) {
                foreach (c; v.connections) {
                    vec2 p1Pos = v.position;
                    vec2 p2Pos = c.position;
                    p1Pos = scrPos + (p1Pos + cPos ) * fscale;
                    p2Pos = scrPos + (p2Pos + cPos ) * fscale;
                    ImDrawList_AddLine(drawList, ImVec2(p1Pos.x, p1Pos.y), ImVec2(p2Pos.x, p2Pos.y), lineColor, 1.0f);
                }
            }
        }
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
                ngCheckbox("###check%x".format(node.uuid).toStringz, &(selected[node.uuid]));
                igSameLine(0, 0);
            } else {
                igSameLine(30, 0);
            }
            if (node.uuid in status) {
                switch (status[node.uuid]) {
                case Status.Waiting:
                    igTextColored(ImVec4(0.8, 0.4, 0, 1), "\uef4a");
                    break;
                case Status.Running:
                    igTextColored(ImVec4(0, 0.4, 0.8, 1), "\ue1c4");
                    break;
                case Status.Succeeded:
                    igTextColored(ImVec4(0, 0.9, 0, 1), "\ue92f");
                    break;
                case Status.Failed:
                    igTextColored(ImVec4(0.9, 0, 0, 1), "\ue000");
                    break;
                default:
                }
            }
            igSameLine(0, 0);
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

    static void runBatch(const AutoMeshProcessor processor, const ApplicableClass[] targets, const IncMesh[] meshes) {
        foreach (i, t; targets) {
            auto start = new StartProcess(t);
            send(ownerTid(), cast(immutable)start);
            auto mesh = processor.autoMesh(t, meshes[i], false, 0, false, 0);
            auto end = new EndProcess(t, mesh);
            send(ownerTid(), cast(immutable)end);
        }
    }

    override
    void onBeginUpdate() {
        flags |= ImGuiWindowFlags.NoSavedSettings;
        
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
        float childWidth = (space.x/3) - gapspace;
        float childHeight = floor(space.y-28-6);
        float previewSize = min(space.x/3 - gapspace, childHeight);
        float filterWidgetHeight = 26;
        float optionsListHeight = 26;

        igBeginGroup();
            if (igBeginChild("###Nodes", ImVec2(childWidth, childHeight))) {
                incInputText("##", childWidth, nodeFilter);

                igBeginListBox("###NodeList", ImVec2(childWidth, childHeight-filterWidgetHeight));
                    treeView();
                igEndListBox();
            }
            igEndChild();

            igSameLine(0, gapspace);

            igBeginGroup();
            igBeginChild("###Processors", ImVec2(childWidth, 40));
            foreach (processor; ngAutoMeshProcessors) {
                if (incButtonColored(processor.icon().toStringz, ImVec2(0, 0), (processor == ngActiveAutoMeshProcessor)? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                    ngActiveAutoMeshProcessor = processor;
                }
                igSameLine(0, 2);
            }
            igEndChild();

            if (igBeginChild("##Configs", ImVec2(childWidth, childHeight - 80))) {
                ngActiveAutoMeshProcessor.configure();
            }
            igEndChild();

            if (running) {
                receiveTimeout(0.msecs,
                    (immutable StartProcess e) {
                        status[e.target.uuid] = Status.Waiting;
                        status[e.target.uuid] = Status.Running;
                        writefln("Start %s", e.target.name);
                    },
                    (immutable EndProcess e) {
                        status[e.target.uuid] = Status.Succeeded;
                        meshes[e.target.uuid] = cast(IncMesh)e.mesh;
                        writefln("End %s", e.target.name);
                    },
                    (LinkTerminated e) {
                        auto parts = nodes.filter!((n)=>n.uuid in selected && selected[n.uuid] && cast(ApplicableClass)n).map!(n=>cast(ApplicableClass)n);
                        foreach (part; parts) part.textures[0].unlock();
                        running = false;
                    });
            }

            igBeginChild("###Actions", ImVec2(childWidth, 40));
            if (incButtonColored(running? __("Cancel") : __("Auto mesh"))) {
                if (!running) {
                    auto parts = nodes.filter!((n)=>n.uuid in selected && selected[n.uuid] && cast(ApplicableClass)n).map!(n=>cast(ApplicableClass)n).array;
                    auto meshes = parts.map!((n)=>meshes[n.uuid]).array;
                    foreach (part; parts) part.textures[0].lock();
                    status.clear();
                    foreach (t; parts) {
                        status[t.uuid] = Status.Waiting;
                    }
                    auto im_parts = cast(immutable)parts;
                    auto im_meshes = cast(immutable)meshes;
                    auto im_processor = cast(immutable)ngActiveAutoMeshProcessor;
                    processingThread = spawnLinked(&runBatch, im_processor, im_parts, im_meshes);
                    running = true;
                }
            }
            igEndChild();
            igEndGroup();

            igSameLine(0, gapspace);
            igBeginGroup();
            incDummy(ImVec2(childWidth, (childHeight - previewSize) / 2));
            // Preview
            if (igBeginChild("###Preview", ImVec2(previewSize, previewSize), true, ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse)) {
                vec4 bounds;
                if (active !is null) {
                    if (auto part = cast(Part)active)
                        bounds = previewImage(part, ImVec2(previewSize / 2, previewSize / 2), previewSize - gapspace * 2, ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,1), meshes[part.uuid]);
                }

            }
            igEndChild();
            igEndGroup();

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

    void close() {
        if (processingThread !is null) {
            // TBD: kill thread
            GC.enable();
        }
        incModalCloseTop();
    }

public:
    ~this() { }

    this() {
        auto puppet = incActivePuppet();
        nodes = puppet.findNodesType!Node(puppet.root);
        nodes.each!((n) {
            selected.require(n.uuid);
            auto part = (cast(ApplicableClass)n);
            selected[n.uuid] = part !is null;
            if (selected[n.uuid])
                meshes[n.uuid] = new IncMesh(part.getMesh());
        });
        // Removing unused pairs (happens when target nodes are removed.)

        super(_("Automesh Batching"), true);
        flags |= ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse;
        gcMutex = cast(shared)new Mutex;
    }
}
