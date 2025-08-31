module nijigenerate.commands.parameter.prop;

import nijigenerate.commands.base;
import nijigenerate.ext;
import nijigenerate.actions;
import nijigenerate.core;
import nijigenerate.viewport.model.deform : incViewportNodeDeformNotifyParamValueChanged;
import i18n;
import nijilive; // vec2, Parameter

// Name only
class SetParameterNameCommand : ExCommand!(TW!(string, "newName", "New parameter name")) {
    this(string newName) { super(_("Set Parameter Name"), newName); }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters) return;
        auto param = ctx.parameters[0];

        // Validate name uniqueness within active puppet
        auto ex = cast(ExPuppet) (ctx.hasPuppet ? ctx.puppet : null);
        if (ex !is null) {
            auto fparam = ex.findParameter(newName);
            if (fparam !is null && fparam.uuid != param.uuid) {
                // Name already taken; do nothing
                return;
            }
        }

        // Push name change as action
        // No action push for name (pointer to property not supported); follow existing behavior
        param.name = newName;
        param.makeIndexable();
    }
}

// Apply min/max and axis breakpoints (normalized values) together
class ApplyParameterPropsAxesCommand : ExCommand!(
    TW!(vec2,    "min",   "New min (x,y)"),
    TW!(vec2,    "max",   "New max (x,y)"),
    TW!(float[], "axisX", "Normalized X breakpoints (including endpoints)"),
    TW!(float[], "axisY", "Normalized Y breakpoints (including endpoints; empty for 1D)")
) {
    this(vec2 min, vec2 max, float[] axisX, float[] axisY) {
        super(_("Apply Parameter Axes + Props"), min, max, axisX, axisY);
    }
    override
    void run(Context ctx) {
        if (!ctx.hasParameters) return;
        auto param = ctx.parameters[0];

        // Apply min/max via actions per component for proper undo/redo
        auto prevMin = param.min; auto prevMax = param.max;
        param.min = min; param.max = max;
        if (prevMin.x != param.min.x)
            incActionPush(new ParameterValueChangeAction!float("min X", param, prevMin.x, param.min.x, &param.min.vector[0]));
        if (param.isVec2 && prevMin.y != param.min.y)
            incActionPush(new ParameterValueChangeAction!float("min Y", param, prevMin.y, param.min.y, &param.min.vector[1]));
        if (prevMax.x != param.max.x)
            incActionPush(new ParameterValueChangeAction!float("max X", param, prevMax.x, param.max.x, &param.max.vector[0]));
        if (param.isVec2 && prevMax.y != param.max.y)
            incActionPush(new ParameterValueChangeAction!float("max Y", param, prevMax.y, param.max.y, &param.max.vector[1]));

        // Helper to rewrite axis points to provided normalized list
        void applyAxis(uint axis, float[] vals) {
            // Guard: endpoints should exist; ensure strictly increasing order
            import std.algorithm : sort;
            sort(vals);
            // Remove all mid points keeping endpoints
            while (param.axisPoints[axis].length > 2) {
                param.deleteAxisPoint(axis, 1);
            }
            // Insert all mid-points (skip endpoints at index 0 and last)
            foreach (i, v; vals) {
                if (i == 0 || i == vals.length - 1) continue;
                param.insertAxisPoint(axis, v);
            }
        }

        // Apply breakpoints
        if (axisX.length >= 2) applyAxis(0, axisX);
        if (param.isVec2 && axisY.length >= 2) applyAxis(1, axisY);

        // Notify deformers
        incViewportNodeDeformNotifyParamValueChanged();
    }
}

enum ParamPropCommand {
    SetParameterName,
    ApplyParameterPropsAxes,
}

Command[ParamPropCommand] commands;

void ngInitCommands(T)() if (is(T == ParamPropCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!ParamPropCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
}
