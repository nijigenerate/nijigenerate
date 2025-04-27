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
import std.algorithm.searching;
//import std.stdio;
import nijilive.core.dbg;
import core.thread.osthread;
import core.sync.mutex;
import core.thread.fiber;
import core.memory;

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

    Thread processingThread;
    shared Mutex gcMutex = null;
    shared bool canceled = false;
    bool selectAll = false;
    enum ToggleAction { NoAction, None, All }
    ToggleAction toggleAction = ToggleAction.NoAction;

    void apply() {
        foreach (node; nodes) {
            auto part = cast(ApplicableClass)node;
            if (!isApplicable(node) || node.uuid !in meshes || node.uuid !in status || status[node.uuid] != Status.Succeeded) continue;
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
        selectAll = true;
        foreach(i, ref Node node; nodes) {
            if (nodeFilter.length > 0 && !node.name.toLower.canFind(nodeFilter.toLower)) continue;

            igPushID(cast(int)i);

            if (igSelectable("###%x".format(node.uuid).toStringz, active == node, ImGuiSelectableFlagsI.SpanAvailWidth | ImGuiSelectableFlags.AllowItemOverlap)) {
                active = node;
            }
            igSameLine(0, 0);
            if (isApplicable(node)) {
                switch (toggleAction) {
                case ToggleAction.None:
                    selected[node.uuid] = false;
                    break;
                case ToggleAction.All:
                    selected[node.uuid] = true;
                    break;
                default:
                }
                if (!selected[node.uuid]) selectAll = false;
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
        toggleAction = ToggleAction.NoAction;

    }

    bool shouldBeSelected(Node node) {
        if (auto part = cast(ApplicableClass)node) {
            if (auto dcomposite = cast(DynamicComposite)part) {
                if (dcomposite.autoResizedMesh)
                    return false;
            }

            return true;
        } else
            return false;
    }

    bool isApplicable(Node node) {
        return cast(ApplicableClass)node !is null;
    }

protected:

    void runBatch() {
        auto targets = nodes.filter!((n)=>n.uuid in selected && selected[n.uuid]).map!(n=>cast(Drawable)n).array;
        auto meshList = targets.map!(t=>new IncMesh(meshes[t.uuid])).array;
        status.clear();
        foreach (t; targets) status[t.uuid] = Status.Waiting;
        Drawable currentTarget = null;
        bool callback(Drawable drawable, IncMesh mesh) {
            currentTarget = drawable;
            if (mesh is null) {
                status[drawable.uuid] = Status.Running;
            } else {
                if (mesh.vertices.length >= 3) {
                    status[drawable.uuid] = Status.Succeeded;
                    meshes[drawable.uuid] = mesh;
                } else {
                    status[drawable.uuid] = Status.Failed;
                }
            }
            bool result = false;
            synchronized(gcMutex) { result = canceled; }
            return !result;
        }
        void work() {
            ngActiveAutoMeshProcessor.autoMesh(targets, meshList, false, 0, false, 0, &callback);
        }
        auto fib = new Fiber(&work, core.memory.pageSize * Fiber.defaultStackPages * 4);
        while (fib.state != Fiber.State.TERM) {
            fib.call();
            bool result = false;
            synchronized(gcMutex) { result = canceled; }
            if (result) {
                if (currentTarget) {
                    status[currentTarget.uuid] = Status.Failed;
                }
                break;
            }
        }
        auto parts = nodes.filter!((n)=>n.uuid in selected && selected[n.uuid] && isApplicable(n)).map!(n=>cast(ApplicableClass)n);
        foreach (part; parts) part.textures[0].unlock();
        import core.memory;
        GC.collect();
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
                if (ngCheckbox("##toggleCheck", &selectAll)) {
                    if (selectAll) {
                        toggleAction = ToggleAction.All;
                    } else {
                        toggleAction = ToggleAction.None;
                    }
                }
                igSameLine(0, 0);
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

            if (processingThread !is null) {
                if (!processingThread.isRunning()) {
                    processingThread.join();
                    processingThread = null;
                    auto parts = nodes.filter!((n)=>n.uuid in selected && selected[n.uuid] && isApplicable(n)).map!(n=>cast(ApplicableClass)n);
                    foreach (part; parts) part.textures[0].unlock();
                    canceled = false;
                    import core.memory;
                    GC.collect();
                }
            }

            igBeginChild("###Actions", ImVec2(childWidth, 40));
            if (incButtonColored(processingThread? __("Cancel") : __("Run batch"))) {
                if (!processingThread) {
                    auto parts = nodes.filter!((n)=>n.uuid in selected && selected[n.uuid] && isApplicable(n)).map!(n=>cast(ApplicableClass)n);
                    foreach (part; parts) part.textures[0].lock();
                    processingThread = new Thread(&runBatch);
                    processingThread.start();
                } else {
                    synchronized(gcMutex) { canceled = true; }
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
                canceled = true;
                this.close();
                
                igEndGroup();
                return;
            }
            igSameLine(0, 0);
            if (incButtonColored(__("Save"), ImVec2(96, 24))) {
                apply();
                canceled = true;
                this.close();
                
                igEndGroup();
                return;
            }
        igEndGroup();
    }

    void close() {
        if (processingThread !is null) {
            // TBD: kill thread
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
            selected[n.uuid] = shouldBeSelected(n);
            if (isApplicable(n))
                meshes[n.uuid] = new IncMesh(part.getMesh());
        });
        // Removing unused pairs (happens when target nodes are removed.)

        super(_("Automesh Batching"), true);
        flags |= ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse;
        gcMutex = cast(shared)new Mutex;
    }
}
