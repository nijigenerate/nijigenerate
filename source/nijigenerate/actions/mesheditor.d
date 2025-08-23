/*
    Copyright © 2020-2023,2022 Inochi2D Project
    Copyright ©           2024 nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/

module nijigenerate.actions.mesheditor;

import nijigenerate.core.actionstack;
import nijigenerate.viewport.common.mesheditor;
import nijigenerate.viewport.model.mesheditor;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.spline;
import nijigenerate.viewport.model.deform;
import nijigenerate.viewport.vertex;
import nijigenerate.actions;
import nijigenerate.viewport.common.mesheditor.tools;
import nijigenerate;
import nijigenerate.viewport.common.mesheditor.tools.onetimedeform;
import nijilive;
import std.format;
import std.range;
import std.algorithm;
import i18n;
//import std.stdio;


/**
    Action for change of binding values at once
*/
class DeformationAction  : LazyBoundAction {
    string name;
    bool dirty;
    Parameter      param;
    Node       target;
    DeformationParameterBinding    deform;
    bool isSet;
    vec2[] vertices;
    vec2u  keypoint;
    bool bindingAdded;
    bool undoable = true;

    this(string name, Node target, void delegate() update = null) {
        this.name   = name;
        this.target = target;
        this.bindingAdded = false;
        this.clear();

        if (update !is null) {
            update();
            this.updateNewState();
        }
        import std.stdio;
        writefln("DeformationAction: %s, target=%s", cast(void*)this, this.target);
    }

    auto self() {
        IncMeshEditor editor = incViewportModelDeformGetEditor();
        IncMeshEditorOne targetEditor = editor? editor.getEditorFor(target): null;
        if (targetEditor is null) {
            targetEditor = ngGetArmedParameterEditorFor(target);
        }
        return targetEditor;
    }

    void addVertex(MeshVertex* vertex) {
    }

    // Resolve target filter to the current OneTimeDeform filter if needed
    void resolveCurrentFilter() {
        if (auto nf = cast(NodeFilter)target) {
            auto cur = ngCurrentNodeFilter();
            if (cur !is null) {
                target = cast(Node)cur;
            }
        }
    }

    void markAsDirty() { dirty = true; }

    void updateNewState() {
        resolveCurrentFilter();
        if (param) {
            auto newDeform      = cast(DeformationParameterBinding)param.getBinding(this.target, "deform");
            if (deform is null && newDeform !is null)
                bindingAdded = true;
            deform = newDeform;
        }
    }

    void clear() {
        resolveCurrentFilter();
        if (self is null) {
            target       = null;
            param        = null;
            deform       = null;
            bindingAdded = false;
            dirty        = false;
            vertices     = null;
            isSet        = false;
        } else {
            param        = incArmedParameter();
            keypoint     = param.findClosestKeypoint();
            vertices     = self.getOffsets();
            deform       = cast(DeformationParameterBinding)param.getBinding(this.target, "deform");
            bindingAdded = false;
        }
        if (deform !is null) {
            isSet    = deform.isSet_[keypoint.x][keypoint.y];
        }
        this.dirty       = false;
    }

    bool isApplyable() {
        return self !is null && self.getTarget() == this.target && incArmedParameter() == this.param &&
               incArmedParameter().findClosestKeypoint() == this.keypoint;
    }

    /**
        Rollback
    */
    void rollback() {
        import std.stdio;
        writefln("undo %s, %s", cast(void*)this, target ? target.name: "<null>");
        resolveCurrentFilter();
        if (undoable) {
            if (vertices) {
                if (deform !is null) {
                    writefln(" execute undo");
                    vec2[] tmpVertices = vertices;
                    bool   tmpIsSet    = isSet;
                    vertices = deform.values[keypoint.x][keypoint.y].vertexOffsets.dup;
                    isSet    = deform.isSet_[keypoint.x][keypoint.y];
                    deform.update(this.keypoint, tmpVertices);
                    deform.isSet_[keypoint.x][keypoint.y] = tmpIsSet;
                    deform.reInterpolate();
                    if (bindingAdded) {
                        param.removeBinding(deform);
                    }
                }
                if (self !is null && self.getTarget() == this.target) {
                    self.resetMesh();
                    if (deform !is null) {
                        vec2[] offs = deform.values[keypoint.x][keypoint.y].vertexOffsets.dup;
                        size_t targetLen = self.getOffsets().length;
                        if (offs.length != targetLen) {
                            vec2[] resized;
                            resized.length = targetLen;
                            size_t n = offs.length < targetLen ? offs.length : targetLen;
                            foreach (i; 0..n) resized[i] = offs[i];
                            foreach (i; n..targetLen) resized[i] = vec2(0);
                            offs = resized;
                        }
                        self.applyOffsets(offs);
                    }
                }
            }
            undoable = false;
        }
    }

    /**
        Redo
    */
    void redo() {
        resolveCurrentFilter();
        if (!undoable) {
            if (vertices) {
                if (deform !is null) {
                    vec2[] tmpVertices = vertices;
                    bool   tmpIsSet    = isSet;
                    vertices = deform.values[keypoint.x][keypoint.y].vertexOffsets.dup;
                    isSet    = deform.isSet_[keypoint.x][keypoint.y];
                    deform.update(this.keypoint, tmpVertices);
                    deform.isSet_[keypoint.x][keypoint.y] = tmpIsSet;
                    deform.reInterpolate();
                    if (bindingAdded) {
                        param.addBinding(deform);
                    }
                }
                if (self !is null && self.getTarget() == this.target) {
                    self.resetMesh();
                    if (deform !is null) {
                        vec2[] offs = deform.values[keypoint.x][keypoint.y].vertexOffsets.dup;
                        size_t targetLen = self.getOffsets().length;
                        if (offs.length != targetLen) {
                            vec2[] resized;
                            resized.length = targetLen;
                            size_t n = offs.length < targetLen ? offs.length : targetLen;
                            foreach (i; 0..n) resized[i] = offs[i];
                            foreach (i; n..targetLen) resized[i] = vec2(0);
                            offs = resized;
                        }
                        self.applyOffsets(offs);
                    }
                }
            }
            undoable = true;
        }
    }

    /**
        Describe the action
    */
    string describe() {
        return _("%s->Edited deformation of %s.").format("deform", name);
    }

    /**
        Describe the action
    */
    string describeUndo() {
        return _("%s->deformation of %s was edited.").format("deform", name);
    }

    /**
        Gets name of this action
    */
    string getName() {
        return this.stringof;
    }
    
    /**
        Merge
    */
    bool merge(Action other) {
        if (this.canMerge(other)) {
            return true;
        }
        return false;
    }

    /**
        Gets whether this node can merge with an other
    */
    bool canMerge(Action other) {
        return false;
    }
};

/**
    Action for change of binding values at once
*/
class MeshEditorAction(T)  : LazyBoundAction {
    alias  TSelf    = typeof(this);
    Node target;
    T action = null;
    mat4 oldEditorTransform = mat4.identity();
    mat4 newEditorTransform = mat4.identity();
    Parameter      param;
    vec2u  oldKeypoint;
    vec2u  newKeypoint;
    SubToolMode[] oldSubToolMode;
    SubToolMode[] newSubToolMode;

    this(Node target, T action = null) {
        this.target = target;
        this.clear();
        this.action = action;
        auto filterTargets = self ? self.getFilterTargets(): [];
        if (filterTargets.length > 0) {
            oldSubToolMode = filterTargets.map!(t=>(cast(OneTimeDeformBase)ngGetEditorFor(t).getTool()).mode).array();
            import std.stdio;
            writefln("MeshEditorAction: capture oldSubToolMode=%s", oldSubToolMode);
        }
    }

    void setAction(T action) {
        this.action = action;
    }

    auto self() {
        IncMeshEditor editor = incViewportModelDeformGetEditor();
        IncMeshEditorOne targetEditor = editor? editor.getEditorFor(target): null;
        if (targetEditor is null) {
            targetEditor = ngGetArmedParameterEditorFor(target);
        }
        return targetEditor;
    }

    void updateNewState() {
        if (auto lazyAction = cast(LazyBoundAction)action)
            lazyAction.updateNewState();
        if (self !is null) {
            newKeypoint = param.findClosestKeypoint();
        }
        auto filterTargets = self ? self.getFilterTargets(): [];
        if (filterTargets.length > 0) {
            newSubToolMode = filterTargets.map!(t=>(cast(OneTimeDeformBase)ngGetEditorFor(t).getTool()).mode).array();
            import std.stdio;
            writefln("MeshEditorAction: capture newSubToolMode=%s", newSubToolMode);
        }
    }

    void clear() {
        if (self is null) {
            target       = null;
            param        = null;
        } else {
            param              = incArmedParameter();
            oldKeypoint        = param.findClosestKeypoint();
            oldEditorTransform = self.transform;
        }
        if (auto lazyAction = cast(LazyBoundAction)action)
            lazyAction.clear();
    }

    bool isApplyable() {
        return self !is null && self.getTarget() == this.target;
    }

    /**
        Rollback
    */
    void rollback() {
        if (action !is null) {
            if (self !is null) {
                param.pushIOffset(param.getKeypointValue(newKeypoint), ParamMergeMode.Forced);
            }
            action.rollback();
            if (isApplyable()) {
                self.transform = oldEditorTransform;
            }
            if (self !is null) {
                param.pushIOffset(param.getKeypointValue(oldKeypoint), ParamMergeMode.Forced);
                self.forceResetAction();
            }
            auto filterTargets = self ? self.getFilterTargets(): [];
            if (filterTargets.length > 0) {
                foreach (i, t; filterTargets) {
                    (cast(OneTimeDeformBase)ngGetEditorFor(t).getTool()).mode = oldSubToolMode[i];
                }
                import std.stdio;
                writefln("MeshEditorAction: undo.mode=%s", oldSubToolMode);
            }
        }
    }

    /**
        Redo
    */
    void redo() {
        if (action !is null) {
            action.redo();
            if (self !is null) {
                param.pushIOffset(param.getKeypointValue(oldKeypoint), ParamMergeMode.Forced);
            }
            if (isApplyable()) {
                self.transform = newEditorTransform;
            }
            if (self !is null) {
                param.pushIOffset(param.getKeypointValue(newKeypoint), ParamMergeMode.Forced);
                self.forceResetAction();
            }
            auto filterTargets = self ? self.getFilterTargets(): [];
            if (filterTargets.length > 0) {
                foreach (i, t; filterTargets) {
                    (cast(OneTimeDeformBase)ngGetEditorFor(t).getTool()).mode = newSubToolMode[i];
                }
                import std.stdio;
                writefln("MeshEditorAction: redo.mode=%s", newSubToolMode);
            }
        }
    }

    /**
        Describe the action
    */
    string describe() {
        if (action !is null)
            return action.describe();
        return "";
    }

    /**
        Describe the action
    */
    string describeUndo() {
        if (action !is null)
            return action.describeUndo();
        return "";
    }

    /**
        Gets name of this action
    */
    string getName() {
        return this.stringof;
    }
    
    /**
        Merge
    */
    bool merge(Action other) {
        if (action !is null)
            return action.merge(other);
        return false;
    }

    /**
        Gets whether this node can merge with an other
    */
    bool canMerge(Action other) {
        if (action !is null)
            return action.canMerge(other);
        return false;
    }

    bool dirty() {
        if (action !is null)
            return true;
        return false;
    }
};

class MeshEditorPathDeformAction(T) : MeshEditorAction!T {
public:
//    CatmullSpline path;
    SplinePoint[] oldPathPoints;
    SplinePoint[] oldTargetPathPoints;
    SplinePoint[] newPathPoints;
    SplinePoint[] newTargetPathPoints;
    vec2[] oldInitTangents;
    vec2[] newInitTangents;
    vec3[] oldRefOffsets;
    vec3[] newRefOffsets;
    ulong[] oldSelected;
    ulong[] newSelected;
    vec2[] oldDeformation;
    vec2[] newDeformation;
    float oldOrigX, oldOrigY, oldOrigRotZ;
    float newOrigX, newOrigY, newOrigRotZ;
    
    IncMeshEditorOneFor!(Drawable, EditMode.ModelEdit) selfDeform() {
        return cast(IncMeshEditorOneFor!(Drawable, EditMode.ModelEdit))self();
    }

    auto path() {
        if (self !is null)
            return self.getPath();
        else
            return null;
    }

    this(Node target, T action = null) {
        super(target, action);
        if (path !is null) {
            oldPathPoints = path.points.dup;
            oldInitTangents = path.initTangents.dup;
            oldRefOffsets = path.refOffsets.dup;
            oldOrigX = path.origX;
            oldOrigY = path.origY;
            oldOrigRotZ = path.origRotZ;
        } else {
            oldPathPoints = null;
            oldInitTangents = null;
            oldRefOffsets = null;
            oldOrigX = 0;
            oldOrigY = 0;
            oldOrigRotZ = 0;
        }
        if (self !is null) {
            oldSelected = self.selected;
            if (selfDeform !is null)
                oldDeformation = selfDeform.deformation;
        } else {
            oldSelected = null;
            oldDeformation = null;
        }

        if (this.path && this.path.target !is null)
            oldTargetPathPoints = this.path.target.points.dup;
        else
            oldTargetPathPoints = null;
    }

    override
    void updateNewState() {
        super.updateNewState();
        if (path !is null) {
            newPathPoints = path.points.dup;
            newInitTangents = path.initTangents.dup;
            newOrigX = path.origX;
            newOrigY = path.origY;
            newOrigRotZ = path.origRotZ;
            newRefOffsets = path.refOffsets.dup;
        }
        if (self !is null) {
            newSelected = self.selected;
            if (selfDeform !is null)
                newDeformation = selfDeform.deformation;
        }
        if (path !is null && path.target !is null) 
            newTargetPathPoints = path.target.points.dup;        
    }

    override
    void clear() {
        super.clear();
        if (path !is null) {
            oldPathPoints = path.points.dup;
            oldInitTangents = path.initTangents.dup;
            oldRefOffsets = path.refOffsets.dup;
            oldOrigX = path.origX;
            oldOrigY = path.origY;
            oldOrigRotZ = path.origRotZ;
        } else {
            oldPathPoints = null;
            oldInitTangents = null;
            oldOrigX = 0;
            oldOrigY = 0;
            oldOrigRotZ = 0;
        }
        if (self !is null) {
            oldSelected = self.selected;
            if (selfDeform !is null)
                oldDeformation = selfDeform.deformation;
        }
        else oldSelected = null;
        if (path !is null && path.target !is null)
            oldTargetPathPoints = path.target.points.dup;
        else
            oldTargetPathPoints = null;
        newPathPoints = null;
        newTargetPathPoints = null;
    }

    /**
        Rollback
    */
    override
    void rollback() {
        super.rollback();
        if (isApplyable()) {
            if (oldPathPoints !is null && oldPathPoints.length > 0 && path !is null) {
                path.points = oldPathPoints.dup;
                path.initTangents = oldInitTangents.dup;
                path.refOffsets = oldRefOffsets.dup;
                path.origX = oldOrigX;
                path.origY = oldOrigY;
                path.origRotZ  = oldOrigRotZ;
                path.update(); /// FIX ME: we need to recreate path object if needed.
            }
            if (oldSelected !is null) {
                self.selected = oldSelected.dup;
            }
            if (oldDeformation !is null)
                selfDeform.deformation = oldDeformation.dup;
            if (oldTargetPathPoints !is null && oldTargetPathPoints.length > 0 && path !is null && path.target !is null) {
                path.target.points = oldTargetPathPoints.dup;
                path.target.update(); /// FIX ME: we need to recreate path object if needed.
            }
        }
    }

    /**
        Redo
    */
    override
    void redo() {
        super.redo();
        if (isApplyable()) {
            if (newPathPoints !is null && newPathPoints.length > 0 && path !is null) {
                path.points = newPathPoints.dup;
                path.initTangents = newInitTangents.dup;
                path.refOffsets = newRefOffsets.dup;
                path.origX = newOrigX;
                path.origY = newOrigY;
                path.origRotZ  = newOrigRotZ;
                path.update();
            }
            if (newSelected !is null) {
                self.selected = newSelected.dup;
            }
            if (newDeformation !is null)
                selfDeform.deformation = newDeformation.dup;
            if (newTargetPathPoints !is null && newTargetPathPoints.length > 0 && path !is null && path.target !is null) {
                path.target.points = newTargetPathPoints.dup;
                path.target.update();
            }
        }
   }
}

/**
    Action to toggle and restore OneTimeDeform sub-tool modes across targets.

    This keeps UI mode changes in the undo/redo history so that
    users see mode flip/restoration alongside data edits.
*/
class SubToolModeChangeAction : Action {
public:
    Node[] targets;
    SubToolMode[] oldModes;
    SubToolMode[] newModes;

    this(Node[] targets, SubToolMode[] oldModes, SubToolMode[] newModes) {
        this.targets = targets.dup;
        this.oldModes = oldModes.dup;
        this.newModes = newModes.dup;
    }

    // Action interface
    void rollback() {
        foreach (i, t; targets) {
            auto ed = ngGetEditorFor(t);
            if (ed is null) continue;
            auto tool = cast(OneTimeDeformBase)ed.getTool();
            if (tool is null) continue;
            if (i < oldModes.length)
                tool.mode = oldModes[i];
        }
    }

    void redo() {
        foreach (i, t; targets) {
            auto ed = ngGetEditorFor(t);
            if (ed is null) continue;
            auto tool = cast(OneTimeDeformBase)ed.getTool();
            if (tool is null) continue;
            if (i < newModes.length)
                tool.mode = newModes[i];
        }
    }

    string describe() { return _("Switched deform sub-tool mode"); }
    string describeUndo() { return _("Reverted deform sub-tool mode"); }
    string getName() { return this.stringof; }
    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
}
