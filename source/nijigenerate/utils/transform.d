module nijigenerate.utils.transform;

import nijigenerate.viewport.model.deform;
import nijigenerate.ext.param;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate.core.math.triangle;
import nijigenerate.core;
import nijigenerate.actions;
import nijigenerate.viewport.common.mesheditor;
import nijigenerate.viewport.common.mesh;
import nijigenerate.windows.flipconfig;
import nijigenerate;
import std.string;
import nijilive;
import i18n;
//import std.stdio;
import nijigenerate.utils;
import std.algorithm.searching: countUntil;



ParameterBinding incBindingGetPairFor(Parameter param, Node target, FlipPair pair, string name, bool forceCreate = false) {
    Node pairNode;
    ParameterBinding result = null;
    if (pair !is null && pair.parts[0] !is null && pair.parts[0].uuid == target.uuid) {
        pairNode = pair.parts[1];
    } else if (pair !is null) {
        pairNode = pair.parts[0];
    }
    if (pairNode is null && forceCreate) {
        pairNode = target;
    }
    if (pairNode !is null) {
        foreach (ParameterBinding binding; param.bindings) {
            if (binding.getTarget().target.uuid == pairNode.uuid && binding.getTarget.name == name)
                return binding;
        }
    }
    if (forceCreate) {
        result = cast(ParameterBinding)param.createBinding(pairNode, name);
        // Skip if trying to add a deform binding to a node that can't get deformed
        if(name == "deform" && cast(Deformable)pairNode is null) return null;
        param.addBinding(result);
        auto action = new ParameterBindingAddAction(param, result);
        incActionPush(action);
        return result;
    }
    return null;
}


/** 
    * incBindingAutoFlip: flipping or paste binding from srcBinding.
    * if extrapolation is set to true, this function copies deformation from mirrored position, otherwise, deformation is copied from same index.
    * if axis is set other than 2, data is flipped in any direction.
    * Usage:
    * 1. set from mirror / mirrrored auto-fill:
    *    extrapolation = true, axis is in one of {0, 1, -1}, srcBinding should be taken from same parameters
    * 2. copy deformation from other binding / parameters
    *    extrapolation = false, axis is in one of {0, 1, -1}, srcBinding should be taken from other parameters
    * 3. copy deformation from other parts.
    *    extrapolation = false, axis = 2
    * Params:
    *   binding = destination binding
    *   srcBinding = source binding to be copied
    *   index = target index of binding
    *   axis = 0: flip horizontally, 1: flip vertically, -1: flip diagonally, 2: not flipped
    *   extrapolation = specifying source index is selected in mirroered position or not.
    */
void incBindingAutoFlip(ParameterBinding binding, ParameterBinding srcBinding, vec2u index, uint axis, bool extrapolation = true, ulong[]* selected = null) {
    T extrapolateValueAt(T)(ParameterBindingImpl!(T) binding, vec2u index, uint axis) {
        vec2 offset = binding.parameter.getKeypointOffset(index);

        switch (axis) {
            case -1: offset = vec2(1, 1) - offset; break;
            case 0: offset.x = 1 - offset.x; break;
            case 1: offset.y = 1 - offset.y; break;
            case 2: break; // Just paste from srcBinding
            default: assert(false, "bad axis");
        }

        vec2u srcIndex;
        vec2 subOffset;
        binding.parameter.findOffset(offset, srcIndex, subOffset);

        return binding.interpolate(srcIndex, subOffset);            
    }
    T interpolateValueAt(T)(ParameterBindingImpl!(T) binding, vec2u index, uint axis) {
        vec2 offset = binding.parameter.getKeypointOffset(index);
        vec2u srcIndex;
        vec2 subOffset;
        binding.parameter.findOffset(offset, srcIndex, subOffset);
        return binding.interpolate(srcIndex, subOffset);            
    }

    Deformation* getMaskedDeformation(Deformation* deform, Deformation* newDeform, ulong[]* selected) {
        if (newDeform is null)
            return null;
        Deformation* maskedDeform = new Deformation(deform.vertexOffsets);
        foreach (i, d; deform.vertexOffsets) {
            if ((*selected).countUntil(i) >= 0) {
                maskedDeform.vertexOffsets[i] = newDeform.vertexOffsets[i];
            }
        }
        return maskedDeform;
    }

    auto deformBinding = cast(DeformationParameterBinding)binding;
    if (srcBinding !is null) {
        auto srcDeformBinding = cast(DeformationParameterBinding)srcBinding;
        if (deformBinding !is null && srcDeformBinding !is null) {
            PathDeformer deformable = cast(PathDeformer)deformBinding.getTarget().node;
            PathDeformer srcDeformable = cast(PathDeformer)srcDeformBinding.getTarget().node;
            Drawable drawable = cast(Drawable)deformBinding.getTarget().node;
            Drawable srcDrawable = cast(Drawable)srcDeformBinding.getTarget().node;
            Deformation* newDeform = null;
            if (drawable !is null && srcDrawable !is null) {
                auto mesh = new IncMesh(drawable.getMesh());
                Deformation deform = extrapolation? extrapolateValueAt!Deformation(srcDeformBinding, index, axis):
                                                    interpolateValueAt!Deformation(srcDeformBinding, index, axis);
                newDeform = deformByDeformationBinding(mesh.vertices, srcDrawable, deform, extrapolation || axis < 1);
                if (selected) newDeform = getMaskedDeformation(&deformBinding.getValue(index), newDeform, selected);

            } else if (deformable !is null && srcDeformable !is null) {
                Deformation deform = extrapolation? extrapolateValueAt!Deformation(srcDeformBinding, index, axis):
                                                    interpolateValueAt!Deformation(srcDeformBinding, index, axis);
                newDeform = deformByDeformationBinding(deformable.vertices, srcDeformable, deform, extrapolation || axis < 1);
                if (selected) newDeform = getMaskedDeformation(&deformBinding.getValue(index), newDeform, selected);
            }
            if (newDeform)
                deformBinding.setValue(index, *newDeform);

        } else {
            auto srcValueBinding = cast(ValueParameterBinding)srcBinding;
            // Return if target binding doesn't support being flipped
            if(srcValueBinding is null) return;
            float value;
            value = extrapolation? extrapolateValueAt!float(srcValueBinding, index, axis):
                                    interpolateValueAt!float(srcValueBinding, index, axis);

            auto valueBinding = cast(ValueParameterBinding)binding;
            valueBinding.setValue(index, value);
            if (axis < 2)
                valueBinding.scaleValueAt(index, axis, -1);
        }

    } else {
        if (deformBinding !is null) {
            Drawable drawable = cast(Drawable)deformBinding.getTarget().node;
            PathDeformer deformable = cast(PathDeformer)deformBinding.getTarget().node;
            // Return if target node doesn't support deformations 
            Deformation* newDeform = null;
            if (drawable !is null) {
                auto mesh = new IncMesh(drawable.getMesh());
                Deformation deform = extrapolation? extrapolateValueAt!Deformation(deformBinding, index, axis):
                                                    interpolateValueAt!Deformation(deformBinding, index, axis);
                newDeform = deformByDeformationBinding(mesh.vertices, drawable, deform, extrapolation || axis < 1);
            } else if (deformable !is null) {
                Deformation deform = extrapolation? extrapolateValueAt!Deformation(deformBinding, index, axis):
                                                    interpolateValueAt!Deformation(deformBinding, index, axis);
                newDeform = deformByDeformationBinding(deformable.vertices, deformable, deform, extrapolation || axis < 1);
            }
            if (selected) newDeform = getMaskedDeformation(&deformBinding.getValue(index), newDeform, selected);
            if (newDeform)
                deformBinding.setValue(index, *newDeform);
        } else {
            if (extrapolation)
                binding.extrapolateValueAt(index, axis);
        }
    }
}