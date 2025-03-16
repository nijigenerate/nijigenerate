/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.viewport.vertex;
import i18n;
import nijigenerate.viewport.base;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.mesheditor;
import nijigenerate.viewport.vertex.mesheditor;
import nijigenerate.actions;
public import nijigenerate.viewport.vertex.automesh;
import nijigenerate.core.input;
import nijigenerate.core.actionstack;
import nijigenerate.widgets;
import nijigenerate;
import nijilive;
import bindbc.imgui;
import std.stdio;
import std.string;
import bindbc.opengl;

private {
    AutoMeshProcessor[] autoMeshProcessors = [
        new OptimumAutoMeshProcessor(),
        new ContourAutoMeshProcessor(),
        new GridAutoMeshProcessor(),
        new SkeletonExtractor()
    ];
    AutoMeshProcessor activeProcessor = null;
    bool isSubPartsMeshVisible = false;
}

class VertexViewport : Viewport {
protected:
    IncMeshEditor editor;
public:

    override
    void draw(Camera camera) { 
        // Draw the part that is currently being edited
        auto targets = editor.getTargets();
        if (targets.length > 0) {
            foreach (target; targets) {
                if (Part part = cast(Part)target) {
                    // Draw albedo texture at 0, 0
                    auto origin = vec2(0, 0);
                    if (part.textures[0] !is null) {
                        if (DynamicComposite dcomposite = cast(DynamicComposite)part) origin = dcomposite.textureOffset;
                        inDrawTextureAtPosition(part.textures[0], origin);
                    } else {
                        mat4 transform = part.transform.matrix.inverse;
                        part.setOneTimeTransform(&transform);
                        part.drawOne();
                        part.setOneTimeTransform(null);
                    }
                } else if (target.coverOthers()) {
                    mat4 transform = target.transform.matrix.inverse;
                    target.setOneTimeTransform(&transform);
                    Node[] subParts;
                    void findSubDrawable(Node n) {
                        if (n.coverOthers()) {
                            foreach (child; n.children)
                                findSubDrawable(child);
                        }
                        if (auto c = cast(Composite)n) {
                            if (c.propagateMeshGroup) {
                                subParts ~= c;
                            }
                        } else if (auto d = cast(Drawable)n) {
                            subParts ~= d;
                            foreach (child; n.children)
                                findSubDrawable(child);
                        }
                    }
                    findSubDrawable(target);
                    import std.algorithm.sorting;
                    import std.algorithm.mutation : SwapStrategy;
                    import std.math : cmp;
                    sort!((a, b) => cmp(
                        a.zSort, 
                        b.zSort) > 0, SwapStrategy.stable)(subParts);

                    foreach (part; subParts) {
                        part.drawOne();
                    }
                    if (isSubPartsMeshVisible) {
                        foreach (node; subParts) {
                            if (target != node)
                                if (auto drawable = cast(Drawable)node)
                                    drawable.drawMeshLines();
                        }
                    }
                    target.setOneTimeTransform(null);
                }
            }
        }

        editor.draw(camera);
    };


    override
    void drawTools() { 
        editor.viewportTools();        
    };


    override
    void drawOptions() { 
        editor.displayGroupIds();

        igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(0, 0));
        igPushStyleVar(ImGuiStyleVar.WindowPadding, ImVec2(4, 4));
            igBeginGroup();
                if (incButtonColored("", ImVec2(0, 0), isSubPartsMeshVisible ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                    isSubPartsMeshVisible = !isSubPartsMeshVisible;
                }
                incTooltip(_("Toggle mesh visibility"));
            igEndGroup();

            igSameLine(0, 4);

            igBeginGroup();
                if (incButtonColored("")) {
                    foreach (d; incSelectedNodes) {
                        if (auto meshEditor = cast(IncMeshEditorOneDrawable)editor.getEditorFor(d))
                            meshEditor.getMesh().flipHorz();
                        else if (auto meshEditor = cast(IncMeshEditorOneDeformable)editor.getEditorFor(d)) {
                            auto moveAction = new VertexMoveAction("flip vertices horizontally", meshEditor);
                            foreach (i, v; meshEditor.vertices)
                                moveAction.moveVertex(v, vec2(-v.position.x, v.position.y));
                            incActionPush(moveAction);
                        }
                    }
                }
                incTooltip(_("Flip Horizontally"));

                igSameLine(0, 0);

                if (incButtonColored("")) {
                    foreach (d; incSelectedNodes) {
                        if (auto meshEditor = cast(IncMeshEditorOneDrawable)editor.getEditorFor(d))
                            meshEditor.getMesh().flipVert();
                        else if (auto meshEditor = cast(IncMeshEditorOneDeformable)editor.getEditorFor(d)) {
                            auto moveAction = new VertexMoveAction("flip vertices vertically", meshEditor);
                            foreach (i, v; meshEditor.vertices)
                                moveAction.moveVertex(v, vec2(v.position.x, -v.position.y));
                            incActionPush(moveAction);
                        }
                    }
                }
                incTooltip(_("Flip Vertically"));
            igEndGroup();

            igSameLine(0, 4);

            igBeginGroup();
                if (incButtonColored("", ImVec2(0, 0), editor.getMirrorHoriz() ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                    editor.setMirrorHoriz(!editor.getMirrorHoriz());
                    editor.refreshMesh();
                }
                incTooltip(_("Mirror Horizontally"));

                igSameLine(0, 0);

                if (incButtonColored("", ImVec2(0, 0), editor.getMirrorVert() ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                    editor.setMirrorVert(!editor.getMirrorVert());
                    editor.refreshMesh();
                }
                incTooltip(_("Mirror Vertically"));
            igEndGroup();

            igSameLine(0, 4);

            igBeginGroup();
                if (incButtonColored("", ImVec2(0, 0),
                    editor.getPreviewTriangulate() ? ImVec4(1, 1, 0, 1) : colorUndefined)) {
                    editor.setPreviewTriangulate(!editor.getPreviewTriangulate());
                    editor.refreshMesh();
                }
                incTooltip(_("Triangulate vertices"));

                if (incBeginDropdownMenu("TRIANGULATE_SETTINGS")) {
                    incDummyLabel("TODO: Options Here", ImVec2(0, 192));

                    // Button which bakes some auto generated content
                    // In this case, a mesh is baked from the triangulation.
                    if (incButtonColored(__("Bake"), ImVec2(incAvailableSpace().x, 0),
                        editor.previewingTriangulation() ? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                        if (editor.previewingTriangulation()) {
                            editor.applyPreview();
                            editor.refreshMesh();
                        }
                    }
                    incTooltip(_("Bakes the triangulation, applying it to the mesh."));
                    
                    incEndDropdownMenu();
                }
                incTooltip(_("Triangulation Options"));

            igEndGroup();

            igSameLine(0, 4);

            igBeginGroup();
                if (incButtonColored("")) {
                    foreach (drawable; editor.getTargets()) {
                        auto e = cast(IncMeshEditorOneDrawable)editor.getEditorFor(drawable);
                        if (e !is null) {
                            import core.thread.fiber;
                            void worker() {
                                auto newMesh = ngActiveAutoMeshProcessor.autoMesh(cast(Drawable)drawable, e.getMesh(), e.mirrorHoriz, 0, e.mirrorVert, 0);
                                e.setMesh(newMesh);
                            }
                            import core.memory: pageSize;
                            auto fib = new Fiber(&worker, pageSize * 32);
                            while (fib.state != Fiber.State.TERM) {
                                fib.call();
                            }
                        }
                    }
                    editor.refreshMesh();
                }
                if (incBeginDropdownMenu("AUTOMESH_SETTINGS")) {
                    igBeginGroup();
                    foreach (processor; autoMeshProcessors) {
                        if (incButtonColored(processor.icon().toStringz, ImVec2(0, 0), (processor == ngActiveAutoMeshProcessor)? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) {
                            ngActiveAutoMeshProcessor = processor;
                        }
                        igSameLine(0, 2);
                    }
                    igEndGroup();

                    ngActiveAutoMeshProcessor.configure();

                    // Button which bakes some auto generated content
                    // In this case, a mesh is baked from the triangulation.
                    if (incButtonColored(__("Bake"),ImVec2(incAvailableSpace().x, 0))) {
                        foreach (drawable; editor.getTargets()) {
                            auto e = cast(IncMeshEditorOneDrawable)editor.getEditorFor(drawable);
                            if (e !is null)
                                e.setMesh(ngActiveAutoMeshProcessor.autoMesh(cast(Drawable)drawable, e.getMesh(), e.mirrorHoriz, 0, e.mirrorVert, 0));
                        }
                        editor.refreshMesh();
                    }
                    incTooltip(_("Bakes the auto mesh."));
                    
                    incEndDropdownMenu();
                }
                incTooltip(_("Auto Meshing Options"));
            igEndGroup();

            igSameLine(0, 4);

            editor.displayToolOptions();
        igPopStyleVar(2);
    };


    override
    void drawConfirmBar() {
        auto target = editor.getTargets();
        igPushStyleVar(ImGuiStyleVar.FramePadding, ImVec2(16, 4));
            if (incButtonColored(__(" Apply"), ImVec2(0, 26))) {
                if (incMeshEditGetIsApplySafe()) {
                    incMeshEditApply();
                } else {
                    incDialog(
                        "CONFIRM_VERTEX_APPLY", 
                        __("Are you sure?"), 
                        _("The layout of the mesh has changed, all deformations to this mesh will be deleted if you continue."),
                        DialogLevel.Warning,
                        DialogButtons.Yes | DialogButtons.No
                    );
                }
            }

            // In case of a warning popup preventing application.
            // TODO: if incDialogButtonSelected does not work, we may implement a DialogHandler for this.
            if (incDialogButtonSelected("CONFIRM_VERTEX_APPLY") == DialogButtons.Yes) {
                incMeshEditApply();
            }
            incTooltip(_("Apply"));
            
            igSameLine(0, 0);

            if (incButtonColored(__(" Cancel"), ImVec2(0, 26))) {
                if (igGetIO().KeyShift) {
                    incMeshEditReset();
                } else {
                    incMeshEditClear();
                }
                incActionPopStack();
                incSetEditMode(EditMode.ModelEdit);
                foreach (d; target) {
                    incAddSelectNode(d);
                }
                incFocusCamera(target[0]);  /// FIX ME!
            }
            incTooltip(_("Cancel"));
        igPopStyleVar();
    };

    override
    void update(ImGuiIO* io, Camera camera) {
        editor.update(io, camera);
    }

    override
    void withdraw() { 
        editor = null;
    };

    override
    void present() { 
        editor = new VertexMeshEditor();
    };

    void copyMeshDataToTarget(Deformable target, Deformable source) {
        if (!editor.getEditorFor(target)) {
            editor.addTarget(target);
            assert(editor.getEditorFor(target));
        }
        if (auto drawable = cast(Drawable)source) {
            editor.getEditorFor(target).importMesh(drawable.getMesh());
        } else {
            MeshData mesh;
            mesh.vertices = source.vertices;
            mesh.uvs = source.vertices;
            editor.getEditorFor(target).importMesh(mesh);
        }
    }


    void mergeMeshDataToTarget(Deformable target, Deformable source) {
        mat4 matrix = source.transform.matrix * target.transform.matrix.inverse;
        if (!editor.getEditorFor(target)) {
            editor.addTarget(target);
            assert(editor.getEditorFor(target));
        }
        if (auto drawable = cast(Drawable)source) {
            editor.getEditorFor(target).mergeMesh(drawable.getMesh(), matrix);
        } else {
            MeshData mesh;
            mesh.vertices = source.vertices;
            mesh.uvs = source.vertices;
            editor.getEditorFor(target).mergeMesh(mesh, matrix);
        }
    }


    void apply() {
        auto target = editor.getTargets();
        
        // Automatically apply triangulation
        if (editor.previewingTriangulation()) {
            editor.applyPreview();
            editor.refreshMesh();
        }

        foreach (d; target) {
            if (Drawable drawable = cast(Drawable)d) {
                auto meshEditor = cast(IncMeshEditorOneDrawable)editor.getEditorFor(drawable);
                /*
                if (meshEditor !is null && (meshEditor.getMesh().getTriCount() < 1)) {
                    incDialog(__("Error"), _("Cannot apply invalid mesh\nAt least 3 vertices forming a triangle is needed."));
                    return;
                }
                */
            }
        }

        incActionPopStack();
        // Apply to target
        editor.applyToTarget();

        // Switch mode
        incSetEditMode(EditMode.ModelEdit);
        foreach (d; target) {
            if (Drawable drawable = cast(Drawable)d)
                incAddSelectNode(drawable);
        }
        incFocusCamera(target[0]); /// FIX ME        
    }

    void clear() {
        foreach (d; editor.getTargets()) {
            auto meshEditor = cast(IncMeshEditorOneDrawable)editor.getEditorFor(d);
            if (meshEditor !is null)
                meshEditor.getMesh().clear();
        }
    }

    void reset() {
        foreach (d; editor.getTargets()) {
            auto meshEditor = cast(IncMeshEditorOneDrawable)editor.getEditorFor(d);
            if (meshEditor !is null)
                meshEditor.getMesh().reset();
        }
    }
}

VertexViewport incVertexViewport() {
    return cast(VertexViewport)incViewport.subView;
}

/*
Drawable incVertexEditGetTarget() {
    return editor.getTarget();
}
*/

void incVertexEditStartEditing(Deformable target) {
    incSetEditMode(EditMode.VertexEdit);
    incSelectNode(target);
    incVertexEditSetTarget(target);
    incFocusCamera(target, vec2(0, 0));
}

void incVertexEditSetTarget(Deformable target) {
    if (auto view = incVertexViewport)
        view.editor.setTarget(target);
}

void incVertexEditCopyMeshDataToTarget(Deformable target, Deformable source) {
    if (auto view = incVertexViewport)
        view.copyMeshDataToTarget(target, source);
}

void incVertexEditMergeMeshDataToTarget(Deformable target, Deformable source) {
    if (auto view = incVertexViewport)
        view.mergeMeshDataToTarget(target, source);
}

bool incMeshEditGetIsApplySafe() {
    /* Disabled temporary
    Drawable target = cast(Drawable)editor.getTarget();
    return !(
        editor.mesh.getVertexCount() != target.getMesh().vertices.length &&
        incActivePuppet().getIsNodeBound(target)
    );
    */
    return true;
}

/**
    Applies the mesh edits
*/
void incMeshEditApply() {
    if (auto view = incVertexViewport)
        view.apply();
}

/**
    Resets the mesh edits
*/
void incMeshEditClear() {
    if (auto view = incVertexViewport)
        view.clear();
}


/**
    Resets the mesh edits
*/
void incMeshEditReset() {
    if (auto view = incVertexViewport)
        view.reset();
}

AutoMeshProcessor ngActiveAutoMeshProcessor() {
    if (!activeProcessor)
        activeProcessor = autoMeshProcessors[0];

    return activeProcessor;
}

void ngActiveAutoMeshProcessor(AutoMeshProcessor processor) {
    activeProcessor = processor;
}

AutoMeshProcessor[] ngAutoMeshProcessors() {
    return autoMeshProcessors;
}